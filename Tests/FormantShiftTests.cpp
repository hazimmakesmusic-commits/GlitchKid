#include <juce_core/juce_core.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <limits>
#include <new>
#include <random>
#include <thread>
#include "../Source/Experimental/FormantShifting/FormantShifter.h"

// Stage 4.3: tests for the experimental formant shifter. Synthetic signals only: they prove the maths (envelope moves,
// harmonics and pitch stay, safety), not how a real voice sounds. Real vocals need the listening test in
// docs/FORMANT_SHIFTING_FEASIBILITY.md.

// --- allocation counter -------------------------------------------------------------------------------------------
// Counts every heap allocation in this test program, so a test can prove that process() allocates nothing.
// (Replacing the global operator new is allowed; these forward to malloc/free so every other allocation still works.)
static std::atomic<long> g_allocations { 0 };

// GCC sees (after inlining) a standard-library new paired with our free() and warns, although the pairing is consistent.
#if defined(__GNUC__) || defined(__clang__)
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#endif

void* operator new (std::size_t n)
{
    g_allocations.fetch_add (1, std::memory_order_relaxed);
    if (void* p = std::malloc (n != 0 ? n : 1))
        return p;
    throw std::bad_alloc();
}
void* operator new[] (std::size_t n) { return ::operator new (n); }
void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }
void* operator new (std::size_t n, std::align_val_t a)
{
    g_allocations.fetch_add (1, std::memory_order_relaxed);
    void* p = nullptr;
    if (posix_memalign (&p, std::max ((std::size_t) a, sizeof (void*)), n != 0 ? n : 1) != 0)
        throw std::bad_alloc();
    return p;
}
void* operator new[] (std::size_t n, std::align_val_t a) { return ::operator new (n, a); }
void operator delete (void* p, std::align_val_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::align_val_t) noexcept { std::free (p); }
void operator delete (void* p, std::size_t, std::align_val_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t, std::align_val_t) noexcept { std::free (p); }

#if defined(__GNUC__) || defined(__clang__)
 #pragma GCC diagnostic pop
#endif

using namespace glitchkid::formant;

namespace
{
using Vec = std::vector<float>;
constexpr double pi = 3.14159265358979323846;
constexpr float qNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float posInf = std::numeric_limits<float>::infinity();
constexpr float negInf = -std::numeric_limits<float>::infinity();

// --- test signals -------------------------------------------------------------------------------------------------
// A vowel with known formants: harmonics of f0 (falling 6 dB per octave) shaped by three resonances. `ratio` scales every
// formant frequency and bandwidth, so vowel(.., 1.0) shifted by the plug-in should resemble vowel(.., ratio).
struct Vowel { const char* name; double F[3], B[3]; };
const Vowel vowels[] = { { "a", { 700, 1200, 2600 }, { 90, 110, 170 } },
                         { "i", { 300, 2300, 3000 }, { 60, 100, 150 } },
                         { "u", { 320, 800, 2500 }, { 60, 90, 150 } } };

double envelope (const Vowel& v, double f, double ratio)
{
    double g = 1.0;
    for (int i = 0; i < 3; ++i)
    {
        const double Fi = v.F[i] * ratio, Q = Fi / (v.B[i] * ratio), r = f / Fi;
        g *= 1.0 / std::sqrt ((1 - r * r) * (1 - r * r) + (r / Q) * (r / Q));
    }
    return g;
}

// `f0At` lets a test jump or glide the pitch; phases are accumulated in double (additive synthesis, band-limited).
template <typename F>
Vec vowelSignal (const Vowel& v, double sr, int n, double ratio, F f0At, double scale)
{
    Vec x ((size_t) n);
    double phase = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double f0 = f0At (i);
        phase += f0 / sr;
        phase -= std::floor (phase);
        double s = 0.0;
        for (int h = 1; h * f0 < 0.45 * sr; ++h)
            s += envelope (v, h * f0, ratio) / h * std::cos (2.0 * pi * h * phase);
        x[(size_t) i] = (float) (s * scale);
    }
    return x;
}

// Scale that gives the unshifted vowel a peak of 0.3 (the same scale is used for the reference).
double vowelScale (const Vowel& v, double sr, double f0)
{
    const auto raw = vowelSignal (v, sr, (int) (sr / f0 * 3), 1.0, [f0] (int) { return f0; }, 1.0);
    float peak = 0.0f;
    for (float s : raw)
        peak = std::max (peak, std::abs (s));
    return 0.3 / (double) peak;
}

Vec steadyVowel (const Vowel& v, double sr, int n, double f0, double ratio = 1.0)
{
    return vowelSignal (v, sr, n, ratio, [f0] (int) { return f0; }, vowelScale (v, sr, f0));
}

Vec whiteNoise (int n, float amp, unsigned seed = 5)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> d (-1.0f, 1.0f);
    Vec v ((size_t) n);
    for (auto& s : v)
        s = amp * d (rng);
    return v;
}

// Gaussian-like noise confined to lo..hi Hz (white noise through a 511-tap windowed-sinc band-pass): a stand-in for an "s".
Vec bandNoise (double sr, int n, double lo, double hi, float amp, unsigned seed = 9)
{
    const int taps = 511, mid = taps / 2;
    std::vector<double> h ((size_t) taps);
    for (int i = 0; i < taps; ++i)
    {
        const double t = i - mid;
        const double hp = i == mid ? 2.0 * hi / sr : std::sin (2.0 * pi * hi * t / sr) / (pi * t);
        const double lp = i == mid ? 2.0 * lo / sr : std::sin (2.0 * pi * lo * t / sr) / (pi * t);
        const double w = 0.42 - 0.5 * std::cos (2.0 * pi * i / (taps - 1)) + 0.08 * std::cos (4.0 * pi * i / (taps - 1));
        h[(size_t) i] = (hp - lp) * w;
    }
    const auto white = whiteNoise (n + taps, 1.0f, seed);
    Vec v ((size_t) n);
    double sumSq = 0;
    for (int i = 0; i < n; ++i)
    {
        double acc = 0;
        for (int k = 0; k < taps; ++k)
            acc += h[(size_t) k] * white[(size_t) (i + taps - 1 - k)];
        v[(size_t) i] = (float) acc;
        sumSq += acc * acc;
    }
    const double scale = (double) amp / std::sqrt (sumSq / n); // amp = RMS
    for (auto& x : v)
        x = (float) (x * scale);
    return v;
}

// --- measurements -------------------------------------------------------------------------------------------------
double cents (double hz, double ref) { return 1200.0 * std::log2 (hz / ref); }

double rms (const float* x, int n)
{
    double s = 0;
    for (int i = 0; i < n; ++i)
        s += (double) x[i] * x[i];
    return std::sqrt (s / std::max (n, 1));
}

double peakOf (const Vec& v, size_t from = 0, size_t to = (size_t) -1)
{
    double p = 0;
    for (size_t i = from; i < std::min (to, v.size()); ++i)
        p = std::max (p, (double) std::abs (v[i]));
    return p;
}

// Frequency of a steady periodic signal: normalised autocorrelation + parabolic peak (independent of the module).
double autocorrFreq (const float* x, int n, double sr, double nearHz)
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

void fftInPlace (std::vector<std::complex<double>>& a)
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

// Level in dB of the first `count` harmonics of f0 (largest FFT bin within +-2 bins of each), 16384-point Hann.
std::vector<double> harmonicDb (const Vec& x, size_t from, double f0, double sr, int count)
{
    const size_t N = 16384;
    std::vector<std::complex<double>> a (N);
    for (size_t i = 0; i < N; ++i)
        a[i] = (double) x[from + i] * (0.5 - 0.5 * std::cos (2.0 * pi * (double) i / (double) N));
    fftInPlace (a);
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

// Mean absolute difference (dB) between two harmonic-level lists after removing their mean offset, over the harmonics that
// are within 40 dB of the reference's loudest. `offset` receives the removed mean (the overall level difference).
double shapeError (const std::vector<double>& a, const std::vector<double>& ref, double& offset)
{
    double top = -1.0e9;
    for (double d : ref)
        top = std::max (top, d);
    double mean = 0;
    int m = 0;
    for (size_t i = 0; i < a.size(); ++i)
        if (ref[i] > top - 40.0)
        {
            mean += a[i] - ref[i];
            ++m;
        }
    mean /= std::max (m, 1);
    offset = mean;
    double e = 0;
    for (size_t i = 0; i < a.size(); ++i)
        if (ref[i] > top - 40.0)
            e += std::abs (a[i] - ref[i] - mean);
    return e / std::max (m, 1);
}

// Power-weighted mean frequency of the harmonics: moves up when the formants move up.
double centroid (const std::vector<double>& db, double f0)
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

// Share of the signal's energy (dB) that is NOT on the harmonics of f0 (a clean periodic voice has almost none). A formant
// shifter that modulates its gain from frame to frame creates sidebands there, so this exposes that kind of artifact.
double inharmonicResidueDb (const Vec& y, size_t from, double f0, double sr)
{
    const size_t N = 32768;
    std::vector<std::complex<double>> a (N);
    for (size_t i = 0; i < N; ++i)
        a[i] = (double) y[from + i] * (0.5 - 0.5 * std::cos (2.0 * pi * (double) i / (double) N));
    fftInPlace (a);
    std::vector<char> onHarmonic (N / 2, 0);
    for (int h = 1; h * f0 < 0.45 * sr; ++h)
    {
        const int b = (int) std::lround (h * f0 * (double) N / sr);
        for (int k = b - 6; k <= b + 6; ++k)
            if (k > 0 && k < (int) N / 2)
                onHarmonic[(size_t) k] = 1;
    }
    double total = 0, residue = 0;
    for (size_t k = 1; k < N / 2; ++k)
    {
        const double p = std::norm (a[k]);
        total += p;
        if (! onHarmonic[k])
            residue += p;
    }
    return 10.0 * std::log10 (residue / total + 1.0e-30);
}

// Strongest 3-bin spectral line, relative to the local average level, inside the flat stretches of a noise spectrum
// (Gaussian noise: a few times the average at most; steep band edges and empty regions are skipped because they are not flat).
double flatSpurRatio (const Vec& y, size_t from)
{
    std::vector<std::complex<double>> a (16384);
    for (size_t i = 0; i < a.size(); ++i)
        a[i] = (double) y[from + i] * (0.5 - 0.5 * std::cos (2.0 * pi * (double) i / 16384.0));
    fftInPlace (a);
    std::vector<double> prefix (8193, 0.0);
    for (size_t k = 0; k < 8192; ++k)
        prefix[k + 1] = prefix[k] + std::norm (a[k]);
    auto mean = [&] (size_t lo, size_t hi) { return (prefix[hi] - prefix[lo]) / (double) (hi - lo); };
    double worst = 0;
    for (size_t k = 1200; k < 6000; k += 3)
    {
        const double here = mean (k - 60, k + 60), left = mean (k - 180, k - 60), right = mean (k + 60, k + 180);
        if (here < 1.0e-6 || left < 0.5 * here || left > 2.0 * here || right < 0.5 * here || right > 2.0 * here)
            continue;
        worst = std::max (worst, mean (k, k + 3) / here);
    }
    return worst;
}

// --- running the shifter ------------------------------------------------------------------------------------------
struct Stereo { Vec l, r; };

// Runs mono `x` through a fresh shifter in blocks of `block`.
Vec runMono (const Vec& x, double sr, float shift, int block)
{
    FormantShifter fs;
    fs.prepare (sr);
    fs.setShiftSemitones (shift);
    fs.reset();
    Vec out = x;
    for (size_t s = 0; s < out.size(); s += (size_t) block)
        fs.processBlock (out.data() + s, out.data() + s, (int) std::min ((size_t) block, out.size() - s));
    return out;
}

Stereo runStereo (const Vec& l, const Vec& r, double sr, float shift, int block)
{
    FormantShifter fs;
    fs.prepare (sr);
    fs.setShiftSemitones (shift);
    fs.reset();
    Stereo o { l, r };
    for (size_t s = 0; s < l.size(); s += (size_t) block)
    {
        float* ch[2] = { o.l.data() + s, o.r.data() + s };
        fs.process (ch, 2, (int) std::min ((size_t) block, l.size() - s));
    }
    return o;
}

bool allFinite (const Vec& v)
{
    for (float s : v)
        if (! std::isfinite (s))
            return false;
    return true;
}
} // namespace

struct FormantShiftTests : public juce::UnitTest
{
    FormantShiftTests() : juce::UnitTest ("FormantShifter (experimental)", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("Latency is frameSize - 1 and constant (21-23 ms), reported honestly");
        {
            expectEquals (FormantShifter::latencyFor (44100.0), 1023);
            expectEquals (FormantShifter::latencyFor (48000.0), 1023);
            expectEquals (FormantShifter::latencyFor (96000.0), 2047);
            expectEquals (FormantShifter::latencyFor (22050.0), 511);
            for (double sr : { 22050.0, 44100.0, 48000.0, 96000.0, 192000.0 })
            {
                FormantShifter fs;
                fs.prepare (sr);
                expectEquals (fs.getLatencySamples(), FormantShifter::latencyFor (sr));
                const double ms = fs.getLatencySamples() * 1000.0 / sr;
                expect (ms > 15.0 && ms < 25.0, juce::String (sr) + " Hz: " + juce::String (ms, 2) + " ms");
            }
            // Measured, not just claimed: noise through the shifter with a shift comes out delayed by the reported latency.
            const double sr = 48000.0;
            const auto x = whiteNoise (24000, 0.3f);
            for (float shift : { 0.0f, 6.0f, -6.0f })
            {
                const auto y = runMono (x, sr, shift, 480);
                const int lat = FormantShifter::latencyFor (sr);
                int bestLag = 0;
                double best = -1.0e30;
                for (int lag = lat - 6; lag <= lat + 6; ++lag)
                {
                    double c = 0;
                    for (int i = 6000; i < 20000; ++i)
                        c += (double) y[(size_t) i] * x[(size_t) (i - lag)];
                    if (c > best) { best = c; bestLag = lag; }
                }
                expect (std::abs (bestLag - lat) <= 1, "measured delay " + juce::String (bestLag) + " for shift " + juce::String (shift));
            }
            logMessage ("  latency: 1023 samples = 23.2 ms at 44.1 kHz and 21.3 ms at 48 kHz; 2047 samples = 23.2 ms at 88.2 kHz and 21.3 ms at 96 kHz (a constant)");
        }

        beginTest ("Neutral setting: the output is the input delayed by the latency (mono and stereo, all rates)");
        for (double sr : { 22050.0, 32000.0, 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        {
            const int n = (int) (0.4 * sr);
            auto x = steadyVowel (vowels[0], sr, n, 140.0);
            const auto noise = whiteNoise (n, 0.05f);
            for (int i = n / 2; i < n; ++i)
                x[(size_t) i] += noise[(size_t) i];
            const int lat = FormantShifter::latencyFor (sr);
            const auto y = runMono (x, sr, 0.0f, 300);
            const int settled = lat + FormantShifter::frameSizeFor (sr); // the first frame's worth of output is the overlap-add filling up
            double worst = 0;
            for (int i = settled; i < n; ++i)
                worst = std::max (worst, (double) std::abs (y[(size_t) i] - x[(size_t) (i - lat)]));
            expectLessThan (worst, 2.0e-6);
            const auto st = runStereo (x, noise, sr, 0.0f, 300);
            double worstS = 0;
            for (int i = settled; i < n; ++i)
                worstS = std::max ({ worstS, (double) std::abs (st.l[(size_t) i] - x[(size_t) (i - lat)]),
                                     (double) std::abs (st.r[(size_t) i] - noise[(size_t) (i - lat)]) });
            expectLessThan (worstS, 2.0e-6);
            if (juce::approximatelyEqual (sr, 48000.0))
                logMessage ("  neutral at 48 kHz: worst difference from the delayed input " + juce::String (worst, 9) + " (peak 0.3), about -120 dBFS");
        }

        beginTest ("Fundamental pitch is preserved: 100-400 Hz voices, shifts of -12 to +12 semitones, two rates");
        {
            double worstCents = 0, worstExtra = -1.0e9;
            int cases = 0;
            for (double sr : { 44100.0, 48000.0 })
                for (double f0 : { 100.0, 150.0, 220.0, 300.0, 400.0 })
                    for (float shift : { -12.0f, -6.0f, -3.0f, 3.0f, 6.0f, 12.0f })
                    {
                        const auto x = steadyVowel (vowels[(int) f0 % 3], sr, (int) (1.0 * sr), f0);
                        const auto y = runMono (x, sr, shift, 512);
                        const int from = (int) (0.6 * sr), len = (int) (0.3 * sr);
                        const double err = cents (autocorrFreq (y.data() + from, len, sr, f0), f0);
                        // what the same measurement says about the untouched voice: the shifter must not add more than 1 cent to that
                        const double base = cents (autocorrFreq (x.data() + from, len, sr, f0), f0);
                        worstCents = std::max (worstCents, std::abs (err));
                        worstExtra = std::max (worstExtra, std::abs (err) - std::abs (base));
                        expectLessThan (std::abs (err) - std::abs (base), 1.0);
                        expectLessThan (std::abs (err), 4.0);
                        ++cases;
                    }
            logMessage ("  " + juce::String (cases) + " cases: worst fundamental error " + juce::String (worstCents, 2) + " cents (measurement limit on the untouched voice: error added by the shifter at most " + juce::String (worstExtra, 2) + " cents)");
        }

        beginTest ("Formants really move: vowels with known formants vs the same vowels synthesised with moved formants");
        {
            double sumNone = 0, sumShifted = 0, worstStrict = 0;
            int cases = 0, centroidCases = 0, centroidAgree = 0;
            for (double f0 : { 110.0, 150.0, 220.0, 300.0 })
                for (const auto& v : vowels)
                    for (float shift : { -6.0f, -3.0f, 3.0f, 6.0f })
                    {
                        const double sr = 48000.0, ratio = std::pow (2.0, shift / 12.0);
                        const int n = (int) sr;
                        const double scale = vowelScale (v, sr, f0);
                        const auto in = steadyVowel (v, sr, n, f0);
                        const auto ref = vowelSignal (v, sr, n, ratio, [f0] (int) { return f0; }, scale);
                        const auto out = runMono (in, sr, shift, 480);
                        const int lat = FormantShifter::latencyFor (sr), nh = (int) std::min (40.0, 0.4 * sr / f0 - 1.0);
                        const auto hOut = harmonicDb (out, (size_t) (24000 + lat), f0, sr, nh);
                        const auto hRef = harmonicDb (ref, 24000, f0, sr, nh);
                        const auto hIn = harmonicDb (in, 24000, f0, sr, nh);
                        double offOut = 0, offIn = 0;
                        const double eOut = shapeError (hOut, hRef, offOut), eIn = shapeError (hIn, hRef, offIn);
                        sumNone += eIn;
                        sumShifted += eOut;
                        ++cases;
                        const juce::String what = juce::String (v.name) + " f0 " + juce::String (f0) + " shift " + juce::String (shift);
                        // direction: the spectral centroid of the harmonics should move the way the ideal vowel's does
                        const double c0 = centroid (hIn, f0), c1 = centroid (hOut, f0), cRef = centroid (hRef, f0);
                        if (std::abs (cRef - c0) > 0.05 * c0)
                        {
                            ++centroidCases;
                            if ((cRef > c0) == (c1 > c0))
                                ++centroidAgree;
                            else if (f0 <= 150.0)
                                expect (false, "centroid moves like the ideal vowel's: " + what);
                        }
                        if (f0 <= 150.0)      // dense harmonics: the envelope is well sampled, so be strict
                        {
                            expectLessThan (eOut, 3.5);
                            expectLessThan (eOut, eIn + 0.5); // never clearly worse than doing nothing
                            worstStrict = std::max (worstStrict, eOut);
                        }
                        else if (f0 < 250.0)  // 220 Hz: sparser harmonics, still a clear gain over doing nothing in every case
                        {
                            expectLessThan (eOut, 6.0);
                            expectLessThan (eOut, eIn);
                        }
                        else                  // 300 Hz: harmonics 300 Hz apart cannot show where a 60-100 Hz-wide formant sits, so any envelope
                            expectLessThan (eOut, 14.0); // method is weak here and can be worse than doing nothing; bounded, and documented
                    }
            expectLessThan (sumShifted, 0.55 * sumNone); // clearly closer to the target than doing nothing
            expect (centroidAgree >= centroidCases * 85 / 100, "centroid direction agrees with the ideal vowel in " + juce::String (centroidAgree) + " of " + juce::String (centroidCases));
            logMessage ("  " + juce::String (cases) + " vowel cases: mean log-spectrum error vs the ideal moved-formant vowel "
                        + juce::String (sumNone / cases, 2) + " dB if nothing is done, " + juce::String (sumShifted / cases, 2)
                        + " dB with the shifter (worst at f0 <= 150 Hz: " + juce::String (worstStrict, 2) + " dB); centroid direction agrees in "
                        + juce::String (centroidAgree) + " of " + juce::String (centroidCases));
        }

        beginTest ("No modulation artifacts: almost no energy between the harmonics, for 5 pitches x 3 vowels x 6 shifts");
        {
            const double sr = 44100.0;
            double sum = 0, worst = -200.0;
            int cases = 0;
            for (double f0 : { 100.0, 150.0, 220.0, 300.0, 400.0 })
                for (const auto& v : vowels)
                    for (float shift : { -12.0f, -6.0f, -3.0f, 3.0f, 6.0f, 12.0f })
                    {
                        const auto x = steadyVowel (v, sr, (int) (1.4 * sr), f0);
                        const auto y = runMono (x, sr, shift, 512);
                        const double r = inharmonicResidueDb (y, (size_t) (0.5 * sr) + (size_t) FormantShifter::latencyFor (sr), f0, sr);
                        sum += r;
                        worst = std::max (worst, r);
                        ++cases;
                        expectLessThan (r, -25.0);
                    }
            expectLessThan (sum / cases, -40.0);
            logMessage ("  inharmonic energy (re total): mean " + juce::String (sum / cases, 1) + " dB, worst " + juce::String (worst, 1) + " dB over "
                        + juce::String (cases) + " cases (a clean input is about -58 dB)");
        }

        beginTest ("Loudness is held: output level stays within 2.5 dB of the input for every shift");
        {
            const double sr = 48000.0;
            double worst = 0;
            for (const auto& v : vowels)
                for (float shift : { -12.0f, -6.0f, -3.0f, 3.0f, 6.0f, 12.0f })
                {
                    const auto x = steadyVowel (v, sr, (int) sr, 130.0);
                    const auto y = runMono (x, sr, shift, 480);
                    const int lat = FormantShifter::latencyFor (sr);
                    const double dB = 20.0 * std::log10 (rms (y.data() + 24000 + lat, 12000) / rms (x.data() + 24000, 12000));
                    worst = std::max (worst, std::abs (dB));
                    expectLessThan (std::abs (dB), 2.5);
                }
            logMessage ("  worst level change " + juce::String (worst, 2) + " dB");
        }

        beginTest ("Silence and very low-level input: silent stays silent, tiny signals stay tiny and finite");
        {
            for (double sr : { 44100.0, 48000.0 })
            {
                const auto silence = runMono (Vec ((size_t) sr, 0.0f), sr, 7.0f, 480);
                expectEquals (peakOf (silence), 0.0);
                const auto st = runStereo (Vec ((size_t) sr, 0.0f), Vec ((size_t) sr, 0.0f), sr, -7.0f, 333);
                expectEquals (std::max (peakOf (st.l), peakOf (st.r)), 0.0);
                for (float amp : { 1.0e-9f, 1.0e-6f, 1.0e-4f })
                {
                    const auto x = whiteNoise ((int) sr, amp);
                    const auto y = runMono (x, sr, 6.0f, 480);
                    expect (allFinite (y));
                    expectLessThan (peakOf (y), 8.0 * (double) amp + 1.0e-12); // never boosts a quiet signal wildly
                }
                const Vec dc ((size_t) sr, 0.1f);
                const auto y = runMono (dc, sr, 5.0f, 480);
                expect (allFinite (y));
                expectLessThan (peakOf (y), 0.5);
            }
        }

        beginTest ("Voiced / unvoiced transitions: vowel, fricative noise burst, vowel stay finite and bounded");
        for (float shift : { -8.0f, 8.0f })
        {
            const double sr = 48000.0;
            auto x = steadyVowel (vowels[0], sr, (int) (1.2 * sr), 170.0);
            const auto burst = bandNoise (sr, (int) (0.25 * sr), 4000.0, 9000.0, 0.25f);
            const int b0 = (int) (0.4 * sr);
            for (size_t i = 0; i < burst.size(); ++i)
                x[(size_t) b0 + i] = burst[i]; // replaces the vowel with noise for 250 ms, abruptly
            const auto y = runMono (x, sr, shift, 480);
            expect (allFinite (y));
            expectLessThan (peakOf (y), 2.0 * peakOf (x));
            const int lat = FormantShifter::latencyFor (sr);
            const double f = autocorrFreq (y.data() + (int) (0.9 * sr) + lat, (int) (0.2 * sr), sr, 170.0);
            expectLessThan (std::abs (cents (f, 170.0)), 3.0); // the vowel after the burst is back at its own pitch
        }

        beginTest ("Sibilant-like noise (5-9 kHz): finite, noise-like (no spurs), level held for moderate shifts, neutral exact");
        {
            const double sr = 48000.0;
            const auto x = bandNoise (sr, (int) (1.0 * sr), 5000.0, 9000.0, 0.25f);
            const int lat = FormantShifter::latencyFor (sr);
            // A formant shifter can only re-weight energy that exists: a band-limited "s" moved a whole octave has little left to
            // carry, so its level may drop at +-12; for moderate shifts it must stay within 6 dB.
            for (float shift : { -12.0f, -6.0f, -3.0f, 3.0f, 6.0f, 12.0f })
            {
                const auto y = runMono (x, sr, shift, 480);
                expect (allFinite (y));
                const double dB = 20.0 * std::log10 (rms (y.data() + 24000 + lat, 16000) / rms (x.data() + 24000, 16000));
                if (std::abs (shift) <= 6.0f)
                    expectLessThan (std::abs (dB), 6.0);
                else
                    expect (dB < 3.0 && dB > -30.0, "level at " + juce::String (shift) + " semitones: " + juce::String (dB, 1) + " dB");
                // spurs (see flatSpurRatio): the output must not have lines that are clearly stronger than the input's own
                const double spurOut = flatSpurRatio (y, (size_t) (24000 + lat)), spurIn = flatSpurRatio (x, 24000);
                expectLessThan (spurOut, std::max (6.0, 2.2 * spurIn));
            }
            expect (runMono (x, sr, 0.0f, 480).size() == x.size());
        }

        beginTest ("Abrupt pitch change (150 -> 300 Hz) with a shift: finite, bounded, each pitch preserved");
        {
            const double sr = 48000.0;
            const int jump = (int) (0.7 * sr), n = (int) (1.4 * sr);
            const double scale = vowelScale (vowels[0], sr, 150.0);
            const auto x = vowelSignal (vowels[0], sr, n, 1.0, [=] (int i) { return i < jump ? 150.0 : 300.0; }, scale);
            for (float shift : { -6.0f, 5.0f })
            {
                const auto y = runMono (x, sr, shift, 480);
                expect (allFinite (y));
                expectLessThan (peakOf (y), 2.0 * peakOf (x));
                const int lat = FormantShifter::latencyFor (sr);
                expectLessThan (std::abs (cents (autocorrFreq (y.data() + (int) (0.35 * sr) + lat, (int) (0.25 * sr), sr, 150.0), 150.0)), 3.0);
                expectLessThan (std::abs (cents (autocorrFreq (y.data() + jump + (int) (0.15 * sr) + lat, (int) (0.25 * sr), sr, 300.0), 300.0)), 3.0);
            }
        }

        beginTest ("Mono and stereo: identical channels equal mono; one shared gain keeps the stereo balance exactly");
        {
            const double sr = 44100.0;
            const auto l = steadyVowel (vowels[1], sr, (int) (0.8 * sr), 200.0);
            Vec r = l;
            for (auto& s : r)
                s *= 0.5f;
            const auto mono = runMono (l, sr, 4.0f, 256);
            const auto same = runStereo (l, l, sr, 4.0f, 256);
            expect (mono == same.l && mono == same.r, "mono == stereo with identical channels, bit for bit");
            const auto half = runStereo (l, r, sr, 4.0f, 700);
            double worst = 0;
            for (size_t i = 0; i < l.size(); ++i)
                worst = std::max (worst, (double) std::abs (half.r[i] - 0.5f * half.l[i]));
            expectLessThan (worst, 2.0e-6);
            // different content per channel: both keep their own pitch
            const auto l2 = steadyVowel (vowels[0], sr, (int) (0.8 * sr), 130.0), r2 = steadyVowel (vowels[2], sr, (int) (0.8 * sr), 210.0);
            const auto two = runStereo (l2, r2, sr, -5.0f, 480);
            const int lat = FormantShifter::latencyFor (sr);
            expectLessThan (std::abs (cents (autocorrFreq (two.l.data() + 20000 + lat, 8000, sr, 130.0), 130.0)), 3.0);
            expectLessThan (std::abs (cents (autocorrFreq (two.r.data() + 20000 + lat, 8000, sr, 210.0), 210.0)), 3.0);
        }

        beginTest ("Sample rates and block sizes: output never depends on the block size; pitch kept at every rate");
        for (double sr : { 22050.0, 32000.0, 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        {
            const int n = (int) (0.45 * sr);
            const auto x = steadyVowel (vowels[0], sr, n, 160.0);
            const auto ref = runMono (x, sr, 4.0f, 1);
            for (int block : { 7, 64, 255, 256, 257, 480, 1000, 4096, n })
                expect (runMono (x, sr, 4.0f, block) == ref, juce::String (sr) + " Hz, block " + juce::String (block));
            expect (allFinite (ref));
            const int lat = FormantShifter::latencyFor (sr);
            expectLessThan (std::abs (cents (autocorrFreq (ref.data() + (int) (0.2 * sr) + lat, (int) (0.1 * sr), sr, 160.0), 160.0)), 3.0);
        }

        beginTest ("Changing the shift while audio runs: identical for any block split, no click, and safe from another thread");
        {
            const double sr = 48000.0;
            const int n = (int) (1.5 * sr);
            const auto x = steadyVowel (vowels[0], sr, n, 150.0);
            const int changes[] = { 20000, 33333, 50000 };
            const float values[] = { 6.0f, -4.0f, 0.0f };
            auto run = [&] (int block)
            {
                FormantShifter fs;
                fs.prepare (sr);
                fs.reset();
                Vec out = x;
                int pos = 0, next = 0;
                while (pos < n)
                {
                    int len = std::min (block, n - pos);
                    if (next < 3 && pos + len > changes[next])
                        len = changes[next] - pos; // split so the change lands on the same sample for every block size
                    if (len > 0)
                        fs.processBlock (out.data() + pos, out.data() + pos, len);
                    pos += len;
                    if (next < 3 && pos == changes[next])
                        fs.setShiftSemitones (values[next++]);
                }
                return out;
            };
            const auto a = run (1), b = run (480), c = run (4096);
            expect (a == b && a == c, "same result for every block size when the changes land on the same samples");
            float ownStep = 0, step = 0;
            for (int i = 2; i < n; ++i)
                ownStep = std::max (ownStep, std::abs (x[(size_t) i] - x[(size_t) i - 1]));
            for (int i = 20000; i < 60000; ++i)
                step = std::max (step, std::abs (a[(size_t) i] - a[(size_t) i - 1]));
            expectLessThan (step, 2.0f * ownStep);
            expect (allFinite (a));

            // setShiftSemitones from another thread while process() runs: no crash, finite, always inside the clamp.
            FormantShifter fs;
            fs.prepare (sr);
            fs.reset();
            std::atomic<bool> stop { false };
            std::thread t ([&]
            {
                float v = -20.0f;
                while (! stop.load())
                {
                    fs.setShiftSemitones (v);
                    v = v > 20.0f ? -20.0f : v + 0.7f;
                }
            });
            Vec out = x;
            for (size_t s = 0; s + 480 <= out.size(); s += 480)
                fs.processBlock (out.data() + s, out.data() + s, 480);
            stop = true;
            t.join();
            expect (allFinite (out));
            expectLessThan (peakOf (out), 4.0 * peakOf (x));
            fs.setShiftSemitones (qNaN);
            expectEquals (fs.getShiftSemitones(), 0.0f);
            fs.setShiftSemitones (99.0f);
            expectEquals (fs.getShiftSemitones(), FormantShifter::maxShiftSemitones);
            fs.setShiftSemitones (-99.0f);
            expectEquals (fs.getShiftSemitones(), FormantShifter::minShiftSemitones);
        }

        beginTest ("NaN, +Inf, -Inf and huge samples behave exactly like zeros, and the shifter recovers at once");
        for (double sr : { 44100.0, 48000.0 })
        {
            auto clean = steadyVowel (vowels[0], sr, (int) (1.0 * sr), 175.0);
            auto dirty = clean;
            for (int i = 10000; i < 10090; ++i)
            {
                dirty[(size_t) i] = (i % 3 == 0) ? qNaN : (i % 3 == 1 ? posInf : negInf);
                clean[(size_t) i] = 0.0f;
            }
            dirty[20000] = 1.0e30f; clean[20000] = 0.0f;
            dirty[20001] = -3.0e38f; clean[20001] = 0.0f;
            const auto a = runMono (clean, sr, 6.0f, 480), b = runMono (dirty, sr, 6.0f, 480);
            expect (allFinite (b));
            expect (a == b, "bit-identical to zero substitution");
            const auto sa = runStereo (clean, clean, sr, -6.0f, 100), sb = runStereo (dirty, clean, sr, -6.0f, 100);
            expect (allFinite (sb.l) && allFinite (sb.r));
            (void) sa;
            const int lat = FormantShifter::latencyFor (sr);
            expectLessThan (std::abs (cents (autocorrFreq (b.data() + (int) (0.7 * sr) + lat, (int) (0.2 * sr), sr, 175.0), 175.0)), 3.0);
        }

        beginTest ("Buffer boundaries, reset and repeated processing");
        {
            const double sr = 48000.0;
            const auto x = steadyVowel (vowels[2], sr, (int) (0.6 * sr), 190.0);
            FormantShifter fs;
            fs.prepare (sr);
            fs.setShiftSemitones (-5.0f);
            fs.reset();
            auto pass = [&]
            {
                Vec out = x;
                size_t s = 0;
                const int sizes[] = { 1, 255, 256, 257, 1023, 3, 512 };
                for (int k = 0; s < out.size(); ++k)
                {
                    const size_t len = std::min ((size_t) sizes[k % 7], out.size() - s);
                    fs.processBlock (out.data() + s, out.data() + s, (int) len);
                    s += len;
                }
                return out;
            };
            const auto first = pass();
            fs.reset();
            const auto second = pass();
            expect (first == second, "reset() restores the initial state exactly");
            expect (first == runMono (x, sr, -5.0f, 4096), "odd block sizes equal one big block");
            // The same block fed again and again (no reset): stays finite and bounded.
            Vec loop = x;
            float peak = 0;
            for (int rep = 0; rep < 12; ++rep)
            {
                fs.processBlock (loop.data(), loop.data(), (int) loop.size());
                expect (allFinite (loop));
                peak = std::max (peak, (float) peakOf (loop));
                loop = x;
            }
            expectLessThan (peak, 2.0f * (float) peakOf (x));
        }

        beginTest ("Real-time safety: process() allocates nothing (mono, stereo, shifting, neutral, NaN input, changing shift)");
        {
            const double sr = 48000.0;
            FormantShifter fs;
            fs.prepare (sr);
            fs.reset();
            Vec a = steadyVowel (vowels[0], sr, 24000, 150.0), b = a;
            a[1000] = qNaN;
            const long before = g_allocations.load();
            for (int mode = 0; mode < 3; ++mode)
            {
                fs.setShiftSemitones (mode == 0 ? 0.0f : (mode == 1 ? 7.0f : -9.0f));
                float* ch[2] = { a.data(), b.data() };
                for (size_t s = 0; s + 333 <= a.size(); s += 333)
                {
                    float* c[2] = { ch[0] + s, ch[1] + s };
                    fs.process (c, 2, 333);
                    fs.setShiftSemitones ((float) (s % 11) - 5.0f);
                }
                fs.processBlock (a.data(), a.data(), 480);
            }
            expectEquals (g_allocations.load() - before, 0L);
        }
    }
};

static FormantShiftTests formantShiftTests;
