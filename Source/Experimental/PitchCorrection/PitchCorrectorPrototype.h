#pragma once
#include <vector>
#include "PsolaPitchShifter.h"
#include "ScaleQuantizer.h"
#include "YinPitchDetector.h"

// EXPERIMENTAL (Stage 4.1). Mono prototype that ties the three pieces together:
//   YIN detector -> (voiced? hold?) -> nearest note in key/scale -> strength + retune speed -> PSOLA shifter.
// It is NOT in the plugin's signal path. Single channel, one sample at a time, so results do not depend
// on the host's block size. Nothing is allocated, locked, logged or written to disk in process().
//
// The output is always delayed by getLatencySamples(), including bypass and dry, so switching never
// shifts timing. Non-finite or absurdly large input samples are treated as silence.
namespace glitchkid::pitch
{
class PitchCorrectorPrototype
{
public:
    struct Settings
    {
        int key = 0;                                              // 0 = C ... 11 = B
        ScaleQuantizer::Scale scale = ScaleQuantizer::Scale::chromatic;
        float strength = 1.0f;       // 0..1: how much of the distance to the target note is corrected
        float retuneSpeedMs = 20.0f; // time constant of the correction; 0 = instant ("hard" tune)
        float mix = 1.0f;            // 0 = dry (delayed), 1 = corrected
        bool bypass = false;         // bit-exact delayed dry
        float refA4Hz = 440.0f;
        float minConfidence = 0.85f; // below this a frame counts as unvoiced
        float holdMs = 60.0f;        // keep the last correction this long after the voice drops out
    };

    // Allocates. Call from the message thread, never while processing.
    void prepare (double sampleRate, float lowestF0Hz = 80.0f, float highestF0Hz = 800.0f);
    void reset();
    void setSettings (const Settings& s);

    float process (float x);
    void processBlock (const float* in, float* out, int numSamples); // in == out is fine

    int getLatencySamples() const { return shifter.getLatencySamples(); }
    size_t getMemoryBytes() const { return detector.getMemoryBytes() + shifter.getMemoryBytes() + dryDelay.size() * sizeof (float); }

    // What it is currently doing, for the harness and tests.
    struct Telemetry
    {
        float detectedHz = 0.0f, confidence = 0.0f, targetMidi = -1.0f, correctionCents = 0.0f;
        bool voiced = false;
    };
    const Telemetry& telemetry() const { return tel; }

private:
    void onNewEstimate();

    Settings s;
    double sr = 48000.0;
    YinPitchDetector detector;
    PsolaPitchShifter shifter;
    std::vector<float> dryDelay;
    size_t dryPos = 0;

    float periods[3] = { 0, 0, 0 }; // last voiced periods, for a 3-point median
    int numPeriods = 0;
    float periodNow = 0.0f;
    float targetNote = -1.0f;
    float targetCorrSemis = 0.0f, corrSemis = 0.0f;
    long samplesSinceVoiced = 1 << 30;
    float mixNow = 1.0f;
    Telemetry tel;
};
} // namespace glitchkid::pitch
