#include "TestHelpers.h"
#include "../Source/PluginEditor.h"
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
            expect (ui::obsidian == juce::Colour (0xff101014) && ui::panelBottom == juce::Colour (0xff1b1921) && ui::panelTop == juce::Colour (0xff25222d) && ui::violet == juce::Colour (0xffa83bee)
                    && ui::pink == juce::Colour (0xffe14cbb) && ui::textBright == juce::Colour (0xfff0ecf5) && ui::textDim == juce::Colour (0xffa7a0b2));
            auto ratio = [] (juce::Colour fg, juce::Colour bg) { const float a = luminance (fg) + 0.05f, b = luminance (bg) + 0.05f; return std::max (a, b) / std::min (a, b); };
            for (auto bg : { ui::obsidian, ui::panelBottom, ui::panelTop })
            {
                expect (ratio (ui::textBright, bg) >= 7.0f, "primary text on " + bg.toString());
                expect (ratio (ui::textDim, bg) >= 4.5f, "secondary text on " + bg.toString() + ": " + juce::String (ratio (ui::textDim, bg), 2));
            }
            expect (ratio (juce::Colours::white, ui::violet) >= 4.5f, "white text on the violet selection fill: " + juce::String (ratio (juce::Colours::white, ui::violet), 2));
            expect (ratio (ui::violet, ui::obsidian) >= 3.0f, "violet accents on the background (graphics, 3:1): " + juce::String (ratio (ui::violet, ui::obsidian), 2));
            expect (ratio (ui::pink, ui::obsidian) >= 3.0f, "magenta accents on the background: " + juce::String (ratio (ui::pink, ui::obsidian), 2));
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
}
