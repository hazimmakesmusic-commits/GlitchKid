#include "DisplaySpectrum.h"
#include <algorithm>

namespace glitchkid::analysis
{
namespace
{
constexpr float minHz = 20.0f;
float topHz (double sr) { return (float) std::min (20000.0, 0.45 * sr); }
}

DisplaySpectrum::DisplaySpectrum() : fft (fftOrder)
{
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * 3.14159265f * (float) i / (float) (fftSize - 1)); // Hann
}

float DisplaySpectrum::columnHz (int c, double sr)
{
    const float hi = topHz (sr);
    return minHz * std::pow (hi / minHz, (float) std::clamp (c, 0, columns - 1) / (float) (columns - 1));
}

void DisplaySpectrum::update (const DisplayTap& tap, double sr, float dt)
{
    if (sr < 8000.0)
        return;
    const bool live = freshness.update (tap, dt);
    const float fall = 1.2f * std::clamp (dt, 0.001f, 0.2f); // 1.2 per second: the same fall as 0.04 per 30 Hz frame
    if (! live) // nothing is being written: the display decays to empty instead of freezing on the last frame
    {
        signal = false;
        for (auto& s : shown)
            s = std::max (0.0f, s - fall);
        return;
    }
    tap.copyLatest (samples.data(), fftSize);
    float peak = 0.0f;
    for (int i = 0; i < fftSize; ++i)
    {
        const float v = std::isfinite (samples[(size_t) i]) ? samples[(size_t) i] : 0.0f;
        peak = std::max (peak, std::abs (v));
        work[(size_t) i] = v * window[(size_t) i];
    }
    signal = peak > 3.0e-5f; // about -90 dBFS
    std::fill (work.begin() + fftSize, work.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (work.data());
    const float binHz = (float) sr / (float) fftSize;
    const float ratio = std::pow (topHz (sr) / minHz, 1.0f / (float) (columns - 1));
    const float halfStep = std::sqrt (ratio);
    for (int c = 0; c < columns; ++c)
    {
        float level = 0.0f;
        if (signal)
        {
            const float f = columnHz (c, sr);
            const int b0 = std::clamp ((int) std::floor (f / halfStep / binHz), 1, fftSize / 2 - 1);
            const int b1 = std::clamp ((int) std::ceil (f * halfStep / binHz), b0, fftSize / 2 - 1);
            float m = 0.0f;
            for (int b = b0; b <= b1; ++b)
                m = std::max (m, work[(size_t) b]);
            const float db = 20.0f * std::log10 (std::max (4.0f * m / (float) fftSize, 1.0e-6f)); // a sine of amplitude A reads about A (0 dB = full scale)
            level = std::clamp ((db + 90.0f) / 84.0f, 0.0f, 1.0f);
        }
        float& s = shown[(size_t) c];
        s = level > s ? level : std::max (level, s - fall);
    }
}

void WaveColumns::update (const DisplayTap& tap, int n, float dt)
{
    if (! freshness.update (tap, dt))
    {
        lo.fill (0.0f);
        hi.fill (0.0f);
        hasSignal = false;
        return;
    }
    n = std::clamp (n, columns, (int) DisplayTap::size);
    std::array<float, DisplayTap::size> tmp;
    tap.copyLatest (tmp.data(), n);
    float peak = 0.0f;
    for (int c = 0; c < columns; ++c)
    {
        const int a = (int) ((long long) c * n / columns), b = std::max (a + 1, (int) ((long long) (c + 1) * n / columns));
        float mn = 0.0f, mx = 0.0f;
        for (int i = a; i < b && i < n; ++i)
        {
            const float v = std::isfinite (tmp[(size_t) i]) ? tmp[(size_t) i] : 0.0f;
            mn = std::min (mn, v);
            mx = std::max (mx, v);
        }
        lo[(size_t) c] = mn;
        hi[(size_t) c] = mx;
        peak = std::max (peak, std::max (-mn, mx));
    }
    hasSignal = peak > 3.0e-5f;
}
}
