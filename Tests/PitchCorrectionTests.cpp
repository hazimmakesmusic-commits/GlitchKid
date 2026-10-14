#include <juce_core/juce_core.h>
#include <cmath>
#include <algorithm>
#include <limits>
#include "../Source/Experimental/PitchCorrection/PitchCorrectorPrototype.h"

// Stage 4.1: tests for the experimental pitch-correction prototype. Synthetic signals only.
using namespace glitchkid::pitch;

namespace
{
constexpr float qNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float posInf = std::numeric_limits<float>::infinity();
constexpr double pi = 3.14159265358979323846;

using Vec = std::vector<float>;

// Vowel-like test voice: harmonics with 1/h amplitude, optional vibrato (depth in cents, rate in Hz).
// `freqAt` lets a test glide or jump the pitch. Phase is accumulated in double.
template <typename F>
Vec voice (double sr, int n, F freqAt, float peak = 0.4f)
{
    Vec v ((size_t) n);
    double phase = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double f0 = freqAt (i);
        phase += f0 / sr;
        phase -= std::floor (phase);
        double s = 0.0, norm = 0.0;
        for (int h = 1; h * f0 < 0.45 * sr && h <= 12; ++h)
        {
            s += std::sin (2.0 * pi * h * phase) / h;
            norm += 1.0 / h;
        }
        v[(size_t) i] = (float) (peak * s / norm * 1.6);
    }
    return v;
}

Vec voice (double sr, int n, double f0, float peak = 0.4f)
{
    return voice (sr, n, [f0] (int) { return f0; }, peak);
}

double cents (double hz, double ref) { return 1200.0 * std::log2 (hz / ref); }

// Independent frequency estimate of a steady tone: normalised autocorrelation + parabolic peak.
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

double rms (const float* x, int n)
{
    double s = 0;
    for (int i = 0; i < n; ++i)
        s += (double) x[i] * x[i];
    return std::sqrt (s / std::max (n, 1));
}

struct DetectStats { int frames = 0, voiced = 0, octaveErrors = 0; double medianAbsCents = 0, maxAbsCents = 0; };

DetectStats detect (const Vec& x, double sr, double trueHz, int skipSamples, float minHz = 70.0f, float maxHz = 900.0f)
{
    YinPitchDetector d;
    d.prepare (sr, minHz, maxHz);
    DetectStats st;
    std::vector<double> errs;
    for (int i = 0; i < (int) x.size(); ++i)
        if (d.push (x[(size_t) i]) && i >= skipSamples)
        {
            ++st.frames;
            const auto& r = d.result();
            if (! r.voiced)
                continue;
            ++st.voiced;
            const double c = cents (r.frequencyHz, trueHz);
            if (std::abs (c) > 450.0) // anything that far off is a wrong octave/harmonic, not a tuning error
                ++st.octaveErrors;
            else
                errs.push_back (std::abs (c));
        }
    if (! errs.empty())
    {
        std::sort (errs.begin(), errs.end());
        st.medianAbsCents = errs[errs.size() / 2];
        st.maxAbsCents = errs.back();
    }
    return st;
}

PitchCorrectorPrototype::Settings settings (int key, ScaleQuantizer::Scale sc, float strength, float speedMs)
{
    PitchCorrectorPrototype::Settings s;
    s.key = key; s.scale = sc; s.strength = strength; s.retuneSpeedMs = speedMs;
    return s;
}

Vec runCorrector (PitchCorrectorPrototype& c, const Vec& in, int block)
{
    Vec out (in.size());
    for (size_t start = 0; start < in.size(); start += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, in.size() - start);
        c.processBlock (in.data() + start, out.data() + start, n);
    }
    return out;
}
}

class PitchCorrectionTests : public juce::UnitTest
{
public:
    PitchCorrectionTests() : juce::UnitTest ("PitchCorrection (experimental)", "GlitchKid") {}

    void runTest() override
    {
        using Sc = ScaleQuantizer::Scale;

        beginTest ("Detector: accuracy and octave errors, E2..G5, three sample rates");
        {
            const double freqs[] = { 82.41, 110.0, 146.83, 196.0, 261.63, 329.63, 440.0, 587.33, 784.0 };
            int totalOct = 0;
            double worstMedian = 0;
            for (double sr : { 44100.0, 48000.0, 96000.0 })
                for (double f : freqs)
                {
                    const auto st = detect (voice (sr, (int) (0.7 * sr), f), sr, f, (int) (0.15 * sr));
                    expect (st.frames > 0 && st.voiced >= st.frames * 95 / 100, "voiced " + juce::String (f) + " Hz @ " + juce::String (sr));
                    expectLessThan (st.medianAbsCents, 3.0);
                    expectLessThan (st.maxAbsCents, 15.0);
                    expectEquals (st.octaveErrors, 0);
                    totalOct += st.octaveErrors;
                    worstMedian = std::max (worstMedian, st.medianAbsCents);
                }
            logMessage ("  27 cases: octave errors " + juce::String (totalOct) + ", worst median error " + juce::String (worstMedian, 2) + " cents");
        }

        beginTest ("Detector: vibrato and glide are followed");
        {
            const double sr = 48000.0;
            const auto v = voice (sr, (int) sr, [sr] (int i) { return 220.0 * std::pow (2.0, 40.0 * std::sin (2 * pi * 5.5 * i / sr) / 1200.0); });
            YinPitchDetector d;
            d.prepare (sr);
            int frames = 0, bad = 0;
            for (int i = 0; i < (int) v.size(); ++i)
                if (d.push (v[(size_t) i]) && i > 9600)
                {
                    ++frames;
                    const double trueHz = 220.0 * std::pow (2.0, 40.0 * std::sin (2 * pi * 5.5 * (i - 700) / sr) / 1200.0); // estimate sits ~15 ms back
                    if (! d.result().voiced || std::abs (cents (d.result().frequencyHz, trueHz)) > 35.0)
                        ++bad;
                }
            expect (bad * 20 < frames, "vibrato tracked within 35 cents on 95% of frames");
            logMessage ("  +-40 cent vibrato at 5.5 Hz: " + juce::String (bad) + " of " + juce::String (frames) + " frames off by > 35 cents");
        }

        beginTest ("Detector: silence, noise, tiny signals are not voiced");
        {
            for (double sr : { 44100.0, 48000.0 })
            {
                const auto st = detect (Vec ((size_t) sr, 0.0f), sr, 220.0, 0);
                expectEquals (st.voiced, 0);
                juce::Random rng (7);
                Vec noise ((size_t) (2 * sr));
                for (auto& s : noise)
                    s = (rng.nextFloat() - 0.5f) * 0.5f;
                const auto sn = detect (noise, sr, 220.0, 0);
                expect (sn.voiced * 20 < sn.frames, "white noise voiced on < 5% of frames: " + juce::String (sn.voiced) + "/" + juce::String (sn.frames));
                logMessage ("  white noise @ " + juce::String (sr) + ": voiced " + juce::String (sn.voiced) + " of " + juce::String (sn.frames) + " frames");
                const auto tiny = detect (voice (sr, (int) sr, 220.0, 1.0e-5f), sr, 220.0, 0);
                expectEquals (tiny.voiced, 0); // -100 dBFS: below the gate
            }
        }

        beginTest ("Detector: abrupt pitch jump is picked up within 45 ms");
        {
            const double sr = 48000.0;
            const int jumpAt = (int) (0.4 * sr);
            const auto v = voice (sr, (int) (0.8 * sr), [=] (int i) { return i < jumpAt ? 196.0 : 311.0; });
            YinPitchDetector d;
            d.prepare (sr);
            int firstGood = -1, wrongAfter = 0;
            for (int i = 0; i < (int) v.size(); ++i)
                if (d.push (v[(size_t) i]) && i >= jumpAt)
                {
                    const bool good = d.result().voiced && std::abs (cents (d.result().frequencyHz, 311.0)) < 25.0;
                    if (good && firstGood < 0)
                        firstGood = i - jumpAt;
                    else if (! good && firstGood >= 0)
                        ++wrongAfter;
                }
            expect (firstGood >= 0 && firstGood < (int) (0.045 * sr), "first correct estimate " + juce::String (firstGood * 1000.0 / sr, 1) + " ms after the jump");
            expectEquals (wrongAfter, 0);
            logMessage ("  196 -> 311 Hz jump: first correct estimate after " + juce::String (firstGood * 1000.0 / sr, 1) + " ms");
        }

        beginTest ("Detector: NaN/Inf/huge samples behave exactly like zeros and recovery is immediate");
        {
            const double sr = 48000.0;
            auto clean = voice (sr, (int) (0.6 * sr), 233.0);
            auto dirty = clean;
            for (int i = 12000; i < 12040; ++i) { dirty[(size_t) i] = (i % 3 == 0) ? qNaN : (i % 3 == 1 ? posInf : -posInf); clean[(size_t) i] = 0.0f; }
            dirty[15000] = 1.0e30f; clean[15000] = 0.0f;
            YinPitchDetector a, b;
            a.prepare (sr); b.prepare (sr);
            bool same = true; int lastGoodFrames = 0;
            for (size_t i = 0; i < clean.size(); ++i)
            {
                const bool na = a.push (clean[i]), nb = b.push (dirty[i]);
                same &= (na == nb) && juce::exactlyEqual (a.result().frequencyHz, b.result().frequencyHz) && a.result().voiced == b.result().voiced;
                if (na && i > 20000 && b.result().voiced && std::abs (cents (b.result().frequencyHz, 233.0)) < 10.0)
                    ++lastGoodFrames;
            }
            expect (same, "bit-identical to zero substitution");
            expect (lastGoodFrames > 30, "recovered after the bursts");
        }

        beginTest ("ScaleQuantizer: known keys and scales");
        {
            expectEquals (ScaleQuantizer::nearestNote (60.4f, 0, Sc::major), 60.0f);
            expectEquals (ScaleQuantizer::nearestNote (61.4f, 0, Sc::major), 62.0f);   // C# is out of C major; D is closer than C
            expectEquals (ScaleQuantizer::nearestNote (70.4f, 9, Sc::naturalMinor), 71.0f); // A minor: A# -> B
            expectEquals (ScaleQuantizer::nearestNote (66.0f, 9, Sc::minorPentatonic), 67.0f); // A C D E G: F# -> G
            expectEquals (ScaleQuantizer::nearestNote (68.9f, 9, Sc::harmonicMinor), 69.0f);
            expectEquals (ScaleQuantizer::nearestNote (68.2f, 9, Sc::harmonicMinor), 68.0f);   // G# is in A harmonic minor
            expectEquals (ScaleQuantizer::nearestNote (68.2f, 9, Sc::naturalMinor), 69.0f);    // ...but not in natural minor (A is closer than G)
            expectEquals (ScaleQuantizer::nearestNote (61.49f, 3, Sc::chromatic), 61.0f);
            expectEquals (ScaleQuantizer::nearestNote (61.51f, 3, Sc::chromatic), 62.0f);
            expectEquals (ScaleQuantizer::nearestNote (60.0f - 20.3f, 7, Sc::majorPentatonic), 40.0f); // G major pent: G A B D E; E2 = 40
            expect (std::isnan (ScaleQuantizer::nearestNote (qNaN, 0, Sc::major)));
            // Exact note lists in C (degrees above the key), so a wrong bit in any scale is caught.
            const std::vector<int> degrees[] = { { 0,1,2,3,4,5,6,7,8,9,10,11 }, { 0,2,4,5,7,9,11 }, { 0,2,3,5,7,8,10 },
                                                 { 0,2,3,5,7,8,11 }, { 0,2,4,7,9 }, { 0,3,5,7,10 } };
            for (int sc = 0; sc < ScaleQuantizer::numScales; ++sc)
                for (int d = 0; d < 12; ++d)
                {
                    const bool want = std::find (degrees[sc].begin(), degrees[sc].end(), d) != degrees[sc].end();
                    expect (ScaleQuantizer::isInScale (60 + d, 0, (Sc) sc) == want, juce::String (ScaleQuantizer::name ((Sc) sc)) + " degree " + juce::String (d));
                    expect (ScaleQuantizer::isInScale (60 + d + 2, 2, (Sc) sc) == want, "key D, " + juce::String (ScaleQuantizer::name ((Sc) sc)) + " degree " + juce::String (d));
                }
            for (int sc = 0; sc < ScaleQuantizer::numScales; ++sc)
                for (int key = 0; key < 12; ++key)
                    for (float m = 20.0f; m < 110.0f; m += 0.37f)
                    {
                        const float t = ScaleQuantizer::nearestNote (m, key, (Sc) sc);
                        expect (ScaleQuantizer::isInScale ((int) t, key, (Sc) sc));
                        expect (std::abs (t - m) <= 1.5f + 1.0e-4f);
                    }
            // Hysteresis: between C and C#, the old target is kept until the pitch is clearly past the middle.
            expectEquals (ScaleQuantizer::select (60.55f, 0, Sc::chromatic, 60.0f), 60.0f);
            expectEquals (ScaleQuantizer::select (60.75f, 0, Sc::chromatic, 60.0f), 61.0f);
            expectEquals (ScaleQuantizer::select (60.55f, 0, Sc::chromatic, -1.0f), 61.0f);
        }

        beginTest ("Shifter: known intervals come out at the expected frequency after the latency");
        {
            for (double sr : { 44100.0, 48000.0 })
                for (double f : { 150.0, 220.0, 330.0 })
                    for (int semis : { -12, -5, -1, 1, 3, 7, 12 })
                    {
                        PsolaPitchShifter sh;
                        sh.prepare (sr, 80.0f);
                        sh.setPeriod ((float) (sr / f));
                        const float ratio = (float) std::pow (2.0, semis / 12.0);
                        sh.setRatio (ratio);
                        const int n = (int) (0.8 * sr);
                        const auto in = voice (sr, n, f);
                        Vec out ((size_t) n);
                        for (int i = 0; i < n; ++i)
                            out[(size_t) i] = sh.process (in[(size_t) i]);
                        const int from = (int) (0.4 * sr);
                        const double got = autocorrFreq (out.data() + from, (int) (0.3 * sr), sr, f * ratio);
                        const double err = cents (got, f * ratio);
                        expectLessThan (std::abs (err), 8.0);
                        const double gain = rms (out.data() + from, (int) (0.3 * sr)) / rms (in.data() + from, (int) (0.3 * sr));
                        // Up-shifts keep the spectral envelope, so this 1/h test voice (no formants) loses its upper
                        // harmonics' energy: about -6 dB at +12. That is expected, not a gain error; just catch dropouts.
                        expect (gain > 0.4 && gain < 1.6, "level " + juce::String (gain, 2) + " for " + juce::String (semis) + " semitones");
                    }
            logMessage ("  3 pitches x 7 intervals x 2 rates: all within 8 cents, level within 0.4x..1.6x (down to 0.5x at +12 is the expected spectral-envelope effect)");
        }

        beginTest ("Shifter: ratio 1 is the input delayed by exactly getLatencySamples()");
        {
            const double sr = 48000.0;
            PsolaPitchShifter sh;
            sh.prepare (sr, 80.0f);
            sh.setPeriod (200.0f); // 240 Hz, a whole number of samples
            const auto in = voice (sr, 20000, 240.0);
            double worst = 0;
            for (int i = 0; i < 20000; ++i)
            {
                const float y = sh.process (in[(size_t) i]);
                if (i > 6000)
                    worst = std::max (worst, (double) std::abs (y - in[(size_t) (i - sh.getLatencySamples())]));
            }
            expectLessThan (worst, 1.0e-4);
            logMessage ("  latency " + juce::String (sh.getLatencySamples()) + " samples = " + juce::String (sh.getLatencySamples() * 1000.0 / sr, 2) + " ms at 48 kHz; max error " + juce::String (worst, 8));
        }

        beginTest ("Corrector: pulls a sharp note onto the note, honours strength, speed, bypass and mix");
        {
            const double sr = 48000.0;
            const double target = 220.0, sung = target * std::pow (2.0, 35.0 / 1200.0); // +35 cents
            const auto in = voice (sr, (int) (1.2 * sr), sung);
            const int from = (int) (0.8 * sr), len = (int) (0.35 * sr);

            auto freqOut = [&] (const PitchCorrectorPrototype::Settings& st)
            {
                PitchCorrectorPrototype c;
                c.prepare (sr);
                c.setSettings (st);
                const auto out = runCorrector (c, in, 128);
                return cents (autocorrFreq (out.data() + from, len, sr, target), target);
            };

            expectLessThan (std::abs (freqOut (settings (9, Sc::major, 1.0f, 0.0f))), 8.0);      // full, instant
            expectLessThan (std::abs (freqOut (settings (0, Sc::chromatic, 1.0f, 30.0f))), 8.0);  // chromatic, fast glide
            expectLessThan (std::abs (freqOut (settings (0, Sc::chromatic, 0.5f, 0.0f)) - 17.5), 8.0); // half strength
            expectLessThan (std::abs (freqOut (settings (0, Sc::chromatic, 0.0f, 0.0f)) - 35.0), 8.0); // strength 0 leaves it alone
            auto mixHalf = settings (0, Sc::chromatic, 1.0f, 0.0f);
            mixHalf.mix = 0.0f;
            expectLessThan (std::abs (freqOut (mixHalf) - 35.0), 8.0);

            // Slow retune: after 100 ms the correction is only part of the way there.
            PitchCorrectorPrototype slow, fast;
            slow.prepare (sr); fast.prepare (sr);
            slow.setSettings (settings (0, Sc::chromatic, 1.0f, 300.0f));
            fast.setSettings (settings (0, Sc::chromatic, 1.0f, 10.0f));
            runCorrector (slow, in, 64);
            runCorrector (fast, in, 64);
            // (after 1.2 s both have converged; compare mid-way instead)
            slow.reset(); fast.reset();
            const Vec part (in.begin(), in.begin() + (int) (0.3 * sr));
            runCorrector (slow, part, 64);
            runCorrector (fast, part, 64);
            expect (std::abs (slow.telemetry().correctionCents) < 0.7 * 35.0, "slow retune still gliding: " + juce::String (slow.telemetry().correctionCents, 1));
            expectLessThan (std::abs (fast.telemetry().correctionCents + 35.0f), 4.0f);

            // Bypass is the delayed input, bit for bit.
            PitchCorrectorPrototype b;
            b.prepare (sr);
            auto sb = settings (0, Sc::chromatic, 1.0f, 0.0f);
            sb.bypass = true;
            b.setSettings (sb);
            b.reset();
            const auto outB = runCorrector (b, in, 100);
            bool exact = true;
            for (size_t i = (size_t) b.getLatencySamples(); i < in.size(); ++i)
                exact &= juce::exactlyEqual (outB[i], in[i - (size_t) b.getLatencySamples()]);
            expect (exact, "bypass == delayed dry, bit-exact");
        }

        beginTest ("Corrector: output is independent of block size, and reset restores a fresh state");
        {
            const double sr = 44100.0;
            const auto in = voice (sr, (int) (0.9 * sr), [sr] (int i) { return 180.0 + 90.0 * (i > (int) (0.4 * sr)); });
            auto make = [&]
            {
                auto c = std::make_unique<PitchCorrectorPrototype>();
                c->prepare (sr);
                c->setSettings (settings (2, Sc::major, 0.8f, 25.0f));
                return c;
            };
            auto ref = make();
            const auto want = runCorrector (*ref, in, 1);
            for (int block : { 7, 64, 480, 4096, 100000 })
            {
                auto c = make();
                expect (runCorrector (*c, in, block) == want, "block " + juce::String (block));
            }
            ref->reset();
            expect (runCorrector (*ref, in, 333) == want, "after reset()");
        }

        beginTest ("Corrector: jump between notes stays bounded and settles");
        {
            const double sr = 48000.0;
            const auto in = voice (sr, (int) (1.0 * sr), [sr] (int i) { return i < (int) (0.5 * sr) ? 207.0 : 262.0; }); // flat Ab3 -> flat C4
            PitchCorrectorPrototype c;
            c.prepare (sr);
            c.setSettings (settings (0, Sc::chromatic, 1.0f, 15.0f));
            const auto out = runCorrector (c, in, 256);
            float peak = 0;
            for (float s : out) { expect (std::isfinite (s)); peak = std::max (peak, std::abs (s)); }
            expect (peak < 0.4f * 1.6f * 1.5f, "no blow-up: peak " + juce::String (peak, 2));
            const double f1 = autocorrFreq (out.data() + (int) (0.35 * sr), (int) (0.12 * sr), sr, 207.65);
            const double f2 = autocorrFreq (out.data() + (int) (0.85 * sr), (int) (0.12 * sr), sr, 261.63);
            logMessage ("  before jump: " + juce::String (cents (f1, 207.65), 1) + " cents from G#3; after: " + juce::String (cents (f2, 261.63), 1) + " cents from C4");
            expectLessThan (std::abs (cents (f1, 207.65)), 10.0);
            expectLessThan (std::abs (cents (f2, 261.63)), 10.0);
        }

        beginTest ("Corrector: noise and silence pass through at the same level, finite, no pitch moves");
        {
            for (double sr : { 44100.0, 48000.0 })
            {
                PitchCorrectorPrototype c;
                c.prepare (sr);
                c.setSettings (settings (0, Sc::major, 1.0f, 0.0f));
                juce::Random rng (3);
                Vec noise ((size_t) sr);
                for (auto& s : noise)
                    s = (rng.nextFloat() - 0.5f) * 0.6f;
                const auto out = runCorrector (c, noise, 480);
                const int from = (int) (0.3 * sr);
                const double ratio = rms (out.data() + from, (int) (0.5 * sr)) / rms (noise.data() + from, (int) (0.5 * sr));
                expect (ratio > 0.8 && ratio < 1.25, "noise level ratio " + juce::String (ratio, 2));
                for (float s : out) expect (std::isfinite (s));

                c.reset();
                const auto z = runCorrector (c, Vec ((size_t) sr, 0.0f), 512);
                expectEquals (rms (z.data(), (int) z.size()), 0.0);
            }
        }

        beginTest ("Corrector: NaN/Inf/huge input is treated as zero, bit-exact, and output stays finite");
        {
            const double sr = 48000.0;
            auto clean = voice (sr, (int) (0.8 * sr), 225.0);
            auto dirty = clean;
            for (int i = 14000; i < 14100; ++i) { dirty[(size_t) i] = (i % 3 == 0) ? qNaN : (i % 3 == 1 ? posInf : -posInf); clean[(size_t) i] = 0.0f; }
            dirty[20000] = 1.0e30f; clean[20000] = 0.0f;
            dirty[20001] = -1.0e38f; clean[20001] = 0.0f;
            auto mk = [&] { PitchCorrectorPrototype c; c.prepare (sr); c.setSettings (settings (0, Sc::chromatic, 1.0f, 20.0f)); return c; };
            auto a = mk(), b = mk();
            const auto oa = runCorrector (a, clean, 200), ob = runCorrector (b, dirty, 200);
            expect (oa == ob, "identical to zero substitution");
            for (float s : ob) expect (std::isfinite (s));
        }

        beginTest ("Corrector: odd settings are sanitised");
        {
            PitchCorrectorPrototype c;
            c.prepare (48000.0);
            auto s = settings (-5, Sc::major, qNaN, posInf);
            s.mix = qNaN; s.refA4Hz = -1.0f; s.holdMs = qNaN; s.minConfidence = posInf;
            c.setSettings (s);
            const auto in = voice (48000.0, 24000, 220.0);
            for (float y : runCorrector (c, in, 512)) expect (std::isfinite (y));
        }
    }
};

static PitchCorrectionTests pitchCorrectionTests;
