#include "MasterDsp.h"
#include <cmath>

namespace glitchkid::master
{
namespace
{
constexpr double pi = 3.14159265358979323846;
double sincD (double u) { return std::abs (u) < 1.0e-9 ? 1.0 : std::sin (pi * u) / (pi * u); }
float toDb (double meanSquare) { return meanSquare > 1.0e-12 ? (float) (-0.691 + 10.0 * std::log10 (meanSquare)) : LoudnessMeter::silence; }
}

// ---- true peak ----
TruePeakDetector::TruePeakDetector()
{
    for (int k = 0; k < phases; ++k)
    {
        double sum = 0.0;
        std::array<double, taps> c {};
        for (int j = 0; j < taps; ++j)
        {
            const double u = (double) j - 5.5 + (double) k / phases; // position relative to the interpolation point
            const double w = std::abs (u) < 6.0 ? 0.5 * (1.0 + std::cos (pi * u / 6.0)) : 0.0;
            c[(size_t) j] = sincD (u) * w;
            sum += c[(size_t) j];
        }
        for (int j = 0; j < taps; ++j)
            coef[(size_t) k][(size_t) j] = (float) (c[(size_t) j] / sum); // unity gain at DC for every phase
    }
}

float TruePeakDetector::push (float x) noexcept
{
    pos = (pos + 1) % taps;
    hist[(size_t) pos] = x;
    hist[(size_t) pos + taps] = x;
    const float* newest = hist.data() + pos + taps; // newest[-j] = x[n - j]
    float peak = std::abs (x);
    for (int k = 0; k < phases; ++k)
    {
        const auto& c = coef[(size_t) k];
        float y = 0.0f;
        for (int j = 0; j < taps; ++j)
            y += newest[-j] * c[(size_t) j];
        peak = std::max (peak, std::abs (y));
    }
    return peak;
}

// ---- loudness ----
void LoudnessMeter::prepare (double sampleRate)
{
    fs = sampleRate > 0.0 ? sampleRate : 48000.0;
    subLen = juce::jmax (1, (int) std::lround (fs * 0.1));
    // BS.1770-4 K-weighting, the two stages re-derived for this sample rate (the published 48 kHz coefficients are reproduced exactly at 48 kHz)
    {
        const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        const double K = std::tan (pi * f0 / fs), Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        for (auto& b : shelf)
        {
            b.b0 = (Vh + Vb * K / Q + K * K) / a0; b.b1 = 2.0 * (K * K - Vh) / a0; b.b2 = (Vh - Vb * K / Q + K * K) / a0;
            b.a1 = 2.0 * (K * K - 1.0) / a0; b.a2 = (1.0 - K / Q + K * K) / a0;
        }
    }
    {
        const double f0 = 38.13547087602444, Q = 0.5003270373238773, K = std::tan (pi * f0 / fs);
        const double a0 = 1.0 + K / Q + K * K;
        for (auto& b : highpass)
        {
            b.b0 = 1.0; b.b1 = -2.0; b.b2 = 1.0;
            b.a1 = 2.0 * (K * K - 1.0) / a0; b.a2 = (1.0 - K / Q + K * K) / a0;
        }
    }
    reset();
}

void LoudnessMeter::reset()
{
    for (auto* set : { &shelf, &highpass })
        for (auto& b : *set) b.z1 = b.z2 = 0.0;
    subSum = 0.0; subPos = 0; subCount = 0; subWrite = 0;
    subs.fill (0.0);
    histEnergy.fill (0.0);
    histCount.fill (0);
    lr = ll = rr = 0.0; corrCoef = 1.0 - std::exp (-1.0 / (0.5 * fs));
    for (auto& t : tp) t.reset();
    peakLin = tpLin = 0.0f;
    mom.store (silence); st.store (silence); integ.store (silence); spk.store (silence); tpk.store (silence); corr.store (1.0f); clips.store (0);
}

void LoudnessMeter::process (const float* left, const float* right, int numSamples) noexcept
{
    if (left == nullptr)
        return;
    int clipped = 0;
    for (int i = 0; i < numSamples; ++i)
    {
        const float l = std::isfinite (left[i]) ? left[i] : 0.0f;
        const float r = right != nullptr ? (std::isfinite (right[i]) ? right[i] : 0.0f) : 0.0f;
        const double kl = highpass[0].tick (shelf[0].tick (l));
        subSum += kl * kl;
        if (right != nullptr)
        {
            const double kr = highpass[1].tick (shelf[1].tick (r));
            subSum += kr * kr;
            lr += corrCoef * ((double) l * r - lr);
            ll += corrCoef * ((double) l * l - ll);
            rr += corrCoef * ((double) r * r - rr);
        }
        const float a = std::max (std::abs (l), std::abs (r));
        peakLin = std::max (peakLin, a);
        clipped += a >= 0.999f ? 1 : 0;
        tpLin = std::max (tpLin, tp[0].push (l));
        if (right != nullptr) tpLin = std::max (tpLin, tp[1].push (r));
        if (++subPos == subLen)
            finishSubBlock();
    }
    if (clipped > 0) clips.fetch_add (clipped, std::memory_order_relaxed);
}

void LoudnessMeter::finishSubBlock() noexcept
{
    subs[(size_t) subWrite] = subSum / subLen;
    subWrite = (subWrite + 1) % (int) subs.size();
    subCount = std::min (subCount + 1, (int) subs.size());
    subSum = 0.0;
    subPos = 0;
    auto meanOfLast = [this] (int n)
    {
        double s = 0.0;
        for (int i = 1; i <= n; ++i) s += subs[(size_t) ((subWrite - i + (int) subs.size()) % (int) subs.size())];
        return s / n;
    };
    if (subCount >= 4)
    {
        const double block = meanOfLast (4); // a 400 ms gating block, every 100 ms (75 % overlap)
        mom.store (toDb (block), std::memory_order_relaxed);
        const float l = toDb (block);
        if (l >= -70.0f)
        {
            const int bin = juce::jlimit (0, histBins - 1, (int) ((l + 70.0f) * 10.0f));
            histEnergy[(size_t) bin] += block;
            ++histCount[(size_t) bin];
        }
        double e = 0.0; long n = 0;
        for (int b = 0; b < histBins; ++b) { e += histEnergy[(size_t) b]; n += histCount[(size_t) b]; }
        if (n > 0)
        {
            const float gate = toDb (e / (double) n) - 10.0f;
            const int first = juce::jlimit (0, histBins, (int) std::ceil ((gate + 70.0f) * 10.0f - 0.5f));
            double ge = 0.0; long gn = 0;
            for (int b = first; b < histBins; ++b) { ge += histEnergy[(size_t) b]; gn += histCount[(size_t) b]; }
            integ.store (gn > 0 ? toDb (ge / (double) gn) : silence, std::memory_order_relaxed);
        }
    }
    st.store (subCount >= 30 ? toDb (meanOfLast (30)) : (subCount >= 4 ? toDb (meanOfLast (subCount)) : silence), std::memory_order_relaxed);
    spk.store (peakLin > 0.0f ? juce::Decibels::gainToDecibels (peakLin, silence) : silence, std::memory_order_relaxed);
    tpk.store (tpLin > 0.0f ? juce::Decibels::gainToDecibels (tpLin, silence) : silence, std::memory_order_relaxed);
    corr.store (ll * rr > 1.0e-12 ? (float) juce::jlimit (-1.0, 1.0, lr / std::sqrt (ll * rr)) : 1.0f, std::memory_order_relaxed);
}

// ---- width ----
void StereoWidth::prepare (double sampleRate)
{
    const double w0 = 2.0 * pi * 120.0 / sampleRate, alpha = std::sin (w0) / (2.0 * 0.70710678), cw = std::cos (w0), a0 = 1.0 + alpha;
    b0 = (1.0 + cw) * 0.5 / a0; b1 = -(1.0 + cw) / a0; b2 = b0; a1 = -2.0 * cw / a0; a2 = (1.0 - alpha) / a0; // RBJ high-pass, Butterworth
    step = (float) (1.0 / (0.02 * sampleRate));
    energyCoef = 1.0 - std::exp (-1.0 / (0.3 * sampleRate));
    reset();
}

void StereoWidth::reset() { z.fill (0.0); em = es = eh = 0.0; current = want = target; sinceCap = 0; }

void StereoWidth::process (float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || right == nullptr)
        return;
    for (int i = 0; i < numSamples; ++i)
    {
        if (--sinceCap <= 0) // never widen the side past 90 % of the mid's energy: side' = low side + width^2 x upper side
        {
            sinceCap = 32;
            want = target;
            if (want > 1.0f && eh > 1.0e-12)
                want = (float) juce::jlimit (1.0, (double) want, std::sqrt (juce::jmax (0.0, 0.9 * em - (es - eh)) / eh));
        }
        current += juce::jlimit (-step, step, want - current);
        const double m = 0.5 * ((double) left[i] + right[i]), s = 0.5 * ((double) left[i] - right[i]);
        double h = b0 * s + z[0];               // high-pass, stage 1
        z[0] = b1 * s - a1 * h + z[1];
        z[1] = b2 * s - a2 * h;
        const double h1 = h;
        h = b0 * h1 + z[2];                     // stage 2
        z[2] = b1 * h1 - a1 * h + z[3];
        z[3] = b2 * h1 - a2 * h;
        em += energyCoef * (m * m - em);
        es += energyCoef * (s * s - es);
        eh += energyCoef * (h * h - eh);
        const double side = current <= 1.0f ? s * current : s + (current - 1.0f) * h;
        left[i] = (float) (m + side);
        right[i] = (float) (m - side);
    }
}

// ---- limiter ----
void MasterLimiter::prepare (double sampleRate)
{
    fs = sampleRate > 0.0 ? sampleRate : 48000.0;
    lookahead = juce::jlimit (8, maxLookahead, (int) std::lround (fs * 0.002)); // 2 ms
    hold = lookahead + TruePeakDetector::delaySamples + 1;
    for (auto& d : delay) d.assign ((size_t) latencySamples(), 0.0f);
    minQ.assign ((size_t) hold + 1, 1.0f);
    minIdx.assign ((size_t) hold + 1, 0);
    avgRing.assign ((size_t) lookahead, 1.0f);
    reset();
}

void MasterLimiter::reset()
{
    for (auto& d : det) d.reset();
    for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
    std::fill (avgRing.begin(), avgRing.end(), 1.0f);
    avgSum = (double) lookahead;
    delayPos = avgPos = qHead = qTail = 0;
    sampleIndex = 0;
    released = 1.0f;
    grDb.store (0.0f);
}

void MasterLimiter::setSettings (float driveDb, float ceilingDbTp, float releaseMs)
{
    drive = juce::Decibels::decibelsToGain (juce::jlimit (0.0f, 12.0f, driveDb));
    ceiling = juce::Decibels::decibelsToGain (juce::jlimit (-6.0f, -0.1f, ceilingDbTp));
    releaseCoef = (float) std::exp (-1.0 / (juce::jlimit (20.0f, 1000.0f, releaseMs) * 0.001 * fs));
}

void MasterLimiter::process (float* left, float* right, int numSamples) noexcept
{
    const int cap = hold + 1, D = latencySamples();
    float minGain = 1.0f;
    for (int i = 0; i < numSamples; ++i, ++sampleIndex)
    {
        const float l = (std::isfinite (left[i]) ? left[i] : 0.0f) * drive;
        const float r = right != nullptr ? (std::isfinite (right[i]) ? right[i] : 0.0f) * drive : 0.0f;
        const float pk = std::max (det[0].push (l), right != nullptr ? det[1].push (r) : 0.0f);
        const float req = pk > ceiling ? ceiling / pk : 1.0f;
        // the smallest gain needed anywhere in the last `hold` steps (a monotonic queue)
        while (qHead != qTail && minQ[(size_t) ((qTail - 1 + cap) % cap)] >= req)
            qTail = (qTail - 1 + cap) % cap;
        minQ[(size_t) qTail] = req;
        minIdx[(size_t) qTail] = (int) (sampleIndex % 0x40000000);
        qTail = (qTail + 1) % cap;
        while (((int) (sampleIndex % 0x40000000) - minIdx[(size_t) qHead] + 0x40000000) % 0x40000000 >= hold)
            qHead = (qHead + 1) % cap;
        const float need = minQ[(size_t) qHead];
        released = need < released ? need : released + (need - released) * (1.0f - releaseCoef); // down at once, up with the release
        avgSum += (double) released - avgRing[(size_t) avgPos];
        avgRing[(size_t) avgPos] = released;
        avgPos = (avgPos + 1) % lookahead;
        const float g = (float) juce::jlimit (0.0, 1.0, avgSum / lookahead);
        minGain = std::min (minGain, g);
        // the audio, delayed so the gain arrives with its peak
        float& dl = delay[0][(size_t) delayPos];
        const float outL = dl;
        dl = l;
        left[i] = juce::jlimit (-ceiling, ceiling, outL * g);
        if (right != nullptr)
        {
            float& dr = delay[1][(size_t) delayPos];
            const float outR = dr;
            dr = r;
            right[i] = juce::jlimit (-ceiling, ceiling, outR * g);
        }
        delayPos = (delayPos + 1) % D;
    }
    avgSum = 0.0; // re-summed once per block, so the running sum cannot drift
    for (auto v : avgRing) avgSum += v;
    grDb.store (-juce::Decibels::gainToDecibels (minGain, -60.0f), std::memory_order_relaxed);
}
}
