#include "NeonKit.h"
#include "PlaygroundWidgets.h"
#include "Theme.h"

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
    if (on) // lit: a faint wash and a gradient edge of the current profile
    {
        g.setGradientFill (theme::primaryGradient (r.getTopRight(), r.getBottomLeft(), getProperties()["secondary"] ? 0.22f : 0.16f));
        g.fillRoundedRectangle (r, rad);
        g.setGradientFill (theme::primaryGradient (r.getTopRight(), r.getBottomLeft()));
        g.drawRoundedRectangle (r, rad, 1.3f);
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
