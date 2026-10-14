#include "YinPitchDetector.h"
#include <algorithm>
#include <cmath>

namespace glitchkid::pitch
{
namespace
{
constexpr double analysisRate = 24000.0;
constexpr double hopSeconds = 0.005;
constexpr double silenceMeanSquare = 1.0e-7; // about -70 dBFS RMS
constexpr float insane = 1.0e6f;
}

void YinPitchDetector::prepare (double sampleRate, float minHz, float maxHz)
{
    sr = sampleRate;
    factor = std::max (1, (int) std::lround (sr / analysisRate));
    decRate = sr / factor;
    minLag = std::max (2, (int) std::floor (decRate / std::max (maxHz, 1.0f)));
    maxLag = std::max (minLag + 2, (int) std::ceil (decRate / std::max (minHz, 1.0f)) + 1);
    window = maxLag;
    hop = std::max (1, (int) std::lround (hopSeconds * decRate));
    windowSamples = (window + maxLag) * factor;

    ring.assign ((size_t) (window + maxLag), 0.0f);
    lin.assign (ring.size(), 0.0f);
    cmndf.assign ((size_t) maxLag + 2, 1.0f);
    reset();
}

void YinPitchDetector::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    accCount = 0;
    accSum = 0.0;
    sinceHop = 0;
    filled = 0;
    writePos = 0;
    last = Result();
}

bool YinPitchDetector::push (float x)
{
    if (! (std::abs (x) < insane)) // false for NaN and +-Inf too
        x = 0.0f;

    accSum += x;
    if (++accCount < factor)
        return false;

    ring[(size_t) writePos] = (float) (accSum / factor);
    accSum = 0.0;
    accCount = 0;
    writePos = (writePos + 1) % (int) ring.size();
    filled = std::min (filled + 1, (int) ring.size());
    if (! analysisEnabled)
        return false;

    if (++sinceHop < hop)
        return false;
    sinceHop = 0;

    if (filled < (int) ring.size())
        last = Result(); // not enough history yet
    else
        analyse();
    return true;
}

void YinPitchDetector::analyse()
{
    const int len = (int) ring.size();
    double mean = 0.0;
    for (int i = 0; i < len; ++i)
    {
        lin[(size_t) i] = ring[(size_t) ((writePos + i) % len)]; // oldest first
        mean += lin[(size_t) i];
    }
    mean /= len;

    double energy = 0.0;
    for (int i = 0; i < len; ++i)
    {
        lin[(size_t) i] = (float) (lin[(size_t) i] - mean);
        energy += (double) lin[(size_t) i] * lin[(size_t) i];
    }
    if (energy / len < silenceMeanSquare)
    {
        last = Result();
        return;
    }

    // Steps 2-3: difference function and its cumulative mean normalisation.
    cmndf[0] = 1.0f;
    double running = 0.0;
    for (int tau = 1; tau <= maxLag; ++tau)
    {
        double s = 0.0;
        for (int j = 0; j < window; ++j)
        {
            const double d = (double) lin[(size_t) j] - (double) lin[(size_t) (j + tau)];
            s += d * d;
        }
        running += s;
        cmndf[(size_t) tau] = running > 0.0 ? (float) (s * tau / running) : 1.0f;
    }

    // Step 4: first dip below the threshold, followed down to its local minimum.
    int best = -1;
    for (int tau = minLag; tau <= maxLag; ++tau)
        if (cmndf[(size_t) tau] < threshold)
        {
            while (tau + 1 <= maxLag && cmndf[(size_t) tau + 1] < cmndf[(size_t) tau])
                ++tau;
            best = tau;
            break;
        }

    Result r;
    r.voiced = best > 0;
    if (! r.voiced) // report the deepest dip anyway, but flagged as unvoiced
    {
        best = minLag;
        for (int tau = minLag; tau <= maxLag; ++tau)
            if (cmndf[(size_t) tau] < cmndf[(size_t) best])
                best = tau;
    }

    // Step 5: parabolic interpolation around the dip.
    double x = best;
    if (best > 1 && best < maxLag)
    {
        const double a = cmndf[(size_t) best - 1], b = cmndf[(size_t) best], c = cmndf[(size_t) best + 1];
        const double den = a - 2.0 * b + c;
        if (den > 1.0e-12)
            x += std::clamp (0.5 * (a - c) / den, -1.0, 1.0);
    }

    r.periodSamples = (float) (x * factor);
    r.frequencyHz = (float) (decRate / x);
    r.confidence = std::clamp (1.0f - cmndf[(size_t) best], 0.0f, 1.0f);
    if (! r.voiced)
        r.confidence = std::min (r.confidence, 0.5f);
    last = r;
}
} // namespace glitchkid::pitch
