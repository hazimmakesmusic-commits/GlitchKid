#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>

// AIR LAB: a vocal enhancer with two independent controls, built as PARALLEL additions to the untouched signal (no latency, no oversampling).
// Original design; it does not reproduce any other product's processing.
//
//   MID AIR   (clarity, presence; ~1.5-6 kHz)
//             - a lift around 3.2 kHz (a band-pass added to the signal) whose depth ADAPTS to the voice: compared with the body of the voice (~1 kHz),
//               a weak presence band is lifted more and a strong (harsh) one less, so it opens up dull moments without sharpening hard ones;
//             - a little density: harmonics of the 1.2-2.4 kHz band (envelope-normalised Chebyshev 2nd/3rd-order shaping) kept between 2.4 and 6 kHz.
//   HIGH AIR  (brilliance, air; above ~8 kHz)
//             - an air lift (a high-pass above 8.5 kHz added to the signal) that adapts the same way, compared with the air already there;
//             - harmonics generated from the 2.5-5.5 kHz band and kept between 7.5 and 16 kHz: content a dark recording does not have.
//               The source band is limited (24 dB/oct above 5.5 kHz) and the shaping is 3rd order, so every harmonic stays under ~16.5 kHz:
//               nothing folds back at 44.1 kHz and up, and no oversampler is needed.
//   Protection, shared: S sounds duck the added highs (the 7.5 kHz band against the body, measured on the mono sum; MID AIR is ducked half as much),
//   and a noise gate keeps the gaps between phrases, the room tone and quiet breaths from being lifted: nothing is added while the voice is more than
//   30 dB under its own recent peak or under -60 dBFS. The adaptive depths move with 30 ms / 150 ms smoothing, so they never flutter.
//   MIX scales everything that is added (0 % = the dry signal). OUTPUT is a gain in dB applied with the module. ON crossfades over 20 ms; off and settled,
//   the audio passes untouched at no CPU cost. getMidLiftDb / getHighLiftDb: the lift each is applying right now, at its centre (for the interface).
//
// Safety: non-finite input is replaced by 0, filter states are flushed against denormals, every control is smoothed over 20 ms, and a smooth knee keeps
// the module from pushing a signal that was below full scale past it (a signal that is already louder is never reduced by the knee).
// Real-time safe: no allocation after prepare(); 1 or 2 channels; reports no latency.
class AirLab
{
public:
    struct Settings
    {
        bool on = false;
        float presence = 0.3f; // MID AIR 0..1
        float air = 0.3f;      // HIGH AIR 0..1
        float mix = 1.0f;      // 0..1
        float trimDb = 0.0f;   // OUTPUT -12..+12
    };

    static constexpr int maxChannels = 2;

    void prepare (double sampleRate);
    void reset();                          // clear filter memory and jump the smoothers to their targets
    void setSettings (const Settings&);
    void process (juce::AudioBuffer<float>&);
    float getMidLiftDb() const { return midNowDb.load (std::memory_order_relaxed); }
    float getHighLiftDb() const { return highNowDb.load (std::memory_order_relaxed); }

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void lowpass (double fs, double f, double q);
        void highpass (double fs, double f, double q);
        void bandpass (double fs, double f, double q); // constant 0 dB peak gain
        void clear() { z1 = z2 = 0.0; }
        float process (float x)
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            if (std::abs (z1) < 1.0e-25) z1 = 0.0; // denormal guard
            if (std::abs (z2) < 1.0e-25) z2 = 0.0;
            return (float) y;
        }
    };

    struct Channel
    {
        Biquad midBp, midExHp, midExLp, midHarmHp, midHarmLp, airHp, exHp, exLp1, exLp2, harmHp1, harmHp2, harmLp;
        float exEnv = 0.0f, midExEnv = 0.0f;
        void clear();
    };

    struct SharedFloat // an atomic float that can be copied with the module
    {
        std::atomic<float> v { 0.0f };
        SharedFloat() = default;
        SharedFloat (const SharedFloat& o) : v (o.v.load (std::memory_order_relaxed)) {}
        SharedFloat& operator= (const SharedFloat& o) { v.store (o.v.load (std::memory_order_relaxed), std::memory_order_relaxed); return *this; }
        void store (float x, std::memory_order m) { v.store (x, m); }
        float load (std::memory_order m) const { return v.load (m); }
    };

    void clearState();

    Channel channels[maxChannels];
    Biquad sibBp, bodyBp, presDet, airDet; // detectors, on the mono sum
    float sibEnv = 0.0f, bodyEnv = 0.0f, presEnv = 0.0f, airEnv = 0.0f, fullEnv = 0.0f, peakEnv = 0.0f, duck = 1.0f;
    float midDyn = 0.6f, highDyn = 0.6f, gate = 0.0f;
    float envAttack = 0.0f, envRelease = 0.0f, exAttack = 0.0f, exRelease = 0.0f, duckAttack = 0.0f, duckRelease = 0.0f, fullRelease = 0.0f, peakRelease = 0.0f;
    float dynUp = 0.0f, dynDown = 0.0f, gateOpen = 0.0f, gateClose = 0.0f;
    int blockLeft = 0;
    SharedFloat midNowDb, highNowDb;

    Settings target;
    juce::SmoothedValue<float> presenceGain { 0.0f }, airLinear { 0.0f }, airHarm { 0.0f }, trimGain { 1.0f }, onGain { 0.0f };
    double sampleRate = 48000.0;
};
