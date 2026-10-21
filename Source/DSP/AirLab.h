#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

// AIR LAB: a sibilance-aware vocal presence and air enhancer, built as PARALLEL additions to the untouched signal (no latency, no oversampling).
//
//   PRESENCE  a band-pass around 3.4 kHz (Q 0.8) added to the signal: a smooth bell-shaped lift.
//   AIR       (a) a 2nd-order high-pass above ~9 kHz added to the signal (gentle sparkle), and
//             (b) harmonics generated from the 2.5-5.5 kHz band: the band is made with a steep filter, normalised by its own envelope, passed through a polynomial
//                 (0.6 n^2 + 0.4 n^3, |n| <= 2) and scaled back by the envelope, then high-passed above ~6 kHz and low-passed at ~16 kHz. The polynomial has order 3, so the
//                 harmonics of a <= 5.5 kHz band reach at most ~16.5 kHz: below Nyquist for every rate from 44.1 kHz up, so there is nothing to fold back and no oversampler
//                 is needed. Scaling by the envelope makes the harmonic amount follow the input level (instead of growing with its cube); a small floor in the normaliser
//                 makes very quiet signals and noise floors produce almost nothing.
//   Sibilance duck: the ratio of the 7.5 kHz band's envelope to the ~1 kHz body's envelope, measured on the mono sum (so both channels are ducked identically).
//             When sibilants dominate, all added content is reduced (attack ~2 ms, release ~60 ms). This lowers the risk of harsh S sounds; it cannot remove it.
//   MIX       scales everything that is added (so 0 % = the dry signal). OUTPUT TRIM is a gain in dB applied with the module.
//   ON        a 20 ms crossfade of the added content and the trim; when off (and settled) the audio is passed through untouched, bit for bit, at no CPU cost.
//
// Safety: non-finite input samples are replaced by 0, filter states are flushed against denormals, every control is smoothed over 20 ms, and a smooth knee keeps
// the module from pushing a signal that was below full scale past it (a signal that is already louder is never reduced by the knee).
// Real-time safe: no allocation after prepare(); 1 or 2 channels; reports no latency.
class AirLab
{
public:
    struct Settings
    {
        bool on = false;
        float presence = 0.3f; // 0..1
        float air = 0.3f;      // 0..1
        float mix = 1.0f;      // 0..1
        float trimDb = 0.0f;   // -12..+12
    };

    static constexpr int maxChannels = 2;

    void prepare (double sampleRate);
    void reset();                          // clear filter memory and jump the smoothers to their targets
    void setSettings (const Settings&);
    void process (juce::AudioBuffer<float>&);

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
        Biquad presenceBp, airHp, exHp, exLp1, exLp2, harmHp1, harmHp2, harmLp;
        float exEnv = 0.0f;
    };

    Channel channels[maxChannels];
    Biquad sibBp, bodyBp; // detector, on the mono sum
    float sibEnv = 0.0f, bodyEnv = 0.0f, duck = 1.0f;
    float envAttack = 0.0f, envRelease = 0.0f, exAttack = 0.0f, exRelease = 0.0f, duckAttack = 0.0f, duckRelease = 0.0f;

    Settings target;
    juce::SmoothedValue<float> presenceGain { 0.0f }, airLinear { 0.0f }, airHarm { 0.0f }, trimGain { 1.0f }, onGain { 0.0f };
    double sampleRate = 48000.0;
};
