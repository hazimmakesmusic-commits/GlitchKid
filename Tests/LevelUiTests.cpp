#include "TestHelpers.h"
#include <limits>
#include "../Source/AutoMode/BeatLock.h"
#include "../Source/PluginEditor.h"
#include "../Source/Analysis/DisplaySpectrum.h"

// The Experimental Lab interface: the real-time display data, workspace navigation and BEAT LOCK in the real editor (no beat, then a real second input), without fake progress or fake results.
namespace
{
namespace am = glitchkid::automode;
constexpr double fs = 48000.0;
constexpr int blockSize = 480;

std::vector<float> lead (double seconds)
{
    std::vector<float> x ((size_t) (fs * seconds));
    for (size_t n = 0; n < x.size(); ++n)
    {
        const double t = (double) n / fs, ph = std::fmod (t, 0.5);
        const double env = ph > 0.4 ? 0.0 : std::sin (3.14159265 * ph / 0.4) * 0.3;
        double s = 0.0;
        for (int h = 1; h <= 14; ++h)
            s += std::sin (2.0 * 3.14159265 * 210.0 * h * t) / h;
        x[n] = (float) (env * s * 0.6);
    }
    return x;
}

std::vector<float> pad (double seconds, float amp)
{
    std::vector<float> x ((size_t) (fs * seconds), 0.0f);
    int k = 0;
    for (double f : { 620.0, 910.0, 1330.0, 1810.0 })
    {
        for (size_t n = 0; n < x.size(); ++n)
            x[n] += amp * (float) std::sin (2.0 * 3.14159265 * f * (double) n / fs + 0.7 * k);
        ++k;
    }
    return x;
}

void stream (GlitchKidProcessor& p, const std::vector<float>& in, const std::vector<float>* beat, int auxChannels)
{
    juce::MidiBuffer midi;
    for (size_t pos = 0; pos + blockSize <= in.size(); pos += blockSize)
    {
        th::Buf b (2 + auxChannels, blockSize);
        for (int i = 0; i < blockSize; ++i)
        {
            for (int ch = 0; ch < 2; ++ch)
                b.setSample (ch, i, in[pos + (size_t) i]);
            for (int ch = 0; ch < auxChannels; ++ch)
                b.setSample (2 + ch, i, beat != nullptr ? (*beat)[pos + (size_t) i] : 0.0f);
        }
        p.processBlock (b, midi);
    }
}

juce::TextButton* findButton (juce::Component& root, const juce::String& text)
{
    for (auto* c : root.getChildren())
    {
        if (auto* b = dynamic_cast<juce::TextButton*> (c); b != nullptr && b->getButtonText() == text)
            return b;
        if (auto* r = findButton (*c, text))
            return r;
    }
    return nullptr;
}

void collectToggles (juce::Component& root, std::vector<juce::ToggleButton*>& out)
{
    for (auto* c : root.getChildren())
    {
        if (auto* t = dynamic_cast<juce::ToggleButton*> (c); t != nullptr && t->isVisible())
            out.push_back (t);
        collectToggles (*c, out);
    }
}

bool anyLabelContains (juce::Component& root, const juce::String& text)
{
    for (auto* c : root.getChildren())
    {
        if (auto* l = dynamic_cast<juce::Label*> (c); l != nullptr && l->isVisible() && l->getText().contains (text))
            return true;
        if (anyLabelContains (*c, text))
            return true;
    }
    return false;
}

void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

void snapshot (juce::AudioProcessorEditor& ed, const juce::String& name)
{
    const juce::String dir = juce::SystemStats::getEnvironmentVariable ("GLITCHKID_SNAPSHOT_DIR", {});
    if (dir.isEmpty())
        return;
    if (auto* e = dynamic_cast<GlitchKidEditor*> (&ed))
        e->uiWaitForRelicForTest();
    pump (150);
    auto img = ed.createComponentSnapshot (ed.getLocalBounds(), true, 1.0f);
    juce::File f = juce::File (dir).getChildFile (name + ".png");
    f.deleteFile();
    juce::FileOutputStream os (f);
    if (os.openedOk())
        juce::PNGImageFormat().writeImageToStream (img, os);
}

struct LevelUiTests : public juce::UnitTest
{
    LevelUiTests() : juce::UnitTest ("Level interface", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("display data: a tap keeps the newest samples, a sine shows up at its own frequency, silence shows nothing, non-finite samples are zeroed");
        {
            using namespace glitchkid::analysis;
            for (double rate : { 44100.0, 48000.0, 96000.0 })
            {
                DisplayTap tap;
                std::vector<float> x (20000);
                for (size_t n = 0; n < x.size(); ++n)
                    x[n] = 0.5f * (float) std::sin (2.0 * 3.14159265358979 * 1000.0 * (double) n / rate);
                for (size_t pos = 0; pos < x.size(); pos += 480) // host-style blocks; more than the tap holds, so it wraps
                    tap.push (x.data() + pos, nullptr, (int) std::min<size_t> (480, x.size() - pos));
                DisplaySpectrum sp;
                for (int i = 0; i < 4; ++i)
                    sp.update (tap, rate);
                expect (sp.hasSignal());
                int best = 0;
                for (int c = 1; c < DisplaySpectrum::columns; ++c)
                    if (sp.levels()[(size_t) c] > sp.levels()[(size_t) best])
                        best = c;
                const float hz = DisplaySpectrum::columnHz (best, rate);
                expect (hz > 850.0f && hz < 1180.0f, "peak column at " + juce::String (hz) + " Hz for a 1 kHz sine at " + juce::String (rate));
                expect (sp.levels()[(size_t) best] > 0.9f, "a -6 dBFS sine reads at the top of the scale");
                expect (sp.levels()[0] < 0.3f && sp.levels()[(size_t) DisplaySpectrum::columns - 1] < 0.3f, "far from the sine the display is low");
                std::array<float, 8> tail;
                tap.copyLatest (tail.data(), 8);
                for (int i = 0; i < 8; ++i)
                    expectWithinAbsoluteError (tail[(size_t) i], x[x.size() - 8 + (size_t) i], 1.0e-6f); // the newest samples, in order
            }
            DisplayTap quiet;
            std::vector<float> zeros (4000, 0.0f);
            quiet.push (zeros.data(), zeros.data(), 4000);
            DisplaySpectrum sp2;
            sp2.update (quiet, 48000.0);
            expect (! sp2.hasSignal());
            for (float v : sp2.levels())
                expectEquals (v, 0.0f);
            WaveColumns wc;
            wc.update (quiet, 2400);
            expect (! wc.hasSignal);
            DisplayTap bad;
            std::vector<float> nasty (2000, 0.1f);
            nasty[10] = std::numeric_limits<float>::quiet_NaN();
            nasty[11] = std::numeric_limits<float>::infinity();
            bad.push (nasty.data(), nullptr, 2000);
            std::array<float, 2000> back;
            bad.copyLatest (back.data(), 2000);
            for (float v : back)
                expect (std::isfinite (v));
            DisplaySpectrum sp3;
            sp3.update (bad, 48000.0);
            for (float v : sp3.levels())
                expect (std::isfinite (v) && v >= 0.0f && v <= 1.0f);
        }

        beginTest ("display data from the real plug-in: input tap = what arrives, output tap = what is returned; an EQ boost shows in the output and not the input; mono works");
        {
            for (int channels : { 2, 1 })
            {
                GlitchKidProcessor p;
                if (channels == 1)
                    th::setLayout (p, juce::AudioChannelSet::mono());
                th::setParam (p, "eq_b2_freq", 1000.0f);
                th::setParam (p, "eq_b2_gain", 12.0f);
                p.prepareToPlay (fs, blockSize);
                juce::MidiBuffer midi;
                for (int b = 0; b < 100; ++b)
                {
                    th::Buf buf (channels, blockSize);
                    for (int i = 0; i < blockSize; ++i)
                        for (int ch = 0; ch < channels; ++ch)
                            buf.setSample (ch, i, 0.03f * (float) std::sin (2.0 * 3.14159265358979 * 1000.0 * (double) (b * blockSize + i) / fs));
                    p.processBlock (buf, midi);
                }
                glitchkid::analysis::DisplaySpectrum in, out;
                for (int i = 0; i < 4; ++i)
                {
                    in.update (p.getInputTap(), fs);
                    out.update (p.getOutputTap(), fs);
                }
                float peakIn = 0.0f, peakOut = 0.0f;
                for (int c = 0; c < glitchkid::analysis::DisplaySpectrum::columns; ++c)
                {
                    peakIn = juce::jmax (peakIn, in.levels()[(size_t) c]);
                    peakOut = juce::jmax (peakOut, out.levels()[(size_t) c]);
                }
                expect (in.hasSignal() && out.hasSignal());
                expect (peakOut > peakIn + 0.08f, "the +12 dB boost shows in the output (" + juce::String (peakIn, 3) + " -> " + juce::String (peakOut, 3) + ") and not in the input");
                glitchkid::analysis::DisplaySpectrum beatSpec;
                beatSpec.update (p.getBeatTap(), fs);
                expect (! beatSpec.hasSignal(), "no instrumental input: the beat tap is empty, so no beat trace can be drawn");
            }
        }

        beginTest ("workspace navigation never changes the sound, the window can be resized, and the workspace is remembered");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (fs, blockSize);
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            auto* content = ed->getChildComponent (0);
            th::setParam (p, "gain", -3.0f);
            th::setParam (p, "eq_b2_gain", 2.5f);
            th::setParam (p, "clean_on", 1.0f);
            const auto before = p.captureSoundState();
            const auto presetName = p.getPresetName();
            for (const char* name : { "CLEAN LAB", "BEAT LOCK", "BUILD MY MIX", "FINAL TOUCH", "HOME", "FINAL TOUCH", "HOME" })
            {
                auto* b = findButton (*content, name);
                expect (b != nullptr && b->isVisible(), juce::String (name) + " is on the navigation bar");
                if (b != nullptr)
                    b->triggerClick();
                pump (60);
                ed->setSize (ed->getWidth() * 5 / 6, ed->getHeight() * 5 / 6);
                ed->setSize (ed->getWidth() * 6 / 5, ed->getHeight() * 6 / 5);
            }
            for (const auto& e : before.values)
                expectWithinAbsoluteError (p.apvts.getRawParameterValue (e.id)->load(), e.value, 1.0e-4f);
            expectEquals (p.getPresetName(), presetName);
            findButton (*content, "BEAT LOCK")->triggerClick();
            pump (60);
            juce::MemoryBlock mb;
            p.getStateInformation (mb);
            GlitchKidProcessor q;
            q.setStateInformation (mb.getData(), (int) mb.getSize());
            expectEquals (q.getUiInt ("uiLevel", -1), 2);
            GlitchKidProcessor fresh;
            expectEquals (fresh.getUiInt ("uiLevel", 0), 0, "a session without the preference opens on HOME");
        }

        beginTest ("HOME with real audio flowing: renders (snapshot when GLITCHKID_SNAPSHOT_DIR is set)");
        {
            GlitchKidProcessor p;
            th::setParam (p, "clean_on", 1.0f);
            p.prepareToPlay (fs, blockSize);
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            pump (100);
            snapshot (*ed, "home_nosignal");
            const auto voice = lead (12.0);
            size_t pos = 0;
            auto play = [&] (double seconds) // audio keeps arriving while the interface refreshes, as in a running host
            {
                const size_t n = (size_t) (seconds * fs) / blockSize * blockSize;
                const std::vector<float> part (voice.begin() + (long) pos, voice.begin() + (long) (pos + n));
                stream (p, part, nullptr, 0);
                pos += n;
            };
            play (5.0);
            for (int i = 0; i < 12; ++i) { play (0.06); pump (16); }
            snapshot (*ed, "home_signal");
            auto* content = ed->getChildComponent (0);
            findButton (*content, "CLEAN LAB")->triggerClick();
            pump (60);
            for (int i = 0; i < 12; ++i) { play (0.06); pump (16); }
            snapshot (*ed, "clean_signal");
        }

        beginTest ("output limiter in FINAL TOUCH > DYNAMICS: the switch and knobs are the real parameters; the peak read-out is after the limiter and says CLIPPING only when the output really reached full scale");
        {
            for (bool limOn : { false, true })
            {
                GlitchKidProcessor p;
                th::setParam (p, "gain", 12.0f);
                p.setUiInt ("uiLevel", 4);
                p.setUiInt ("uiCategory", 2);
                p.prepareToPlay (fs, blockSize);
                std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
                auto* content = ed->getChildComponent (0);
                std::vector<juce::ToggleButton*> toggles;
                collectToggles (*content, toggles);
                juce::ToggleButton* lim = nullptr;
                for (auto* t : toggles)
                    if (t->getButtonText() == "LIMITER")
                        lim = t;
                expect (lim != nullptr, "the limiter switch is on the DYNAMICS page");
                if (lim == nullptr)
                    continue;
                if (limOn)
                {
                    lim->triggerClick();
                    pump (60);
                    expectEquals (th::getParam (p, "lim_on"), 1.0f);
                }
                std::vector<juce::Slider*> knobs;
                std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
                {
                    for (auto* ch : c.getChildren())
                    {
                        if (auto* sl = dynamic_cast<juce::Slider*> (ch); sl != nullptr && sl->isVisible() && sl->getSliderStyle() == juce::Slider::RotaryHorizontalVerticalDrag)
                            knobs.push_back (sl);
                        walk (*ch);
                    }
                };
                walk (*content);
                expectEquals ((int) knobs.size(), 9, "five compressor knobs, output gain, and ceiling, drive and release");
                if (knobs.size() == 9)
                {
                    knobs[6]->setValue (-6.0, juce::sendNotificationSync); // CEILING
                    knobs[7]->setValue (3.0, juce::sendNotificationSync);  // DRIVE
                    knobs[8]->setValue (150.0, juce::sendNotificationSync); // RELEASE
                    expectWithinAbsoluteError (th::getParam (p, "lim_ceiling"), -6.0f, 0.05f);
                    expectWithinAbsoluteError (th::getParam (p, "lim_drive"), 3.0f, 0.05f);
                    expectEquals (th::getParam (p, "lim_release"), 150.0f);
                    th::setParam (p, "lim_ceiling", -1.0f);
                    th::setParam (p, "lim_drive", 0.0f);
                }
                p.prepareToPlay (fs, blockSize);
                std::vector<float> hot ((size_t) (fs * 2.0));
                for (size_t n = 0; n < hot.size(); ++n)
                    hot[n] = 0.9f * (float) std::sin (2.0 * 3.14159265358979 * 220.0 * (double) n / fs);
                stream (p, hot, nullptr, 0);
                pump (250);
                if (limOn)
                {
                    expect (anyLabelContains (*content, "after the limiter"), "the read-out names the post-limiter peak");
                    expect (! anyLabelContains (*content, "CLIPPING"), "the limited output does not clip");
                }
                else
                {
                    expect (anyLabelContains (*content, "limiter off"));
                    expect (anyLabelContains (*content, "CLIPPING"), "a +12 dB output really clips, and the display says so");
                }
                if (limOn)
                    snapshot (*ed, "limiter_on");
            }
        }

        beginTest ("BEAT LOCK without a beat: says so, starts no analysis, offers the vocal-only workflow, applies nothing");
        {
            GlitchKidProcessor p;
            p.setUiInt ("uiLevel", 2);
            p.prepareToPlay (fs, blockSize);
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            auto* content = ed->getChildComponent (0);
            pump (200);
            auto vis = [&] (const juce::String& t) { auto* b = findButton (*content, t); return b != nullptr && b->isVisible(); };
            expect (vis ("ANALYZE BEAT") && vis ("VOCAL-ONLY: BUILD MY MIX  >"));
            expect (! vis ("APPLY SELECTED") && ! vis ("UNDO BEAT LOCK"), "nothing to apply without an analysis");
            expect (anyLabelContains (*content, "BEAT REQUIRED"), "the missing beat is stated");
            const auto count = p.getAnalyzer().completedCount();
            const auto before = p.captureSoundState();
            findButton (*content, "ANALYZE BEAT")->triggerClick();
            pump (150);
            expect (! p.getAnalyzer().isListening() && p.getAnalyzer().completedCount() == count, "no analysis was started: there is nothing to listen to");
            expect (findButton (*content, "STOP NOW") == nullptr, "no fake progress: the button never switches to the listening state");
            std::vector<juce::ToggleButton*> ticks;
            collectToggles (*content, ticks);
            expect (ticks.empty(), "no suggestions are listed");
            for (const auto& e : before.values)
                expectWithinAbsoluteError (p.apvts.getRawParameterValue (e.id)->load(), e.value, 1.0e-4f);
            findButton (*content, "VOCAL-ONLY: BUILD MY MIX  >")->triggerClick();
            pump (60);
            expectEquals (p.getUiInt ("uiLevel", -1), 3);
        }

        beginTest ("BEAT LOCK with a real second input: analyses both, lists reasons, apply changes only the ticked, undo restores");
        {
            GlitchKidProcessor p;
            juce::AudioProcessor::BusesLayout l;
            l.inputBuses.add (juce::AudioChannelSet::stereo());
            l.inputBuses.add (juce::AudioChannelSet::stereo());
            l.outputBuses.add (juce::AudioChannelSet::stereo());
            expect (p.setBusesLayout (l));
            p.setUiInt ("uiLevel", 2);
            p.prepareToPlay (fs, blockSize);
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            auto* content = ed->getChildComponent (0);
            const auto voice = lead (14.0), beat = pad (14.0, 0.22f);
            const std::vector<float> one (voice.begin(), voice.begin() + (long) fs);
            const std::vector<float> oneBeat (beat.begin(), beat.begin() + (long) fs);
            stream (p, one, &oneBeat, 2);
            pump (200);
            expect (p.getInstrumentalStatus() == GlitchKidProcessor::SourceStatus::live);
            expect (! findButton (*content, "APPLY SELECTED")->isEnabled(), "nothing analysed yet: nothing to apply");

            findButton (*content, "ANALYZE BEAT")->triggerClick();
            pump (100);
            const std::vector<float> first (voice.begin(), voice.begin() + (long) (fs * 0.5));
            const std::vector<float> firstBeat (beat.begin(), beat.begin() + (long) (fs * 0.5));
            stream (p, first, &firstBeat, 2); // the request is picked up by the audio thread at the next block
            expect (p.getAnalyzer().isListening() && p.getInstrumentalAnalyzer().isListening(), "the analysis really started, on both inputs");
            pump (100);
            expect (findButton (*content, "STOP NOW") != nullptr, "while listening the button offers to stop");
            stream (p, voice, &beat, 2);
            pump (400);
            expect (! p.getAnalyzer().isListening());
            const auto direct = am::analyseBeatLock (p.getAnalyzer().fetch(), p.getInstrumentalAnalyzer().fetch(), am::InstrumentalInput::live);
            expect (direct.valid, direct.message);
            int adjustable = 0;
            for (const auto& it : direct.items)
                adjustable += it.advisory ? 0 : 1;
            expect (adjustable >= 1, "a pad on the voice's mids produces at least one adjustment");
            std::vector<juce::ToggleButton*> ticks;
            collectToggles (*content, ticks);
            expectEquals ((int) ticks.size(), adjustable, "one tick per adjustable suggestion");
            expect (anyLabelContains (*content, "->"), "the real current -> proposed value is shown");
            expect (findButton (*content, "APPLY SELECTED")->isEnabled());
            snapshot (*ed, "beatlock_analysed");

            const auto before = p.captureSoundState();
            for (auto* t : ticks)
                t->triggerClick(); // reject everything
            findButton (*content, "APPLY SELECTED")->triggerClick();
            pump (60);
            for (const auto& e : before.values)
                expectWithinAbsoluteError (p.apvts.getRawParameterValue (e.id)->load(), e.value, 1.0e-4f);
            expect (! findButton (*content, "UNDO")->isEnabled(), "nothing was applied, so there is nothing to undo");

            for (auto* t : ticks)
                t->triggerClick(); // accept everything again
            findButton (*content, "APPLY SELECTED")->triggerClick();
            pump (60);
            int moved = 0;
            for (const auto& e : before.values)
                moved += std::abs (p.apvts.getRawParameterValue (e.id)->load() - e.value) > 1.0e-3f ? 1 : 0;
            expect (moved >= 1, "the adjustments really moved parameters: " + juce::String (moved));
            expect (findButton (*content, "UNDO")->isEnabled() && findButton (*content, "UNDO BEAT LOCK")->isVisible());
            snapshot (*ed, "beatlock_applied");
            expect (! findButton (*content, "APPLY SELECTED")->isEnabled(), "one apply per analysis: it cannot be stacked by accident");
            findButton (*content, "UNDO")->triggerClick();
            pump (60);
            for (const auto& e : before.values)
                expectWithinAbsoluteError (p.apvts.getRawParameterValue (e.id)->load(), e.value, 1.0e-3f);
        }

        beginTest ("cost and latency: a live Instrumental input and CLEAN LAB add no latency and little CPU (measured, logged)");
        {
            auto timed = [&] (bool full)
            {
                GlitchKidProcessor p;
                juce::AudioProcessor::BusesLayout l;
                l.inputBuses.add (juce::AudioChannelSet::stereo());
                l.inputBuses.add (full ? juce::AudioChannelSet::stereo() : juce::AudioChannelSet::disabled());
                l.outputBuses.add (juce::AudioChannelSet::stereo());
                p.setBusesLayout (l);
                if (full)
                {
                    th::setParam (p, "clean_on", 1.0f);
                    th::setParam (p, "clean_amount", 100.0f);
                    th::setParam (p, "air_on", 1.0f);
                    p.startSourceAnalysis();
                }
                p.prepareToPlay (fs, blockSize);
                const auto voice = lead (10.0), beat = pad (10.0, 0.22f);
                const auto t0 = juce::Time::getMillisecondCounterHiRes();
                stream (p, voice, full ? &beat : nullptr, full ? 2 : 0);
                const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
                return std::pair<double, int> { ms / 10000.0, p.getLatencySamples() }; // fraction of real time
            };
            const auto base = timed (false), full = timed (true);
            logMessage ("CPU (one core, fraction of real time): default " + juce::String (base.first * 100.0, 2) + " %, CLEAN LAB + AIR LAB + live beat input + analysis "
                        + juce::String (full.first * 100.0, 2) + " %; reported latency " + juce::String (base.second) + " / " + juce::String (full.second) + " samples");
            expectEquals (base.second, full.second, "the Instrumental input and CLEAN LAB add no latency");
            expect (full.first < 0.5, "well inside real time");
        }
    }
};
LevelUiTests levelUiTests;
}
