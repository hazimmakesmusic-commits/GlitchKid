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

        using MS = glitchkid::ui::MainScreen;
        auto findLabel = [&] (const juce::String& prefix)
        {
            std::function<juce::Label* (juce::Component&)> walk = [&] (juce::Component& c) -> juce::Label*
            {
                for (auto* ch : c.getChildren())
                {
                    if (auto* l = dynamic_cast<juce::Label*> (ch); l != nullptr && l->isVisible() && l->getText().startsWith (prefix))
                        return l;
                    if (auto* r = walk (*ch))
                        return r;
                }
                return nullptr;
            };
            return walk (*ed);
        };

        beginTest ("HOME: the first page, the whole window; the rail, SAVE and A/B wait in Advanced Mixing; the shared header buttons stay");
        {
            expectEquals (ed->uiPageForTest(), 6);
            expectEquals (ed->uiRailIndexForTest(), 0);
            expect (main.isVisible() && main.getBounds() == juce::Rectangle<int> (0, 0, 1120, 680), "the whole window");
            expect (kid.getParentComponent() == nullptr, "Kid is not attached while HOME shows");
            const char* names[] = { "CORRECTION", "INTENSITY", "HIGH AIR", "CEILING", "TONE", "PUNCH", "SPACE", "MID AIR", "MIX", "OUTPUT" };
            for (int i = 0; i < MS::numDials; ++i)
                expectEquals (main.dialNameForTest (i), juce::String (names[i]));
            for (auto* b : { "ORIGINAL", "PROCESSED", "UNDO", "SAVE", "SET A", "SET B", "A", "B", "BUILD MY MIX", "Fit to beat", "ADVANCED MIXING", "Mandala" })
                expect (visibleButton (*ed, b), b);
            for (auto* b : { "Ask Kid", "Choose a sound" })
                expect (! visibleButton (*ed, b), juce::String (b) + " is not on HOME (Kid is deferred; the sound selector replaces the shortcut)");
            for (auto* b : { "HOME", "SOUNDS", "CLEAN", "TUNE", "FINISH", "MASTER", "DELETE" })
                expect (! visibleButton (*ed, b), juce::String (b) + " stays in Advanced Mixing");
            for (int m = 0; m < MS::numModules; ++m)
                expect (main.moduleSwitchForTest (m).isVisible() && main.moduleSwitchForTest (m).getButtonText() == "OFF", "every module starts off");
            shot ("home_idle");
        }

        beginTest ("the module switches and every dial move their real parameters, and follow them when they move elsewhere");
        {
            const char* switchIds[] = { "tune_on", "clean_on", "air_on", "lim_on" };
            for (int m = 0; m < MS::numModules; ++m)
            {
                main.moduleSwitchForTest (m).triggerClick();
                juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
                expectEquals (getParam (p, switchIds[m]), 1.0f, switchIds[m]);
                expectEquals (main.moduleSwitchForTest (m).getButtonText(), juce::String ("ON"));
                th::setParam (p, switchIds[m], 0.0f); // switched off elsewhere (a detailed page, automation)
                expect (! main.moduleSwitchForTest (m).getToggleState() && main.moduleSwitchForTest (m).getButtonText() == "OFF", "the switch follows");
            }
            main.dialForTest (MS::tune).setValue (60.0, juce::sendNotificationSync);
            expectWithinAbsoluteError (getParam (p, "tune_strength"), 60.0f, 0.2f, "CORRECTION");
            expectEquals (getParam (p, "tune_on"), 0.0f, "automation-style moves never switch a module on");
            main.dragForTest (MS::tune, 70.0);
            expect (getParam (p, "tune_on") > 0.5f && std::abs (getParam (p, "tune_strength") - 70.0f) < 0.2f, "a hand drag on an off module switches it on");
            main.dragForTest (MS::clean, 45.0);
            expect (getParam (p, "clean_on") > 0.5f && std::abs (getParam (p, "clean_amount") - 45.0f) < 0.2f, "CLEAN LAB intensity");
            main.dragForTest (MS::air, 35.0);
            expect (getParam (p, "air_on") > 0.5f && std::abs (getParam (p, "air_air") - 35.0f) < 0.2f, "AIR");
            main.dragForTest (MS::ceiling, -3.0);
            expect (getParam (p, "lim_on") > 0.5f && std::abs (getParam (p, "lim_ceiling") + 3.0f) < 0.06f, "CEILING");
            th::setParam (p, "air_on", 0.0f);
            main.dragForTest (MS::presence, 55.0);
            expect (getParam (p, "air_on") > 0.5f && std::abs (getParam (p, "air_presence") - 55.0f) < 0.2f, "MID AIR is AIR LAB's presence, and switches it on");
            expectWithinAbsoluteError (getParam (p, "air_air"), 35.0f, 0.2f, "MID AIR never moves HIGH AIR");
            th::setParam (p, "air_link", 1.0f); // even an old session with the link saved on
            main.dragForTest (MS::presence, 62.0);
            expectWithinAbsoluteError (getParam (p, "air_air"), 35.0f, 0.2f, "the retired link does nothing");
            main.dragForTest (MS::air, 40.0);
            expectWithinAbsoluteError (getParam (p, "air_presence"), 62.0f, 0.2f, "HIGH AIR never moves MID AIR");
            th::setParam (p, "air_link", 0.0f);
            main.dialForTest (MS::tone).setValue (85.0, juce::sendNotificationSync);
            expect (getParam (p, "eq_hi_gain") > 1.0f && main.dialTextForTest (MS::tone).startsWith ("Brighter"), "TONE drives the TONE macro");
            main.dialForTest (MS::punch).setValue (60.0, juce::sendNotificationSync);
            expect (getParam (p, "comp_ratio") > 1.5f, "PUNCH drives the compressor");
            main.dialForTest (MS::space).setValue (40.0, juce::sendNotificationSync);
            expect (getParam (p, "echo_on") > 0.5f, "SPACE drives echo and width");
            main.dialForTest (MS::mix).setValue (70.0, juce::sendNotificationSync);
            expectWithinAbsoluteError (getParam (p, "mix"), 70.0f, 0.2f, "MIX");
            main.dialForTest (MS::output).setValue (-6.0, juce::sendNotificationSync);
            expectWithinAbsoluteError (getParam (p, "gain"), -6.0f, 0.1f, "OUTPUT");

            th::setParam (p, "mix", 100.0f);
            th::setParam (p, "gain", 0.0f);
            th::setParam (p, "clean_amount", 80.0f);
            th::setParam (p, "lim_ceiling", -1.0f);
            th::setParam (p, "air_presence", 30.0f);
            th::setParam (p, "eq_hi_gain", 0.0f);
            th::setParam (p, "eq_b2_gain", 0.0f);
            for (int i = 0; i < 6; ++i) main.tickForTest();
            expectWithinAbsoluteError ((float) main.dialForTest (MS::mix).getValue(), 100.0f, 0.2f, "MIX follows");
            expectWithinAbsoluteError ((float) main.dialForTest (MS::output).getValue(), 0.0f, 0.1f, "OUTPUT follows");
            expectWithinAbsoluteError ((float) main.dialForTest (MS::clean).getValue(), 80.0f, 0.2f, "INTENSITY follows");
            expectWithinAbsoluteError ((float) main.dialForTest (MS::ceiling).getValue(), -1.0f, 0.06f, "CEILING follows");
            expectWithinAbsoluteError ((float) main.dialForTest (MS::presence).getValue(), 30.0f, 0.2f, "PRESENCE follows");
            expectWithinAbsoluteError ((float) main.dialForTest (MS::tone).getValue(), 50.0f, 0.6f, "TONE follows (flat)");
            main.pressForTest ("Use");
            expect (! main.useKeyShownForTest(), "no key suggestion without a vocal file analysed by Kid");
            expect (main.moduleStatusForTest (MS::autotune).startsWith ("Listening"), main.moduleStatusForTest (MS::autotune));
            for (auto* id : { "tune_on", "clean_on", "air_on", "lim_on" })
                th::setParam (p, id, 0.0f);
        }

        beginTest ("ADVANCED MIXING: the detailed pages with the rail; they share every setting with HOME; HOME in the rail comes back");
        {
            const auto before = p.captureSoundState();
            main.pressForTest ("ADVANCED MIXING");
            expectEquals (ed->uiRailIndexForTest(), 3, "first time: TUNE, the first stage");
            expect (! main.isShowing() && visibleButton (*ed, "HOME") && visibleButton (*ed, "SAVE") && visibleButton (*ed, "SET A"), "the rail, SAVE and A/B are back");
            ed->uiSelectRailForTest (5); // FINISH: the limiter's own controls
            th::setParam (p, "lim_ceiling", -4.0f); // as its ceiling knob would
            ed->uiSelectRailForTest (0);
            expect (ed->uiPageForTest() == 6 && main.isVisible() && main.getX() == 0, "HOME");
            expectWithinAbsoluteError ((float) main.dialForTest (MS::ceiling).getValue(), -4.0f, 0.06f, "a change in Advanced shows on HOME");
            main.pressForTest ("ADVANCED MIXING");
            expectEquals (ed->uiRailIndexForTest(), 5, "ADVANCED MIXING reopens the last detailed page");
            th::setParam (p, "lim_ceiling", -1.0f);
            juce::MemoryBlock mb;
            p.getStateInformation (mb);
            GlitchKidProcessor q;
            q.setStateInformation (mb.getData(), (int) mb.getSize());
            expectEquals (q.getUiInt ("uiAdvancedRow", -1), 5, "remembered with the session");
            expectEquals (q.getUiInt ("uiLevel", -1), 4, "the session reopens on the detailed page it was left on");
            ed->uiSelectRailForTest (0);
            main.pressForTest ("Fit to beat");
            expectEquals (ed->uiRailIndexForTest(), 8, "Fit to beat opens BEAT");
            ed->uiSelectRailForTest (0);
            ed->uiShowKidForTest (true); // Kid is no longer on HOME; a session saved with it open still opens it, with its own way back
            expect (ed->uiKidViewForTest() && kid.getParentComponent() != nullptr && main.getParentComponent() == nullptr, "Kid takes the window");
            expectEquals (p.getUiInt ("uiMainView", -1), 1, "remembered with the session");
            kid.pressForTest ("Instrument");
            expect (! ed->uiKidViewForTest() && main.getParentComponent() != nullptr && main.getX() == 0, "back to HOME");
            for (int row = 0; row < 11; ++row)
                ed->uiSelectRailForTest (row);
            ed->uiSelectRailForTest (0);
            expect (main.isVisible() && main.getParentComponent() != nullptr, "HOME in the rail comes back to the instrument");
            expect (same (before, p.captureSoundState()), "navigation never changes the sound");
        }

        beginTest ("one engine, two workspaces: HOME and FINISH move the same MIX both ways; the stages in order; an old DYNAMICS session opens FINISH");
        {
            auto rotaries = [&]
            {
                std::vector<juce::Slider*> out;
                std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
                {
                    for (auto* ch : c.getChildren())
                    {
                        if (! ch->isVisible()) continue;
                        if (auto* sl = dynamic_cast<juce::Slider*> (ch); sl != nullptr && sl->getSliderStyle() == juce::Slider::RotaryHorizontalVerticalDrag) out.push_back (sl);
                        walk (*ch);
                    }
                };
                walk (*ed);
                return out;
            };
            main.dialForTest (MS::mix).setValue (70.0, juce::sendNotificationSync);
            ed->uiSelectRailForTest (5); // FINISH
            auto finish = rotaries();
            expect (finish.size() == 5 && std::abs (finish[1]->getValue() - 70.0) < 0.2, "FINISH's MIX shows HOME's change");
            if (finish.size() == 5)
                finish[1]->setValue (55.0, juce::sendNotificationSync);
            ed->uiSelectRailForTest (0);
            for (int i = 0; i < 6; ++i) main.tickForTest();
            expectWithinAbsoluteError ((float) main.dialForTest (MS::mix).getValue(), 55.0f, 0.2f, "HOME's MIX shows FINISH's change");
            th::setParam (p, "mix", 100.0f);

            ed->uiSelectRailForTest (3);
            const int order[] = { 0, 3, 2, 4, 6, 5, 1, 8, 7, 9, 10 }; // HOME, TUNE, CLEAN, SHAPE, SPACE, FINISH, then MORE: SOUNDS, BEAT, CREATIVE, REFERENCE, MASTER
            for (int i = 0; i + 1 < 11; ++i)
                expect (ed->uiBarButtonBoundsForTest (order[i]).getY() < ed->uiBarButtonBoundsForTest (order[i + 1]).getY(), "rail order at " + juce::String (i));
            for (auto* b : { "HOME", "TUNE", "CLEAN", "SHAPE", "SPACE", "FINISH", "SOUNDS", "BEAT", "CREATIVE", "REFERENCE", "MASTER" })
                expect (visibleButton (*ed, b), b);
            ed->uiSelectRailForTest (0);

            GlitchKidProcessor old;
            old.setUiInt ("uiLevel", 4);
            old.setUiInt ("uiCategory", 2); // saved on DYNAMICS before this redesign
            std::unique_ptr<juce::AudioProcessorEditor> e3 (old.createEditor());
            auto* ed3 = dynamic_cast<GlitchKidEditor*> (e3.get());
            expectEquals (ed3->uiRailIndexForTest(), 5, "opens FINISH, where the limiter and output still are");
            expect (visibleButton (*ed3, "LIMITER"), "the limiter switch is there");
        }

        beginTest ("HOME's journey: the sound choice is shared with SOUNDS and changes nothing; BUILD MY MIX really builds; A/B and SAVE are the shared buttons");
        {
            const auto before = p.captureSoundState();
            auto& genre = main.genreForTest();
            expectEquals (genre.getNumItems(), glitchkid::FactoryPresets::count + 1, "no profile + the eight starting sounds");
            genre.setSelectedId (4, juce::sendNotificationSync); // the third profile
            expectEquals (p.getUiInt ("uiProfile", -9), 2, "the same choice as SOUNDS, kept in the session");
            expect (same (before, p.captureSoundState()), "choosing a sound changes nothing");
            ed->uiSelectRailForTest (1);
            ed->uiSelectRailForTest (0);
            expectEquals (genre.getSelectedId(), 4, "HOME and SOUNDS agree");
            expect (main.nextForTest().isNotEmpty(), "a next step is always named");

            main.pressForTest ("BUILD MY MIX");
            juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
            expect (p.getAnalyzer().isListening() || main.buildStatusForTest().startsWith ("Waiting for audio"), "BUILD MY MIX starts the real analysis: " + main.buildStatusForTest());
            for (int k = 0; k < 10; ++k) { play (100, 0.5f); juce::MessageManager::getInstance()->runDispatchLoopUntil (40); }
            juce::MessageManager::getInstance()->runDispatchLoopUntil (400);
            expect (p.getPresetName().startsWith ("Built: "), "a mix was built from HOME: " + p.getPresetName());
            expect (p.getPresetName().containsIgnoreCase (glitchkid::FactoryPresets::info (2).name), "from the chosen sound");
            expectEquals (main.buildTextForTest(), juce::String ("BUILD AGAIN"));
            shot ("home_built");
            ed->uiUndoForTest();
            expect (same (before, p.captureSoundState()), "UNDO puts the earlier sound back");

            std::function<juce::Button* (juce::Component&, const juce::String&)> find = [&] (juce::Component& c, const juce::String& text) -> juce::Button*
            {
                for (auto* ch : c.getChildren())
                {
                    if (! ch->isVisible()) continue;
                    if (auto* b = dynamic_cast<juce::Button*> (ch); b != nullptr && b->getButtonText() == text) return b;
                    if (auto* r = find (*ch, text)) return r;
                }
                return nullptr;
            };
            find (*ed, "SET A")->triggerClick(); // asynchronous in JUCE: let it run
            juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
            th::setParam (p, "gain", -9.0f);
            find (*ed, "A")->triggerClick();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
            expectWithinAbsoluteError (getParam (p, "gain"), 0.0f, 0.06f, "A on HOME recalls the snapshot");
            th::setParam (p, "gain", 0.0f);
            genre.setSelectedId (1, juce::sendNotificationSync);
            expectEquals (p.getUiInt ("uiProfile", -9), -1, "no profile: a clean start");
        }

        beginTest ("HOME's states come from the real signal and settings: no audio, silence, playing, ORIGINAL, modules, mastering");
        {
            for (int i = 0; i < 40; ++i) main.tickForTest();
            expect (main.headlineForTest().startsWith ("No audio is reaching"), main.headlineForTest());
            expectEquals (main.beatForTest(), juce::String ("No beat connected"));
            play (500, 0.0f); // long enough for any echo tail from earlier audio to die away: until then the output is (really) not silent
            for (int i = 0; i < 3; ++i) { play (2, 0.0f); main.tickForTest(); }
            expect (main.headlineForTest().startsWith ("Silence on the input"), main.headlineForTest());
            th::setParam (p, "clean_on", 1.0f);
            th::setParam (p, "lim_on", 1.0f);
            for (int i = 0; i < 6; ++i) { play (4, 0.4f); main.tickForTest(); }
            expect (main.headlineForTest() == "Hearing your vocal, processed:  CLEAN LAB, LIMITER", main.headlineForTest() + " | clean_on " + juce::String (getParam (p, "clean_on")) + " lim_on " + juce::String (getParam (p, "lim_on")) + " bypass " + juce::String (getParam (p, "bypass")));
            expect (main.moduleStatusForTest (MS::cleanLab).startsWith ("Learning your voice") || main.moduleStatusForTest (MS::cleanLab).startsWith ("Fixing")
                        || main.moduleStatusForTest (MS::cleanLab).startsWith ("Listening"), main.moduleStatusForTest (MS::cleanLab));
            expect (main.moduleStatusForTest (MS::airLab) == "Off" && main.moduleStatusForTest (MS::autotune) == "Off");

            th::setParam (p, "clean_on", 0.0f); // CLEAN LAB rightly cuts this 220 Hz tone as low-mid build-up; off, the full level reaches the limiter
            th::setParam (p, "lim_ceiling", -12.0f); // a loud signal against a low ceiling: the limiter works, and HOME shows its real reduction
            bool caught = false; // the test tone swells and fades: HOME says so while the limiter really reduces (its own reading, once per tick)
            for (int i = 0; i < 30; ++i)
            {
                play (4, 0.9f);
                ed->uiTickForTest();
                main.tickForTest();
                caught = caught || main.moduleStatusForTest (MS::limiter).startsWith ("Catching peaks: -");
            }
            expect (caught, "the limiter's real gain reduction shows on HOME");
            shot ("home_playing_limiter");
            th::setParam (p, "lim_ceiling", -1.0f);

            const auto c = main.visualForTest().getCentre();
            main.holdForTest (c);
            expectEquals (getParam (p, "bypass"), 1.0f, "press and hold the vocal: ORIGINAL");
            for (int i = 0; i < 3; ++i) { play (2, 0.4f); main.tickForTest(); }
            expect (main.headlineForTest().startsWith ("ORIGINAL"), main.headlineForTest());
            shot ("home_original");
            main.releaseForTest();
            expectEquals (getParam (p, "bypass"), 0.0f, "let go: PROCESSED");
            main.holdForTest ({ 4, 4 });
            main.releaseForTest();
            expectEquals (getParam (p, "bypass"), 0.0f, "a press outside the vocal changes nothing");
            th::setParam (p, "bypass", 1.0f);
            main.holdForTest (c);
            main.releaseForTest();
            expectEquals (getParam (p, "bypass"), 1.0f, "ORIGINAL chosen with the button stays ORIGINAL");
            th::setParam (p, "bypass", 0.0f);
            th::setParam (p, "master_on", 1.0f);
            for (int i = 0; i < 3; ++i) main.tickForTest();
            expect (main.headlineForTest().startsWith ("Mastering mode is on"), main.headlineForTest());
            th::setParam (p, "master_on", 0.0f);
            th::setParam (p, "clean_on", 0.0f);
            th::setParam (p, "lim_on", 0.0f);
        }

        beginTest ("the key suggestion is used only when asked; a window closed mid-hold never leaves the track on ORIGINAL");
        {
            MS::Hooks h;
            h.holdOriginal = [&] (bool original) { th::setParam (p, "bypass", original ? 1.0f : 0.0f); };
            h.fileKey = [] (int& tonic, bool& minor) { tonic = 9; minor = true; return true; }; // as Kid reports a reliable A minor
            auto home = std::make_unique<MS> (p, std::move (h));
            home->setSize (1120, 680);
            th::setParam (p, "tune_key", 0.0f);
            th::setParam (p, "tune_scale", 0.0f);
            home->tickForTest(); home->tickForTest(); home->tickForTest();
            expect (home->useKeyShownForTest(), "a suggestion that differs from the current key offers Use");
            expect (getParam (p, "tune_key") < 0.5f && getParam (p, "tune_scale") < 0.5f, "showing it changes nothing");
            home->pressForTest ("Use");
            expectWithinAbsoluteError (getParam (p, "tune_key"), 9.0f, 0.01f, "A");
            expectWithinAbsoluteError (getParam (p, "tune_scale"), 2.0f, 0.01f, "Minor");
            expect (! home->useKeyShownForTest(), "nothing left to suggest");
            expectEquals (getParam (p, "tune_on"), 0.0f, "using the key never switches TUNE on");
            home->holdForTest (home->visualForTest().getCentre());
            expectEquals (getParam (p, "bypass"), 1.0f);
            home.reset();
            expectEquals (getParam (p, "bypass"), 0.0f, "closed while held: back to PROCESSED");
            th::setParam (p, "tune_key", 0.0f);
            th::setParam (p, "tune_scale", 0.0f);
        }

        beginTest ("a preset shows in HOME's bar; a restored session is announced; HOME survives every window size");
        {
            auto s = p.captureSoundState();
            p.applyPreset ("Home Test", s);
            ed->uiTickForTest();
            expect (visibleButton (*ed, "PRESET:  Home Test   v") || visibleButton (*ed, "USER:  Home Test   v") || visibleButton (*ed, "PROFILE:  Home Test   v"), "the preset name on HOME");
            juce::MemoryBlock mb;
            p.getStateInformation (mb);
            GlitchKidProcessor q;
            q.prepareToPlay (48000.0, 480);
            q.setStateInformation (mb.getData(), (int) mb.getSize());
            std::unique_ptr<juce::AudioProcessorEditor> e2 (q.createEditor());
            auto* ed2 = dynamic_cast<GlitchKidEditor*> (e2.get());
            std::function<juce::Label* (juce::Component&)> walk = [&] (juce::Component& c) -> juce::Label*
            {
                for (auto* ch : c.getChildren())
                {
                    if (auto* l = dynamic_cast<juce::Label*> (ch); l != nullptr && l->isVisible() && l->getText().startsWith ("Session restored"))
                        return l;
                    if (auto* r = walk (*ch))
                        return r;
                }
                return nullptr;
            };
            auto* note = walk (*ed2);
            expect (note != nullptr && note->getText() == "Session restored: Home Test", note != nullptr ? note->getText() : juce::String ("no note"));
            expect (ed2->uiPageForTest() == 6 && ed2->uiMainScreenForTest().isVisible(), "the restored session opens on HOME");
            expect (findLabel ("Session restored") == nullptr, "a fresh instance announces nothing");
            for (int w : { 784, 1120, 1680 })
            {
                ed->setSize (w, w * 680 / 1120);
                expect (main.getBounds() == juce::Rectangle<int> (0, 0, 1120, 680), "HOME keeps its design size inside the scaled window");
                shot ("home_" + juce::String (w));
            }
            ed->setSize (1120, 680);
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

        beginTest ("MANDALA: the live view as a kaleidoscope of the same real levels; remembered; never changes the sound; repaints cheaply");
        {
            const auto before = p.captureSoundState();
            expect (! main.mandalaForTest());
            main.pressForTest ("Mandala");
            expect (main.mandalaForTest() && p.getUiFlag ("uiMandala", false), "on, and remembered with the session");
            for (int k = 0; k < 240; ++k) { play (2, 0.04f + 0.5f * std::abs (std::sin ((float) k * 0.13f)) * (k % 40 < 28 ? 1.0f : 0.1f)); main.tickForTest(); } // phrases with gaps
            expect (main.signalForTest());
            shot ("main_mandala");
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
            logMessage ("  Mandala: " + juce::String (ms, 2) + " ms per full repaint, audio included (software renderer)");
            expect (ms < 40.0, juce::String (ms, 2) + " ms");
            for (int k = 0; k < 260; ++k) main.tickForTest(); // no audio: the history drains, the seed stays still
            expect (! main.signalForTest(), "no audio, nothing invented");
            shot ("main_mandala_idle");
            expect (same (before, p.captureSoundState()), "the view never changes the sound");
            main.pressForTest ("Mandala");
            expect (! main.mandalaForTest() && ! p.getUiFlag ("uiMandala", true), "off again");
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

        beginTest ("MASTER in the rail; in the frame; every control is a mastering parameter");
        {
            ed->uiSelectRailForTest (10);
            expectEquals (ed->uiPageForTest(), 8);
            expect (lab.getParentComponent() != nullptr && lab.getX() > 100 && lab.isVisible());
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
            th::setParam (p, "bypass", 1.0f); // the header's ORIGINAL
            juce::AudioBuffer<float> more = mixOf (1.0, -6.0f);
            for (int s = 0; s < more.getNumSamples(); s += 480) { juce::AudioBuffer<float> v (more.getArrayOfWritePointers(), 2, s, 480); p.processBlock (v, midi); }
            expect (p.getMastering().compareGainDb() > 0.5f, "ORIGINAL is raised to the master's loudness");
            th::setParam (p, "bypass", 0.0f);
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
            ed->uiSelectRailForTest (0);
            expect (ed->uiPageForTest() == 6 && ed->uiMainScreenForTest().isVisible(), "START again");
            th::setParam (p, "master_on", 0.0f);
        }
    }
};
static MasteringScreenTests masteringScreenTests;
}
