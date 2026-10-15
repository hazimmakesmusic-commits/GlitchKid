#pragma once
#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

// Synthetic voice signals and measurements shared by the pitch/formant plug-in tests: vowels with known formants, an
// independent frequency estimator (autocorrelation) and harmonic-level analysis. (Header-only; test code.)
namespace vs
{
using Vec = std::vector<float>;
constexpr double pi = 3.14159265358979323846;

struct Vowel { const char* name; double F[3], B[3]; };
inline const Vowel* vowels()
{
    static const Vowel v[] = { { "a", { 700, 1200, 2600 }, { 90, 110, 170 } },
                               { "i", { 300, 2300, 3000 }, { 60, 100, 150 } },
                               { "u", { 320, 800, 2500 }, { 60, 90, 150 } } };
    return v;
}

inline double envelope (const Vowel& v, double f, double ratio)
{
    double g = 1.0;
    for (int i = 0; i < 3; ++i)
    {
        const double Fi = v.F[i] * ratio, Q = Fi / (v.B[i] * ratio), r = f / Fi;
        g *= 1.0 / std::sqrt ((1 - r * r) * (1 - r * r) + (r / Q) * (r / Q));
    }
    return g;
}

// A vowel at fixed f0 with every formant frequency and bandwidth scaled by `ratio`; peak 0.3 for ratio 1 (the scale is
// computed from the unscaled vowel, so a reference with ratio != 1 is comparable with the original).
inline Vec vowel (const Vowel& v, double sr, int n, double f0, double ratio = 1.0)
{
    auto raw = [&] (int count, double r)
    {
        Vec x ((size_t) count);
        for (int i = 0; i < count; ++i)
        {
            double s = 0.0;
            for (int h = 1; h * f0 < 0.45 * sr; ++h)
                s += envelope (v, h * f0, r) / h * std::cos (2.0 * pi * h * f0 * i / sr);
            x[(size_t) i] = (float) s;
        }
        return x;
    };
    const auto probe = raw ((int) (sr / f0 * 3), 1.0);
    float peak = 0.0f;
    for (float s : probe)
        peak = std::max (peak, std::abs (s));
    auto x = raw (n, ratio);
    for (auto& s : x)
        s = (float) (s * 0.3 / (double) peak);
    return x;
}

inline double cents (double hz, double ref) { return 1200.0 * std::log2 (hz / ref); }

inline double rms (const float* x, int n)
{
    double s = 0;
    for (int i = 0; i < n; ++i)
        s += (double) x[i] * x[i];
    return std::sqrt (s / std::max (n, 1));
}

// Frequency of a steady periodic signal: normalised autocorrelation + parabolic peak.
inline double autocorrFreq (const float* x, int n, double sr, double nearHz)
{
    const int lo = (int) (sr / (nearHz * 1.3)), hi = (int) (sr / (nearHz / 1.3));
    const int w = n - hi - 1;
    std::vector<double> r ((size_t) hi + 2, 0.0);
    for (int lag = lo - 1; lag <= hi + 1; ++lag)
    {
        double xy = 0, xx = 0, yy = 0;
        for (int i = 0; i < w; ++i)
        {
            xy += (double) x[i] * x[i + lag];
            xx += (double) x[i] * x[i];
            yy += (double) x[i + lag] * x[i + lag];
        }
        r[(size_t) lag] = xy / std::sqrt (xx * yy + 1.0e-30);
    }
    int best = lo;
    for (int lag = lo; lag <= hi; ++lag)
        if (r[(size_t) lag] > r[(size_t) best])
            best = lag;
    const double a = r[(size_t) best - 1], b = r[(size_t) best], c = r[(size_t) best + 1];
    const double den = a - 2 * b + c;
    return sr / (best + (den < 0 ? 0.5 * (a - c) / den : 0.0));
}

inline void fft (std::vector<std::complex<double>>& a)
{
    const size_t N = a.size();
    for (size_t i = 1, j = 0; i < N; ++i)
    {
        size_t bit = N >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= N; len <<= 1)
        for (size_t i = 0; i < N; i += len)
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto w = std::polar (1.0, -2.0 * pi * (double) k / (double) len);
                const auto u = a[i + k], t = a[i + k + len / 2] * w;
                a[i + k] = u + t;
                a[i + k + len / 2] = u - t;
            }
}

// Level in dB of the first `count` harmonics of f0 (largest bin within +-2 of each), 16384-point Hann.
inline std::vector<double> harmonicDb (const Vec& x, size_t from, double f0, double sr, int count)
{
    const size_t N = 16384;
    std::vector<std::complex<double>> a (N);
    for (size_t i = 0; i < N; ++i)
        a[i] = (double) x[from + i] * (0.5 - 0.5 * std::cos (2.0 * pi * (double) i / (double) N));
    fft (a);
    std::vector<double> out;
    for (int h = 1; h <= count; ++h)
    {
        const double bin = h * f0 * (double) N / sr;
        double m = 0;
        for (int b = (int) bin - 2; b <= (int) bin + 2; ++b)
            m = std::max (m, std::abs (a[(size_t) b]));
        out.push_back (20.0 * std::log10 (m + 1.0e-12));
    }
    return out;
}

// Power-weighted mean frequency of the harmonics: moves up when the formants move up.
inline double centroid (const std::vector<double>& db, double f0)
{
    double num = 0, den = 0;
    for (size_t h = 0; h < db.size(); ++h)
    {
        const double p = std::pow (10.0, db[h] / 10.0);
        num += p * f0 * (double) (h + 1);
        den += p;
    }
    return num / den;
}
} // namespace vs
