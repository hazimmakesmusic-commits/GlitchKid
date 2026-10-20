#include "VocalAnalyzer.h"
#include <algorithm>
#include <cmath>

namespace glitchkid::analysis
{
namespace
{
constexpr double corners[4] = { 120.0, 500.0, 2000.0, 5000.0 };
constexpr float activityGateDb = -50.0f;
constexpr float clipLevel = 0.999f, nearClipLevel = 0.98f;
constexpr double eps = 1.0e-12;

float toDb (double energyPerSample) { return (float) (10.0 * std::log10 (energyPerSample + eps)); }
}

void VocalAnalyzer::Biquad::setLowpass (double sampleRate, double freq)
{
    const double w0 = 2.0 * 3.14159265358979323846 * std::min (freq, 0.45 * sampleRate) / sampleRate;
    const double cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * 0.70710678118654752);
    const double a0 = 1.0 + alpha;
    b0 = (1.0 - cw) * 0.5 / a0;
    b1 = (1.0 - cw) / a0;
    b2 = b0;
    a1 = -2.0 * cw / a0;
    a2 = (1.0 - alpha) / a0;
}

void VocalAnalyzer::prepare (double sampleRate)
{
    fs = sampleRate > 1000.0 ? sampleRate : 48000.0;
    frameLength = std::max (1, (int) std::lround (0.1 * fs));
    for (int i = 0; i < numLowpass; ++i)
        lp[(size_t) i].setLowpass (fs, corners[i]);
    reset();
    listening = false;
    request.store (0);
    status.store ((int) VocalAnalysis::Status::idle, std::memory_order_release);
}

void VocalAnalyzer::reset() noexcept
{
    framePos = 0;
    frameTotal = 0.0;
    frameBand.fill (0.0);
    bandSum.fill (0.0);
    activeEnergySum = 0.0;
    activeFrames = totalSamples = sibilantFrames = 0;
    hist.fill (0);
    peak = 0.0f;
    clipped = nearClipped = nonFinite = 0;
    for (auto& f : lp)
        f.reset();
    liveActive.store (0.0f, std::memory_order_relaxed);
    liveTotal.store (0.0f, std::memory_order_relaxed);
}

void VocalAnalyzer::process (const float* const* channels, int numChannels, int numSamples) noexcept
{
    const int req = request.exchange (0, std::memory_order_acq_rel);
    if (req == 1)
    {
        reset();
        listening = true;
        status.store ((int) VocalAnalysis::Status::listening, std::memory_order_release);
    }
    else if (req == 2 && listening)
    {
        finish();
        return;
    }
    if (! listening || numChannels < 1 || channels == nullptr)
        return;

    const int used = std::min (numChannels, 2);
    for (int i = 0; i < numSamples; ++i)
    {
        double sum = 0.0;
        int good = 0;
        for (int ch = 0; ch < used; ++ch)
        {
            const float v = channels[ch][i];
            if (! std::isfinite (v))
            {
                ++nonFinite;
                continue;
            }
            const float mag = std::fabs (v);
            peak = std::max (peak, mag);
            if (mag >= nearClipLevel)
                ++nearClipped;
            if (mag >= clipLevel)
                ++clipped;
            sum += (double) v;
            ++good;
        }
        const double x = good > 0 ? sum / (double) used : 0.0; // a non-finite channel counts as silence in the mono sum

        double l[numLowpass];
        for (int k = 0; k < numLowpass; ++k)
            l[k] = lp[(size_t) k].tick (x);
        const double band[VocalAnalysis::numBands] = { l[0], l[1] - l[0], l[2] - l[1], l[3] - l[2], x - l[3] };
        for (int b = 0; b < VocalAnalysis::numBands; ++b)
            frameBand[(size_t) b] += band[b] * band[b];
        frameTotal += x * x;
        ++totalSamples;

        if (++framePos >= frameLength)
        {
            const float rmsDb = toDb (frameTotal / (double) frameLength);
            if (rmsDb > activityGateDb)
            {
                ++activeFrames;
                activeEnergySum += frameTotal;
                for (int b = 0; b < VocalAnalysis::numBands; ++b)
                    bandSum[(size_t) b] += frameBand[(size_t) b];
                const double mid = frameBand[2] + frameBand[3];
                if (frameBand[4] > 0.5 * mid)
                    ++sibilantFrames;
                hist[(size_t) std::clamp ((int) std::lround (rmsDb) + 80, 0, histBins - 1)]++;
            }
            framePos = 0;
            frameTotal = 0.0;
            frameBand.fill (0.0);
        }
    }

    const float active = (float) ((double) activeFrames * (double) frameLength / fs);
    const float total = (float) ((double) totalSamples / fs);
    liveActive.store (active, std::memory_order_relaxed);
    liveTotal.store (total, std::memory_order_relaxed);
    if (active >= maxActiveSeconds || total >= maxTotalSeconds)
        finish();
}

void VocalAnalyzer::finish() noexcept
{
    listening = false;
    VocalAnalysis a;
    a.totalSeconds = (float) ((double) totalSamples / fs);
    a.activeSeconds = (float) ((double) activeFrames * (double) frameLength / fs);
    a.peakDb = 20.0f * std::log10 (peak + 1.0e-6f);
    a.clippedSamples = clipped;
    a.nearClippedSamples = nearClipped;
    a.nonFiniteSamples = nonFinite;

    if (totalSamples > 0 && (double) nonFinite > 0.01 * (double) totalSamples)
        a.status = VocalAnalysis::Status::invalid;
    else if (a.activeSeconds < minActiveSeconds)
        a.status = a.peakDb < -60.0f || activeFrames == 0 ? VocalAnalysis::Status::tooQuiet : VocalAnalysis::Status::tooShort;
    else
        a.status = VocalAnalysis::Status::ready;

    if (activeFrames > 0)
    {
        a.rmsDb = toDb (activeEnergySum / ((double) activeFrames * (double) frameLength));
        double bandTotal = 0.0;
        for (double b : bandSum)
            bandTotal += b;
        for (int b = 0; b < VocalAnalysis::numBands; ++b)
            a.bandFraction[(size_t) b] = bandTotal > 0.0 ? (float) (bandSum[(size_t) b] / bandTotal) : 0.0f;
        a.sibilanceIndex = (float) sibilantFrames / (float) activeFrames;

        auto percentile = [this] (double p)
        {
            long total = 0;
            for (int n : hist) total += n;
            const long target = (long) std::ceil (p * (double) total);
            long run = 0;
            for (int i = 0; i < histBins; ++i)
            {
                run += hist[(size_t) i];
                if (run >= target)
                    return (float) (i - 80);
            }
            return 0.0f;
        };
        a.dynP10Db = percentile (0.10);
        a.dynP50Db = percentile (0.50);
        a.dynP90Db = percentile (0.90);
        a.dynamicRangeDb = a.dynP90Db - a.dynP10Db;
        a.crestDb = a.peakDb - a.rmsDb;
    }
    publish (a);
}

// Field order is private to publish()/fetch(); status is written last (release) so a reader that sees a final status sees every field.
void VocalAnalyzer::publish (const VocalAnalysis& a) noexcept
{
    const float v[numFields] = { a.totalSeconds, a.activeSeconds, a.peakDb, a.rmsDb, (float) a.clippedSamples, (float) a.nonFiniteSamples,
                                 a.dynP10Db, a.dynP50Db, a.dynP90Db, a.dynamicRangeDb, a.sibilanceIndex,
                                 a.crestDb, (float) a.nearClippedSamples,
                                 a.bandFraction[0], a.bandFraction[1], a.bandFraction[2], a.bandFraction[3], a.bandFraction[4] };
    for (int i = 0; i < numFields; ++i)
        fields[(size_t) i].store (v[i], std::memory_order_relaxed);
    status.store ((int) a.status, std::memory_order_release);
    if (a.status != VocalAnalysis::Status::listening && a.status != VocalAnalysis::Status::idle)
        completed.fetch_add (1, std::memory_order_release); // one more analysis has FINISHED (any outcome)
}

VocalAnalysis VocalAnalyzer::analyzeOffline (const float* mono, int numSamples, double sampleRate)
{
    VocalAnalyzer an; // a private instance: nothing is shared with the plug-in's live analyzer
    an.prepare (sampleRate);
    an.reset();
    an.listening = true;
    constexpr int block = 4096;
    for (int pos = 0; mono != nullptr && pos < numSamples && an.listening; pos += block)
    {
        const float* ch[1] = { mono + pos };
        an.process (ch, 1, std::min (block, numSamples - pos));
    }
    if (an.listening)
        an.finish(); // short, empty or null input: finish with whatever was heard
    return an.fetch();
}

VocalAnalysis VocalAnalyzer::fetch() const
{
    VocalAnalysis a;
    a.status = (VocalAnalysis::Status) status.load (std::memory_order_acquire);
    if (a.status == VocalAnalysis::Status::idle || a.status == VocalAnalysis::Status::listening)
        return a;
    auto f = [this] (int i) { return fields[(size_t) i].load (std::memory_order_relaxed); };
    a.totalSeconds = f (0); a.activeSeconds = f (1); a.peakDb = f (2); a.rmsDb = f (3);
    a.clippedSamples = (int) f (4); a.nonFiniteSamples = (int) f (5);
    a.dynP10Db = f (6); a.dynP50Db = f (7); a.dynP90Db = f (8); a.dynamicRangeDb = f (9); a.sibilanceIndex = f (10);
    a.crestDb = f (11); a.nearClippedSamples = (int) f (12);
    for (int b = 0; b < VocalAnalysis::numBands; ++b)
        a.bandFraction[(size_t) b] = f (13 + b);
    return a;
}
}
