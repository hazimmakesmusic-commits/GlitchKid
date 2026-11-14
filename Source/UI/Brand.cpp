#include "Brand.h"
#include "PlaygroundWidgets.h"
#include "Theme.h"
#include "BinaryData.h"

namespace glitchkid::ui::brand
{
namespace
{
// the mark's outline: the "d" of the SVG's single path, parsed exactly (no rendering)
juce::Path load (const char* data, int size)
{
    if (auto xml = juce::XmlDocument::parse (juce::String::fromUTF8 (data, size)))
        if (auto* path = xml->getChildByName ("path"))
        {
            auto p = juce::Drawable::parseSVGPath (path->getStringAttribute ("d"));
            p.setUsingNonZeroWinding (true);
            return p;
        }
    return {};
}

// brushed chrome: a bright top, a dark horizon a little below the middle, a lit lower band, then graphite (the same stops as the SVG assets)
juce::ColourGradient chrome (juce::Rectangle<float> r, float alpha)
{
    juce::ColourGradient g (juce::Colour (0xfff7f8fa).withMultipliedAlpha (alpha), r.getX(), r.getY(), juce::Colour (0xff6e747e).withMultipliedAlpha (alpha),
                            r.getX() + r.getHeight() * 0.25f, r.getBottom(), false);
    g.addColour (0.38, juce::Colour (0xffc3c8d0).withMultipliedAlpha (alpha));
    g.addColour (0.50, juce::Colour (0xff8a909a).withMultipliedAlpha (alpha));
    g.addColour (0.62, juce::Colour (0xffdde1e6).withMultipliedAlpha (alpha));
    return g;
}

void drawMark (juce::Graphics& g, const juce::Path& source, juce::Rectangle<float> area, juce::Justification j, float alpha, float edge)
{
    if (source.isEmpty())
        return;
    const auto target = fit (source, area, j);
    juce::Path p (source);
    p.applyTransform (juce::AffineTransform::fromTargetPoints (source.getBounds().getTopLeft(), target.getTopLeft(),
                                                               source.getBounds().getTopRight(), target.getTopRight(),
                                                               source.getBounds().getBottomLeft(), target.getBottomLeft()));
    g.setGradientFill (chrome (target, alpha));
    g.fillPath (p);
    // the profile's accent: a fine gradient edge along the outline (the "logo accent" of each colour profile)
    g.setGradientFill (theme::primaryGradient (target.getTopLeft(), target.getBottomRight(), 0.55f * alpha));
    g.strokePath (p, juce::PathStrokeType (edge));
}
}

const juce::Path& emblem()
{
    static const juce::Path p = load (BinaryData::glitchkidemblemwhite_svg, BinaryData::glitchkidemblemwhite_svgSize);
    return p;
}

const juce::Path& wordmark()
{
    static const juce::Path p = load (BinaryData::glitchkidwordmarkwhite_svg, BinaryData::glitchkidwordmarkwhite_svgSize);
    return p;
}

juce::Rectangle<float> fit (const juce::Path& p, juce::Rectangle<float> area, juce::Justification j)
{
    const auto b = p.getBounds();
    if (b.isEmpty())
        return area;
    const float s = juce::jmin (area.getWidth() / b.getWidth(), area.getHeight() / b.getHeight());
    return j.appliedToRectangle (juce::Rectangle<float> (b.getWidth() * s, b.getHeight() * s), area);
}

void drawEmblem (juce::Graphics& g, juce::Rectangle<float> area, juce::Justification j, float alpha)
{
    drawMark (g, emblem(), area, j, alpha, juce::jmax (0.6f, area.getHeight() / 60.0f));
}

void drawWordmark (juce::Graphics& g, juce::Rectangle<float> area, juce::Justification j, float alpha)
{
    drawMark (g, wordmark(), area, j, alpha, juce::jmax (0.5f, area.getHeight() / 40.0f));
}

void drawTagline (juce::Graphics& g, juce::Rectangle<float> area, float fontHeight)
{
    auto f = fontOf (fontHeight, true);
    // track the letters out so the line spans the area, like the printed identity
    const float plain = juce::GlyphArrangement::getStringWidth (f, tagline);
    const float spare = area.getWidth() * 0.94f - plain; // a small margin: kerning is added after every letter, the last one included
    const int letters = juce::String (tagline).length();
    if (spare > 0.0f)
        f = f.withExtraKerningFactor (juce::jmin (0.6f, spare / (float) letters / fontHeight));
    g.setFont (f);
    g.setColour (juce::Colour (0xffb9bec7));
    g.drawFittedText (tagline, area.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
}

std::unique_ptr<juce::Drawable> appIcon()
{
    return juce::Drawable::createFromSVGString (juce::String::fromUTF8 (BinaryData::glitchkidicon_svg, BinaryData::glitchkidicon_svgSize));
}
}
