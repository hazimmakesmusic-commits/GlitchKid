#include "TestHelpers.h"
#include "../Source/Macros/MacroEngine.h"
#include "../Source/Presets/PresetController.h"
#include "../Source/UI/EffectModel.h"
#include "../Source/UI/PlaygroundWidgets.h"
#include <set>

// Interactive playground UI: the non-visual logic (macros, quick commands, parameter application, UI flags) and a smoke test that builds the real
// editor, lets its timer run and renders it. A rendered image being "not blank" does not mean the interface looks good; look at the snapshots
// (set GLITCHKID_SNAPSHOT_DIR to write them as PNG files).
namespace
{
using glitchkid::SoundState;
using glitchkid::macros::Macro;
using glitchkid::macros::Command;

struct Range { float lo, hi; };

Range rangeOf (juce::AudioProcessor& p, const juce::String& id)
{
    for (auto* q : p.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (q); r && r->paramID == id)
            return { r->getNormalisableRange().start, r->getNormalisableRange().end };
    return { 1.0f, 0.0f };
}

bool inRange (juce::AudioProcessor& p, const SoundState& s)
{
    for (const auto& e : s.values)
    {
        const auto r = rangeOf (p, e.id);
        if (! std::isfinite (e.value) || e.value < r.lo - 1.0e-4f || e.value > r.hi + 1.0e-4f)
            return false;
    }
    return true;
}

std::set<juce::String> changedIds (const SoundState& a, const SoundState& b)
{
    std::set<juce::String> out;
    for (const auto& e : b.values)
        if (std::abs (e.value - a.get (e.id, e.value + 1.0f)) > 1.0e-6f)
            out.insert (e.id);
    return out;
}

std::vector<float> vocalish (double fs, double seconds)
{
    std::vector<float> x ((size_t) (fs * seconds));
    for (size_t n = 0; n < x.size(); ++n)
    {
        const double t = (double) n / fs, ph = std::fmod (t, 0.4);
        const double env = ph > 0.3 ? 0.0 : std::sin (3.14159265 * ph / 0.3) * 0.3;
        double s = 0.0;
        for (int h = 1; h <= 14; ++h)
            s += std::sin (2.0 * 3.14159265 * 210.0 * h * t) / h;
        x[n] = (float) (env * s * 0.6);
    }
    return x;
}

struct MacroTests : public juce::UnitTest
{
    MacroTests() : juce::UnitTest ("Playground UI logic", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("macros: every value in range, only the documented parameters change, deterministic, NaN/out-of-range input is safe");
        {
            GlitchKidProcessor p;
            const auto base = p.captureSoundState();
            const std::map<int, std::set<juce::String>> allowed = {
                { 0, { "echo_on", "dim_on", "dbl_on", "echo_mix", "echo_fb", "echo_width", "dim_mix", "dbl_amount" } },
                { 1, { "sat_drive", "eq_b2_gain" } },
                { 2, { "comp_ratio", "comp_thresh", "comp_attack", "comp_release", "comp_knee", "comp_makeup" } } };
            for (int m = 0; m < glitchkid::macros::numMacros; ++m)
                for (float v : { 0.0f, 0.1f, 0.33f, 0.5f, 0.77f, 1.0f, -3.0f, 9.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() })
                {
                    auto a = base, b = base;
                    glitchkid::macros::apply ((Macro) m, v, a);
                    glitchkid::macros::apply ((Macro) m, v, b);
                    expect (inRange (p, a), juce::String (glitchkid::macros::macroName ((Macro) m)) + " " + juce::String (v));
                    for (const auto& id : changedIds (base, a))
                        expect (allowed.at (m).count (id) == 1, "unexpected parameter " + id);
                    for (size_t i = 0; i < a.values.size(); ++i)
                        expect (! (a.values[i].value < b.values[i].value) && ! (a.values[i].value > b.values[i].value));
                }
            for (const auto* never : { "gain", "mix", "tune_on", "tune_key", "formant_shift", "mut_on", "harm_on", "stutter_on", "echo_time", "echo_div" })
            {
                auto s = base;
                for (int m = 0; m < glitchkid::macros::numMacros; ++m)
                    glitchkid::macros::apply ((Macro) m, 1.0f, s);
                expectEquals (s.get (never, -1.0f), base.get (never, -2.0f), never);
            }
        }

        beginTest ("macros: a dial position written to the plug-in is read back (within one step), and more is never less");
        {
            GlitchKidProcessor p;
            for (int m = 0; m < glitchkid::macros::numMacros; ++m)
            {
                float previous = -1.0f;
                for (int k = 0; k <= 20; ++k)
                {
                    const float v = (float) k / 20.0f;
                    auto s = p.captureSoundState();
                    glitchkid::macros::apply ((Macro) m, v, s);
                    p.applyParameterChanges (s);
                    const float back = glitchkid::macros::read ((Macro) m, p.captureSoundState());
                    expectWithinAbsoluteError (back, v, 0.03f);
                    expect (back >= previous - 1.0e-4f, "monotonic");
                    previous = back;
                }
            }
            auto s = p.captureSoundState();
            glitchkid::macros::apply (Macro::space, 0.0f, s);
            p.applyParameterChanges (s);
            expectEquals (th::getParam (p, "echo_on"), 0.0f);
            expectEquals (glitchkid::macros::read (Macro::space, p.captureSoundState()), 0.0f);
        }

        beginTest ("quick commands: bounded, in range, deterministic, repeated presses stop at the limits");
        {
            GlitchKidProcessor p;
            const auto base = p.captureSoundState();
            for (int c = 0; c < glitchkid::macros::numCommands; ++c)
            {
                auto s = base, t = base;
                glitchkid::macros::apply ((Command) c, s);
                glitchkid::macros::apply ((Command) c, t);
                expect (inRange (p, s));
                expect (! changedIds (base, s).empty(), glitchkid::macros::commandLabel ((Command) c));
                for (size_t i = 0; i < s.values.size(); ++i)
                    expect (! (s.values[i].value < t.values[i].value) && ! (s.values[i].value > t.values[i].value));
                for (int n = 0; n < 60; ++n)
                    glitchkid::macros::apply ((Command) c, s);
                expect (inRange (p, s), "after 60 presses");
            }
            auto d = base;
            for (int n = 0; n < 60; ++n) glitchkid::macros::apply (Command::darker, d);
            expectEquals (d.get ("eq_hi_gain", 0.0f), -15.0f);
            expectEquals (d.get ("echo_tone", 0.0f), 400.0f);
            auto h = base;
            for (int n = 0; n < 60; ++n) glitchkid::macros::apply (Command::harderTune, h);
            expectEquals (h.get ("tune_strength", 0.0f), 100.0f);
            expectEquals (h.get ("tune_speed", 9.0f), 0.0f);
            expectEquals (h.get ("tune_on", 0.0f), 1.0f);
            auto w = base;
            for (int n = 0; n < 60; ++n) glitchkid::macros::apply (Command::weirder, w);
            expectEquals (w.get ("mut_amount", 0.0f), 100.0f);
            auto sp = base;
            float last = glitchkid::macros::read (Macro::space, sp);
            for (int n = 0; n < 10; ++n)
            {
                glitchkid::macros::apply (Command::moreSpace, sp);
                const float now = glitchkid::macros::read (Macro::space, sp);
                expect (now >= last);
                last = now;
            }
            expectWithinAbsoluteError (last, 1.0f, 1.0e-4f);
            // Commands never touch the things they do not say they touch.
            auto dk = base;
            glitchkid::macros::apply (Command::darker, dk);
            for (const auto& id : changedIds (base, dk))
                expect (id == "eq_hi_gain" || id == "eq_b2_gain" || id == "echo_tone", id);
        }

        beginTest ("applyParameterChanges: sets only what differs, is idempotent, ignores bad ids and values, never touches Bypass");
        {
            GlitchKidProcessor p;
            auto s = p.captureSoundState();
            expectEquals (p.applyParameterChanges (s), 0);
            s.set ("sat_drive", 7.0f);
            s.set ("echo_mix", 33.0f);
            s.set ("does_not_exist", 5.0f);
            s.set ("comp_ratio", std::numeric_limits<float>::quiet_NaN());
            s.set ("bypass", 1.0f);
            expectEquals (p.applyParameterChanges (s), 2);
            expectEquals (p.applyParameterChanges (s), 0);
            expectWithinAbsoluteError (th::getParam (p, "sat_drive"), 7.0f, 0.06f);
            expectEquals (th::getParam (p, "bypass"), 0.0f);
            expectWithinAbsoluteError (th::getParam (p, "comp_ratio"), 1.0f, 1.0e-3f);
            s.set ("sat_drive", 999.0f); // out of range is clamped by the parameter, never stored
            p.applyParameterChanges (s);
            expectEquals (th::getParam (p, "sat_drive"), 18.0f);
        }

        beginTest ("UI preferences: saved with the session, default when absent, not part of a sound, preset or A/B");
        {
            GlitchKidProcessor a;
            expect (a.getUiFlag ("uiMotion", true) && a.getUiInt ("uiPage", 0) == 0 && a.getUiInt ("uiEffect", 0) == 0);
            const auto before = a.captureSoundState();
            a.setUiFlag ("uiMotion", false);
            a.setUiInt ("uiPage", 2);
            a.setUiInt ("uiEffect", 3);
            expectEquals ((int) a.captureSoundState().values.size(), (int) before.values.size());
            juce::MemoryBlock mb;
            a.getStateInformation (mb);
            GlitchKidProcessor b;
            b.setStateInformation (mb.getData(), (int) mb.getSize());
            expect (! b.getUiFlag ("uiMotion", true) && b.getUiInt ("uiPage", 0) == 2 && b.getUiInt ("uiEffect", 0) == 3);
            auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("glitchkid-ui-" + juce::String::toHexString (juce::Random().nextInt64()));
            glitchkid::PresetController c (a, glitchkid::PresetStore (dir));
            c.loadFactory (0);
            expect (! a.getUiFlag ("uiMotion", true) && a.getUiInt ("uiPage", 0) == 2, "loading a preset leaves the UI preferences alone");
            dir.deleteRecursively();
        }


        beginTest ("tuning styles: fixed strength/speed, never touch key or scale, read back correctly, custom values show no style");
        {
            GlitchKidProcessor p;
            const auto base = p.captureSoundState();
            for (int i = 0; i < glitchkid::macros::numTuneStyles; ++i)
            {
                auto s = base;
                glitchkid::macros::apply ((glitchkid::macros::TuneStyle) i, s);
                expect (inRange (p, s));
                for (const auto& id : changedIds (base, s))
                    expect (id == "tune_on" || id == "tune_strength" || id == "tune_speed", id);
                expectEquals (glitchkid::macros::readTuneStyle (s), i);
                p.applyParameterChanges (s);
                expectEquals (glitchkid::macros::readTuneStyle (p.captureSoundState()), i); // survives the parameter quantisation
            }
            auto h = base, n = base;
            glitchkid::macros::apply (glitchkid::macros::TuneStyle::hard, h);
            glitchkid::macros::apply (glitchkid::macros::TuneStyle::natural, n);
            expect (h.get ("tune_strength", 0.0f) > n.get ("tune_strength", 100.0f) && h.get ("tune_speed", 9.0f) < n.get ("tune_speed", 0.0f), "HARD is firmer and faster than NATURAL");
            auto custom = h;
            custom.set ("tune_strength", 90.0f);
            expectEquals (glitchkid::macros::readTuneStyle (custom), -1);
            custom = h;
            custom.set ("tune_on", 0.0f);
            expectEquals (glitchkid::macros::readTuneStyle (custom), -1);
            expectEquals (glitchkid::macros::readTuneStyle (base), -1);
        }


        beginTest ("effect model: the real processing order, real enable parameters, activity read from real values, non-toggle effects are never faked");
        {
            namespace fx = glitchkid::fx;
            GlitchKidProcessor p;
            const char* order[] = { "PITCH TUNE", "FORMANT", "HARMONY", "MUTATION", "DOUBLER", "EQ", "COMPRESSOR", "SATURATION", "STUTTER", "ECHO", "DIMENSION" };
            expectEquals ((int) fx::numEffects, 11);
            int toggles = 0;
            for (int i = 0; i < fx::numEffects; ++i)
            {
                expectEquals (juce::String (fx::info (i).name), juce::String (order[i]));
                expect (juce::String (fx::info (i).description).isNotEmpty());
                if (fx::canToggle (i))
                {
                    ++toggles;
                    expect (p.apvts.getParameter (fx::info (i).enableId) != nullptr, juce::String ("enable parameter exists: ") + fx::info (i).enableId);
                }
            }
            expectEquals (toggles, 7);
            expect (! fx::canToggle (fx::formant) && ! fx::canToggle (fx::eq) && ! fx::canToggle (fx::compressor) && ! fx::canToggle (fx::saturation));

            auto s = p.captureSoundState();
            for (int i = 0; i < fx::numEffects; ++i)
                expect (! fx::isActive (i, s), "factory state: nothing is active");
            for (int i = 0; i < fx::numEffects; ++i)
            {
                auto t = s;
                fx::setEnabled (i, true, t);
                expect (fx::isActive (i, t) == fx::canToggle (i), "enabling only does something for effects with a real switch");
                fx::setEnabled (i, false, t);
                expect (! fx::isActive (i, t));
            }
            auto t = s;
            t.set ("formant_shift", 2.0f);
            expect (fx::isActive (fx::formant, t));
            t = s; t.set ("eq_b1_gain", -3.0f);
            expect (fx::isActive (fx::eq, t));
            t = s; t.set ("comp_ratio", 3.0f);
            expect (fx::isActive (fx::compressor, t));
            t = s; t.set ("sat_drive", 5.0f);
            expect (fx::isActive (fx::saturation, t));
            expect (! fx::isActive (-1, s) && ! fx::isActive (99, s) && ! fx::canToggle (-1) && ! fx::canToggle (99));
        }

        beginTest ("editor: builds, lays out, animates and renders every page, every effect and several sizes");
        {
            namespace fx = glitchkid::fx;
            const juce::String outDir = juce::SystemStats::getEnvironmentVariable ("GLITCHKID_SNAPSHOT_DIR", {});
            auto snapshot = [&] (juce::AudioProcessorEditor& ed, const juce::String& shot)
            {
                juce::MessageManager::getInstance()->runDispatchLoopUntil (200); // the editor's 30 Hz timer runs
                auto img = ed.createComponentSnapshot (ed.getLocalBounds(), true, 1.0f);
                expect (img.isValid() && img.getWidth() == ed.getWidth());
                // not a blank or single-colour image: count distinct colours on a coarse grid
                std::set<juce::uint32> colours;
                for (int y = 0; y < img.getHeight(); y += 7)
                    for (int x = 0; x < img.getWidth(); x += 7)
                        colours.insert (img.getPixelAt (x, y).getARGB());
                expect (colours.size() > 20, shot + " looks blank: " + juce::String ((int) colours.size()) + " colours");
                if (outDir.isNotEmpty())
                {
                    juce::File f = juce::File (outDir).getChildFile (shot + ".png");
                    f.deleteFile();
                    juce::FileOutputStream os (f);
                    if (os.openedOk())
                        juce::PNGImageFormat().writeImageToStream (img, os);
                }
            };

            auto vocalBlocks = [] (GlitchKidProcessor& p, double seconds)
            {
                const auto x = vocalish (48000.0, seconds);
                juce::MidiBuffer midi;
                for (size_t pos = 0; pos + 480 <= x.size(); pos += 480)
                {
                    th::Buf b (2, 480);
                    for (int i = 0; i < 480; ++i) { b.setSample (0, i, x[pos + (size_t) i]); b.setSample (1, i, x[pos + (size_t) i]); }
                    p.processBlock (b, midi);
                }
            };

            // every page renders; the EFFECTS page renders every effect
            for (int pg = 0; pg < 4; ++pg)
            {
                GlitchKidProcessor p;
                p.setUiInt ("uiPage", pg);
                p.prepareToPlay (48000.0, 480);
                std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
                expect (ed != nullptr);
                snapshot (*ed, "page" + juce::String (pg));
            }
            for (int e = 0; e < fx::numEffects; ++e)
            {
                GlitchKidProcessor p;
                p.setUiInt ("uiPage", 1);
                p.setUiInt ("uiEffect", e);
                p.prepareToPlay (48000.0, 480);
                std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
                snapshot (*ed, "effect" + juce::String (e));
            }

            // resizing keeps the layout (and does not crash) at the limits
            {
                GlitchKidProcessor p;
                p.setUiInt ("uiPage", 1);
                p.setUiInt ("uiEffect", fx::echo);
                std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
                const int w0 = ed->getWidth(), h0 = ed->getHeight();
                for (int w : { w0 * 3 / 5 + 10, w0, w0 * 3 / 2 - 10 })
                {
                    ed->setSize (w, w * h0 / w0);
                    snapshot (*ed, "resized_" + juce::String (w));
                }
            }

            // navigation and selection through the real controls
            {
                GlitchKidProcessor p;
                p.prepareToPlay (48000.0, 480);
                std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
                auto* content = ed->getChildComponent (0);
                expect (content != nullptr);

                std::function<void (juce::Component&, std::function<void (juce::Component&)>)> walk = [&] (juce::Component& root, std::function<void (juce::Component&)> f)
                {
                    for (auto* c : root.getChildren())
                    {
                        f (*c);
                        walk (*c, f);
                    }
                };
                auto findButton = [&] (const juce::String& text) -> juce::Button*
                {
                    juce::Button* found = nullptr;
                    walk (*content, [&] (juce::Component& c)
                    {
                        if (auto* b = dynamic_cast<juce::Button*> (&c); b != nullptr && found == nullptr && b->getButtonText() == text && dynamic_cast<glitchkid::ui::EffectTile*> (b) == nullptr
                                                                          && dynamic_cast<glitchkid::ui::ChainRow*> (b) == nullptr)
                            found = b;
                    });
                    return found;
                };
                auto visible = [&] (const juce::String& text) { auto* b = findButton (text); return b != nullptr && b->isVisible(); };
                auto settle = [] { juce::MessageManager::getInstance()->runDispatchLoopUntil (60); };
                auto click = [&] (const juce::String& text)
                {
                    auto* b = findButton (text);
                    expect (b != nullptr && b->isVisible(), "button '" + text + "' is there to click");
                    if (b != nullptr)
                    {
                        b->triggerClick(); // posts the click: let the message loop deliver it
                        settle();
                    }
                };
                auto tile = [&] (int effect) -> glitchkid::ui::EffectTile*
                {
                    glitchkid::ui::EffectTile* t = nullptr;
                    walk (*content, [&] (juce::Component& c) { if (auto* x = dynamic_cast<glitchkid::ui::EffectTile*> (&c); x != nullptr && x->getEffect() == effect) t = x; });
                    return t;
                };
                auto row = [&] (int effect) -> glitchkid::ui::ChainRow*
                {
                    glitchkid::ui::ChainRow* r = nullptr;
                    walk (*content, [&] (juce::Component& c) { if (auto* x = dynamic_cast<glitchkid::ui::ChainRow*> (&c); x != nullptr && x->getEffect() == effect) r = x; });
                    return r;
                };
                auto countVisibleTiles = [&]
                {
                    int n = 0;
                    walk (*content, [&] (juce::Component& c) { if (dynamic_cast<glitchkid::ui::EffectTile*> (&c) != nullptr && c.isVisible()) ++n; });
                    return n;
                };

                // first launch: HOME
                expect (visible ("HOME") && visible ("EFFECTS") && visible ("PRESETS") && visible ("SETTINGS"), "the four sections are always shown");
                expect (visible ("ANALYZE VOCAL") && visible ("APPLY SOUND") && visible ("DARKER"), "HOME");
                expect (! visible ("SAVE PRESET") && ! visible ("SURPRISE ME") && ! visible ("HARD") && ! visible ("ANIMATIONS: ON"), "HOME shows only its own controls");
                expect (visible ("DRY") && visible ("PROCESSED") && visible ("SET A") && visible ("A"), "monitor and A/B are always shown");
                expectEquals (countVisibleTiles(), 0);
                for (int i = 0; i < fx::numEffects; ++i)
                    expect (row (i) != nullptr && row (i)->isVisible(), "every effect is listed in the FX CHAIN");

                // a quick tweak is a real parameter change
                const float eqBefore = th::getParam (p, "eq_hi_gain");
                click ("DARKER");
                expect (th::getParam (p, "eq_hi_gain") < eqBefore - 1.0f);

                click ("DRY");
                expectEquals (th::getParam (p, "bypass"), 1.0f);
                click ("PROCESSED");
                expectEquals (th::getParam (p, "bypass"), 0.0f);

                // EFFECTS: the grid, and only the selected effect's controls
                click ("EFFECTS");
                expectEquals (p.getUiInt ("uiPage", -1), 1);
                expectEquals (countVisibleTiles(), (int) fx::numEffects);
                expect (! visible ("ANALYZE VOCAL") && ! visible ("DARKER"));
                expect (visible ("HARD") && visible ("NATURAL"), "Pitch Tune is selected first");
                tile (fx::eq)->triggerClick();
                settle();
                expectEquals (p.getUiInt ("uiEffect", -1), (int) fx::eq);
                expect (! visible ("HARD"), "the Pitch Tune controls are gone");
                tile (fx::stutter)->triggerClick();
                settle();
                expectEquals (p.getUiInt ("uiEffect", -1), (int) fx::stutter);

                // the chain switch changes the real parameter and the tile agrees; effects without a switch only select
                expectEquals (th::getParam (p, "echo_on"), 0.0f);
                row (fx::echo)->triggerClick(); // (a programmatic click lands at the switch)
                settle();
                expectEquals (th::getParam (p, "echo_on"), 1.0f);
                row (fx::echo)->triggerClick();
                settle();
                expectEquals (th::getParam (p, "echo_on"), 0.0f);
                row (fx::compressor)->triggerClick();
                settle();
                expectEquals (th::getParam (p, "comp_ratio"), 1.0f);
                expectEquals (p.getUiInt ("uiEffect", -1), (int) fx::compressor);

                // PRESETS
                click ("PRESETS");
                expectEquals (p.getUiInt ("uiPage", -1), 2);
                expect (visible ("SAVE PRESET") && visible ("SURPRISE ME") && visible ("RESET") && visible ("DELETE"));
                expectEquals (countVisibleTiles(), 0);
                juce::ListBox* list = nullptr;
                walk (*content, [&] (juce::Component& c) { if (auto* l = dynamic_cast<juce::ListBox*> (&c)) list = l; });
                expect (list != nullptr && list->isVisible());
                if (list != nullptr)
                {
                    expect (list->getListBoxModel()->getNumRows() >= 1 + glitchkid::FactoryPresets::count, "the list holds the factory default and the built-in presets");
                }

                // SETTINGS, from the gear and from the tab
                click ("SETTINGS");
                expectEquals (p.getUiInt ("uiPage", -1), 3);
                expect (visible ("ANIMATIONS: ON") && visible ("SHOW PRESET FOLDER") && ! visible ("SAVE PRESET"));
                click ("ANIMATIONS: ON");
                expect (! p.getUiFlag ("uiMotion", true));
                juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
                expect (visible ("ANIMATIONS: OFF"));
                click ("HOME");
                expect (visible ("ANALYZE VOCAL"));
                walk (*content, [&] (juce::Component& c) { if (auto* g = dynamic_cast<glitchkid::ui::GearButton*> (&c)) { g->triggerClick(); } });
                settle();
                expectEquals (p.getUiInt ("uiPage", -1), 3, "the gear opens SETTINGS");
            }

            // HOME in every AutoMode state, a loaded Sound World, the dry state and still mode
            {
                GlitchKidProcessor p;
                p.setUiInt ("uiPage", 0);
                p.prepareToPlay (48000.0, 480);
                std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
                auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("glitchkid-ed-" + juce::String::toHexString (juce::Random().nextInt64()));
                glitchkid::PresetController c (p, glitchkid::PresetStore (dir));
                c.loadFactory (0);
                snapshot (*ed, "home_world_loaded");
                p.getAnalyzer().start();
                vocalBlocks (p, 1.0);
                snapshot (*ed, "home_listening");
                vocalBlocks (p, 12.0);
                snapshot (*ed, "home_ready");
                p.applyAutoMode (0.7f);
                snapshot (*ed, "home_applied");
                th::setParam (p, "bypass", 1.0f);
                snapshot (*ed, "home_dry");
                th::setParam (p, "bypass", 0.0f);
                p.setUiFlag ("uiMotion", false);
                snapshot (*ed, "home_still");
                p.getAnalyzer().start();
                juce::MidiBuffer midi;
                th::Buf quiet (2, 480);
                p.processBlock (quiet, midi);
                p.getAnalyzer().stop();
                p.processBlock (quiet, midi);
                snapshot (*ed, "home_error");
                ed.reset();
                dir.deleteRecursively();
            }
            {
                GlitchKidProcessor p;
                th::setLayout (p, juce::AudioChannelSet::mono());
                std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
                snapshot (*ed, "mono");
            }
        }
    }
};
MacroTests macroTests;
}
