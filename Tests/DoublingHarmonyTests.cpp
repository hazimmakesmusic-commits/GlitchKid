#include "TestHelpers.h"
#include "../Source/DSP/VocalDoubling/VocalDoubler.h"
#include "../Source/DSP/Harmony/HarmonyVoice.h"
#include "../Source/AlterEgos.h"

// Stage 4.8: the Vocal Doubler and the Harmony voice, alone and inside the plug-in. Synthetic signals only: these tests prove the maths,
// the timing, the safety and the signal flow, not that the doubles or the harmony sound good on a real voice.
extern std::atomic<long> g_allocations; // counted in FormantShiftTests.cpp (it replaces the global operator new)

namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr float qNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float posInf = std::numeric_limits<float>::infinity();
using glitchkid::harmony::HarmonyVoice;

DoublerSettings dbl (float amount = 50.0f, float width = 70.0f, float detune = 8.0f, float timing = 18.0f, bool on = true)
{
    DoublerSettings s;
    s.on = on; s.amountPct = amount; s.widthPct = width; s.detuneCents = detune; s.timingMs = timing;
    return s;
}

VocalDoubler makeDoubler (double sr, const DoublerSettings& s)
{
    VocalDoubler d;
    d.setSettings (s);
    d.prepare (sr);
    return d;
}

template <typename F>
Buf stream (VocalDoubler& d, Buf in, int blockSize, F before)
{
    for (int start = 0; start < in.getNumSamples(); start += blockSize)
    {
        before (start, d);
        Buf view (in.getArrayOfWritePointers(), in.getNumChannels(), start, juce::jmin (blockSize, in.getNumSamples() - start));
        d.process (view);
    }
    return in;
}
Buf stream (VocalDoubler& d, Buf in, int blockSize = 512) { return stream (d, std::move (in), blockSize, [] (int, VocalDoubler&) {}); }

Buf silent (int ch, int n) { Buf b (ch, n); b.clear(); return b; }

Buf sine (int channels, int n, double sr, double freq, float amp = 0.5f)
{
    Buf b (channels, n);
    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < n; ++i)
            b.setSample (ch, i, (float) (amp * std::sin (2.0 * pi * freq * (double) i / sr)));
    return b;
}

// The same noise on every channel (a "mono vocal" in a stereo track), low-passed a little so it is speech-like.
Buf monoNoise (int channels, int n, juce::int64 seed = 1, float amp = 0.3f)
{
    juce::Random rng (seed);
    Buf b (channels, n);
    double lp = 0.0;
    for (int i = 0; i < n; ++i)
    {
        lp += 0.35 * ((rng.nextDouble() * 2.0 - 1.0) - lp);
        for (int ch = 0; ch < channels; ++ch)
            b.setSample (ch, i, (float) (lp * amp * 3.0));
    }
    return b;
}

// A steady vowel-like voice: 12 partials of f0 with 1/h amplitudes, phase accumulated in double.
Buf voice (int channels, int n, double sr, double f0, float peak = 0.4f)
{
    Buf b (channels, n);
    double phase = 0.0;
    for (int i = 0; i < n; ++i)
    {
        phase += f0 / sr;
        phase -= std::floor (phase);
        double s = 0.0, norm = 0.0;
        for (int h = 1; h * f0 < 0.45 * sr && h <= 12; ++h)
        {
            s += std::sin (2.0 * pi * h * phase) / h;
            norm += 1.0 / h;
        }
        for (int ch = 0; ch < channels; ++ch)
            b.setSample (ch, i, (float) (peak * s / norm * 1.6));
    }
    return b;
}

bool allFinite (const Buf& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (ch, i)))
                return false;
    return true;
}

bool exactlyZero (const Buf& b, int from, int to)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = from; i < to; ++i)
            if (! juce::exactlyEqual (b.getSample (ch, i), 0.0f))
                return false;
    return true;
}

float maxStep (const Buf& b, int from = 1)
{
    float m = 0.0f;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = from; i < b.getNumSamples(); ++i)
            m = juce::jmax (m, std::abs (b.getSample (ch, i) - b.getSample (ch, i - 1)));
    return m;
}

double rms (const Buf& b, int ch, int from, int count) { return (double) b.getRMSLevel (ch, from, count); }
double db (double a, double b) { return 20.0 * std::log10 ((a + 1.0e-12) / (b + 1.0e-12)); }

Buf monoSum (const Buf& b)
{
    Buf m (1, b.getNumSamples());
    for (int i = 0; i < b.getNumSamples(); ++i)
        m.setSample (0, i, 0.5f * (b.getSample (0, i) + b.getSample (1, i)));
    return m;
}

// Normalised autocorrelation with a parabolic peak: an estimate of a steady tone's frequency that does not use the plug-in's own detector.
double autocorrFreq (const float* x, int n, double sr, double nearHz)
{
    const int lo = (int) (sr / (nearHz * 1.35)), hi = (int) (sr / (nearHz / 1.35));
    const int w = n - hi - 1;
    std::vector<double> r ((size_t) hi + 2, 0.0);
    double r0 = 0.0;
    for (int i = 0; i < w; ++i) r0 += (double) x[i] * x[i];
    for (int lag = lo - 1; lag <= hi + 1; ++lag)
    {
        double acc = 0.0, e = 0.0;
        for (int i = 0; i < w; ++i) { acc += (double) x[i] * x[i + lag]; e += (double) x[i + lag] * x[i + lag]; }
        r[(size_t) lag] = acc / std::sqrt (r0 * e + 1.0e-30);
    }
    int best = lo;
    for (int lag = lo; lag <= hi; ++lag)
        if (r[(size_t) lag] > r[(size_t) best]) best = lag;
    const double a = r[(size_t) best - 1], b = r[(size_t) best], c = r[(size_t) best + 1];
    const double denom = a - 2.0 * b + c;
    const double shift = std::abs (denom) > 1.0e-12 ? 0.5 * (a - c) / denom : 0.0;
    return sr / ((double) best + shift);
}

double cents (double hz, double ref) { return 1200.0 * std::log2 (hz / ref); }

HarmonyVoice makeHarmony (double sr, float semitones, float levelDb)
{
    HarmonyVoice h;
    h.setSettings ({ semitones, levelDb });
    h.prepare (sr);
    return h;
}

Buf runHarmony (HarmonyVoice& h, Buf in, int blockSize)
{
    for (int start = 0; start < in.getNumSamples(); start += blockSize)
    {
        const int n = juce::jmin (blockSize, in.getNumSamples() - start);
        float* ptrs[2] = { in.getWritePointer (0) + start, in.getNumChannels() > 1 ? in.getWritePointer (1) + start : nullptr };
        h.process (ptrs, in.getNumChannels(), n);
    }
    return in;
}

// The harmony voice alone: (output) - (the lead delayed by the latency), measured with the harmony at 0 dB.
Buf harmonyOnly (HarmonyVoice& h, const Buf& in, int blockSize = 512)
{
    Buf out = runHarmony (h, in, blockSize);
    const int L = h.getLatencySamples();
    for (int ch = 0; ch < out.getNumChannels(); ++ch)
        for (int i = 0; i < out.getNumSamples(); ++i)
            out.setSample (ch, i, out.getSample (ch, i) - (i >= L ? in.getSample (ch, i - L) : 0.0f));
    return out;
}
} // namespace

struct DoublingHarmonyTests : public juce::UnitTest
{
    DoublingHarmonyTests() : juce::UnitTest ("Doubling and Harmony", "GlitchKid") {}

    void runTest() override
    {
        // ------------------------------------------------------------------------------------------------ the Vocal Doubler
        beginTest ("Doubler: off, and amount 0, are bit-exact copies; the stage reports idle when off; silence in gives silence out");
        for (int ch : { 1, 2 })
        {
            Buf in = monoNoise (ch, 20000, 3);
            auto off = makeDoubler (48000.0, dbl (100.0f, 100.0f, 25.0f, 45.0f, false));
            Buf o = stream (off, in);
            expectEquals (maxAbsDiff (o, in), 0.0f);
            expect (! off.isActive());
            auto zero = makeDoubler (48000.0, dbl (0.0f));
            expectEquals (maxAbsDiff (stream (zero, in), in), 0.0f);
            auto s = makeDoubler (48000.0, dbl (100.0f, 50.0f, 25.0f, 45.0f));
            Buf q = stream (s, silent (ch, 48000));
            expect (allFinite (q) && exactlyZero (q, 0, q.getNumSamples()), "silence stays silence, channels " + juce::String (ch));
        }

        beginTest ("Doubler: timing - the doubles sit behind the lead by 0.85 x and 1.25 x TIMING (5, 18 and 45 ms; 44.1-96 kHz); nothing arrives early");
        for (double sr : { 44100.0, 48000.0, 96000.0 })
            for (float timingMs : { 5.0f, 18.0f, 45.0f })
            {
                // Detune 0: the delays are exactly the timing offsets.
                auto d = makeDoubler (sr, dbl (100.0f, 0.0f, 0.0f, timingMs));
                Buf imp = silent (1, (int) (0.3 * sr));
                const int at = 2000;
                imp.setSample (0, at, 1.0f);
                Buf o = stream (d, imp, 256);
                const double expected0 = timingMs * 0.001 * sr * 0.85, expected1 = timingMs * 0.001 * sr * 1.25;
                expectWithinAbsoluteError (d.getVoiceDelaySamples (0), expected0, 0.01, "voice 1 delay at " + juce::String (sr) + " Hz, " + juce::String (timingMs) + " ms");
                expectWithinAbsoluteError (d.getVoiceDelaySamples (1), expected1, 0.01, "voice 2 delay");
                // the dry impulse is untouched and there is no energy before the first double
                double early = 0.0, late = 0.0;
                for (int i = 0; i < o.getNumSamples(); ++i)
                {
                    const double v = (double) o.getSample (0, i) - (i == at ? 1.0 : 0.0);
                    if (i < at + (int) expected0 - 2) early += v * v; else late += v * v;
                }
                expect (early < 1.0e-12, "nothing before the first double: " + juce::String (early));
                expect (late > 0.01, "the doubles arrive: " + juce::String (late));
            }

        beginTest ("Doubler: DETUNE - the largest pitch wobble equals the setting (25 cents at most), is 0 at 0, and really moves; the mean pitch does not shift");
        {
            const double sr = 48000.0;
            for (float cents : { 0.0f, 3.0f, 8.0f, 25.0f })
            {
                auto d = makeDoubler (sr, dbl (100.0f, 50.0f, cents, 20.0f));
                Buf in = silent (1, 1);
                double prev0 = -1.0, prev1 = -1.0, maxDev0 = 0.0, maxDev1 = 0.0, sumDev0 = 0.0, lo0 = 1.0e9, hi0 = -1.0e9;
                const int total = (int) (20.0 * sr);
                for (int i = 0; i < total; ++i)
                {
                    in.setSample (0, 0, 0.0f);
                    d.process (in);
                    const double d0 = d.getVoiceDelaySamples (0), d1 = d.getVoiceDelaySamples (1);
                    if (prev0 >= 0.0)
                    {
                        maxDev0 = juce::jmax (maxDev0, std::abs (d0 - prev0)); // delay change per sample = pitch deviation (ratio - 1)
                        maxDev1 = juce::jmax (maxDev1, std::abs (d1 - prev1));
                        sumDev0 += d0 - prev0;
                    }
                    lo0 = juce::jmin (lo0, d0); hi0 = juce::jmax (hi0, d0);
                    prev0 = d0; prev1 = d1;
                }
                const double limit = std::pow (2.0, (double) cents / 1200.0) - 1.0;
                if (cents < 0.01f)
                {
                    expect (maxDev0 < 1.0e-9 && maxDev1 < 1.0e-9, "no wobble at 0 cents");
                    continue;
                }
                logMessage ("detune " + juce::String (cents) + " cents: largest measured wobble " + juce::String (1200.0 * std::log2 (1.0 + juce::jmax (maxDev0, maxDev1)), 2) + " cents, delay swing "
                            + juce::String ((hi0 - lo0) / sr * 1000.0, 2) + " ms");
                expect (maxDev0 <= limit * 1.03 && maxDev1 <= limit * 1.03, "wobble within the setting: " + juce::String (maxDev0 / limit) + " of it");
                expect (juce::jmax (maxDev0, maxDev1) >= limit * 0.4, "the wobble really reaches a good part of the setting: " + juce::String (juce::jmax (maxDev0, maxDev1) / limit));
                expect (std::abs (sumDev0) / (double) total < limit * 0.02, "the average pitch is not shifted");
            }
        }

        beginTest ("Doubler: the doubled signal differs from the source in the intended way (delayed, correlated copies at the timing offsets), not just louder");
        {
            const double sr = 48000.0;
            Buf in = monoNoise (1, 96000, 5);
            auto d = makeDoubler (sr, dbl (100.0f, 0.0f, 0.0f, 20.0f));
            Buf out = stream (d, in);
            Buf wet (1, in.getNumSamples());
            for (int i = 0; i < in.getNumSamples(); ++i) wet.setSample (0, i, out.getSample (0, i) - in.getSample (0, i));
            // cross-correlation of the wet part with the source: peaks at the two timing offsets
            auto xc = [&] (int lag)
            {
                double acc = 0.0;
                for (int i = 30000; i < 90000; ++i) acc += (double) wet.getSample (0, i) * in.getSample (0, i - lag);
                return acc;
            };
            const int lag0 = (int) std::lround (0.020 * sr * 0.85), lag1 = (int) std::lround (0.020 * sr * 1.25);
            double best = 0.0; int bestLag = 0;
            for (int lag = 100; lag < 3000; ++lag)
                if (std::abs (xc (lag)) > best) { best = std::abs (xc (lag)); bestLag = lag; }
            expect (std::abs (bestLag - lag0) <= 2 || std::abs (bestLag - lag1) <= 2, "strongest copy at lag " + juce::String (bestLag) + " (expected " + juce::String (lag0) + " or " + juce::String (lag1) + ")");
            expect (xc (lag0) > 0.0 && xc (lag1) > 0.0, "both doubles are positively correlated with the source");
            expect (std::abs (xc (lag0 + 600)) < 0.3 * std::abs (xc (lag0)), "and not at unrelated lags");
        }

        beginTest ("Doubler: WIDTH - at 100 % the mono sum equals the dry input exactly (mono-safe); at 0 % the doubles are in the centre; levels stay controlled");
        {
            const double sr = 48000.0;
            Buf in = monoNoise (2, 96000, 7);
            auto wide = makeDoubler (sr, dbl (100.0f, 100.0f, 8.0f, 20.0f));
            Buf w = stream (wide, in);
            Buf wm = monoSum (w), im = monoSum (in);
            expect (maxAbsDiff (wm, im) < 2.0e-6f, "mono sum of the widest double equals the input, worst " + juce::String (maxAbsDiff (wm, im)));
            expect (maxAbsDiff (w, in) > 0.02f, "...although each channel is clearly different");

            auto centre = makeDoubler (sr, dbl (100.0f, 0.0f, 8.0f, 20.0f));
            Buf c = stream (centre, in);
            expectEquals (maxAbsDiff (Buf (c.getArrayOfWritePointers(), 1, 0, c.getNumSamples()), Buf (c.getArrayOfWritePointers() + 1, 1, 0, c.getNumSamples())), 0.0f, "centre doubles keep L = R");

            double worstUp = -100.0, worstDown = 100.0, defaultUp = 0.0;
            for (float width : { 0.0f, 35.0f, 70.0f, 100.0f })
                for (float amount : { 25.0f, 50.0f, 100.0f })
                {
                    auto m = makeDoubler (sr, dbl (amount, width, 8.0f, 20.0f));
                    Buf o = stream (m, in);
                    const double change = db (rms (o, 0, 20000, 60000), rms (in, 0, 20000, 60000));
                    worstUp = juce::jmax (worstUp, change);
                    worstDown = juce::jmin (worstDown, change);
                    if (juce::approximatelyEqual (amount, 50.0f) && juce::approximatelyEqual (width, 70.0f)) defaultUp = change;
                }
            logMessage ("level change from the doubles on speech-like noise (25..100 % amount, 0..100 % width): " + juce::String (worstDown, 2) + " .. +" + juce::String (worstUp, 2) + " dB; at the defaults +" + juce::String (defaultUp, 2) + " dB");
            expect (worstUp < 3.1 && worstDown > -0.1, "never more than about 3 dB louder, never quieter");
            expect (defaultUp < 1.5, "the default thickens without a big level jump: +" + juce::String (defaultUp, 2) + " dB");

            // mono compatibility of the less-wide settings: the mono sum is coloured by a bounded amount, not cancelled
            auto mid = makeDoubler (sr, dbl (100.0f, 0.0f, 8.0f, 20.0f));
            Buf md = stream (mid, in);
            const double monoChange = db (rms (md, 0, 20000, 60000), rms (in, 0, 20000, 60000));
            expect (monoChange > -1.0 && monoChange < 6.0, "centre-only doubles in mono: " + juce::String (monoChange, 2) + " dB");
        }

        beginTest ("Doubler: the double is softer than the lead (one-pole low-pass at 8 kHz) and its low end is kept");
        {
            const double sr = 48000.0;
            // a single double (width 0, one tone): high tones come back quieter than low tones
            Buf lo = sine (1, 96000, sr, 400.0, 0.4f), hi = sine (1, 96000, sr, 14000.0, 0.4f);
            auto a = makeDoubler (sr, dbl (100.0f, 0.0f, 0.0f, 20.0f)), b = makeDoubler (sr, dbl (100.0f, 0.0f, 0.0f, 20.0f));
            Buf ol = stream (a, lo), oh = stream (b, hi);
            // The wet part of a steady tone, at 0 detune and 0 width, is the sum of two delayed, filtered copies; compare its level relative to the tone.
            auto wetRms = [&] (const Buf& o, const Buf& in) { Buf w (1, o.getNumSamples()); for (int i = 0; i < o.getNumSamples(); ++i) w.setSample (0, i, o.getSample (0, i) - in.getSample (0, i)); return rms (w, 0, 30000, 60000); };
            const double lowWet = wetRms (ol, lo) / rms (lo, 0, 30000, 60000), highWet = wetRms (oh, hi) / rms (hi, 0, 30000, 60000);
            expect (highWet < lowWet * 0.75 + 1.0e-3 || highWet < 0.9 * lowWet, "the 14 kHz double is quieter than the 400 Hz double: " + juce::String (highWet) + " vs " + juce::String (lowWet));
        }

        beginTest ("Doubler: mono and stereo - identical channels stay identical at any width when the width is 0; mono layout works; 44.1/48/88.2/96 kHz");
        for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0 })
        {
            auto s = makeDoubler (sr, dbl (80.0f, 60.0f, 12.0f, 25.0f));
            Buf st = stream (s, monoNoise (2, (int) sr, 9));
            expect (allFinite (st) && maxAbsDiff (st, monoNoise (2, (int) sr, 9)) > 0.01f, "stereo works at " + juce::String (sr));
            auto m = makeDoubler (sr, dbl (80.0f, 60.0f, 12.0f, 25.0f));
            Buf mo = stream (m, monoNoise (1, (int) sr, 9));
            expect (allFinite (mo) && maxAbsDiff (mo, monoNoise (1, (int) sr, 9)) > 0.01f, "mono works at " + juce::String (sr));
        }

        beginTest ("Doubler: bit-identical output for 1, 7, 64, 333 and 2048-sample blocks (mono and stereo)");
        for (int ch : { 1, 2 })
        {
            Buf in = monoNoise (ch, 30000, 11);
            auto ref = makeDoubler (48000.0, dbl (65.0f, 40.0f, 17.0f, 27.0f));
            Buf expected = stream (ref, in, 512);
            for (int bs : { 1, 7, 64, 333, 2048 })
            {
                auto d = makeDoubler (48000.0, dbl (65.0f, 40.0f, 17.0f, 27.0f));
                Buf got = stream (d, in, bs);
                expectEquals (maxAbsDiff (got, expected), 0.0f, "block size " + juce::String (bs) + ", channels " + juce::String (ch));
            }
        }

        beginTest ("Doubler: parameter changes during playback and on/off switching are click-free; off is exact; no stale audio; no runaway at the extremes");
        {
            const double sr = 48000.0;
            auto d = makeDoubler (sr, dbl());
            juce::Random rng (21);
            Buf in = sine (2, 48000 * 4, sr, 220.0, 0.5f);
            Buf o = stream (d, in, 256, [&] (int, VocalDoubler& x)
            {
                x.setSettings (dbl (100.0f * rng.nextFloat(), 100.0f * rng.nextFloat(), 25.0f * rng.nextFloat(), 5.0f + 40.0f * rng.nextFloat()));
            });
            expect (allFinite (o));
            expect (maxStep (o, 100) < 0.09f, "largest step under random automation " + juce::String (maxStep (o, 100)) + " (dry sine alone: 0.0144)");

            auto startOff = dbl (60.0f, 70.0f, 10.0f, 22.0f, false);
            auto g = makeDoubler (sr, startOff);
            Buf toggled = stream (g, sine (2, 48000 * 4, sr, 220.0, 0.5f), 480, [&] (int start, VocalDoubler& x)
            {
                auto t = dbl (60.0f, 70.0f, 10.0f, 22.0f);
                t.on = (start / 48000) % 2 == 1; // off, on, off, on
                x.setSettings (t);
            });
            expect (maxStep (toggled, 1) < 0.09f, "on/off largest step " + juce::String (maxStep (toggled)));
            Buf firstSecond (2, 48000), dryFirst = sine (2, 48000, sr, 220.0, 0.5f);
            for (int ch = 0; ch < 2; ++ch) firstSecond.copyFrom (ch, 0, toggled, ch, 0, 48000);
            expectEquals (maxAbsDiff (firstSecond, dryFirst), 0.0f);

            auto h = makeDoubler (sr, dbl());
            stream (h, sine (2, 20000, sr, 220.0, 0.5f));
            h.setSettings (dbl (50.0f, 70.0f, 8.0f, 18.0f, false));
            stream (h, silent (2, 8000));
            expect (! h.isActive(), "idle after the fade");
            h.setSettings (dbl());
            expect (exactlyZero (stream (h, silent (2, 20000)), 0, 20000), "no stale audio after re-enable");

            // extremes for a long time: bounded
            auto e = makeDoubler (sr, dbl (100.0f, 50.0f, 25.0f, 45.0f));
            Buf loud = monoNoise (2, 48000 * 20, 3, 0.5f);
            float inPeak = loud.getMagnitude (0, loud.getNumSamples());
            Buf eo = stream (e, loud);
            expect (allFinite (eo) && eo.getMagnitude (0, eo.getNumSamples()) < 3.0f * inPeak, "no runaway: peak " + juce::String (eo.getMagnitude (0, eo.getNumSamples())) + " for an input peak of " + juce::String (inPeak));
        }

        beginTest ("Doubler: NaN and +-Inf are treated as 0 (bit-exact with the same signal where they are zero) and do not contaminate later audio");
        {
            Buf clean = voice (2, 40000, 48000.0, 190.0, 0.4f);
            Buf dirty = clean, zeroed = clean;
            const int at[] = { 100, 101, 5000, 5001, 5002, 12000, 20000, 20001, 33333 };
            for (int k = 0; k < 9; ++k)
                for (int ch = 0; ch < 2; ++ch)
                {
                    dirty.setSample (ch, at[k], k % 3 == 0 ? qNaN : (k % 3 == 1 ? posInf : -posInf));
                    zeroed.setSample (ch, at[k], 0.0f);
                }
            auto a = makeDoubler (48000.0, dbl (80.0f, 60.0f, 15.0f, 30.0f)), b = makeDoubler (48000.0, dbl (80.0f, 60.0f, 15.0f, 30.0f));
            Buf oa = stream (a, dirty, 333), ob = stream (b, zeroed, 333);
            expect (allFinite (oa));
            expectEquals (maxAbsDiff (oa, ob), 0.0f);
            auto h = makeDoubler (48000.0, dbl (100.0f, 50.0f, 25.0f, 45.0f));
            Buf burst (2, 3000);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 3000; ++i)
                    burst.setSample (ch, i, (i & 1) ? -3.0e38f : 3.0e38f);
            Buf bo = stream (h, burst);
            // (the dry channel passes huge finite values through unchanged, as everywhere in the plug-in; the doubles must not turn them into Inf)
            bool finite = true;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 3000; ++i)
                    finite &= std::isfinite (bo.getSample (ch, i)) || std::abs (burst.getSample (ch, i)) > 1.0e30f;
            expect (finite, "no new Inf from an overflow burst");
        }

        // ------------------------------------------------------------------------------------------------ the Harmony voice
        beginTest ("Harmony: every interval sings the lead's melody at the right pitch (all 9 intervals, steady voices at 130, 220 and 330 Hz)");
        {
            const double sr = 48000.0;
            const int n = (int) (2.0 * sr);
            double worstCents = 0.0;
            for (int idx = 0; idx < glitchkid::harmony::numIntervals; ++idx)
                for (double f0 : { 130.0, 220.0, 330.0 })
                {
                    const float st = glitchkid::harmony::intervalInfo (idx).semitones;
                    const double expectedHz = f0 * std::pow (2.0, (double) st / 12.0);
                    if (expectedHz < 70.0 || expectedHz > 700.0) continue;
                    auto h = makeHarmony (sr, st, 0.0f);
                    Buf in = voice (1, n, sr, f0);
                    Buf v = harmonyOnly (h, in);
                    const double hz = autocorrFreq (v.getReadPointer (0) + (int) (1.0 * sr), (int) (0.5 * sr), sr, expectedHz);
                    const double err = std::abs (cents (hz, expectedHz));
                    worstCents = juce::jmax (worstCents, err);
                    expect (err < 25.0, juce::String (glitchkid::harmony::intervalInfo (idx).name) + " on " + juce::String (f0) + " Hz: measured " + juce::String (hz, 1) + " Hz, expected " + juce::String (expectedHz, 1) + " (" + juce::String (err, 1) + " cents off)");
                }
            logMessage ("worst interval error over all intervals and pitches: " + juce::String (worstCents, 1) + " cents (a steady synthetic voice)");
            // direction is unambiguous: UP is higher, DOWN is lower
            for (int idx = 0; idx < glitchkid::harmony::numIntervals; ++idx)
            {
                const juce::String label (glitchkid::harmony::intervalInfo (idx).name);
                expect (label.contains (glitchkid::harmony::intervalInfo (idx).semitones > 0 ? "UP" : "DOWN"), "name states the direction: " + label);
            }
        }

        beginTest ("Harmony: the lead is delayed by exactly the reported latency and kept at unity; the harmony level is the setting; time-aligned with the lead");
        for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0 })
        {
            const int n = (int) (1.5 * sr);
            auto h = makeHarmony (sr, 7.0f, -60.0f); // the voice at -60 dB: what remains is the delayed lead
            expectEquals (h.getLatencySamples(), HarmonyVoice::latencyFor (sr));
            expectEquals (h.getLatencySamples(), glitchkid::pitch::PitchCorrector::latencyFor (sr), "the same delay as Pitch Tune");
            Buf in = voice (2, n, sr, 200.0);
            Buf out = runHarmony (h, in, 480);
            const int L = h.getLatencySamples();
            float worst = 0.0f;
            for (int i = L; i < n; ++i) worst = juce::jmax (worst, std::abs (out.getSample (0, i) - in.getSample (0, i - L)));
            expect (worst < 0.02f, "the lead is the input delayed by " + juce::String (L) + " samples (the -60 dB voice leaks at most " + juce::String (worst) + ")");

            // level: at 0 dB the voice is about as loud as the lead, at -12 dB about a quarter
            auto h0 = makeHarmony (sr, 7.0f, 0.0f), h12 = makeHarmony (sr, 7.0f, -12.0f);
            Buf v0 = harmonyOnly (h0, in), v12 = harmonyOnly (h12, in);
            const double ref = rms (in, 0, (int) sr, (int) (0.5 * sr));
            const double l0 = db (rms (v0, 0, (int) sr, (int) (0.5 * sr)), ref), l12 = db (rms (v12, 0, (int) sr, (int) (0.5 * sr)), ref);
            // The setting is exact (12 dB apart). The absolute level is below the setting because the shifter keeps the voice's formant envelope: moving
            // the harmonics of this 1/h test voice up puts them lower on the envelope (here about -5 dB for a fifth up); real voices differ.
            expect (std::abs ((l0 - l12) - 12.0) < 0.7, "the level control is exact: " + juce::String (l0 - l12, 2) + " dB between the 0 and -12 dB settings");
            expect (l0 < 1.0 && l0 > -9.0, "the voice at 0 dB is about as loud as the lead (" + juce::String (l0, 1) + " dB re the lead)");
        }

        beginTest ("Harmony: the voice is gated off while the singer is not voiced (noise, silence): nothing buzzes under consonants");
        {
            const double sr = 48000.0;
            auto h = makeHarmony (sr, 7.0f, 0.0f);
            Buf noise = monoNoise (1, (int) (2.0 * sr), 4, 0.3f);
            Buf v = harmonyOnly (h, noise);
            const double rel = db (rms (v, 0, (int) sr, (int) sr), rms (noise, 0, (int) sr, (int) sr));
            logMessage ("harmony voice level on unvoiced noise: " + juce::String (rel, 1) + " dB re the noise");
            expect (rel < -20.0, "unvoiced input produces (almost) no harmony: " + juce::String (rel, 1) + " dB");

            auto h2 = makeHarmony (sr, -12.0f, 0.0f);
            Buf sil = silent (1, 48000);
            Buf so = runHarmony (h2, sil, 512);
            expect (allFinite (so) && exactlyZero (so, 0, so.getNumSamples()), "silence in gives silence out");
        }

        beginTest ("Harmony: no feedback - after the lead stops, the harmony dies out within the latency plus the hold time; the output stays bounded");
        {
            const double sr = 48000.0;
            auto h = makeHarmony (sr, 12.0f, 0.0f);
            Buf in = silent (1, (int) (3.0 * sr));
            Buf v = voice (1, (int) (1.5 * sr), sr, 220.0);
            in.copyFrom (0, 0, v, 0, 0, v.getNumSamples());
            float peak = 0.0f;
            Buf out = runHarmony (h, in, 480);
            for (int i = 0; i < out.getNumSamples(); ++i) peak = juce::jmax (peak, std::abs (out.getSample (0, i)));
            expect (allFinite (out) && peak < 2.0f * v.getMagnitude (0, v.getNumSamples()) + 0.1f, "bounded: peak " + juce::String (peak));
            const int stop = (int) (1.5 * sr) + h.getLatencySamples() + (int) (0.5 * sr); // far longer than latency + hold + gate release
            expect (exactlyZero (out, stop, out.getNumSamples()), "nothing remains after the lead has stopped (the voice never feeds itself)");
        }

        beginTest ("Harmony: note transitions - a jump from 200 to 300 Hz gives finite output with no extra large clicks");
        {
            const double sr = 48000.0;
            Buf a = voice (1, (int) sr, sr, 200.0), b = voice (1, (int) sr, sr, 300.0);
            Buf in (1, (int) (2.0 * sr));
            in.copyFrom (0, 0, a, 0, 0, (int) sr);
            in.copyFrom (0, (int) sr, b, 0, 0, (int) sr);
            auto h = makeHarmony (sr, 4.0f, -6.0f);
            Buf out = runHarmony (h, in, 480);
            expect (allFinite (out));
            const float inStep = maxStep (in), outStep = maxStep (out, 1);
            expect (outStep < 3.0f * inStep + 0.05f, "largest step " + juce::String (outStep) + " versus the input's own " + juce::String (inStep));
        }

        beginTest ("Harmony: bit-identical output for 1, 7, 64, 333 and 2048-sample blocks (mono and stereo); mono and stereo both work");
        for (int ch : { 1, 2 })
        {
            const double sr = 48000.0;
            Buf in = voice (ch, (int) (1.2 * sr), sr, 180.0);
            auto ref = makeHarmony (sr, -5.0f, -7.0f);
            Buf expected = runHarmony (ref, in, 512);
            expect (maxAbsDiff (expected, in) > 0.01f, "the harmony adds something");
            for (int bs : { 1, 7, 64, 333, 2048 })
            {
                auto h = makeHarmony (sr, -5.0f, -7.0f);
                expectEquals (maxAbsDiff (runHarmony (h, in, bs), expected), 0.0f, "block size " + juce::String (bs) + ", channels " + juce::String (ch));
            }
        }

        beginTest ("Harmony: NaN, +-Inf and overflow input are treated as silence (bit-exact with zeros) and never contaminate later audio; feed() then process() has no gap");
        {
            const double sr = 48000.0;
            Buf clean = voice (2, (int) (1.5 * sr), sr, 210.0);
            Buf dirty = clean, zeroed = clean;
            const int at[] = { 2000, 2001, 9000, 15000, 15001, 30000, 44000 };
            for (int k = 0; k < 7; ++k)
                for (int ch = 0; ch < 2; ++ch)
                {
                    dirty.setSample (ch, at[k], k % 3 == 0 ? qNaN : (k % 3 == 1 ? posInf : -posInf));
                    zeroed.setSample (ch, at[k], 0.0f);
                }
            auto a = makeHarmony (sr, 3.0f, -6.0f), b = makeHarmony (sr, 3.0f, -6.0f);
            Buf oa = runHarmony (a, dirty, 333), ob = runHarmony (b, zeroed, 333);
            expect (allFinite (oa));
            expectEquals (maxAbsDiff (oa, ob), 0.0f);

            // idle listening: feeding first, then processing, works straight away
            auto f = makeHarmony (sr, 7.0f, 0.0f);
            Buf v = voice (1, (int) (1.5 * sr), sr, 200.0);
            for (int start = 0; start < 24000; start += 480)
            {
                const float* p[1] = { v.getReadPointer (0) + start };
                f.feed (p, 1, 480);
            }
            Buf rest (1, 24000);
            rest.copyFrom (0, 0, v, 0, 24000, 24000);
            Buf restCopy; restCopy.makeCopyOf (rest);
            Buf fo = runHarmony (f, rest, 480);
            expect (allFinite (fo));
            // the lead is there from the first sample (history was recorded while idle)
            float lead = 0.0f;
            for (int i = 0; i < 400; ++i) lead = juce::jmax (lead, std::abs (fo.getSample (0, i) - v.getSample (0, 24000 - f.getLatencySamples() + i)));
            expect (lead < 0.4f * v.getMagnitude (0, v.getNumSamples()) + 0.01f, "the lead is continuous across idle -> process: " + juce::String (lead));
        }

        beginTest ("Pitch Tune is unchanged by the new fixed-interval option (default off: same output as before)");
        {
            const double sr = 48000.0;
            glitchkid::pitch::PitchCorrector a, b;
            a.prepare (sr); b.prepare (sr);
            glitchkid::pitch::PitchCorrector::Settings s;
            s.key = 9; s.scale = glitchkid::pitch::ScaleQuantizer::Scale::chromatic; s.strength = 0.8f; s.retuneSpeedMs = 25.0f;
            a.setSettings (s);
            auto s2 = s; s2.fixedShift = false; s2.fixedShiftSemitones = 5.0f; // the semitone value is ignored while the mode is off
            b.setSettings (s2);
            Buf in = voice (1, 48000, sr, 187.0), in2 = in;
            float* pa[1] = { in.getWritePointer (0) };
            float* pb[1] = { in2.getWritePointer (0) };
            a.process (pa, 1, 48000);
            b.process (pb, 1, 48000);
            expectEquals (maxAbsDiff (in, in2), 0.0f);
        }

        // ------------------------------------------------------------------------------------------------ inside the plug-in
        beginTest ("Plug-in: new parameters exist with the right defaults; both effects are off by default (bit-exact, 6 samples); each is automatable and clamped");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 512);
            expectEquals (getParam (p, "dbl_on"), 0.0f);
            expectWithinAbsoluteError (getParam (p, "dbl_amount"), 50.0f, 0.01f);
            expectWithinAbsoluteError (getParam (p, "dbl_width"), 70.0f, 0.01f);
            expectWithinAbsoluteError (getParam (p, "dbl_detune"), 8.0f, 0.01f);
            expectWithinAbsoluteError (getParam (p, "dbl_timing"), 18.0f, 0.01f);
            expectEquals (getParam (p, "harm_on"), 0.0f);
            expectEquals (getParam (p, "harm_interval"), (float) glitchkid::harmony::defaultInterval);
            expectWithinAbsoluteError (getParam (p, "harm_level"), -9.0f, 0.01f);
            expectEquals (p.getLatencySamples(), Saturation::latencySamples);
            Buf in, out;
            run (p, 2, 512, 20, in, out);
            expectEquals (maxAbsDiff (in, out), 0.0f);
            setParam (p, "dbl_timing", 200.0f); expectEquals (getParam (p, "dbl_timing"), VocalDoubler::maxTimingMs);
            setParam (p, "dbl_detune", -3.0f); expectEquals (getParam (p, "dbl_detune"), 0.0f);
            setParam (p, "harm_level", 9.0f); expectEquals (getParam (p, "harm_level"), 0.0f);
            setParam (p, "harm_interval", 99.0f); expectEquals (getParam (p, "harm_interval"), (float) (glitchkid::harmony::numIntervals - 1));
        }

        beginTest ("Plug-in latency: Doubler adds none; Harmony adds exactly the Pitch Tune delay while it is on (6 + L), also together with Pitch Tune and FORMANT");
        for (double sr : { 44100.0, 48000.0, 96000.0 })
        {
            GlitchKidProcessor p;
            p.prepareToPlay (sr, 512);
            const int base = p.getLatencySamples();
            setParam (p, "dbl_on", 1.0f);
            p.updateReportedLatency();
            expectEquals (p.getLatencySamples(), base, "the doubler reports no latency");
            setParam (p, "harm_on", 1.0f);
            p.updateReportedLatency();
            const int L = glitchkid::pitch::PitchCorrector::latencyFor (sr);
            expectEquals (p.getLatencySamples(), base + L, "harmony: 6 + " + juce::String (L) + " at " + juce::String (sr));
            setParam (p, "tune_on", 1.0f);
            p.updateReportedLatency();
            expectEquals (p.getLatencySamples(), base + 2 * L, "with Pitch Tune as well");
            setParam (p, "harm_on", 0.0f); setParam (p, "tune_on", 0.0f);
            p.updateReportedLatency();
            expectEquals (p.getLatencySamples(), base);
        }

        beginTest ("Plug-in: Mix 0 % and Bypass give the exact input delayed by the TOTAL latency with Harmony and the Doubler on (dry path aligned); the Doubler alone needs no delay");
        for (int channels : { 1, 2 })
        {
            auto check = [&] (const char* id, float value, bool harmonyOn, int expectedExtra)
            {
                GlitchKidProcessor p;
                if (channels == 1) setLayout (p, juce::AudioChannelSet::mono());
                p.prepareToPlay (48000.0, 480);
                setParam (p, "dbl_on", 1.0f);
                if (harmonyOn) setParam (p, "harm_on", 1.0f);
                setParam (p, id, value);
                p.updateReportedLatency();
                const int total = p.getLatencySamples();
                expectEquals (total, Saturation::latencySamples + expectedExtra);
                juce::MidiBuffer midi;
                const int blocks = 80;
                Buf all = makeNoise (channels, 480 * blocks, 5);
                Buf outAll (channels, 480 * blocks);
                for (int b = 0; b < blocks; ++b)
                {
                    Buf blk (channels, 480);
                    for (int ch = 0; ch < channels; ++ch) blk.copyFrom (ch, 0, all, ch, b * 480, 480);
                    p.processBlock (blk, midi);
                    for (int ch = 0; ch < channels; ++ch) outAll.copyFrom (ch, b * 480, blk, ch, 0, 480);
                }
                float worst = 0.0f;
                for (int ch = 0; ch < channels; ++ch)
                    for (int i = 480 * 60; i < 480 * blocks; ++i)
                        worst = juce::jmax (worst, std::abs (outAll.getSample (ch, i) - all.getSample (ch, i - total)));
                expect (worst < 1.0e-9f, juce::String (id) + " = " + juce::String (value) + ", harmony " + (harmonyOn ? "on" : "off") + ", channels " + juce::String (channels) + ": worst difference " + juce::String (worst));
            };
            check ("mix", 0.0f, true, glitchkid::pitch::PitchCorrector::latencyFor (48000.0));
            check ("bypass", 1.0f, true, glitchkid::pitch::PitchCorrector::latencyFor (48000.0));
            check ("mix", 0.0f, false, 0);
            check ("bypass", 1.0f, false, 0);
        }

        beginTest ("MOODS + Doubler: CELESTIAL, THE CLONE and ALTERED STATE use it as designed; VOIDWALKER and THE PARASITE only switch it off; Harmony is never touched and latency never changes");
        {
            struct Map { int ego; float amount0, amount1, width0, width1, detune0, detune1, timing0, timing1; };
            const Map maps[] = {
                { 0, 22.0f, 34.0f,  90.0f, 100.0f,  5.0f,  7.0f, 24.0f, 24.0f },  // CELESTIAL: a wide, airy halo
                { 1, 40.0f, 62.0f,  70.0f,  90.0f,  8.0f, 13.0f, 20.0f, 26.0f },  // THE CLONE: layered
                { 4, 35.0f, 55.0f, 100.0f, 100.0f, 12.0f, 22.0f, 30.0f, 40.0f } };// ALTERED STATE: loose and unsteady
            for (const auto& m : maps)
                for (float chaos : { 0.0f, 0.5f, 1.0f })
                {
                    GlitchKidProcessor p;
                    setParam (p, "dbl_on", 0.0f); setParam (p, "dbl_amount", 99.0f); setParam (p, "dbl_width", 1.0f); setParam (p, "dbl_detune", 24.0f); setParam (p, "dbl_timing", 44.0f);
                    p.applyAlterEgo (m.ego, chaos, 0);
                    expectEquals (getParam (p, "dbl_on"), 1.0f);
                    expectWithinAbsoluteError (getParam (p, "dbl_amount"), m.amount0 + chaos * (m.amount1 - m.amount0), 0.06f);
                    expectWithinAbsoluteError (getParam (p, "dbl_width"), m.width0 + chaos * (m.width1 - m.width0), 0.06f);
                    expectWithinAbsoluteError (getParam (p, "dbl_detune"), m.detune0 + chaos * (m.detune1 - m.detune0), 0.06f);
                    expectWithinAbsoluteError (getParam (p, "dbl_timing"), m.timing0 + chaos * (m.timing1 - m.timing0), 0.06f);
                    expect (p.isAlterEgoActive());
                }
            for (int ego : { 2, 3 })
            {
                GlitchKidProcessor p;
                setParam (p, "dbl_on", 1.0f); setParam (p, "dbl_amount", 77.0f); setParam (p, "dbl_width", 13.0f); setParam (p, "dbl_detune", 19.0f); setParam (p, "dbl_timing", 33.0f);
                p.applyAlterEgo (ego, 1.0f, 0);
                expectEquals (getParam (p, "dbl_on"), 0.0f);
                expectWithinAbsoluteError (getParam (p, "dbl_amount"), 77.0f, 0.06f);
                expectWithinAbsoluteError (getParam (p, "dbl_width"), 13.0f, 0.06f);
                expectWithinAbsoluteError (getParam (p, "dbl_detune"), 19.0f, 0.06f);
                expectWithinAbsoluteError (getParam (p, "dbl_timing"), 33.0f, 0.06f);
                expect (p.isAlterEgoActive());
            }
            // Harmony is never owned: every Alter Ego, Chaos and Surprise seed leaves it alone, and the reported latency never changes
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 480);
            setParam (p, "harm_on", 1.0f); setParam (p, "harm_interval", 1.0f); setParam (p, "harm_level", -17.0f);
            p.updateReportedLatency();
            const int latency = p.getLatencySamples();
            bool untouched = true;
            for (int ego = 0; ego < AlterEgos::count; ++ego)
                for (std::uint32_t seed : { 0u, 7u, 4242u })
                {
                    p.applyAlterEgo (ego, 1.0f, seed);
                    p.updateReportedLatency();
                    untouched &= juce::exactlyEqual (getParam (p, "harm_on"), 1.0f) && juce::exactlyEqual (getParam (p, "harm_interval"), 1.0f)
                                 && std::abs (getParam (p, "harm_level") + 17.0f) < 0.06f && p.getLatencySamples() == latency;
                }
            expect (untouched, "Harmony and the reported latency are never changed by an Alter Ego");
            // Surprise Me does not vary the Doubler
            for (int ego : { 0, 1, 4 })
            {
                GlitchKidProcessor a, b;
                a.applyAlterEgo (ego, 0.5f, 0); b.applyAlterEgo (ego, 0.5f, 9999);
                for (auto* id : { "dbl_on", "dbl_amount", "dbl_width", "dbl_detune", "dbl_timing" })
                    expectWithinAbsoluteError (getParam (a, id), getParam (b, id), 1.0e-4f, "Surprise leaves " + juce::String (id) + " alone");
            }
            // cycling through all five (mono and stereo, NaN/Inf injected): finite, bounded
            for (int channels : { 1, 2 })
            {
                GlitchKidProcessor q;
                if (channels == 1) setLayout (q, juce::AudioChannelSet::mono());
                q.prepareToPlay (48000.0, 480);
                juce::MidiBuffer midi;
                const Buf lead = voice (channels, 480 * 300, 48000.0, 190.0);
                bool finite = true;
                float peak = 0.0f;
                for (int blk = 0; blk < 300; ++blk)
                {
                    if (blk % 50 == 0) q.applyAlterEgo ((blk / 50) % AlterEgos::count, (blk % 100) == 0 ? 1.0f : 0.0f, 0);
                    Buf b (channels, 480);
                    for (int ch = 0; ch < channels; ++ch) b.copyFrom (ch, 0, lead, ch, blk * 480, 480);
                    if (blk % 37 == 11) b.setSample (0, 100, qNaN);
                    q.processBlock (b, midi);
                    finite &= allFinite (b);
                    peak = juce::jmax (peak, b.getMagnitude (0, 480));
                }
                expect (finite && peak < 4.0f, "channels " + juce::String (channels) + ", peak " + juce::String (peak));
            }
        }

        beginTest ("Plug-in: Doubler and Harmony change the sound when on; the chain stays finite with everything on (mono and stereo, NaN/Inf injected, random automation)");
        for (int channels : { 1, 2 })
            for (int bs : { 64, 480, 1024 })
            {
                GlitchKidProcessor p;
                if (channels == 1) setLayout (p, juce::AudioChannelSet::mono());
                p.prepareToPlay (44100.0, bs);
                setParam (p, "tune_on", 1.0f); setParam (p, "formant_shift", 2.0f); setParam (p, "mut_on", 1.0f); setParam (p, "echo_on", 1.0f); setParam (p, "dim_on", 1.0f);
                juce::Random rng (bs + channels);
                juce::MidiBuffer midi;
                bool finite = true;
                float peak = 0.0f;
                const Buf v = voice (channels, bs * 120, 44100.0, 190.0);
                for (int blk = 0; blk < 120; ++blk)
                {
                    setParam (p, "dbl_on", rng.nextInt (4) == 0 ? 0.0f : 1.0f);
                    setParam (p, "dbl_amount", 100.0f * rng.nextFloat()); setParam (p, "dbl_width", 100.0f * rng.nextFloat());
                    setParam (p, "dbl_detune", 25.0f * rng.nextFloat()); setParam (p, "dbl_timing", 5.0f + 40.0f * rng.nextFloat());
                    setParam (p, "harm_on", rng.nextInt (3) == 0 ? 0.0f : 1.0f);
                    setParam (p, "harm_interval", (float) rng.nextInt (glitchkid::harmony::numIntervals)); setParam (p, "harm_level", -24.0f * rng.nextFloat());
                    Buf b (channels, bs);
                    for (int ch = 0; ch < channels; ++ch) b.copyFrom (ch, 0, v, ch, blk * bs, bs);
                    if (blk % 17 == 5) b.setSample (0, bs / 2, qNaN);
                    if (blk % 23 == 7) b.setSample (channels - 1, 3, posInf);
                    p.processBlock (b, midi);
                    finite &= allFinite (b);
                    peak = juce::jmax (peak, b.getMagnitude (0, bs));
                }
                expect (finite, "channels " + juce::String (channels) + " block " + juce::String (bs));
                expect (peak < 8.0f, "bounded: peak " + juce::String (peak));
            }

        beginTest ("Plug-in: the Doubler widens a mono vocal and the Harmony adds an audible voice at the chosen interval (end to end, through the whole chain)");
        {
            const double sr = 48000.0;
            // Doubler: a mono vocal in a stereo track becomes a stereo one
            {
                GlitchKidProcessor p;
                p.prepareToPlay (sr, 480);
                setParam (p, "dbl_on", 1.0f); setParam (p, "dbl_amount", 100.0f); setParam (p, "dbl_width", 100.0f);
                juce::MidiBuffer midi;
                const Buf in = voice (2, 480 * 100, sr, 200.0);
                Buf all (2, 480 * 100);
                for (int b = 0; b < 100; ++b)
                {
                    Buf blk (2, 480);
                    for (int ch = 0; ch < 2; ++ch) blk.copyFrom (ch, 0, in, ch, b * 480, 480);
                    p.processBlock (blk, midi);
                    for (int ch = 0; ch < 2; ++ch) all.copyFrom (ch, b * 480, blk, ch, 0, 480);
                }
                double diff = 0.0;
                for (int i = 480 * 50; i < 480 * 100; ++i) diff = juce::jmax (diff, (double) std::abs (all.getSample (0, i) - all.getSample (1, i)));
                expect (diff > 0.02, "left and right differ: " + juce::String (diff));
            }
            // Harmony: the energy at the interval's frequency rises
            {
                const double f0 = 200.0;
                const int idx = 7; // fifth up -> about 299.7 Hz; 200 Hz voice partials at 200k; 299.7 is NOT a partial of the lead, so it is the harmony's fundamental
                auto level = [&] (bool on)
                {
                    GlitchKidProcessor p;
                    p.prepareToPlay (sr, 480);
                    if (on) { setParam (p, "harm_on", 1.0f); setParam (p, "harm_interval", (float) idx); setParam (p, "harm_level", 0.0f); }
                    juce::MidiBuffer midi;
                    const int blocks = 200;
                    const Buf in = voice (2, 480 * blocks, sr, f0);
                    Buf all (1, 480 * blocks);
                    for (int b = 0; b < blocks; ++b)
                    {
                        Buf blk (2, 480);
                        for (int ch = 0; ch < 2; ++ch) blk.copyFrom (ch, 0, in, ch, b * 480, 480);
                        p.processBlock (blk, midi);
                        all.copyFrom (0, b * 480, blk, 0, 0, 480);
                    }
                    auto energyAt = [&] (double hz)
                    {
                        double re = 0.0, im = 0.0;
                        for (int i = 480 * 100; i < 480 * 190; ++i)
                        {
                            const double w = 0.5 - 0.5 * std::cos (2.0 * pi * (i - 480 * 100) / (480.0 * 90));
                            const double x = all.getSample (0, i) * w;
                            re += x * std::cos (2.0 * pi * hz * i / sr); im += x * std::sin (2.0 * pi * hz * i / sr);
                        }
                        return std::sqrt (re * re + im * im);
                    };
                    const double up = f0 * std::pow (2.0, glitchkid::harmony::intervalInfo (idx).semitones / 12.0);
                    const double mirrored = f0 * std::pow (2.0, -glitchkid::harmony::intervalInfo (idx).semitones / 12.0); // the same interval in the other direction
                    return std::pair<double, double> { energyAt (up), energyAt (mirrored) };
                };
                const auto off = level (false), on = level (true);
                logMessage ("energy a fifth above / a fifth below a 200 Hz voice: harmony off " + juce::String (off.first, 3) + " / " + juce::String (off.second, 3)
                            + ", on " + juce::String (on.first, 3) + " / " + juce::String (on.second, 3));
                expect (on.first > 8.0 * off.first + 1.0, "the harmony voice is audible at the interval: " + juce::String (on.first) + " vs " + juce::String (off.first));
                expect (on.first > 6.0 * on.second, "...and it is ABOVE the lead, not below: " + juce::String (on.first) + " above vs " + juce::String (on.second) + " below");
            }
        }

        beginTest ("Plug-in state: Doubler and Harmony parameters round-trip and survive a DAW save/reopen; an older state without them loads with their defaults; presets and A/B include them");
        {
            GlitchKidProcessor a;
            setParam (a, "dbl_on", 1.0f); setParam (a, "dbl_amount", 73.0f); setParam (a, "dbl_width", 31.0f); setParam (a, "dbl_detune", 17.0f);
            setParam (a, "dbl_timing", 33.0f); setParam (a, "harm_on", 1.0f); setParam (a, "harm_interval", 2.0f); setParam (a, "harm_level", -15.0f);
            setParam (a, "gain", -2.0f);
            juce::MemoryBlock state;
            a.getStateInformation (state);
            GlitchKidProcessor b;
            b.setStateInformation (state.getData(), (int) state.getSize());
            for (auto* id : { "dbl_on", "dbl_amount", "dbl_width", "dbl_detune", "dbl_timing", "harm_on", "harm_interval", "harm_level" })
                expectWithinAbsoluteError (getParam (b, id), getParam (a, id), 0.06f, id);

            if (auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize()))
            {
                for (int i = xml->getNumChildElements() - 1; i >= 0; --i)
                {
                    const auto id = xml->getChildElement (i)->getStringAttribute ("id");
                    if (id.startsWith ("dbl_") || id.startsWith ("harm_"))
                        xml->removeChildElement (xml->getChildElement (i), true);
                }
                juce::MemoryBlock old;
                juce::AudioProcessor::copyXmlToBinary (*xml, old);
                GlitchKidProcessor c;
                setParam (c, "dbl_amount", 5.0f);
                c.setStateInformation (old.getData(), (int) old.getSize());
                expectWithinAbsoluteError (getParam (c, "gain"), -2.0f, 0.05f);
                expectEquals (getParam (c, "dbl_on"), 0.0f);
                expectWithinAbsoluteError (getParam (c, "dbl_amount"), 50.0f, 0.1f);
                expectWithinAbsoluteError (getParam (c, "dbl_timing"), 18.0f, 0.1f);
                expectEquals (getParam (c, "harm_on"), 0.0f);
                expectEquals (getParam (c, "harm_interval"), (float) glitchkid::harmony::defaultInterval);
            }
            else
                expect (false, "could not read saved state");

            // presets and A/B capture and restore them like every other parameter
            const auto snap = a.captureSoundState();
            expect (snap.has ("dbl_amount") && snap.has ("harm_interval") && snap.has ("harm_level"));
            GlitchKidProcessor d;
            d.applySoundState (snap);
            for (auto* id : { "dbl_on", "dbl_amount", "dbl_width", "dbl_detune", "dbl_timing", "harm_on", "harm_interval", "harm_level" })
                expectWithinAbsoluteError (getParam (d, id), getParam (a, id), 0.06f, id);
        }

        beginTest ("real-time safety: no memory allocation while processing (both modules alone, and inside the plug-in with both on)");
        {
            auto d = makeDoubler (48000.0, dbl());
            Buf b = monoNoise (2, 480, 3);
            d.process (b);
            long before = g_allocations.load();
            for (int i = 0; i < 200; ++i)
            {
                d.setSettings (dbl (10.0f + (float) (i % 80), (float) (i % 100), (float) (i % 25), 5.0f + (float) (i % 40)));
                d.process (b);
            }
            expectEquals (g_allocations.load() - before, 0L);

            auto h = makeHarmony (48000.0, 7.0f, -6.0f);
            Buf v = voice (2, 480 * 60, 48000.0, 200.0);
            float* p[2] = { v.getWritePointer (0), v.getWritePointer (1) };
            h.process (p, 2, 480);
            before = g_allocations.load();
            for (int i = 1; i < 60; ++i)
            {
                float* q[2] = { v.getWritePointer (0) + i * 480, v.getWritePointer (1) + i * 480 };
                h.setSettings ({ (float) (i % 12) - 6.0f, -3.0f - (float) (i % 10) });
                h.process (q, 2, 480);
            }
            expectEquals (g_allocations.load() - before, 0L);

            GlitchKidProcessor proc;
            proc.prepareToPlay (48000.0, 480);
            setParam (proc, "dbl_on", 1.0f); setParam (proc, "harm_on", 1.0f); setParam (proc, "tune_on", 1.0f);
            std::vector<Buf> blocks;
            const Buf lead = voice (2, 480 * 100, 48000.0, 200.0);
            for (int i = 0; i < 100; ++i)
            {
                Buf blk (2, 480);
                for (int ch = 0; ch < 2; ++ch) blk.copyFrom (ch, 0, lead, ch, i * 480, 480);
                blocks.push_back (std::move (blk));
            }
            juce::MidiBuffer midi;
            for (int i = 0; i < 20; ++i) proc.processBlock (blocks[(size_t) i], midi); // warm-up
            before = g_allocations.load();
            for (size_t i = 20; i < blocks.size(); ++i) proc.processBlock (blocks[i], midi);
            expectEquals (g_allocations.load() - before, 0L);
        }
    }
};

static DoublingHarmonyTests doublingHarmonyTests;
