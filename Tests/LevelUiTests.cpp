#include "TestHelpers.h"
#include "../Source/AutoMode/BeatLock.h"
#include "../Source/PluginEditor.h"

// The level-based interface: BEAT LOCK in the real editor (no beat, then a real second input), without fake progress or fake results.
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
