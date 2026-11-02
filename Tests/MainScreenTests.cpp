#include "TestHelpers.h"
#include "../Source/PluginEditor.h"

// The instrument (the first screen): every dial is bound to existing parameters, the views never change the sound, the ribbon shows real audio only.
namespace
{
struct MainScreenTests : public juce::UnitTest
{
    MainScreenTests() : juce::UnitTest ("Main screen", "GlitchKid") {}

    void runTest() override
    {
        GlitchKidProcessor p;
        p.prepareToPlay (48000.0, 480);
        std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
        auto* ed = dynamic_cast<GlitchKidEditor*> (e.get());
        auto& main = ed->uiMainScreenForTest();
        auto& kid = ed->uiKidPanelForTest();
        std::function<bool (juce::Component&, const juce::String&)> visibleButton = [&] (juce::Component& c, const juce::String& text)
        {
            for (auto* ch : c.getChildren())
                if (ch->isVisible() && ((dynamic_cast<juce::Button*> (ch) != nullptr && dynamic_cast<juce::Button*> (ch)->getButtonText() == text) || visibleButton (*ch, text)))
                    return true;
            return false;
        };
        auto same = [] (const glitchkid::SoundState& a, const glitchkid::SoundState& b)
        {
            for (auto& v : a.values)
                if (std::abs (b.get (v.id, -999) - v.value) > 1e-4f) return false;
            return true;
        };
        auto shot = [&] (const juce::String& shotName)
        {
            const auto dir = juce::SystemStats::getEnvironmentVariable ("GLITCHKID_SNAPSHOT_DIR", {});
            if (dir.isEmpty()) return;
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::File f = juce::File (dir).getChildFile (shotName + ".png");
            f.deleteFile();
            juce::FileOutputStream os (f);
            if (os.openedOk()) juce::PNGImageFormat().writeImageToStream (img, os);
        };
        auto play = [&] (int blocks, float amp)
        {
            juce::MidiBuffer midi;
            for (int b = 0; b < blocks; ++b)
            {
                juce::AudioBuffer<float> buf (2, 480);
                for (int i = 0; i < 480; ++i)
                {
                    const double t = (double) (b * 480 + i) / 48000.0;
                    const float v = amp * (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * t) * (0.5f + 0.5f * (float) std::sin (t * 5.0));
                    buf.setSample (0, i, v);
                    buf.setSample (1, i, v);
                }
                p.processBlock (buf, midi);
            }
        };

        beginTest ("the first screen: the instrument fills the window; three zones; seven dials; Kid and the detailed pages one click away");
        {
            expectEquals (ed->uiRailIndexForTest(), 0);
            expect (main.isVisible() && main.getParentComponent() != nullptr && main.getBounds() == main.getParentComponent()->getLocalBounds(), "the instrument fills the window");
            expect (kid.getParentComponent() == nullptr, "Kid is not attached while the instrument shows");
            const char* names[] = { "TUNE", "CLEAN", "TONE", "PUNCH", "SPACE", "MIX", "OUTPUT" };
            for (int i = 0; i < glitchkid::ui::MainScreen::numDials; ++i)
                expectEquals (main.dialNameForTest (i), juce::String (names[i]));
            expect (visibleButton (*ed, "Kid") && visibleButton (*ed, "Advanced") && visibleButton (*ed, "Original") && visibleButton (*ed, "Processed"));
            expect (! visibleButton (*ed, "TUNE") && ! visibleButton (*ed, "SAVE"), "no rail and no preset bar here: the header has its own preset selector");
            shot ("main_idle");
        }

        beginTest ("every dial moves its real parameters, and follows them when they move elsewhere");
        {
            main.dialForTest (0).setValue (60.0, juce::sendNotificationSync);
            expect (getParam (p, "tune_on") > 0.5f && std::abs (getParam (p, "tune_strength") - 60.0f) < 0.2f, "TUNE");
            main.dialForTest (0).setValue (0.0, juce::sendNotificationSync);
            expect (getParam (p, "tune_on") < 0.5f && main.dialTextForTest (0) == "Off", "TUNE at 0 is off");
            main.dialForTest (1).setValue (45.0, juce::sendNotificationSync);
            expect (getParam (p, "clean_on") > 0.5f && std::abs (getParam (p, "clean_amount") - 45.0f) < 0.2f, "CLEAN");
            main.dialForTest (2).setValue (85.0, juce::sendNotificationSync);
            expect (getParam (p, "eq_hi_gain") > 1.0f && main.dialTextForTest (2).startsWith ("Brighter"), "TONE drives the TONE macro");
            main.dialForTest (3).setValue (60.0, juce::sendNotificationSync);
            expect (getParam (p, "comp_ratio") > 1.5f, "PUNCH drives the compressor");
            main.dialForTest (4).setValue (40.0, juce::sendNotificationSync);
            expect (getParam (p, "echo_on") > 0.5f, "SPACE drives echo and width");
            main.dialForTest (5).setValue (70.0, juce::sendNotificationSync);
            expectWithinAbsoluteError (getParam (p, "mix"), 70.0f, 0.2f, "MIX");
            main.dialForTest (6).setValue (-6.0, juce::sendNotificationSync);
            expectWithinAbsoluteError (getParam (p, "gain"), -6.0f, 0.1f, "OUTPUT");

            setParam (p, "mix", 100.0f);
            setParam (p, "gain", 0.0f);
            setParam (p, "clean_amount", 80.0f);
            setParam (p, "eq_hi_gain", 0.0f);
            setParam (p, "eq_b2_gain", 0.0f);
            for (int i = 0; i < 6; ++i) main.tickForTest();
            expectWithinAbsoluteError ((float) main.dialForTest (5).getValue(), 100.0f, 0.2f, "MIX follows");
            expectWithinAbsoluteError ((float) main.dialForTest (6).getValue(), 0.0f, 0.1f, "OUTPUT follows");
            expectWithinAbsoluteError ((float) main.dialForTest (1).getValue(), 80.0f, 0.2f, "CLEAN follows");
            expectWithinAbsoluteError ((float) main.dialForTest (2).getValue(), 50.0f, 0.6f, "TONE follows (flat)");
        }

        beginTest ("presets from the header, the real bypass, and the views never change the sound");
        {
            const auto presetBefore = main.presetTextForTest();
            main.pressForTest (">");
            for (int i = 0; i < 6; ++i) main.tickForTest();
            expect (main.presetTextForTest() != presetBefore && main.presetTextForTest() == p.getPresetName(), "next preset: " + main.presetTextForTest());
            main.pressForTest ("Original");
            expect (getParam (p, "bypass") > 0.5f && ! p.isLevelMatchedCompare(), "ORIGINAL is the exact bypass here");
            main.pressForTest ("Processed");
            expect (getParam (p, "bypass") < 0.5f);

            const auto before = p.captureSoundState();
            main.pressForTest ("Kid");
            expect (ed->uiKidViewForTest() && kid.getParentComponent() != nullptr && main.getParentComponent() == nullptr, "Kid takes the window");
            expectEquals (p.getUiInt ("uiMainView", -1), 1, "remembered with the session");
            kid.pressForTest ("Instrument");
            expect (! ed->uiKidViewForTest() && main.getParentComponent() != nullptr, "back to the instrument");
            main.pressForTest ("Advanced");
            expectEquals (ed->uiRailIndexForTest(), 4, "the detailed pages, starting with TUNE");
            expect (visibleButton (*ed, "MAIN") && visibleButton (*ed, "SAVE"));
            ed->uiSelectRailForTest (0);
            expect (main.isVisible() && main.getParentComponent() != nullptr, "MAIN comes back to the instrument");
            expect (same (before, p.captureSoundState()), "navigation never changes the sound");
        }

        beginTest ("the ribbon shows real audio only, and repaints cheaply");
        {
            for (int i = 0; i < 4; ++i) main.tickForTest();
            expect (! main.signalForTest(), "no audio, no ribbon");
            play (40, 0.4f);
            main.tickForTest();
            expect (main.signalForTest(), "audio through the plug-in shows up");
            for (int k = 0; k < 60; ++k) { play (2, 0.4f); main.tickForTest(); }
            shot ("main_playing");
            juce::Image img (juce::Image::ARGB, main.getWidth(), main.getHeight(), true);
            juce::Graphics g (img);
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            for (int i = 0; i < 30; ++i)
            {
                play (2, 0.4f);
                main.tickForTest();
                main.paintEntireComponent (g, true);
            }
            const double ms = (juce::Time::getMillisecondCounterHiRes() - t0) / 30.0;
            logMessage ("  Main screen: " + juce::String (ms, 2) + " ms per full repaint, audio included (software renderer)");
            expect (ms < 40.0, juce::String (ms, 2) + " ms");
        }
    }
};
static MainScreenTests mainScreenTests;

// The Mastering Lab inside the editor: reached from the instrument, every control a parameter, the mode switch, the live verdict, a file loaded and exported.
struct MasteringScreenTests : public juce::UnitTest
{
    MasteringScreenTests() : juce::UnitTest ("Mastering Lab (screen)", "GlitchKid") {}

    void runTest() override
    {
        const double fs = 48000.0;
        GlitchKidProcessor p;
        p.prepareToPlay (fs, 480);
        std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
        auto* ed = dynamic_cast<GlitchKidEditor*> (e.get());
        auto& lab = ed->uiMasteringScreenForTest();
        auto mixOf = [fs] (double seconds, float peak)
        {
            juce::Random rng (3);
            juce::AudioBuffer<float> b (2, (int) (fs * seconds));
            double a = 0.0;
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                a += 0.03 * ((rng.nextFloat() * 2.0 - 1.0) - a);
                const float side = 0.15f * (rng.nextFloat() * 2.0f - 1.0f);
                b.setSample (0, i, (float) (4.0 * a) + side);
                b.setSample (1, i, (float) (4.0 * a) - side);
            }
            b.applyGain (juce::Decibels::decibelsToGain (peak) / juce::jmax (b.getMagnitude (0, 0, b.getNumSamples()), b.getMagnitude (1, 0, b.getNumSamples())));
            return b;
        };
        auto shot = [&] (const juce::String& shotName)
        {
            const auto dir = juce::SystemStats::getEnvironmentVariable ("GLITCHKID_SNAPSHOT_DIR", {});
            if (dir.isEmpty()) return;
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::File f = juce::File (dir).getChildFile (shotName + ".png");
            f.deleteFile();
            juce::FileOutputStream os (f);
            if (os.openedOk()) juce::PNGImageFormat().writeImageToStream (img, os);
        };

        beginTest ("reached from the instrument; the whole window; every control is a mastering parameter; Vocal goes back");
        {
            ed->uiMainScreenForTest().pressForTest ("Mastering");
            expectEquals (ed->uiPageForTest(), 8);
            expect (lab.getParentComponent() != nullptr && lab.getBounds() == lab.getParentComponent()->getLocalBounds() && lab.isVisible());
            expectEquals (p.getUiInt ("uiLevel", -1), 8, "remembered with the session");
            lab.pressForTest ("Warm & Smooth");
            expectWithinAbsoluteError (getParam (p, "master_style"), 1.0f, 0.01f, "a style tile sets the style");
            lab.dialForTest (0).setValue (80.0, juce::sendNotificationSync);
            lab.dialForTest (1).setValue (5.0, juce::sendNotificationSync);
            lab.dialForTest (2).setValue (-40.0, juce::sendNotificationSync);
            lab.dialForTest (3).setValue (130.0, juce::sendNotificationSync);
            lab.dialForTest (4).setValue (-2.0, juce::sendNotificationSync);
            expectWithinAbsoluteError (getParam (p, "master_intensity"), 80.0f, 0.6f);
            expectWithinAbsoluteError (getParam (p, "master_loudness"), 5.0f, 0.06f);
            expectWithinAbsoluteError (getParam (p, "master_tone"), -40.0f, 0.6f);
            expectWithinAbsoluteError (getParam (p, "master_width"), 130.0f, 0.6f);
            expectWithinAbsoluteError (getParam (p, "master_output"), -2.0f, 0.06f);
            for (auto& v : p.captureSoundState().values)
                expect (! v.id.startsWith ("master_"));
            expect (! p.isMasteringMode() && lab.verdictForTest().startsWith ("Mastering mode is off"), "off until switched on, and says so");
            expect (! lab.exportEnabledForTest(), "nothing to export before a mix is loaded");
        }

        beginTest ("Mastering mode on: the live mix is measured and judged; ORIGINAL / MASTER switch the real bypass");
        {
            lab.pressForTest ("Mastering mode: off");
            expect (p.isMasteringMode());
            p.updateReportedLatency();
            expectEquals (p.getLatencySamples(), p.getMastering().latencySamples());
            auto mix = mixOf (6.0, -6.0f);
            juce::MidiBuffer midi;
            for (int s = 0; s < mix.getNumSamples(); s += 480) { juce::AudioBuffer<float> v (mix.getArrayOfWritePointers(), 2, s, 480); p.processBlock (v, midi); }
            lab.tickForTest();
            expect (lab.verdictForTest().startsWith ("Ready to master"), lab.verdictForTest());
            lab.pressForTest ("Original");
            expect (getParam (p, "bypass") > 0.5f);
            lab.pressForTest ("Master");
            expect (getParam (p, "bypass") < 0.5f);
            shot ("mastering_live");
        }

        beginTest ("a mix file: measured, then exported as a 24-bit WAV master with these settings");
        {
            juce::TemporaryFile dirFile;
            const auto dir = dirFile.getFile();
            dir.createDirectory();
            const auto src = dir.getChildFile ("song.wav");
            {
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::OutputStream> os (src.createOutputStream());
                auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (fs).withNumChannels (2).withBitsPerSample (24));
                const auto b = mixOf (5.0, -9.0f);
                if (w != nullptr) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
            }
            lab.loadForTest (src);
            for (int i = 0; i < 500 && lab.busyForTest(); ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
            expect (lab.statusForTest().startsWith ("Loaded") && lab.exportEnabledForTest(), lab.statusForTest());
            expect (lab.verdictForTest().startsWith ("Ready to master"), lab.verdictForTest());
            const auto out = dir.getChildFile ("song master.wav");
            lab.exportForTest (out);
            for (int i = 0; i < 1000 && lab.busyForTest(); ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
            expect (lab.statusForTest().startsWith ("Saved") && out.existsAsFile(), lab.statusForTest());
            shot ("mastering_file");
            dir.deleteRecursively();
            lab.pressForTest ("Vocal");
            expect (ed->uiPageForTest() == 6 && ed->uiMainScreenForTest().isVisible(), "back to the instrument");
            th::setParam (p, "master_on", 0.0f);
        }
    }
};
static MasteringScreenTests masteringScreenTests;
}
