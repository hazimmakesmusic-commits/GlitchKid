#include "TestHelpers.h"
#include "../Source/Mastering/MasterRender.h"
#include "../Source/Kid/KidDirector.h"

// Mastering inside the plug-in: off by default (the vocal chain and old sessions are untouched), a separate chain when on, never part of a vocal preset,
// never touched by Kid; and the offline path: analysing a mix file and rendering a time-aligned master to WAV.
namespace
{
using namespace glitchkid::master;

juce::AudioBuffer<float> testMix (double fs, double seconds, float peakDb, float stereo, juce::int64 seed)
{
    juce::Random rng (seed);
    juce::AudioBuffer<float> b (2, (int) (fs * seconds));
    double a = 0, c = 0;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double common = rng.nextFloat() * 2.0 - 1.0;
        a += 0.02 * (common - a);
        c += 0.02 * ((rng.nextFloat() * 2.0 - 1.0) - c);
        const double kick = (i % (int) (fs * 0.5)) < (int) (fs * 0.03) ? std::sin (juce::MathConstants<double>::twoPi * 55.0 * i / fs) : 0.0;
        b.setSample (0, i, (float) (3.0 * a + 0.2 * common + stereo * 2.0 * c + kick));
        b.setSample (1, i, (float) (3.0 * a + 0.2 * common - stereo * 2.0 * c + kick));
    }
    const float pk = juce::jmax (b.getMagnitude (0, 0, b.getNumSamples()), b.getMagnitude (1, 0, b.getNumSamples()));
    b.applyGain (juce::Decibels::decibelsToGain (peakDb) / pk);
    return b;
}

juce::File writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double fs)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
    auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (fs).withNumChannels (b.getNumChannels()).withBitsPerSample (24));
    if (w != nullptr) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    return f;
}

juce::AudioBuffer<float> readWav (const juce::File& f, double& fs)
{
    auto r = glitchkid::analysis::openAudioFile (f);
    juce::AudioBuffer<float> b;
    if (r == nullptr) return b;
    fs = r->sampleRate;
    b.setSize ((int) r->numChannels, (int) r->lengthInSamples);
    r->read (&b, 0, b.getNumSamples(), 0, true, true);
    return b;
}

struct MasteringPluginTests : public juce::UnitTest
{
    MasteringPluginTests() : juce::UnitTest ("Mastering (plug-in)", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("off by default: the vocal chain, the latency, presets, A/B and Kid are exactly as before");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 480);
            expect (! p.isMasteringMode());
            expectEquals (p.getLatencySamples(), th::pluginLatency, "the vocal chain's latency");
            for (auto& v : p.captureSoundState().values)
                expect (! v.id.startsWith ("master_"), "not part of a vocal sound: " + v.id);
            for (auto* id : { "master_on", "master_style", "master_loudness" })
                expect (! glitchkid::kid::Director::isAllowed (id), juce::String ("Kid never touches ") + id);
            juce::AudioBuffer<float> in = th::makeNoise (2, 4800), out (in);
            juce::MidiBuffer midi;
            for (int s = 0; s < 4800; s += 480) { juce::AudioBuffer<float> v (out.getArrayOfWritePointers(), 2, s, 480); p.processBlock (v, midi); }
            float err = 0.0f;
            for (int i = th::pluginLatency; i < 4800; ++i) err = juce::jmax (err, std::abs (out.getSample (0, i) - in.getSample (0, i - th::pluginLatency)));
            expect (juce::exactlyEqual (err, 0.0f), "default settings still pass the audio through bit-exactly");
        }

        beginTest ("Mastering mode: the mix goes through the mastering chain instead of the vocal chain; latency reported; settings saved with the session");
        {
            const double fs = 48000.0;
            GlitchKidProcessor p;
            p.prepareToPlay (fs, 480);
            th::setParam (p, "sat_drive", 18.0f); // vocal settings that would change the sound a lot
            th::setParam (p, "mut_on", 1.0f);
            th::setParam (p, "master_on", 1.0f);
            th::setParam (p, "master_style", 2.0f);
            th::setParam (p, "master_loudness", 6.0f);
            p.updateReportedLatency();
            MasteringEngine ref;
            ref.prepare (fs, 512);
            ref.setControls (p.readMasterControls());
            expectEquals (p.getLatencySamples(), ref.latencySamples(), "the mastering latency is reported to the host");
            const auto mix = testMix (fs, 4.0, -6.0f, 0.3f, 5);
            juce::AudioBuffer<float> a (mix), b (mix);
            juce::MidiBuffer midi;
            for (int s = 0; s < a.getNumSamples(); s += 480)
            {
                juce::AudioBuffer<float> va (a.getArrayOfWritePointers(), 2, s, 480), vb (b.getArrayOfWritePointers(), 2, s, 480);
                p.processBlock (va, midi);
                ref.process (vb);
            }
            float err = 0.0f;
            int at = 0;
            for (int i = (int) fs / 2; i < a.getNumSamples(); ++i) { const float d = std::abs (a.getSample (1, i) - b.getSample (1, i)); if (d > err) { err = d; at = i; } }
            logMessage ("  worst at " + juce::String (at));
            expect (err < 1.0e-5f, "exactly the mastering chain, no vocal processing: " + juce::String (err));
            expect (p.getMastering().inputMeter().integratedLufs() > -70.0f && p.getMastering().outputMeter().truePeakDb() <= -0.75f, "metered; under the ceiling");

            juce::MemoryBlock state;
            p.getStateInformation (state);
            GlitchKidProcessor q;
            q.setStateInformation (state.getData(), (int) state.getSize());
            expect (q.isMasteringMode() && std::abs (th::getParam (q, "master_loudness") - 6.0f) < 0.05f && std::abs (th::getParam (q, "master_style") - 2.0f) < 0.5f, "restored");

            th::setParam (p, "master_on", 0.0f);
            p.updateReportedLatency();
            expectEquals (p.getLatencySamples(), p.getWantedLatencySamples(), "back to the vocal chain's latency");
        }

        beginTest ("ORIGINAL in Mastering mode: Bypass plays the untouched mix at the master's loudness");
        {
            const double fs = 48000.0;
            GlitchKidProcessor p;
            p.prepareToPlay (fs, 480);
            th::setParam (p, "master_on", 1.0f);
            th::setParam (p, "master_loudness", 8.0f);
            const auto mix = testMix (fs, 8.0, -12.0f, 0.3f, 6);
            juce::MidiBuffer midi;
            auto run = [&] (bool original)
            {
                th::setParam (p, "bypass", original ? 1.0f : 0.0f);
                juce::AudioBuffer<float> x (mix);
                for (int s = 0; s < x.getNumSamples(); s += 480) { juce::AudioBuffer<float> v (x.getArrayOfWritePointers(), 2, s, 480); p.processBlock (v, midi); }
                LoudnessMeter m;
                m.prepare (fs);
                m.process (x.getReadPointer (0) + (int) fs * 4, x.getReadPointer (1) + (int) fs * 4, x.getNumSamples() - (int) fs * 4);
                return m.integratedLufs();
            };
            const float master = run (false), original = run (true);
            expectWithinAbsoluteError (original, master, 1.0f, "level-matched: " + juce::String (original) + " vs " + juce::String (master));
        }

        juce::TemporaryFile dirFile;
        const auto dir = dirFile.getFile();
        dir.createDirectory();
        beginTest ("a mix file: measured to BS.1770, and judged from the measurements");
        {
            juce::AudioBuffer<float> s (2, 48000 * 12);
            const float a = juce::Decibels::decibelsToGain (-23.0f);
            for (int i = 0; i < s.getNumSamples(); ++i)
                for (int ch = 0; ch < 2; ++ch) s.setSample (ch, i, a * (float) std::sin (juce::MathConstants<double>::twoPi * 1000.0 * i / 48000.0));
            std::atomic<bool> no { false };
            const auto r = analyseMix (writeWav (dir.getChildFile ("tone.wav"), s, 48000.0), no);
            expect (r.ok, r.message);
            expectWithinAbsoluteError (r.integratedLufs, -23.0f, 0.15f, "24-bit file: " + juce::String (r.integratedLufs));
            expect (suitability (-9.0f, -0.1f, 0, 0.6f, true).startsWith ("Already loud"));
            expect (suitability (-14.0f, -2.0f, 12, 0.6f, true).startsWith ("The mix clips"));
            expect (suitability (-16.0f, -3.0f, 0, -0.3f, true).contains ("out of phase"));
            expect (suitability (-16.0f, -3.0f, 0, 0.5f, true).startsWith ("Ready to master"));
            expect (! analyseMix (dir.getChildFile ("missing.wav"), no).ok);
        }

        beginTest ("export: a 24-bit WAV master, time-aligned and as long as the mix, under -1 dBTP; the mix is never touched");
        for (double fs : { 44100.0, 48000.0 })
        {
            std::atomic<bool> no { false };
            const auto mix = testMix (fs, 10.0, -14.0f, 0.4f, 9);
            const auto src = writeWav (dir.getChildFile ("mix.wav"), mix, fs);
            auto bytes = [] (const juce::File& f) { juce::MemoryBlock m; f.loadFileAsData (m); return m; };
            const auto srcHash = bytes (src);
            Controls c;
            c.style = Style::loudPunchy;
            c.loudnessDb = 6.0f;
            const auto out = dir.getChildFile ("master.wav");
            const auto r = renderMaster (src, out, c, no);
            expect (r.ok, r.message);
            double outFs = 0.0;
            const auto rendered = readWav (out, outFs);
            expectEquals (rendered.getNumSamples(), mix.getNumSamples(), "same length");
            expectEquals (outFs, fs);
            expect (r.after.truePeakDb <= -1.0f + 0.25f && r.after.clippedSamples == 0, "true peak " + juce::String (r.after.truePeakDb));
            expect (r.after.integratedLufs > r.before.integratedLufs + 2.0f, "louder: " + juce::String (r.before.integratedLufs) + " -> " + juce::String (r.after.integratedLufs));
            expect (bytes (src) == srcHash, "the mix file is unchanged");
            // aligned: with nothing but the limiter at no drive and a quiet mix, the master is the mix (to 24-bit precision)
            Controls plain;
            plain.intensity = 0.0f;
            plain.loudnessDb = 0.0f;
            const auto quiet = testMix (fs, 3.0, -20.0f, 0.4f, 10);
            const auto q = writeWav (dir.getChildFile ("quiet.wav"), quiet, fs);
            const auto r2 = renderMaster (q, dir.getChildFile ("quiet-master.wav"), plain, no);
            const auto qm = readWav (dir.getChildFile ("quiet-master.wav"), outFs);
            double sourceFs = 0.0;
            const auto qs = readWav (q, sourceFs);
            float err = 0.0f;
            for (int i = 0; i < qm.getNumSamples(); ++i) err = juce::jmax (err, std::abs (qm.getSample (0, i) - qs.getSample (0, i)));
            expect (r2.ok && err < 1.0e-5f, "sample-aligned with the mix: " + juce::String (err));
            expect (! renderMaster (src, src, c, no).ok && bytes (src) == srcHash, "the mix itself is never the destination");
            std::atomic<bool> stop { true };
            expect (! renderMaster (src, dir.getChildFile ("cancelled.wav"), c, stop).ok && ! dir.getChildFile ("cancelled.wav").existsAsFile(), "cancelled: nothing written");
        }
        dir.deleteRecursively();
    }
};
static MasteringPluginTests masteringPluginTests;
}
