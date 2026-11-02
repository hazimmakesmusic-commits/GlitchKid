#pragma once
#include "../Analysis/ReferenceAnalyzer.h"
#include <array>
#include <vector>

namespace glitchkid::kid
{
// What KID knows about the recording: only fields the engine really measures (VocalAnalysis live or from a file, VocalFileReport, CleanLab telemetry).
struct AnalysisFacts
{
    bool valid = false;
    float activeSeconds = 0.0f, sibilanceIndex = 0.0f, dynamicRangeDb = 0.0f, mudHz = 0.0f; // mudHz: CLEAN's detected mud frequency, 0 = not measured (CLEAN off)
    std::array<float, 5> bandFraction {};  // < 120 Hz, 120-500, 500-2k, 2k-5k, > 5k
    int clippedSamples = 0;
    float peakDb = -120.0f, rmsDb = -120.0f, crestDb = 0.0f;
    // From a loaded vocal file (VocalFileReport) when there is one; estimates, used only as stated.
    bool fromFile = false;
    bool keyReliable = false;            // only then may KID set Pitch Tune's key and scale
    int keyTonic = 0;
    bool keyMinor = false;
    bool noiseKnown = false;
    float noiseFloorDb = -120.0f;
    float voicedShare = 0.0f, offNoteCents = 0.0f;
    bool shapeKnown = false;             // the third-octave tonal shape (file only)
    std::array<float, analysis::SpectralProfile::numBands> shapeDb {};
};

// VOCAL DIAGNOSIS: findings drawn from the facts, each with how sure it is (measured or estimated), a plain sentence and the technical detail behind it.
// The thresholds are documented production heuristics for an isolated vocal, not standards: they decide whether something is worth mentioning or correcting.
struct Finding
{
    juce::String id;      // clipping, quiet, noise, rumble, mud, harsh, sibilance, dull, bright, dynamic, compressed, offpitch, key, mono
    bool measured = true; // false = an estimate (an inference from measurements)
    float severity = 0.0f; // 0..1
    float hz = 0.0f;       // where it is, when the analysis supports a frequency (0 = no frequency claimed)
    bool narrow = false;   // harshness concentrated in one third-octave band rather than spread over the upper mids
    juce::String plain, detail;
};

struct Diagnosis
{
    std::vector<Finding> items;
    const Finding* find (const char* id) const;
    bool has (const char* id) const { return find (id) != nullptr; }
    bool empty() const { return items.empty(); }
};

Diagnosis diagnose (const AnalysisFacts&);

// Tone regions from the third-octave shape (dB relative to the 800 Hz - 1.6 kHz body of the voice). Public for the tests.
struct ToneRegions { float lowMidExcessDb = 0.0f, lowMidHz = 0.0f, upperMidDb = 0.0f, upperMidHz = 0.0f, upperMidPeakDb = 0.0f, airDb = 0.0f; };
ToneRegions toneRegions (const std::array<float, analysis::SpectralProfile::numBands>& shapeDb);
}
