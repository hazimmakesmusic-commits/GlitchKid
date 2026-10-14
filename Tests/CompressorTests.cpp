#include "TestHelpers.h"
#include "../Source/DSP/Compressor.h"

namespace
{
using Gains = std::vector<float>;

Compressor makeComp (double sr, CompressorSettings s)
{
    Compressor c;
    c.setSettings (s);
    c.prepare (sr);
    return c;
}

// Run a mono signal through the compressor; returns the applied gain (dB) at every sample.
Gains gainsDb (Compressor& c, const std::vector<float>& in)
{
    Buf b (1, (int) in.size());
    for (size_t i = 0; i < in.size(); ++i)
        b.setSample (0, (int) i, in[i]);
    c.process (b);
    Gains g (in.size());
    for (size_t i = 0; i < in.size(); ++i)
        g[i] = juce::Decibels::gainToDecibels (b.getSample (0, (int) i) / in[i], -200.0f);
    return g;
}

std::vector<float> dc (float level, int n) { return std::vector<float> ((size_t) n, level); }

// Steady-state gain (dB) for a DC input of `level`.
float steadyGainDb (CompressorSettings s, float level, double sr = 48000.0)
{
    auto c = makeComp (sr, s);
    return gainsDb (c, dc (level, (int) (sr * 2.0))).back();
}

CompressorSettings hard (float thresh, float ratio)
{
    return { thresh, ratio, 1.0f, 100.0f, 0.0f, 0.0f };
}
} // namespace

struct CompressorTests : public juce::UnitTest
{
    CompressorTests() : juce::UnitTest ("Compressor", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("default (ratio 1:1) is bit-exact and reports no gain reduction");
        {
            Compressor c;
            c.prepare (48000.0);
            Buf in = makeNoise (2, 4096), out;
            out.makeCopyOf (in);
            c.process (out);
            expectEquals (maxAbsDiff (in, out), 0.0f);
            expectEquals (c.getGainReductionDb(), 0.0f);
        }

        beginTest ("hard-knee static curve: reduction = (level - threshold) * (1 - 1/ratio)");
        for (float ratio : { 2.0f, 4.0f, 10.0f, 20.0f })
            for (float level : { 0.9f, 0.5f, 0.1f })
            {
                const float levelDb = juce::Decibels::gainToDecibels (level);
                const float expected = (levelDb + 30.0f) * (1.0f - 1.0f / ratio);
                expectWithinAbsoluteError (steadyGainDb (hard (-30.0f, ratio), level), -expected, 0.05f);
            }

        beginTest ("below threshold: no reduction");
        expectWithinAbsoluteError (steadyGainDb (hard (-3.0f, 4.0f), 0.5f), 0.0f, 0.001f);

        beginTest ("soft knee (T -20, width 10, 4:1): 0 below, 0.9375 dB at T, 3.75 dB at T + W/2");
        {
            const CompressorSettings s { -20.0f, 4.0f, 1.0f, 100.0f, 10.0f, 0.0f };
            expectWithinAbsoluteError (steadyGainDb (s, juce::Decibels::decibelsToGain (-25.5f)), 0.0f, 0.01f);
            expectWithinAbsoluteError (steadyGainDb (s, juce::Decibels::decibelsToGain (-20.0f)), -0.9375f, 0.05f);
            expectWithinAbsoluteError (steadyGainDb (s, juce::Decibels::decibelsToGain (-15.0f)), -3.75f, 0.05f);
            expectWithinAbsoluteError (steadyGainDb (s, juce::Decibels::decibelsToGain (-10.0f)), -7.5f, 0.05f); // 10 dB over, 4:1
        }

        for (double sr : { 44100.0, 48000.0, 96000.0 })
        {
            beginTest ("attack and release time constants (" + juce::String (sr) + " Hz)");
            // 8:1 above -30 dB; a 0.5 (-6.02 dB) input wants 20.98 dB of reduction.
            const CompressorSettings s { -30.0f, 8.0f, 20.0f, 200.0f, 0.0f, 0.0f };
            const float target = (juce::Decibels::gainToDecibels (0.5f) + 30.0f) * (1.0f - 1.0f / 8.0f);

            auto c = makeComp (sr, s);
            auto in = dc (0.5f, (int) sr);
            auto g = gainsDb (c, in);
            const int atTau = (int) (sr * 0.020) - 1;
            expectWithinAbsoluteError (-g[(size_t) atTau], target * (1.0f - std::exp (-1.0f)), 0.3f); // 63% at one attack time constant
            expect (-g[(size_t) (sr * 0.100)] > 0.99f * target); // essentially done after five

            // Release: loud for 1 s, then quiet (below threshold) for 1 s.
            auto c2 = makeComp (sr, s);
            std::vector<float> in2 = dc (0.5f, (int) sr);
            auto quiet = dc (0.001f, (int) sr);
            in2.insert (in2.end(), quiet.begin(), quiet.end());
            auto g2 = gainsDb (c2, in2);
            const int atRel = (int) sr + (int) (sr * 0.200) - 1;
            expectWithinAbsoluteError (-g2[(size_t) atRel], target * std::exp (-1.0f), 0.3f); // 37% one release time constant later
        }

        beginTest ("makeup gain: +6 dB alone, and after compression");
        {
            expectWithinAbsoluteError (steadyGainDb ({ 0.0f, 1.0f, 1.0f, 100.0f, 0.0f, 6.0f }, 0.1f), 6.0f, 0.01f);
            const float comp = steadyGainDb (hard (-20.0f, 4.0f), 0.5f);
            expectWithinAbsoluteError (steadyGainDb ({ -20.0f, 4.0f, 1.0f, 100.0f, 0.0f, 6.0f }, 0.5f), comp + 6.0f, 0.02f);
        }

        beginTest ("stereo link: both channels receive the same gain from the louder one");
        {
            auto c = makeComp (48000.0, hard (-30.0f, 8.0f));
            Buf b (2, 48000);
            for (int i = 0; i < 48000; ++i) { b.setSample (0, i, 0.5f); b.setSample (1, i, 0.01f); }
            c.process (b);
            const float gl = juce::Decibels::gainToDecibels (b.getSample (0, 47999) / 0.5f);
            const float gr = juce::Decibels::gainToDecibels (b.getSample (1, 47999) / 0.01f);
            expectWithinAbsoluteError (gl, gr, 0.001f);
            expect (gl < -15.0f);
        }

        beginTest ("mono matches stereo");
        {
            auto c1 = makeComp (48000.0, hard (-30.0f, 8.0f));
            auto c2 = makeComp (48000.0, hard (-30.0f, 8.0f));
            auto mono = gainsDb (c1, dc (0.5f, 48000));
            Buf st (2, 48000);
            for (int i = 0; i < 48000; ++i) { st.setSample (0, i, 0.5f); st.setSample (1, i, 0.5f); }
            c2.process (st);
            expectWithinAbsoluteError (mono.back(), juce::Decibels::gainToDecibels (st.getSample (1, 47999) / 0.5f), 0.001f);
        }

        beginTest ("gain reduction meter value, and return to bit-exact after the release tail");
        {
            auto c = makeComp (48000.0, { -30.0f, 8.0f, 5.0f, 100.0f, 0.0f, 0.0f });
            Buf loud (1, 48000);
            for (int i = 0; i < 48000; ++i) loud.setSample (0, i, 0.5f);
            c.process (loud);
            expectWithinAbsoluteError (c.getGainReductionDb(), 20.98f, 0.1f);

            c.setSettings ({ -30.0f, 1.0f, 5.0f, 100.0f, 0.0f, 0.0f }); // compressor switched off
            Buf in = makeNoise (1, 48000), out;
            out.makeCopyOf (in);
            for (int k = 0; k < 4; ++k) { out.makeCopyOf (in); c.process (out); } // 4 s >> release tail
            expectEquals (maxAbsDiff (in, out), 0.0f);
            expectEquals (c.getGainReductionDb(), 0.0f);
        }

        beginTest ("silence with makeup stays silent and finite");
        {
            auto c = makeComp (48000.0, { -30.0f, 8.0f, 5.0f, 100.0f, 6.0f, 12.0f });
            Buf z (2, 4096);
            z.clear();
            c.process (z);
            expectEquals (z.getMagnitude (0, 4096), 0.0f);
        }

        beginTest ("random settings on noise: finite, bounded, no runaway");
        for (double sr : { 44100.0, 96000.0 })
        {
            Compressor c;
            c.prepare (sr);
            juce::Random rng (3);
            float peak = 0.0f;
            for (int blk = 0; blk < 600; ++blk)
            {
                c.setSettings ({ -60.0f + rng.nextFloat() * 60.0f, 1.0f + rng.nextFloat() * 19.0f,
                                 0.1f + rng.nextFloat() * 99.9f, 10.0f + rng.nextFloat() * 990.0f,
                                 rng.nextFloat() * 24.0f, rng.nextFloat() * 24.0f });
                Buf b = makeNoise (2, 256, blk);
                c.process (b);
                peak = juce::jmax (peak, b.getMagnitude (0, 256));
                expect (std::isfinite (peak), "non-finite at block " + juce::String (blk));
            }
            expect (peak < 8.5f, "peak " + juce::String (peak)); // 0.5 input, at most +24 dB makeup (x15.85)
        }

        beginTest ("processor: compressor is in the path, meter reports, bypass is exact, state round-trips");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 512);
            setParam (p, "comp_thresh", -30.0f);
            setParam (p, "comp_ratio", 8.0f);
            setParam (p, "comp_attack", 5.0f);
            setParam (p, "comp_release", 100.0f);
            setParam (p, "comp_knee", 0.0f);
            Buf in (2, 512), out;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                    in.setSample (ch, i, 0.5f);
            juce::MidiBuffer midi;
            for (int blk = 0; blk < 100; ++blk) { out.makeCopyOf (in); p.processBlock (out, midi); }
            expectWithinAbsoluteError (juce::Decibels::gainToDecibels (out.getSample (0, 511) / 0.5f), -20.98f, 0.3f);
            p.consumeGainReductionDb();
            out.makeCopyOf (in); p.processBlock (out, midi);
            expectWithinAbsoluteError (p.consumeGainReductionDb(), 20.98f, 0.3f);

            setParam (p, "bypass", 1.0f);
            for (int blk = 0; blk < 10; ++blk) { out.makeCopyOf (in); p.processBlock (out, midi); }
            expectEquals (maxAbsDiff (in, out), 0.0f);

            setParam (p, "comp_thresh", -24.5f); setParam (p, "comp_ratio", 6.5f); setParam (p, "comp_attack", 3.2f);
            setParam (p, "comp_release", 250.0f); setParam (p, "comp_knee", 9.0f); setParam (p, "comp_makeup", 4.5f);
            juce::MemoryBlock state;
            p.getStateInformation (state);
            GlitchKidProcessor q;
            q.setStateInformation (state.getData(), (int) state.getSize());
            expectWithinAbsoluteError (getParam (q, "comp_thresh"), -24.5f, 0.06f);
            expectWithinAbsoluteError (getParam (q, "comp_ratio"), 6.5f, 0.06f);
            expectWithinAbsoluteError (getParam (q, "comp_attack"), 3.2f, 0.06f);
            expectWithinAbsoluteError (getParam (q, "comp_release"), 250.0f, 1.0f);
            expectWithinAbsoluteError (getParam (q, "comp_knee"), 9.0f, 0.06f);
            expectWithinAbsoluteError (getParam (q, "comp_makeup"), 4.5f, 0.06f);

            GlitchKidProcessor fresh; // defaults must not change the sound
            expectEquals (getParam (fresh, "comp_ratio"), 1.0f);
        }
    }
};

static CompressorTests compressorTests;
