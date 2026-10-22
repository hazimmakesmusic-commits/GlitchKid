#include "MixEngine.h"
#include "../DSP/PitchCorrection/ScaleQuantizer.h"
#include <cmath>
#include <map>

namespace glitchkid::automode
{
namespace
{
float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
bool ok (float v) { return std::isfinite (v); }

constexpr float referenceRmsDb = -20.0f;  // the active level the compressor thresholds in the profiles were set for
constexpr float maxThresholdShift = 8.0f; // dB
constexpr float maxEqGain = 6.0f;         // no EQ band is ever pushed beyond +/-6 dB by a build

// Effect amounts that the intensity scales (value moves between its default and the profile's value).
const char* const amountIds[] = { "echo_mix", "dim_mix", "dbl_amount", "sat_drive" };

juce::String f1 (float v) { return juce::String (v, 1); }

const char* area (const juce::String& id)
{
    if (id.startsWith ("tune") || id == "formant_shift" || id.startsWith ("harm"))
        return "pitch";
    if (id.startsWith ("eq_"))
        return "tone";
    if (id.startsWith ("comp_"))
        return "dynamics";
    if (id == "sat_drive" || id.startsWith ("mut_") || id.startsWith ("stutter"))
        return "character";
    return "space"; // echo, dimension, doubler
}
}

MixProposal buildMix (const MixInput& in, const RangeLookup& rangeOf)
{
    using S = analysis::VocalAnalysis::Status;
    MixProposal p;
    p.sound = in.current;
    p.profileName = in.profile >= 0 && in.profile < FactoryPresets::count ? juce::String (FactoryPresets::info (in.profile).name) : juce::String ("CLEAN DEFAULT");

    // ---- is there anything to build from? (never pretend) ----
    const auto& a = in.lead;
    const bool partial = in.allowProfileOnly && a.status == S::tooShort; // a little audio was heard, but not enough to fit anything to it
    if (! partial && (a.status != S::ready || ! ok (a.rmsDb) || ! ok (a.peakDb)))
    {
        p.message = a.status == S::tooShort ? "Too little vocal was heard (about 2 seconds of singing or rapping are needed). Nothing was changed. Try again."
                  : a.status == S::tooQuiet ? "No usable vocal was heard: the input was silent or very quiet. Check the track and that the host is playing. Nothing was changed."
                  : a.status == S::invalid  ? "The input contained invalid audio. Nothing was changed. Try again."
                  : a.status == S::listening ? "Still listening to the vocal. Nothing was changed yet."
                                             : "The vocal has not been analysed yet. Nothing was changed.";
        return p;
    }
    const float intensity = clampf (ok (in.intensity) ? in.intensity : 0.6f, 0.0f, 1.0f);

    // ---- foundation: the profile's own values ----
    SoundState s = in.profile >= 0 && in.profile < FactoryPresets::count ? FactoryPresets::build (in.profile, in.factoryDefault) : in.factoryDefault;
    for (const char* keep : { "tune_on", "tune_strength", "tune_speed", "tune_key", "tune_scale", "gain", "mix", "air_on", "air_presence", "air_air", "air_link", "air_mix", "air_trim", "clean_on", "clean_amount", "clean_tone", "clean_res", "clean_ess" })
        s.set (keep, in.current.get (keep, in.factoryDefault.get (keep, 0.0f)));
    s.egoId = {};
    s.egoChaos = 0.0f;
    s.egoSeed = 0;

    std::map<juce::String, juce::String> why;
    auto get = [&s] (const char* id, float fallback = 0.0f) { return s.get (id, fallback); };
    auto put = [&] (const char* id, float v, const juce::String& reason)
    {
        s.set (id, v);
        auto& w = why[id];
        w = w.isEmpty() ? reason : w + " " + reason;
    };

    // the old creative effects are not part of a vocal mix any more: a build switches them off (and says so); BACK TO MY SOUND restores them
    for (const char* on : { "mut_on", "harm_on", "stutter_on" })
        if (in.current.get (on, 0.0f) >= 0.5f)
            why[on] = "Not part of the vocal mix any more: switched off (BACK TO MY SOUND brings it back).";
    if (std::abs (in.current.get ("formant_shift", 0.0f)) > 0.05f)
        why["formant_shift"] = "Not part of the vocal mix any more: set back to 0 (BACK TO MY SOUND brings it back).";

    if (partial)
        p.evidence.add ("LEAD: only " + f1 (a.activeSeconds) + " s of vocal was heard (about 2 s are needed), so nothing was fitted to your voice.");
    else
        p.evidence.add ("LEAD: " + f1 (a.activeSeconds) + " s of vocal, peak " + f1 (a.peakDb) + " dB, level " + f1 (a.rmsDb) + " dB (digital, not LUFS), dynamic range "
                        + f1 (a.dynamicRangeDb) + " dB.");

    // ---- intensity: how much of the profile's effect amounts is used ----
    {
        const float k = 0.5f + 0.5f * intensity;
        if (k < 0.999f)
        {
            for (const char* id : amountIds)
                if (in.profile >= 0 && s.has (id))
                {
                    const float v = s.get (id, 0.0f); // every amount is "none" at 0, so scaling is a plain multiplication
                    if (v > 1.0e-4f)
                        put (id, v * k, "Scaled to " + juce::String (juce::roundToInt (k * 100.0f)) + " % of the profile (intensity).");
                }
            p.evidence.add ("Intensity " + juce::String (juce::roundToInt (intensity * 100.0f)) + " %: the profile's effect amounts are used at " + juce::String (juce::roundToInt (k * 100.0f)) + " %.");
        }
    }

    // ---- dynamics: keep the compressor doing the same amount of work whatever the recording level ----
    const bool compOn = get ("comp_ratio", 1.0f) > 1.05f && ! partial;
    if (compOn)
    {
        const float shift = clampf (0.7f * (a.rmsDb - referenceRmsDb), -maxThresholdShift, maxThresholdShift);
        if (std::abs (shift) >= 0.5f)
            put ("comp_thresh", get ("comp_thresh") + shift, "Your vocal is " + f1 (a.rmsDb) + " dB, so the compressor threshold moved " + f1 (shift) + " dB to compress about as much as the profile intends.");
        if (a.dynamicRangeDb > 20.0f)
            put ("comp_ratio", get ("comp_ratio") + 0.5f, "The level moves a lot (" + f1 (a.dynamicRangeDb) + " dB range): slightly firmer compression.");
        else if (a.dynamicRangeDb < 6.0f && a.crestDb < 9.0f)
            put ("comp_ratio", std::max (1.5f, get ("comp_ratio") * 0.75f), "The take is already compressed (range " + f1 (a.dynamicRangeDb) + " dB): gentler compression.");
    }

    // ---- headroom: the profiles' makeup gain was set for vocals peaking around -8 dB; louder peaks get less makeup so the output keeps its headroom ----
    if (compOn && a.peakDb > -8.0f)
    {
        const float cut = clampf ((a.peakDb + 8.0f) * 0.6f, 0.0f, 6.0f);
        if (cut >= 0.3f)
            put ("comp_makeup", std::max (0.0f, get ("comp_makeup") - cut), "Your peaks are at " + f1 (a.peakDb) + " dB: makeup gain reduced by " + f1 (cut) + " dB to keep headroom.");
    }

    // ---- hot input ----
    if (! partial && (a.clippedSamples >= 3 || a.peakDb > -0.5f))
    {
        put ("sat_drive", get ("sat_drive") * 0.5f, "The input is clipping or touching full scale: saturation halved.");
        put ("comp_makeup", std::max (0.0f, get ("comp_makeup") - 2.0f), "The input is hot: 2 dB less makeup gain.");
        p.evidence.add ("The recording clips or touches full scale (" + juce::String (a.clippedSamples) + " clipped samples): fix this at the source if you can.");
    }

    // ---- tone from the balance of the voice itself ----
    // (with only a sliver of audio the band shares mean nothing: neutral values trigger none of the tone rules)
    const float lowShare = partial ? 0.1f : a.bandFraction[0], lowMid = partial ? 0.45f : a.bandFraction[1], presence = partial ? 0.1f : a.bandFraction[3], air = partial ? 0.05f : a.bandFraction[4];
    const float sibilance = partial ? 0.0f : a.sibilanceIndex;
    auto eqAdd = [&] (const char* id, float delta, const juce::String& reason)
    {
        put (id, clampf (get (id) + delta, -maxEqGain, maxEqGain), reason);
    };
    // CLEAN LAB measures and corrects rumble, low-mid build-up and sibilance itself (after the EQ, from the actual signal); when it is on, BUILD leaves those three
    // corrections to it, so the same fault is never corrected twice.
    const bool cleanLab = in.current.get ("clean_on", 0.0f) >= 0.5f && in.current.get ("clean_amount", 0.0f) > 5.0f;
    if (cleanLab && (lowShare > 0.30f || lowMid > 0.55f || sibilance > 0.35f))
        p.evidence.add ("CLEAN LAB is on: BUILD left the low-end, low-mid and S-sound corrections to it, so nothing is corrected twice.");
    if (lowShare > 0.30f && ! cleanLab)
        eqAdd ("eq_lo_gain", -2.0f, "A lot of the vocal's energy is under 120 Hz (" + juce::String (juce::roundToInt (lowShare * 100.0f)) + " %): low shelf lowered.");
    if (lowMid > 0.55f && ! cleanLab)
        eqAdd ("eq_b1_gain", -1.5f, "The vocal is heavy in the low-mids (" + juce::String (juce::roundToInt (lowMid * 100.0f)) + " %): the 400 Hz bell was cut a little more.");
    if (presence < 0.06f)
    {
        eqAdd ("eq_b2_gain", 1.0f, "The words may be hard to hear (little energy at 2-5 kHz): presence raised.");
        p.evidence.add ("Presence (2-5 kHz) is only " + juce::String (juce::roundToInt (presence * 100.0f)) + " % of the vocal's energy.");
    }
    if (sibilance > 0.35f)
    {
        if (! cleanLab)
            eqAdd ("eq_hi_gain", -clampf ((sibilance - 0.35f) * 6.0f, 0.0f, 2.0f), "S sounds look strong (estimate): high shelf lowered.");
    }
    else if (air < 0.015f && presence < 0.12f)
        eqAdd ("eq_hi_gain", 1.0f, "The vocal is dark on top: high shelf raised a little.");
    if (air > 0.12f)
        put ("sat_drive", get ("sat_drive") * 0.5f, "The vocal is already bright: saturation halved so it does not get harsh.");

    // ---- the beat, only if it was really analysed ----
    p.mode = MixMode::vocalOnly;
    if (in.beat.status == S::ready && in.instrumental != InstrumentalInput::notConnected)
    {
        const auto rel = compare (a, in.beat);
        if (rel.valid)
        {
            p.mode = MixMode::beatInformed;
            for (const auto& r : rel.reasons)
                p.evidence.add ("BEAT: " + juce::String (r));
            for (const auto& m : beatMoves (rel))
            {
                if (m.scale)
                {
                    if (s.has (m.id.c_str()) && get (m.id.c_str()) > 0.0f)
                        put (m.id.c_str(), get (m.id.c_str()) * m.factor, m.reason);
                }
                else
                    eqAdd (m.id.c_str(), m.delta, m.reason);
            }
            p.evidence.add ("The lead is " + f1 (rel.vocalMinusBeatDb) + " dB relative to the beat on this track; balance the levels in your mixer (GlitchKid does not change the beat or your fader).");
        }
    }
    if (p.mode == MixMode::vocalOnly)
        p.unavailable.add (in.instrumental == InstrumentalInput::notConnected ? "No instrumental is connected, so this is a VOCAL ONLY mix. Nothing is assumed about the beat."
                           : in.instrumental == InstrumentalInput::connectedSilent ? "The instrumental input is on but was silent, so this is a VOCAL ONLY mix."
                                                                                  : "The instrumental was not analysed (not enough usable audio while listening), so this is a VOCAL ONLY mix.");
    p.unavailable.add ("Ad-libs cannot be heard separately, key and tempo are not detected, and there is no reverb: the echo, Dimension and Doubler carry the space.");

    // ---- pitch: never changed by a build, only reported ----
    if (get ("tune_on") >= 0.5f)
    {
        const int key = juce::roundToInt (s.get ("tune_key", 0.0f)), scale = juce::roundToInt (s.get ("tune_scale", 0.0f));
        static const char* keys[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        p.evidence.add (juce::String ("Pitch Tune is ON and left exactly as you set it on TUNE: ") + keys[juce::jlimit (0, 11, key)] + " "
                        + glitchkid::pitch::ScaleQuantizer::name ((glitchkid::pitch::ScaleQuantizer::Scale) juce::jlimit (0, glitchkid::pitch::ScaleQuantizer::numScales - 1, scale))
                        + ", strength " + juce::String (juce::roundToInt (s.get ("tune_strength", 100.0f))) + " % (your key is not detected).");
        if (key == 0 && scale == 0)
            p.unavailable.add ("Pitch Tune is on but no key and scale are set (C Chromatic lets every note through). Choose them on TUNE for real tuning.");
    }
    else
        p.evidence.add ("Pitch Tune is OFF: a build never switches tuning on. Use the TUNE screen if you want it.");

    // ---- make every value legal, then list what actually changes ----
    for (auto& e : s.values)
    {
        Range r;
        if (! rangeOf (e.id, r))
            continue;
        float v = ok (e.value) ? e.value : in.current.get (e.id, 0.0f);
        v = clampf (v, r.lo, r.hi);
        e.value = r.snap ? clampf (r.snap (v), r.lo, r.hi) : v;
    }
    p.sound = s;
    for (const auto& e : s.values)
    {
        if (e.id == "bypass")
            continue;
        const float from = in.current.get (e.id, e.value);
        if (std::abs (e.value - from) > 1.0e-4f)
        {
            const auto it = why.find (e.id);
            p.changes.push_back ({ e.id, from, e.value, it != why.end() ? it->second : "From the " + p.profileName + " profile." });
        }
    }
    std::stable_sort (p.changes.begin(), p.changes.end(), [] (const MixChange& x, const MixChange& y)
    {
        static const juce::StringArray order { "pitch", "tone", "dynamics", "character", "space" };
        return order.indexOf (area (x.id)) < order.indexOf (area (y.id));
    });

    p.usable = true;
    juce::StringArray areas;
    for (const auto& c : p.changes)
        if (! areas.contains (area (c.id)))
            areas.add (area (c.id));
    p.message = juce::String (partial ? "STARTING MIX (profile only, not fitted to your voice)" : p.mode == MixMode::beatInformed ? "BEAT INFORMED mix" : "VOCAL ONLY mix") + " built from " + p.profileName + ": "
                + (p.changes.empty() ? juce::String ("nothing needed changing.") : juce::String ((int) p.changes.size()) + " settings changed (" + areas.joinIntoString (", ") + ").");
    return p;
}
}
