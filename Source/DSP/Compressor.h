#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

struct CompressorSettings
{
    float thresholdDb = 0.0f;
    float ratio = 1.0f;       // 1 = off
    float attackMs = 10.0f;   // time constant: gain reduction reaches ~63% of a step in this time
    float releaseMs = 100.0f; // time constant: gain reduction falls to ~37% in this time
    float kneeDb = 6.0f;      // soft-knee width centred on the threshold
    float makeupDb = 0.0f;
};

// Feed-forward compressor for 1-2 channels, stereo-linked (both channels get the same gain).
// Level detector: instantaneous peak of the louder channel, in dB.
// Gain computer: static curve with a quadratic soft knee; ballistics: one-pole smoothing of the gain
// reduction in dB with separate attack and release ("smooth branching" detector).
// Reference: Giannoulis, Massberg & Reiss, "Digital Dynamic Range Compressor Design", JAES 2012.
// No lookahead, so latency is 0 samples; a very fast transient can overshoot by the attack time.
// Real-time safe: no allocation, no locks. Ratio 1 with no makeup and no remaining gain reduction is
// skipped entirely, so the compressor is bit-exact (and free) when it is not being used.
class Compressor
{
public:
    void prepare (double sampleRate);
    void reset();
    void setSettings (const CompressorSettings&);
    void process (juce::AudioBuffer<float>&);

    // Largest gain reduction (positive dB) applied during the last process() call.
    float getGainReductionDb() const { return blockMaxReductionDb; }

    // Gain reduction (positive dB) the static curve asks for at a given input level.
    static float staticReductionDb (float inputLevelDb, const CompressorSettings&);

private:
    void updateCoefficients();

    CompressorSettings settings;
    double sampleRate = 48000.0;
    float attackCoef = 0.0f, releaseCoef = 0.0f;
    float reductionDb = 0.0f; // smoothed gain reduction state
    float blockMaxReductionDb = 0.0f;
    bool makeupIsUnity = true;
    juce::SmoothedValue<float> makeupGain { 1.0f }; // linear, smoothed
};
