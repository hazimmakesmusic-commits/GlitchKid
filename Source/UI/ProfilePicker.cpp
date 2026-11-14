#include "ProfilePicker.h"
#include "PlaygroundWidgets.h"

namespace glitchkid::ui
{
// A preview of a profile: two soft silk ribbons in its gradient over the near-black tile, the name below. Static artwork (no audio is implied).
void paintProfileTile (juce::Graphics& g, juce::Rectangle<float> r, int index, bool selected, bool hover)
{
    const auto& p = theme::profile (index);
    auto art = r.withTrimmedBottom (juce::jmin (22.0f, r.getHeight() * 0.24f));
    {
        juce::Graphics::ScopedSaveState s (g);
        juce::Path clip;
        clip.addRoundedRectangle (art, 9.0f);
        g.reduceClipRegion (clip);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff17151e), art.getX(), art.getY(), juce::Colour (0xff08070b), art.getX(), art.getBottom(), false));
        g.fillRect (art);
        for (int k = 0; k < 2; ++k) // two ribbons: an S-curve band across the tile, the second fainter and offset
        {
            const float off = (float) k * art.getHeight() * 0.16f, w = art.getWidth(), h = art.getHeight();
            juce::Path band;
            band.startNewSubPath (art.getX() - 4.0f, art.getY() + h * (0.78f - 0.1f * (float) k) + off);
            band.cubicTo (art.getX() + w * 0.35f, art.getY() + h * 0.95f + off, art.getX() + w * 0.55f, art.getY() + h * 0.10f + off, art.getRight() + 4.0f, art.getY() + h * (0.30f + 0.08f * (float) k) + off);
            band.lineTo (art.getRight() + 4.0f, art.getY() + h * (0.44f + 0.08f * (float) k) + off);
            band.cubicTo (art.getX() + w * 0.58f, art.getY() + h * 0.30f + off, art.getX() + w * 0.40f, art.getY() + h * 1.05f + off, art.getX() - 4.0f, art.getY() + h * (0.92f - 0.1f * (float) k) + off);
            band.closeSubPath();
            juce::ColourGradient grad (p.secondary.withAlpha (k == 0 ? 0.95f : 0.45f), art.getX(), art.getBottom(), p.deep.withAlpha (0.2f), art.getRight(), art.getY(), false);
            grad.addColour (0.45, p.primary.withAlpha (k == 0 ? 0.9f : 0.4f));
            g.setGradientFill (grad);
            g.fillPath (band);
            g.setColour (p.soft.withAlpha (k == 0 ? 0.55f : 0.25f)); // a fine bright edge: the silk's sheen
            g.strokePath (band, juce::PathStrokeType (0.8f));
        }
    }
    g.setColour (selected ? p.secondary : hover ? border.brighter (0.4f) : border);
    g.drawRoundedRectangle (art.reduced (0.5f), 9.0f, selected ? 1.6f : 1.0f);
    g.setFont (fontOf (10.5f, true).withExtraKerningFactor (0.18f));
    g.setColour (selected ? textBright : textDim);
    g.drawText (p.name, r.withTop (art.getBottom() + 4.0f), juce::Justification::centredTop, false);
}

// ---- the swatch in the top bar ----
ProfileSwatch::ProfileSwatch() : juce::Button ("Colour profile")
{
    setTooltip ("COLOUR PROFILE: the interface's accent colours (VIRIDIAN, MAGENTA, SAPPHIRE, AMBER, VOID). Visual only: the sound, presets and your session never change.");
    setWantsKeyboardFocus (true);
    theme::changes().addChangeListener (this);
}

ProfileSwatch::~ProfileSwatch() { theme::changes().removeChangeListener (this); }

void ProfileSwatch::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto r = getLocalBounds().toFloat().reduced (3.0f);
    const auto c = juce::Rectangle<float> (juce::jmin (r.getWidth(), r.getHeight()), juce::jmin (r.getWidth(), r.getHeight())).withCentre (r.getCentre());
    g.setGradientFill (theme::primaryGradient (c.getTopLeft(), c.getBottomRight()));
    g.fillEllipse (c.reduced (down ? 1.5f : 0.0f));
    g.setColour (juce::Colours::white.withAlpha (over ? 0.55f : 0.25f));
    g.drawEllipse (c.reduced (0.5f), 1.0f);
    if (hasKeyboardFocus (false))
    {
        g.setColour (neon);
        g.drawEllipse (c.expanded (2.0f), 1.0f);
    }
}

// ---- the picker ----
ProfilePicker::ProfilePicker()
{
    setSize (pad + theme::numProfiles * (tileW + pad), 30 + tileH + pad);
    setTitle ("Colour profiles");
    theme::changes().addChangeListener (this);
}

ProfilePicker::~ProfilePicker() { theme::changes().removeChangeListener (this); }

juce::Rectangle<float> ProfilePicker::tile (int i) const { return { (float) (pad + i * (tileW + pad)), 30.0f, (float) tileW, (float) tileH }; }

int ProfilePicker::tileAt (juce::Point<int> pt) const
{
    for (int i = 0; i < theme::numProfiles; ++i)
        if (tile (i).contains (pt.toFloat()))
            return i;
    return -1;
}

void ProfilePicker::paint (juce::Graphics& g)
{
    g.setFont (fontOf (10.5f, true).withExtraKerningFactor (0.25f));
    g.setColour (textDim);
    g.drawText ("COLOUR PROFILES", pad, 8, getWidth() - 2 * pad, 16, juce::Justification::centredLeft, false);
    for (int i = 0; i < theme::numProfiles; ++i)
        paintProfileTile (g, tile (i), i, i == theme::current(), i == hover);
}

void ProfilePicker::mouseMove (const juce::MouseEvent& e)
{
    const int h = tileAt (e.getPosition());
    if (h != hover)
    {
        hover = h;
        repaint();
    }
}

void ProfilePicker::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

void ProfilePicker::mouseUp (const juce::MouseEvent& e)
{
    if (const int i = tileAt (e.getPosition()); i >= 0)
        choose (i);
}

void ProfilePicker::choose (int i)
{
    theme::apply (i);
    theme::save (i);
    repaint();
}
}
