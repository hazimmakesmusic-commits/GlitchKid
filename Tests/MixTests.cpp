#include "TestHelpers.h"

namespace
{
constexpr double pi = 3.14159265358979323846;

void stream (GlitchKidProcessor& p, const Buf& in, Buf& out, int blockSize = 512)
{
    out.setSize (in.getNumChannels(), in.getNumSamples());
    juce::MidiBuffer midi;
    for (int start = 0; start < in.getNumSamples(); start += blockSize)
    {
        const int n = juce::jmin (blockSize, in.getNumSamples() - start);
        Buf block (in.getNumChannels(), n);
        for (int ch = 0; ch < in.getNumChannels(); ++ch)
            block.copyFrom (ch, 0, in, ch, start, n);
        p.processBlock (block, midi);
        for (int ch = 0; ch < in.getNumChannels(); ++ch)
            out.copyFrom (ch, start, block, ch, 0, n);
    }
}

double rms (const Buf& b, int from, int count) { return b.getRMSLevel (0, from, count); }
} // namespace

struct MixTests : public juce::UnitTest
{
    MixTests() : juce::UnitTest ("Mix", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("default is 100 % processed");
        {
            GlitchKidProcessor p;
            expectEquals (getParam (p, "mix"), 100.0f);
        }

        beginTest ("0 % is an exact (latency-delayed) copy even with every stage engaged");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 512);
            setParam (p, "eq_b2_gain", 12.0f);
            setParam (p, "comp_ratio", 8.0f);
            setParam (p, "comp_thresh", -40.0f);
            setParam (p, "sat_drive", 12.0f);
            setParam (p, "gain", 6.0f);
            setParam (p, "mix", 0.0f);
            Buf in = makeNoise (2, 48000), out;
            stream (p, in, out);
            for (int i = 24000; i < 48000; ++i)
                for (int ch = 0; ch < 2; ++ch)
                    expectEquals (out.getSample (ch, i), in.getSample (ch, i - pluginLatency));
        }

        beginTest ("50 % with +6 dB gain gives dry * (1 + (g - 1) * 0.5); 25 % with -60 dB gives about 0.75");
        for (auto [gainDb, mix] : { std::pair { 6.0f, 50.0f }, std::pair { -60.0f, 25.0f }, std::pair { 12.0f, 80.0f } })
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 512);
            setParam (p, "gain", gainDb);
            setParam (p, "mix", mix);
            Buf in = makeNoise (2, 48000), out;
            stream (p, in, out);
            const float g = juce::Decibels::decibelsToGain (gainDb), m = mix * 0.01f;
            float worst = 0.0f;
            for (int i = 24000; i < 48000; ++i)
                worst = juce::jmax (worst, std::abs (out.getSample (0, i) - in.getSample (0, i - pluginLatency) * (1.0f + (g - 1.0f) * m)));
            expect (worst < 1.0e-4f, "gain " + juce::String (gainDb) + " dB, mix " + juce::String (mix) + " %: error " + juce::String (worst));
        }

        beginTest ("a blend with every stage idle is bit-exact (dry and processed paths are aligned)");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 512);
            setParam (p, "mix", 50.0f);
            Buf in = makeNoise (2, 48000), out;
            stream (p, in, out);
            for (int i = 1000; i < 48000; ++i)
                expectEquals (out.getSample (0, i), in.getSample (0, i - pluginLatency));
        }

        beginTest ("dry and saturated paths are time-aligned at 6 kHz (a one-sample error would be about 1.3 dB)");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 512);
            setParam (p, "sat_drive", 6.0f);
            setParam (p, "mix", 50.0f);
            Buf in (1, 96000), out;
            for (int i = 0; i < 96000; ++i)
                in.setSample (0, i, 0.001f * (float) std::sin (2.0 * pi * 6000.0 * i / 48000.0));
            stream (p, in, out);
            const double k = std::pow (10.0, 6.0 / 20.0) - 1.0, a = k / std::tanh (k);
            const double expected = 20.0 * std::log10 (0.5 + 0.5 * a);
            const double measured = 20.0 * std::log10 (rms (out, 48000, 48000) / rms (in, 48000, 48000));
            expectWithinAbsoluteError ((float) measured, (float) expected, 0.15f);
        }

        beginTest ("bypass overrides mix");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 512);
            setParam (p, "gain", 12.0f);
            setParam (p, "mix", 50.0f);
            setParam (p, "bypass", 1.0f);
            Buf in = makeNoise (2, 48000), out;
            stream (p, in, out);
            for (int i = 24000; i < 48000; ++i)
                expectEquals (out.getSample (1, i), in.getSample (1, i - pluginLatency));
        }

        beginTest ("mix changes are ramped (no step), then settle exactly");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 4096);
            setParam (p, "gain", 12.0f);
            Buf dc (2, 8192);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 8192; ++i)
                    dc.setSample (ch, i, 0.25f);
            Buf out;
            stream (p, dc, out, 4096); // settle at 100 % mix
            setParam (p, "mix", 0.0f);
            Buf more, outMore;
            more.makeCopyOf (dc);
            stream (p, more, outMore, 4096);
            float maxStep = 0.0f;
            for (int i = 1; i < 8192; ++i)
                maxStep = juce::jmax (maxStep, std::abs (outMore.getSample (0, i) - outMore.getSample (0, i - 1)));
            expect (maxStep < 0.002f, "max step " + juce::String (maxStep)); // 0.25 * (3.98 - 1) over ~960 samples is ~0.0008
            expectEquals (outMore.getSample (0, 8191), 0.25f);
        }

        beginTest ("state round-trips");
        {
            GlitchKidProcessor p;
            setParam (p, "mix", 37.5f);
            juce::MemoryBlock state;
            p.getStateInformation (state);
            GlitchKidProcessor q;
            q.setStateInformation (state.getData(), (int) state.getSize());
            expectWithinAbsoluteError (getParam (q, "mix"), 37.5f, 0.06f);
        }

        beginTest ("level-matched compare: measures processed vs dry loudness; bypass with matching on plays the dry at that loudness; off stays exact");
        {
            auto rms = [] (const Buf& b, int from) {
                double s = 0.0;
                for (int ch = 0; ch < b.getNumChannels(); ++ch)
                    for (int i = from; i < b.getNumSamples(); ++i)
                        s += (double) b.getSample (ch, i) * b.getSample (ch, i);
                return (float) std::sqrt (s / (double) (b.getNumChannels() * (b.getNumSamples() - from)));
            };
            Buf in = makeNoise (2, 48000 * 12);
            in.applyGain (0.2f); // about -25 dBFS RMS: well inside the limiter's ceiling at +6 dB
            for (float gainDb : { 6.0f, -6.0f })
            {
                GlitchKidProcessor p;
                p.prepareToPlay (48000.0, 512);
                expect (! p.isLevelMatchedCompare(), "off by default");
                setParam (p, "gain", gainDb);
                Buf processed;
                stream (p, in, processed);
                expectWithinAbsoluteError (p.getCompareGainDb(), gainDb, 0.3f, "measured " + juce::String (p.getCompareGainDb()));

                p.setLevelMatchedCompare (true);
                setParam (p, "bypass", 1.0f);
                Buf matched;
                stream (p, in, matched);
                expectWithinAbsoluteError (p.getCompareGainDb(), gainDb, 0.3f, "bypassed blocks never move the measurement");
                const float procDb = juce::Decibels::gainToDecibels (rms (processed, 48000 * 6)),
                            origDb = juce::Decibels::gainToDecibels (rms (matched, 48000));
                expectWithinAbsoluteError (origDb, procDb, 0.5f, "ORIGINAL " + juce::String (origDb) + " dB vs PROCESSED " + juce::String (procDb) + " dB");

                p.setLevelMatchedCompare (false);
                Buf exact;
                stream (p, in, exact);
                for (int i = 48000; i < exact.getNumSamples(); i += 97)
                    for (int ch = 0; ch < 2; ++ch)
                        expectEquals (exact.getSample (ch, i), in.getSample (ch, i - pluginLatency));
            }
        }
    }
};

static MixTests mixTests;
