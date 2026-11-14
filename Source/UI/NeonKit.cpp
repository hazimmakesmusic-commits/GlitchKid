#include "NeonKit.h"
#include "PlaygroundWidgets.h"

namespace glitchkid::ui::nk
{
// ---- the dial: the shared GlitchKid dial (PlaygroundWidgets) ----
void Knob::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    drawDial (g, juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h), start, end, pos, pinkNeon, s.isEnabled() ? 1.0f : 0.4f,
              s.isMouseOverOrDragging(), s.hasKeyboardFocus (false));
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
