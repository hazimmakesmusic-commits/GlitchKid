#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include "../Source/DSP/ParametricEq.h"

namespace
{
using Buf = juce::AudioBuffer<float>;

// Deterministic test signal in [-0.5, 0.5].
Buf makeNoise (int channels, int samples, juce::int64 seed = 1)
{
    juce::Random rng (seed);
    Buf b (channels, samples);
    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < samples; ++i)
            b.setSample (ch, i, rng.nextFloat() - 0.5f);
    return b;
}

void setParam (juce::AudioProcessor& p, const char* id, float plainValue)
{
    auto* prm = dynamic_cast<juce::RangedAudioParameter*> (p.getParameters()[0]);
    for (auto* q : p.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (q); r && r->paramID == id)
            prm = r;
    prm->setValueNotifyingHost (prm->convertTo0to1 (plainValue));
}

float getParam (juce::AudioProcessor& p, const char* id)
{
    for (auto* q : p.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (q); r && r->paramID == id)
            return r->convertFrom0to1 (r->getValue());
    return -999.0f;
}

void setLayout (juce::AudioProcessor& p, const juce::AudioChannelSet& set)
{
    juce::AudioProcessor::BusesLayout l;
    l.inputBuses.add (set);
    l.outputBuses.add (set);
    p.setBusesLayout (l);
}

// Run `blocks` blocks of `blockSize`; returns the last processed block and its input.
void run (GlitchKidProcessor& p, int channels, int blockSize, int blocks, Buf& lastIn, Buf& lastOut)
{
    juce::MidiBuffer midi;
    for (int b = 0; b < blocks; ++b)
    {
        lastIn = makeNoise (channels, blockSize, b + 1);
        lastOut.makeCopyOf (lastIn);
        p.processBlock (lastOut, midi);
    }
}

float maxAbsDiff (const Buf& a, const Buf& b, float gain = 1.0f)
{
    float m = 0.0f;
    for (int ch = 0; ch < a.getNumChannels(); ++ch)
        for (int i = 0; i < a.getNumSamples(); ++i)
            m = juce::jmax (m, std::abs (a.getSample (ch, i) * gain - b.getSample (ch, i)));
    return m;
}
} // namespace

struct GlitchKidTests : public juce::UnitTest
{
    GlitchKidTests() : juce::UnitTest ("GlitchKid processor", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("default settings pass audio through bit-exactly (all rates, block sizes)");
        for (double sr : { 44100.0, 48000.0, 96000.0 })
            for (int bs : { 1, 64, 512, 2048 })
            {
                GlitchKidProcessor p;
                p.prepareToPlay (sr, bs);
                Buf in, out;
                run (p, 2, bs, 4, in, out);
                expectEquals (maxAbsDiff (in, out), 0.0f);
            }

        beginTest ("gain produces the expected level change (parameter steps are 0.1 dB)");
        for (float db : { 6.0f, -60.0f, 12.0f, -12.5f })
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 512);
            setParam (p, "gain", db);
            Buf in, out;
            run (p, 2, 512, 8, in, out); // 8 blocks >> 20 ms ramp
            expect (maxAbsDiff (in, out, juce::Decibels::decibelsToGain (db)) < 1.0e-4f, "gain " + juce::String (db) + " dB");
        }

        beginTest ("bypass returns to unity regardless of gain");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 512);
            setParam (p, "gain", 12.0f);
            setParam (p, "bypass", 1.0f);
            Buf in, out;
            run (p, 2, 512, 8, in, out);
            expectEquals (maxAbsDiff (in, out), 0.0f);
        }

        beginTest ("gain and bypass changes are ramped (no sample-to-sample jump larger than the ramp step)");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 4096);
            Buf in (2, 4096), out;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 4096; ++i)
                    in.setSample (ch, i, 0.25f); // DC makes any step in gain visible
            setParam (p, "gain", 12.0f);
            out.makeCopyOf (in);
            juce::MidiBuffer midi;
            p.processBlock (out, midi);
            float maxStep = 0.0f;
            for (int i = 1; i < 4096; ++i)
                maxStep = juce::jmax (maxStep, std::abs (out.getSample (0, i) - out.getSample (0, i - 1)));
            // 0.25 * (3.98 - 1) spread over ~960 samples is ~0.0008 per sample; an unramped jump would be ~0.75.
            expect (maxStep < 0.002f, "max step " + juce::String (maxStep));
        }

        beginTest ("mono layout is supported and processed");
        {
            GlitchKidProcessor p;
            setLayout (p, juce::AudioChannelSet::mono());
            p.prepareToPlay (48000.0, 256);
            setParam (p, "gain", 6.0f);
            Buf in, out;
            run (p, 1, 256, 8, in, out);
            expect (maxAbsDiff (in, out, juce::Decibels::decibelsToGain (6.0f)) < 1.0e-4f);
        }

        beginTest ("mismatched in/out layouts are rejected");
        {
            GlitchKidProcessor p;
            juce::AudioProcessor::BusesLayout l;
            l.inputBuses.add (juce::AudioChannelSet::mono());
            l.outputBuses.add (juce::AudioChannelSet::stereo());
            expect (! p.isBusesLayoutSupported (l));
        }

        beginTest ("silence stays silent");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 512);
            setParam (p, "gain", 12.0f);
            Buf silent (2, 512);
            silent.clear();
            juce::MidiBuffer midi;
            for (int i = 0; i < 4; ++i)
                p.processBlock (silent, midi);
            expectEquals (silent.getMagnitude (0, 512), 0.0f);
        }

        beginTest ("peak meters report input and (post-gain) output peaks, then reset when read");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 512);
            setParam (p, "gain", 6.0f);
            Buf in, out;
            run (p, 2, 512, 8, in, out);
            p.consumeInputPeak(); p.consumeOutputPeak();   // drop history
            run (p, 2, 512, 1, in, out);                   // one settled block
            expectWithinAbsoluteError (p.consumeInputPeak(),  in.getMagnitude (0, 512), 1.0e-6f);
            expectWithinAbsoluteError (p.consumeOutputPeak(), out.getMagnitude (0, 512), 1.0e-6f);
            expectEquals (p.consumeInputPeak(), 0.0f);
        }

        beginTest ("state round-trips; malformed state is ignored");
        {
            GlitchKidProcessor a;
            setParam (a, "gain", -7.5f);
            setParam (a, "bypass", 1.0f);
            juce::MemoryBlock state;
            a.getStateInformation (state);

            GlitchKidProcessor b;
            b.setStateInformation (state.getData(), (int) state.getSize());
            expectWithinAbsoluteError (getParam (b, "gain"), -7.5f, 0.05f);
            expectEquals (getParam (b, "bypass"), 1.0f);

            GlitchKidProcessor c;
            const char junk[] = "this is not plugin state";
            c.setStateInformation (junk, (int) sizeof (junk));
            juce::MemoryBlock empty;
            c.setStateInformation (empty.getData(), 0);
            expectWithinAbsoluteError (getParam (c, "gain"), 0.0f, 0.05f); // parameter grid may leave ~1e-6 residue
            expectEquals (getParam (c, "bypass"), 0.0f);
        }
    }
};

static GlitchKidTests glitchKidTests;

// ---- EQ tests -------------------------------------------------------------------------------
namespace
{
// Level change in dB of a steady sine at `freq` through the EQ (measured after the filter settles).
float eqGainDbAt (ParametricEq& eq, double sr, float freq)
{
    const int n = (int) sr; // 1 s
    Buf b (2, n);
    for (int i = 0; i < n; ++i)
    {
        const float v = 0.25f * std::sin (juce::MathConstants<float>::twoPi * freq * (float) i / (float) sr);
        b.setSample (0, i, v);
        b.setSample (1, i, v);
    }
    Buf ref;
    ref.makeCopyOf (b);
    eq.process (b);
    return juce::Decibels::gainToDecibels (b.getRMSLevel (0, n / 2, n / 2) / ref.getRMSLevel (0, n / 2, n / 2));
}

ParametricEq makeEq (double sr, int band, EqBandSettings s)
{
    ParametricEq eq;
    eq.setBand (band, s);
    eq.prepare (sr);
    return eq;
}
} // namespace

struct ParametricEqTests : public juce::UnitTest
{
    ParametricEqTests() : juce::UnitTest ("Parametric EQ", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("flat EQ is bit-exact");
        {
            ParametricEq eq;
            eq.prepare (48000.0);
            Buf in = makeNoise (2, 4096), out;
            out.makeCopyOf (in);
            eq.process (out);
            expectEquals (maxAbsDiff (in, out), 0.0f);
        }

        for (double sr : { 44100.0, 48000.0, 96000.0 })
        {
            beginTest ("bell +6 / -6 dB at 1 kHz, Q 1 (" + juce::String (sr) + " Hz)");
            for (float g : { 6.0f, -6.0f })
            {
                auto eq = makeEq (sr, 1, { 1000.0f, g, 1.0f });
                expectWithinAbsoluteError (eqGainDbAt (eq, sr, 1000.0f), g, 0.15f);
                auto eq2 = makeEq (sr, 1, { 1000.0f, g, 1.0f });
                expectWithinAbsoluteError (eqGainDbAt (eq2, sr, 100.0f), 0.0f, 0.5f);
                auto eq3 = makeEq (sr, 1, { 1000.0f, g, 1.0f });
                expectWithinAbsoluteError (eqGainDbAt (eq3, sr, 10000.0f), 0.0f, 0.7f);
            }
        }

        beginTest ("narrow bell (Q 8) affects fewer frequencies than wide bell (Q 0.5)");
        {
            auto narrow = makeEq (48000.0, 2, { 3000.0f, 9.0f, 8.0f });
            auto wide   = makeEq (48000.0, 2, { 3000.0f, 9.0f, 0.5f });
            expect (eqGainDbAt (narrow, 48000.0, 4500.0f) < eqGainDbAt (wide, 48000.0, 4500.0f) - 3.0f);
        }

        beginTest ("low shelf +6 dB at 100 Hz: +6 well below, +3 at the corner, 0 well above");
        {
            auto a = makeEq (48000.0, 0, { 100.0f, 6.0f, 0.7071f });
            auto b = makeEq (48000.0, 0, { 100.0f, 6.0f, 0.7071f });
            auto c = makeEq (48000.0, 0, { 100.0f, 6.0f, 0.7071f });
            expectWithinAbsoluteError (eqGainDbAt (a, 48000.0, 20.0f), 6.0f, 0.5f);
            expectWithinAbsoluteError (eqGainDbAt (b, 48000.0, 100.0f), 3.0f, 0.4f);
            expectWithinAbsoluteError (eqGainDbAt (c, 48000.0, 5000.0f), 0.0f, 0.2f);
        }

        beginTest ("high shelf -6 dB at 8 kHz: 0 well below, -3 at the corner, -6 well above");
        {
            auto a = makeEq (96000.0, 3, { 8000.0f, -6.0f, 0.7071f });
            auto b = makeEq (96000.0, 3, { 8000.0f, -6.0f, 0.7071f });
            auto c = makeEq (96000.0, 3, { 8000.0f, -6.0f, 0.7071f });
            expectWithinAbsoluteError (eqGainDbAt (a, 96000.0, 200.0f), 0.0f, 0.2f);
            expectWithinAbsoluteError (eqGainDbAt (b, 96000.0, 8000.0f), -3.0f, 0.4f);
            expectWithinAbsoluteError (eqGainDbAt (c, 96000.0, 30000.0f), -6.0f, 0.5f);
        }

        beginTest ("left and right channels match for identical input");
        {
            ParametricEq eq;
            eq.setBand (1, { 1000.0f, 6.0f, 1.0f });
            eq.prepare (48000.0);
            Buf b (2, 2048);
            auto n = makeNoise (1, 2048);
            b.copyFrom (0, 0, n, 0, 0, 2048);
            b.copyFrom (1, 0, n, 0, 0, 2048);
            eq.process (b);
            for (int i = 0; i < 2048; ++i)
                expectEquals (b.getSample (0, i), b.getSample (1, i));
        }

        beginTest ("mono buffers are supported");
        {
            ParametricEq eq;
            eq.setBand (1, { 1000.0f, 6.0f, 1.0f });
            eq.prepare (48000.0);
            Buf b = makeNoise (1, 1024);
            eq.process (b);
            expect (std::isfinite (b.getMagnitude (0, 1024)));
        }

        beginTest ("extreme settings and moving parameters stay finite and bounded");
        for (double sr : { 44100.0, 96000.0 })
        {
            ParametricEq eq;
            eq.prepare (sr);
            juce::Random rng (7);
            float peak = 0.0f;
            for (int blk = 0; blk < 400; ++blk)
            {
                for (int band = 0; band < ParametricEq::numBands; ++band)
                    eq.setBand (band, { 20.0f + rng.nextFloat() * 20000.0f, (rng.nextFloat() - 0.5f) * 30.0f,
                                        0.3f + rng.nextFloat() * 9.7f });
                Buf b = makeNoise (2, 256, blk);
                eq.process (b);
                peak = juce::jmax (peak, b.getMagnitude (0, 256));
                expect (std::isfinite (peak), "non-finite output at block " + juce::String (blk));
            }
            expect (peak < 200.0f, "peak " + juce::String (peak)); // +60 dB of stacked boost is 1000x; random walk stays far below
        }

        beginTest ("processor: EQ is part of the signal path, bypass removes it, bands are in saved state");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 512);
            setParam (p, "eq_b1_freq", 1000.0f);
            setParam (p, "eq_b1_gain", 6.0f);
            setParam (p, "eq_b1_q", 1.0f);
            // 1 kHz sine, 1 s
            Buf in (2, 512), out;
            juce::MidiBuffer midi;
            float rms = 0.0f, rmsIn = 0.0f;
            for (int blk = 0; blk < 94; ++blk)
            {
                for (int i = 0; i < 512; ++i)
                {
                    const float v = 0.25f * std::sin (juce::MathConstants<float>::twoPi * 1000.0f * (float) (blk * 512 + i) / 48000.0f);
                    in.setSample (0, i, v);
                    in.setSample (1, i, v);
                }
                out.makeCopyOf (in);
                p.processBlock (out, midi);
                rms = out.getRMSLevel (0, 0, 512);
                rmsIn = in.getRMSLevel (0, 0, 512);
            }
            expectWithinAbsoluteError (juce::Decibels::gainToDecibels (rms / rmsIn), 6.0f, 0.3f);

            setParam (p, "bypass", 1.0f);
            for (int blk = 0; blk < 8; ++blk)
            {
                out.makeCopyOf (in);
                p.processBlock (out, midi);
            }
            expectEquals (maxAbsDiff (in, out), 0.0f);

            juce::MemoryBlock state;
            p.getStateInformation (state);
            GlitchKidProcessor q;
            q.setStateInformation (state.getData(), (int) state.getSize());
            expectWithinAbsoluteError (getParam (q, "eq_b1_gain"), 6.0f, 0.05f);
            expectWithinAbsoluteError (getParam (q, "eq_b1_freq"), 1000.0f, 1.0f);
            expectWithinAbsoluteError (getParam (q, "eq_b1_q"), 1.0f, 0.02f);
        }
    }
};

static ParametricEqTests parametricEqTests;


int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult (i)->failures;

    std::printf ("%s: %d failure(s)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
