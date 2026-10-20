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
const char* const amountIds[] = { "echo_mix", "dim_mix", "dbl_amount", "mut_amount", "sat_drive", "stutter_mix" };

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
    if (a.status != S::ready || ! ok (a.rmsDb) || ! ok (a.peakDb))
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
    for (const char* keep : { "tune_key", "tune_scale", "gain", "mix" })
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
    const bool compOn = get ("comp_ratio", 1.0f) > 1.05f;
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
    if (a.clippedSamples >= 3 || a.peakDb > -0.5f)
    {
        put ("sat_drive", get ("sat_drive") * 0.5f, "The input is clipping or touching full scale: saturation halved.");
        put ("comp_makeup", std::max (0.0f, get ("comp_makeup") - 2.0f), "The input is hot: 2 dB less makeup gain.");
        p.evidence.add ("The recording clips or touches full scale (" + juce::String (a.clippedSamples) + " clipped samples): fix this at the source if you can.");
    }

    // ---- tone from the balance of the voice itself ----
    const float lowShare = a.bandFraction[0], lowMid = a.bandFraction[1], presence = a.bandFraction[3], air = a.bandFraction[4];
    auto eqAdd = [&] (const char* id, float delta, const juce::String& reason)
    {
        put (id, clampf (get (id) + delta, -maxEqGain, maxEqGain), reason);
    };
    if (lowShare > 0.30f)
        eqAdd ("eq_lo_gain", -2.0f, "A lot of the vocal's energy is under 120 Hz (" + juce::String (juce::roundToInt (lowShare * 100.0f)) + " %): low shelf lowered.");
    if (lowMid > 0.55f)
        eqAdd ("eq_b1_gain", -1.5f, "The vocal is heavy in the low-mids (" + juce::String (juce::roundToInt (lowMid * 100.0f)) + " %): the 400 Hz bell was cut a little more.");
    if (presence < 0.06f)
    {
        eqAdd ("eq_b2_gain", 1.0f, "The words may be hard to hear (little energy at 2-5 kHz): presence raised.");
        p.evidence.add ("Presence (2-5 kHz) is only " + juce::String (juce::roundToInt (presence * 100.0f)) + " % of the vocal's energy.");
    }
    if (a.sibilanceIndex > 0.35f)
        eqAdd ("eq_hi_gain", -clampf ((a.sibilanceIndex - 0.35f) * 6.0f, 0.0f, 2.0f), "S sounds look strong (estimate): high shelf lowered.");
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
            float clutter = 1.0f;
            if (rel.worstBand >= 2)
            {
                const float risk = rel.maskRisk[(size_t) rel.worstBand];
                eqAdd ("eq_b2_gain", clampf (risk * 2.0f, 0.0f, 2.5f), "The beat competes with the voice at " + juce::String (bandName (rel.worstBand)) + ": presence raised so the words stay clear.");
                clutter *= 0.85f;
            }
            if (rel.maskRisk[1] > 0.35f)
            {
                eqAdd ("eq_b1_gain", -1.0f, "The beat is busy in the low-mids: the 400 Hz bell was cut a little.");
                eqAdd ("eq_lo_gain", -1.0f, "The beat is busy in the low-mids: low shelf lowered.");
            }
            if (rel.beatLowEndHeavy)
                eqAdd ("eq_lo_gain", -1.5f, "The beat is heavy under 120 Hz: the lead's low end was kept light.");
            if (rel.beatVeryLoud)
                clutter *= 0.9f;
            if (clutter < 0.999f)
                for (const char* id : { "echo_mix", "dim_mix" })
                    if (s.has (id) && get (id) > 0.0f)
                        put (id, get (id) * clutter, "The beat is crowded: this ambience was reduced so the vocal does not get lost.");
            p.evidence.add ("The lead is " + f1 (rel.vocalMinusBeatDb) + " dB relative to the beat on this track; balance the levels in your mixer (GlitchKid does not change the beat or your fader).");
        }
    }
    if (p.mode == MixMode::vocalOnly)
        p.unavailable.add (in.instrumental == InstrumentalInput::notConnected ? "No instrumental is connected, so this is a VOCAL ONLY mix. Nothing is assumed about the beat."
                           : in.instrumental == InstrumentalInput::connectedSilent ? "The instrumental input is on but was silent, so this is a VOCAL ONLY mix."
                                                                                  : "The instrumental was not analysed (not enough usable audio while listening), so this is a VOCAL ONLY mix.");
    p.unavailable.add ("Ad-libs cannot be heard separately, key and tempo are not detected, and there is no reverb: the echo, Dimension and Doubler carry the space.");

    // ---- pitch: only what the user set ----
    if (get ("tune_on") >= 0.5f)
    {
        const int key = juce::roundToInt (s.get ("tune_key", 0.0f)), scale = juce::roundToInt (s.get ("tune_scale", 0.0f));
        static const char* keys[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        p.evidence.add (juce::String ("Pitch Tune follows YOUR key and scale: ") + keys[juce::jlimit (0, 11, key)] + " "
                        + glitchkid::pitch::ScaleQuantizer::name ((glitchkid::pitch::ScaleQuantizer::Scale) juce::jlimit (0, glitchkid::pitch::ScaleQuantizer::numScales - 1, scale)) + " (not detected).");
        if (key == 0 && scale == 0)
            p.unavailable.add ("Pitch Tune is on but no key and scale are set (C Chromatic lets every note through). Choose them on TUNE for real tuning.");
    }

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
    p.message = juce::String (p.mode == MixMode::beatInformed ? "BEAT INFORMED" : "VOCAL ONLY") + " mix built from " + p.profileName + ": "
                + (p.changes.empty() ? juce::String ("nothing needed changing.") : juce::String ((int) p.changes.size()) + " settings changed (" + areas.joinIntoString (", ") + ").");
    return p;
}
}
