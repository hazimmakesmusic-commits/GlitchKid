#include "TestHelpers.h"
#include "../Source/DSP/CleanLab.h"
#include "../Source/DSP/Compressor.h"
#include "../Source/DSP/ParametricEq.h"
#include "../Source/AutoMode/MixEngine.h"
#include "../Source/UI/PlaygroundWidgets.h"

extern std::atomic<long> g_allocations; // counted in FormantShiftTests.cpp (it replaces the global operator new)

// CLEAN LAB: corrective EQ (rumble, low-mid build-up), resonance suppression, de-esser, amount law, interaction with the other stages and with BUILD, state,
// latency, allocation and safety. All signals are synthetic: they show the behaviour is correct and bounded, not that it sounds good on a real vocal.
namespace
{
using Buf = juce::AudioBuffer<float>;
constexpr double fs = 48000.0;
constexpr double twoPi = 6.283185307179586;

// A voice-like source: harmonics of a gliding fundamental shaped by two moving formants (a vowel that changes), 1/h tilt, a slow syllable envelope.
std::vector<float> voice (int n, float amp, double f0Lo = 130.0, double f0Hi = 230.0, double glideHz = 0.45, bool changingVowel = true)
{
    std::vector<float> x ((size_t) n, 0.0f);
    double phase[96] = {};
    for (int i = 0; i < n; ++i)
    {
        const double t = (double) i / fs;
        const double f0 = f0Lo + (f0Hi - f0Lo) * (0.5 + 0.5 * std::sin (twoPi * glideHz * t));
        const double mix = changingVowel ? 0.5 + 0.5 * std::sin (twoPi * 0.31 * t) : 0.0;
        const double F1 = 700.0 + (400.0 - 700.0) * mix, F2 = 1200.0 + (2200.0 - 1200.0) * mix, F3 = 2600.0 + 400.0 * mix;
        const double env = 0.6 + 0.4 * std::sin (twoPi * 2.7 * t);
        double s = 0.0;
        for (int h = 1; h <= 90; ++h)
        {
            const double f = f0 * h;
            if (f > 9000.0)
                break;
            phase[h] += twoPi * f / fs;
            auto bump = [f] (double c, double w, double g) { return g * std::exp (-0.5 * std::pow ((f - c) / w, 2.0)); };
            const double a = (0.25 + bump (F1, 150.0, 1.0) + bump (F2, 250.0, 0.6) + bump (F3, 350.0, 0.35)) / std::pow ((double) h, 0.8);
            s += a * std::sin (phase[h]);
        }
        x[(size_t) i] = (float) (amp * env * s * 0.45);
    }
    return x;
}

std::vector<float> held (int n, float amp, double f0 = 200.0)
{
    std::vector<float> x ((size_t) n, 0.0f);
    for (int i = 0; i < n; ++i)
    {
        double s = 0.0;
        for (int h = 1; h <= 30; ++h)
            s += std::sin (twoPi * f0 * h * (double) i / fs) / std::pow ((double) h, 0.9);
        x[(size_t) i] = (float) (amp * s * 0.4);
    }
    return x;
}

void peakingInPlace (std::vector<float>& x, double f, double q, double gainDb)
{
    const double A = std::pow (10.0, gainDb / 40.0), w0 = twoPi * f / fs, c = std::cos (w0), alpha = std::sin (w0) / (2.0 * q), a0 = 1.0 + alpha / A;
    const double b0 = (1.0 + alpha * A) / a0, b1 = -2.0 * c / a0, b2 = (1.0 - alpha * A) / a0, a1 = b1, a2 = (1.0 - alpha / A) / a0;
    double z1 = 0.0, z2 = 0.0;
    for (auto& v : x)
    {
        const double y = b0 * v + z1;
        z1 = b1 * v - a1 * y + z2;
        z2 = b2 * v - a2 * y;
        v = (float) y;
    }
}

std::vector<float> whiteNoise (int n, juce::int64 seed, float amp)
{
    juce::Random rng (seed);
    std::vector<float> x ((size_t) n);
    for (auto& v : x)
        v = (rng.nextFloat() * 2.0f - 1.0f) * amp;
    return x;
}

void highpassInPlace (std::vector<float>& x, double f)
{
    const double w0 = twoPi * f / fs, c = std::cos (w0), a = std::sin (w0) / (2.0 * 0.7071), a0 = 1.0 + a;
    const double b0 = (1.0 + c) * 0.5 / a0, b1 = -(1.0 + c) / a0, a1 = -2.0 * c / a0, a2 = (1.0 - a) / a0;
    double z1 = 0.0, z2 = 0.0;
    for (auto& v : x)
    {
        const double y = b0 * v + z1;
        z1 = b1 * v - a1 * y + z2;
        z2 = b0 * v - a2 * y;
        v = (float) y;
    }
}

// Bursts of high-passed noise (S sounds) of `burstMs` every `periodMs`, silent in between.
std::vector<float> sibilantBursts (int n, float amp, int burstMs = 120, int periodMs = 500)
{
    auto x = whiteNoise (n, 5, amp);
    highpassInPlace (x, 5000.0);
    for (int i = 0; i < n; ++i)
        if ((int) ((double) i * 1000.0 / fs) % periodMs >= burstMs)
            x[(size_t) i] = 0.0f;
    return x;
}

CleanLab::Settings on (float amount, float tone = 1.0f, float res = 1.0f, float ess = 1.0f)
{
    CleanLab::Settings s;
    s.on = true;
    s.amount = amount;
    s.tone = tone;
    s.res = res;
    s.ess = ess;
    return s;
}

struct Run
{
    std::vector<std::vector<float>> out;
    CleanLab::Applied last;
    float maxEss = 0.0f;
};

Run run (const CleanLab::Settings& s, const std::vector<std::vector<float>>& in, bool corrective = true, bool ess = true, int block = 480, CleanLab* labOut = nullptr)
{
    CleanLab local;
    CleanLab& lab = labOut != nullptr ? *labOut : local;
    lab.setSettings (s);
    lab.prepare (fs);
    Run r;
    r.out = in;
    const int channels = (int) in.size(), n = (int) in[0].size();
    for (int pos = 0; pos < n; pos += block)
    {
        const int len = juce::jmin (block, n - pos);
        Buf b (channels, len);
        for (int ch = 0; ch < channels; ++ch)
            b.copyFrom (ch, 0, in[(size_t) ch].data() + pos, len);
        lab.setSettings (s);
        if (corrective)
            lab.processCorrective (b);
        if (ess)
            lab.processDeEss (b);
        for (int ch = 0; ch < channels; ++ch)
            std::copy (b.getReadPointer (ch), b.getReadPointer (ch) + len, r.out[(size_t) ch].begin() + pos);
        r.maxEss = juce::jmax (r.maxEss, lab.applied().essGrDb);
    }
    r.last = lab.applied();
    return r;
}

double bandDb (const std::vector<float>& x, float lo, float hi, size_t from = 0)
{
    constexpr int order = 13, N = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> frame ((size_t) N * 2);
    double total = 0.0;
    int frames = 0;
    for (size_t pos = juce::jmax (from, x.size() / 2); pos + (size_t) N <= x.size(); pos += (size_t) N / 2)
    {
        for (int i = 0; i < N; ++i)
            frame[(size_t) i] = x[pos + (size_t) i] * (0.5f - 0.5f * std::cos ((float) twoPi * (float) i / (float) N));
        std::fill (frame.begin() + N, frame.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (frame.data());
        for (int k = 0; k < N / 2; ++k)
        {
            const double f = (double) k * fs / (double) N;
            if (f >= lo && f < hi)
                total += (double) frame[(size_t) k] * (double) frame[(size_t) k];
        }
        ++frames;
    }
    return 10.0 * std::log10 (total / (double) juce::jmax (1, frames) + 1.0e-20);
}

double rmsDb (const std::vector<float>& x, size_t from = 0)
{
    double s = 0.0;
    for (size_t i = from; i < x.size(); ++i)
        s += (double) x[i] * (double) x[i];
    return 10.0 * std::log10 (s / (double) (x.size() - from) + 1.0e-20);
}

struct CleanLabTests : public juce::UnitTest
{
    CleanLabTests() : juce::UnitTest ("CLEAN LAB", "GlitchKid") {}

    void runTest() override
    {
        const int n = (int) fs * 14;

        // the signals used by several groups
        const auto cleanVoice = voice (n, 0.5f);
        auto resonantVoice = cleanVoice;
        peakingInPlace (resonantVoice, 2900.0, 12.0, 14.0);
        auto muddyVoice = voice (n, 0.5f);
        peakingInPlace (muddyVoice, 260.0, 0.7, 12.0);
        auto rumblyVoice = cleanVoice;
        for (size_t i = 0; i < rumblyVoice.size(); ++i)
            rumblyVoice[i] += 0.06f * (float) std::sin (twoPi * 38.0 * (double) i / fs) + 0.03f * (float) std::sin (twoPi * 25.0 * (double) i / fs);
        const auto bursts = sibilantBursts (n, 0.3f);
        auto sibilantVoice = cleanVoice;
        for (size_t i = 0; i < sibilantVoice.size(); ++i)
            sibilantVoice[i] += bursts[i];

        beginTest ("neutral: off is an exact pass-through, amount 0 changes nothing and reports nothing, trims at 0 switch their section off");
        {
            const auto in = std::vector<std::vector<float>> { whiteNoise (n / 2, 3, 0.4f), whiteNoise (n / 2, 4, 0.4f) };
            CleanLab::Settings off = on (1.0f);
            off.on = false;
            expect (run (off, in).out == in, "off = bit-exact");
            const auto zero = run (on (0.0f), { resonantVoice, muddyVoice });
            expect (zero.out[0] == resonantVoice && zero.out[1] == muddyVoice, "amount 0 = bit-exact");
            expect (zero.last.mudDb <= 0.0f && zero.last.resDb[0] <= 0.0f && zero.last.rumbleAmount <= 0.0f && zero.last.essGrDb <= 0.0f, "and nothing is reported");
            const auto noTone = run (on (1.0f, 0.0f, 0.0f, 0.0f), { rumblyVoice });
            expect (noTone.out[0] == rumblyVoice, "all section trims at 0 = bit-exact");
        }

        beginTest ("de-esser: lowers S bursts, leaves vowels alone, keeps the S audible, follows the amount, treats stereo identically");
        {
            const std::vector<std::vector<float>> in { sibilantVoice };
            const auto r = run (on (1.0f), in);
            const double before = bandDb (sibilantVoice, 6000.0f, 10000.0f), after = bandDb (r.out[0], 6000.0f, 10000.0f);
            logMessage ("de-esser: 6-10 kHz " + juce::String (after - before, 2) + " dB, body (100 Hz-2 kHz) " + juce::String (bandDb (r.out[0], 100.0f, 2000.0f) - bandDb (sibilantVoice, 100.0f, 2000.0f), 2) + " dB, max GR " + juce::String (r.maxEss, 1) + " dB");
            expect (after - before < -2.5, "sibilant energy is lowered");
            expect (after - before > -12.0, "but the S is still clearly there (not removed)");
            expectWithinAbsoluteError (bandDb (r.out[0], 100.0f, 2000.0f), bandDb (sibilantVoice, 100.0f, 2000.0f), 0.45, "the body of the voice is essentially untouched");
            expect (r.maxEss > 3.0 && r.maxEss <= 9.01f, "the gain reduction is reported and bounded: " + juce::String (r.maxEss, 2));

            // the voice alone (vowels, harmonics up to 9 kHz): almost no reduction
            const auto v = run (on (1.0f), { cleanVoice }, false, true);
            expect (v.maxEss < 1.0f, "vowels are not de-essed: " + juce::String (v.maxEss, 2) + " dB");
            expect (std::abs (rmsDb (v.out[0]) - rmsDb (cleanVoice)) < 0.15, "and the voice level is unchanged");

            // intelligibility proxy: the 4-10 kHz envelope of the S bursts keeps its shape
            auto envOf = [] (const std::vector<float>& x)
            {
                std::vector<double> e;
                for (size_t i = 0; i + 480 <= x.size(); i += 480)
                {
                    double s = 0.0;
                    for (size_t j = 0; j < 480; ++j)
                        s += (double) x[i + j] * (double) x[i + j];
                    e.push_back (10.0 * std::log10 (s / 480.0 + 1.0e-12));
                }
                return e;
            };
            auto hp = [] (std::vector<float> x) { highpassInPlace (x, 4000.0); return x; };
            const auto e1 = envOf (hp (sibilantVoice)), e2 = envOf (hp (r.out[0]));
            double m1 = 0, m2 = 0;
            for (size_t i = 0; i < e1.size(); ++i) { m1 += e1[i]; m2 += e2[i]; }
            m1 /= (double) e1.size(); m2 /= (double) e2.size();
            double cov = 0, v1 = 0, v2 = 0;
            for (size_t i = 0; i < e1.size(); ++i) { cov += (e1[i] - m1) * (e2[i] - m2); v1 += (e1[i] - m1) * (e1[i] - m1); v2 += (e2[i] - m2) * (e2[i] - m2); }
            expect (cov / std::sqrt (v1 * v2 + 1.0e-9) > 0.97, "the pattern of the consonants is preserved (envelope correlation " + juce::String (cov / std::sqrt (v1 * v2), 3) + ")");

            // amount law
            const float g25 = run (on (0.25f), in).maxEss, g50 = run (on (0.5f), in).maxEss, g100 = r.maxEss;
            expect (g25 < g50 && g50 < g100 + 1.0e-3f, "more CLEAN = more reduction: " + juce::String (g25, 1) + ", " + juce::String (g50, 1) + ", " + juce::String (g100, 1) + " dB");
            expect (run (on (1.0f, 1.0f, 1.0f, 0.0f), in, false, true).out[0] == sibilantVoice, "the de-esser trim at 0 switches it off");

            // stereo: identical channels give identical outputs, and a silent channel stays silent
            const auto st = run (on (1.0f), { sibilantVoice, sibilantVoice });
            expect (st.out[0] == st.out[1] && st.out[0] == r.out[0], "L == R == mono");
            const std::vector<float> silent (sibilantVoice.size(), 0.0f);
            const auto one = run (on (1.0f), { sibilantVoice, silent });
            double peak = 0.0;
            for (float x : one.out[1])
                peak = juce::jmax (peak, (double) std::abs (x));
            expect (peak <= 0.0, "the silent channel stays silent");
        }

        beginTest ("de-esser: silence, very low levels, non-finite input and extremes cannot destabilise it");
        {
            const std::vector<float> zeros ((size_t) n / 2, 0.0f);
            const auto z = run (on (1.0f), { zeros });
            double peak = 0.0;
            for (float x : z.out[0])
                peak = juce::jmax (peak, (double) std::abs (x));
            expect (peak <= 0.0 && z.maxEss <= 0.0f, "silence in, silence out, no reduction");
            const auto quiet = whiteNoise (n / 2, 9, 3.0e-5f); // -90 dBFS
            const auto q = run (on (1.0f), { quiet });
            expect (q.maxEss < 0.01f, "a -90 dBFS signal is never reduced");
            auto bad = whiteNoise (n / 2, 8, 0.3f);
            bad[1000] = std::numeric_limits<float>::quiet_NaN();
            bad[2000] = std::numeric_limits<float>::infinity();
            const auto b = run (on (1.0f), { bad });
            bool finite = true;
            for (float x : b.out[0])
                finite = finite && std::isfinite (x);
            expect (finite, "NaN and infinity do not reach the output or poison later audio");
            const auto hot = run (on (1.0f), { whiteNoise (n / 2, 21, 1.0f) });
            double hp = 0.0;
            for (float x : hot.out[0])
                hp = juce::jmax (hp, (double) std::abs (x));
            expect (hp <= 2.0 && std::isfinite (hp), "full-scale noise stays bounded");
        }

        beginTest ("resonance: a known narrow resonance on a moving voice is found and lowered, a clean voice and a held note are left alone");
        {
            const auto r = run (on (1.0f), { resonantVoice });
            int found = -1;
            for (int i = 0; i < CleanLab::numRes; ++i)
                if (r.last.resDb[i] > 1.0f && std::abs (r.last.resHz[i] - 2900.0f) < 300.0f)
                    found = i;
            logMessage ("resonance found at " + juce::String (found >= 0 ? r.last.resHz[found] : 0.0f, 0) + " Hz, cut " + juce::String (found >= 0 ? r.last.resDb[found] : 0.0f, 2) + " dB");
            expect (found >= 0, "the 2.9 kHz resonance is detected");
            if (found >= 0)
                expect (r.last.resDb[found] <= 6.001f, "bounded at 6 dB");
            const double ref = bandDb (resonantVoice, 2000.0f, 2400.0f) + bandDb (resonantVoice, 3600.0f, 4200.0f);
            const double lowered = (bandDb (resonantVoice, 2700.0f, 3150.0f) - bandDb (r.out[0], 2700.0f, 3150.0f));
            const double refAfter = bandDb (r.out[0], 2000.0f, 2400.0f) + bandDb (r.out[0], 3600.0f, 4200.0f);
            logMessage ("resonance band lowered by " + juce::String (lowered, 2) + " dB; neighbours changed by " + juce::String ((refAfter - ref) / 2.0, 2) + " dB");
            expect (lowered > 1.5, "the resonant band is measurably lower");
            expectWithinAbsoluteError (bandDb (r.out[0], 200.0f, 800.0f), bandDb (resonantVoice, 200.0f, 800.0f), 0.5, "the body is untouched");

            for (const auto& sig : { cleanVoice, voice (n, 0.4f, 90.0, 160.0, 0.8, false), voice (n, 0.4f, 160.0, 290.0, 0.6, true), voice (n, 0.3f, 110.0, 190.0, 1.1, true) })
            {
                const auto c = run (on (1.0f), { sig });
                expect (c.last.resDb[0] < 0.3f && c.last.resDb[1] < 0.3f && c.last.resDb[2] < 0.3f, "no resonance cut on a clean moving voice");
            }
            const auto h = run (on (1.0f), { held (n, 0.5f) });
            expect (h.last.resDb[0] < 0.3f && h.last.resDb[1] < 0.3f && h.last.resDb[2] < 0.3f && h.last.mudDb < 0.3f && h.last.rumbleAmount < 0.05f, "a held note's harmonics are not mistaken for a resonance, mud or rumble");
        }

        beginTest ("resonance: several resonances use at most three bells within the limit; stable, no modulation; silence and level changes do not move it");
        {
            auto many = cleanVoice;
            for (double f : { 1800.0, 2900.0, 4300.0, 6100.0 })
                peakingInPlace (many, f, 12.0, 14.0);
            const auto r = run (on (1.0f), { many });
            int active = 0;
            for (int i = 0; i < CleanLab::numRes; ++i)
            {
                active += r.last.resDb[i] > 0.3f ? 1 : 0;
                expect (r.last.resDb[i] <= 6.001f, "per-band limit");
            }
            expect (active <= CleanLab::numRes && active >= 1, "at most three reductions are active: " + juce::String (active));

            // stability while it runs: record the cut over the last 4 s, then keep going through silence and a +/-12 dB level change
            CleanLab lab;
            lab.setSettings (on (1.0f));
            lab.prepare (fs);
            float lo = 100.0f, hi = -100.0f, afterSilence = 0.0f, atLevel = 0.0f, before = 0.0f;
            auto feed = [&] (const std::vector<float>& sig, size_t from, size_t to, bool record, float gain)
            {
                for (size_t pos = from; pos + 480 <= to; pos += 480)
                {
                    Buf b (1, 480);
                    for (int i = 0; i < 480; ++i)
                        b.setSample (0, i, sig[pos + (size_t) i] * gain);
                    lab.processCorrective (b);
                    lab.processDeEss (b);
                    if (record)
                    {
                        float cut = 0.0f;
                        const auto a = lab.applied();
                        for (int k = 0; k < CleanLab::numRes; ++k)
                            cut = juce::jmax (cut, a.resDb[k]);
                        lo = juce::jmin (lo, cut);
                        hi = juce::jmax (hi, cut);
                    }
                }
            };
            feed (resonantVoice, 0, (size_t) n - 4 * 48000, false, 1.0f);
            feed (resonantVoice, (size_t) n - 4 * 48000, (size_t) n, true, 1.0f);
            expect (hi - lo < 0.5f, "the cut does not wobble: range " + juce::String (hi - lo, 2) + " dB");
            for (int k = 0; k < CleanLab::numRes; ++k)
                before = juce::jmax (before, lab.applied().resDb[k]);
            const std::vector<float> zeros ((size_t) fs * 5, 0.0f);
            feed (zeros, 0, zeros.size(), false, 1.0f);
            for (int k = 0; k < CleanLab::numRes; ++k)
                afterSilence = juce::jmax (afterSilence, lab.applied().resDb[k]);
            expect (std::abs (afterSilence - before) < 0.3f, "5 s of silence does not change the learned cut");
            feed (resonantVoice, 0, (size_t) fs * 6, false, 0.25f); // 12 dB quieter
            for (int k = 0; k < CleanLab::numRes; ++k)
                atLevel = juce::jmax (atLevel, lab.applied().resDb[k]);
            expect (std::abs (atLevel - before) < 1.0f, "a 12 dB level change does not change the cut by more than 1 dB: " + juce::String (atLevel - before, 2));
        }

        beginTest ("resonance: amount 0 and the section trim at 0 do nothing; mono equals stereo");
        {
            expect (run (on (1.0f, 0.0f, 0.0f, 0.0f), { resonantVoice }).out[0] == resonantVoice, "all trims 0");
            const auto resOff = run (on (1.0f, 1.0f, 0.0f, 1.0f), { resonantVoice }, true, false);
            expect (resOff.last.resDb[0] <= 0.0f && resOff.last.resDb[1] <= 0.0f && resOff.last.resDb[2] <= 0.0f, "the resonance trim at 0 applies no resonance cut");
            const auto st = run (on (1.0f), { resonantVoice, resonantVoice }, true, false), mo = run (on (1.0f), { resonantVoice }, true, false);
            expect (st.out[0] == st.out[1] && st.out[0] == mo.out[0], "L == R == mono");
        }

        beginTest ("corrective EQ: low-frequency excess is found and lowered, a muddy build-up is found near its centre, normal voices receive (almost) nothing");
        {
            const auto r = run (on (1.0f), { rumblyVoice }, true, false);
            logMessage ("rumble: amount " + juce::String (r.last.rumbleAmount, 2));
            expect (r.last.rumbleAmount > 0.5f, "rumble detected");
            expect (bandDb (rumblyVoice, 15.0f, 55.0f) - bandDb (r.out[0], 15.0f, 55.0f) > 6.0, "energy under 55 Hz is lowered by more than 6 dB");
            expectWithinAbsoluteError (bandDb (r.out[0], 150.0f, 500.0f), bandDb (rumblyVoice, 150.0f, 500.0f), 0.5, "the low-mids are untouched");
            const auto m = run (on (1.0f), { muddyVoice }, true, false);
            logMessage ("mud: " + juce::String (m.last.mudDb, 2) + " dB at " + juce::String (m.last.mudHz, 0) + " Hz");
            expect (m.last.mudDb > 1.0f && m.last.mudDb <= 3.51f, "a low-mid build-up is lowered, within the 3.5 dB limit");
            expect (m.last.mudHz > 170.0f && m.last.mudHz < 450.0f, "near the build-up");
            expect (bandDb (muddyVoice, 200.0f, 400.0f) - bandDb (m.out[0], 200.0f, 400.0f) > 0.7, "the build-up is measurably lower");
            for (const auto& sig : { cleanVoice, voice (n, 0.4f, 90.0, 160.0, 0.8, false), voice (n, 0.4f, 160.0, 290.0, 0.6, true) })
            {
                const auto c = run (on (1.0f), { sig }, true, false);
                expect (c.last.mudDb < 0.6f && c.last.rumbleAmount < 0.05f, "a normal voice gets almost no corrective EQ: mud " + juce::String (c.last.mudDb, 2) + ", rumble " + juce::String (c.last.rumbleAmount, 2));
            }
            expect (run (on (1.0f, 0.0f), { muddyVoice }, true, false).out[0] == muddyVoice, "tone trim 0 = bit-exact");
        }

        beginTest ("corrective EQ and the user's EQ: a cut the user already made is not repeated, and the graph overlay matches the real DSP");
        {
            const auto direct = run (on (1.0f), { muddyVoice }, true, false);
            auto preCut = muddyVoice;
            peakingInPlace (preCut, 260.0, 0.7, -12.0); // the user's own EQ cut (what the EQ stage would have done before CLEAN LAB)
            const auto afterUser = run (on (1.0f), { preCut }, true, false);
            logMessage ("mud with no user cut " + juce::String (direct.last.mudDb, 2) + " dB, with the user's cut " + juce::String (afterUser.last.mudDb, 2) + " dB");
            expect (afterUser.last.mudDb < direct.last.mudDb - 0.5f && afterUser.last.mudDb < 0.6f, "no double correction");

            // overlay vs the DSP: after learning, steady tones come out at the gain the overlay computes from the applied values
            CleanLab lab;
            lab.setSettings (on (1.0f));
            lab.prepare (fs);
            auto both = muddyVoice;
            peakingInPlace (both, 2900.0, 12.0, 14.0);
            for (size_t pos = 0; pos + 480 <= both.size(); pos += 480)
            {
                Buf b (1, 480);
                for (int i = 0; i < 480; ++i)
                    b.setSample (0, i, both[pos + (size_t) i]);
                lab.processCorrective (b);
            }
            const auto a = lab.applied();
            logMessage ("overlay test: mud " + juce::String (a.mudDb, 2) + " dB @ " + juce::String (a.mudHz, 0) + ", res " + juce::String (a.resDb[0], 2) + " dB @ " + juce::String (a.resHz[0], 0));
            // freeze what was learned, then probe with noise and compare the band gain with the overlay's calculation
            lab.setLearningFrozen (true);
            const auto probe = whiteNoise ((int) fs * 4, 99, 0.25f);
            std::vector<float> probeOut (probe.size());
            for (size_t pos = 0; pos + 480 <= probe.size(); pos += 480)
            {
                Buf b (1, 480);
                for (int i = 0; i < 480; ++i)
                    b.setSample (0, i, probe[pos + (size_t) i]);
                lab.processCorrective (b);
                for (int i = 0; i < 480; ++i)
                    probeOut[pos + (size_t) i] = b.getSample (0, i);
            }
            const auto applied = lab.applied();
            for (float hz : { 150.0f, a.mudHz, 1200.0f, a.resHz[0] > 0.0f ? a.resHz[0] : 2900.0f, 6000.0f })
            {
                const double measured = bandDb (probeOut, hz / 1.07f, hz * 1.07f) - bandDb (probe, hz / 1.07f, hz * 1.07f);
                const double calc = glitchkid::ui::EqGraph::cleanResponseDb (applied, hz, fs);
                expectWithinAbsoluteError (calc, measured, 0.7, "overlay vs DSP at " + juce::String (hz, 0) + " Hz: overlay " + juce::String (calc, 2) + ", DSP " + juce::String (measured, 2));
            }
            expect (applied.mudDb > 1.0f && applied.resDb[0] > 1.0f, "the overlay test really had corrections to compare");
        }

        beginTest ("amount law: more CLEAN is more cleanup, never less; zero is neutral; the sections stay independent");
        {
            auto problem = resonantVoice;
            peakingInPlace (problem, 260.0, 0.7, 12.0);
            for (size_t i = 0; i < problem.size(); ++i)
                problem[i] += bursts[i] + 0.06f * (float) std::sin (twoPi * 38.0 * (double) i / fs);
            double previous = -1.0;
            for (float amount : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
            {
                const auto r = run (on (amount), { problem });
                double d = 0.0;
                for (size_t i = (size_t) fs * 8; i < problem.size(); ++i)
                    d += std::pow ((double) r.out[0][i] - (double) problem[i], 2.0);
                d = std::sqrt (d / (double) (problem.size() - (size_t) fs * 8));
                expect (d >= previous - 1.0e-6, "monotonic in the amount: " + juce::String (amount) + " -> " + juce::String (d, 5));
                previous = d;
                if (amount <= 0.0f)
                    expect (d <= 0.0, "amount 0 changes nothing");
            }
            expect (previous > 0.0, "full amount changes the signal");
        }

        beginTest ("switching on and off and moving the amount: no clicks, finite, bounded");
        {
            CleanLab lab;
            lab.setSettings (CleanLab::Settings {});
            lab.prepare (fs);
            const auto& src = rumblyVoice; // no S bursts: the de-esser's 0.5 ms attack is a deliberate fast change and is tested elsewhere
            std::vector<float> out;
            juce::Random rng (3);
            for (size_t pos = 0; pos + 480 <= (size_t) fs * 8; pos += 480)
            {
                Buf b (1, 480);
                for (int i = 0; i < 480; ++i)
                    b.setSample (0, i, src[pos + (size_t) i]);
                CleanLab::Settings s = on (rng.nextFloat());
                s.on = pos >= (size_t) fs * 2; // off for the first 2 s, then on, with the amount changing every block
                lab.setSettings (s);
                lab.processCorrective (b);
                lab.processDeEss (b);
                for (int i = 0; i < 480; ++i)
                    out.push_back (b.getSample (0, i));
            }
            double worst = 0.0, settled = 0.0;
            bool finite = true;
            for (size_t i = 1; i < out.size(); ++i)
            {
                finite = finite && std::isfinite (out[i]) && std::abs (out[i]) < 3.0f;
                const double step = std::abs (((double) out[i] - (double) src[i]) - ((double) out[i - 1] - (double) src[i - 1]));
                worst = juce::jmax (worst, step);
                if (i < (size_t) fs * 2)
                    settled = juce::jmax (settled, step);
            }
            expect (finite, "finite and bounded");
            expectEquals (settled, 0.0); // while off the output is exactly the input
            expect (worst < 0.03, "the correction changes smoothly: worst step of (out - in) " + juce::String (worst, 4));
        }

        beginTest ("with the compressor and AIR LAB in the chain: finite, no clipping caused by the stage, the order is respected");
        {
            // user EQ -> CLEAN LAB corrective -> compressor -> CLEAN LAB de-esser -> (AIR LAB not repeated here: see the processor test)
            ParametricEq eq;
            eq.prepare (fs);
            eq.setBand (1, { 400.0f, -3.0f, 1.0f });
            Compressor comp;
            CompressorSettings cs;
            cs.thresholdDb = -20.0f;
            cs.ratio = 3.0f;
            cs.makeupDb = 3.0f;
            comp.setSettings (cs);
            comp.prepare (fs);
            CleanLab lab;
            lab.setSettings (on (0.8f));
            lab.prepare (fs);
            double peak = 0.0;
            bool finite = true;
            for (size_t pos = 0; pos + 480 <= sibilantVoice.size(); pos += 480)
            {
                Buf b (1, 480);
                for (int i = 0; i < 480; ++i)
                    b.setSample (0, i, sibilantVoice[pos + (size_t) i] * 0.9f);
                eq.process (b);
                lab.processCorrective (b);
                comp.process (b);
                lab.processDeEss (b);
                for (int i = 0; i < 480; ++i)
                {
                    finite = finite && std::isfinite (b.getSample (0, i));
                    peak = juce::jmax (peak, (double) std::abs (b.getSample (0, i)));
                }
            }
            expect (finite && peak < 1.5, "finite and bounded through EQ, CLEAN LAB, compressor: peak " + juce::String (peak, 3));
        }

        beginTest ("in the real plug-in: a resonant, muddy vocal learned through the processor reports its corrections, and the TONE graph shows them (snapshot written when GLITCHKID_SNAPSHOT_DIR is set)");
        {
            auto both = muddyVoice;
            peakingInPlace (both, 2900.0, 12.0, 14.0);
            GlitchKidProcessor p;
            th::setParam (p, "clean_on", 1.0f);
            th::setParam (p, "clean_amount", 100.0f);
            th::setParam (p, "comp_ratio", 3.0f);
            th::setParam (p, "comp_thresh", -24.0f);
            p.setUiInt ("uiLevel", 4);
            p.setUiInt ("uiCategory", 1);
            p.prepareToPlay (fs, 480);
            juce::MidiBuffer midi;
            double peak = 0.0;
            for (size_t pos = 0; pos + 480 <= both.size(); pos += 480)
            {
                Buf b (2, 480);
                for (int i = 0; i < 480; ++i) { b.setSample (0, i, both[pos + (size_t) i] * 0.8f); b.setSample (1, i, both[pos + (size_t) i] * 0.8f); }
                p.processBlock (b, midi);
                peak = juce::jmax (peak, (double) b.getMagnitude (0, 480));
            }
            const auto a = p.getCleanApplied();
            logMessage ("processor telemetry: mud " + juce::String (a.mudDb, 2) + " dB @ " + juce::String (a.mudHz, 0) + ", res " + juce::String (a.resDb[0], 2) + " dB @ " + juce::String (a.resHz[0], 0) + ", level change " + juce::String (a.levelChangeDb, 2) + " dB");
            expect (a.mudDb > 1.0f && a.resDb[0] > 1.0f, "the processor reports the corrections it applies");
            expect (std::isfinite (peak) && peak < 2.0, "finite output through EQ, CLEAN LAB, compressor and the rest");
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
            const juce::String outDir = juce::SystemStats::getEnvironmentVariable ("GLITCHKID_SNAPSHOT_DIR", {});
            if (outDir.isNotEmpty() && ed != nullptr)
            {
                auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                juce::File f = juce::File (outDir).getChildFile ("tone_clean_overlay.png");
                f.deleteFile();
                juce::FileOutputStream os (f);
                if (os.openedOk())
                    juce::PNGImageFormat().writeImageToStream (img, os);
            }
        }

        beginTest ("sample rates and block sizes: 44.1, 48, 88.2, 96 and 192 kHz, odd block sizes, mono and stereo stay finite and bounded");
        {
            for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
                for (int block : { 1, 37, 480, 2048 })
                {
                    CleanLab lab;
                    lab.setSettings (on (1.0f));
                    lab.prepare (rate);
                    juce::Random rng (7);
                    bool finite = true;
                    const int total = (int) rate * 3;
                    for (int pos = 0; pos < total; pos += block)
                    {
                        const int len = juce::jmin (block, total - pos);
                        Buf b (2, len);
                        for (int ch = 0; ch < 2; ++ch)
                            for (int i = 0; i < len; ++i)
                                b.setSample (ch, i, (rng.nextFloat() - 0.5f) * 0.6f * (float) std::sin (twoPi * 3.0 * (double) (pos + i) / rate));
                        lab.processCorrective (b);
                        lab.processDeEss (b);
                        for (int ch = 0; ch < 2; ++ch)
                            for (int i = 0; i < len; ++i)
                                finite = finite && std::isfinite (b.getSample (ch, i)) && std::abs (b.getSample (ch, i)) < 4.0f;
                    }
                    expect (finite, "finite at " + juce::String (rate) + " Hz, block " + juce::String (block));
                }
        }

        beginTest ("CPU (information only, not an assertion): stereo, 480-sample blocks, everything on");
        {
            CleanLab lab;
            lab.setSettings (on (1.0f));
            lab.prepare (fs);
            Buf b (2, 480);
            const int blocks = 48000 * 10 / 480;
            const auto t0 = juce::Time::getHighResolutionTicks();
            for (int k = 0; k < blocks; ++k)
            {
                for (int i = 0; i < 480; ++i)
                {
                    const float v = resonantVoice[(size_t) ((k * 480 + i) % (int) resonantVoice.size())];
                    b.setSample (0, i, v);
                    b.setSample (1, i, v);
                }
                lab.processCorrective (b);
                lab.processDeEss (b);
            }
            const double seconds = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
            logMessage ("CLEAN LAB CPU: " + juce::String (seconds * 10.0, 2) + " % of one core at 48 kHz stereo (10 s of audio processed in " + juce::String (seconds, 3) + " s)");
            expect (std::isfinite (b.getSample (0, 0)));
        }

        beginTest ("processor: parameters appended with neutral defaults, state and presets, latency unchanged, no allocation while running, BUILD does not repeat CLEAN LAB's corrections");
        {
            GlitchKidProcessor p;
            expectEquals (p.getParameters().size(), 65);
            const char* ids[] = { "clean_on", "clean_amount", "clean_tone", "clean_res", "clean_ess" };
            for (int i = 0; i < 5; ++i)
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p.getParameters()[60 + i]))
                    expect (r->paramID == ids[i], r->paramID);
            expectEquals (th::getParam (p, "clean_on"), 0.0f);
            expectEquals (th::getParam (p, "clean_amount"), 50.0f);
            expectEquals (th::getParam (p, "clean_tone"), 100.0f);
            expectEquals (th::getParam (p, "clean_res"), 100.0f);
            expectEquals (th::getParam (p, "clean_ess"), 100.0f);

            auto render = [&] (GlitchKidProcessor& proc, const std::vector<float>& src, int seconds)
            {
                proc.prepareToPlay (fs, 480);
                juce::MidiBuffer midi;
                std::vector<float> out;
                for (int pos = 0; pos + 480 <= (int) fs * seconds; pos += 480)
                {
                    Buf b (2, 480);
                    for (int i = 0; i < 480; ++i) { b.setSample (0, i, src[(size_t) (pos + i)]); b.setSample (1, i, src[(size_t) (pos + i)]); }
                    proc.processBlock (b, midi);
                    for (int i = 0; i < 480; ++i)
                        out.push_back (b.getSample (0, i));
                }
                return out;
            };
            GlitchKidProcessor off1, off2, withClean;
            th::setParam (withClean, "clean_on", 1.0f);
            th::setParam (withClean, "clean_amount", 100.0f);
            const auto a = render (off1, sibilantVoice, 10), c = render (off2, sibilantVoice, 10), b = render (withClean, sibilantVoice, 10);
            expect (a == c, "CLEAN LAB off: two instances identical");
            double diff = 0.0;
            for (size_t i = 200000; i < a.size(); ++i)
                diff = juce::jmax (diff, std::abs ((double) a[i] - (double) b[i]));
            expect (diff > 0.005, "CLEAN LAB on changes the processed output (max difference " + juce::String (diff, 4) + ")");
            expectEquals (withClean.getLatencySamples(), off1.getLatencySamples());
            double peak = 0.0;
            for (float x : b)
                peak = juce::jmax (peak, (double) std::abs (x));
            expect (std::isfinite (peak) && peak < 1.2, "no clipping caused by the stage in the processor: peak " + juce::String (peak, 3));

            // no allocation while it runs (after a warm-up block)
            {
                GlitchKidProcessor q;
                th::setParam (q, "clean_on", 1.0f);
                th::setParam (q, "clean_amount", 100.0f);
                th::setParam (q, "air_on", 1.0f);
                q.prepareToPlay (fs, 480);
                juce::MidiBuffer midi;
                auto block = [&] (int pos)
                {
                    Buf bb (2, 480);
                    for (int i = 0; i < 480; ++i) { bb.setSample (0, i, resonantVoice[(size_t) (pos + i)]); bb.setSample (1, i, resonantVoice[(size_t) (pos + i)]); }
                    q.processBlock (bb, midi);
                };
                block (0);
                const long before = g_allocations.load();
                for (int pos = 480; pos < 480 * 400; pos += 480)
                    block (pos);
                expectEquals (g_allocations.load() - before, 0L);
            }

            // state round trip and an older state
            juce::MemoryBlock mb;
            withClean.getStateInformation (mb);
            GlitchKidProcessor restored;
            restored.setStateInformation (mb.getData(), (int) mb.getSize());
            expectEquals (th::getParam (restored, "clean_on"), 1.0f);
            expectEquals (th::getParam (restored, "clean_amount"), 100.0f);
            if (auto xml = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize()))
            {
                for (int i = xml->getNumChildElements() - 1; i >= 0; --i)
                    if (auto* child = xml->getChildElement (i); child != nullptr && child->getStringAttribute ("id").startsWith ("clean_"))
                        xml->removeChildElement (child, true);
                juce::MemoryBlock old;
                juce::AudioProcessor::copyXmlToBinary (*xml, old);
                GlitchKidProcessor loaded;
                loaded.setStateInformation (old.getData(), (int) old.getSize());
                expectEquals (th::getParam (loaded, "clean_on"), 0.0f);
                expectEquals (th::getParam (loaded, "clean_amount"), 50.0f);
            }

            // BUILD keeps the CLEAN LAB settings and, while it is on, leaves the low-end / low-mid / sibilance corrections to it
            namespace au = glitchkid::analysis;
            namespace am = glitchkid::automode;
            au::VocalAnalysis lead;
            lead.status = au::VocalAnalysis::Status::ready;
            lead.activeSeconds = 8.0f; lead.totalSeconds = 9.0f;
            lead.peakDb = -6.0f; lead.rmsDb = -20.0f; lead.crestDb = 14.0f;
            lead.dynP10Db = -32.0f; lead.dynP50Db = -22.0f; lead.dynP90Db = -16.0f; lead.dynamicRangeDb = 16.0f;
            lead.bandFraction = { 0.40f, 0.58f, 0.10f, 0.08f, 0.02f }; // low-end heavy and low-mid heavy
            lead.sibilanceIndex = 0.6f;                              // sibilant
            GlitchKidProcessor bp;
            auto rangeLookup = bp.makeRangeLookup();
            am::MixInput in;
            in.profile = 0;
            in.current = bp.captureSoundState();
            in.factoryDefault = bp.factoryDefaultState();
            in.lead = lead;
            const auto without = am::buildMix (in, rangeLookup);
            in.current.set ("clean_on", 1.0f);
            in.current.set ("clean_amount", 70.0f);
            const auto with = am::buildMix (in, rangeLookup);
            expect (without.usable && with.usable);
            auto reasonFor = [] (const am::MixProposal& pr, const char* id) -> juce::String
            {
                for (const auto& ch : pr.changes)
                    if (ch.id == id)
                        return ch.reason;
                return {};
            };
            expect (reasonFor (without, "eq_lo_gain").contains ("under 120 Hz") && reasonFor (without, "eq_b1_gain").contains ("low-mids") && reasonFor (without, "eq_hi_gain").contains ("S sounds"),
                    "without CLEAN LAB, BUILD applies its own corrections");
            expect (! reasonFor (with, "eq_lo_gain").contains ("under 120 Hz") && ! reasonFor (with, "eq_b1_gain").contains ("low-mids") && ! reasonFor (with, "eq_hi_gain").contains ("S sounds"),
                    "with CLEAN LAB on, BUILD leaves them to it");
            bool noted = false;
            for (const auto& e : with.evidence)
                noted = noted || e.contains ("CLEAN LAB is on");
            expect (noted, "and says so");
            expectEquals (with.sound.get ("clean_on", -1.0f), 1.0f);
            expectEquals (with.sound.get ("clean_amount", -1.0f), 70.0f);
        }
    }
};
CleanLabTests cleanLabTests;
}
