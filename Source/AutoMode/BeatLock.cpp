#include "BeatLock.h"
#include <cmath>

namespace glitchkid::automode
{
namespace
{
juce::String f1 (float v) { return juce::String (v, 1); }

juce::String titleFor (const std::string& id)
{
    if (id == "eq_b2_gain") return "PRESENCE BELL (3 kHz)";
    if (id == "eq_b1_gain") return "BODY BELL (400 Hz)";
    if (id == "eq_lo_gain") return "LOW SHELF";
    if (id.empty())         return "NOT SUGGESTED";
    if (id == "echo_mix")   return "ECHO AMOUNT";
    if (id == "dim_mix")    return "WIDTH AMOUNT";
    return juce::String (id);
}
}

BeatLockResult analyseBeatLock (const analysis::VocalAnalysis& lead, const analysis::VocalAnalysis& beat, InstrumentalInput input)
{
    using S = analysis::VocalAnalysis::Status;
    BeatLockResult r;
    r.unavailable.add ("BEAT LOCK adjusts only the vocal's own controls. It does not process the instrumental, move your mixer faders, or add reverb (GlitchKid has none).");
    if (input == InstrumentalInput::notConnected)
    {
        r.status = BeatLockResult::Status::noBeatInput;
        r.message = "BEAT REQUIRED: no instrumental is connected to GlitchKid, so the beat cannot be heard. Route the beat's track to the Instrumental input (see docs/SOURCES.md), or use BUILD MY VOCAL MIX, which works with the vocal alone.";
        return r;
    }
    if (input == InstrumentalInput::connectedSilent)
    {
        r.status = BeatLockResult::Status::beatSilent;
        r.message = "The Instrumental input is on, but it is carrying silence. Hosts send silence for an unrouted input, so check the routing and that the beat is playing.";
        return r;
    }
    if (lead.status != S::ready)
    {
        r.status = BeatLockResult::Status::notAnalysed;
        r.message = lead.status == S::tooShort ? "Too little vocal was heard (about 2 seconds of singing or rapping are needed). Play the vocal and the beat together and try again."
                  : lead.status == S::tooQuiet ? "No usable vocal was heard: the vocal input was silent or very quiet."
                  : lead.status == S::invalid  ? "The vocal input contained invalid audio. Try again."
                                               : "Not analysed yet. Press ANALYZE BEAT, then play the vocal together with the beat.";
        return r;
    }
    if (beat.status != S::ready)
    {
        r.status = BeatLockResult::Status::beatNotAnalysed;
        r.message = "The vocal was analysed, but the instrumental was not (not enough usable audio from the beat while listening). Play the beat together with the vocal and try again.";
        return r;
    }
    const auto rel = compare (lead, beat);
    if (! rel.valid)
    {
        r.status = BeatLockResult::Status::notAnalysed;
        r.message = juce::String (rel.unavailable);
        return r;
    }

    r.status = BeatLockResult::Status::ready;
    r.valid = true;
    r.confidence = rel.confidence;
    r.vocalMinusBeatDb = rel.vocalMinusBeatDb;
    r.vocalRmsDb = lead.rmsDb;
    r.beatRmsDb = beat.rmsDb;
    r.vocalPeakDb = lead.peakDb;
    r.beatPeakDb = beat.peakDb;
    r.vocalBands = lead.bandFraction;
    r.beatBands = beat.bandFraction;
    r.maskRisk = rel.maskRisk;
    r.worstBand = rel.worstBand;
    r.facts.add ("Vocal: level " + f1 (lead.rmsDb) + " dB, peak " + f1 (lead.peakDb) + " dB.   Beat: level " + f1 (beat.rmsDb) + " dB, peak " + f1 (beat.peakDb) + " dB   (digital levels, not LUFS).");
    for (const auto& s : rel.reasons)
        r.facts.add (juce::String (s));
    if (rel.confidence < 0.4f)
        r.facts.add ("Only a short stretch was heard: these are rough estimates.");
    r.unavailable.add ("The masking figures are estimates from five coarse bands, not a judgement of your mix.");

    // BALANCE: advice only (GlitchKid cannot move your fader)
    {
        BeatLockItem balance;
        balance.advisory = true;
        balance.move.category = "BALANCE";
        balance.title = "VOCAL VS BEAT LEVEL";
        const float d = rel.vocalMinusBeatDb;
        const juce::String advice = d < -6.0f ? "The beat is " + f1 (-d) + " dB louder than your vocal. If the words are hard to follow, raise the vocal fader in your mixer. GlitchKid cannot move it."
                            : d > 6.0f  ? "Your vocal is " + f1 (d) + " dB louder than the beat. Check the balance in your mixer."
                                        : "Vocal and beat are within 6 dB of each other (" + f1 (d) + " dB). That is a measurement, not a verdict.";
        balance.move.reason = advice.toStdString();
        r.items.push_back (balance);
    }
    for (const auto& m : beatMoves (rel))
    {
        BeatLockItem it;
        it.move = m;
        it.advisory = m.id.empty(); // a withheld move is shown with its reason, never applied
        it.title = titleFor (m.id);
        r.items.push_back (it);
    }
    int adjustable = 0;
    for (const auto& it : r.items)
        adjustable += it.advisory ? 0 : 1;
    r.message = adjustable > 0 ? "Analysed. " + juce::String (adjustable) + " suggested adjustment" + (adjustable > 1 ? "s" : "") + " below: untick any you do not want, then APPLY."
                               : "Analysed. No adjustment is suggested: the voice and the beat do not obviously conflict in these five bands, or the safe limits leave nothing to change.";
    return r;
}

SoundState applyBeatLock (const BeatLockResult& result, const std::vector<bool>& selected, const SoundState& current, const RangeLookup& rangeOf, std::vector<MixChange>* changes)
{
    SoundState s = current;
    if (! result.valid)
        return s;
    for (size_t i = 0; i < result.items.size(); ++i)
    {
        const auto& m = result.items[i].move;
        if (result.items[i].advisory || m.id.empty() || i >= selected.size() || ! selected[i])
            continue;
        const juce::String id (m.id);
        Range range;
        if (! rangeOf (id, range) || ! s.has (id))
            continue;
        const float base = s.get (id, 0.0f);
        if (m.scale && base <= 0.0f)
            continue; // an ambience that is off stays off
        float v = applyBeatMove (base, m);
        if (! std::isfinite (v))
            continue;
        v = std::min (std::max (v, range.lo), range.hi);
        if (range.snap)
            v = std::min (std::max (range.snap (v), range.lo), range.hi);
        s.set (id, v);
    }
    if (changes != nullptr)
        for (const auto& e : s.values)
        {
            const float from = current.get (e.id, e.value);
            if (std::abs (e.value - from) > 1.0e-4f)
            {
                juce::String why;
                for (const auto& it : result.items)
                    if (juce::String (it.move.id) == e.id)
                        why = (why.isEmpty() ? juce::String() : why + " ") + juce::String (it.move.reason);
                changes->push_back ({ e.id, from, e.value, why });
            }
        }
    return s;
}
}
