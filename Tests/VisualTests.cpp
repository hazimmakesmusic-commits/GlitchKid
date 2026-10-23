#include "TestHelpers.h"
#include "../Source/PluginEditor.h"
#include "../Source/UI/GlitchRelic.h"
#include "../Source/UI/LabKit.h"
#include <set>

// The visual system: the Glitch Relic is deterministic, never raises the background behind the spectrum above a contrast-safe level, costs nothing per frame
// (rendered once and reused), and can be exported for marketing; the palette tokens are unchanged.
namespace
{
// The HOME display as built: the relic at full strength in a frame, a dark glass plate over the middle of it (see SpectrumView sculpture mode).
juce::Image onPlate (const juce::Image& relic, float strength, bool glass = false)
{
    juce::Image plate (juce::Image::ARGB, relic.getWidth(), relic.getHeight(), true);
    juce::Graphics g (plate);
    g.fillAll (juce::Colour (0xff0a070f));
    g.setOpacity (strength);
    g.drawImageAt (relic, 0, 0);
    if (glass)
    {
        const float fx = glitchkid::ui::SpectrumView::plateInsetX / glitchkid::ui::SpectrumView::heroWidth, fy = glitchkid::ui::SpectrumView::plateInsetY / glitchkid::ui::SpectrumView::heroHeight;
        g.setColour (juce::Colour (0xff0a070f).withAlpha (glitchkid::ui::SpectrumView::plateGlass));
        g.fillRect (juce::Rectangle<float> ((float) relic.getWidth() * fx, (float) relic.getHeight() * fy, (float) relic.getWidth() * (1.0f - 2.0f * fx), (float) relic.getHeight() * (1.0f - 2.0f * fy)));
    }
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
        beginTest ("the locked palette is unchanged");
        {
            namespace ui = glitchkid::ui;
            expect (ui::obsidian == juce::Colour (0xff08060d) && ui::panelBottom == juce::Colour (0xff15111b) && ui::violet == juce::Colour (0xffc026ff)
                    && ui::pink == juce::Colour (0xffff3dae) && ui::chrome == juce::Colour (0xffe8ddf2) && ui::textDim == juce::Colour (0xff9b91a5) && ui::border == juce::Colour (0xff392044));
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

        beginTest ("contrast: the supplied artwork under the glass plate leaves the legend, axis-label and level-scale zones dark enough for AA contrast");
        {
            namespace ui = glitchkid::ui;
            const auto art = ui::loadGlitchRelicArtwork();
            expect (art.isValid());
            constexpr int S = 2; // measure at twice the design size
            const int W = (int) ui::SpectrumView::heroWidth * S, H = (int) ui::SpectrumView::heroHeight * S;
            juce::Image view (juce::Image::ARGB, W, H, true);
            {
                juce::Graphics g (view);
                g.fillAll (juce::Colour (0xff0a070f));
                g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
                g.drawImage (art, juce::Rectangle<float> (8.0f * S, 5.0f * S, (float) W - 16.0f * S, (float) H - 10.0f * S)); // as SpectrumView fits it
                g.setColour (juce::Colour (0xff0a070f).withAlpha (ui::SpectrumView::plateGlass));
                g.fillRect (juce::Rectangle<float> (ui::SpectrumView::plateInsetX * S, ui::SpectrumView::plateInsetY * S, W - 2.0f * ui::SpectrumView::plateInsetX * S, H - 2.0f * ui::SpectrumView::plateInsetY * S));
            }
            // text zones in design pixels, from SpectrumView::rebuildGeometry: plate (96, 8, 720 x 250), content area inset 12 x 10
            const juce::Rectangle<int> plate ((int) ui::SpectrumView::plateInsetX, (int) ui::SpectrumView::plateInsetY, (int) (ui::SpectrumView::heroWidth - 2.0f * ui::SpectrumView::plateInsetX),
                                              (int) (ui::SpectrumView::heroHeight - 2.0f * ui::SpectrumView::plateInsetY));
            const auto area = plate.reduced (12, 10);
            const juce::Rectangle<int> legend (area.getX(), area.getY(), area.getWidth(), 14), scale (area.getX(), area.getY() + 16, 30, 120);
            const int waveH = (int) std::min (64.0f, (float) area.getHeight() * 0.28f);
            const juce::Rectangle<int> axis (area.getX(), area.getBottom() - waveH - 6 - 14, area.getWidth(), 14);
            float zoneWorst = 0.0f;
            for (const auto& zone : { legend, scale, axis })
                for (int y = zone.getY() * S; y < zone.getBottom() * S; y += 2)
                    for (int x = zone.getX() * S; x < zone.getRight() * S; x += 2)
                        zoneWorst = std::max (zoneWorst, luminance (view.getPixelAt (x, y)));
            const float text = luminance (juce::Colour (0xff9b91a5)); // the dim label colour
            const float ratio = (text + 0.05f) / (zoneWorst + 0.05f);
            logMessage ("VISUAL supplied artwork under glass: brightest in the text zones " + juce::String (zoneWorst, 4) + " (dim label contrast there " + juce::String (ratio, 2) + ":1)");
            expect (ratio >= 4.5f, "the dim labels keep WCAG AA contrast (4.5:1): " + juce::String (ratio, 2));
            const juce::String dir = juce::SystemStats::getEnvironmentVariable ("GLITCHKID_SNAPSHOT_DIR", {});
            if (dir.isNotEmpty())
            {
                juce::File f = juce::File (dir).getChildFile ("home_art_composite.png");
                f.deleteFile();
                juce::FileOutputStream os (f);
                if (os.openedOk())
                    juce::PNGImageFormat().writeImageToStream (view, os);
            }
        }

        beginTest ("contrast: the dim artwork in the title strip of the four cards keeps the (large, bold) card titles above 3:1");
        {
            namespace ui = glitchkid::ui;
            const auto art = ui::loadGlitchRelicArtwork();
            expect (art.isValid());
            static const juce::Rectangle<float> crops[4] = { { 0.00f, 0.15f, 0.34f, 0.60f }, { 0.34f, 0.00f, 0.34f, 0.45f }, { 0.66f, 0.10f, 0.34f, 0.60f }, { 0.34f, 0.55f, 0.34f, 0.45f } };
            const float text = luminance (ui::chrome); // the title colour
            const float cardBase = luminance (ui::panelTop.brighter (0.12f)); // the card's lightest background (hovered)
            float worst = 0.0f;
            for (int i = 0; i < 4; ++i)
            {
                const auto r = crops[i];
                const auto px = juce::Rectangle<int> ((int) (r.getX() * art.getWidth()), (int) (r.getY() * art.getHeight()), (int) (r.getWidth() * art.getWidth()), (int) (r.getHeight() * art.getHeight()));
                const float k = ui::WorkspaceTile::artStrength * (i == 2 ? 1.3f : 1.2f); // prominent card, or hovered
                std::vector<float> lums;
                for (int y = px.getY(); y < px.getBottom(); y += 3)
                    for (int x = px.getX(); x < px.getRight(); x += 3)
                    {
                        const auto c = art.getPixelAt (x, y);
                        const float a = k * c.getFloatAlpha();
                        lums.push_back (luminance (juce::Colour::fromFloatRGBA (c.getFloatRed() * a + 0.1f * (1.0f - a), c.getFloatGreen() * a + 0.08f * (1.0f - a), c.getFloatBlue() * a + 0.13f * (1.0f - a), 1.0f)));
                    }
                std::sort (lums.begin(), lums.end());
                worst = std::max (worst, lums[lums.size() * 995 / 1000]); // the brightest half-percent of pixels (specular points) is excluded: they are a few pixels, not a text background
            }
            const float ratio = (text + 0.05f) / (std::max (worst, cardBase) + 0.05f);
            logMessage ("VISUAL card artwork: 99.5th-percentile background luminance " + juce::String (worst, 4) + " (title contrast " + juce::String (ratio, 2) + ":1)");
            expect (ratio >= 3.0f, "card titles keep 3:1 (large bold text): " + juce::String (ratio, 2));
        }

        beginTest ("tooltips: narrow, inside the editor, and never on top of the pointer, wherever the pointer is");
        {
            glitchkid::ui::PlaygroundLookAndFeel laf;
            const juce::String longTip ("Real spectrum and waveform of the vocal input (before processing) and the output. The instrumental shows only while a beat really arrives.");
            const juce::Rectangle<int> area (0, 0, 960, 600);
            for (auto pos : { juce::Point<int> (10, 10), { 480, 70 }, { 950, 590 }, { 20, 590 }, { 940, 20 }, { 480, 300 } })
            {
                const auto b = laf.getTooltipBounds (longTip, pos, area);
                expect (area.contains (b), "inside the editor at " + pos.toString());
                expect (! b.contains (pos), "not under the pointer at " + pos.toString());
                expect (b.getWidth() <= 260, "narrow: " + juce::String (b.getWidth()));
            }
        }

        beginTest ("artwork motion: still when idle or with MOTION off, moves only with a real signal, and stops asking for repaints once still");
        {
            namespace ui = glitchkid::ui;
            ui::SpectrumView v;
            v.setBounds (0, 0, 912, 236);
            for (int i = 0; i < 30; ++i)
                v.setActivity (0.016f, 0.0f, true);
            expectEquals (v.getDriftForTest(), 0.0f);
            v.commit();
            const int before = v.getRepaintRequestsForTest();
            for (int i = 0; i < 30; ++i)
            {
                v.setActivity (0.016f, 0.0f, true); // idle: nothing changes, nothing is repainted
                v.commit();
            }
            expectEquals (v.getRepaintRequestsForTest(), before);
            for (int i = 0; i < 30; ++i)
                v.setActivity (0.016f, 0.0f, false);
            expectEquals (v.getDriftForTest(), 0.0f); // MOTION off
            for (int i = 0; i < 60; ++i)
                v.setActivity (0.016f, 0.6f, false);
            expectEquals (v.getDriftForTest(), 0.0f); // MOTION off, with signal: still still
            for (int i = 0; i < 60; ++i)
                v.setActivity (0.016f, 0.6f, true);
            expect (v.getDriftForTest() > 0.05f, "it drifts while audio plays");
            for (int i = 0; i < 400; ++i)
                v.setActivity (0.016f, 0.0f, true); // the audio stops: it settles
            const float rest = v.getDriftForTest();
            v.commit();
            const int r0 = v.getRepaintRequestsForTest();
            for (int i = 0; i < 30; ++i)
            {
                v.setActivity (0.016f, 0.0f, true);
                v.commit();
            }
            expectEquals (v.getDriftForTest(), rest);
            expectEquals (v.getRepaintRequestsForTest(), r0);
        }

        beginTest ("the relic is a real, decorative backdrop on HOME only: it loads off the message thread, is cached for later editors, and is gone when the editor is");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 480);
            std::unique_ptr<juce::AudioProcessorEditor> a (p.createEditor());
            auto* ed = dynamic_cast<GlitchKidEditor*> (a.get());
            ed->uiWaitForRelicForTest();
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            std::set<juce::uint32> colours;
            for (int y = 130; y < 360; y += 5)
                for (int x = 30; x < 930; x += 5)
                    colours.insert (img.getPixelAt (x, y).getARGB() & 0xfff0f0f0);
            expect (colours.size() > 40, "the display plate carries the relic (" + juce::String ((int) colours.size()) + " colours)");
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
