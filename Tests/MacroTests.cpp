#include "TestHelpers.h"
#include "../Source/Macros/MacroEngine.h"
#include "../Source/Presets/PresetController.h"
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
            expect (a.getUiFlag ("uiMotion", true) && a.getUiInt ("uiPage", 0) == 0 && a.getUiInt ("uiCategory", 0) == 0);
            const auto before = a.captureSoundState();
            a.setUiFlag ("uiMotion", false);
            a.setUiInt ("uiPage", 2);
            a.setUiInt ("uiCategory", 3);
            expectEquals ((int) a.captureSoundState().values.size(), (int) before.values.size());
            juce::MemoryBlock mb;
            a.getStateInformation (mb);
            GlitchKidProcessor b;
            b.setStateInformation (mb.getData(), (int) mb.getSize());
            expect (! b.getUiFlag ("uiMotion", true) && b.getUiInt ("uiPage", 0) == 2 && b.getUiInt ("uiCategory", 0) == 3);
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

        beginTest ("editor: builds, lays out, animates and renders at several sizes, with the LAB closed and open, in every AutoMode state");
        {
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

            // every page, and every MODIFY category, renders
            for (int pg = 0; pg < 3; ++pg)
                for (int cat = 0; cat < (pg == 2 ? 5 : 1); ++cat)
                {
                    GlitchKidProcessor p;
                    p.setUiInt ("uiPage", pg);
                    p.setUiInt ("uiCategory", cat);
                    p.prepareToPlay (48000.0, 480);
                    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
                    expect (ed != nullptr);
                    snapshot (*ed, "page" + juce::String (pg) + (pg == 2 ? "_cat" + juce::String (cat) : juce::String()));
                }

            // resizing keeps the layout (and does not crash) at the limits
            {
                GlitchKidProcessor p;
                p.setUiInt ("uiPage", 2);
                p.setUiInt ("uiCategory", 3);
                std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
                const int w0 = ed->getWidth(), h0 = ed->getHeight();
                for (int w : { w0 * 3 / 5 + 10, w0, w0 * 3 / 2 - 10 })
                {
                    ed->setSize (w, w * h0 / w0);
                    snapshot (*ed, "resized_" + juce::String (w));
                }
            }

            // navigation: the real buttons switch pages, and each page shows only its own controls
            {
                GlitchKidProcessor p;
                p.prepareToPlay (48000.0, 480);
                std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
                auto* content = ed->getChildComponent (0);
                expect (content != nullptr);

                std::function<juce::TextButton*(juce::Component&, const juce::String&)> findButton = [&] (juce::Component& root, const juce::String& text) -> juce::TextButton*
                {
                    for (auto* c : root.getChildren())
                    {
                        if (auto* b = dynamic_cast<juce::TextButton*> (c); b != nullptr && b->getButtonText() == text)
                            return b;
                        if (auto* r = findButton (*c, text))
                            return r;
                    }
                    return nullptr;
                };
                auto visible = [&] (const juce::String& text) { auto* b = findButton (*content, text); return b != nullptr && b->isVisible(); };
                auto click = [&] (const juce::String& text)
                {
                    auto* b = findButton (*content, text);
                    expect (b != nullptr && b->isVisible(), "button '" + text + "' is there to click");
                    if (b != nullptr)
                    {
                        b->triggerClick(); // posts the click: let the message loop deliver it
                        juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
                    }
                };

                // first launch: TUNE
                expect (visible ("NEXT  >") && visible ("HARD") && ! visible ("ANALYZE VOCAL") && ! visible ("PITCH & KEY") && ! visible ("RESET"));
                expect (visible ("1  TUNE") && visible ("2  AUTO") && visible ("3  MODIFY"), "the three steps are always shown");
                expect (visible ("SAVE") && visible ("SET A"), "the preset bar is always shown");

                click ("HARD"); // a real action: sets Pitch Tune
                expectEquals (th::getParam (p, "tune_on"), 1.0f);
                expectEquals (th::getParam (p, "tune_strength"), 100.0f);
                expectEquals (th::getParam (p, "tune_speed"), 0.0f);

                click ("NEXT  >");
                expectEquals (p.getUiInt ("uiPage", -1), 1);
                expect (visible ("ANALYZE VOCAL") && visible ("APPLY SOUND") && visible ("MODIFY  >") && visible ("DRY") && ! visible ("NEXT  >") && ! visible ("HARD"));

                click ("DRY");
                expectEquals (th::getParam (p, "bypass"), 1.0f);
                click ("PROCESSED");
                expectEquals (th::getParam (p, "bypass"), 0.0f);

                click ("MODIFY  >");
                expectEquals (p.getUiInt ("uiPage", -1), 2);
                expect (visible ("PITCH & KEY") && visible ("<  BACK TO AUTO") && visible ("RESET") && visible ("SAVE PRESET") && visible ("DARKER") && ! visible ("ANALYZE VOCAL"));

                // each category shows only its own controls (checked with the Alter Ego buttons and the DRY/PROCESSED switch as markers)
                click ("CHARACTER");
                expectEquals (p.getUiInt ("uiCategory", -1), 4);
                expect (visible ("SURPRISE ME") && visible ("CELESTIAL") && ! visible ("HARD"));
                click ("PITCH & KEY");
                expect (visible ("HARD") && ! visible ("SURPRISE ME") && ! visible ("CELESTIAL"));
                click ("TONE");
                expect (! visible ("HARD") && ! visible ("SURPRISE ME") && visible ("DARKER") && visible ("RESET"), "the footer stays, the category controls change");

                // a quick tweak is a real parameter change
                const float before = th::getParam (p, "eq_hi_gain");
                click ("DARKER");
                expect (th::getParam (p, "eq_hi_gain") < before - 1.0f);

                // reset asks first; the preset system underneath is the existing one
                click ("<  BACK TO AUTO");
                expect (visible ("ANALYZE VOCAL"));
                click ("1  TUNE");
                expect (visible ("NEXT  >"));
            }

            // the AUTO page in every AutoMode state, a loaded Sound World, the dry state and still mode
            {
                GlitchKidProcessor p;
                p.setUiInt ("uiPage", 1);
                p.prepareToPlay (48000.0, 480);
                std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
                auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("glitchkid-ed-" + juce::String::toHexString (juce::Random().nextInt64()));
                glitchkid::PresetController c (p, glitchkid::PresetStore (dir));
                c.loadFactory (0);
                snapshot (*ed, "auto_world_loaded");
                p.getAnalyzer().start();
                vocalBlocks (p, 1.0);
                snapshot (*ed, "auto_listening");
                vocalBlocks (p, 12.0);
                snapshot (*ed, "auto_ready");
                p.applyAutoMode (0.7f);
                snapshot (*ed, "auto_applied");
                th::setParam (p, "bypass", 1.0f);
                snapshot (*ed, "auto_dry");
                th::setParam (p, "bypass", 0.0f);
                p.setUiFlag ("uiMotion", false);
                snapshot (*ed, "auto_still");
                p.getAnalyzer().start();
                juce::MidiBuffer midi;
                th::Buf quiet (2, 480);
                p.processBlock (quiet, midi);
                p.getAnalyzer().stop();
                p.processBlock (quiet, midi);
                snapshot (*ed, "auto_error");
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
