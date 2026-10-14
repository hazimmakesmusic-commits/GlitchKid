#include "TestHelpers.h"
#include "../Source/DSP/Dimension.h"

namespace
{
constexpr float qNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float posInf = std::numeric_limits<float>::infinity();

DimensionSettings dim (float width = 60.0f, float detune = 6.0f, float depth = 50.0f, float mix = 100.0f)
{
    DimensionSettings s;
    s.on = true; s.widthPct = width; s.detuneCents = detune; s.depthPct = depth; s.mixPct = mix;
    return s;
}

Dimension make (double sr, const DimensionSettings& s)
{
    Dimension d;
    d.setSettings (s);
    d.prepare (sr);
    return d;
}

template <typename F>
Buf stream (Dimension& d, Buf in, int blockSize, F before)
{
    for (int start = 0; start < in.getNumSamples(); start += blockSize)
    {
        before (start, d);
        Buf view (in.getArrayOfWritePointers(), in.getNumChannels(), start, juce::jmin (blockSize, in.getNumSamples() - start));
        d.process (view);
    }
    return in;
}
Buf stream (Dimension& d, Buf in, int blockSize = 512) { return stream (d, std::move (in), blockSize, [] (int, Dimension&) {}); }

Buf silent (int ch, int n) { Buf b (ch, n); b.clear(); return b; }

// The same noise on every channel (a "mono vocal" in a stereo track).
Buf monoNoise (int channels, int n, juce::int64 seed = 1, float amp = 1.0f)
{
    Buf one = makeNoise (1, n, seed);
    Buf b (channels, n);
    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < n; ++i)
            b.setSample (ch, i, one.getSample (0, i) * 2.0f * amp);
    return b;
}

Buf sine (int channels, int n, double sr, float freq, float amp = 0.5f)
{
    Buf b (channels, n);
    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < n; ++i)
            b.setSample (ch, i, (float) (amp * std::sin (juce::MathConstants<double>::twoPi * freq * (double) i / sr)));
    return b;
}

Buf impulse (int channels, int n, int at = 0)
{
    Buf b = silent (channels, n);
    for (int ch = 0; ch < channels; ++ch)
        b.setSample (ch, at, 1.0f);
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

double rmsOf (const Buf& b, int ch, int from, int count) { return b.getRMSLevel (ch, from, count); }

// Correlation coefficient between channels 0 and 1.
double correlation (const Buf& b, int from, int count)
{
    double lr = 0, ll = 0, rr = 0;
    for (int i = from; i < from + count; ++i)
    {
        const double l = b.getSample (0, i), r = b.getSample (1, i);
        lr += l * r; ll += l * l; rr += r * r;
    }
    return lr / std::sqrt (ll * rr);
}

// Mono sum (L + R) / 2.
Buf monoSum (const Buf& b)
{
    Buf m (1, b.getNumSamples());
    for (int i = 0; i < b.getNumSamples(); ++i)
        m.setSample (0, i, 0.5f * (b.getSample (0, i) + b.getSample (1, i)));
    return m;
}

double dbRatio (double a, double b) { return 20.0 * std::log10 (a / b); }

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
} // namespace

struct DimensionTests : public juce::UnitTest
{
    DimensionTests() : juce::UnitTest ("Dimension", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("silence in gives exact, finite silence out (mono and stereo)");
        for (int ch : { 1, 2 })
        {
            auto d = make (48000.0, dim (100.0f, 20.0f, 100.0f));
            Buf out = stream (d, silent (ch, 48000));
            expect (allFinite (out) && exactlyZero (out, 0, out.getNumSamples()), "channels " + juce::String (ch));
        }

        beginTest ("mono layout: works, adds the thickening voices, ignores Width");
        {
            Buf in = makeNoise (1, 24000, 2), a, b;
            auto d1 = make (48000.0, dim (0.0f)), d2 = make (48000.0, dim (100.0f));
            a = stream (d1, in); b = stream (d2, in);
            expect (allFinite (a));
            expect (maxAbsDiff (a, in) > 0.01f, "the voices are audible");
            expectEquals (maxAbsDiff (a, b), 0.0f);
        }

        beginTest ("stereo: no panning (centred in = centred out; a hard-left voice stays left); Mix 0 and Depth 0 are exact dry");
        {
            auto d = make (48000.0, dim (60.0f));
            Buf c = stream (d, monoNoise (2, 48000, 4, 0.5f));
            expect (std::abs (dbRatio (rmsOf (c, 0, 4800, 40000), rmsOf (c, 1, 4800, 40000))) < 0.5, "L and R equally loud for a centred voice");

            auto d0 = make (48000.0, dim (0.0f));
            Buf c0 = stream (d0, monoNoise (2, 20000, 4));
            expectEquals (maxAbsDiff (Buf (c0.getArrayOfWritePointers(), 1, 0, 20000), Buf (c0.getArrayOfWritePointers() + 1, 1, 0, 20000)), 0.0f);

            Buf left (2, 48000);
            left.clear();
            left.copyFrom (0, 0, makeNoise (1, 48000, 5), 0, 0, 48000);
            auto dl = make (48000.0, dim (100.0f, 20.0f, 100.0f));
            Buf lo = stream (dl, left);
            expect (rmsOf (lo, 0, 4800, 40000) > 2.0 * rmsOf (lo, 1, 4800, 40000), "a hard-left voice stays mostly left");

            Buf in = makeNoise (2, 20000, 6);
            auto m0 = make (48000.0, dim (60.0f, 6.0f, 50.0f, 0.0f));
            expectEquals (maxAbsDiff (in, stream (m0, in)), 0.0f);
            auto p0 = make (48000.0, dim (60.0f, 6.0f, 0.0f, 100.0f));
            expectEquals (maxAbsDiff (in, stream (p0, in)), 0.0f);
        }

        beginTest ("Width changes the stereo image (measured as L/R correlation of a mono voice)");
        {
            double prev = 2.0;
            for (float w : { 0.0f, 25.0f, 50.0f, 75.0f, 100.0f })
            {
                auto d = make (48000.0, dim (w, 12.0f, 100.0f));
                Buf out = stream (d, monoNoise (2, 96000, 7, 0.5f));
                const double c = correlation (out, 9600, 80000);
                logMessage ("  width " + juce::String (w) + " %: L/R correlation " + juce::String (c, 3));
                expect (c < prev + 1.0e-9, "correlation falls as Width rises, width " + juce::String (w));
                prev = c;
                if (w < 0.5f) expect (c > 0.9999, "width 0 is a centred (identical L/R) signal");
            }
            expect (prev < 0.97, "width 100 clearly decorrelates L and R (" + juce::String (prev, 3) + ")");
        }

        beginTest ("Width 100 %: a mono sum gives back the dry signal exactly; low Width colours it only moderately");
        {
            Buf dry = monoNoise (2, 96000, 8, 0.5f);
            auto d = make (48000.0, dim (100.0f, 20.0f, 100.0f));
            Buf out = stream (d, dry);
            expect (maxAbsDiff (monoSum (out), monoSum (dry)) < 2.0e-6f, "side-only wet cancels in mono");
            for (float w : { 0.0f, 30.0f, 60.0f })
                for (float detune : { 0.0f, 20.0f })
                {
                    auto e = make (48000.0, dim (w, detune, 100.0f));
                    Buf o = stream (e, dry);
                    const double db = dbRatio (rmsOf (monoSum (o), 0, 9600, 80000), rmsOf (dry, 0, 9600, 80000));
                    logMessage ("  mono-sum level vs dry, width " + juce::String (w) + " %, detune " + juce::String (detune) + " c, depth 100: "
                                + juce::String (db, 2) + " dB");
                    expect (std::abs (db) < 3.0, "mono sum within 3 dB of the dry level");
                }
        }

        beginTest ("worst case: swept sines at Width 0 (plain chorus, the least mono-friendly setting) vs Width 100");
        {
            for (auto setting : { std::pair<float, float> { 0.0f, 100.0f }, { 100.0f, 100.0f }, { 60.0f, 50.0f } }) // width, depth
            {
                const float w = setting.first, depthPct = setting.second;
                double lo = 1e9, hi = -1e9;
                for (int k = 0; k < 120; ++k)
                {
                    const float f = 250.0f * std::pow (10000.0f / 250.0f, (float) k / 119.0f);
                    auto d = make (48000.0, dim (w, 0.0f, depthPct));
                    Buf in = sine (2, 14400, 48000.0, f, 0.5f);
                    Buf o = stream (d, in);
                    const double db = dbRatio (rmsOf (monoSum (o), 0, 7200, 7200), rmsOf (in, 0, 7200, 7200));
                    lo = juce::jmin (lo, db); hi = juce::jmax (hi, db);
                }
                logMessage ("  width " + juce::String (w) + " %, depth " + juce::String (depthPct) + ", 250 Hz..10 kHz: mono sum between " + juce::String (lo, 2) + " and " + juce::String (hi, 2) + " dB of the dry level");
                expect (lo > -9.0 && hi < 4.5, "mono dip/peak bounded by 1 -+ 0.6");
                if (w > 99.0f) expect (std::abs (lo) < 0.01 && std::abs (hi) < 0.01, "width 100: no dip, no peak");
            }
        }

        beginTest ("output level stays consistent: within 1 dB of the dry level, and Width does not change it");
        {
            Buf dry = monoNoise (2, 96000, 9, 0.5f);
            const double ref = rmsOf (dry, 0, 9600, 80000);
            double lo = 1e9, hi = -1e9;
            for (float w : { 0.0f, 50.0f, 100.0f })
                for (float depth : { 50.0f, 100.0f })
                {
                    auto d = make (48000.0, dim (w, 10.0f, depth));
                    Buf o = stream (d, dry);
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        const double db = dbRatio (rmsOf (o, ch, 9600, 80000), ref);
                        expect (std::abs (db) < 1.0, "channel " + juce::String (ch) + " width " + juce::String (w) + " depth " + juce::String (depth) + ": " + juce::String (db, 2) + " dB");
                        if (depth > 99.0f) { lo = juce::jmin (lo, db); hi = juce::jmax (hi, db); }
                    }
                }
            logMessage ("  level change at depth 100 across Width 0..100: " + juce::String (lo, 2) + " to " + juce::String (hi, 2) + " dB");
            expect (hi - lo < 0.6, "Width changes the level by less than 0.6 dB");
        }

        beginTest ("bass is left (almost) alone: a 30 Hz tone changes by less than 0.7 dB at Width 0, Depth 100");
        {
            auto d = make (48000.0, dim (0.0f, 0.0f, 100.0f));
            Buf in = sine (2, 24000, 48000.0, 30.0f, 0.5f);
            Buf o = stream (d, in);
            const double db = dbRatio (rmsOf (o, 0, 4800, 19200), rmsOf (in, 0, 4800, 19200));
            logMessage ("  30 Hz level change at Width 0, Depth 100: " + juce::String (db, 2) + " dB");
            expect (std::abs (db) < 0.7);
        }

        beginTest ("Depth and Detune change the signal as intended; without Detune the effect is time-invariant");
        {
            Buf dry = monoNoise (2, 48000, 10, 0.5f);
            auto diffEnergy = [&] (float width, float detune, float depth)
            {
                auto d = make (48000.0, dim (width, detune, depth));
                Buf o = stream (d, dry);
                double e = 0;
                for (int i = 9600; i < 48000; ++i) { const double x = o.getSample (0, i) - dry.getSample (0, i); e += x * x; }
                return e;
            };
            expect (diffEnergy (60.0f, 6.0f, 25.0f) < diffEnergy (60.0f, 6.0f, 50.0f), "more Depth, more effect");
            expect (diffEnergy (60.0f, 6.0f, 50.0f) < diffEnergy (60.0f, 6.0f, 100.0f));

            auto steadyError = [&] (float detune)
            {
                auto d = make (48000.0, dim (100.0f, detune, 100.0f));
                Buf o = stream (d, sine (2, 96000, 48000.0, 1000.0f));
                float e = 0;
                for (int i = 48000; i < 48000 + 4800; ++i)
                    e = juce::jmax (e, std::abs (o.getSample (0, i) - o.getSample (0, i + 48 * 100)));
                return e;
            };
            expect (steadyError (0.0f) < 1.0e-4f, "detune 0: the output repeats exactly like the input");
            expect (steadyError (20.0f) > 1.0e-2f, "detune 20: the voices move");
        }

        beginTest ("timing: the voices come 9 ms (+) and 13 ms (-) after the input, at every sample rate");
        for (double sr : { 44100.0, 48000.0, 96000.0 })
        {
            auto d = make (sr, dim (100.0f, 0.0f, 100.0f));
            Buf o = stream (d, impulse (2, (int) (0.03 * sr)));
            const int a = (int) (0.002 * sr), b = (int) (0.011 * sr), e = (int) (0.03 * sr);
            int pk = a, tr = b;
            for (int i = a; i < b; ++i) if (o.getSample (0, i) > o.getSample (0, pk)) pk = i;
            for (int i = b; i < e; ++i) if (o.getSample (0, i) < o.getSample (0, tr)) tr = i;
            expect (std::abs (pk - (int) std::lround (0.009 * sr)) <= 1, "positive voice at 9 ms, sr " + juce::String (sr));
            expect (std::abs (tr - (int) std::lround (0.013 * sr)) <= 1, "negative voice at 13 ms, sr " + juce::String (sr));
        }

        beginTest ("extreme permitted settings stay finite and bounded; block size never changes the result");
        {
            for (float w : { 0.0f, 100.0f })
            {
                auto d = make (48000.0, dim (w, 20.0f, 100.0f));
                Buf o = stream (d, monoNoise (2, 48000 * 5, 11, 1.0f), 333);
                expect (allFinite (o) && o.getMagnitude (0, o.getNumSamples()) < 1.6f, "bounded, width " + juce::String (w));
            }
            for (double sr : { 44100.0, 96000.0 })
            {
                Buf in = makeNoise (2, (int) sr, 12), ref;
                for (int bs : { 1, 37, 512, 2048 })
                {
                    auto d = make (sr, dim (70.0f, 15.0f, 80.0f, 90.0f));
                    Buf o = stream (d, in, bs);
                    if (bs == 1) ref = o;
                    else expectEquals (maxAbsDiff (ref, o), 0.0f, "sr " + juce::String (sr) + " block " + juce::String (bs));
                }
            }
        }

        beginTest ("NaN/Inf input: finite output, equivalent to a zero sample, nothing stays poisoned; overflow recovers");
        {
            Buf a = makeNoise (2, 24000, 13), b;
            b.makeCopyOf (a);
            const float bad[3] = { qNaN, posInf, -posInf };
            for (int ch = 0; ch < 2; ++ch)
                for (int k = 0; k < 3; ++k)
                {
                    a.setSample (ch, 3000 * (k + 1) + ch, bad[k]);
                    b.setSample (ch, 3000 * (k + 1) + ch, 0.0f);
                }
            auto da = make (48000.0, dim (80.0f, 12.0f, 90.0f)), db = make (48000.0, dim (80.0f, 12.0f, 90.0f));
            Buf oa = stream (da, a), ob = stream (db, b);
            expect (allFinite (oa));
            expectEquals (maxAbsDiff (oa, ob), 0.0f);

            auto h = make (48000.0, dim (80.0f, 12.0f, 90.0f)), ref = make (48000.0, dim (80.0f, 12.0f, 90.0f));
            Buf burst (2, 2000);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 2000; ++i)
                    burst.setSample (ch, i, (i & 1) ? -3.0e38f : 3.0e38f);
            expect (allFinite (stream (h, burst)), "finite during an overflow burst");
            stream (ref, silent (2, 2000));
            Buf clean = makeNoise (2, 20000, 14);
            expect (maxAbsDiff (stream (h, clean), stream (ref, clean)) < 1.0e-6f, "after the burst it behaves like an unaffected one");
        }

        beginTest ("parameter changes during playback and switching on/off are click-free; off is exact; no stale audio");
        {
            auto d = make (48000.0, dim (50.0f, 8.0f, 60.0f));
            juce::Random rng (21);
            Buf in = sine (2, 48000 * 4, 48000.0, 220.0f);
            Buf o = stream (d, in, 256, [&] (int, Dimension& x)
            {
                x.setSettings (dim (100.0f * rng.nextFloat(), 20.0f * rng.nextFloat(), 100.0f * rng.nextFloat(), 100.0f * rng.nextFloat()));
            });
            expect (allFinite (o));
            expect (maxStep (o, 100) < 0.08f, "largest step " + juce::String (maxStep (o, 100)) + " (dry sine alone: 0.0144)");

            auto s = dim (50.0f, 8.0f, 60.0f);
            auto startOff = s; startOff.on = false;
            auto g = make (48000.0, startOff);
            Buf toggled = stream (g, sine (2, 48000 * 4, 48000.0, 220.0f), 480, [&] (int start, Dimension& x)
            {
                auto t = s;
                t.on = (start / 48000) % 2 == 1; // off, on, off, on
                x.setSettings (t);
            });
            expect (maxStep (toggled, 1) < 0.08f, "on/off largest step " + juce::String (maxStep (toggled)));
            Buf firstSecond (2, 48000), dryFirst = sine (2, 48000, 48000.0, 220.0f);
            for (int ch = 0; ch < 2; ++ch) firstSecond.copyFrom (ch, 0, toggled, ch, 0, 48000);
            expectEquals (maxAbsDiff (firstSecond, dryFirst), 0.0f);

            auto on = s; on.on = true;
            auto h = make (48000.0, on);
            stream (h, sine (2, 20000, 48000.0, 220.0f));
            auto off = s; off.on = false;
            h.setSettings (off);
            stream (h, silent (2, 3000));
            expect (! h.isActive(), "idle after the fade");
            h.setSettings (on);
            expect (exactlyZero (stream (h, silent (2, 20000)), 0, 20000), "no stale audio after re-enable");
        }

        beginTest ("processor: Dimension off by default, no latency change, works in mono and stereo, parameters automatable");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 512);
            expectEquals (p.getLatencySamples(), Saturation::latencySamples);
            expectEquals (getParam (p, "dim_on"), 0.0f);
            setParam (p, "dim_on", 1.0f); setParam (p, "dim_width", 100.0f); setParam (p, "dim_depth", 100.0f);
            juce::MidiBuffer midi;
            Buf warm = monoNoise (2, 4096, 3, 0.5f);
            p.processBlock (warm, midi);
            Buf in = monoNoise (2, 8192, 3, 0.5f), out;
            out.makeCopyOf (in);
            p.processBlock (out, midi);
            expect (allFinite (out) && correlation (out, 1024, 7000) < 0.97, "the processor path widens a mono voice");
            expectEquals (p.getLatencySamples(), Saturation::latencySamples);

            GlitchKidProcessor m;
            setLayout (m, juce::AudioChannelSet::mono());
            m.prepareToPlay (48000.0, 256);
            setParam (m, "dim_on", 1.0f);
            juce::Random rng (5);
            bool finite = true;
            for (int blk = 0; blk < 200; ++blk)
            {
                setParam (m, "dim_width", 100.0f * rng.nextFloat());
                setParam (m, "dim_detune", 20.0f * rng.nextFloat());
                setParam (m, "dim_depth", 100.0f * rng.nextFloat());
                setParam (m, "dim_mix", 100.0f * rng.nextFloat());
                setParam (m, "dim_on", rng.nextInt (5) == 0 ? 0.0f : 1.0f);
                Buf b = makeNoise (1, 256, blk);
                m.processBlock (b, midi);
                finite &= allFinite (b);
            }
            expect (finite);
        }

        beginTest ("state: Dimension parameters round-trip; an older state without them loads with Dimension defaults");
        {
            GlitchKidProcessor a;
            setParam (a, "dim_on", 1.0f); setParam (a, "dim_width", 85.0f); setParam (a, "dim_detune", 13.0f);
            setParam (a, "dim_depth", 70.0f); setParam (a, "dim_mix", 40.0f); setParam (a, "gain", -2.0f);
            juce::MemoryBlock state;
            a.getStateInformation (state);
            GlitchKidProcessor b;
            b.setStateInformation (state.getData(), (int) state.getSize());
            expectEquals (getParam (b, "dim_on"), 1.0f);
            expectWithinAbsoluteError (getParam (b, "dim_width"), 85.0f, 0.1f);
            expectWithinAbsoluteError (getParam (b, "dim_detune"), 13.0f, 0.1f);
            expectWithinAbsoluteError (getParam (b, "dim_depth"), 70.0f, 0.1f);
            expectWithinAbsoluteError (getParam (b, "dim_mix"), 40.0f, 0.1f);

            if (auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize()))
            {
                for (int i = xml->getNumChildElements() - 1; i >= 0; --i)
                    if (xml->getChildElement (i)->getStringAttribute ("id").startsWith ("dim_"))
                        xml->removeChildElement (xml->getChildElement (i), true);
                juce::MemoryBlock old;
                juce::AudioProcessor::copyXmlToBinary (*xml, old);
                GlitchKidProcessor c;
                setParam (c, "dim_width", 10.0f);
                c.setStateInformation (old.getData(), (int) old.getSize());
                expectWithinAbsoluteError (getParam (c, "gain"), -2.0f, 0.05f);
                expectEquals (getParam (c, "dim_on"), 0.0f);
                expectWithinAbsoluteError (getParam (c, "dim_width"), 60.0f, 0.1f);
                expectWithinAbsoluteError (getParam (c, "dim_detune"), 6.0f, 0.1f);
                expectWithinAbsoluteError (getParam (c, "dim_depth"), 50.0f, 0.1f);
                expectWithinAbsoluteError (getParam (c, "dim_mix"), 100.0f, 0.1f);
            }
            else
                expect (false, "could not read saved state");
        }
    }
};

static DimensionTests dimensionTests;
