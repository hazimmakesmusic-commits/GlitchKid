#include "TestHelpers.h"
#include "../Source/DSP/GlitchStutter.h"

namespace
{
constexpr double pi = 3.14159265358979323846;

// A play head that supplies (or deliberately does not supply) a tempo.
struct TestPlayHead : juce::AudioPlayHead
{
    juce::Optional<double> bpm;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm (bpm);
        return info;
    }
};

GlitchStutter makeStutter (double sr, StutterSettings s = {})
{
    GlitchStutter g;
    g.prepare (sr);
    g.setSettings (s);
    return g;
}

// Process `in` in blocks of `blockSize`; `change(startSample, stutter)` runs at block boundaries.
template <typename Change>
Buf stream (GlitchStutter& g, Buf in, int blockSize, Change change)
{
    for (int start = 0; start < in.getNumSamples(); start += blockSize)
    {
        change (start, g);
        const int n = juce::jmin (blockSize, in.getNumSamples() - start);
        Buf view (in.getArrayOfWritePointers(), in.getNumChannels(), start, n);
        g.process (view);
    }
    return in;
}

Buf noise (int channels, int n, float amp = 0.5f, juce::int64 seed = 1)
{
    Buf b = makeNoise (channels, n, seed);
    b.applyGain (2.0f * amp); // makeNoise is in [-0.5, 0.5]
    return b;
}

float maxStepOf (const Buf& b, int from, int to)
{
    float m = 0.0f;
    for (int i = from + 1; i < to; ++i)
        m = juce::jmax (m, std::abs (b.getSample (0, i) - b.getSample (0, i - 1)));
    return m;
}

float maxAbs (const Buf& b) { return b.getMagnitude (0, b.getNumSamples()); }
} // namespace

struct StutterTests : public juce::UnitTest
{
    StutterTests() : juce::UnitTest ("Glitch Stutter", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("off by default: a bit-exact copy, no matter the block size");
        {
            auto g = makeStutter (48000.0);
            Buf in = noise (2, 20000), out = stream (g, in, 333, [] (int, GlitchStutter&) {});
            expectEquals (maxAbsDiff (in, out), 0.0f);
            expect (! g.isActive());
        }

        beginTest ("tempo resolution: host tempo is used and clamped; missing or invalid tempo falls back to 120 BPM");
        {
            bool host = false;
            expectEquals (GlitchStutter::resolveTempo (true, 90.0, host), 90.0f);        expect (host);
            expectEquals (GlitchStutter::resolveTempo (true, 30.0, host), 40.0f);        expect (host);   // clamped
            expectEquals (GlitchStutter::resolveTempo (true, 400.0, host), 300.0f);      expect (host);   // clamped
            expectEquals (GlitchStutter::resolveTempo (false, 90.0, host), 120.0f);      expect (! host); // no tempo
            expectEquals (GlitchStutter::resolveTempo (true, 0.0, host), 120.0f);        expect (! host);
            expectEquals (GlitchStutter::resolveTempo (true, -50.0, host), 120.0f);      expect (! host);
            expectEquals (GlitchStutter::resolveTempo (true, std::nan (""), host), 120.0f);  expect (! host);
            expectEquals (GlitchStutter::resolveTempo (true, 1.0e9, host), 120.0f);      expect (! host);
        }

        beginTest ("slice length follows rate and tempo at every sample rate (including the longest case)");
        for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
            for (int rate = 0; rate < GlitchStutter::numRates; ++rate)
                for (float bpm : { 40.0f, 120.0f, 300.0f })
                {
                    auto g = makeStutter (sr, { true, rate, 100.0f, bpm });
                    Buf b = noise (2, 64);
                    g.process (b);
                    const int expected = juce::jlimit (64, (int) std::ceil (1.5 * sr) + 1,
                                                       (int) std::lround (GlitchStutter::rateInBeats (rate) * 60.0 / bpm * sr));
                    expectEquals (g.getLoopLengthSamples(), expected);
                }

        beginTest ("repeats the real incoming audio: the slice that played just before switching on");
        {
            const double sr = 48000.0;
            const int L = 12000, xf = 192, n0 = 30000; // 1/8 note at 120 BPM
            Buf in = noise (2, 80000);
            auto g = makeStutter (sr);
            Buf out = stream (g, in, 500, [&] (int start, GlitchStutter& s) { if (start == n0) s.setSettings ({ true, 1, 100.0f, 120.0f }); });
            expectEquals (g.getLoopLengthSamples(), L);

            for (int ch = 0; ch < 2; ++ch)
            {
                for (int j = 300; j < L - xf; ++j)        // first repeat, before the seam fade
                    expectWithinAbsoluteError (out.getSample (ch, n0 + j), in.getSample (ch, n0 - L + j), 1.0e-5f);
                for (int j = L + 300; j < 2 * L - xf; ++j) // second repeat: the same slice again, not newer audio
                    expectWithinAbsoluteError (out.getSample (ch, n0 + j), in.getSample (ch, n0 - L + (j - L)), 1.0e-5f);
                const int k = xf / 2, p = L - xf + k;      // halfway through the seam crossfade
                const float expectedSeam = 0.5f * in.getSample (ch, n0 - L + p) + 0.5f * in.getSample (ch, n0 - L - xf + k);
                expectWithinAbsoluteError (out.getSample (ch, n0 + p), expectedSeam, 1.0e-5f);
            }
            // before the switch-on the audio is untouched
            for (int i = 0; i < n0; ++i)
                expectEquals (out.getSample (0, i), in.getSample (0, i));
        }

        beginTest ("the loop is the same whatever block size the host uses");
        {
            Buf in = noise (2, 60000), reference;
            bool first = true;
            for (int bs : { 100, 250, 500, 1000, 4000 })
            {
                auto g = makeStutter (48000.0);
                Buf out = stream (g, in, bs, [] (int start, GlitchStutter& s) { if (start == 20000) s.setSettings ({ true, 2, 80.0f, 128.0f }); });
                if (first) { reference = out; first = false; }
                else expectEquals (maxAbsDiff (reference, out), 0.0f);
            }
        }

        beginTest ("switching off returns to the exact live audio");
        {
            Buf in = noise (2, 80000);
            auto g = makeStutter (48000.0);
            Buf out = stream (g, in, 500, [] (int start, GlitchStutter& s)
            {
                if (start == 20000) s.setSettings ({ true, 1, 100.0f, 120.0f });
                if (start == 50000) s.setSettings ({ false, 1, 100.0f, 120.0f });
            });
            for (int i = 50500; i < 80000; ++i)
                expectEquals (out.getSample (1, i), in.getSample (1, i));
            expect (! g.isActive());
        }

        beginTest ("Mix: 0 % is the exact dry signal even when on; 50 % is the average of live and loop");
        {
            Buf in = noise (2, 70000);
            auto dry = makeStutter (48000.0);
            Buf outDry = stream (dry, in, 500, [] (int start, GlitchStutter& s) { if (start == 20000) s.setSettings ({ true, 1, 0.0f, 120.0f }); });
            expectEquals (maxAbsDiff (in, outDry), 0.0f);

            auto full = makeStutter (48000.0), half = makeStutter (48000.0);
            Buf outFull = stream (full, in, 500, [] (int start, GlitchStutter& s) { if (start == 30000) s.setSettings ({ true, 1, 100.0f, 120.0f }); });
            Buf outHalf = stream (half, in, 500, [] (int start, GlitchStutter& s) { if (start == 30000) s.setSettings ({ true, 1, 50.0f, 120.0f }); });
            for (int i = 33000; i < 40000; ++i)
                expectWithinAbsoluteError (outHalf.getSample (0, i), 0.5f * (in.getSample (0, i) + outFull.getSample (0, i)), 1.0e-5f);
        }

        beginTest ("stereo channels repeat their own audio; mono works");
        {
            Buf in = noise (2, 50000, 0.5f, 1);
            in.copyFrom (1, 0, noise (1, 50000, 0.5f, 2), 0, 0, 50000); // different left and right
            auto g = makeStutter (48000.0);
            Buf out = stream (g, in, 500, [] (int start, GlitchStutter& s) { if (start == 20000) s.setSettings ({ true, 1, 100.0f, 120.0f }); });
            for (int j = 400; j < 5000; ++j)
                for (int ch = 0; ch < 2; ++ch)
                    expectWithinAbsoluteError (out.getSample (ch, 20000 + j), in.getSample (ch, 20000 - 12000 + j), 1.0e-5f);

            Buf mono = noise (1, 50000);
            auto gm = makeStutter (48000.0);
            Buf outMono = stream (gm, mono, 500, [] (int start, GlitchStutter& s) { if (start == 20000) s.setSettings ({ true, 1, 100.0f, 120.0f }); });
            expectWithinAbsoluteError (outMono.getSample (0, 21000), mono.getSample (0, 21000 - 12000), 1.0e-5f);
        }

        beginTest ("no clicks: switching on, changing rate, switching off, and the loop seam stay smooth");
        {
            const int n = 192000;
            Buf in (1, n);
            for (int i = 0; i < n; ++i)
                // 437 Hz is deliberately not commensurate with the slice lengths, so loop and live audio are out of phase
                in.setSample (0, i, 0.5f * (float) std::sin (2.0 * pi * 437.0 * i / 48000.0)); // steepest step is 0.029
            auto g = makeStutter (48000.0);
            Buf out = stream (g, in, 500, [] (int start, GlitchStutter& s)
            {
                if (start == 48000)  s.setSettings ({ true, 1, 100.0f, 120.0f });
                if (start == 96000)  s.setSettings ({ true, 2, 100.0f, 120.0f }); // rate change while on
                if (start == 144000) s.setSettings ({ false, 2, 100.0f, 120.0f });
            });
            expect (maxStepOf (out, 0, n) < 0.1f, "max step " + juce::String (maxStepOf (out, 0, n)) + " (a hard cut would be up to 1.0)");
        }

        beginTest ("rate change while on re-captures the slice; a small tempo drift does not");
        {
            Buf in = noise (2, 120000);
            auto g = makeStutter (48000.0);
            int lengthAfterRate = 0, lengthAfterDrift = 0, lengthAfterBigTempo = 0;
            stream (g, in, 500, [&] (int start, GlitchStutter& s)
            {
                if (start == 20000) s.setSettings ({ true, 1, 100.0f, 120.0f });
                if (start == 40000) lengthAfterDrift = 0;
                if (start == 40000) s.setSettings ({ true, 1, 100.0f, 121.0f });  // 0.8 % tempo drift
                if (start == 41000) lengthAfterDrift = g.getLoopLengthSamples();
                if (start == 60000) s.setSettings ({ true, 2, 100.0f, 121.0f });  // rate change
                if (start == 61000) lengthAfterRate = g.getLoopLengthSamples();
                if (start == 80000) s.setSettings ({ true, 2, 100.0f, 130.0f });  // 7 % tempo change
                if (start == 81000) lengthAfterBigTempo = g.getLoopLengthSamples();
            });
            expectEquals (lengthAfterDrift, 12000);                       // unchanged
            expectEquals (lengthAfterRate, (int) std::lround (0.25 * 60.0 / 121.0 * 48000.0));
            expectEquals (lengthAfterBigTempo, (int) std::lround (0.25 * 60.0 / 130.0 * 48000.0));
        }

        beginTest ("no feedback: after a rate change the new slice comes from the live input, never from the loop itself");
        {
            Buf in = noise (2, 100000);
            auto g = makeStutter (48000.0);
            Buf out = stream (g, in, 500, [] (int start, GlitchStutter& s)
            {
                if (start == 20000) s.setSettings ({ true, 1, 100.0f, 120.0f });
                if (start == 60000) s.setSettings ({ true, 2, 100.0f, 120.0f }); // 1/16: 6000 samples
            });
            // The re-capture happens a few ms after the change (after the 3 ms release); find where, then check it.
            bool found = false;
            for (int capture = 60000; capture < 60400 && ! found; ++capture)
            {
                bool ok = true;
                for (int j = 400; j < 1200 && ok; ++j)
                    ok = std::abs (out.getSample (0, capture + j) - in.getSample (0, capture - 6000 + j)) < 1.0e-5f;
                found = ok;
            }
            expect (found, "the re-captured slice is not made of live input");
        }

        beginTest ("never louder than the input, never non-finite, under random on/off, rate, mix and tempo changes");
        for (double sr : { 44100.0, 96000.0 })
        {
            auto g = makeStutter (sr);
            juce::Random rng (5);
            float peak = 0.0f;
            for (int blk = 0; blk < 800; ++blk)
            {
                g.setSettings ({ rng.nextBool(), rng.nextInt (4), rng.nextFloat() * 100.0f, 30.0f + rng.nextFloat() * 300.0f });
                Buf b = noise (2, 256, 1.0f, blk + 1); // full-scale noise
                g.process (b);
                peak = juce::jmax (peak, b.getMagnitude (0, 256), b.getMagnitude (1, 0, 256));
                expect (std::isfinite (peak), "non-finite at block " + juce::String (blk));
            }
            expect (peak <= 1.0f + 1.0e-5f, "peak " + juce::String (peak));
        }

        beginTest ("silence in, silence out");
        {
            auto g = makeStutter (48000.0, { true, 1, 100.0f, 120.0f });
            Buf z (2, 30000);
            z.clear();
            g.process (z);
            expectEquals (maxAbs (z), 0.0f);
        }

        // ---------------- processor-level ----------------

        beginTest ("processor: parameters exist with safe defaults (off, 1/8, 100 %)");
        {
            GlitchKidProcessor p;
            expectEquals (getParam (p, "stutter_on"), 0.0f);
            expectEquals (getParam (p, "stutter_rate"), 1.0f);
            expectEquals (getParam (p, "stutter_mix"), 100.0f);
        }

        beginTest ("processor: tempo comes from the host play head; missing or invalid tempo falls back to 120 BPM");
        for (auto [bpmOpt, lag, fromHost] : { std::tuple<juce::Optional<double>, int, bool> { 90.0, 16000, true },
                                              { juce::nullopt, 12000, false },
                                              { 0.0, 12000, false },
                                              { std::nan (""), 12000, false },
                                              { 1.0e6, 12000, false },
                                              { 30.0, 36000, true } })   // clamped to 40 BPM
        {
            TestPlayHead head;
            head.bpm = bpmOpt;
            GlitchKidProcessor p;
            p.setPlayHead (&head);
            p.prepareToPlay (48000.0, 500);
            setParam (p, "stutter_rate", 1.0f); // 1/8 note

            Buf in = noise (2, 160000), out;
            out.makeCopyOf (in);
            juce::MidiBuffer midi;
            for (int start = 0; start < 160000; start += 500)
            {
                if (start == 60000)
                    setParam (p, "stutter_on", 1.0f);
                Buf view (out.getArrayOfWritePointers(), 2, start, 500);
                p.processBlock (view, midi);
            }
            expectEquals (p.getStutterLoopLength(), lag);
            expect (p.isStutterTempoFromHost() == fromHost);

            // the output must repeat with exactly that period, and not with another one
            float atLag = 0.0f, atWrong = 0.0f;
            const int wrongLag = lag == 12000 ? 16000 : 12000;
            for (int i = 70000; i < 76000; ++i)
            {
                atLag = juce::jmax (atLag, std::abs (out.getSample (0, i + lag) - out.getSample (0, i)));
                atWrong = juce::jmax (atWrong, std::abs (out.getSample (0, i + wrongLag) - out.getSample (0, i)));
            }
            expect (atLag < 1.0e-4f, "not periodic at " + juce::String (lag) + ": " + juce::String (atLag));
            expect (atWrong > 0.1f, "also periodic at the wrong lag " + juce::String (wrongLag));
        }

        beginTest ("processor: block size does not change the result; global bypass is exact; latency unchanged");
        {
            Buf in = noise (2, 96000), reference;
            bool first = true;
            for (int bs : { 64, 500, 3000 })
            {
                GlitchKidProcessor p;
                p.prepareToPlay (48000.0, bs);
                setParam (p, "stutter_rate", 2.0f);
                Buf out;
                out.makeCopyOf (in);
                juce::MidiBuffer midi;
                for (int start = 0; start < 96000; start += bs)
                {
                    if (start == 48000)
                        setParam (p, "stutter_on", 1.0f);
                    const int n = juce::jmin (bs, 96000 - start);
                    Buf view (out.getArrayOfWritePointers(), 2, start, n);
                    p.processBlock (view, midi);
                }
                if (first) { reference = out; first = false; }
                else expectEquals (maxAbsDiff (reference, out), 0.0f);
                expectEquals (p.getLatencySamples(), (int) pluginLatency);
            }

            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 500);
            setParam (p, "stutter_on", 1.0f);
            setParam (p, "bypass", 1.0f);
            Buf out;
            out.makeCopyOf (in);
            juce::MidiBuffer midi;
            for (int start = 0; start < 96000; start += 500)
            {
                Buf view (out.getArrayOfWritePointers(), 2, start, 500);
                p.processBlock (view, midi);
            }
            for (int i = 20000; i < 96000; ++i)
                expectEquals (out.getSample (1, i), in.getSample (1, i - pluginLatency));
        }

        beginTest ("processor: state round-trips; mono layout works; stutter is off and exact by default");
        {
            GlitchKidProcessor p;
            setParam (p, "stutter_on", 1.0f);
            setParam (p, "stutter_rate", 3.0f);
            setParam (p, "stutter_mix", 42.5f);
            juce::MemoryBlock state;
            p.getStateInformation (state);
            GlitchKidProcessor q;
            q.setStateInformation (state.getData(), (int) state.getSize());
            expectEquals (getParam (q, "stutter_on"), 1.0f);
            expectEquals (getParam (q, "stutter_rate"), 3.0f);
            expectWithinAbsoluteError (getParam (q, "stutter_mix"), 42.5f, 0.06f);

            GlitchKidProcessor m;
            setLayout (m, juce::AudioChannelSet::mono());
            m.prepareToPlay (48000.0, 500);
            setParam (m, "stutter_on", 1.0f);
            Buf in = noise (1, 60000);
            juce::MidiBuffer midi;
            for (int start = 0; start < 60000; start += 500)
            {
                Buf view (in.getArrayOfWritePointers(), 1, start, 500);
                m.processBlock (view, midi);
            }
            expect (std::isfinite (maxAbs (in)));

            GlitchKidProcessor d;
            d.prepareToPlay (48000.0, 512);
            Buf a, b;
            run (d, 2, 512, 20, a, b);
            expectEquals (maxAbsDiff (a, b), 0.0f);
        }
    }
};

static StutterTests stutterTests;
