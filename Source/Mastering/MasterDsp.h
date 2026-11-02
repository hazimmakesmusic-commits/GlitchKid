#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <vector>

// Building blocks for the MASTERING chain (a finished stereo mix, never the vocal chain). Real-time safe: everything is allocated in prepare().
namespace glitchkid::master
{
// Inter-sample ("true") peak estimate, ITU-R BS.1770-4 Annex 2 style: 4x oversampling by a 12-tap-per-phase Hann-windowed sinc interpolator.
// peakAt() returns, per input sample, the largest magnitude among the sample itself and three interpolated points around 5-6 samples earlier
// (the interpolator's delay). Under-reads by at most a few tenths of a dB near Nyquist, as the standard allows for 4x.
class TruePeakDetector
{
public:
    static constexpr int taps = 12, phases = 4, delaySamples = 6;
    TruePeakDetector();
    void reset() { hist.fill (0.0f); pos = 0; }
    float push (float x) noexcept; // returns the true-peak estimate (linear) for this step

private:
    std::array<std::array<float, taps>, phases> coef {};
    std::array<float, 2 * taps> hist {}; // written twice, so the newest `taps` samples are always contiguous
    int pos = 0;
};

// Loudness to ITU-R BS.1770-4 / EBU R128: K-weighting (the standard's two biquads, re-derived for any sample rate), 100 ms sub-blocks, momentary (400 ms),
// short-term (3 s), integrated (absolute gate -70 LUFS, relative gate -10 LU; a 0.1 LU histogram, so it never allocates), sample and true peak since reset,
// and the stereo correlation (about 0.5 s). Results are published through atomics for the interface.
class LoudnessMeter
{
public:
    void prepare (double sampleRate);
    void reset();
    void process (const float* left, const float* right, int numSamples) noexcept; // right may be nullptr (mono)

    float momentaryLufs() const { return mom.load (std::memory_order_relaxed); }
    float shortTermLufs() const { return st.load (std::memory_order_relaxed); }
    float integratedLufs() const { return integ.load (std::memory_order_relaxed); }   // -inf (below -100) until a block passes the gates
    float samplePeakDb() const { return spk.load (std::memory_order_relaxed); }
    float truePeakDb() const { return tpk.load (std::memory_order_relaxed); }
    float correlation() const { return corr.load (std::memory_order_relaxed); }       // +1 mono, 0 unrelated, -1 out of phase
    int clippedSamples() const { return clips.load (std::memory_order_relaxed); }      // samples at or above 0.999 full scale since reset
    static constexpr float silence = -120.0f;

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        double tick (double x) noexcept { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    };
    static constexpr int histBins = 1000; // -70 .. +30 LUFS in 0.1 LU steps
    double fs = 48000.0;
    int subLen = 4800, subPos = 0;
    std::array<Biquad, 2> shelf, highpass;
    double subSum = 0.0;
    std::array<double, 30> subs {}; // the last 30 sub-block energies (3 s)
    int subCount = 0, subWrite = 0;
    std::array<double, histBins> histEnergy {};
    std::array<long, histBins> histCount {};
    double lr = 0.0, ll = 0.0, rr = 0.0, corrCoef = 0.0;
    std::array<TruePeakDetector, 2> tp;
    float peakLin = 0.0f, tpLin = 0.0f;
    std::atomic<float> mom { silence }, st { silence }, integ { silence }, spk { silence }, tpk { silence }, corr { 1.0f };
    std::atomic<int> clips { 0 };
    void finishSubBlock() noexcept;
};

// Mid/side width that cannot hurt mono: the mid (L+R) is never touched, so the mono sum is exactly the input's. Widening adds only the side's content
// above about 120 Hz (a 4th-order high-pass: 40 Hz is 38 dB down), so the low end is not widened (a wide low end is a vinyl and club problem);
// narrowing (width < 1) scales the whole side. Width 1 is exactly transparent.
// Widening is also capped so the side never grows past 90 % of the mid's energy (measured over ~0.3 s): a mix cannot be pushed out of phase.
class StereoWidth
{
public:
    void prepare (double sampleRate);
    void reset();
    void setWidth (float w) { target = juce::jlimit (0.0f, 2.0f, w); }
    void process (float* left, float* right, int numSamples) noexcept;

private:
    double b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    std::array<double, 4> z {}; // two cascaded high-pass biquads
    float target = 1.0f, current = 1.0f, step = 0.0f, want = 1.0f;
    double em = 0.0, es = 0.0, eh = 0.0, energyCoef = 0.0; // running energies of the mid, the side and the side's upper part
    int sinceCap = 0;
};

// The mastering limiter: look-ahead, linked across channels, TRUE-PEAK aware (the gain is computed from the 4x-oversampled detector, so inter-sample
// peaks are caught, not only samples). For every step the gain that keeps the detected peak at the ceiling is found; a window minimum over the
// look-ahead (plus the detector's delay) and a moving average of the same length make the gain arrive smoothly and exactly in time; the release lets it
// recover. The audio is delayed by latencySamples(); the gain itself is smooth, so applying it at the base rate creates no aliasing. A final clamp at
// the ceiling only catches float rounding. Drive is gain into the limiter (the Loudness control); the ceiling is a true-peak ceiling.
class MasterLimiter
{
public:
    static constexpr int maxLookahead = 512;
    void prepare (double sampleRate);
    void reset();
    void setSettings (float driveDb, float ceilingDbTp, float releaseMs);
    int latencySamples() const { return lookahead - 1 + TruePeakDetector::delaySamples + 1; }
    void process (float* left, float* right, int numSamples) noexcept; // right may be nullptr
    float gainReductionDb() const { return grDb.load (std::memory_order_relaxed); } // the largest reduction in the last block (positive dB)

private:
    double fs = 48000.0;
    int lookahead = 96, hold = 103;
    float drive = 1.0f, ceiling = 0.891f, releaseCoef = 0.999f;
    std::array<TruePeakDetector, 2> det;
    std::array<std::vector<float>, 2> delay;
    int delayPos = 0;
    std::vector<float> reqRing, minQ; std::vector<int> minIdx;
    int reqPos = 0, qHead = 0, qTail = 0;
    long sampleIndex = 0;
    std::vector<float> avgRing;
    int avgPos = 0;
    double avgSum = 0.0;
    float released = 1.0f;
    std::atomic<float> grDb { 0.0f };
};
}
