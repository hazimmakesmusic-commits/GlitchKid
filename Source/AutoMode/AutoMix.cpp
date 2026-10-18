#include "AutoMix.h"
#include <cmath>

namespace glitchkid::automode
{
const char* profileName (Profile) { return "ASTRO MODE"; }

namespace
{
float lerp (float a, float b, float t) { return a + (b - a) * t; }
float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Thresholds (documented in docs/ARCHITECTURE.md). Band fractions are shares of active-frame energy.
constexpr float boomyLow = 0.30f;        // < 120 Hz share above this: do not boost the low end, cut the mud a little more
constexpr float muddyLowMid = 0.55f;     // 120-500 Hz share above this: cut the 400 Hz bell more
constexpr float dullPresence = 0.06f;    // 2-5 kHz share below this: the words may be hard to hear -> add presence, keep ambience lower
constexpr float forwardPresence = 0.22f; // 2-5 kHz share above this: already forward -> no boost
constexpr float darkTop = 0.015f;        // > 5 kHz share below this: already dark -> do not cut the top
constexpr float brightTop = 0.12f;       // > 5 kHz share above this: bright -> halve the saturation
constexpr float sibilant = 0.35f;        // sibilanceIndex above this: cut the top a bit more
constexpr float hotPeakDb = -0.5f;       // peak above this (or any clipping) -> no added saturation
}

Result compute (const analysis::VocalAnalysis& a, Profile, float intensity, const SoundState& current, const RangeLookup& rangeOf)
{
    Result r;
    r.sound = current;

    if (a.status != analysis::VocalAnalysis::Status::ready)
    {
        r.message = "Analysis not usable yet. Nothing was changed.";
        return r;
    }
    for (float v : { a.peakDb, a.rmsDb, a.dynP10Db, a.dynP50Db, a.dynP90Db, a.dynamicRangeDb, a.sibilanceIndex })
        if (! std::isfinite (v))
        {
            r.message = "Analysis contained invalid numbers. Nothing was changed.";
            return r;
        }
    for (float v : a.bandFraction)
        if (! std::isfinite (v))
        {
            r.message = "Analysis contained invalid numbers. Nothing was changed.";
            return r;
        }
    const float i = clampf (std::isfinite (intensity) ? intensity : 0.0f, 0.0f, 1.0f);
    if (i < 0.05f)
    {
        r.message = "Intensity is at zero. Nothing was changed.";
        return r;
    }

    const float low = a.bandFraction[0], lowMid = a.bandFraction[1], presence = a.bandFraction[3], top = a.bandFraction[4];
    const bool clipping = a.clippedSamples >= 3;
    const bool hot = clipping || a.peakDb > hotPeakDb;
    const bool boomy = low > boomyLow, muddy = lowMid > muddyLowMid;
    const bool dull = presence < dullPresence;
    const bool alreadyDark = top < darkTop;

    SoundState target = current;
    auto put = [&target] (const char* id, float v) { target.set (id, v); };

    // 1. Pitch Tune: switched on, moderately firm. Strength/speed follow the intensity; nothing here comes from the recording, because
    //    the plug-in has no verified standalone pitch statistics. Key and scale are left exactly as the user set them.
    const bool on = i >= 0.15f;
    put ("tune_on", on ? 1.0f : 0.0f);
    put ("tune_strength", 60.0f + 25.0f * i);
    put ("tune_speed", 30.0f - 18.0f * i);

    // 2. Tone. Gains scale with intensity; each rule only ever REMOVES a boost/cut that the recording does not need.
    put ("eq_lo_gain", boomy ? 0.0f : 1.5f * i);
    put ("eq_b1_freq", 400.0f);
    put ("eq_b1_gain", ((boomy || muddy) ? -3.5f : -2.0f) * i);
    put ("eq_b2_freq", 3000.0f);
    put ("eq_b2_gain", dull ? 2.0f * i : (presence > forwardPresence ? -1.0f * i : 0.5f * i));
    put ("eq_hi_freq", 8000.0f);
    put ("eq_hi_gain", alreadyDark ? 0.0f : (a.sibilanceIndex > sibilant ? -4.5f : -3.0f) * i);

    // 3. Compressor, set relative to how loud this vocal is. threshold = median frame level - 3 dB (kept between -40 and -12 dB);
    //    ratio 2 / 3 / 4 for a dynamic range under 8 dB / 8-18 dB / over 18 dB (blended from 1:1 by intensity);
    //    makeup = half of the estimated gain reduction at the loud (90th percentile) level, at most 6 dB. The reduction ignores the knee: an estimate.
    const float thresh = clampf (a.dynP50Db - 3.0f, -40.0f, -12.0f);
    const float ratioTarget = a.dynamicRangeDb > 18.0f ? 4.0f : (a.dynamicRangeDb < 8.0f ? 2.0f : 3.0f);
    const float ratio = lerp (1.0f, ratioTarget, i);
    const float grLoud = std::fmax (0.0f, a.dynP90Db - thresh) * (1.0f - 1.0f / ratio);
    put ("comp_thresh", thresh);
    put ("comp_ratio", ratio);
    put ("comp_attack", 12.0f);
    put ("comp_release", 120.0f);
    put ("comp_knee", 8.0f);
    put ("comp_makeup", clampf (0.5f * grLoud, 0.0f, 6.0f));

    // 4. Saturation: up to 4 dB; none if the take is clipping or peaks within 0.5 dB of full scale; halved if it is already bright.
    put ("sat_drive", hot ? 0.0f : 4.0f * i * (top > brightTop ? 0.5f : 1.0f));

    // 5. Atmosphere (Liquid Echo = the delay, Dimension, Doubler = the width). There is no reverb in GlitchKid. When the 2-5 kHz share
    //    looks low (the words may be hard to follow) the amounts are scaled to 60 %. Time/tone/width settings are fixed ASTRO choices.
    const float amb = i * (dull ? 0.6f : 1.0f);
    put ("echo_on", on ? 1.0f : 0.0f);
    put ("echo_sync", 1.0f);
    put ("echo_div", 2.0f); // 1/4 dotted
    put ("echo_fb", 40.0f * amb);
    put ("echo_tone", 2500.0f);
    put ("echo_width", 70.0f);
    put ("echo_mix", 28.0f * amb);
    put ("dim_on", on ? 1.0f : 0.0f);
    put ("dim_width", 60.0f);
    put ("dim_detune", 6.0f);
    put ("dim_depth", 50.0f);
    put ("dim_mix", 35.0f * amb);
    put ("dbl_on", on ? 1.0f : 0.0f);
    put ("dbl_amount", 35.0f * amb);
    put ("dbl_width", 80.0f);
    put ("dbl_detune", 8.0f);
    put ("dbl_timing", 18.0f);

    // Validate: every owned value is finite and clamped to the parameter's own range; an unknown id is dropped.
    for (const auto& e : target.values)
    {
        Range range;
        if (! rangeOf (e.id, range))
            continue;
        const float before = current.get (e.id, e.value + 1.0f);
        float v = clampf (std::isfinite (e.value) ? e.value : before, range.lo, range.hi);
        if (range.snap)
            v = clampf (range.snap (v), range.lo, range.hi);
        if (std::fabs (v - before) > 1.0e-4f)
            ++r.changedCount;
        r.sound.set (e.id, v);
    }

    if (boomy) r.notes.add ("Low shelf kept flat and the 400 Hz cut deepened: the low end already looks strong.");
    else if (muddy) r.notes.add ("400 Hz cut deepened: a lot of energy sits at 120-500 Hz.");
    if (dull) r.notes.add ("Presence boosted and ambience reduced to 60 %: the 2-5 kHz share looks low, so the words may be hard to hear.");
    else if (presence > forwardPresence) r.notes.add ("No presence boost: the 2-5 kHz share already looks strong.");
    if (alreadyDark) r.notes.add ("Top end left alone: the recording already looks dark.");
    else if (a.sibilanceIndex > sibilant) r.notes.add ("Top end cut deeper: the high-frequency energy looks sibilant (estimate).");
    if (hot) r.notes.add (clipping ? "No saturation: the recording has clipped samples." : "No saturation: the peaks are within 0.5 dB of full scale.");
    else if (top > brightTop) r.notes.add ("Saturation halved: the recording is already bright.");
    r.notes.add ("Compressor set from the loudness of this take; Pitch Tune key and scale are untouched.");

    r.usable = true;
    r.message = juce::String (profileName (Profile::astroMode)) + " applied to " + juce::String (r.changedCount) + " controls. All stay editable.";
    return r;
}
}
