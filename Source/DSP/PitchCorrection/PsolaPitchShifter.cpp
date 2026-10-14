#include "PsolaPitchShifter.h"
#include <algorithm>
#include <cmath>

namespace glitchkid::pitch
{
int PsolaPitchShifter::latencyFor (double sampleRate, float lowestF0Hz)
{
    const double maxP = (double) (float) (sampleRate / std::max (lowestF0Hz, 20.0f));
    return (int) std::ceil (2.5 * maxP) + 4;
}

void PsolaPitchShifter::prepare (double sampleRate, float lowestF0Hz)
{
    maxPeriod = (float) (sampleRate / std::max (lowestF0Hz, 20.0f));
    minPeriod = std::max (8.0f, (float) (sampleRate / 1200.0));
    defaultPeriod = std::clamp ((float) std::round (sampleRate / 150.0), minPeriod, maxPeriod);
    // Grains reach 1.5 periods ahead of the read point, and a grain is two periods wide: 2.5 periods
    // of delay (plus interpolation margin) guarantees every grain is complete when it is cut.
    latency = latencyFor (sampleRate, lowestF0Hz);

    size_t size = 1024;
    while ((int) size < latency + 3 * (int) std::ceil (maxPeriod) + 16)
        size *= 2;
    in.assign (size, 0.0f);
    acc.assign (size, 0.0f);
    wsum.assign (size, 0.0f);
    mask = (int64_t) size - 1;
    reset();
}

void PsolaPitchShifter::reset()
{
    std::fill (in.begin(), in.end(), 0.0f);
    std::fill (acc.begin(), acc.end(), 0.0f);
    std::fill (wsum.begin(), wsum.end(), 0.0f);
    period = defaultPeriod;
    ratio = 1.0f;
    n = 0;
    sc = 0.0;
    ac = 0.0;
    first = true;
}

void PsolaPitchShifter::setPeriod (float samples)
{
    period = std::isfinite (samples) ? std::clamp (samples, minPeriod, maxPeriod) : defaultPeriod;
}

void PsolaPitchShifter::setRatio (float r)
{
    ratio = std::isfinite (r) ? std::clamp (r, minRatio, maxRatio) : 1.0f;
}

// Catmull-Rom (cubic Hermite) read at a fractional position.
float PsolaPitchShifter::readInput (double pos) const
{
    const double fl = std::floor (pos);
    const float f = (float) (pos - fl);
    const int64_t i = (int64_t) fl;
    const float y0 = in[(size_t) ((i - 1) & mask)], y1 = in[(size_t) (i & mask)];
    const float y2 = in[(size_t) ((i + 1) & mask)], y3 = in[(size_t) ((i + 2) & mask)];
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + y1;
}

void PsolaPitchShifter::emitGrain()
{
    const double T = period;
    const int half = (int) std::ceil (T);

    // Analysis position: whole periods forward from the last grain, aiming at the input time that
    // lines up with where this grain lands in the output (constant latency). Repeats a period when
    // k = 0 (shifting up), skips one when k = 2 (shifting down).
    const double target = sc - latency;
    if (first)
    {
        ac = target;
        first = false;
    }
    else
    {
        const double k = std::max (0.0, std::floor ((target - ac) / T + 0.5));
        ac += k * T;
    }

    const int64_t isc = std::llround (sc);
    const double shift = sc - (double) isc; // sub-sample part of the placement, compensated in the read
    const double invHalf = 1.0 / half;

    for (int j = -half - 1; j <= half + 1; ++j)
    {
        const int64_t idx = isc + j;
        if (idx < n)
            continue; // already played
        const double u = ((double) j - shift) * invHalf;
        if (std::abs (u) >= 1.0)
            continue;
        const float w = (float) (0.5 + 0.5 * std::cos (3.14159265358979323846 * u));
        acc[(size_t) (idx & mask)] += w * readInput (ac - shift + (double) j);
        wsum[(size_t) (idx & mask)] += w;
    }

    sc += T / (double) ratio;
}

void PsolaPitchShifter::feed (float x)
{
    in[(size_t) (n & mask)] = x;
    ++n;
    sc = (double) n; // the first grain after feed() lands exactly at the next sample
    first = true;
}

void PsolaPitchShifter::flush()
{
    std::fill (acc.begin(), acc.end(), 0.0f);
    std::fill (wsum.begin(), wsum.end(), 0.0f);
}

float PsolaPitchShifter::process (float x)
{
    in[(size_t) (n & mask)] = x;

    while (sc - std::ceil (period) <= (double) n)
        emitGrain();

    const size_t slot = (size_t) (n & mask);
    const float w = wsum[slot];
    const float y = w > 1.0e-6f ? acc[slot] / w : 0.0f;
    acc[slot] = 0.0f;
    wsum[slot] = 0.0f;
    ++n;
    return y;
}
} // namespace glitchkid::pitch
