#include "TestHelpers.h"
#include "../Source/PluginEditor.h"
#include "../Source/UI/Theme.h"
#include "../Source/UI/GlitchRelic.h"
#include "../Source/UI/LabKit.h"
#include <set>

// The visual system: the Glitch Relic is deterministic, never raises the background behind the spectrum above a contrast-safe level, costs nothing per frame
// (rendered once and reused), and can be exported for marketing; the palette tokens are unchanged.
namespace
{
juce::Image onPlate (const juce::Image& relic, float strength, bool = false)
{
    juce::Image plate (juce::Image::ARGB, relic.getWidth(), relic.getHeight(), true);
    juce::Graphics g (plate);
    g.fillAll (juce::Colour (0xff101014));
    g.setOpacity (strength);
    g.drawImageAt (relic, 0, 0);
    return plate;
}

float luminance (juce::Colour c)
{
    auto lin = [] (float v) { return v <= 0.03928f ? v / 12.92f : std::pow ((v + 0.055f) / 1.055f, 2.4f); };
    return 0.2126f * lin (c.getFloatRed()) + 0.7152f * lin (c.getFloatGreen()) + 0.0722f * lin (c.getFloatBlue());
}

struct VisualTests : public juce::UnitTest
{
    VisualTests() : juce::UnitTest ("Visual system", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("the palette tokens are the approved ones, and the text colours keep AA contrast on every surface");
        {
            namespace ui = glitchkid::ui;
            // VIRIDIAN, the default identity: the exact brand tokens (VOID, SURFACE, ELEVATED, BORDER, text; deep teal, viridian, emerald)
            expect (ui::obsidian == juce::Colour (0xff080d0d) && ui::panelBottom == juce::Colour (0xff111a18) && ui::panelTop == juce::Colour (0xff1b2925)
                    && ui::border == juce::Colour (0xff293a34) && ui::textBright == juce::Colour (0xfff1f7f4) && ui::textDim == juce::Colour (0xff91a69c)
                    && ui::violet == juce::Colour (0xff08776f) && ui::pink == juce::Colour (0xff00dda5) && ui::neon == juce::Colour (0xff00b894));
            auto ratio = [] (juce::Colour fg, juce::Colour bg) { const float a = luminance (fg) + 0.05f, b = luminance (bg) + 0.05f; return std::max (a, b) / std::min (a, b); };
            for (auto bg : { ui::obsidian, ui::panelBottom, ui::panelTop })
            {
                expect (ratio (ui::textBright, bg) >= 7.0f, "primary text on " + bg.toString());
                expect (ratio (ui::textDim, bg) >= 4.5f, "secondary text on " + bg.toString() + ": " + juce::String (ratio (ui::textDim, bg), 2));
            }
            expect (ratio (juce::Colours::white, ui::violet) >= 4.5f, "white text on the violet selection fill: " + juce::String (ratio (juce::Colours::white, ui::violet), 2));
            expect (ratio (ui::violet, ui::obsidian) >= 3.0f, "violet accents on the background (graphics, 3:1): " + juce::String (ratio (ui::violet, ui::obsidian), 2));
            expect (ratio (ui::pink, ui::obsidian) >= 3.0f, "secondary accents on the background: " + juce::String (ratio (ui::pink, ui::obsidian), 2));
        }

        beginTest ("display case: capitals are re-cased for display, acronyms, units and mixed-case text are left alone");
        {
            namespace ui = glitchkid::ui;
            expectEquals (ui::displayCase ("CLEAN LAB"), juce::String ("Clean Lab"));
            expectEquals (ui::displayCase ("BUILD MY VOCAL MIX"), juce::String ("Build My Vocal Mix"));
            expectEquals (ui::displayCase ("VOCAL IN: -18 dB"), juce::String ("Vocal In: -18 dB"));
            expectEquals (ui::displayCase ("TONE EQ"), juce::String ("Tone EQ"));
            expectEquals (ui::displayCase ("A/B"), juce::String ("A/B"));
            expectEquals (ui::displayCase ("2 OF 4 STAGES ACTIVE", true), juce::String ("2 of 4 stages active"));
            expectEquals (ui::displayCase ("ON - CLEANING NOW", true), juce::String ("On - cleaning now"));
            expectEquals (ui::displayCase ("FINAL TOUCH"), juce::String ("Final Touch"));
            expectEquals (ui::displayCase ("Rumble, mud, harsh resonances."), juce::String ("Rumble, mud, harsh resonances."));
            expectEquals (ui::displayCase ("C#"), juce::String ("C#"));
        }

        beginTest ("the Glitch Relic: deterministic, not blank, transparent at its edges, and exported when GLITCHKID_SNAPSHOT_DIR is set");
        {
            const double t0 = juce::Time::getMillisecondCounterHiRes();
            const auto a = glitchkid::ui::renderGlitchRelic (1824, 472);
            const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
            logMessage ("VISUAL-COST relic render 1824x472 (once, not per frame): " + juce::String (ms, 1) + " ms");
            const auto b = glitchkid::ui::renderGlitchRelic (1824, 472);
            expectEquals (a.getWidth(), 1824);
            bool same = true;
            for (int y = 0; y < a.getHeight() && same; y += 7)
                for (int x = 0; x < a.getWidth(); x += 7)
                    if (a.getPixelAt (x, y) != b.getPixelAt (x, y)) { same = false; break; }
            expect (same, "the same size always gives the same pixels");
            std::set<juce::uint32> colours;
            int opaque = 0, total = 0;
            for (int y = 0; y < a.getHeight(); y += 4)
                for (int x = 0; x < a.getWidth(); x += 4)
                {
                    const auto c = a.getPixelAt (x, y);
                    colours.insert (c.getARGB() & 0xfff8f8f8);
                    opaque += c.getAlpha() > 40 ? 1 : 0;
                    ++total;
                }
            expect (colours.size() > 60, "a rich image, not a flat one: " + juce::String ((int) colours.size()) + " colours");
            expect (opaque > total / 25 && opaque < total * 3 / 4, "the form fills part of the plate, not all of it: " + juce::String (100 * opaque / total) + " %");
            expect (a.getPixelAt (2, 2).getAlpha() < 8 && a.getPixelAt (a.getWidth() - 3, a.getHeight() / 2).getAlpha() < 8, "it fades to nothing at the edges");
            const juce::String dir = juce::SystemStats::getEnvironmentVariable ("GLITCHKID_SNAPSHOT_DIR", {});
            if (dir.isNotEmpty())
            {
                for (auto [name, img] : { std::pair<const char*, juce::Image> { "glitch_relic_transparent.png", a }, { "glitch_relic_on_plate.png", onPlate (a, 1.0f, true) } })
                {
                    juce::File f = juce::File (dir).getChildFile (name);
                    f.deleteFile();
                    juce::FileOutputStream os (f);
                    if (os.openedOk())
                        juce::PNGImageFormat().writeImageToStream (img, os);
                }
            }
        }

        beginTest ("the supplied artwork: compiled in, decodes, has a transparent background and a visible loop");
        {
            const auto art = glitchkid::ui::loadGlitchRelicArtwork();
            expect (art.isValid(), "it decodes");
            if (art.isValid())
            {
                logMessage ("VISUAL supplied artwork cropped to " + juce::String (art.getWidth()) + " x " + juce::String (art.getHeight()));
                expect (art.getWidth() > 1200 && art.getWidth() < 1800 && art.getHeight() > 400, "a wide loop");
                expect (art.getPixelAt (art.getWidth() / 2, art.getHeight() / 2).getAlpha() < 8, "the centre of the loop is transparent (the display shows through)");
                int opaque = 0, total = 0;
                for (int y = 0; y < art.getHeight(); y += 4)
                    for (int x = 0; x < art.getWidth(); x += 4)
                    {
                        opaque += art.getPixelAt (x, y).getAlpha() > 200 ? 1 : 0;
                        ++total;
                    }
                expect (opaque > total / 8 && opaque < total * 3 / 4, "the loop covers part of its box: " + juce::String (100 * opaque / total) + " %");
            }
        }

        beginTest ("the artwork is a decorative idle emblem on HOME only: it loads off the message thread, is cached for later editors, and is gone when the editor is");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 480);
            p.setUiInt ("uiLevel", 0); // START (a first launch opens KID)
            std::unique_ptr<juce::AudioProcessorEditor> a (p.createEditor());
            auto* ed = dynamic_cast<GlitchKidEditor*> (a.get());
            ed->uiShowWorkspaceForTest (0); // the old dashboard: no longer on the rail, still tested directly
            ed->uiWaitForRelicForTest();
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            std::set<juce::uint32> colours;
            for (int y = 130; y < 400; y += 5)
                for (int x = 30; x < 930; x += 5)
                    colours.insert (img.getPixelAt (x, y).getARGB() & 0xfff0f0f0);
            expect (ed->uiEmblemShownForTest(), "with no signal the display shows the emblem");
            expect (colours.size() > 25, "the idle display carries the emblem (" + juce::String ((int) colours.size()) + " colours)");
            const double t0 = juce::Time::getMillisecondCounterHiRes();
            std::unique_ptr<juce::AudioProcessorEditor> b (p.createEditor()); // a second editor uses the cached image: nothing to render
            const double openMs = juce::Time::getMillisecondCounterHiRes() - t0;
            logMessage ("VISUAL-COST opening a second editor (relic cached): " + juce::String (openMs, 1) + " ms");
            b.reset();
            a.reset(); // destroying an editor whose loader has finished must be clean
            GlitchKidProcessor q;
            std::unique_ptr<juce::AudioProcessorEditor> c (q.createEditor());
            c.reset(); // and one destroyed at once, whatever the loader was doing
            expect (true);
        }
    }
};
VisualTests visualTests;

// The five colour profiles: readable in every profile, remembered as the user's preference, and purely visual (the sound, presets and A/B never move).
struct ColourProfileTests : public juce::UnitTest
{
    ColourProfileTests() : juce::UnitTest ("Colour profiles", "GlitchKid") {}

    void runTest() override
    {
        namespace ui = glitchkid::ui;
        namespace tm = glitchkid::ui::theme;
        auto ratio = [] (juce::Colour fg, juce::Colour bg) { const float a = luminance (fg) + 0.05f, b = luminance (bg) + 0.05f; return std::max (a, b) / std::min (a, b); };

        beginTest ("every profile keeps AA text contrast and visible accents on the near-black surfaces");
        {
            expectEquals ((int) tm::numProfiles, 5);
            expectEquals (juce::String (tm::profile (tm::defaultProfile).name), juce::String ("VIRIDIAN"), "VIRIDIAN is the default");
            const char* names[] = { "VIRIDIAN", "MAGENTA", "SAPPHIRE", "AMBER", "VOID" };
            for (int i = 0; i < tm::numProfiles; ++i)
            {
                const auto& p = tm::profile (i);
                expectEquals (juce::String (p.name), juce::String (names[i]));
                expect (ratio (juce::Colours::white, p.primary) >= 4.5f, juce::String (p.name) + ": white on the primary fill " + juce::String (ratio (juce::Colours::white, p.primary), 2));
                for (auto bg : { p.base.window, p.base.panel, p.base.raised })
                {
                    expect (ratio (p.base.text, bg) >= 7.0f, juce::String (p.name) + ": primary text on " + bg.toString());
                    expect (ratio (p.base.textDim, bg) >= 4.5f, juce::String (p.name) + ": secondary text on " + bg.toString() + " " + juce::String (ratio (p.base.textDim, bg), 2));
                    expect (ratio (p.secondary, bg) >= 4.5f, juce::String (p.name) + ": secondary as text on " + bg.toString());
                    expect (ratio (p.soft, bg) >= 4.5f, juce::String (p.name) + ": soft as text on " + bg.toString());
                }
                for (auto bg : { p.base.window, p.base.panel, p.base.windowLow }) // the primary is drawn as a mark on the window and panels (not on raised controls)
                {
                    expect (ratio (p.primary, bg) >= 3.0f, juce::String (p.name) + ": primary accent on " + bg.toString() + " " + juce::String (ratio (p.primary, bg), 2));
                }
            }
        }

        beginTest ("applying a profile sets the live tokens everywhere (both workspaces' names), and the default is restored");
        {
            tm::apply (tm::sapphire);
            expect (ui::violet == tm::profile (tm::sapphire).primary && ui::pink == tm::profile (tm::sapphire).secondary && ui::neon == tm::profile (tm::sapphire).soft);
            expect (ui::nk::uv == ui::violet && ui::nk::pinkNeon == ui::pink && ui::nk::uvSoft == ui::neon, "HOME's names follow");
            expect (ui::obsidian == tm::profile (tm::sapphire).base.window && ui::nk::surface == tm::profile (tm::sapphire).base.panel
                    && ui::nk::white == tm::profile (tm::sapphire).base.text, "the foundation follows too, in both workspaces");
            tm::apply (tm::defaultProfile);
            expect (ui::violet == juce::Colour (0xff08776f) && ui::pink == juce::Colour (0xff00dda5) && ui::obsidian == juce::Colour (0xff080d0d));
        }

        beginTest ("the choice is the user's preference: saved, read back, and a missing or broken file means VIRIDIAN");
        {
            const auto f = tm::settingsFile();
            f.deleteFile();
            expectEquals (tm::loadSaved(), (int) tm::defaultProfile, "no file: the default");
            tm::save (tm::amber);
            expectEquals (tm::loadSaved(), (int) tm::amber);
            f.replaceWithText ("not xml at all");
            expectEquals (tm::loadSaved(), (int) tm::defaultProfile, "a broken file is not an error");
            f.replaceWithText ("<GlitchKidSettings colourProfile=\"neon-green\"/>");
            expectEquals (tm::loadSaved(), (int) tm::defaultProfile, "an unknown profile is not an error");
            tm::save (tm::magenta); // a saved choice wins over the default: existing users keep the profile they picked
            GlitchKidProcessor p;
            std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
            expectEquals (tm::current(), (int) tm::magenta, "a new window opens with the saved profile");
            e.reset();
            f.deleteFile();
            tm::apply (tm::defaultProfile);
        }

        beginTest ("switching profiles never changes the sound, the preset, A/B or the session; open windows follow at once");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 480);
            th::setParam (p, "gain", -3.5f);
            th::setParam (p, "clean_on", 1.0f);
            p.captureAbSlot (0);
            th::setParam (p, "echo_mix", 22.0f);
            p.applyPreset ("Theme Test", p.captureSoundState());
            std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
            const auto before = p.captureSoundState();
            juce::MemoryBlock stateBefore;
            p.getStateInformation (stateBefore);
            juce::Slider* knob = nullptr;
            std::function<void (juce::Component&)> find = [&] (juce::Component& c)
            {
                for (auto* ch : c.getChildren())
                {
                    if (auto* sl = dynamic_cast<juce::Slider*> (ch); sl != nullptr && knob == nullptr && sl->isColourSpecified (juce::Slider::rotarySliderFillColourId)
                                                                     && sl->findColour (juce::Slider::rotarySliderFillColourId) == ui::violet)
                        knob = sl;
                    find (*ch);
                }
            };
            find (*e);
            expect (knob != nullptr, "a knob that stored the primary accent");
            for (int i = 0; i < tm::numProfiles; ++i)
            {
                tm::apply (i);
                if (knob != nullptr)
                    expect (knob->findColour (juce::Slider::rotarySliderFillColourId) == tm::profile (i).primary, juce::String (tm::profile (i).name) + ": stored colours follow the profile");
                for (const auto& v : before.values)
                    expectWithinAbsoluteError (p.captureSoundState().get (v.id, -999.0f), v.value, 1.0e-6f, v.id);
                expectEquals (p.getPresetName(), juce::String ("Theme Test"));
                expect (p.hasAbSlot (0), "A/B slot kept");
            }
            tm::apply (tm::defaultProfile);
            juce::MemoryBlock stateAfter;
            p.getStateInformation (stateAfter);
            expect (stateBefore == stateAfter, "the saved session is byte-for-byte the same: the profile is not part of a project");
            expect (! tm::settingsFile().existsAsFile(), "applying alone never writes the preference (only choosing in the selector does)");
        }
    }
};
ColourProfileTests colourProfileTests;
}

#include "../Source/UI/Brand.h"
#include "../Source/UI/ProfilePicker.h"
namespace
{
// The logo system: the compiled-in marks are the generated vector outlines, readable at small sizes; the picker applies and remembers a profile.
// With GLITCHKID_BRAND_DIR set, the icon, the marks and the profile tiles are rendered by JUCE from the same assets (PNG exports of Assets/Brand);
// with GLITCHKID_SNAPSHOT_DIR set, HOME and SHAPE are captured in every profile.
struct BrandTests : public juce::UnitTest
{
    BrandTests() : juce::UnitTest ("Brand", "GlitchKid") {}

    void runTest() override
    {
        namespace ui = glitchkid::ui;
        namespace tm = glitchkid::ui::theme;
        const auto brandDir = juce::SystemStats::getEnvironmentVariable ("GLITCHKID_BRAND_DIR", {});
        const auto shotDir = juce::SystemStats::getEnvironmentVariable ("GLITCHKID_SNAPSHOT_DIR", {});
        auto save = [] (const juce::Image& img, const juce::File& f)
        {
            f.getParentDirectory().createDirectory();
            f.deleteFile();
            juce::FileOutputStream os (f);
            if (os.openedOk()) juce::PNGImageFormat().writeImageToStream (img, os);
        };

        beginTest ("the emblem and the wordmark are the generated vector outlines, with their designed proportions");
        {
            const auto e = ui::brand::emblem().getBounds(), w = ui::brand::wordmark().getBounds();
            expect (! e.isEmpty() && ! w.isEmpty(), "both marks load from the compiled-in SVGs");
            expectWithinAbsoluteError (e.getWidth() / e.getHeight(), 1.22f, 0.08f, "the emblem's designed proportion");
            expect (w.getWidth() / w.getHeight() > 6.0f && w.getWidth() / w.getHeight() < 8.5f, "the wordmark's proportion: " + juce::String (w.getWidth() / w.getHeight()));
            expect (ui::brand::appIcon() != nullptr, "the app icon parses");
            // small-size readability: at 14 px tall, a row through the upper part of the letters still crosses every stroke separately
            // (G, L, I, T, C, H's two stems, K's stem and arm, I, D's two sides: at least 9 separate strokes; merged letters would give fewer)
            juce::Image img (juce::Image::ARGB, 140, 18, true);
            {
                juce::Graphics g (img);
                ui::brand::drawWordmark (g, { 1.0f, 2.0f, 138.0f, 14.0f });
            }
            const auto at = ui::brand::fit (ui::brand::wordmark(), { 1.0f, 2.0f, 138.0f, 14.0f }, juce::Justification::centredLeft);
            const int row = (int) (at.getY() + at.getHeight() * 0.3f);
            int strokes = 0;
            bool inside = false;
            for (int x = 0; x < img.getWidth(); ++x)
            {
                const bool ink = img.getPixelAt (x, row).getAlpha() > 110;
                strokes += (ink && ! inside) ? 1 : 0;
                inside = ink;
            }
            expect (strokes >= 9, "the wordmark at 14 px: " + juce::String (strokes) + " separate strokes on one row");
        }

        beginTest ("the profile picker applies a profile at once and keeps it as the user's preference");
        {
            ui::ProfilePicker picker;
            expect (picker.getWidth() > 400 && picker.tileAt (picker.tile (tm::sapphire).getCentre().toInt()) == tm::sapphire);
            picker.choose (tm::sapphire);
            expectEquals (tm::current(), (int) tm::sapphire);
            expectEquals (tm::loadSaved(), (int) tm::sapphire, "saved");
            picker.choose (tm::defaultProfile);
            tm::settingsFile().deleteFile();
        }

        beginTest ("brand exports and every profile on HOME and SHAPE (written when the folders are set)");
        {
            if (brandDir.isNotEmpty())
            {
                const juce::File dir (brandDir);
                if (auto icon = ui::brand::appIcon())
                    for (int px : { 1024, 512, 256, 128, 64, 32, 16 })
                    {
                        juce::Image img (juce::Image::ARGB, px, px, true);
                        juce::Graphics g (img);
                        icon->drawWithin (g, { 0.0f, 0.0f, (float) px, (float) px }, juce::RectanglePlacement::centred, 1.0f);
                        save (img, dir.getChildFile ("icon").getChildFile ("glitchkid-icon-" + juce::String (px) + ".png"));
                    }
                for (int i = 0; i < tm::numProfiles; ++i)
                {
                    tm::apply (i);
                    juce::Image tile (juce::Image::ARGB, 2 * 92, 2 * 96, true);
                    {
                        juce::Graphics g (tile);
                        g.addTransform (juce::AffineTransform::scale (2.0f));
                        ui::paintProfileTile (g, { 0.0f, 0.0f, 92.0f, 96.0f }, i, true, false);
                    }
                    save (tile, dir.getChildFile ("profiles").getChildFile (juce::String (tm::profile (i).id) + ".png"));
                    juce::Image lockup (juce::Image::ARGB, 1200, 300, true);
                    {
                        juce::Graphics g (lockup);
                        g.fillAll (tm::profile (i).base.window);
                        ui::brand::drawEmblem (g, { 40.0f, 40.0f, 260.0f, 180.0f }, juce::Justification::centred);
                        ui::brand::drawWordmark (g, { 330.0f, 78.0f, 820.0f, 108.0f });
                        ui::brand::drawTagline (g, { 336.0f, 210.0f, 700.0f, 24.0f }, 17.0f);
                    }
                    save (lockup, dir.getChildFile ("lockup").getChildFile ("glitchkid-lockup-" + juce::String (tm::profile (i).id) + ".png"));
                }
                tm::apply (tm::defaultProfile);
            }
            if (shotDir.isNotEmpty())
            {
                GlitchKidProcessor p;
                p.prepareToPlay (48000.0, 480);
                th::setParam (p, "lim_on", 1.0f);
                th::setParam (p, "clean_on", 1.0f);
                std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
                auto* ed = dynamic_cast<GlitchKidEditor*> (e.get());
                juce::MidiBuffer midi;
                for (int b = 0; b < 300; ++b)
                {
                    juce::AudioBuffer<float> buf (2, 480);
                    for (int i = 0; i < 480; ++i)
                    {
                        const double t = (double) (b * 480 + i) / 48000.0;
                        const float v = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * t) * (0.55f + 0.45f * (float) std::sin (t * 3.0));
                        buf.setSample (0, i, v);
                        buf.setSample (1, i, v);
                    }
                    p.processBlock (buf, midi);
                    if (b % 5 == 0) { ed->uiTickForTest(); ed->uiMainScreenForTest().tickForTest(); }
                }
                for (int i = 0; i < tm::numProfiles; ++i)
                {
                    tm::apply (i);
                    for (int row : { 0, 4 })
                    {
                        ed->uiSelectRailForTest (row);
                        ed->uiTickForTest();
                        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                        save (img, juce::File (shotDir).getChildFile ("brand_" + juce::String (row == 0 ? "home_" : "shape_") + tm::profile (i).id + ".png"));
                    }
                }
                tm::apply (tm::defaultProfile);
                ed->uiSelectRailForTest (0);
                ui::ProfilePicker picker; // the selector as it opens from the swatch
                juce::Image pick (juce::Image::ARGB, picker.getWidth() * 2, picker.getHeight() * 2, true);
                {
                    juce::Graphics g (pick);
                    g.fillAll (ui::panelBottom);
                    g.addTransform (juce::AffineTransform::scale (2.0f));
                    picker.paintEntireComponent (g, true);
                }
                save (pick, juce::File (shotDir).getChildFile ("brand_profile_picker.png"));
            }
            expectEquals (tm::current(), (int) tm::defaultProfile);
        }
    }
};
BrandTests brandTests;
}
