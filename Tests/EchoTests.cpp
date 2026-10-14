#include "TestHelpers.h"
#include "../Source/DSP/LiquidEcho.h"
#include "../Source/DSP/GlitchStutter.h"

namespace
{
constexpr float qNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float posInf = std::numeric_limits<float>::infinity();

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

// Free-time, wet-only, no feedback, bright tone: the simplest echo to reason about.
EchoSettings basic (float ms = 100.0f)
{
    EchoSettings s;
    s.on = true; s.sync = false; s.timeMs = ms;
    s.feedbackPct = 0.0f; s.toneHz = 20000.0f; s.widthPct = 0.0f; s.mixPct = 100.0f;
    return s;
}

LiquidEcho make (double sr, const EchoSettings& s)
{
    LiquidEcho e;
    e.setSettings (s);
    e.prepare (sr);
    return e;
}

Buf silent (int channels, int n)
{
    Buf b (channels, n);
    b.clear();
    return b;
}

Buf impulse (int channels, int n, int at = 0, float amp = 1.0f)
{
    Buf b (channels, n);
    b.clear();
    for (int ch = 0; ch < channels; ++ch)
        b.setSample (ch, at, amp);
    return b;
}

Buf sine (int channels, int n, double sr, float freq = 220.0f, float amp = 0.5f)
{
    Buf b (channels, n);
    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < n; ++i)
            b.setSample (ch, i, amp * std::sin (juce::MathConstants<float>::twoPi * freq * (float) i / (float) sr));
    return b;
}

// Process in blocks; `before(startSample, echo)` runs before each block (to change settings).
template <typename F>
Buf stream (LiquidEcho& e, Buf in, int blockSize, F before)
{
    for (int start = 0; start < in.getNumSamples(); start += blockSize)
    {
        before (start, e);
        Buf view (in.getArrayOfWritePointers(), in.getNumChannels(), start, juce::jmin (blockSize, in.getNumSamples() - start));
        e.process (view);
    }
    return in;
}
Buf stream (LiquidEcho& e, Buf in, int blockSize = 512) { return stream (e, std::move (in), blockSize, [] (int, LiquidEcho&) {}); }

bool allFinite (const Buf& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (ch, i)))
                return false;
    return true;
}

int peakIndex (const Buf& b, int ch, int from = 0, int to = -1)
{
    if (to < 0) to = b.getNumSamples();
    int best = from;
    for (int i = from; i < to; ++i)
        if (std::abs (b.getSample (ch, i)) > std::abs (b.getSample (ch, best)))
            best = i;
    return best;
}

bool exactlyZero (const Buf& b, int ch, int from, int to)
{
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

// First-difference RMS: a crude but monotonic measure of high-frequency content.
float brightness (const Buf& b, int from, int to)
{
    double s = 0.0;
    for (int i = from + 1; i < to; ++i)
    {
        const double d = b.getSample (0, i) - b.getSample (0, i - 1);
        s += d * d;
    }
    return (float) std::sqrt (s / (to - from));
}

// Processor helpers (the whole plug-in path; adds the 6-sample latency).
void stream (GlitchKidProcessor& p, Buf& in, int blockSize = 512)
{
    juce::MidiBuffer midi;
    for (int start = 0; start < in.getNumSamples(); start += blockSize)
    {
        Buf view (in.getArrayOfWritePointers(), in.getNumChannels(), start, juce::jmin (blockSize, in.getNumSamples() - start));
        p.processBlock (view, midi);
    }
}

void setEchoFree (GlitchKidProcessor& p, float ms, float mix = 100.0f)
{
    setParam (p, "echo_on", 1.0f);
    setParam (p, "echo_sync", 0.0f);
    setParam (p, "echo_time", ms);
    setParam (p, "echo_fb", 0.0f);
    setParam (p, "echo_tone", 20000.0f);
    setParam (p, "echo_mix", mix);
}
} // namespace

struct EchoTests : public juce::UnitTest
{
    EchoTests() : juce::UnitTest ("Liquid Echo", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("silence in gives exact, finite silence out (echo on, feedback on, mono and stereo)");
        for (int ch : { 1, 2 })
        {
            auto s = basic();
            s.feedbackPct = 60.0f;
            auto e = make (48000.0, s);
            Buf out = stream (e, silent (ch, 48000));
            out = stream (e, out);
            expect (allFinite (out) && exactlyZero (out, 0, 0, out.getNumSamples()), "channels " + juce::String (ch));
        }

        beginTest ("an impulse comes back after exactly the set time (wet only), at several sample rates and block sizes");
        for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
            for (int bs : { 1, 64, 2048 })
            {
                auto e = make (sr, basic (100.0f));
                const int t = (int) std::lround (0.1 * sr);
                Buf out = stream (e, impulse (2, t + 2000), bs);
                expectEquals (peakIndex (out, 0), t, "sr " + juce::String (sr) + " block " + juce::String (bs));
                expectEquals (peakIndex (out, 1), t);
                expect (exactlyZero (out, 0, 0, t), "nothing before the delay (wet only)");
                expect (std::abs (out.getSample (0, t)) > 0.4f, "echo has body"); // a 20 kHz low-pass at 192 kHz lowers an impulse to ~0.48
            }

        beginTest ("tempo sync: divisions give the right delay length at the given tempo");
        {
            EchoSettings s;
            s.sync = true;
            s.bpm = 120.0f; s.division = 4;      expectEquals (LiquidEcho::delaySamplesFor (s, 48000.0), 12000.0);  // 1/8
            s.division = 1;                      expectEquals (LiquidEcho::delaySamplesFor (s, 48000.0), 24000.0);  // 1/4
            s.division = 2;  s.bpm = 90.0f;      expectEquals (LiquidEcho::delaySamplesFor (s, 48000.0), 48000.0);  // 1/4 dotted at 90
            s.division = 6;  s.bpm = 120.0f;     expectWithinAbsoluteError (LiquidEcho::delaySamplesFor (s, 48000.0), 8000.0, 1.0e-6); // 1/8 triplet
            s.division = 0;  s.bpm = 40.0f;      expectEquals (LiquidEcho::delaySamplesFor (s, 48000.0), 144000.0); // longest: 3 s
            s.bpm = 1.0f;                        expectEquals (LiquidEcho::delaySamplesFor (s, 48000.0), 144000.0); // clamped to the buffer
            s.sync = false; s.timeMs = 250.0f;   expectEquals (LiquidEcho::delaySamplesFor (s, 48000.0), 12000.0);
            auto sy = basic(); sy.sync = true; sy.division = 4; sy.bpm = 120.0f;
            auto e = make (48000.0, sy);
            expectEquals (peakIndex (stream (e, impulse (1, 14000)), 0), 12000);
        }

        beginTest ("feedback makes repeats that decay; max feedback stays stable and gives no DC build-up");
        {
            auto s = basic (100.0f);
            s.feedbackPct = 50.0f;
            auto e = make (48000.0, s);
            Buf out = stream (e, impulse (1, 4800 * 24 + 100));
            float prev = 1.0e9f;
            for (int k = 1; k <= 6; ++k)
            {
                const float pk = std::abs (out.getSample (0, peakIndex (out, 0, 4800 * k - 3, 4800 * k + 3)));
                expect (pk < prev && pk > 0.0f, "repeat " + juce::String (k) + " is quieter than the one before");
                prev = pk;
            }
            const float r = std::abs (out.getSample (0, 4800 * 2)) / std::abs (out.getSample (0, 4800));
            expect (r > 0.3f && r < 0.55f, "second repeat about half the first, got " + juce::String (r));
            expect (out.getMagnitude (0, 4800 * 20, 4800 * 4) < 1.0e-3f, "decayed after 20 repeats");

            s.feedbackPct = 500.0f; // out of range: must be limited to 90 %
            auto m = make (48000.0, s);
            Buf noise = makeNoise (1, 48000 * 3, 5);
            noise.applyGain (1.0f);
            Buf out2 = stream (m, noise);
            expect (allFinite (out2) && out2.getMagnitude (0, 0, out2.getNumSamples()) < 10.0f, "bounded with loud noise at max feedback");
            Buf silence (1, 48000 * 40);
            silence.clear();
            Buf tail = stream (m, silence);
            expect (allFinite (tail), "finite");
            expect (tail.getMagnitude (0, tail.getNumSamples() - 4800, 4800) < 1.0e-6f, "max-feedback tail dies away");

            Buf dc (1, 48000 * 6);
            for (int i = 0; i < dc.getNumSamples(); ++i) dc.setSample (0, i, 0.5f);
            auto d = make (48000.0, s);
            Buf od = stream (d, dc);
            double mean = 0.0;
            for (int i = od.getNumSamples() - 48000; i < od.getNumSamples(); ++i) mean += od.getSample (0, i);
            expect (std::abs (mean / 48000.0) < 1.0e-3, "no DC build-up in the repeats (mean " + juce::String (mean / 48000.0) + ")");
        }

        beginTest ("Mix 0 keeps the dry signal bit-exact while the echo runs; Mix 100 is wet only; Mix 50 is a crossfade");
        {
            Buf in = makeNoise (2, 20000, 3);
            auto s = basic (100.0f);
            s.feedbackPct = 40.0f;
            s.mixPct = 0.0f;
            auto e = make (48000.0, s);
            expectEquals (maxAbsDiff (in, stream (e, in)), 0.0f);

            s.mixPct = 100.0f;
            auto w = make (48000.0, s);
            expect (exactlyZero (stream (w, in), 0, 0, 4800), "no dry sound at 100 %");

            s.mixPct = 50.0f;
            auto h = make (48000.0, s);
            Buf out = stream (h, in);
            float err = 0.0f;
            for (int i = 0; i < 4800; ++i)
                err = juce::jmax (err, std::abs (out.getSample (0, i) - 0.5f * in.getSample (0, i)));
            expect (err < 1.0e-7f, "dry half before the first echo");
        }

        beginTest ("Tone darkens the repeats and leaves the dry signal alone");
        {
            Buf in = makeNoise (1, 30000, 9);
            Buf wetBright, wetDark, mixBright, mixDark;
            for (float hz : { 20000.0f, 500.0f })
                for (float mix : { 100.0f, 50.0f })
                {
                    auto s = basic (100.0f);
                    s.mixPct = mix; s.toneHz = hz;
                    auto e = make (48000.0, s);
                    (hz > 1000.0f ? (mix > 75.0f ? wetBright : mixBright) : (mix > 75.0f ? wetDark : mixDark)) = stream (e, in);
                }
            expect (brightness (wetBright, 6000, 30000) > 1.5f * brightness (wetDark, 6000, 30000), "low tone is much darker");
            expectEquals (maxAbsDiff (Buf (mixBright.getArrayOfWritePointers(), 1, 0, 4800), Buf (mixDark.getArrayOfWritePointers(), 1, 0, 4800)), 0.0f);
        }

        beginTest ("Width offsets the right echo (1 + 0.5 x width) without changing its level; mono ignores it");
        {
            for (float width : { 0.0f, 50.0f, 100.0f })
            {
                auto s = basic (100.0f);
                s.widthPct = width;
                auto e = make (48000.0, s);
                Buf out = stream (e, impulse (2, 9000));
                const int right = 4800 + (int) std::lround (4800 * 0.5 * width * 0.01);
                expectEquals (peakIndex (out, 0), 4800, "left, width " + juce::String (width));
                expectEquals (peakIndex (out, 1), right, "right, width " + juce::String (width));
                expectWithinAbsoluteError (out.getSample (1, right), out.getSample (0, 4800), 1.0e-6f);
            }
            auto s = basic (100.0f);
            s.widthPct = 100.0f;
            auto e = make (48000.0, s);
            Buf mono = stream (e, impulse (1, 9000));
            expectEquals (peakIndex (mono, 0), 4800);
            expect (allFinite (mono));
        }

        beginTest ("block size does not change the result (bit-exact), at several sample rates");
        for (double sr : { 44100.0, 96000.0 })
        {
            auto s = basic (123.0f);
            s.feedbackPct = 55.0f; s.toneHz = 3000.0f; s.widthPct = 40.0f; s.mixPct = 45.0f;
            Buf in = makeNoise (2, (int) sr, 11), ref;
            for (int bs : { 1, 37, 512, 2048 })
            {
                auto e = make (sr, s);
                Buf out = stream (e, in, bs);
                if (bs == 1) ref = out;
                else expectEquals (maxAbsDiff (ref, out), 0.0f, "sr " + juce::String (sr) + " block " + juce::String (bs));
            }
        }

        beginTest ("changing the time during playback glides: no clicks, finite, and the new time is reached");
        {
            auto s = basic (100.0f);
            s.feedbackPct = 40.0f; s.mixPct = 50.0f;
            auto e = make (48000.0, s);
            const float times[] = { 100.0f, 130.0f, 90.0f, 140.0f, 100.0f }; // moderate moves: a hard switch would step by ~0.5
            Buf out = stream (e, sine (2, 48000 * 5, 48000.0), 512, [&] (int start, LiquidEcho& echo)
            {
                auto t = s;
                t.timeMs = times[juce::jmin (4, start / 48000)];
                echo.setSettings (t);
            });
            expect (allFinite (out));
            expect (maxStep (out, 100) < 0.06f, "largest sample-to-sample step " + juce::String (maxStep (out, 100)));

            // Extreme jumps bend the pitch of the repeats (by design, 80 ms glide) but stay finite and bounded.
            auto x = make (48000.0, s);
            const float wild[] = { 100.0f, 1500.0f, 10.0f, 2000.0f, 1.0f };
            Buf wildOut = stream (x, sine (2, 48000 * 5, 48000.0), 512, [&] (int start, LiquidEcho& echo)
            {
                auto t = s;
                t.timeMs = wild[juce::jmin (4, start / 48000)];
                echo.setSettings (t);
            });
            expect (allFinite (wildOut) && wildOut.getMagnitude (0, wildOut.getNumSamples()) < 3.0f);

            auto f = make (48000.0, basic (100.0f));
            Buf silence (1, 48000);
            silence.clear();
            stream (f, silence);
            auto t = basic (200.0f);
            f.setSettings (t);
            Buf wait (1, 24000);
            wait.clear();
            stream (f, wait);   // let the 80 ms glide finish
            Buf probe = stream (f, impulse (1, 12000));
            expectEquals (peakIndex (probe, 0), 9600);
        }

        beginTest ("switching on and off during playback is click-free; off is bit-exact and leaves no stale echo");
        {
            auto s = basic (100.0f);
            s.feedbackPct = 50.0f; s.mixPct = 60.0f; s.on = false;
            auto e = make (48000.0, s);
            Buf in = sine (1, 48000 * 4, 48000.0);
            Buf out = stream (e, in, 480, [&] (int start, LiquidEcho& echo)
            {
                auto t = s;
                t.on = (start / 48000) % 2 == 1; // off, on, off, on
                echo.setSettings (t);
            });
            expect (allFinite (out));
            expect (maxStep (out, 1) < 0.1f, "largest step " + juce::String (maxStep (out)));
            expectEquals (maxAbsDiff (Buf (in.getArrayOfWritePointers(), 1, 0, 48000), Buf (out.getArrayOfWritePointers(), 1, 0, 48000)), 0.0f);

            // After a settled off period the line was cleared: a re-enable with silent input is exactly silent.
            auto g = make (48000.0, s);
            auto on = s; on.on = true;
            g.setSettings (on);
            stream (g, sine (1, 20000, 48000.0));
            g.setSettings (s);
            Buf quiet (1, 3000);
            quiet.clear();
            stream (g, quiet);
            expect (! g.isActive(), "idle once the fade has finished");
            g.setSettings (on);
            expect (exactlyZero (stream (g, silent (1, 20000)), 0, 0, 20000), "no stale echo after re-enable");
        }

        beginTest ("NaN/Inf input: finite output, equivalent to a zero sample, nothing stays poisoned; huge values recover");
        {
            auto s = basic (50.0f);
            s.feedbackPct = 70.0f; s.mixPct = 80.0f;
            Buf a = makeNoise (2, 24000, 4), b;
            b.makeCopyOf (a);
            const float bad[3] = { qNaN, posInf, -posInf };
            for (int ch = 0; ch < 2; ++ch)
                for (int k = 0; k < 3; ++k)
                {
                    a.setSample (ch, 3000 * (k + 1) + ch, bad[k]);
                    b.setSample (ch, 3000 * (k + 1) + ch, 0.0f);
                }
            auto ea = make (48000.0, s), eb = make (48000.0, s);
            Buf oa = stream (ea, a), ob = stream (eb, b);
            expect (allFinite (oa));
            expectEquals (maxAbsDiff (oa, ob), 0.0f);

            auto h = make (48000.0, s), fresh = make (48000.0, s);
            Buf burst (2, 2000);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 2000; ++i)
                    burst.setSample (ch, i, (i & 1) ? -3.0e38f : 3.0e38f);
            expect (allFinite (stream (h, burst)), "finite during an overflow burst");
            Buf clean = makeNoise (2, 20000, 6);
            Buf oh = stream (h, clean), of = stream (fresh, clean);
            expect (allFinite (oh));
            expect (maxAbsDiff (oh, of) < 1.0e-6f, "after the burst the echo behaves like a fresh one");
        }

        beginTest ("processor: Liquid Echo is off by default, adds no latency, and sits after Stutter in the chain");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 512);
            expectEquals (p.getLatencySamples(), Saturation::latencySamples);
            expectEquals (getParam (p, "echo_on"), 0.0f);
            setEchoFree (p, 100.0f);
            Buf settle = silent (2, 2048); // the 20 ms switch-on fade
            stream (p, settle);
            Buf in = impulse (2, 6000, 0, 0.5f);
            stream (p, in);
            expectEquals (peakIndex (in, 0), Saturation::latencySamples + 4800);
            expectEquals (p.getLatencySamples(), Saturation::latencySamples);
        }

        beginTest ("processor: host tempo sets the synced delay; no/invalid tempo falls back to 120 BPM; tempo changes are followed");
        {
            struct Case { bool has; double bpm; int expected; };
            for (auto c : { Case { true, 120.0, 24000 }, Case { true, 60.0, 48000 }, Case { false, 0.0, 24000 },
                            Case { true, 0.0, 24000 }, Case { true, -50.0, 24000 }, Case { true, (double) qNaN, 24000 } })
            {
                GlitchKidProcessor p;
                TestPlayHead ph;
                if (c.has) ph.bpm = c.bpm;
                p.setPlayHead (&ph);
                p.prepareToPlay (48000.0, 512);
                setParam (p, "echo_on", 1.0f); setParam (p, "echo_div", 1.0f); // 1/4, synced
                setParam (p, "echo_fb", 0.0f); setParam (p, "echo_tone", 20000.0f); setParam (p, "echo_mix", 100.0f);
                Buf settle = silent (2, 2048);
                stream (p, settle);
                Buf in = impulse (2, 50000 + 600, 0, 0.5f);
                stream (p, in);
                expectEquals (peakIndex (in, 0), Saturation::latencySamples + c.expected,
                              "bpm " + juce::String (c.bpm) + (c.has ? "" : " (no host)"));
            }

            GlitchKidProcessor p;
            TestPlayHead ph;
            ph.bpm = 120.0;
            p.setPlayHead (&ph);
            p.prepareToPlay (48000.0, 512);
            setParam (p, "echo_on", 1.0f); setParam (p, "echo_div", 1.0f);
            setParam (p, "echo_fb", 0.0f); setParam (p, "echo_tone", 20000.0f); setParam (p, "echo_mix", 100.0f);
            Buf warm = silent (2, 30000); stream (p, warm);
            ph.bpm = 60.0;
            Buf settle = silent (2, 30000); stream (p, settle); // > the 80 ms glide
            Buf in = impulse (2, 50000, 0, 0.5f);
            stream (p, in);
            expectEquals (peakIndex (in, 0), Saturation::latencySamples + 48000);
        }

        beginTest ("processor: mono layout works; echo parameters are automatable (changed every block) and stay finite");
        {
            GlitchKidProcessor p;
            setLayout (p, juce::AudioChannelSet::mono());
            p.prepareToPlay (48000.0, 256);
            setEchoFree (p, 100.0f, 40.0f);
            setParam (p, "echo_fb", 60.0f);
            juce::Random rng (3);
            bool finite = true;
            juce::MidiBuffer midi;
            for (int blk = 0; blk < 200; ++blk)
            {
                setParam (p, "echo_time", 20.0f + 900.0f * rng.nextFloat());
                setParam (p, "echo_tone", 400.0f + 19000.0f * rng.nextFloat());
                setParam (p, "echo_fb", 90.0f * rng.nextFloat());
                setParam (p, "echo_width", 100.0f * rng.nextFloat());
                setParam (p, "echo_mix", 100.0f * rng.nextFloat());
                setParam (p, "echo_sync", rng.nextBool() ? 1.0f : 0.0f);
                setParam (p, "echo_div", (float) rng.nextInt (LiquidEcho::numDivisions));
                setParam (p, "echo_on", rng.nextInt (5) == 0 ? 0.0f : 1.0f);
                Buf b = makeNoise (1, 256, blk);
                p.processBlock (b, midi);
                finite &= allFinite (b);
            }
            expect (finite);
        }

        beginTest ("state: echo parameters round-trip; an older state without them loads with echo defaults");
        {
            GlitchKidProcessor a;
            setParam (a, "echo_on", 1.0f); setParam (a, "echo_sync", 0.0f); setParam (a, "echo_time", 420.0f);
            setParam (a, "echo_div", 7.0f); setParam (a, "echo_fb", 62.0f); setParam (a, "echo_tone", 2500.0f);
            setParam (a, "echo_width", 80.0f); setParam (a, "echo_mix", 55.0f); setParam (a, "gain", -3.0f);
            juce::MemoryBlock state;
            a.getStateInformation (state);
            GlitchKidProcessor b;
            b.setStateInformation (state.getData(), (int) state.getSize());
            expectEquals (getParam (b, "echo_on"), 1.0f);
            expectEquals (getParam (b, "echo_sync"), 0.0f);
            expectWithinAbsoluteError (getParam (b, "echo_time"), 420.0f, 1.0f);
            expectEquals (getParam (b, "echo_div"), 7.0f);
            expectWithinAbsoluteError (getParam (b, "echo_fb"), 62.0f, 0.1f);
            expectWithinAbsoluteError (getParam (b, "echo_tone"), 2500.0f, 5.0f);
            expectWithinAbsoluteError (getParam (b, "echo_width"), 80.0f, 0.1f);
            expectWithinAbsoluteError (getParam (b, "echo_mix"), 55.0f, 0.1f);

            // Simulate a session saved before Liquid Echo existed: remove its parameters from the saved XML.
            if (auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize()))
            {
                for (int i = xml->getNumChildElements() - 1; i >= 0; --i)
                    if (xml->getChildElement (i)->getStringAttribute ("id").startsWith ("echo_"))
                        xml->removeChildElement (xml->getChildElement (i), true);
                juce::MemoryBlock old;
                juce::AudioProcessor::copyXmlToBinary (*xml, old);
                GlitchKidProcessor c;
                setParam (c, "echo_mix", 90.0f); // non-default, to prove missing parameters return to their defaults
                c.setStateInformation (old.getData(), (int) old.getSize());
                expectWithinAbsoluteError (getParam (c, "gain"), -3.0f, 0.05f);
                expectEquals (getParam (c, "echo_on"), 0.0f);
                expectEquals (getParam (c, "echo_sync"), 1.0f);
                expectWithinAbsoluteError (getParam (c, "echo_mix"), 30.0f, 0.1f); // default, not the instance's earlier 90
            }
            else
                expect (false, "could not read saved state");
        }
    }
};

static EchoTests echoTests;
