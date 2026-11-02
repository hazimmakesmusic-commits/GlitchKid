#include "VocalDiagnosis.h"
#include "../Analysis/VocalFileAnalysis.h"
#include <algorithm>
#include <cmath>

namespace glitchkid::kid
{
namespace
{
using analysis::SpectralProfile;
juce::String hzText (float hz) { return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz"; }
juce::String db1 (float v) { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; }
float sev (float v, float lo, float hi) { return juce::jlimit (0.0f, 1.0f, (v - lo) / (hi - lo)); }
}

const Finding* Diagnosis::find (const char* id) const
{
    for (auto& f : items)
        if (f.id == id)
            return &f;
    return nullptr;
}

ToneRegions toneRegions (const std::array<float, SpectralProfile::numBands>& s)
{
    ToneRegions t;
    auto mean = [&s] (int a, int b) { float m = 0.0f; for (int i = a; i <= b; ++i) m += s[(size_t) i]; return m / (float) (b - a + 1); };
    const float body = mean (6, 9); // 800 Hz - 1.6 kHz

    // the voice's own slope through its middle (635 Hz - 4 kHz), extended down: low mids are "built up" only where they stand above it (as CLEAN LAB's detector does)
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (int b = 5; b <= 13; ++b) { sx += b; sy += s[(size_t) b]; sxx += (double) b * b; sxy += (double) b * s[(size_t) b]; }
    const double n = 9.0, slope = (n * sxy - sx * sy) / (n * sxx - sx * sx), icpt = (sy - slope * sx) / n;
    for (int b = 0; b <= 4; ++b)
    {
        const float excess = s[(size_t) b] - (float) (icpt + slope * b);
        if (b == 0 || excess > t.lowMidExcessDb) { t.lowMidExcessDb = excess; t.lowMidHz = SpectralProfile::bandCentreHz (b); }
    }
    t.upperMidDb = mean (10, 14) - body; // 2 - 5 kHz against the body
    int peak = 10;
    for (int b = 11; b <= 14; ++b)
        if (s[(size_t) b] > s[(size_t) peak]) peak = b;
    t.upperMidHz = SpectralProfile::bandCentreHz (peak);
    float others = 0.0f;
    for (int b = 10; b <= 14; ++b)
        if (b != peak) others += s[(size_t) b];
    t.upperMidPeakDb = s[(size_t) peak] - others / 4.0f; // how far one band stands out from the rest of the upper mids (a natural slope gives about 3 dB)
    t.airDb = mean (16, 18) - body; // 8 - 12.8 kHz against the body
    return t;
}

Diagnosis diagnose (const AnalysisFacts& f)
{
    Diagnosis d;
    if (! f.valid)
        return d;
    auto add = [&d] (const char* id, bool measured, float severity, const juce::String& plain, const juce::String& detail, float hz = 0.0f, bool narrow = false)
    {
        d.items.push_back ({ id, measured, severity, hz, narrow, plain, detail });
    };
    const auto& band = f.bandFraction;

    // recording problems (not fixable in the mix)
    if (f.clippedSamples > 0)
        add ("clipping", true, sev ((float) f.clippedSamples, 0.0f, 200.0f), "The recording clips: " + juce::String (f.clippedSamples) + " samples hit full scale. That's a recording problem: GlitchKid can't repair it.",
             "Samples at or above 0.999 full scale. Clipping distortion is baked into the take; re-record about 6 dB lower if you can.");
    if (f.peakDb > -120.0f && f.peakDb < -18.0f)
        add ("quiet", true, sev (-f.peakDb, 18.0f, 30.0f), "It was recorded quietly (peaks at " + juce::String (f.peakDb, 1) + " dBFS).",
             "Low recording level. KID sets compression from your real level, and never raises Gain: level is yours.");
    if (f.noiseKnown && f.noiseFloorDb > -55.0f)
        add ("noise", false, sev (f.noiseFloorDb, -55.0f, -35.0f), "There's background noise between phrases (about " + juce::String (juce::roundToInt (f.noiseFloorDb)) + " dBFS). GlitchKid has no noise reduction, so I keep compression gentle to avoid lifting it.",
             "Estimated from the quietest moments 25 dB or more below the singing (room, hiss, breaths or bleed: the analysis can't tell which). Compression and makeup gain raise it.");

    // tone
    if (f.shapeKnown)
    {
        const auto t = toneRegions (f.shapeDb);
        if (t.lowMidExcessDb >= 6.0f)
            add ("mud", false, sev (t.lowMidExcessDb, 6.0f, 12.0f), "The low mids are built up around " + hzText (t.lowMidHz) + ", which can sound muddy.",
                 "Third-octave band at " + hzText (t.lowMidHz) + " stands " + db1 (t.lowMidExcessDb) + " above your voice's own slope (fitted 635 Hz - 4 kHz). An estimate: CLEAN's low-mid detector finds the exact frequency while it listens.", t.lowMidHz);
        if (t.upperMidDb > -4.0f)
        {
            const bool narrow = t.upperMidPeakDb >= 4.0f;
            add ("harsh", false, sev (t.upperMidDb, -4.0f, 4.0f), narrow ? "The upper mids are sharp, concentrated around " + hzText (t.upperMidHz) + "."
                                                                         : "The upper mids (2-5 kHz) are forward across the whole region, which can sound harsh.",
                 "2-5 kHz sits " + db1 (t.upperMidDb) + " against the 0.8-1.6 kHz body" + (narrow ? "; the " + hzText (t.upperMidHz) + " band stands " + db1 (t.upperMidPeakDb) + " above the rest of the upper mids (a narrower problem)."
                                                                                             : juce::String ("; no single band stands out (a broad tilt, not a resonance).")),
                 t.upperMidHz, narrow);
        }
        if (t.airDb < -26.0f)
            add ("dull", false, sev (-t.airDb, 26.0f, 40.0f), "The top end is dark: there's little air above 8 kHz.", "8-12.8 kHz sits " + db1 (t.airDb) + " against the body.");
        else if (t.airDb > -10.0f)
            add ("bright", false, sev (t.airDb, -10.0f, 0.0f), "Your vocal is already bright on top, so I won't add much air.", "8-12.8 kHz sits " + db1 (t.airDb) + " against the body.");
    }
    else // the live analyser: five bands only, so no frequency is claimed
    {
        if (band[1] > 0.35f)
            add ("mud", false, sev (band[1], 0.35f, 0.6f), "There's a build-up in the low mids (120-500 Hz).", juce::String (juce::roundToInt (band[1] * 100.0f)) + " % of the energy is in 120-500 Hz (live, five-band read: no exact frequency).");
        if (band[3] > 0.28f)
            add ("harsh", false, sev (band[3], 0.28f, 0.45f), "The upper mids (2-5 kHz) are strong, which can sound harsh.", juce::String (juce::roundToInt (band[3] * 100.0f)) + " % of the energy is in 2-5 kHz (live, five-band read).");
        if (band[4] < 0.03f && band[3] < 0.08f)
            add ("dull", false, 0.5f, "The top end is dark.", "Less than 3 % of the energy above 5 kHz (live, five-band read).");
        else if (band[4] > 0.2f)
            add ("bright", false, 0.5f, "Your vocal is already bright on top, so I won't add much air.", juce::String (juce::roundToInt (band[4] * 100.0f)) + " % of the energy above 5 kHz.");
    }
    if (band[0] > 0.15f)
        add ("rumble", false, sev (band[0], 0.15f, 0.4f), "There's a lot of low end below 120 Hz; part of it may be rumble or handling noise.",
             juce::String (juce::roundToInt (band[0] * 100.0f)) + " % of the energy is below 120 Hz. Some of it can be your own low notes, so I use CLEAN's rumble filter (a 70 Hz high-pass blended in only as far as the energy below 60 Hz calls for), not a fixed cut.");
    if (f.sibilanceIndex > 0.25f)
        add ("sibilance", true, sev (f.sibilanceIndex, 0.25f, 0.6f), "The S sounds are sharp.",
             "Sibilance index " + juce::String (f.sibilanceIndex, 2) + ": the share of moments where energy above 5 kHz exceeds half of the 0.5-5 kHz energy (a rough measure).");

    // dynamics
    if (f.dynamicRangeDb > 12.0f)
        add ("dynamic", true, sev (f.dynamicRangeDb, 12.0f, 24.0f), "The level jumps around a lot between words.", "Loud-to-quiet spread (P90 - P10 of 100 ms levels) is " + juce::String (f.dynamicRangeDb, 1) + " dB.");
    else if (f.crestDb > 0.0f && f.crestDb < 10.0f && f.dynamicRangeDb < 6.0f)
        add ("compressed", false, sev (10.0f - f.crestDb, 0.0f, 5.0f), "The take already sounds heavily compressed, so I add only light compression.",
             "Crest factor " + juce::String (f.crestDb, 1) + " dB and a " + juce::String (f.dynamicRangeDb, 1) + " dB level spread: little movement left.");

    // pitch
    if (f.fromFile && f.voicedShare > 0.5f && f.offNoteCents > 15.0f)
        add ("offpitch", false, sev (f.offNoteCents, 15.0f, 35.0f), "Some notes drift off pitch; gentle correction would help.",
             "Held notes sit a median " + juce::String (juce::roundToInt (f.offNoteCents)) + " cents from the nearest note (50 = exactly between two).");
    if (f.keyReliable)
        add ("key", false, 0.0f, "Your key is probably " + analysis::keyName (f.keyTonic, f.keyMinor) + ".", "Pitch-class fit of the held notes, clearly ahead of every key with different notes. Check it on TUNE.");
    return d;
}
}
