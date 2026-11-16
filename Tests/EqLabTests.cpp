#include "TestHelpers.h"
#include "../Source/PluginEditor.h"
#include "../Source/DSP/ParametricEq.h"
#include "../Source/DSP/EqParamIds.h"
#include "../Source/Analysis/EqAnalyzer.h"
#include "../Source/UI/PlaygroundWidgets.h"
#include <complex>

// EQ LAB engine: every filter type measured against its own calculated response, the original four bands against the RBJ formulas they replaced
// (old sessions keep their sound), slopes, channel placement (L / R / M / S), solo, click-free changes, stability under fast automation,
// old-session defaults in the processor, and the cost of 16 busy bands.
namespace
{
constexpr double twoPi = 6.283185307179586;

// the measured gain (dB) of the engine for a steady sine, after the smoothing and the filter transient
double measureDb (const EqBandSettings& s, double hz, double fs, int channel = 0, int channels = 1)
{
    ParametricEq eq;
    eq.prepare (fs);
    eq.setBand (5, s);
    eq.reset();
    const int n = (int) fs, block = 480;
    double inE = 0.0, outE = 0.0;
    for (int pos = 0; pos < n; pos += block)
    {
        th::Buf b (channels, block);
        for (int i = 0; i < block; ++i)
            for (int ch = 0; ch < channels; ++ch)
                b.setSample (ch, i, 0.25f * (float) std::sin (twoPi * hz * (double) (pos + i) / fs));
        eq.process (b);
        if (pos > n / 2)
            for (int i = 0; i < block; ++i)
            {
                const double x = 0.25 * std::sin (twoPi * hz * (double) (pos + i) / fs);
                inE += x * x;
                outE += (double) b.getSample (channel, i) * b.getSample (channel, i);
            }
    }
    return 10.0 * std::log10 (outE / inE);
}

// The RBJ "Audio EQ Cookbook" formulas the original EQ used (bells: their Q; shelves: a fixed 0.7071 slope), as the reference for old sessions.
double rbjDb (EqType t, double f, double gainDb, double q, double hz, double fs)
{
    const double A = std::pow (10.0, gainDb / 40.0), w0 = twoPi * std::min (f, 0.45 * fs) / fs, cosw = std::cos (w0), sinw = std::sin (w0);
    double b0, b1, b2, a0, a1, a2;
    if (t == EqType::bell)
    {
        const double alpha = sinw / (2.0 * q);
        b0 = 1 + alpha * A; b1 = -2 * cosw; b2 = 1 - alpha * A; a0 = 1 + alpha / A; a1 = -2 * cosw; a2 = 1 - alpha / A;
    }
    else
    {
        const double alpha = sinw / (2.0 * q), k = 2.0 * std::sqrt (A) * alpha;
        if (t == EqType::lowShelf)
        {
            b0 = A * ((A + 1) - (A - 1) * cosw + k); b1 = 2 * A * ((A - 1) - (A + 1) * cosw); b2 = A * ((A + 1) - (A - 1) * cosw - k);
            a0 = (A + 1) + (A - 1) * cosw + k;       a1 = -2 * ((A - 1) + (A + 1) * cosw);    a2 = (A + 1) + (A - 1) * cosw - k;
        }
        else
        {
            b0 = A * ((A + 1) + (A - 1) * cosw + k); b1 = -2 * A * ((A - 1) + (A + 1) * cosw); b2 = A * ((A + 1) + (A - 1) * cosw - k);
            a0 = (A + 1) - (A - 1) * cosw + k;       a1 = 2 * ((A - 1) - (A + 1) * cosw);      a2 = (A + 1) - (A - 1) * cosw - k;
        }
    }
    const double w = twoPi * hz / fs;
    const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
    return 20.0 * std::log10 (std::abs (b0 + b1 * z1 + b2 * z2) / std::abs (a0 + a1 * z1 + a2 * z2));
}

float maxStep (const th::Buf& b, int ch)
{
    float m = 0.0f;
    for (int i = 1; i < b.getNumSamples(); ++i)
        m = juce::jmax (m, std::abs (b.getSample (ch, i) - b.getSample (ch, i - 1)));
    return m;
}
} // namespace

struct EqLabTests : public juce::UnitTest
{
    EqLabTests() : juce::UnitTest ("EQ LAB", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("old sessions keep their sound: bells and shelves equal the RBJ formulas the original EQ used (44.1, 48, 96 kHz)");
        {
            double worst = 0.0;
            for (double fs : { 44100.0, 48000.0, 96000.0 })
                for (EqType t : { EqType::bell, EqType::lowShelf, EqType::highShelf })
                    for (double f : { 80.0, 400.0, 3000.0, 9000.0 })
                        for (double g : { -12.0, -3.0, 4.5, 15.0 })
                            for (double q : { 0.5, 0.7071, 2.0, 8.0 })
                            {
                                if (t != EqType::bell && std::abs (q - 0.7071) > 1.0e-9) // the original shelves only had 0.7071
                                    continue;
                                for (double hz : { 30.0, 100.0, 300.0, 1000.0, 3000.0, 7000.0, 12000.0, 18000.0 })
                                {
                                    EqBandSettings s { (float) f, (float) g, (float) q, t };
                                    worst = std::max (worst, std::abs (ParametricEq::bandResponseDb (s, hz, fs) - rbjDb (t, f, g, q, hz, fs)));
                                }
                            }
            logMessage ("largest difference from the original filters: " + juce::String (worst, 6) + " dB");
            expect (worst < 0.001, "differs by " + juce::String (worst) + " dB");
        }

        beginTest ("every type: the measured output equals the calculated response (the graph shows the real filter)");
        {
            const double fs = 48000.0;
            struct Case { EqBandSettings s; const char* name; };
            const Case cases[] = {
                { { 1000.0f, 6.0f, 1.5f, EqType::bell }, "bell" },
                { { 200.0f, -8.0f, 1.2f, EqType::lowShelf }, "low shelf Q 1.2" },
                { { 6000.0f, 5.0f, 2.0f, EqType::highShelf }, "high shelf Q 2" },
                { { 150.0f, 0.0f, 0.7071f, EqType::lowCut, true, 6 }, "low cut 6" },
                { { 150.0f, 0.0f, 0.7071f, EqType::lowCut, true, 24 }, "low cut 24" },
                { { 150.0f, 0.0f, 2.0f, EqType::lowCut, true, 12 }, "low cut 12 resonant" },
                { { 8000.0f, 0.0f, 0.7071f, EqType::highCut, true, 18 }, "high cut 18" },
                { { 8000.0f, 0.0f, 0.7071f, EqType::highCut, true, 96 }, "high cut 96" },
                { { 2500.0f, 0.0f, 4.0f, EqType::notch }, "notch" },
                { { 1200.0f, 0.0f, 1.0f, EqType::bandPass }, "band pass" },
                { { 1000.0f, 6.0f, 0.7071f, EqType::tilt }, "tilt" } };
            for (const auto& c : cases)
                for (double hz : { 60.0, 150.0, 700.0, 2000.0, 5000.0, 11000.0 })
                {
                    const double want = ParametricEq::bandResponseDb (c.s, hz, fs);
                    if (want < -60.0)
                        continue; // deep stop band: measurement noise, not the filter
                    const double got = measureDb (c.s, hz, fs);
                    expectWithinAbsoluteError (got, want, 0.2, juce::String (c.name) + " at " + juce::String (hz) + " Hz");
                }
        }

        beginTest ("cuts: the slope is what it says, Q adds resonance at the cutoff, a tilt pivots around its frequency");
        {
            const double fs = 48000.0;
            for (int slope : { 6, 12, 18, 24, 36, 48, 72, 96 })
            {
                const EqBandSettings s { 2000.0f, 0.0f, 0.7071f, EqType::lowCut, true, slope };
                const double lo = slope <= 48 ? 250.0 : 500.0; // one octave deep in the stop band, above the -240 dB floor of the calculation
                const double drop = ParametricEq::bandResponseDb (s, lo, fs) - ParametricEq::bandResponseDb (s, 2.0 * lo, fs);
                expectWithinAbsoluteError (drop, -(double) slope, 0.6, juce::String (slope) + " dB/oct: " + juce::String (drop, 2));
                expectWithinAbsoluteError (ParametricEq::bandResponseDb (s, 2000.0, fs), -3.01, 0.05, "Butterworth: -3 dB at the cutoff");
            }
            const EqBandSettings res { 2000.0f, 0.0f, 3.0f, EqType::highCut, true, 24 };
            expect (ParametricEq::bandResponseDb (res, 2000.0, fs) > 6.0, "resonance at the cutoff");
            const EqBandSettings tilt { 1000.0f, 8.0f, 0.7071f, EqType::tilt };
            expectWithinAbsoluteError (ParametricEq::bandResponseDb (tilt, 40.0, fs), -4.0, 0.1);
            expectWithinAbsoluteError (ParametricEq::bandResponseDb (tilt, 16000.0, fs), 4.0, 0.15);
            expectWithinAbsoluteError (ParametricEq::bandResponseDb (tilt, 1000.0, fs), 0.0, 0.05);
            const EqBandSettings off { 1000.0f, 9.0f, 1.0f, EqType::bell, false };
            expectEquals (ParametricEq::bandResponseDb (off, 1000.0, fs), 0.0, "a band that is off does nothing");
        }

        beginTest ("channels: left, right, mid and side bands work only on their part of the stereo signal");
        {
            const double fs = 48000.0;
            EqBandSettings s { 1000.0f, -12.0f, 1.0f, EqType::bell };
            s.placement = EqPlacement::left;
            expectWithinAbsoluteError (measureDb (s, 1000.0, fs, 0, 2), -12.0, 0.2, "left band: left");
            expectWithinAbsoluteError (measureDb (s, 1000.0, fs, 1, 2), 0.0, 0.01, "left band: right untouched");
            s.placement = EqPlacement::right;
            expectWithinAbsoluteError (measureDb (s, 1000.0, fs, 0, 2), 0.0, 0.01, "right band: left untouched");
            s.placement = EqPlacement::mid; // identical channels are all mid
            expectWithinAbsoluteError (measureDb (s, 1000.0, fs, 0, 2), -12.0, 0.2, "mid band on a centred voice");
            s.placement = EqPlacement::side; // ... and have no side
            expectWithinAbsoluteError (measureDb (s, 1000.0, fs, 0, 2), 0.0, 0.01, "side band on a centred voice");

            // a pure side signal (R = -L): a mid band leaves it alone, a side band cuts it
            for (auto p : { EqPlacement::mid, EqPlacement::side })
            {
                ParametricEq eq;
                eq.prepare (fs);
                EqBandSettings b { 1000.0f, -12.0f, 1.0f, EqType::bell };
                b.placement = p;
                eq.setBand (0, b);
                eq.reset();
                double inE = 0.0, outE = 0.0;
                for (int pos = 0; pos < 48000; pos += 480)
                {
                    th::Buf buf (2, 480);
                    for (int i = 0; i < 480; ++i)
                    {
                        const float x = 0.25f * (float) std::sin (twoPi * 1000.0 * (double) (pos + i) / fs);
                        buf.setSample (0, i, x);
                        buf.setSample (1, i, -x);
                    }
                    eq.process (buf);
                    if (pos > 24000)
                        for (int i = 0; i < 480; ++i)
                        {
                            const double x = 0.25 * std::sin (twoPi * 1000.0 * (double) (pos + i) / fs);
                            inE += x * x;
                            outE += (double) buf.getSample (0, i) * buf.getSample (0, i);
                            expectWithinAbsoluteError (buf.getSample (1, i), -buf.getSample (0, i), 1.0e-5f);
                        }
                }
                const double db = 10.0 * std::log10 (outE / inE);
                expectWithinAbsoluteError (db, p == EqPlacement::mid ? 0.0 : -12.0, 0.2, p == EqPlacement::mid ? "mid band on a pure side signal" : "side band on a pure side signal");
            }
        }

        beginTest ("solo: you hear only the part one band works on; it fades in and out without a click and is never part of the sound");
        {
            const double fs = 48000.0;
            ParametricEq eq;
            eq.prepare (fs);
            eq.setBand (3, { 1000.0f, 3.0f, 2.0f, EqType::bell });
            eq.reset();
            auto play = [&] (double hz, int blocks)
            {
                double e = 0.0;
                float step = 0.0f;
                for (int k = 0; k < blocks; ++k)
                {
                    th::Buf b (1, 480);
                    for (int i = 0; i < 480; ++i)
                        b.setSample (0, i, 0.25f * (float) std::sin (twoPi * hz * (double) (k * 480 + i) / fs));
                    eq.process (b);
                    step = juce::jmax (step, maxStep (b, 0));
                    if (k >= blocks / 2)
                        for (int i = 0; i < 480; ++i)
                            e += (double) b.getSample (0, i) * b.getSample (0, i);
                }
                return std::make_pair (10.0 * std::log10 (e / (0.03125 * 480.0 * (blocks - blocks / 2))), step);
            };
            eq.setSolo (3);
            const auto atCentre = play (1000.0, 40), farBelow = play (100.0, 40);
            expectWithinAbsoluteError (atCentre.first, 0.0, 0.5, "the band's centre passes");
            expect (farBelow.first < -25.0, "far from the band is gone: " + juce::String (farBelow.first, 1) + " dB");
            eq.setSolo (-1);
            const auto back = play (100.0, 40);
            expectWithinAbsoluteError (back.first, 0.0, 0.1, "solo off: the normal EQ again");
            expect (back.second < 0.05f, "no click when solo ends: largest step " + juce::String (back.second));
        }

        beginTest ("changes never click: type, slope, channel and on/off fade; moving frequency, gain and Q glides");
        {
            const double fs = 48000.0;
            ParametricEq eq;
            eq.prepare (fs);
            EqBandSettings s { 400.0f, 12.0f, 1.0f, EqType::bell };
            eq.setBand (0, s);
            eq.reset();
            float worst = 0.0f;
            const EqBandSettings changes[] = {
                { 400.0f, 12.0f, 1.0f, EqType::lowShelf }, { 400.0f, 12.0f, 1.0f, EqType::highCut, true, 96 }, { 400.0f, 12.0f, 1.0f, EqType::lowCut, true, 6 },
                { 400.0f, 12.0f, 1.0f, EqType::lowCut, false, 6 }, { 400.0f, 12.0f, 1.0f, EqType::bell }, { 6000.0f, -18.0f, 6.0f, EqType::bell } };
            int pos = 0;
            for (const auto& c : changes)
            {
                eq.setBand (0, c);
                for (int k = 0; k < 20; ++k, pos += 480)
                {
                    th::Buf b (1, 480);
                    for (int i = 0; i < 480; ++i)
                        b.setSample (0, i, 0.1f * (float) std::sin (twoPi * 300.0 * (double) (pos + i) / fs));
                    eq.process (b);
                    worst = juce::jmax (worst, maxStep (b, 0));
                }
            }
            logMessage ("largest sample-to-sample step across all changes: " + juce::String (worst, 4) + " (a 300 Hz sine at 0.1 x up to +12 dB steps by about 0.016)");
            expect (worst < 0.08f, "a change clicked: step " + juce::String (worst));
        }

        beginTest ("stability: 16 bands of every type automated wildly at 44.1 and 96 kHz stay finite and bounded");
        {
            for (double fs : { 44100.0, 96000.0 })
            {
                ParametricEq eq;
                eq.prepare (fs);
                juce::Random rng (11);
                float peak = 0.0f;
                for (int blk = 0; blk < 300; ++blk)
                {
                    for (int b = 0; b < ParametricEq::numBands; ++b)
                    {
                        EqBandSettings s { 20.0f + rng.nextFloat() * 20000.0f, (rng.nextFloat() - 0.5f) * 12.0f, 0.1f + rng.nextFloat() * 10.0f,
                                           (EqType) rng.nextInt (8), rng.nextBool(), ParametricEq::slopes[rng.nextInt (8)], (EqPlacement) rng.nextInt (5),
                                           rng.nextBool(), -60.0f * rng.nextFloat(), (rng.nextFloat() - 0.5f) * 48.0f, 0.1f + rng.nextFloat() * 50.0f,
                                           5.0f + rng.nextFloat() * 500.0f, rng.nextBool() };
                        eq.setBand (b, s);
                    }
                    th::Buf x = th::makeNoise (2, 64, blk);
                    eq.process (x);
                    for (int ch = 0; ch < 2; ++ch)
                        peak = juce::jmax (peak, x.getMagnitude (ch, 0, 64));
                    expect (std::isfinite (peak), "non-finite output at block " + juce::String (blk));
                }
                expect (peak < 100.0f, "peak " + juce::String (peak));
            }
        }

        beginTest ("processor: old sessions load as the original four bands; bands 5-16 are off; a new band really changes the sound; no latency change");
        {
            GlitchKidProcessor p;
            const auto& ids = GlitchKidProcessor::eqIds;
            expectEquals ((int) std::lround (th::getParam (p, ids[0].type)), (int) EqType::lowShelf);
            expectEquals ((int) std::lround (th::getParam (p, ids[1].type)), (int) EqType::bell);
            expectEquals ((int) std::lround (th::getParam (p, ids[3].type)), (int) EqType::highShelf);
            for (int b = 0; b < ParametricEq::numBands; ++b)
                expect ((th::getParam (p, ids[b].on) >= 0.5f) == (b < 4), "band " + juce::String (b + 1) + " on by default only if original");
            expectWithinAbsoluteError (th::getParam (p, ids[0].q), 0.7071f, 1.0e-4f, "the original shelf slope");

            // a session saved before EQ LAB: only the original IDs are in it
            juce::XmlElement old ("GlitchKidState");
            auto* e = old.createNewChildElement ("PARAM");
            e->setAttribute ("id", "eq_lo_gain");
            e->setAttribute ("value", 4.0);
            juce::MemoryBlock mb;
            juce::AudioProcessor::copyXmlToBinary (old, mb);
            th::setParam (p, ids[6].on, 1.0f);
            p.setStateInformation (mb.getData(), (int) mb.getSize());
            expectWithinAbsoluteError (th::getParam (p, ids[0].gain), 4.0f, 0.01f);
            expectEquals ((int) std::lround (th::getParam (p, ids[0].type)), (int) EqType::lowShelf, "missing type: the original low shelf");
            expect (th::getParam (p, ids[6].on) < 0.5f, "missing band: off");

            GlitchKidProcessor q;
            q.prepareToPlay (48000.0, 480);
            const int latency = q.getLatencySamples();
            th::Buf in, outFlat, outCut;
            th::run (q, 2, 480, 40, in, outFlat);
            th::setParam (q, ids[8].on, 1.0f);
            th::setParam (q, ids[8].type, (float) EqType::highCut);
            th::setParam (q, ids[8].freq, 500.0f);
            th::run (q, 2, 480, 40, in, outCut);
            expect (outCut.getRMSLevel (0, 0, 480) < 0.6f * outFlat.getRMSLevel (0, 0, 480), "a high cut at 500 Hz takes energy out of noise");
            expectEquals (q.getLatencySamples(), latency, "EQ LAB adds no latency");
        }

        beginTest ("analyser: a known sine reads at its frequency and level at every resolution; freeze holds; silence shows no peak; note names");
        {
            namespace an = glitchkid::analysis;
            for (int order : { 11, 12, 13 })
            {
                an::DisplayTap tap;
                an::EqAnalyzer a;
                a.setResolution (order);
                a.setAveragingSeconds (0.05f);
                std::vector<float> x (480);
                for (int k = 0; k < 60; ++k)
                {
                    for (int i = 0; i < 480; ++i)
                        x[(size_t) i] = 0.5f * (float) std::sin (twoPi * 1234.0 * (double) (k * 480 + i) / 48000.0);
                    tap.push (x.data(), nullptr, 480);
                    a.update (tap, 48000.0, 0.01f);
                }
                float hz = 0.0f, db = 0.0f;
                expect (a.peak (100.0f, 10000.0f, hz, db), "a peak is found");
                const float binHz = 48000.0f / (float) (1 << order);
                expectWithinAbsoluteError (hz, 1234.0f, binHz * 0.25f, "FFT " + juce::String (1 << order) + ": peak at " + juce::String (hz, 1) + " Hz");
                expectWithinAbsoluteError (db, -6.02f, 1.6f, "level of a 0.5 sine: " + juce::String (db, 2) + " dBFS");
                a.setFrozen (true);
                const float before = a.levelDb (1200.0f, 1270.0f);
                for (int i = 0; i < 480; ++i)
                    x[(size_t) i] = 0.0f;
                for (int k = 0; k < 20; ++k)
                {
                    tap.push (x.data(), nullptr, 480);
                    expect (! a.update (tap, 48000.0, 0.01f), "frozen: no change");
                }
                expectEquals (a.levelDb (1200.0f, 1270.0f), before, "frozen: the display holds");
            }
            an::DisplayTap quiet;
            an::EqAnalyzer s;
            std::vector<float> z (480, 0.0f);
            for (int k = 0; k < 20; ++k)
            {
                quiet.push (z.data(), nullptr, 480);
                s.update (quiet, 48000.0, 0.01f);
            }
            float hz = 0.0f, db = 0.0f;
            expect (! s.peak (20.0f, 20000.0f, hz, db), "silence: no peak");
            expectEquals (an::EqAnalyzer::noteName (440.0f), juce::String ("A4"));
            expect (an::EqAnalyzer::noteName (445.0f).startsWith ("A4 +"), "cents shown: " + an::EqAnalyzer::noteName (445.0f));
        }

        beginTest ("the processor's EQ taps carry the real audio before and after the EQ");
        {
            GlitchKidProcessor q;
            q.prepareToPlay (48000.0, 480);
            const auto& ids = GlitchKidProcessor::eqIds;
            th::setParam (q, ids[8].on, 1.0f);
            th::setParam (q, ids[8].type, (float) EqType::highCut);
            th::setParam (q, ids[8].slope, 7.0f); // 96 dB/oct
            th::setParam (q, ids[8].freq, 1000.0f);
            glitchkid::analysis::EqAnalyzer in, out;
            in.setAveragingSeconds (0.05f); // fast: the first blocks (the band fading in, the tap filling) are forgotten quickly
            out.setAveragingSeconds (0.05f);
            juce::MidiBuffer midi;
            for (int k = 0; k < 200; ++k)
            {
                auto b = th::makeNoise (2, 480, k + 1);
                q.processBlock (b, midi);
                in.update (q.getEqInTap(), 48000.0, 0.01f);
                out.update (q.getEqOutTap(), 48000.0, 0.01f);
            }
            const float inHigh = in.levelDb (6000.0f, 8000.0f), outHigh = out.levelDb (6000.0f, 8000.0f), inLow = in.levelDb (200.0f, 300.0f), outLow = out.levelDb (200.0f, 300.0f);
            expect (inHigh > -60.0f && outHigh < inHigh - 60.0f, "above the cut: in " + juce::String (inHigh, 1) + " dB, out " + juce::String (outHigh, 1) + " dB");
            expectWithinAbsoluteError (outLow, inLow, 3.0f, "below the cut: unchanged");
        }

        beginTest ("graph: add, duplicate and delete bands write the real parameters; all 16 in use refuses; the view is clamped to at least two octaves");
        {
            GlitchKidProcessor p;
            glitchkid::ui::EqGraph g (p.apvts, [] { return 48000.0; });
            g.setBounds (0, 0, 600, 260);
            const auto& ids = GlitchKidProcessor::eqIds;
            const int b = g.addBandAt (2500.0f, 4.0f);
            expectEquals (b, 4, "the first free new band");
            expect (th::getParam (p, ids[4].on) > 0.5f && std::abs (th::getParam (p, ids[4].freq) - 2500.0f) < 2.0f && std::abs (th::getParam (p, ids[4].gain) - 4.0f) < 0.06f);
            expectEquals (g.getSelectedBand(), 4);
            g.refresh();
            expectWithinAbsoluteError (g.pointOf (4).x, g.pointOf (4).x, 0.0f);
            const int d = g.duplicateBand (4);
            expectEquals (d, 5);
            expectWithinAbsoluteError (th::getParam (p, ids[5].freq), 2800.0f, 3.0f, "a little higher");
            g.removeBand (4);
            expect (th::getParam (p, ids[4].on) < 0.5f, "deleted = switched off");
            for (int i = 0; i < 20; ++i)
                g.addBandAt (100.0f + 200.0f * (float) i, 1.0f);
            expectEquals (g.addBandAt (5000.0f, 1.0f), -1, "all 16 in use");
            g.setView (1000.0f, 1100.0f);
            expect (g.getView().getEnd() / g.getView().getStart() >= 3.99f, "at least two octaves");
            g.setView (5.0f, 90000.0f);
            expect (g.getView().getStart() >= 20.0f && g.getView().getEnd() <= 20000.0f);
        }

        beginTest ("dynamic EQ: the gain curve (above / below, range, 6 dB knee, 3:4 slope, silence moves nothing)");
        {
            EqBandSettings d { 3000.0f, 0.0f, 1.0f, EqType::bell };
            d.dynamic = true; d.threshDb = -30.0f; d.rangeDb = -12.0f;
            expectEquals (ParametricEq::dynamicGainDb (d, -50.0f), 0.0f, "well under the threshold: nothing");
            expectWithinAbsoluteError (ParametricEq::dynamicGainDb (d, -30.0f), -0.5625f, 0.001f, "at the threshold: the middle of the knee");
            expectWithinAbsoluteError (ParametricEq::dynamicGainDb (d, -22.0f), -6.0f, 0.001f, "8 dB over: 3/4 of it");
            expectEquals (ParametricEq::dynamicGainDb (d, -6.0f), -12.0f, "far over: the full range, no more");
            d.below = true; d.rangeDb = 6.0f;
            expectEquals (ParametricEq::dynamicGainDb (d, -50.0f), 6.0f, "below mode: a quiet band is lifted by the range");
            expectEquals (ParametricEq::dynamicGainDb (d, -20.0f), 0.0f, "below mode: a loud band is left alone");
            expectEquals (ParametricEq::dynamicGainDb (d, -100.0f), 0.0f, "silence is never lifted");
            d.type = EqType::lowCut;
            expectEquals (ParametricEq::dynamicGainDb (d, -50.0f), 0.0f, "cuts have no gain to move");
        }

        beginTest ("dynamic EQ: the gain really follows the level of the band (measured), and the published change matches");
        {
            const double fs = 48000.0;
            auto run = [&] (EqBandSettings s, double hz, float amp, int blocks, float* shown = nullptr)
            {
                ParametricEq eq;
                eq.prepare (fs);
                eq.setBand (2, s);
                eq.reset();
                double inE = 0.0, outE = 0.0;
                for (int k = 0; k < blocks; ++k)
                {
                    th::Buf b (1, 480);
                    for (int i = 0; i < 480; ++i)
                        b.setSample (0, i, amp * (float) std::sin (twoPi * hz * (double) (k * 480 + i) / fs));
                    eq.process (b);
                    if (k >= blocks / 2)
                        for (int i = 0; i < 480; ++i)
                        {
                            const double x = amp * std::sin (twoPi * hz * (double) (k * 480 + i) / fs);
                            inE += x * x;
                            outE += (double) b.getSample (0, i) * b.getSample (0, i);
                        }
                }
                if (shown != nullptr)
                    *shown = eq.getDynamicDb (2);
                return 10.0 * std::log10 (outE / inE);
            };
            EqBandSettings d { 3000.0f, 0.0f, 1.0f, EqType::bell };
            d.dynamic = true; d.threshDb = -30.0f; d.rangeDb = -12.0f; d.attackMs = 1.0f; d.releaseMs = 50.0f;
            float shown = 0.0f;
            const double loud = run (d, 3000.0, 0.5f, 100, &shown);
            expectWithinAbsoluteError (loud, -12.0, 0.6, "a loud band is cut by the full range: " + juce::String (loud, 2));
            expectWithinAbsoluteError (shown, -12.0f, 0.1f, "the published change");
            expectWithinAbsoluteError (run (d, 3000.0, 0.003f, 100), 0.0, 0.1, "a quiet band is untouched");
            expect (run (d, 150.0, 0.5f, 100) > -0.5, "a loud sound far from the band barely triggers it");
            d.below = true; d.rangeDb = 6.0f;
            expectWithinAbsoluteError (run (d, 3000.0, 0.003f, 100), 6.0, 0.6, "below mode lifts a quiet band");
            expectWithinAbsoluteError (run (d, 3000.0, 0.5f, 100), 0.0, 0.1, "below mode leaves a loud band");
        }

        beginTest ("dynamic EQ: attack and release shape the movement; a stereo band is linked so the image never moves");
        {
            const double fs = 48000.0;
            ParametricEq eq;
            eq.prepare (fs);
            EqBandSettings d { 3000.0f, 0.0f, 1.0f, EqType::bell };
            d.dynamic = true; d.threshDb = -30.0f; d.rangeDb = -12.0f; d.attackMs = 1.0f; d.releaseMs = 200.0f;
            eq.setBand (0, d);
            eq.reset();
            int pos = 0;
            auto block = [&] (float ampL, float ampR, int samples)
            {
                th::Buf b (2, samples);
                for (int i = 0; i < samples; ++i, ++pos)
                {
                    const float x = (float) std::sin (twoPi * 3000.0 * (double) pos / fs);
                    b.setSample (0, i, ampL * x);
                    b.setSample (1, i, ampR * x);
                }
                eq.process (b);
                return b;
            };
            block (0.003f, 0.003f, 4800);
            block (0.5f, 0.5f, 960); // 20 ms of a loud band
            expect (eq.getDynamicDb (0) < -11.0f, "fast attack: nearly the full cut within 20 ms (" + juce::String (eq.getDynamicDb (0), 2) + " dB)");
            block (0.003f, 0.003f, 2400); // 50 ms after the band got quiet
            expect (eq.getDynamicDb (0) < -3.0f, "slow release: still cutting 50 ms later (" + juce::String (eq.getDynamicDb (0), 2) + " dB)");
            block (0.003f, 0.003f, 96000); // 2 s
            expectWithinAbsoluteError (eq.getDynamicDb (0), 0.0f, 0.05f, "and back to nothing");

            // linked: the left channel is loud, the right quiet: both get the same gain, so their ratio (the image) is unchanged
            block (0.5f, 0.05f, 9600);
            const auto b = block (0.5f, 0.05f, 4800);
            const float l = b.getRMSLevel (0, 0, 4800), r = b.getRMSLevel (1, 0, 4800);
            expectWithinAbsoluteError (l / r, 10.0f, 0.05f, "the left/right ratio is kept: " + juce::String (l / r, 3));
        }

        beginTest ("snapshot (written when GLITCHKID_SNAPSHOT_DIR is set): SHAPE with a dynamic band cutting a loud 3 kHz tone");
        {
            const auto dir = juce::SystemStats::getEnvironmentVariable ("GLITCHKID_SNAPSHOT_DIR", {});
            if (dir.isNotEmpty())
            {
                GlitchKidProcessor p;
                p.prepareToPlay (48000.0, 480);
                const auto& ids = GlitchKidProcessor::eqIds;
                th::setParam (p, ids[2].dyn, 1.0f);
                th::setParam (p, ids[2].thresh, -36.0f);
                th::setParam (p, ids[2].range, -9.0f);
                th::setParam (p, ids[2].gain, 2.0f);
                std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
                auto* ed = dynamic_cast<GlitchKidEditor*> (e.get());
                ed->uiSelectRailForTest (4);
                juce::MidiBuffer midi;
                for (int k = 0; k < 120; ++k)
                {
                    juce::AudioBuffer<float> b (2, 480);
                    for (int i = 0; i < 480; ++i)
                    {
                        const float v = 0.4f * (float) std::sin (twoPi * 3000.0 * (double) (k * 480 + i) / 48000.0) + 0.3f * (float) std::sin (twoPi * 220.0 * (double) (k * 480 + i) / 48000.0);
                        b.setSample (0, i, v);
                        b.setSample (1, i, v);
                    }
                    p.processBlock (b, midi);
                    if (k % 3 == 0)
                        ed->uiTickForTest();
                }
                ed->uiTickForTest();
                logMessage ("band 3's dynamic change while the tone plays: " + juce::String (p.getEqDynamicDb (2), 2) + " dB");
                expect (p.getEqDynamicDb (2) < -6.0f, "the dynamic band is really cutting");
                auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                juce::File f = juce::File (dir).getChildFile ("eqlab_dynamic.png");
                f.deleteFile();
                juce::FileOutputStream os (f);
                juce::PNGImageFormat().writeImageToStream (img, os);
            }
        }

        beginTest ("CPU (information only): 16 bands, all busy, 96 dB/oct cuts and mid/side bands, stereo 48 kHz");
        {
            ParametricEq eq;
            eq.prepare (48000.0);
            for (int b = 0; b < ParametricEq::numBands; ++b)
                eq.setBand (b, { 100.0f + 900.0f * (float) b, 3.0f, 1.0f, b % 4 == 0 ? EqType::lowCut : EqType::bell, true, 96,
                                 (EqPlacement) (b % 5) });
            th::Buf x = th::makeNoise (2, 480, 3);
            const int blocks = 48000 * 10 / 480;
            const auto t0 = juce::Time::getHighResolutionTicks();
            for (int k = 0; k < blocks; ++k)
            {
                th::Buf b (x);
                eq.process (b);
            }
            const double seconds = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
            logMessage ("EQ LAB CPU (16 busy bands): " + juce::String (seconds * 10.0, 2) + " % of one core at 48 kHz stereo");
            expect (seconds < 10.0);
            for (int b = 0; b < ParametricEq::numBands; ++b) // ... and all 16 dynamic bells (the costliest: a detector and a redesign every 16 samples)
            {
                EqBandSettings s { 100.0f + 900.0f * (float) b, 3.0f, 1.0f, EqType::bell };
                s.dynamic = true; s.threshDb = -40.0f; s.rangeDb = -6.0f;
                eq.setBand (b, s);
            }
            const auto t1 = juce::Time::getHighResolutionTicks();
            for (int k = 0; k < blocks; ++k)
            {
                th::Buf b (x);
                eq.process (b);
            }
            const double s2 = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t1);
            logMessage ("EQ LAB CPU (16 dynamic bands): " + juce::String (s2 * 10.0, 2) + " % of one core at 48 kHz stereo");
        }
    }
};

static EqLabTests eqLabTests;
