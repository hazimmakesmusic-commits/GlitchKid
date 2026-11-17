#include "BeatLockAnalysis.h"
#include <algorithm>
#include <cmath>

namespace glitchkid::beatlock
{
// ---------------------------------------------------------------- FIFO
void AnalysisFifo::write (const float* v, const float* i, const float* g, int n) noexcept
{
    if (! armed.load (std::memory_order_acquire) || n <= 0)
        return;
    int start1, size1, start2, size2;
    fifo.prepareToWrite (n, start1, size1, start2, size2);
    const int written = size1 + size2;
    auto copy = [&] (int dst, int src, int len)
    {
        std::copy (v + src, v + src + len, vocal.begin() + dst);
        std::copy (i + src, i + src + len, inst.begin() + dst);
        std::copy (g + src, g + src + len, gate.begin() + dst);
    };
    if (size1 > 0) copy (start1, 0, size1);
    if (size2 > 0) copy (start2, size1, size2);
    fifo.finishedWrite (written);
    if (written < n)
        droppedCount.fetch_add (n - written, std::memory_order_relaxed);
}

int AnalysisFifo::read (float* v, float* i, float* g, int max) noexcept
{
    int start1, size1, start2, size2;
    fifo.prepareToRead (max, start1, size1, start2, size2);
    auto copy = [&] (int src, int dst, int len)
    {
        std::copy (vocal.begin() + src, vocal.begin() + src + len, v + dst);
        std::copy (inst.begin() + src, inst.begin() + src + len, i + dst);
        std::copy (gate.begin() + src, gate.begin() + src + len, g + dst);
    };
    if (size1 > 0) copy (start1, 0, size1);
    if (size2 > 0) copy (start2, size1, size2);
    fifo.finishedRead (size1 + size2);
    return size1 + size2;
}

void AnalysisFifo::clear() noexcept
{
    fifo.reset();
    droppedCount.store (0, std::memory_order_relaxed);
}

// ---------------------------------------------------------------- analysis
const std::vector<float>& ConflictAnalysis::regionCentres()
{
    static const std::vector<float> c = [] {
        std::vector<float> v;
        for (int k = 0; k <= 21; ++k) // 100 Hz * 2^(k/3): 100 Hz ... 12.7 kHz
            v.push_back (100.0f * std::pow (2.0f, (float) k / 3.0f));
        return v;
    }();
    return c;
}

ConflictAnalysis::ConflictAnalysis()
    : window ((size_t) fftSize), bufV ((size_t) fftSize), bufI ((size_t) fftSize), bufG ((size_t) fftSize), work ((size_t) fftSize * 2)
{
    for (int k = 0; k < fftSize; ++k)
        window[(size_t) k] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) k / (float) fftSize);
}

void ConflictAnalysis::start (Mode m, double sampleRate, float targetActiveSeconds, float timeoutSeconds)
{
    currentMode = m;
    rate = sampleRate > 1000.0 ? sampleRate : 48000.0;
    target = targetActiveSeconds;
    timeout = timeoutSeconds;
    active = total = 0.0f;
    fill = 0;
    const auto n = regionCentres().size();
    sumV.assign (n, 0.0);
    sumI.assign (n, 0.0);
    conflictFrames.assign (n, 0);
    activeFrames = 0;
    gapHistogram.fill (0);
    gapCount = 0;
    started = true;
    done = false;
}

float ConflictAnalysis::progress() const
{
    return juce::jlimit (0.0f, 1.0f, active / std::max (0.1f, target));
}

void ConflictAnalysis::feed (const float* v, const float* i, const float* g, int n)
{
    for (int k = 0; k < n && running(); ++k)
    {
        bufV[(size_t) fill] = std::isfinite (v[k]) ? v[k] : 0.0f;
        bufI[(size_t) fill] = std::isfinite (i[k]) ? i[k] : 0.0f;
        bufG[(size_t) fill] = g[k];
        if (++fill == fftSize)
        {
            frame();
            // 50 % overlap: keep the second half
            std::copy (bufV.begin() + hop, bufV.end(), bufV.begin());
            std::copy (bufI.begin() + hop, bufI.end(), bufI.begin());
            std::copy (bufG.begin() + hop, bufG.end(), bufG.begin());
            fill = hop;
        }
    }
}

void ConflictAnalysis::frame()
{
    const float hopSeconds = (float) (hop / rate);
    total += hopSeconds;
    double gsum = 0.0;
    for (const float x : bufG) gsum += x;
    const bool isActive = gsum / fftSize >= 0.6;
    if (isActive)
    {
        active += hopSeconds;
        const auto& centres = regionCentres();
        const size_t nr = centres.size();
        std::vector<double> pv (nr, 0.0), pi (nr, 0.0); // message thread: allocation here is fine
        const double binHz = rate / fftSize, norm = 4.0 / ((double) fftSize * fftSize); // Hann: a sine of amplitude A reads about A^2 in its bins' sum
        for (int which = 0; which < 2; ++which)
        {
            const auto& src = which == 0 ? bufV : bufI;
            std::fill (work.begin(), work.end(), 0.0f);
            for (int k = 0; k < fftSize; ++k)
                work[(size_t) k] = src[(size_t) k] * window[(size_t) k];
            fft.performFrequencyOnlyForwardTransform (work.data(), true);
            auto& p = which == 0 ? pv : pi;
            for (size_t r = 0; r < nr; ++r)
            {
                const double lo = centres[r] / std::pow (2.0, 1.0 / 6.0), hi = centres[r] * std::pow (2.0, 1.0 / 6.0);
                const int b0 = std::max (1, (int) std::ceil (lo / binHz)), b1 = std::min (fftSize / 2, (int) std::floor (hi / binHz));
                double acc = 0.0;
                for (int b = b0; b <= b1; ++b)
                    acc += (double) work[(size_t) b] * work[(size_t) b];
                p[r] = acc * norm;
            }
        }
        double vMax = 1.0e-20;
        for (const double x : pv) vMax = std::max (vMax, x);
        const double vMaxDb = 10.0 * std::log10 (vMax);
        for (size_t r = 0; r < nr; ++r)
        {
            sumV[r] += pv[r];
            sumI[r] += pi[r];
            const double vDb = 10.0 * std::log10 (pv[r] + 1.0e-20), iDb = 10.0 * std::log10 (pi[r] + 1.0e-20);
            const bool relevant = vDb > std::max (vMaxDb - 20.0, -70.0);
            if (relevant && iDb > -70.0)
            {
                if (iDb >= vDb - 6.0)
                    ++conflictFrames[r];
                const int bin = juce::jlimit (0, 160, (int) std::lround (iDb - vDb) + 80);
                ++gapHistogram[(size_t) bin];
                ++gapCount;
            }
        }
        ++activeFrames;
    }
    if (active >= target || total >= timeout)
        done = true;
}

AnalysisResult ConflictAnalysis::result() const
{
    AnalysisResult r;
    r.activeSeconds = active;
    r.totalSeconds = total;
    if (active < minActiveSeconds || activeFrames == 0)
    {
        r.status = AnalysisResult::Status::insufficientVocal;
        r.message = "Not enough vocal was heard (" + juce::String (active, 1) + " s active in " + juce::String (total, 1)
                    + " s; at least " + juce::String (minActiveSeconds, 0) + " s is needed). Play a section where the vocal and the beat play together.";
        return r;
    }
    r.status = AnalysisResult::Status::ready;
    const auto& centres = regionCentres();
    double vMax = -200.0;
    for (size_t k = 0; k < centres.size(); ++k)
        vMax = std::max (vMax, 10.0 * std::log10 (sumV[k] / activeFrames + 1.0e-20));
    for (size_t k = 0; k < centres.size(); ++k)
    {
        Region g;
        g.centreHz = centres[k];
        g.vocalDb = (float) (10.0 * std::log10 (sumV[k] / activeFrames + 1.0e-20));
        g.instDb = (float) (10.0 * std::log10 (sumI[k] / activeFrames + 1.0e-20));
        g.conflictPct = 100.0f * (float) conflictFrames[k] / (float) activeFrames;
        g.vocalRelevant = g.vocalDb > std::max (vMax - 20.0, -70.0);
        r.regions.push_back (g);
    }
    std::vector<int> order;
    for (int k = 0; k < (int) r.regions.size(); ++k)
        if (r.regions[(size_t) k].conflictPct > 0.0f && r.regions[(size_t) k].vocalRelevant)
            order.push_back (k);
    std::sort (order.begin(), order.end(), [&] (int a, int b) { return r.regions[(size_t) a].conflictPct > r.regions[(size_t) b].conflictPct; });
    for (int k = 0; k < std::min (3, (int) order.size()); ++k)
        r.strongest.push_back (order[(size_t) k]);
    if (gapCount > 0)
    {
        int acc = 0;
        for (int b = 0; b < (int) gapHistogram.size(); ++b)
            if ((acc += gapHistogram[(size_t) b]) * 2 >= gapCount)
            {
                r.medianInstMinusVocalDb = (float) (b - 80);
                break;
            }
    }
    r.message = "Measured " + juce::String (active, 1) + " s of active vocal (" + juce::String (total, 1) + " s in total).";
    return r;
}

LearnProposal ConflictAnalysis::propose (const AnalysisResult& r, const std::array<float, numBands>& current)
{
    LearnProposal p;
    if (r.status != AnalysisResult::Status::ready || r.regions.empty())
    {
        p.reasons.add (r.message.isNotEmpty() ? r.message : juce::String ("No measurement to learn from."));
        return p;
    }
    // score: how often a region conflicted, among the regions where the vocal really has energy
    std::vector<int> byConflict, byVocal;
    for (int k = 0; k < (int) r.regions.size(); ++k)
        if (r.regions[(size_t) k].vocalRelevant)
        {
            if (r.regions[(size_t) k].conflictPct >= 5.0f) byConflict.push_back (k);
            byVocal.push_back (k);
        }
    std::sort (byConflict.begin(), byConflict.end(), [&] (int a, int b) { return r.regions[(size_t) a].conflictPct > r.regions[(size_t) b].conflictPct; });
    std::sort (byVocal.begin(), byVocal.end(), [&] (int a, int b) { return r.regions[(size_t) a].vocalDb > r.regions[(size_t) b].vocalDb; });
    std::vector<float> chosen;
    auto farEnough = [&] (float hz, float octaves) {
        for (const float c : chosen)
            if (std::abs (std::log2 (hz / c)) < octaves) return false;
        return true;
    };
    int fromConflict = 0, fromVocal = 0, fromCurrent = 0;
    // widest spacing first, then closer, so the eight bands stay distinct; the current settings fill what the measurement cannot justify
    for (const float spacing : { 0.6f, 0.35f, 0.2f })
    {
        for (const int k : byConflict)
            if ((int) chosen.size() < numBands && farEnough (r.regions[(size_t) k].centreHz, spacing)) { chosen.push_back (r.regions[(size_t) k].centreHz); ++fromConflict; }
        for (const int k : byVocal)
            if ((int) chosen.size() < numBands && spacing >= 0.6f && farEnough (r.regions[(size_t) k].centreHz, spacing)) { chosen.push_back (r.regions[(size_t) k].centreHz); ++fromVocal; }
        for (const float c : current)
            if ((int) chosen.size() < numBands && farEnough (c, spacing)) { chosen.push_back (c); ++fromCurrent; }
    }
    for (int k = 0; (int) chosen.size() < numBands && k < 64; ++k) // still short (current bands bunched together): spread new ones over the range
    {
        const float hz = 150.0f * std::pow (2.0f, (float) k * 0.25f);
        if (hz <= 9000.0f && farEnough (hz, 0.2f)) { chosen.push_back (hz); ++fromCurrent; }
    }
    std::sort (chosen.begin(), chosen.end());
    for (int b = 0; b < numBands; ++b)
        p.freqHz[(size_t) b] = juce::jlimit (100.0f, 12000.0f, std::round (chosen[(size_t) b]));
    p.sensitivityDb = juce::jlimit (-12.0f, 12.0f, std::round ((-3.0f - r.medianInstMinusVocalDb) * 2.0f) / 2.0f);
    p.valid = true;
    p.reasons.add (juce::String (fromConflict) + " band(s) placed where the beat most often covered the vocal, " + juce::String (fromVocal)
                   + " where the vocal is strongest" + (fromCurrent > 0 ? ", " + juce::String (fromCurrent) + " kept from your settings." : "."));
    p.reasons.add ("Sensitivity from the measured median gap: the beat sits " + juce::String (r.medianInstMinusVocalDb, 0)
                   + " dB relative to the vocal in the regions where the vocal lives.");
    return p;
}
}
