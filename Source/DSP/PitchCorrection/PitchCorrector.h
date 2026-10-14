#pragma once
#include <vector>
#include "PsolaPitchShifter.h"
#include "ScaleQuantizer.h"
#include "YinPitchDetector.h"

// Vocal pitch corrector (Stage 4.1 prototype, productised in Stage 4.2). Mono or stereo:
//   YIN detector (on the channel average) -> voiced? hold? -> nearest note in key/scale -> strength + retune speed
//   -> one PSOLA shifter per channel, all driven by the same period and ratio so the channels stay coherent.
// One sample at a time, so the result does not depend on the host's block size. Nothing is allocated, locked,
// logged or written to disk in process() or feed().
//
// The corrected output is delayed by getLatencySamples(); with mix < 1 or bypass, the dry part is delayed by the same
// amount. Non-finite or absurdly large input samples are treated as silence.
namespace glitchkid::pitch
{
class PitchCorrector
{
public:
    static constexpr int maxChannels = 2;
    static constexpr float defaultLowestHz = 80.0f, defaultHighestHz = 800.0f;

    // The latency prepare() will give, so a host-facing owner can report it without a prepared instance.
    static int latencyFor (double sampleRate, float lowestF0Hz = defaultLowestHz)
    {
        return PsolaPitchShifter::latencyFor (sampleRate, lowestF0Hz);
    }

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
    void prepare (double sampleRate, float lowestF0Hz = defaultLowestHz, float highestF0Hz = defaultHighestHz);
    void reset();
    void setSettings (const Settings& s);

    // Correct in place. channels: 1 or 2 pointers. Output = corrected audio, delayed by getLatencySamples().
    void process (float* const* channels, int numChannels, int numSamples);
    void processBlock (const float* in, float* out, int numSamples); // mono convenience, in == out is fine

    // Idle mode: keep the history up to date without correcting (cheap), so process() can start straight away
    // with no gap. The first process() after feed() starts from a clean decision state.
    void feed (const float* const* channels, int numChannels, int numSamples);

    int getLatencySamples() const { return shifters[0].getLatencySamples(); }
    size_t getMemoryBytes() const
    {
        return detector.getMemoryBytes() + 2 * shifters[0].getMemoryBytes() + 2 * dryDelay[0].size() * sizeof (float);
    }

    // What it is currently doing, for the harness, the UI and tests.
    struct Telemetry
    {
        float detectedHz = 0.0f, confidence = 0.0f, targetMidi = -1.0f, correctionCents = 0.0f;
        float detectedMidi = -1.0f; // MIDI note number of the detected pitch (fractional), -1 when none
        bool voiced = false;
    };
    const Telemetry& telemetry() const { return tel; }

private:
    void onNewEstimate();
    void resetDecisionState();

    Settings s;
    double sr = 48000.0;
    YinPitchDetector detector;
    PsolaPitchShifter shifters[maxChannels];
    std::vector<float> dryDelay[maxChannels];
    size_t dryPos = 0;
    bool idle = false;

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
