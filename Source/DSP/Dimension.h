#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

struct DimensionSettings
{
    bool on = false;
    float widthPct = 60.0f;  // 0..100: 0 = centred thickening (mid), 100 = pure stereo spread (side)
    float detuneCents = 6.0f; // 0..20: peak pitch swing of the modulation
    float depthPct = 50.0f;  // 0..100: level of the thickening voices
    float mixPct = 100.0f;   // 0 = dry, 100 = dry + full effect
};

// Dimension: stereo thickening and widening for vocals (a Roland-"Dimension"-style dual chorus voice design).
//
//  - The input is summed to mono (mid) and high-passed at ~200 Hz (one pole, 6 dB/octave), so the effect adds little below 200 Hz and the bass stays centred.
//  - Two chorus voices are read from a short delay line: base delays 9 ms and 13 ms, each modulated by a slow sine
//    LFO (0.37 Hz and 0.53 Hz, so the movement never repeats exactly). The LFO excursion is set from Detune so
//    that the peak pitch swing equals the chosen number of cents.
//  - Voice sum = mid part M, voice difference = side part S. Width cross-fades M and S with equal power
//    (cos / sin), so the wet level does not change with Width.
//  - Output: L = dry + g (M cos t + S sin t),  R = dry + g (M cos t - S sin t), g = 0.6 x Depth x Mix.
//    The wet is symmetric, so it never pans the vocal; at Width 100 % only S is added, and S cancels exactly
//    when L and R are summed to mono (mono sum == dry). At low Width the wet is plain chorus and a mono sum
//    gets chorus colouring (up to about +-3 dB, see docs/TESTING.md).
//  - Mono layout: there is no side channel, so only the thickening (M) part is added and Width has no effect.
//  - Off (and settled): skipped, bit-exact, memory cleared once. Zero latency. NaN/Inf input is replaced by 0,
//    and only the high-pass memory is cleared if it ever overflows; the delay line only receives finite values.
//  - Detune is a creative thickening effect, not pitch correction. No allocation after prepare().
class Dimension
{
public:
    static constexpr float maxWet = 0.6f;
    static constexpr double baseDelayMs[2] = { 9.0, 13.0 };
    static constexpr double lfoHz[2] = { 0.37, 0.53 };

    void prepare (double sampleRate);
    void reset();
    void setSettings (const DimensionSettings&); // once per block; audio thread safe
    void process (juce::AudioBuffer<float>&);    // 1 or 2 channels

    bool isActive() const { return active; }

private:
    float readVoice (double delaySamples) const;

    double sampleRate = 48000.0;
    int ringSize = 0, writePos = 0;
    std::vector<float> ring;
    DimensionSettings target;
    bool active = false;
    juce::SmoothedValue<float> wet { 0.0f }, widthAngle { 0.0f }, excursion { 0.0f }; // excursion = detune in cents
    double phase[2] { 0.0, 2.1 };
    double hpX = 0.0, hpY = 0.0, hpR = 0.99;
};
