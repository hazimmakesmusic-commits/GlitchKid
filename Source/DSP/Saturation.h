#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "FixedDelay.h"

// Soft saturation (tanh waveshaper) with a drive control, for 1-2 channels, 4x oversampled.
//
// Curve: f(x) = tanh(k x) / tanh(k), with k = 10^(drive/20) - 1.
//   - drive 0 dB -> k = 0 -> the identity (the stage is idle).
//   - quiet material is boosted by k / tanh(k) (about +17 dB at 18 dB drive); trim with Output Gain.
//   - a full-scale input maps to full scale; the oversampling filters can overshoot that by up to ~2 dB
//     on hard-driven, square-ish waves.
//   - the curve is odd-symmetric, so it adds odd harmonics (3rd, 5th, ...) and no even ones.
//
// Aliasing: a waveshaper creates harmonics above Nyquist that fold back as inharmonic tones. The shaper
// therefore runs at 4x the sample rate (JUCE's polyphase IIR half-band oversampler) and the extra
// harmonics are filtered out before returning to the base rate. Measured against an ideal reference:
// plain tanh at 10 kHz / 6 dB drive aliases at -27 dB, this at about -90 dB.
//
// Latency: exactly `latencySamples` (constant, whether or not drive is on). When idle the audio is an
// exact delayed copy, and the stage is skipped (no oversampling CPU). Turning drive on or off fades over
// 20 ms between the exact copy and the oversampled signal.
//
// Real-time safe: all memory is allocated in prepare().
class Saturation
{
public:
    static constexpr int latencySamples = 6; // 4x IIR half-band with integer latency; verified in prepare()

    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void setDriveDb (float driveDb); // 0..18 dB; below 0.05 dB counts as off
    void process (juce::AudioBuffer<float>&); // 1-2 channels, any block size

private:
    void processChunk (juce::AudioBuffer<float>&);

    juce::dsp::Oversampling<float> oversampler { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };
    FixedDelay exactDelay;
    juce::AudioBuffer<float> exact;
    juce::SmoothedValue<float> driveDb { 0.0f }; // smoothed in dB over 20 ms
    juce::SmoothedValue<float> wet { 0.0f };     // 0 = exact delayed copy, 1 = oversampled saturation
    bool targetIsOff = true;
    bool oversamplerDirty = false;
    int maxBlock = 512;
};
