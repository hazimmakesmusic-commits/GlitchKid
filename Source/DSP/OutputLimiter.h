#pragma once
#include <array>
#include <cstdint>
#include <atomic>
#include <juce_audio_basics/juce_audio_basics.h>

// OUTPUT LIMITER: the last stage of the output path. A look-ahead SAMPLE-PEAK limiter, linked across channels (one gain for both, so the stereo image
// does not move). Real-time safe: fixed arrays, no allocation, no locks.
//
// How it works. The input is multiplied by the drive and written into a delay of `lookahead` samples (about 1.5 ms). For every incoming sample the gain that
// would bring it to the ceiling is computed (ceiling / peak, or 1 below the ceiling). Over a window of lookahead + 1 samples the smallest of those gains is
// taken, then the window is averaged (a smooth ramp that reaches the needed gain exactly when the peak leaves the delay), then a release limits how fast
// the gain may rise again: gain = min (ramp, previous gain recovering toward 1). The delayed audio is multiplied by that gain. Because the gain never
// exceeds the gain that a sample in the window needs, every output SAMPLE stays at or below the ceiling (a final clamp covers float rounding only).
//
// SAMPLE PEAK, NOT TRUE PEAK. The ceiling is enforced on the samples. Between samples a reconstructed (D/A, or an oversampled) waveform can be higher: for
// content with energy near a quarter of the sample rate the true peak can be up to about 3 dB above the ceiling, and typically well under 1 dB for voice. This
// limiter does no oversampling or inter-sample detection and does not claim true-peak protection. Leave some margin (the default ceiling is -1 dBFS).
//
// Latency. While the limiter is ON it delays the output by `latencySamples()`. While it is off it adds none: the processor reports that to the host only
// while the limiter is on, and the switch is a 20 ms crossfade between the undelayed signal and the limited, delayed one (so toggling does not click; it is a
// short comb-free blend of two time-shifted copies and is not meant to be automated rapidly).
class OutputLimiter
{
public:
    static constexpr int maxLookahead = 256;

    struct Settings
    {
        bool on = false;
        float ceilingDb = -1.0f;   // -12 .. -0.1 dBFS
        float driveDb = 0.0f;      // 0 .. 24 dB of gain into the limiter (before it; it is not compensated afterwards)
        float releaseMs = 80.0f;   // 10 .. 500 ms
    };

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);
    int latencySamples() const { return lookahead; }

    // In place. `numChannels` is 1 or 2 (extra channels are left alone). Non-finite input is treated as 0.
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    // The largest gain reduction (dB, positive) applied since the last call; the audio thread raises it, the message thread consumes it.
    float consumeGainReductionDb() { return grDb.exchange (0.0f, std::memory_order_relaxed); }

private:
    static constexpr int ring = 512; // power of two > maxLookahead + 1
    int lookahead = 72;
    double sr = 48000.0;
    Settings s;
    float ceilingLin = 0.891f, driveLin = 1.0f, relCoef = 0.001f;
    std::array<float, ring> audio[2], reqRing, minRing;
    std::uint32_t pos = 0;
    float gain = 1.0f;
    juce::SmoothedValue<float> blend { 0.0f };
    std::atomic<float> grDb { 0.0f };
};
