#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/Master/MasterProcessor.h"
#include "../Source/Master/MasterEditor.h"
#include "../Source/PluginProcessor.h"

long threadAllocations(); // FormantShiftTests.cpp replaces the global operator new and counts per thread

// GLITCHKID MASTER, the separate plug-in: identity and layouts, routing, latency (the oversampled saturation and the look-ahead true-peak limiter),
// bypass, state, automation, loudness and true peak, the ceiling, stereo safety, NaN / Inf, sample rates, block sizes, allocation and CPU, the interface.
// The mastering DSP itself (BS.1770, the limiter, width, the styles) is tested module by module in "Mastering".
namespace
{
using Buf = juce::AudioBuffer<float>;
using namespace glitchkid::masterplugin;
constexpr double twoPi = juce::MathConstants<double>::twoPi;

void setParam (MasterProcessor& p, const char* id, float plain)
{
    auto* r = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (id));
    r->setValueNotifyingHost (r->convertTo0to1 (plain));
}
float getParam (MasterProcessor& p, const char* id)
{
    auto* r = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (id));
    return r->convertFrom0to1 (r->getValue());
}

// music-like: pink-ish noise with more low end, optional clicks and stereo spread, scaled to a sample peak
Buf mix (double fs, double seconds, float peakDb, float stereo = 0.3f, bool clicks = false, juce::int64 seed = 7, int channels = 2)
{
    juce::Random rng (seed);
    Buf b (channels, (int) (fs * seconds));
    double l1 = 0, l2 = 0, r1 = 0, r2 = 0;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double c = rng.nextFloat() * 2.0 - 1.0, uL = rng.nextFloat() * 2.0 - 1.0, uR = rng.nextFloat() * 2.0 - 1.0;
        const double nl = (1.0 - stereo) * c + stereo * uL, nr = (1.0 - stereo) * c + stereo * uR;
        l1 += 0.05 * (nl - l1); l2 += 0.005 * (nl - l2);
        r1 += 0.05 * (nr - r1); r2 += 0.005 * (nr - r2);
        double l = 0.3 * nl + 1.5 * l1 + 4.0 * l2, r = 0.3 * nr + 1.5 * r1 + 4.0 * r2;
        if (clicks && i % (int) (fs * 0.25) < (int) (fs * 0.002)) { const double k = (i % 2 ? 1.0 : -1.0) * 3.0; l += k; r += k; }
        b.setSample (0, i, (float) l);
        if (channels > 1) b.setSample (1, i, (float) r);
    }
    float pk = 0.0f;
    for (int ch = 0; ch < channels; ++ch) pk = juce::jmax (pk, b.getMagnitude (ch, 0, b.getNumSamples()));
    b.applyGain (juce::Decibels::decibelsToGain (peakDb) / juce::jmax (1.0e-9f, pk));
    return b;
}

// a mastered-like top end: a 4th-order low-pass at 18 kHz (real masters carry little above it), then back to the same sample peak
Buf bandLimited (Buf b, double fs)
{
    float before = 0.0f, after = 0.0f;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
    {
        before = juce::jmax (before, b.getMagnitude (ch, 0, b.getNumSamples()));
        juce::IIRFilter f1, f2;
        f1.setCoefficients (juce::IIRCoefficients::makeLowPass (fs, 18000.0, 0.54));
        f2.setCoefficients (juce::IIRCoefficients::makeLowPass (fs, 18000.0, 1.31));
        f1.processSamples (b.getWritePointer (ch), b.getNumSamples());
        f2.processSamples (b.getWritePointer (ch), b.getNumSamples());
        after = juce::jmax (after, b.getMagnitude (ch, 0, b.getNumSamples()));
    }
    b.applyGain (before / juce::jmax (1.0e-9f, after));
    return b;
}

Buf silence (int channels, int samples)
{
    Buf b (channels, samples);
    b.clear(); // a new juce::AudioBuffer is not zeroed
    return b;
}

Buf run (MasterProcessor& p, const Buf& in, int block)
{
    Buf out (in);
    juce::MidiBuffer midi;
    for (int s = 0; s < out.getNumSamples(); s += block)
    {
        const int n = juce::jmin (block, out.getNumSamples() - s);
        Buf v (out.getArrayOfWritePointers(), out.getNumChannels(), s, n);
        p.processBlock (v, midi);
    }
    return out;
}

void setLayout (MasterProcessor& p, const juce::AudioChannelSet& set)
{
    juce::AudioProcessor::BusesLayout l;
    l.inputBuses.add (set);
    l.outputBuses.add (set);
    p.setBusesLayout (l);
}

// an independent true-peak estimate: 16x band-limited interpolation (windowed sinc, 64 taps per phase) of the whole signal
float truePeakDb (const Buf& b, int skip)
{
    constexpr int over = 16, half = 32;
    float peak = 0.0f;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
    {
        const float* x = b.getReadPointer (ch);
        for (int i = skip + half; i < b.getNumSamples() - half; ++i)
        {
            peak = juce::jmax (peak, std::abs (x[i]));
            for (int ph = 1; ph < over; ++ph)
            {
                const double t = (double) ph / over;
                double acc = 0.0;
                for (int k = -half + 1; k <= half; ++k)
                {
                    const double d = t - k, w = 0.5 + 0.5 * std::cos (juce::MathConstants<double>::pi * d / half);
                    acc += x[i + k] * (std::abs (d) < 1e-12 ? 1.0 : std::sin (juce::MathConstants<double>::pi * d) / (juce::MathConstants<double>::pi * d)) * w;
                }
                peak = juce::jmax (peak, (float) std::abs (acc));
            }
        }
    }
    return juce::Decibels::gainToDecibels (peak, -200.0f);
}

struct MasterPluginTests : public juce::UnitTest
{
    MasterPluginTests() : juce::UnitTest ("GlitchKid Master (plug-in)", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("identity: its own name, parameters (gkm_*, in a fixed order) and host bypass; mono or stereo, in = out; the vocal plug-in is untouched");
        {
            MasterProcessor p;
            expectEquals (p.getName(), juce::String ("GlitchKid Master"));
            const char* expected[] = { ids::style, ids::intensity, ids::input, ids::loudness, ids::tone, ids::width, ids::ceiling, ids::output, ids::bypass };
            expectEquals (p.getParameters().size(), 9);
            for (int i = 0; i < 9; ++i)
                expectEquals (dynamic_cast<juce::RangedAudioParameter*> (p.getParameters()[i])->paramID, juce::String (expected[i]));
            expect (p.getBypassParameter() == p.apvts.getParameter (ids::bypass));
            using CS = juce::AudioChannelSet;
            auto layout = [] (const CS& in, const CS& out) { juce::AudioProcessor::BusesLayout l; l.inputBuses.add (in); l.outputBuses.add (out); return l; };
            expect (p.checkBusesLayoutSupported (layout (CS::stereo(), CS::stereo())));
            expect (p.checkBusesLayoutSupported (layout (CS::mono(), CS::mono())));
            expect (! p.checkBusesLayoutSupported (layout (CS::mono(), CS::stereo())), "in must equal out");
            expect (! p.checkBusesLayoutSupported (layout (CS::create5point1(), CS::create5point1())), "no surround");
            GlitchKidProcessor vocal;
            expectEquals (vocal.getParameters().size(), 291, "the vocal plug-in's parameters are unchanged");
            for (auto* prm : vocal.getParameters())
                expect (! dynamic_cast<juce::RangedAudioParameter*> (prm)->paramID.startsWith ("gkm_"));
        }

        beginTest ("latency: reported, constant across every setting, and exactly where an impulse comes out (44.1 / 48 / 96 kHz)");
        for (double fs : { 44100.0, 48000.0, 96000.0 })
        {
            MasterProcessor p;
            p.prepareToPlay (fs, 512);
            const int lat = p.getLatencySamples();
            expect (lat > 0 && lat == p.getEngine().latencySamples(), "reported");
            setParam (p, ids::intensity, 0.0f);
            setParam (p, ids::loudness, 0.0f);
            Buf in (2, (int) fs);
            in.clear();
            in.setSample (0, 1000, 0.1f);
            in.setSample (1, 1000, 0.1f);
            const auto out = run (p, in, 512);
            int at = 0;
            for (int i = 0; i < out.getNumSamples(); ++i)
                if (std::abs (out.getSample (0, i)) > std::abs (out.getSample (0, at))) at = i;
            expectEquals (at - 1000, lat, "the impulse arrives exactly the reported latency later at " + juce::String (fs));
            for (int style = 0; style < 4; ++style)
            {
                setParam (p, ids::style, (float) style);
                setParam (p, ids::intensity, 100.0f);
                p.prepareToPlay (fs, 512);
                expectEquals (p.getLatencySamples(), lat, "the same latency for every style (the saturation's delay is kept when it is off)");
            }
        }

        beginTest ("bypass: the untouched input, delayed by exactly the latency (after the 20 ms fade), and back without a jump");
        {
            MasterProcessor p;
            p.prepareToPlay (48000.0, 480);
            setParam (p, ids::loudness, 8.0f);
            Buf in (2, 96000);
            for (int i = 0; i < in.getNumSamples(); ++i)
                for (int ch = 0; ch < 2; ++ch) in.setSample (ch, i, 0.5f * (float) std::sin (twoPi * 200.0 * i / 48000.0 + ch));
            setParam (p, ids::bypass, 1.0f);
            const auto out = run (p, in, 480);
            const int lat = p.getLatencySamples();
            float diff = 0.0f;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 4800; i < in.getNumSamples(); ++i)
                    diff = juce::jmax (diff, std::abs (out.getSample (ch, i) - in.getSample (ch, i - lat)));
            expectEquals (diff, 0.0f, "bit-exact delayed input");
            setParam (p, ids::bypass, 0.0f);
            Buf more (2, 24000);
            for (int i = 0; i < more.getNumSamples(); ++i)
                for (int ch = 0; ch < 2; ++ch) more.setSample (ch, i, 0.5f * (float) std::sin (twoPi * 200.0 * (i + 96000) / 48000.0 + ch));
            const auto back = run (p, more, 480);
            float jump = 0.0f;
            for (int i = 1; i < back.getNumSamples(); ++i)
                jump = juce::jmax (jump, std::abs (back.getSample (0, i) - back.getSample (0, i - 1)));
            const float slope = 0.5f * (float) (twoPi * 200.0 / 48000.0); // the tone's own steepest step, before the master's drive
            expect (jump < slope * 3.0f, "no step when bypass is switched off (a 20 ms crossfade): " + juce::String (jump));
        }

        beginTest ("routing: left stays left, right stays right; a mono-compatible mix stays mono (L = R); mono layout works");
        {
            MasterProcessor p;
            p.prepareToPlay (48000.0, 512);
            setParam (p, ids::style, 3.0f); // Wide & Atmospheric: the widest style
            setParam (p, ids::intensity, 100.0f);
            setParam (p, ids::width, 200.0f);
            auto in = mix (48000.0, 2.0, -3.0f, 0.0f);
            for (int i = 0; i < in.getNumSamples(); ++i) in.setSample (1, i, 0.0f); // left only
            const auto out = run (p, in, 512);
            const float r = out.getMagnitude (1, 0, out.getNumSamples()), l = out.getMagnitude (0, 0, out.getNumSamples());
            expect (l > 0.1f, "the left channel carries the mix");
            // width adds side above 120 Hz to both channels with opposite signs: a left-only mix legitimately spreads (it is a side signal); it must not swap
            expect (r < l, "never swapped: " + juce::String (r) + " vs " + juce::String (l));

            MasterProcessor q;
            q.prepareToPlay (48000.0, 512);
            setParam (q, ids::style, 3.0f);
            setParam (q, ids::intensity, 100.0f);
            setParam (q, ids::width, 200.0f);
            const auto monoMix = mix (48000.0, 2.0, -3.0f, 0.0f); // L = R
            const auto o2 = run (q, monoMix, 512);
            float d = 0.0f;
            for (int i = 0; i < o2.getNumSamples(); ++i) d = juce::jmax (d, std::abs (o2.getSample (0, i) - o2.getSample (1, i)));
            expect (d < 1.0e-6f, "L = R in gives L = R out at the widest setting: " + juce::String (d));

            MasterProcessor m;
            setLayout (m, juce::AudioChannelSet::mono());
            m.prepareToPlay (48000.0, 512);
            setParam (m, ids::loudness, 8.0f);
            const auto mo = run (m, bandLimited (mix (48000.0, 2.0, -1.0f, 0.0f, true, 3, 1), 48000.0), 512);
            bool finite = true;
            for (int i = 0; i < mo.getNumSamples(); ++i) finite = finite && std::isfinite (mo.getSample (0, i));
            expect (finite && mo.getMagnitude (0, 0, mo.getNumSamples()) > 0.1f, "mono: processed");
            expect (truePeakDb (mo, 0) <= -1.0f + 0.1f, "mono: under the ceiling: " + juce::String (truePeakDb (mo, 0), 2));
        }

        beginTest ("the ceiling holds (independent 16x true-peak check): hot, transient, bright material with a tone near a quarter of the rate, at -1, -3 and -0.3 dBTP");
        for (float ceiling : { -1.0f, -3.0f, -0.3f })
        {
            MasterProcessor p;
            p.prepareToPlay (48000.0, 512);
            setParam (p, ids::style, 2.0f); // Loud & Punchy
            setParam (p, ids::intensity, 100.0f);
            setParam (p, ids::loudness, 10.0f);
            setParam (p, ids::input, 6.0f);
            setParam (p, ids::ceiling, ceiling);
            auto in = bandLimited (mix (48000.0, 3.0, 0.0f, 0.4f, true), 48000.0);
            for (int i = 0; i < in.getNumSamples(); ++i) // a loud tone near a quarter of the sample rate, phased for inter-sample peaks
                for (int ch = 0; ch < 2; ++ch)
                    in.setSample (ch, i, in.getSample (ch, i) * 0.6f + 0.4f * (float) std::sin (twoPi * 11990.0 * i / 48000.0 + 0.785));
            const auto out = run (p, in, 512);
            const float tp = truePeakDb (out, 2400);
            logMessage ("GKM true peak at ceiling " + juce::String (ceiling, 1) + ": " + juce::String (tp, 2) + " dBTP (independent 16x)");
            expect (tp <= ceiling + 0.1f, "true peak " + juce::String (tp, 2) + " dBTP under " + juce::String (ceiling, 1));
            expect (out.getMagnitude (0, 0, out.getNumSamples()) <= juce::Decibels::decibelsToGain (ceiling) + 1.0e-6f, "sample peak under the ceiling");
            expect (p.getEngine().outputMeter().truePeakDb() <= ceiling + 0.15f, "the plug-in's own true-peak reading agrees");
            expectEquals (p.getEngine().outputMeter().clippedSamples(), 0, "nothing clipped");
        }

        beginTest ("documented limit: material with strong energy right up to Nyquist (white noise, full-scale Nyquist bursts) can exceed the ceiling between samples");
        {
            MasterProcessor p;
            p.prepareToPlay (48000.0, 512);
            setParam (p, ids::loudness, 10.0f);
            const auto out = run (p, mix (48000.0, 3.0, 0.0f, 0.4f, true), 512); // unfiltered: 5 % of the energy above 20 kHz, plus Nyquist bursts
            const float tp = truePeakDb (out, 2400);
            logMessage ("GKM extreme material (not music): true peak " + juce::String (tp, 2) + " dBTP at a -1 dBTP ceiling; samples " + juce::String (juce::Decibels::gainToDecibels (out.getMagnitude (0, 0, out.getNumSamples())), 2));
            expect (out.getMagnitude (0, 0, out.getNumSamples()) <= juce::Decibels::decibelsToGain (-1.0f) + 1.0e-6f, "the samples themselves still hold");
            expect (tp + 1.0f < 2.5f, "bounded, at most 2.5 dB over (see docs/MASTER.md): " + juce::String (tp + 1.0f, 2) + " dB over");
        }

        beginTest ("loudness readings are the real BS.1770 values: a -23 dBFS 1 kHz stereo sine reads -23 LUFS on the input; unavailable before the gate");
        {
            MasterProcessor p;
            p.prepareToPlay (48000.0, 512);
            expect (p.getEngine().inputMeter().integratedLufs() <= -99.0f, "nothing measured yet: not a number");
            Buf s (2, 48000 * 6);
            const float a = juce::Decibels::decibelsToGain (-23.0f);
            for (int i = 0; i < s.getNumSamples(); ++i)
                for (int ch = 0; ch < 2; ++ch) s.setSample (ch, i, a * (float) std::sin (twoPi * 1000.0 * i / 48000.0));
            run (p, s, 512);
            expectWithinAbsoluteError (p.getEngine().inputMeter().integratedLufs(), -23.0f, 0.1f, "integrated");
            expectWithinAbsoluteError (p.getEngine().inputMeter().shortTermLufs(), -23.0f, 0.1f, "short-term");
            expectWithinAbsoluteError (p.getEngine().inputMeter().momentaryLufs(), -23.0f, 0.1f, "momentary");
            setParam (p, ids::input, -6.0f); // the INPUT meters read after the INPUT trim
            run (p, s, 512);
            expectWithinAbsoluteError (p.getEngine().inputMeter().shortTermLufs(), -29.0f, 0.15f, "after INPUT -6 dB");
            p.requestMeterReset();
            run (p, silence (2, 512), 512);
            expect (p.getEngine().inputMeter().integratedLufs() <= -99.0f, "reset: integrated starts again");
        }

        beginTest ("state: every parameter and A / B saved and restored; garbage changes nothing; a missing value takes its default; ORIGINAL is never saved");
        {
            MasterProcessor p;
            setParam (p, ids::style, 1.0f);
            setParam (p, ids::intensity, 80.0f);
            setParam (p, ids::input, -2.5f);
            setParam (p, ids::loudness, 6.0f);
            setParam (p, ids::tone, -30.0f);
            setParam (p, ids::width, 140.0f);
            setParam (p, ids::ceiling, -2.0f);
            setParam (p, ids::output, -1.5f);
            p.storeSlot (0);
            setParam (p, ids::loudness, 2.0f);
            p.storeSlot (1);
            p.setCompareOriginal (true);
            juce::MemoryBlock mb;
            p.getStateInformation (mb);
            MasterProcessor q;
            q.setStateInformation (mb.getData(), (int) mb.getSize());
            for (auto* id : { ids::style, ids::intensity, ids::input, ids::loudness, ids::tone, ids::width, ids::ceiling, ids::output })
                expectWithinAbsoluteError (getParam (q, id), getParam (p, id), 0.02f, id);
            expect (q.hasSlot (0) && q.hasSlot (1) && q.activeSlot() == 1);
            expect (! q.isCompareOriginal(), "ORIGINAL is a listening switch: a session never opens on it");
            expect (q.recallSlot (0));
            expectWithinAbsoluteError (getParam (q, ids::loudness), 6.0f, 0.02f, "A recalled");
            expectWithinAbsoluteError (getParam (q, ids::style), 1.0f, 0.01f);

            const char junk[] = "not a GlitchKid Master state";
            q.setStateInformation (junk, (int) sizeof (junk));
            expectWithinAbsoluteError (getParam (q, ids::loudness), 6.0f, 0.02f, "garbage changes nothing");
            juce::ValueTree old ("GlitchKidMaster"); // an older state without most parameters
            juce::ValueTree prm ("PARAM");
            prm.setProperty ("id", ids::tone, nullptr);
            prm.setProperty ("value", 50.0f, nullptr);
            old.appendChild (prm, nullptr);
            juce::MemoryBlock mb2;
            juce::AudioProcessor::copyXmlToBinary (*old.createXml(), mb2);
            q.setStateInformation (mb2.getData(), (int) mb2.getSize());
            expectWithinAbsoluteError (getParam (q, ids::tone), 50.0f, 0.1f);
            expectWithinAbsoluteError (getParam (q, ids::loudness), 3.0f, 0.02f, "missing: default");
            expect (! q.hasSlot (0), "no slots in that state");
        }

        beginTest ("automation: every parameter reaches the DSP; level changes are smoothed (no click); the vocal plug-in's state is independent");
        {
            MasterProcessor p;
            p.prepareToPlay (48000.0, 256);
            setParam (p, ids::ceiling, -2.0f);
            setParam (p, ids::style, 3.0f);
            setParam (p, ids::tone, 40.0f);
            setParam (p, ids::width, 50.0f);
            setParam (p, ids::output, -3.0f);
            setParam (p, ids::intensity, 20.0f);
            setParam (p, ids::loudness, 7.0f);
            const auto c = p.readControls();
            expect ((int) c.style == 3 && std::abs (c.tone - 0.4f) < 1e-3f && std::abs (c.width - 0.5f) < 1e-3f && std::abs (c.ceilingDbTp + 2.0f) < 1e-3f
                    && std::abs (c.outputDb + 3.0f) < 1e-3f && std::abs (c.intensity - 0.2f) < 1e-3f && std::abs (c.loudnessDb - 7.0f) < 1e-3f, "controls read from the parameters");
            setParam (p, ids::output, 0.0f);
            setParam (p, ids::input, 0.0f);
            Buf tone (2, 48000);
            for (int i = 0; i < tone.getNumSamples(); ++i)
                for (int ch = 0; ch < 2; ++ch) tone.setSample (ch, i, 0.2f * (float) std::sin (twoPi * 200.0 * i / 48000.0));
            setParam (p, ids::loudness, 0.0f);
            run (p, tone, 256);
            juce::MidiBuffer midi;
            float worst = 0.0f, prev = 0.0f;
            int written = 0;
            for (int b = 0; b < 40; ++b)
            {
                if (b == 10) setParam (p, ids::output, -12.0f);
                if (b == 20) setParam (p, ids::input, 12.0f);
                if (b == 30) setParam (p, ids::input, -12.0f);
                Buf v (2, 256);
                for (int i = 0; i < 256; ++i)
                    for (int ch = 0; ch < 2; ++ch) v.setSample (ch, i, 0.2f * (float) std::sin (twoPi * 200.0 * (48000 + b * 256 + i) / 48000.0));
                p.processBlock (v, midi);
                for (int i = 0; i < 256; ++i, ++written)
                {
                    if (written > 0) worst = juce::jmax (worst, std::abs (v.getSample (0, i) - prev));
                    prev = v.getSample (0, i);
                }
            }
            const float maxSlope = (float) (twoPi * 200.0 / 48000.0) * 0.9f; // the steepest a 200 Hz tone under the ceiling can move per sample, with margin
            expect (worst < maxSlope * 1.5f, "no step larger than the tone's own slope: " + juce::String (worst));
        }

        beginTest ("NaN and Inf input: the output stays finite, the plug-in says so, and it recovers at once");
        {
            MasterProcessor p;
            p.prepareToPlay (48000.0, 512);
            auto in = mix (48000.0, 1.0, -3.0f);
            for (int i = 1000; i < 1100; ++i) in.setSample (0, i, std::numeric_limits<float>::quiet_NaN());
            in.setSample (1, 5000, std::numeric_limits<float>::infinity());
            in.setSample (0, 7000, -std::numeric_limits<float>::infinity());
            const auto out = run (p, in, 512);
            bool finite = true;
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < out.getNumSamples(); ++i) finite = finite && std::isfinite (out.getSample (ch, i));
            expect (finite, "finite output");
            expect (p.nonFiniteBlocks() > 0, "counted for the interface");
            const auto after = run (p, mix (48000.0, 1.0, -3.0f, 0.3f, false, 9), 512);
            expect (after.getMagnitude (0, 24000, 24000) > 0.05f, "audio flows again (no filter stuck)");
        }

        beginTest ("sample rates 44.1 - 192 kHz: finite, under the ceiling, the latency scales with the look-ahead");
        for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        {
            MasterProcessor p;
            p.prepareToPlay (fs, 512);
            setParam (p, ids::loudness, 8.0f);
            const auto out = run (p, mix (fs, 1.5, 0.0f, 0.3f, true), 512);
            bool finite = true;
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < out.getNumSamples(); ++i) finite = finite && std::isfinite (out.getSample (ch, i));
            expect (finite, juce::String (fs));
            expect (out.getMagnitude (0, 0, out.getNumSamples()) <= juce::Decibels::decibelsToGain (-1.0f) + 1e-6f, "sample peak at " + juce::String (fs));
            expect (p.getLatencySamples() >= (int) (fs * 0.002), "latency covers the 2 ms look-ahead at " + juce::String (fs));
        }

        beginTest ("block sizes 1 - 8192 (also larger than the host announced) give the same master");
        {
            const auto in = mix (48000.0, 2.0, -1.0f, 0.3f, true);
            auto render = [&] (int block)
            {
                MasterProcessor p;
                p.prepareToPlay (48000.0, 512); // the host announces 512, then may send anything
                setParam (p, ids::style, 2.0f);
                setParam (p, ids::intensity, 100.0f);
                setParam (p, ids::loudness, 6.0f);
                return run (p, in, block);
            };
            const auto ref = render (512);
            for (int block : { 1, 7, 64, 1000, 4096, 8192 })
            {
                const auto o = render (block);
                float d = 0.0f;
                for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < o.getNumSamples(); ++i) d = juce::jmax (d, std::abs (o.getSample (ch, i) - ref.getSample (ch, i)));
                logMessage ("GKM block " + juce::String (block) + ": largest difference from 512 = " + juce::String (juce::Decibels::gainToDecibels (d, -200.0f), 1) + " dBFS");
                expect (d < juce::Decibels::decibelsToGain (-60.0f), "block " + juce::String (block) + ": " + juce::String (juce::Decibels::gainToDecibels (d, -200.0f), 1) + " dBFS");
            }
        }

        beginTest ("audio thread: no allocation (also for blocks larger than announced); CPU a small fraction of real time");
        {
            MasterProcessor p;
            p.prepareToPlay (48000.0, 512);
            setParam (p, ids::style, 1.0f); // Warm: saturation on (the oversampler)
            setParam (p, ids::intensity, 100.0f);
            juce::MidiBuffer midi;
            const auto src = mix (48000.0, 1.0, -1.0f);
            long allocs = 0;
            for (int block : { 512, 64, 3000 })
            {
                Buf b (2, block);
                for (int k = 0; k < 5; ++k) { for (int ch = 0; ch < 2; ++ch) b.copyFrom (ch, 0, src, ch, k * block % 20000, block); p.processBlock (b, midi); }
                for (int k = 0; k < 20; ++k)
                {
                    for (int ch = 0; ch < 2; ++ch) b.copyFrom (ch, 0, src, ch, k * block % 20000, block);
                    const long before = threadAllocations();
                    p.processBlock (b, midi);
                    allocs += threadAllocations() - before;
                }
            }
            expectEquals (allocs, 0L, "allocations in processBlock");
            const auto ten = mix (48000.0, 10.0, -1.0f);
            const double t0 = juce::Time::getMillisecondCounterHiRes();
            run (p, ten, 512);
            const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
            logMessage ("GKM CPU: " + juce::String (ms / 100.0, 2) + " % of one core (10 s of stereo at 48 kHz)");
            expect (ms < 10000.0 * 0.25, "under a quarter of real time even on a slow test machine");
        }

        beginTest ("interface: seven controls on real parameters, the styles, A/B, bypass, ORIGINAL; honest readings and status; no Export button");
        {
            MasterProcessor p;
            p.prepareToPlay (48000.0, 512);
            std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
            auto* ed = dynamic_cast<MasterEditor*> (e.get());
            expect (ed != nullptr);
            ed->tickForTest();
            ed->tickForTest();
            expect (ed->statusForTest().startsWith ("NO AUDIO"), ed->statusForTest());
            expectEquals (ed->readoutForTest (false, 2), juce::String ("-"), "integrated: not available yet, and says so");
            const char* knobIds[] = { ids::input, ids::intensity, ids::loudness, ids::tone, ids::width, ids::ceiling, ids::output };
            expectEquals (ed->numKnobsForTest(), 7);
            for (int i = 0; i < 7; ++i)
            {
                auto& s = ed->knobForTest (i);
                const double target = s.getMinimum() + 0.25 * (s.getMaximum() - s.getMinimum());
                s.setValue (target, juce::sendNotificationSync);
                expectWithinAbsoluteError (getParam (p, knobIds[i]), (float) target, 0.05f, ed->knobNameForTest (i));
            }
            p.resetToDefaults();
            ed->pressForTest ("Loud & Punchy");
            expectWithinAbsoluteError (getParam (p, ids::style), 2.0f, 0.01f, "a style tile sets the style");

            run (p, mix (48000.0, 2.0, -3.0f), 512);
            ed->tickForTest();
            expectEquals (ed->statusForTest(), juce::String ("MASTERING THE MIX"));
            expect (ed->readoutForTest (false, 2).endsWith ("LUFS") && ed->readoutForTest (true, 3).endsWith ("dBTP"), "real readings once audio played");
            run (p, mix (48000.0, 0.5, -3.0f), 512);
            ed->tickForTest();
            expect (ed->grHistoryForTest (0) >= 0.0f);

            ed->pressForTest ("Original");
            expect (p.isCompareOriginal());
            run (p, mix (48000.0, 0.5, -3.0f), 512);
            ed->tickForTest();
            expect (ed->statusForTest().startsWith ("ORIGINAL, MATCHED"), ed->statusForTest());
            ed->pressForTest ("Processed");
            expect (! p.isCompareOriginal());
            ed->pressForTest ("Bypass");
            expect (p.isBypassed());
            run (p, mix (48000.0, 0.2, -3.0f), 512);
            ed->tickForTest();
            expect (ed->statusForTest().startsWith ("BYPASSED"), ed->statusForTest());
            ed->pressForTest ("Bypass");

            ed->pressForTest ("Set A");
            setParam (p, ids::loudness, 9.0f);
            ed->pressForTest ("Set B");
            ed->pressForTest ("A");
            expectWithinAbsoluteError (getParam (p, ids::loudness), 3.0f, 0.02f, "A recalls the stored settings");

            run (p, silence (2, 48000), 512);
            ed->tickForTest();
            expectEquals (ed->statusForTest(), juce::String ("SILENCE ON THE INPUT"));

            ed->pressForTest ("How to export");
            expect (ed->exportHelpShownForTest(), "the DAW export workflow is explained");
            std::function<bool (juce::Component&)> hasExport = [&] (juce::Component& c)
            {
                for (auto* ch : c.getChildren())
                    if (auto* b = dynamic_cast<juce::Button*> (ch); (b != nullptr && b->getButtonText().containsIgnoreCase ("export master")) || hasExport (*ch))
                        return true;
                return false;
            };
            expect (! hasExport (*ed), "no Export button that could not export reliably");
            ed->pressForTest ("Original");
            e.reset();
            expect (! p.isCompareOriginal(), "closing the window never leaves the master on ORIGINAL");

            const auto dir = juce::SystemStats::getEnvironmentVariable ("GLITCHKID_SNAPSHOT_DIR", {});
            if (dir.isNotEmpty())
            {
                std::unique_ptr<juce::AudioProcessorEditor> e2 (p.createEditor());
                p.requestMeterReset();
                run (p, bandLimited (mix (48000.0, 4.0, -1.0f, 0.3f, true), 48000.0), 512);
                auto* ed2 = dynamic_cast<MasterEditor*> (e2.get());
                for (int k = 0; k < 40; ++k) { run (p, bandLimited (mix (48000.0, 0.033, -1.0f, 0.3f, true, k), 48000.0), 512); ed2->tickForTest(); }
                auto img = e2->createComponentSnapshot (e2->getLocalBounds(), true, 1.0f);
                juce::File f = juce::File (dir).getChildFile ("glitchkid_master.png");
                f.deleteFile();
                juce::FileOutputStream os (f);
                if (os.openedOk()) juce::PNGImageFormat().writeImageToStream (img, os);
            }
        }
    }
};

static MasterPluginTests masterPluginTests;
}
