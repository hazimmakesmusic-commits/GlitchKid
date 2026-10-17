#include "TestHelpers.h"
#include "../Source/DSP/VocalMutation.h"
#include "../Source/DSP/Saturation.h"

// Stage 4.5: Vocal Mutation (single-sideband frequency shifter / ring modulator), alone and inside the plug-in.
// Synthetic signals only: these tests prove the maths, safety and signal flow, not that it sounds good on a voice.
extern std::atomic<long> g_allocations; // counted in FormantShiftTests.cpp (it replaces the global operator new)

namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr float qNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float posInf = std::numeric_limits<float>::infinity();

MutationSettings mut (float mutation = 40.0f, float character = 50.0f, float blend = 100.0f, bool on = true)
{
    MutationSettings s;
    s.on = on; s.mutationPct = mutation; s.characterPct = character; s.blendPct = blend;
    return s;
}

VocalMutation make (double sr, const MutationSettings& s)
{
    VocalMutation m;
    m.setSettings (s);
    m.prepare (sr);
    return m;
}

template <typename F>
Buf stream (VocalMutation& m, Buf in, int blockSize, F before)
{
    for (int start = 0; start < in.getNumSamples(); start += blockSize)
    {
        before (start, m);
        Buf view (in.getArrayOfWritePointers(), in.getNumChannels(), start, juce::jmin (blockSize, in.getNumSamples() - start));
        m.process (view);
    }
    return in;
}
Buf stream (VocalMutation& m, Buf in, int blockSize = 512) { return stream (m, std::move (in), blockSize, [] (int, VocalMutation&) {}); }

Buf silent (int ch, int n) { Buf b (ch, n); b.clear(); return b; }

Buf sine (int channels, int n, double sr, double freq, float amp = 0.5f)
{
    Buf b (channels, n);
    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < n; ++i)
            b.setSample (ch, i, (float) (amp * std::sin (2.0 * pi * freq * (double) i / sr)));
    return b;
}

// A harmonic "voice": 12 partials of f0 with 1/h amplitudes.
Buf harmonicVoice (int channels, int n, double sr, double f0)
{
    Buf b (channels, n);
    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < n; ++i)
        {
            double s = 0.0;
            for (int h = 1; h <= 12; ++h)
                s += std::sin (2.0 * pi * h * f0 * (double) i / sr) / h;
            b.setSample (ch, i, (float) (0.15 * s));
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

// Amplitude of the component at `hz` (Hann-windowed single-bin DFT).
double amp (const Buf& b, int ch, int from, int count, double hz, double sr)
{
    double re = 0.0, im = 0.0, ws = 0.0;
    for (int i = 0; i < count; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * pi * i / count);
        const double x = (double) b.getSample (ch, from + i);
        const double ph = 2.0 * pi * hz * (double) (from + i) / sr;
        re += w * x * std::cos (ph);
        im += w * x * std::sin (ph);
        ws += w;
    }
    return 2.0 * std::sqrt (re * re + im * im) / ws;
}

double db (double a, double b) { return 20.0 * std::log10 ((a + 1.0e-12) / (b + 1.0e-12)); }

double rms (const Buf& b, int ch, int from, int count) { return (double) b.getRMSLevel (ch, from, count); }

float maxStep (const Buf& b, int from = 1)
{
    float m = 0.0f;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = from; i < b.getNumSamples(); ++i)
            m = juce::jmax (m, std::abs (b.getSample (ch, i) - b.getSample (ch, i - 1)));
    return m;
}

bool exactlyZero (const Buf& b, int from, int to)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = from; i < to; ++i)
            if (! juce::exactlyEqual (b.getSample (ch, i), 0.0f))
                return false;
    return true;
}

// Fraction of a signal's power that sits at exact multiples of f0 (1 = a perfectly harmonic tone).
double harmonicFraction (const Buf& b, int ch, int from, int count, double f0, double sr)
{
    double harm = 0.0;
    for (int k = 1; k * f0 < 0.45 * sr; ++k)
        harm += 0.5 * std::pow (amp (b, ch, from, count, k * f0, sr), 2.0);
    const double total = std::pow (rms (b, ch, from, count), 2.0);
    return total > 0.0 ? harm / total : 0.0;
}
} // namespace

struct MutationTests : public juce::UnitTest
{
    MutationTests() : juce::UnitTest ("Vocal Mutation", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("parameter mapping: Mutation 0..100 % -> 0..600 Hz (squared, monotonic); Character 0..100 % -> -1..+1");
        {
            expectEquals (VocalMutation::carrierHzFor (0.0f), 0.0);
            expectWithinAbsoluteError (VocalMutation::carrierHzFor (100.0f), 600.0, 1.0e-9);
            expectWithinAbsoluteError (VocalMutation::carrierHzFor (50.0f), 150.0, 1.0e-9);
            double prev = -1.0;
            bool monotonic = true;
            for (float m = 0.0f; m <= 100.0f; m += 0.5f)
            {
                const double f = VocalMutation::carrierHzFor (m);
                monotonic &= f > prev;
                prev = f;
            }
            expect (monotonic);
            expectEquals (VocalMutation::carrierHzFor (-30.0f), 0.0);
            expectWithinAbsoluteError (VocalMutation::carrierHzFor (250.0f), 600.0, 1.0e-9);
            expectEquals (VocalMutation::sidebandFor (0.0f), -1.0);
            expectEquals (VocalMutation::sidebandFor (50.0f), 0.0);
            expectEquals (VocalMutation::sidebandFor (100.0f), 1.0);
        }

        beginTest ("neutral: off, Mutation 0 and Blend 0 are bit-exact copies and the stage reports idle");
        for (int ch : { 1, 2 })
        {
            Buf in = makeNoise (ch, 20000, 3);
            for (auto s : { mut (40.0f, 50.0f, 100.0f, false), mut (0.0f), mut (60.0f, 20.0f, 0.0f) })
            {
                auto m = make (48000.0, s);
                Buf out = stream (m, in);
                expectEquals (maxAbsDiff (out, in), 0.0f);
                expect (! m.isActive());
            }
        }

        beginTest ("single sideband: UP moves every partial up by the carrier, DOWN down, ROBOT makes both sidebands; image >= 35 dB down (44.1/48/88.2/96 kHz)");
        double worstRejectionDb = -1000.0; // the weakest (least negative) image level seen, relative to the wanted sideband
        for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0 })
            for (double f : { 300.0, 1000.0, 5000.0 })
            {
                const int n = (int) sr;
                const double fc = 150.0; // Mutation 50 %
                Buf in = sine (2, n, sr, f, 0.5f);
                const juce::String where = juce::String (sr) + " Hz, tone " + juce::String (f);

                auto up = make (sr, mut (50.0f, 100.0f)); Buf u = stream (up, in);
                const double uWanted = amp (u, 0, n / 4, n / 2, f + fc, sr);
                expectWithinAbsoluteError (uWanted, 0.5, 0.01, "UP wanted sideband level " + where);
                worstRejectionDb = juce::jmax (worstRejectionDb, db (amp (u, 0, n / 4, n / 2, f - fc, sr), uWanted));
                expect (db (amp (u, 0, n / 4, n / 2, f - fc, sr), uWanted) < -35.0, "UP image " + where);
                expect (db (amp (u, 0, n / 4, n / 2, f, sr), uWanted) < -35.0, "UP original partial gone " + where);

                auto dn = make (sr, mut (50.0f, 0.0f)); Buf d = stream (dn, in);
                const double dWanted = amp (d, 0, n / 4, n / 2, f - fc, sr);
                expectWithinAbsoluteError (dWanted, 0.5, 0.01, "DOWN wanted sideband level " + where);
                worstRejectionDb = juce::jmax (worstRejectionDb, db (amp (d, 0, n / 4, n / 2, f + fc, sr), dWanted));
                expect (db (amp (d, 0, n / 4, n / 2, f + fc, sr), dWanted) < -35.0, "DOWN image " + where);

                auto rb = make (sr, mut (50.0f, 50.0f)); Buf r = stream (rb, in);
                const double lo = amp (r, 0, n / 4, n / 2, f - fc, sr), hi = amp (r, 0, n / 4, n / 2, f + fc, sr);
                // Ring modulation splits the 0.5 tone into two sidebands of 0.25 each; the equal-level gain sqrt(2) lifts them to 0.354
                // (total power equal to the input's, 0.5^2 / 2).
                expectWithinAbsoluteError (lo, 0.25 * std::sqrt (2.0), 0.01, "ROBOT lower sideband " + where);
                expectWithinAbsoluteError (hi, lo, 0.01, "ROBOT sidebands equal " + where);
                expect (db (amp (r, 0, n / 4, n / 2, f, sr), hi) < -35.0, "ROBOT original partial gone " + where);
            }
        logMessage ("worst unwanted-sideband level over 4 sample rates x 3 tones x UP/DOWN: " + juce::String (worstRejectionDb, 1) + " dB re the wanted sideband");

        beginTest ("Mutation moves the sidebands (carrier = 600 Hz x m^2) and Character moves them between DOWN / ROBOT / UP");
        {
            const double sr = 48000.0;
            const int n = 48000;
            Buf in = sine (1, n, sr, 1000.0, 0.5f);
            for (float m : { 25.0f, 50.0f, 75.0f, 100.0f })
            {
                auto u = make (sr, mut (m, 100.0f));
                Buf o = stream (u, in);
                const double fc = VocalMutation::carrierHzFor (m);
                expect (amp (o, 0, n / 4, n / 2, 1000.0 + fc, sr) > 0.45, "UP at Mutation " + juce::String (m));
            }
            // Character morphs smoothly: the lower sideband falls and the upper rises, monotonically.
            double prevLo = 1.0e9, prevHi = -1.0;
            bool ok = true;
            for (float c : { 0.0f, 20.0f, 40.0f, 50.0f, 60.0f, 80.0f, 100.0f })
            {
                auto mm = make (sr, mut (50.0f, c));
                Buf o = stream (mm, in);
                const double lo = amp (o, 0, n / 4, n / 2, 850.0, sr), hi = amp (o, 0, n / 4, n / 2, 1150.0, sr);
                ok &= lo < prevLo + 1.0e-3 && hi > prevHi - 1.0e-3;
                prevLo = lo; prevHi = hi;
            }
            expect (ok, "sidebands change monotonically with Character");
        }

        beginTest ("it is not a loudness control: level is equal across Character, but the spectrum is different");
        {
            const double sr = 48000.0;
            Buf in = makeNoise (1, 96000, 8);
            const double ref = rms (in, 0, 20000, 60000);
            Buf first;
            double maxDevDb = 0.0;
            for (float c : { 0.0f, 25.0f, 50.0f, 75.0f, 100.0f })
            {
                auto m = make (sr, mut (60.0f, c));
                Buf o = stream (m, in);
                maxDevDb = juce::jmax (maxDevDb, std::abs (db (rms (o, 0, 20000, 60000), ref)));
                if (c < 1.0f) first = o;
                else if (c > 99.0f)
                    expect (maxAbsDiff (o, first) > 0.1f, "DOWN and UP outputs differ strongly");
            }
            logMessage ("noise level across Character 0..100 %: worst deviation from the input " + juce::String (maxDevDb, 2) + " dB");
            expect (maxDevDb < 1.0, "level varies by " + juce::String (maxDevDb) + " dB");
        }

        beginTest ("different from the saturator: a harmonic voice stays harmonic through tanh, but not through Mutation");
        {
            const double sr = 48000.0;
            const int n = 96000;
            const double f0 = 220.0;
            Buf in = harmonicVoice (2, n, sr, f0);
            const double inFrac = harmonicFraction (in, 0, n / 4, n / 2, f0, sr);
            expect (inFrac > 0.97, "test voice is harmonic: " + juce::String (inFrac));

            Saturation sat;
            sat.setDriveDb (15.0f);
            sat.prepare (sr, 512);
            Buf s = in;
            for (int start = 0; start < n; start += 512)
            {
                Buf view (s.getArrayOfWritePointers(), 2, start, juce::jmin (512, n - start));
                sat.process (view);
            }
            const double satFrac = harmonicFraction (s, 0, n / 4, n / 2, f0, sr);

            auto m = make (sr, mut (42.8f, 100.0f)); // carrier about 110 Hz = half the fundamental
            Buf o = stream (m, in);
            const double mutFrac = harmonicFraction (o, 0, n / 4, n / 2, f0, sr);
            logMessage ("share of power at exact multiples of 220 Hz: input " + juce::String (inFrac, 3) + ", saturator (15 dB) " + juce::String (satFrac, 3)
                        + ", mutation (UP, 110 Hz) " + juce::String (mutFrac, 3));
            expect (satFrac > 0.9, "saturator keeps partials harmonic: " + juce::String (satFrac));
            expect (mutFrac < 0.1, "mutation breaks the harmonic series: " + juce::String (mutFrac));
        }

        beginTest ("Blend: 0 is exact, 100 is fully mutated, 50 is the exact average of the two");
        {
            const double sr = 48000.0;
            Buf in = makeNoise (2, 30000, 5);
            auto m0 = make (sr, mut (60.0f, 80.0f, 0.0f)), m100 = make (sr, mut (60.0f, 80.0f, 100.0f)), m50 = make (sr, mut (60.0f, 80.0f, 50.0f));
            Buf a = stream (m0, in), b = stream (m100, in), c = stream (m50, in);
            expectEquals (maxAbsDiff (a, in), 0.0f);
            expect (maxAbsDiff (b, in) > 0.1f);
            Buf avg (2, 30000);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 30000; ++i)
                    avg.setSample (ch, i, 0.5f * (in.getSample (ch, i) + b.getSample (ch, i)));
            expect (maxAbsDiff (c, avg) < 1.0e-5f, "worst error " + juce::String (maxAbsDiff (c, avg)));
        }

        beginTest ("continuity at the bottom of the range: just above Mutation 0 the sound stays close to the dry voice (the wet fades in)");
        for (float character : { 0.0f, 50.0f, 100.0f })
        {
            Buf in = makeNoise (1, 30000, 12);
            auto m = make (48000.0, mut (0.3f, character));
            Buf out = stream (m, in);
            double num = 0.0, den = 0.0;
            for (int i = 5000; i < 30000; ++i)
            {
                const double d = (double) out.getSample (0, i) - (double) in.getSample (0, i);
                num += d * d;
                den += (double) in.getSample (0, i) * (double) in.getSample (0, i);
            }
            expect (std::sqrt (num / den) < 0.25, "relative difference " + juce::String (std::sqrt (num / den)) + " at Character " + juce::String (character));
        }

        beginTest ("silence in gives exact silence out; very low-level signals stay finite, keep their level and do not blow up");
        for (int ch : { 1, 2 })
        {
            auto m = make (48000.0, mut (70.0f, 30.0f));
            Buf out = stream (m, silent (ch, 48000));
            expect (allFinite (out) && exactlyZero (out, 0, out.getNumSamples()));

            for (float scale : { 1.0e-7f, 1.0e-20f, 1.0e-37f })
            {
                Buf quiet = makeNoise (ch, 48000, 6);
                quiet.applyGain (scale);
                auto q = make (48000.0, mut (70.0f, 30.0f));
                Buf o = stream (q, quiet);
                expect (allFinite (o));
                expect (rms (o, 0, 10000, 30000) < 4.0f * rms (quiet, 0, 10000, 30000) + 1.0e-45f, "no gain blow-up at scale " + juce::String (scale));
            }
        }

        beginTest ("impulse and a hard step: finite, bounded, and the tail dies out");
        for (float ch : { 0.0f, 50.0f, 100.0f })
        {
            auto m = make (48000.0, mut (80.0f, ch));
            Buf imp = silent (2, 60000);
            imp.setSample (0, 100, 1.0f); imp.setSample (1, 100, 1.0f);
            Buf o = stream (m, imp);
            double energy = 0.0, tail = 0.0;
            for (int i = 0; i < 60000; ++i)
            {
                const double v = (double) o.getSample (0, i);
                energy += v * v;
                if (i > 40000) tail += v * v;
            }
            expect (allFinite (o));
            expect (energy > 0.3 && energy < 4.5, "impulse energy " + juce::String (energy));
            expect (tail < 1.0e-6, "tail energy " + juce::String (tail));

            auto s = make (48000.0, mut (80.0f, ch));
            Buf step = silent (2, 20000);
            for (int i = 5000; i < 20000; ++i) { step.setSample (0, i, 0.5f); step.setSample (1, i, 0.5f); }
            Buf so = stream (s, step);
            float peak = 0.0f;
            for (int i = 0; i < 20000; ++i) peak = juce::jmax (peak, std::abs (so.getSample (0, i)));
            expect (peak < 1.3f, "step response peak " + juce::String (peak) + " for input 0.5");
        }

        beginTest ("mono and stereo: identical channels stay identical, a one-sided signal stays one-sided, mono matches stereo");
        {
            Buf one = makeNoise (1, 30000, 9);
            Buf two (2, 30000);
            two.copyFrom (0, 0, one, 0, 0, 30000);
            two.copyFrom (1, 0, one, 0, 0, 30000);
            auto ms = make (48000.0, mut (70.0f, 90.0f)), mm = make (48000.0, mut (70.0f, 90.0f));
            Buf s = stream (ms, two), m = stream (mm, one);
            float lr = 0.0f, mono = 0.0f;
            for (int i = 0; i < 30000; ++i)
            {
                lr = juce::jmax (lr, std::abs (s.getSample (0, i) - s.getSample (1, i)));
                mono = juce::jmax (mono, std::abs (s.getSample (0, i) - m.getSample (0, i)));
            }
            expectEquals (lr, 0.0f);
            expectEquals (mono, 0.0f);

            Buf left = two;
            left.clear (1, 0, 30000);
            auto ml = make (48000.0, mut (70.0f, 90.0f));
            Buf lo = stream (ml, left);
            expect (exactlyZero (lo, 0, 30000) == false);
            bool rightZero = true;
            for (int i = 0; i < 30000; ++i) rightZero &= juce::exactlyEqual (lo.getSample (1, i), 0.0f);
            expect (rightZero, "silent right channel stays silent");
        }

        beginTest ("block size independence: 1, 7, 64, 333 and 2048-sample blocks give bit-identical output");
        {
            Buf in = makeNoise (2, 20000, 11);
            auto ref = make (48000.0, mut (65.0f, 25.0f, 80.0f));
            Buf expected = stream (ref, in, 512);
            for (int bs : { 1, 7, 64, 333, 2048 })
            {
                auto m = make (48000.0, mut (65.0f, 25.0f, 80.0f));
                expectEquals (maxAbsDiff (stream (m, in, bs), expected), 0.0f, "block size " + juce::String (bs));
            }
        }

        beginTest ("parameter changes during processing and on/off switching are click-free; off is exact; no stale audio");
        {
            const double sr = 48000.0;
            auto m = make (sr, mut (50.0f, 50.0f, 60.0f));
            juce::Random rng (21);
            Buf in = sine (2, 48000 * 4, sr, 220.0, 0.5f);
            Buf o = stream (m, in, 256, [&] (int, VocalMutation& x)
            {
                x.setSettings (mut (100.0f * rng.nextFloat(), 100.0f * rng.nextFloat(), 100.0f * rng.nextFloat()));
            });
            expect (allFinite (o));
            expect (maxStep (o, 100) < 0.2f, "largest step under random automation " + juce::String (maxStep (o, 100)) + " (dry sine alone: 0.0144)");

            // One large jump of Mutation in the middle of a block must be smoothed.
            auto j = make (sr, mut (10.0f, 100.0f));
            Buf jo = stream (j, sine (2, 48000, sr, 220.0, 0.5f), 4800, [&] (int start, VocalMutation& x)
            {
                if (start == 24000) x.setSettings (mut (95.0f, 0.0f));
            });
            expect (maxStep (jo, 100) < 0.2f, "step across a Mutation/Character jump " + juce::String (maxStep (jo, 100)));

            auto startOff = mut (50.0f, 50.0f, 100.0f, false);
            auto g = make (sr, startOff);
            Buf toggled = stream (g, sine (2, 48000 * 4, sr, 220.0, 0.5f), 480, [&] (int start, VocalMutation& x)
            {
                auto t = mut (50.0f, 50.0f);
                t.on = (start / 48000) % 2 == 1; // off, on, off, on
                x.setSettings (t);
            });
            expect (maxStep (toggled, 1) < 0.2f, "on/off largest step " + juce::String (maxStep (toggled)));
            Buf firstSecond (2, 48000), dryFirst = sine (2, 48000, sr, 220.0, 0.5f);
            for (int ch = 0; ch < 2; ++ch) firstSecond.copyFrom (ch, 0, toggled, ch, 0, 48000);
            expectEquals (maxAbsDiff (firstSecond, dryFirst), 0.0f);

            auto h = make (sr, mut (50.0f, 50.0f));
            stream (h, sine (2, 20000, sr, 220.0, 0.5f));
            h.setSettings (mut (50.0f, 50.0f, 100.0f, false));
            stream (h, silent (2, 5000));
            expect (! h.isActive(), "idle after the fade");
            h.setSettings (mut (50.0f, 50.0f));
            expect (exactlyZero (stream (h, silent (2, 20000)), 0, 20000), "no stale audio after re-enable");
        }

        beginTest ("NaN and +-Inf are treated as 0 (bit-exact with the same signal where they are zero) and do not contaminate later audio");
        {
            Buf clean = sine (2, 40000, 48000.0, 330.0, 0.4f);
            Buf dirty = clean, zeroed = clean;
            const int at[] = { 100, 101, 5000, 5001, 5002, 12000, 20000, 20001, 33333 };
            for (int k = 0; k < 9; ++k)
                for (int ch = 0; ch < 2; ++ch)
                {
                    dirty.setSample (ch, at[k], k % 3 == 0 ? qNaN : (k % 3 == 1 ? posInf : -posInf));
                    zeroed.setSample (ch, at[k], 0.0f);
                }
            auto a = make (48000.0, mut (70.0f, 80.0f)), b = make (48000.0, mut (70.0f, 80.0f));
            Buf oa = stream (a, dirty, 333), ob = stream (b, zeroed, 333);
            expect (allFinite (oa));
            expectEquals (maxAbsDiff (oa, ob), 0.0f);

            // An overflow burst: finite, and afterwards the stage behaves like one that saw silence.
            auto h = make (48000.0, mut (70.0f, 80.0f)), ref = make (48000.0, mut (70.0f, 80.0f));
            Buf burst (2, 2000);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 2000; ++i)
                    burst.setSample (ch, i, (i & 1) ? -3.0e38f : 3.0e38f);
            expect (allFinite (stream (h, burst)), "finite during an overflow burst");
            stream (ref, silent (2, 2000));
            Buf next = makeNoise (2, 20000, 14);
            expectEquals (maxAbsDiff (stream (h, next), stream (ref, next)), 0.0f);
        }

        beginTest ("processor: default off, 4 new parameters registered with the right defaults and ranges, no latency change");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 512);
            expectEquals (p.getParameters().size(), 46);
            expectEquals (getParam (p, "mut_on"), 0.0f);
            expectWithinAbsoluteError (getParam (p, "mut_amount"), 40.0f, 0.01f);
            expectWithinAbsoluteError (getParam (p, "mut_character"), 50.0f, 0.01f);
            expectWithinAbsoluteError (getParam (p, "mut_blend"), 100.0f, 0.01f);
            const int base = p.getLatencySamples();
            expectEquals (base, Saturation::latencySamples);
            setParam (p, "mut_on", 1.0f);
            setParam (p, "mut_amount", 100.0f);
            expectEquals (p.getWantedLatencySamples(), base);
            p.updateReportedLatency();
            expectEquals (p.getLatencySamples(), base);
            setParam (p, "mut_amount", 250.0f); // clamped by the host-facing range
            expectEquals (getParam (p, "mut_amount"), 100.0f);
            setParam (p, "mut_character", -5.0f);
            expectEquals (getParam (p, "mut_character"), 0.0f);
        }

        beginTest ("processor: default is a bit-exact pass-through; on it shifts the voice; Bypass and Mix 0 restore the exact input");
        for (int channels : { 1, 2 })
        {
            auto runCase = [&] (const char* id, float value, bool expectExact)
            {
                GlitchKidProcessor p;
                if (channels == 1) setLayout (p, juce::AudioChannelSet::mono());
                p.prepareToPlay (48000.0, 512);
                if (id != nullptr)
                {
                    setParam (p, "mut_on", 1.0f);
                    setParam (p, "mut_amount", 80.0f);
                    setParam (p, "mut_character", 100.0f);
                    setParam (p, id, value);
                }
                Buf lastIn, lastOut;
                run (p, channels, 512, 40, lastIn, lastOut);
                const float diff = maxAbsDiff (lastIn, lastOut);
                if (expectExact)
                    expectEquals (diff, 0.0f, juce::String (id == nullptr ? "default" : id) + " channels " + juce::String (channels));
                else
                    expect (diff > 0.05f, "mutation audible, channels " + juce::String (channels));
            };
            runCase (nullptr, 0.0f, true);
            runCase ("mut_on", 1.0f, false);
            runCase ("bypass", 1.0f, true);
            runCase ("mix", 0.0f, true);
            runCase ("mut_blend", 0.0f, true);
        }

        beginTest ("processor: after a settled Bypass nothing of the old audio comes back when Bypass is released");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 480);
            setParam (p, "mut_on", 1.0f); setParam (p, "mut_amount", 80.0f); setParam (p, "mut_character", 90.0f);
            juce::MidiBuffer midi;
            const Buf voice = harmonicVoice (2, 480 * 40, 48000.0, 220.0);
            for (int blk = 0; blk < 40; ++blk)
            {
                Buf b (2, 480);
                for (int ch = 0; ch < 2; ++ch) b.copyFrom (ch, 0, voice, ch, blk * 480, 480);
                p.processBlock (b, midi);
            }
            setParam (p, "bypass", 1.0f);
            for (int blk = 0; blk < 10; ++blk) { Buf b (2, 480); b.clear(); p.processBlock (b, midi); }
            setParam (p, "bypass", 0.0f);
            bool allZero = true;
            for (int blk = 0; blk < 10; ++blk)
            {
                Buf b (2, 480);
                b.clear();
                p.processBlock (b, midi);
                allZero &= exactlyZero (b, 0, 480);
            }
            expect (allZero, "silence in after un-bypassing gives exact silence out");
        }

        beginTest ("processor: with Pitch Tune and FORMANT on as well the chain stays finite, latency is unchanged by Mutation, and Mutation does change the sound");
        {
            GlitchKidProcessor p, q;
            for (auto* x : { &p, &q })
            {
                x->prepareToPlay (48000.0, 480);
                setParam (*x, "tune_on", 1.0f); setParam (*x, "formant_shift", 3.0f);
                setParam (*x, "echo_on", 1.0f); setParam (*x, "dim_on", 1.0f);
            }
            setParam (q, "mut_on", 1.0f); setParam (q, "mut_amount", 70.0f); setParam (q, "mut_character", 20.0f);
            p.updateReportedLatency(); q.updateReportedLatency();
            expectEquals (p.getLatencySamples(), q.getLatencySamples());
            juce::MidiBuffer midi;
            bool finite = true;
            float diff = 0.0f;
            const Buf voice = harmonicVoice (2, 480 * 120, 48000.0, 220.0);
            for (int blk = 0; blk < 120; ++blk)
            {
                Buf a (2, 480), b;
                for (int ch = 0; ch < 2; ++ch)
                    a.copyFrom (ch, 0, voice, ch, blk * 480, 480);
                b.makeCopyOf (a);
                p.processBlock (a, midi); q.processBlock (b, midi);
                finite &= allFinite (a) && allFinite (b);
                diff = juce::jmax (diff, maxAbsDiff (a, b));
            }
            expect (finite);
            expect (diff > 0.05f, "mutation changes the chain output: " + juce::String (diff));
        }

        beginTest ("processor: random automation of every Mutation parameter, mono and stereo, various block sizes, stays finite");
        for (int channels : { 1, 2 })
            for (int bs : { 64, 257, 1024 })
            {
                GlitchKidProcessor p;
                if (channels == 1) setLayout (p, juce::AudioChannelSet::mono());
                p.prepareToPlay (44100.0, bs);
                juce::Random rng (bs + channels);
                juce::MidiBuffer midi;
                bool finite = true;
                for (int blk = 0; blk < 120; ++blk)
                {
                    setParam (p, "mut_on", rng.nextInt (4) == 0 ? 0.0f : 1.0f);
                    setParam (p, "mut_amount", 100.0f * rng.nextFloat());
                    setParam (p, "mut_character", 100.0f * rng.nextFloat());
                    setParam (p, "mut_blend", 100.0f * rng.nextFloat());
                    Buf b = makeNoise (channels, bs, blk);
                    if (blk % 17 == 5) b.setSample (0, bs / 2, qNaN);
                    if (blk % 23 == 7) b.setSample (channels - 1, 3, posInf);
                    p.processBlock (b, midi);
                    finite &= allFinite (b);
                }
                expect (finite, "channels " + juce::String (channels) + " block " + juce::String (bs));
            }

        beginTest ("MOODS: each Alter Ego maps to Vocal Mutation as designed; THE CLONE only switches it off and leaves the other three values alone");
        {
            struct Map { int ego; bool on; float amount0, amount1, character0, character1, blend0, blend1; };
            const Map maps[] = {
                { 0, true,  8.0f, 14.0f, 100.0f, 100.0f, 18.0f, 28.0f },  // CELESTIAL: slow UP shimmer, light blend
                { 2, true, 30.0f, 45.0f,   8.0f,   0.0f, 22.0f, 38.0f },  // VOIDWALKER: DOWN shift, hollow undertone
                { 3, true, 45.0f, 68.0f,  50.0f,  50.0f, 35.0f, 60.0f },  // THE PARASITE: ROBOT ring modulation
                { 4, true, 25.0f, 58.0f,  75.0f, 100.0f, 30.0f, 52.0f } };// ALTERED STATE: alien UP, moving toward full UP
            for (const auto& m : maps)
                for (float chaos : { 0.0f, 0.5f, 1.0f })
                {
                    GlitchKidProcessor p;
                    setParam (p, "mut_on", 0.0f); setParam (p, "mut_amount", 99.0f); setParam (p, "mut_character", 1.0f); setParam (p, "mut_blend", 1.0f);
                    expectEquals (p.applyAlterEgo (m.ego, chaos, 0), AlterEgos::numActive (m.ego));
                    expectEquals (AlterEgos::numActive (m.ego), AlterEgos::numOwned, "uses all four Mutation values");
                    expectEquals (getParam (p, "mut_on"), 1.0f);
                    expectWithinAbsoluteError (getParam (p, "mut_amount"), m.amount0 + chaos * (m.amount1 - m.amount0), 0.06f);
                    expectWithinAbsoluteError (getParam (p, "mut_character"), m.character0 + chaos * (m.character1 - m.character0), 0.06f);
                    expectWithinAbsoluteError (getParam (p, "mut_blend"), m.blend0 + chaos * (m.blend1 - m.blend0), 0.06f);
                    expect (p.isAlterEgoActive());
                }
            // Distinct identities: the four mappings really are different kinds of transformation.
            expect (VocalMutation::sidebandFor (8.0f) < -0.8 && VocalMutation::sidebandFor (100.0f) > 0.99, "VOIDWALKER is a DOWN shift, CELESTIAL an UP shift");
            expectWithinAbsoluteError (VocalMutation::sidebandFor (50.0f), 0.0, 1.0e-9, "THE PARASITE is ring modulation");
            expect (VocalMutation::carrierHzFor (68.0f) > 250.0 && VocalMutation::carrierHzFor (14.0f) < 15.0, "Parasite robotic (hundreds of Hz), Celestial a slow shimmer (a few Hz)");

            GlitchKidProcessor c;
            setParam (c, "mut_on", 1.0f); setParam (c, "mut_amount", 66.0f); setParam (c, "mut_character", 22.0f); setParam (c, "mut_blend", 77.0f);
            expectEquals (c.applyAlterEgo (1, 1.0f, 0), AlterEgos::numActive (1));
            expectEquals (getParam (c, "mut_on"), 0.0f);
            expectWithinAbsoluteError (getParam (c, "mut_amount"), 66.0f, 0.06f);
            expectWithinAbsoluteError (getParam (c, "mut_character"), 22.0f, 0.06f);
            expectWithinAbsoluteError (getParam (c, "mut_blend"), 77.0f, 0.06f);
            expect (c.isAlterEgoActive(), "the Clone stays 'active' although it leaves three Mutation values alone");
            // Applying another Alter Ego afterwards fully replaces the Mutation identity (no leak from a previous one).
            c.applyAlterEgo (3, 0.0f, 0);
            expectEquals (getParam (c, "mut_on"), 1.0f);
            c.applyAlterEgo (1, 0.0f, 0);
            expectEquals (getParam (c, "mut_on"), 0.0f);
        }

        beginTest ("MOODS + Mutation: Surprise Me never varies Mutation; switching Alter Egos never produces invalid or runaway output; mono, stereo and NaN/Inf stay safe");
        {
            for (int ego = 0; ego < AlterEgos::count; ++ego)
            {
                GlitchKidProcessor a, b;
                a.applyAlterEgo (ego, 0.5f, 0);
                b.applyAlterEgo (ego, 0.5f, 9999);
                for (auto* id : { "mut_on", "mut_amount", "mut_character", "mut_blend" })
                    expectWithinAbsoluteError (getParam (a, id), getParam (b, id), 1.0e-4f, "Surprise leaves " + juce::String (id) + " alone");
            }

            const double sr = 48000.0;
            const Buf voice = harmonicVoice (2, 48000 * 3, sr, 180.0);
            for (int channels : { 1, 2 })
            {
                GlitchKidProcessor p;
                if (channels == 1) setLayout (p, juce::AudioChannelSet::mono());
                p.prepareToPlay (sr, 480);
                juce::MidiBuffer midi;
                bool finite = true;
                float peak = 0.0f;
                for (int blk = 0; blk < 300; ++blk)
                {
                    if (blk % 50 == 0) // switch Alter Ego every 50 blocks, cycling through all five at varied Chaos
                        p.applyAlterEgo ((blk / 50) % AlterEgos::count, (blk % 100) == 0 ? 1.0f : 0.0f, 0);
                    Buf b (channels, 480);
                    for (int ch = 0; ch < channels; ++ch)
                        b.copyFrom (ch, 0, voice, ch, (blk % 290) * 480, 480);
                    if (blk % 37 == 11) b.setSample (0, 100, qNaN);
                    if (blk % 41 == 13) b.setSample (channels - 1, 200, posInf);
                    p.processBlock (b, midi);
                    finite &= allFinite (b);
                    peak = juce::jmax (peak, b.getMagnitude (0, 480));
                }
                expect (finite, "channels " + juce::String (channels));
                expect (peak < 4.0f, "bounded output, peak " + juce::String (peak));
            }
        }

        beginTest ("MOODS + Mutation: the level of every Alter Ego stays within +-1 dB of the input at Chaos 0, 0.5 and 1 (makeup is tuned for the Mutation blend)");
        {
            double worst = 0.0;
            for (int ego = 0; ego < AlterEgos::count; ++ego)
                for (float chaos : { 0.0f, 0.5f, 1.0f })
                {
                    GlitchKidProcessor p;
                    p.prepareToPlay (48000.0, 512);
                    p.applyAlterEgo (ego, chaos, 0);
                    Buf in (2, 48000 * 4);
                    juce::Random rng (60 + ego);
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        double lp = 0.0;
                        for (int i = 0; i < in.getNumSamples(); ++i)
                        {
                            lp += 0.35 * ((rng.nextDouble() * 2.0 - 1.0) - lp);
                            in.setSample (ch, i, (float) lp);
                        }
                        in.applyGain (ch, 0, in.getNumSamples(), 0.1f / in.getRMSLevel (ch, 0, in.getNumSamples()));
                    }
                    Buf out;
                    out.makeCopyOf (in);
                    juce::MidiBuffer midi;
                    for (int start = 0; start < out.getNumSamples(); start += 512)
                    {
                        Buf view (out.getArrayOfWritePointers(), 2, start, juce::jmin (512, out.getNumSamples() - start));
                        p.processBlock (view, midi);
                    }
                    worst = juce::jmax (worst, std::abs (db (rms (out, 0, 48000, 48000 * 3), rms (in, 0, 48000, 48000 * 3))));
                }
            logMessage ("worst level change over 5 Alter Egos x 3 Chaos values: " + juce::String (worst, 2) + " dB");
            expect (worst < 1.0, "level change " + juce::String (worst) + " dB");
        }

        beginTest ("state: Mutation parameters round-trip; an older state without them loads with Mutation defaults");
        {
            GlitchKidProcessor a;
            setParam (a, "mut_on", 1.0f); setParam (a, "mut_amount", 73.0f); setParam (a, "mut_character", 12.0f);
            setParam (a, "mut_blend", 44.0f); setParam (a, "gain", -2.0f);
            juce::MemoryBlock state;
            a.getStateInformation (state);
            GlitchKidProcessor b;
            b.setStateInformation (state.getData(), (int) state.getSize());
            expectEquals (getParam (b, "mut_on"), 1.0f);
            expectWithinAbsoluteError (getParam (b, "mut_amount"), 73.0f, 0.1f);
            expectWithinAbsoluteError (getParam (b, "mut_character"), 12.0f, 0.1f);
            expectWithinAbsoluteError (getParam (b, "mut_blend"), 44.0f, 0.1f);

            if (auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize()))
            {
                for (int i = xml->getNumChildElements() - 1; i >= 0; --i)
                    if (xml->getChildElement (i)->getStringAttribute ("id").startsWith ("mut_"))
                        xml->removeChildElement (xml->getChildElement (i), true);
                juce::MemoryBlock old;
                juce::AudioProcessor::copyXmlToBinary (*xml, old);
                GlitchKidProcessor c;
                setParam (c, "mut_amount", 10.0f);
                c.setStateInformation (old.getData(), (int) old.getSize());
                expectWithinAbsoluteError (getParam (c, "gain"), -2.0f, 0.05f);
                expectEquals (getParam (c, "mut_on"), 0.0f);
                expectWithinAbsoluteError (getParam (c, "mut_amount"), 40.0f, 0.1f);
                expectWithinAbsoluteError (getParam (c, "mut_character"), 50.0f, 0.1f);
                expectWithinAbsoluteError (getParam (c, "mut_blend"), 100.0f, 0.1f);
            }
            else
                expect (false, "could not read saved state");
        }

        beginTest ("real-time safety: no memory allocation while processing (module alone, and inside the plug-in)");
        {
            auto m = make (48000.0, mut (70.0f, 30.0f));
            Buf b = makeNoise (2, 480, 3);
            m.process (b);
            long before = g_allocations.load();
            for (int i = 0; i < 200; ++i)
            {
                m.setSettings (mut (10.0f + (float) (i % 80), (float) (i % 100)));
                m.process (b);
            }
            expectEquals (g_allocations.load() - before, 0L);

            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 480);
            setParam (p, "mut_on", 1.0f);
            setParam (p, "mut_amount", 85.0f);
            setParam (p, "mut_character", 15.0f);
            std::vector<Buf> blocks;
            for (int i = 0; i < 100; ++i)
                blocks.push_back (makeNoise (2, 480, i + 1));
            juce::MidiBuffer midi;
            for (int i = 0; i < 10; ++i)
                p.processBlock (blocks[(size_t) i], midi); // warm-up
            before = g_allocations.load();
            for (size_t i = 10; i < blocks.size(); ++i)
                p.processBlock (blocks[i], midi);
            expectEquals (g_allocations.load() - before, 0L);
        }
    }
};

static MutationTests mutationTests;
