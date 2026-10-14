#include "TestHelpers.h"
#include "../Source/DSP/Saturation.h"
#include "../Source/DSP/FixedDelay.h"

namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr double sr = 48000.0;
constexpr int second = 48000;

Buf sine (double freq, float amp, int n, int channels = 1)
{
    Buf b (channels, n);
    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < n; ++i)
            b.setSample (ch, i, amp * (float) std::sin (2.0 * pi * freq * i / sr));
    return b;
}

// Power (amplitude^2) of the component at `freq` over the second after `skip` samples
// (freq must be a whole number of Hz, so the window holds whole cycles).
double powerAt (const Buf& b, int skip, double freq)
{
    double re = 0, im = 0;
    for (int i = 0; i < second; ++i)
    {
        const double w = 2.0 * pi * freq * (skip + i) / sr;
        re += b.getSample (0, skip + i) * std::cos (w);
        im += b.getSample (0, skip + i) * std::sin (w);
    }
    const double amp = 2.0 * std::sqrt (re * re + im * im) / second;
    return amp * amp;
}

double db (double power) { return 10.0 * std::log10 (juce::jmax (power, 1.0e-30)); }

// Process a whole buffer in blocks of 512 with the given drive (already settled: drive is set before prepare).
Buf run (float driveDb, Buf in, int blockSize = 512)
{
    Saturation s;
    s.setDriveDb (driveDb);
    s.prepare (sr, blockSize);
    s.process (in);
    return in;
}

// Same, but with the drive smoother already settled on the target: 1 s of warm-up is dropped by callers.
float peakOf (const Buf& b, int from, int to)
{
    float p = 0;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        p = juce::jmax (p, b.getMagnitude (ch, from, to - from));
    return p;
}
} // namespace

struct FixedDelayTests : public juce::UnitTest
{
    FixedDelayTests() : juce::UnitTest ("Fixed delay", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("delays by exactly N samples across blocks of any size");
        for (int delay : { 0, 1, 6, 37 })
        {
            FixedDelay d;
            d.prepare (delay);
            Buf in = makeNoise (2, 5000), out;
            out.makeCopyOf (in);
            int pos = 0;
            for (int bs : { 1, 7, 64, 333, 1024, 3571 })
            {
                const int n = juce::jmin (bs, 5000 - pos);
                if (n <= 0)
                    break;
                Buf view (out.getArrayOfWritePointers(), 2, pos, n);
                d.process (view);
                pos += n;
            }
            for (int i = 0; i < pos; ++i)
                for (int ch = 0; ch < 2; ++ch)
                    expectEquals (out.getSample (ch, i), i >= delay ? in.getSample (ch, i - delay) : 0.0f);
        }
    }
};

struct SaturationTests : public juce::UnitTest
{
    SaturationTests() : juce::UnitTest ("Saturation", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("drive 0: exact copy of the input, delayed by the reported latency");
        {
            Buf in = makeNoise (2, 8192), out = run (0.0f, in);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 8192; ++i)
                    expectEquals (out.getSample (ch, i), i >= Saturation::latencySamples ? in.getSample (ch, i - Saturation::latencySamples) : 0.0f);
        }

        beginTest ("the oversampled path has the same latency as the idle path");
        {
            // Nearly-linear drive (0.1 dB): the output must line up with the input delayed by exactly the latency.
            Buf in = sine (200.0, 0.3f, 2 * second), out = run (0.1f, in);
            int bestShift = -1;
            double best = 1.0e9;
            for (int shift = 0; shift < 16; ++shift)
            {
                double e = 0;
                for (int i = second; i < second + 4800; ++i)
                    e += std::pow (out.getSample (0, i) - in.getSample (0, i - shift), 2.0);
                if (e < best) { best = e; bestShift = shift; }
            }
            expectEquals (bestShift, (int) Saturation::latencySamples);
        }

        beginTest ("small signals keep gain k/tanh(k), at low and high frequency (no treble dulling)");
        for (float drive : { 3.0f, 6.0f, 12.0f, 18.0f })
            for (double freq : { 500.0, 15000.0 })
            {
                const double k = std::pow (10.0, drive / 20.0) - 1.0;
                Buf out = run (drive, sine (freq, 0.001f, 2 * second));
                expectWithinAbsoluteError ((float) (db (powerAt (out, second, freq)) - db (1.0e-6)),
                                           (float) (20.0 * std::log10 (k / std::tanh (k))), 0.15f);
            }

        beginTest ("full scale stays near full scale (the oversampling filters may overshoot a little)");
        for (float drive : { 6.0f, 12.0f, 18.0f })
        {
            Buf out = run (drive, sine (100.0, 1.0f, 2 * second));
            const float peak = peakOf (out, second, 2 * second);
            expect (peak > 0.97f && peak < 1.35f, "peak " + juce::String (peak) + " at " + juce::String (drive) + " dB");
        }

        beginTest ("adds odd harmonics only, growing with drive");
        {
            auto h = [&] (float drive, double harmonic)
            {
                Buf out = run (drive, sine (1000.0, 0.5f, 2 * second));
                return db (powerAt (out, second, 1000.0 * harmonic)) - db (powerAt (out, second, 1000.0));
            };
            const float h3at12 = (float) h (12.0f, 3);
            expect (h3at12 > h (6.0f, 3) && h (6.0f, 3) > h (1.0f, 3));
            expect (h3at12 > -40.0f, "3rd harmonic " + juce::String (h3at12) + " dB");
            expect (h (12.0f, 2) < h3at12 - 40.0f, "2nd harmonic should be absent (odd-symmetric curve)");
        }

        beginTest ("anti-aliasing: folded-back harmonics are far below plain tanh");
        {
            // 7 kHz at 48 kHz: the 5th harmonic (35 kHz) folds to 13 kHz and the 7th (49 kHz) to 1 kHz.
            const float drive = 18.0f;
            const double k = std::pow (10.0, drive / 20.0) - 1.0;
            Buf in = sine (7000.0, 0.8f, 2 * second);
            Buf os = run (drive, in);
            Buf naive;
            naive.makeCopyOf (in);
            for (int i = 0; i < 2 * second; ++i)
                naive.setSample (0, i, (float) (std::tanh (k * in.getSample (0, i)) / std::tanh (k)));

            const double alias = powerAt (os, second, 13000.0) + powerAt (os, second, 1000.0);
            const double reference = powerAt (naive, second, 13000.0) + powerAt (naive, second, 1000.0);
            logMessage ("alias power: plain tanh " + juce::String (db (reference)) + " dB, oversampled " + juce::String (db (alias)) + " dB");
            expect (db (alias) < db (reference) - 15.0, "alias reduction only " + juce::String (db (reference) - db (alias)) + " dB");
        }

        beginTest ("stereo channels are independent and identical for identical input; mono matches");
        {
            Buf st = run (12.0f, sine (1000.0, 0.7f, 8192, 2));
            for (int i = 0; i < 8192; ++i)
                expectEquals (st.getSample (0, i), st.getSample (1, i));
            Buf mono = run (12.0f, sine (1000.0, 0.7f, 8192, 1));
            expectWithinAbsoluteError (mono.getSample (0, 8000), st.getSample (0, 8000), 1.0e-6f);
        }

        beginTest ("any block size gives the same result");
        {
            Buf in = sine (1000.0, 0.7f, 8192, 2);
            Buf a = run (12.0f, in, 512), b = run (12.0f, in, 64), c = run (12.0f, in, 3000);
            expectEquals (maxAbsDiff (a, b), 0.0f);
            expectEquals (maxAbsDiff (a, c), 0.0f);
        }

        beginTest ("silence stays silent; random drive changes on loud noise stay finite and bounded");
        {
            Buf z (2, 4096);
            z.clear();
            Buf out = run (18.0f, z);
            expectEquals (out.getMagnitude (0, 4096), 0.0f);

            Saturation s;
            s.prepare (sr, 256);
            juce::Random rng (11);
            float peak = 0.0f;
            for (int blk = 0; blk < 400; ++blk)
            {
                s.setDriveDb (rng.nextFloat() * 18.0f);
                Buf b = makeNoise (2, 256, blk);
                b.applyGain (2.0f); // [-1, 1]
                s.process (b);
                peak = juce::jmax (peak, b.getMagnitude (0, 256));
                expect (std::isfinite (peak), "non-finite at block " + juce::String (blk));
            }
            expect (peak < 2.5f, "peak " + juce::String (peak));
        }

        beginTest ("switching drive on is faded in (no click), and switching it off returns to the exact copy");
        {
            Saturation s;
            s.prepare (sr, 512);
            Buf first = sine (200.0, 0.3f, 4800), more = sine (200.0, 0.3f, 4800);
            s.process (first);
            s.setDriveDb (18.0f);
            s.process (more);
            float maxStep = 0.0f;
            for (int i = 1; i < 4800; ++i)
                maxStep = juce::jmax (maxStep, std::abs (more.getSample (0, i) - more.getSample (0, i - 1)));
            expect (maxStep < 0.1f, "max step " + juce::String (maxStep));

            s.setDriveDb (0.0f);
            Buf tail = makeNoise (2, 8192), expected;
            expected.makeCopyOf (tail);
            Buf settle = makeNoise (2, 4800);
            s.process (settle); // 100 ms: the fade-out finishes
            Buf probe = makeNoise (2, 2048, 5), probeIn;
            probeIn.makeCopyOf (probe);
            Buf pre = makeNoise (2, 64, 9), preIn;
            preIn.makeCopyOf (pre);
            s.process (pre);
            s.process (probe);
            for (int i = 0; i < 2048; ++i)
                expectEquals (probe.getSample (0, i), i >= Saturation::latencySamples ? probeIn.getSample (0, i - Saturation::latencySamples)
                                                                                         : preIn.getSample (0, 64 - Saturation::latencySamples + i));
        }

        beginTest ("processor: reports the latency, saturation is in the path, bypass is exact, state round-trips");
        {
            GlitchKidProcessor p;
            expectEquals (p.getLatencySamples(), (int) Saturation::latencySamples);
            p.prepareToPlay (48000.0, 512);
            setParam (p, "sat_drive", 12.0f);
            Buf in (2, 512), out;
            juce::MidiBuffer midi;
            for (int blk = 0; blk < 60; ++blk)
            {
                for (int i = 0; i < 512; ++i)
                {
                    const float v = 0.5f * (float) std::sin (2.0 * pi * 1000.0 * (blk * 512 + i) / 48000.0);
                    in.setSample (0, i, v); in.setSample (1, i, v);
                }
                out.makeCopyOf (in);
                p.processBlock (out, midi);
            }
            expect (maxAbsDiff (in, out) > 0.05f, "saturation should change the sound");

            setParam (p, "bypass", 1.0f);
            for (int blk = 0; blk < 8; ++blk) { out.makeCopyOf (in); p.processBlock (out, midi); }
            expect (rotatedExactly (in, out, Saturation::latencySamples));

            juce::MemoryBlock state;
            p.getStateInformation (state);
            GlitchKidProcessor q;
            q.setStateInformation (state.getData(), (int) state.getSize());
            expectWithinAbsoluteError (getParam (q, "sat_drive"), 12.0f, 0.06f);

            GlitchKidProcessor fresh;
            expectEquals (getParam (fresh, "sat_drive"), 0.0f);
        }
    }
};

static FixedDelayTests fixedDelayTests;
static SaturationTests saturationTests;
