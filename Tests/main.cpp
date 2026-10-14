#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"

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
            expectEquals (getParam (c, "gain"), 0.0f);
            expectEquals (getParam (c, "bypass"), 0.0f);
        }
    }
};

static GlitchKidTests glitchKidTests;

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
