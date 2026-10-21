#include "TestHelpers.h"
#include "../Source/Macros/MacroEngine.h"
#include "../Source/Presets/PresetController.h"
#include "../Source/UI/PlaygroundWidgets.h"
#include "../Source/Presets/FactoryPresets.h"
#include "../Source/DSP/ParametricEq.h"
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
                { 1, { "comp_ratio", "comp_thresh", "comp_attack", "comp_release", "comp_knee", "comp_makeup" } },
                { 2, { "eq_hi_gain", "eq_b2_gain" } } };
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
            a.setUiInt ("uiProfile", 5);
            expectEquals ((int) a.captureSoundState().values.size(), (int) before.values.size());
            juce::MemoryBlock mb;
            a.getStateInformation (mb);
            GlitchKidProcessor b;
            b.setStateInformation (mb.getData(), (int) mb.getSize());
            expect (! b.getUiFlag ("uiMotion", true) && b.getUiInt ("uiPage", 0) == 2 && b.getUiInt ("uiCategory", 0) == 3 && b.getUiInt ("uiProfile", -1) == 5);
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

        beginTest ("EQ graph: the calculated response matches the real ParametricEq output, and the locked palette tokens are in place");
        {
            const double fs = 48000.0;
            const float F[4] = { 100.0f, 400.0f, 3000.0f, 8000.0f }, G[4] = { 5.0f, -4.0f, 6.0f, -5.0f }, Q[4] = { 0.7071f, 2.0f, 1.2f, 0.7071f };
            for (float hz : { 60.0f, 150.0f, 400.0f, 1000.0f, 3000.0f, 6000.0f, 10000.0f })
            {
                ParametricEq eq;
                eq.prepare (fs);
                for (int b = 0; b < 4; ++b)
                    eq.setBand (b, { F[b], G[b], Q[b] });
                const int n = (int) fs * 2;
                double inE = 0.0, outE = 0.0;
                for (int pos = 0; pos < n; pos += 480)
                {
                    th::Buf buf (1, 480);
                    for (int i = 0; i < 480; ++i)
                        buf.setSample (0, i, 0.3f * (float) std::sin (2.0 * 3.14159265358979 * (double) hz * (double) (pos + i) / fs));
                    eq.process (buf);
                    if (pos > n * 3 / 4) // after the 20 ms settings smoothing and the filter transient
                        for (int i = 0; i < 480; ++i)
                        {
                            const double x = 0.3 * std::sin (2.0 * 3.14159265358979 * (double) hz * (double) (pos + i) / fs);
                            inE += x * x;
                            outE += (double) buf.getSample (0, i) * (double) buf.getSample (0, i);
                        }
                }
                const float measured = (float) (10.0 * std::log10 (outE / inE));
                const float calculated = glitchkid::ui::EqGraph::responseDb (F, G, Q, hz, fs);
                expectWithinAbsoluteError (calculated, measured, 0.15f, "response at " + juce::String (hz) + " Hz: graph " + juce::String (calculated) + " dB, DSP " + juce::String (measured) + " dB");
            }
            const float flat[4] = { 100.0f, 400.0f, 3000.0f, 8000.0f }, zero[4] = { 0, 0, 0, 0 };
            expectEquals (glitchkid::ui::EqGraph::responseDb (flat, zero, Q, 1000.0f, fs), 0.0f);

            namespace ui = glitchkid::ui;
            expect (ui::obsidian == juce::Colour (0xff08060d) && ui::panelBottom == juce::Colour (0xff15111b) && ui::violet == juce::Colour (0xffc026ff)
                    && ui::pink == juce::Colour (0xffff3dae) && ui::textBright == juce::Colour (0xffe8ddf2) && ui::textDim == juce::Colour (0xff9b91a5)
                    && ui::border == juce::Colour (0xff392044), "the locked brand colours");
        }

        beginTest ("editor: builds, lays out and renders at several sizes; three pages, four MODIFY categories, eight profiles, BUILD end to end");
        {
            const juce::String outDir = juce::SystemStats::getEnvironmentVariable ("GLITCHKID_SNAPSHOT_DIR", {});
            auto snapshot = [&] (juce::AudioProcessorEditor& ed, const juce::String& shot)
            {
                juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
                auto img = ed.createComponentSnapshot (ed.getLocalBounds(), true, 1.0f);
                expect (img.isValid() && img.getWidth() == ed.getWidth());
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

            auto feedBlocks = [] (GlitchKidProcessor& p, double seconds, bool silent)
            {
                const auto x = vocalish (48000.0, seconds);
                juce::MidiBuffer midi;
                for (size_t pos = 0; pos + 480 <= x.size(); pos += 480)
                {
                    th::Buf b (2, 480);
                    for (int i = 0; i < 480; ++i) { const float v = silent ? 0.0f : x[pos + (size_t) i]; b.setSample (0, i, v); b.setSample (1, i, v); }
                    p.processBlock (b, midi);
                }
            };

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
                        b->triggerClick();
                        juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
                    }
                };
                std::vector<glitchkid::ui::ProfileCard*> cards;
                auto collectCards = [&]
                {
                    cards.clear();
                    std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
                    {
                        for (auto* ch : c.getChildren())
                        {
                            if (auto* a = dynamic_cast<glitchkid::ui::ProfileCard*> (ch); a != nullptr && a->isVisible())
                                cards.push_back (a);
                            walk (*ch);
                        }
                    };
                    walk (*content);
                };

                expect (visible ("NEXT: AUTO  >") && visible ("HARD") && ! visible ("BUILD MY VOCAL MIX") && ! visible ("PITCH & KEY") && ! visible ("RESET"));
                expect (visible ("TUNE") && visible ("AUTO") && visible ("MODIFY") && visible ("DRY") && visible ("PROCESSED"), "the three workspaces and DRY / PROCESSED are always shown");
                expect (visible ("SAVE") && visible ("SET A"), "the preset bar is always shown");
                expect (findButton (*content, "CHARACTER") == nullptr && findButton (*content, "SURPRISE ME") == nullptr, "no Character tab");

                click ("HARD");
                expectEquals (th::getParam (p, "tune_on"), 1.0f);
                expectEquals (th::getParam (p, "tune_strength"), 100.0f);
                expectEquals (th::getParam (p, "tune_speed"), 0.0f);

                {
                    glitchkid::ui::NoteKey* d = nullptr;
                    int keys = 0;
                    std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
                    {
                        for (auto* ch : c.getChildren())
                        {
                            if (auto* k = dynamic_cast<glitchkid::ui::NoteKey*> (ch); k != nullptr && k->isVisible())
                            {
                                ++keys;
                                if (k->getButtonText() == "D")
                                    d = k;
                            }
                            walk (*ch);
                        }
                    };
                    walk (*content);
                    expectEquals (keys, 12);
                    expect (d != nullptr);
                    if (d != nullptr)
                    {
                        d->triggerClick();
                        juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
                        expectEquals (th::getParam (p, "tune_key"), 2.0f);
                    }
                    click ("Natural minor");
                    expectEquals (th::getParam (p, "tune_scale"), 2.0f);
                    snapshot (*ed, "tune_active");
                    click ("Chromatic");
                    expectEquals (th::getParam (p, "tune_scale"), 0.0f);
                }

                click ("NEXT: AUTO  >");
                expectEquals (p.getUiInt ("uiPage", -1), 1);
                expect (visible ("BUILD MY VOCAL MIX") && visible ("WHAT CHANGED") && visible ("MODIFY  >") && visible ("DRY") && ! visible ("NEXT: AUTO  >") && ! visible ("HARD"));

                collectCards();
                expectEquals ((int) cards.size(), 8, "all eight profiles are on one page");

                // select -> LOAD PROFILE -> BACK TO MY SOUND
                {
                    expect (findButton (*content, "BACK TO MY SOUND") != nullptr && ! findButton (*content, "BACK TO MY SOUND")->isEnabled(), "nothing to go back to yet");
                    th::setParam (p, "gain", -4.0f);
                    th::setParam (p, "echo_mix", 61.0f);
                    const int first = cards[3]->getProfile();
                    cards[3]->triggerClick();
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
                    expectEquals (p.getUiInt ("uiProfile", -1), first, "the selected profile is remembered in the session");
                    expectEquals (th::getParam (p, "echo_mix"), 61.0f, "selecting a card only selects: nothing is loaded");
                    click ("LOAD PROFILE");
                    expectEquals (p.getPresetName(), juce::String (glitchkid::FactoryPresets::info (first).name));
                    expect (std::abs (th::getParam (p, "echo_mix") - 61.0f) > 0.5f, "the profile replaced the sound");
                    expectEquals (th::getParam (p, "gain"), -4.0f); // gain is the user's, a profile never sets it
                    expect (findButton (*content, "BACK TO MY SOUND")->isEnabled());
                    click ("BACK TO MY SOUND");
                    expectEquals (th::getParam (p, "echo_mix"), 61.0f);
                    expectEquals (th::getParam (p, "gain"), -4.0f);
                    th::setParam (p, "gain", -2.0f);
                    th::setParam (p, "echo_mix", 0.0f);
                }

                // BUILD MY VOCAL MIX through the real button: listens, builds, applies, can be undone; silence fails honestly
                {
                    const auto sBefore = p.captureSoundState();
                    expect (! findButton (*content, "WHAT CHANGED")->isEnabled(), "nothing to explain before a build");
                    collectCards();
                    cards[1]->triggerClick();
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
                    click ("BUILD MY VOCAL MIX");
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
                    feedBlocks (p, 12.0, false);
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
                    expect (p.getPresetName().startsWith ("Built: "), "a mix was built: " + p.getPresetName());
                    expect (p.getPresetName().containsIgnoreCase (glitchkid::FactoryPresets::info (cards[1]->getProfile()).name), "built from the selected profile");
                    int moved = 0;
                    for (const auto& e : p.captureSoundState().values)
                        moved += std::abs (e.value - sBefore.get (e.id, e.value)) > 0.06f ? 1 : 0;
                    expect (moved > 5, "the build really moved the parameters: " + juce::String (moved));
                    expect (findButton (*content, "WHAT CHANGED")->isEnabled() && findButton (*content, "BUILD AGAIN") != nullptr);
                    snapshot (*ed, "auto_built");
                    expect (std::abs (th::getParam (p, "gain") + 2.0f) < 0.06f, "Gain is never touched");
                    expectEquals (th::getParam (p, "tune_key"), 2.0f); // key and scale stay the user's
                    click ("BACK TO MY SOUND");
                    for (const auto& e : sBefore.values)
                        expect (std::abs (th::getParam (p, e.id.toRawUTF8()) - e.value) < 0.06f, "undo restored " + e.id);

                    const auto sQuiet = p.captureSoundState();
                    click ("BUILD AGAIN");
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
                    feedBlocks (p, 31.0, true);
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
                    expect (findButton (*content, "TRY AGAIN") != nullptr, "a silent input is reported as a failure");
                    for (const auto& e : sQuiet.values)
                        expect (std::abs (th::getParam (p, e.id.toRawUTF8()) - e.value) < 0.06f, "a failed build changed nothing: " + e.id);
                }

                click ("DRY");
                expectEquals (th::getParam (p, "bypass"), 1.0f);
                click ("PROCESSED");
                expectEquals (th::getParam (p, "bypass"), 0.0f);

                click ("MODIFY  >");
                expectEquals (p.getUiInt ("uiPage", -1), 2);
                expect (visible ("PITCH & KEY") && visible ("TONE") && visible ("DYNAMICS") && visible ("SPACE") && visible ("<  BACK TO AUTO") && visible ("RESET") && visible ("SAVE PRESET") && ! visible ("BUILD MY VOCAL MIX"));

                click ("PITCH & KEY");
                expectEquals (p.getUiInt ("uiCategory", -1), 0);
                expect (visible ("HARD"));
                click ("TONE");
                expect (! visible ("HARD") && visible ("RESET"), "the footer stays, the category controls change");
                click ("DYNAMICS");
                expectEquals (p.getUiInt ("uiCategory", -1), 2);
                click ("SPACE");
                expectEquals (p.getUiInt ("uiCategory", -1), 3);

                // CLEAN LAB: the switch, the amount and the section trims are attached to the real parameters; the read-outs say what is true; RESET CLEAN restores the defaults
                {
                    click ("CLEAN");
                    expectEquals (p.getUiInt ("uiCategory", -1), 4);
                    expect (visible ("RESET CLEAN") && ! visible ("HARD"), "the CLEAN page shows its own controls");
                    juce::ToggleButton* cleanOn = nullptr;
                    std::vector<juce::Slider*> sliders;
                    std::vector<juce::Label*> labels;
                    std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
                    {
                        for (auto* ch : c.getChildren())
                        {
                            if (auto* tb = dynamic_cast<juce::ToggleButton*> (ch); tb != nullptr && tb->isVisible() && tb->getButtonText() == "CLEAN LAB")
                                cleanOn = tb;
                            if (auto* sl = dynamic_cast<juce::Slider*> (ch); sl != nullptr && sl->isVisible() && sl->getSliderStyle() == juce::Slider::LinearHorizontal
                                                                                 && sl->getTextBoxPosition() == juce::Slider::TextBoxRight)
                                sliders.push_back (sl);
                            if (auto* lb = dynamic_cast<juce::Label*> (ch); lb != nullptr && lb->isVisible())
                                labels.push_back (lb);
                            walk (*ch);
                        }
                    };
                    walk (*content);
                    expect (cleanOn != nullptr && sliders.size() == 4, "the switch and the amount plus three section trims are shown");
                    auto labelWith = [&] (const juce::String& start)
                    {
                        for (auto* lb : labels)
                            if (lb->getText().startsWith (start))
                                return lb;
                        return static_cast<juce::Label*> (nullptr);
                    };
                    expect (labelWith ("CLEAN LAB is off") != nullptr, "while off it says so");
                    if (cleanOn != nullptr && sliders.size() == 4)
                    {
                        cleanOn->triggerClick();
                        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
                        expectEquals (th::getParam (p, "clean_on"), 1.0f);
                        sliders[0]->setValue (80.0, juce::sendNotificationSync); // CLEAN
                        sliders[1]->setValue (40.0, juce::sendNotificationSync); // corrective EQ
                        sliders[2]->setValue (30.0, juce::sendNotificationSync); // resonance
                        sliders[3]->setValue (20.0, juce::sendNotificationSync); // de-esser
                        expectWithinAbsoluteError (th::getParam (p, "clean_amount"), 80.0f, 0.1f);
                        expectWithinAbsoluteError (th::getParam (p, "clean_tone"), 40.0f, 0.1f);
                        expectWithinAbsoluteError (th::getParam (p, "clean_res"), 30.0f, 0.1f);
                        expectWithinAbsoluteError (th::getParam (p, "clean_ess"), 20.0f, 0.1f);
                        feedBlocks (p, 1.0, false); // a second of audio: CLEAN LAB can only say it is still learning
                        juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
                        labels.clear();
                        walk (*content);
                        expect (labelWith ("Learning your voice") != nullptr && labelWith ("Rumble: none found") != nullptr, "while on and still learning it says so and invents no correction");
                        snapshot (*ed, "clean_page");
                        click ("RESET CLEAN");
                        expectEquals (th::getParam (p, "clean_on"), 0.0f);
                        expectEquals (th::getParam (p, "clean_amount"), 50.0f);
                        expectEquals (th::getParam (p, "clean_tone"), 100.0f);
                    }
                }
                click ("SPACE");

                // detailed knobs are attached to the real (automatable) parameters, in both directions
                {
                    click ("TONE");
                    juce::Slider* knob = nullptr;
                    std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
                    {
                        for (auto* ch : c.getChildren())
                        {
                            if (auto* sl = dynamic_cast<juce::Slider*> (ch); sl != nullptr && sl->isVisible() && sl->getSliderStyle() == juce::Slider::RotaryHorizontalVerticalDrag && knob == nullptr)
                                knob = sl; // the first TONE knob is the selected band's FREQ (default band: PRESENCE, eq_b2_freq)
                            walk (*ch);
                        }
                    };
                    walk (*content);
                    expect (knob != nullptr);
                    if (knob != nullptr)
                    {
                        knob->setValue (2500.0, juce::sendNotificationSync);
                        expectWithinAbsoluteError (th::getParam (p, "eq_b2_freq"), 2500.0f, 2.0f);
                        th::setParam (p, "eq_b2_freq", 4000.0f);
                        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
                        expectWithinAbsoluteError ((float) knob->getValue(), 4000.0f, 2.0f);
                        th::setParam (p, "eq_b2_freq", 3000.0f);
                    }

                    // the EQ graph is a real control: dragging its PRESENCE point writes the real frequency and gain; the knobs and the graph agree
                    glitchkid::ui::EqGraph* graph = nullptr;
                    std::function<void (juce::Component&)> walkG = [&] (juce::Component& c)
                    {
                        for (auto* ch : c.getChildren())
                        {
                            if (auto* eg = dynamic_cast<glitchkid::ui::EqGraph*> (ch))
                                graph = eg;
                            walkG (*ch);
                        }
                    };
                    walkG (*content);
                    expect (graph != nullptr && graph->isVisible(), "the EQ graph is on the TONE page");
                    if (graph != nullptr)
                    {
                        auto ev = [&] (juce::Point<float> pos, bool dragged)
                        {
                            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), pos, juce::ModifierKeys(), 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, graph, graph,
                                                     juce::Time::getCurrentTime(), graph->pointOf (2), juce::Time::getCurrentTime(), 1, dragged);
                        };
                        const auto start = graph->pointOf (2);
                        graph->mouseDown (ev (start, false));
                        graph->mouseDrag (ev (start.translated (60.0f, -40.0f), true)); // up and to the right: more gain, higher frequency
                        graph->mouseUp (ev (start.translated (60.0f, -40.0f), true));
                        expect (th::getParam (p, "eq_b2_gain") > 3.0f, "dragging up raised the real gain: " + juce::String (th::getParam (p, "eq_b2_gain")));
                        expect (th::getParam (p, "eq_b2_freq") > 3500.0f, "dragging right raised the real frequency: " + juce::String (th::getParam (p, "eq_b2_freq")));
                        const auto moved = graph->pointOf (2);
                        expect (moved.getDistanceFrom (start.translated (60.0f, -40.0f)) < 4.0f, "the point follows the parameters");
                        snapshot (*ed, "tone_eq_dragged");
                        graph->mouseDoubleClick (ev (moved, false)); // flatten the band
                        expectEquals (th::getParam (p, "eq_b2_gain"), 0.0f);
                        th::setParam (p, "eq_b2_freq", 3000.0f);
                    }
                }

                // AIR LAB: the switch and the sliders are attached to the real parameters, and the chain's TONE stage lights up while it is on
                {
                    click ("TONE");
                    juce::ToggleButton* airOn = nullptr;
                    std::vector<juce::Slider*> airSliders;
                    std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
                    {
                        for (auto* ch : c.getChildren())
                        {
                            if (auto* tb = dynamic_cast<juce::ToggleButton*> (ch); tb != nullptr && tb->isVisible() && tb->getButtonText() == "AIR LAB")
                                airOn = tb;
                            if (auto* sl = dynamic_cast<juce::Slider*> (ch); sl != nullptr && sl->isVisible() && sl->getSliderStyle() == juce::Slider::LinearHorizontal
                                                                                 && sl->getTextBoxPosition() == juce::Slider::TextBoxRight)
                                airSliders.push_back (sl);
                            walk (*ch);
                        }
                    };
                    walk (*content);
                    expect (airOn != nullptr && airSliders.size() == 4, "the AIR LAB switch and its four sliders are on the TONE page");
                    if (airOn != nullptr && airSliders.size() == 4)
                    {
                        expectEquals (th::getParam (p, "air_on"), 0.0f);
                        airOn->triggerClick();
                        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
                        expectEquals (th::getParam (p, "air_on"), 1.0f);
                        airSliders[0]->setValue (65.0, juce::sendNotificationSync);  // PRESENCE
                        airSliders[2]->setValue (40.0, juce::sendNotificationSync);  // MIX
                        airSliders[3]->setValue (-2.5, juce::sendNotificationSync);  // TRIM
                        expectWithinAbsoluteError (th::getParam (p, "air_presence"), 65.0f, 0.1f);
                        expectWithinAbsoluteError (th::getParam (p, "air_mix"), 40.0f, 0.1f);
                        expectWithinAbsoluteError (th::getParam (p, "air_trim"), -2.5f, 0.1f);
                        th::setParam (p, "air_air", 80.0f);
                        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
                        expectWithinAbsoluteError ((float) airSliders[1]->getValue(), 80.0f, 0.1f); // AIR follows the parameter
                        snapshot (*ed, "tone_air_lab");
                        th::setParam (p, "air_on", 0.0f);
                        th::setParam (p, "air_presence", 30.0f);
                        th::setParam (p, "air_air", 30.0f);
                        th::setParam (p, "air_mix", 100.0f);
                        th::setParam (p, "air_trim", 0.0f);
                    }
                }

                // the TONE high-level slider is a real control: it moves the AIR and PRESENCE gains together and reads back
                {
                    click ("TONE");
                    juce::Slider* tone = nullptr;
                    std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
                    {
                        for (auto* ch : c.getChildren())
                        {
                            if (auto* sl = dynamic_cast<juce::Slider*> (ch); sl != nullptr && sl->isVisible() && sl->getSliderStyle() == juce::Slider::LinearHorizontal
                                                                                 && sl->getTextBoxPosition() == juce::Slider::NoTextBox)
                                tone = sl;
                            walk (*ch);
                        }
                    };
                    walk (*content);
                    expect (tone != nullptr, "the TONE slider is shown");
                    if (tone != nullptr)
                    {
                        tone->setValue (0.0, juce::sendNotificationSync);
                        expect (th::getParam (p, "eq_hi_gain") < -5.5f && th::getParam (p, "eq_b2_gain") < -1.0f, "DARKER really lowers the EQ");
                        tone->setValue (1.0, juce::sendNotificationSync);
                        expect (th::getParam (p, "eq_hi_gain") > 5.5f, "BRIGHTER really raises the EQ");
                        th::setParam (p, "eq_hi_gain", 0.0f);
                        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
                        expectWithinAbsoluteError (tone->getValue(), 0.5, 0.02, "the slider follows the parameter");
                    }
                }

                // the processing chain is a real control: clicking a stage opens its MODIFY category
                {
                    glitchkid::ui::ChainStrip* chain = nullptr;
                    std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
                    {
                        for (auto* ch : c.getChildren())
                        {
                            if (auto* cs = dynamic_cast<glitchkid::ui::ChainStrip*> (ch))
                                chain = cs;
                            walk (*ch);
                        }
                    };
                    walk (*content);
                    expect (chain != nullptr && chain->isVisible(), "the chain is always shown");
                    if (chain != nullptr)
                    {
                        click ("AUTO");
                        const auto b = chain->nodeBounds (3); // DYNAMICS
                        const juce::MouseEvent down (juce::Desktop::getInstance().getMainMouseSource(), b.getCentre().toFloat(), juce::ModifierKeys(), 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, chain, chain,
                                                     juce::Time::getCurrentTime(), b.getCentre().toFloat(), juce::Time::getCurrentTime(), 1, false);
                        chain->mouseUp (down);
                        expectEquals (p.getUiInt ("uiPage", -1), 2);
                        expectEquals (p.getUiInt ("uiCategory", -1), 2);
                    }
                }

                click ("<  BACK TO AUTO");
                expect (visible ("TRY AGAIN"), "AUTO shows the build button (TRY AGAIN after the silent attempt)");
                click ("TUNE");
                expect (visible ("NEXT: AUTO  >"));
            }

            {
                GlitchKidProcessor p;
                p.setUiInt ("uiPage", 1);
                p.prepareToPlay (48000.0, 480);
                std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
                snapshot (*ed, "auto_idle");
                p.getAnalyzer().start();
                feedBlocks (p, 1.0, false);
                snapshot (*ed, "auto_listening");
                feedBlocks (p, 12.0, false);
                snapshot (*ed, "auto_ready");
                p.applyAutoMode (0.7f);
                snapshot (*ed, "auto_applied");
                th::setParam (p, "bypass", 1.0f);
                snapshot (*ed, "auto_dry");
                th::setParam (p, "bypass", 0.0f);
                p.setUiFlag ("uiMotion", false);
                snapshot (*ed, "auto_still");
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
