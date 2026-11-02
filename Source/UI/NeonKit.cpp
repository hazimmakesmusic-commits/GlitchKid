#include "NeonKit.h"
#include "PlaygroundWidgets.h"

namespace glitchkid::ui::nk
{
// ---- the dial: a tactile dark body with a neon value ring ----
void Knob::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const float size = (float) juce::jmin (w, h), r = size * 0.5f - 7.0f;
    const juce::Point<float> c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f);
    const bool hot = s.isMouseOverOrDragging() || s.hasKeyboardFocus (false);
    const float angle = start + pos * (end - start);

    juce::Path track;
    track.addCentredArc (c.x, c.y, r, r, 0.0f, start, end, true);
    g.setColour (hairline);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (pos > 0.002f)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, r, r, 0.0f, start, angle, true);
        for (auto [width, alpha] : { std::pair { 10.0f, 0.08f }, std::pair { 6.0f, 0.16f } }) // a soft neon halo
        {
            g.setColour (uv.withAlpha (alpha * (hot ? 1.6f : 1.0f)));
            g.strokePath (arc, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (hot ? uvSoft : uv);
        g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // the body: a dark machined cap, lit from above
    const float br = r * 0.74f;
    const auto body = juce::Rectangle<float> (2.0f * br, 2.0f * br).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (body.translated (0.0f, 3.0f).expanded (1.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2e2a38), c.x, body.getY(), juce::Colour (0xff111016), c.x, body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (juce::Colours::white.withAlpha (0.09f));
    g.drawEllipse (body.reduced (0.5f), 1.0f);
    const auto cap = body.reduced (br * 0.22f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1820), c.x, cap.getY(), juce::Colour (0xff23202b), c.x, cap.getBottom(), false));
    g.fillEllipse (cap);
    // the pointer
    const juce::Point<float> a (c.x + std::sin (angle) * br * 0.38f, c.y - std::cos (angle) * br * 0.38f), b (c.x + std::sin (angle) * br * 0.84f, c.y - std::cos (angle) * br * 0.84f);
    g.setColour (s.isEnabled() ? white : faint);
    g.drawLine ({ a, b }, 2.4f);
}

void Pill::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const float rad = r.getHeight() * 0.5f;
    const bool on = getToggleState();
    const juce::Colour accent = getProperties()["secondary"] ? pinkNeon : uv;
    if (on)
    {
        g.setColour (accent.withAlpha (0.2f));
        g.fillRoundedRectangle (r, rad);
        g.setColour (accent);
        g.drawRoundedRectangle (r, rad, 1.2f);
    }
    else
    {
        g.setColour (down ? surfaceHi.brighter (0.1f) : over ? surfaceHi : surface);
        g.fillRoundedRectangle (r, rad);
        g.setColour (hairline);
        g.drawRoundedRectangle (r, rad, 1.0f);
    }
    g.setColour (on || over ? white : juce::Colour (0xffd6d2de));
    g.setFont (fontOf (13.0f, on));
    g.drawFittedText (getButtonText(), getLocalBounds().reduced (10, 0), juce::Justification::centred, 1, 0.8f);
    if (hasKeyboardFocus (false))
    {
        g.setColour (uvSoft.withAlpha (0.8f));
        g.drawRoundedRectangle (r.expanded (1.0f), rad + 1.0f, 1.0f);
    }
}

}
