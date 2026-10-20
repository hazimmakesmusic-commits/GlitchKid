#include "PlaygroundWidgets.h"
#include <BinaryData.h>
#include "../Presets/FactoryPresets.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace glitchkid::ui
{
namespace
{
float peakToLevel (float peak) { return juce::jmap (juce::jlimit (-60.0f, 0.0f, juce::Decibels::gainToDecibels (std::isfinite (peak) ? peak : 0.0f, -60.0f)), -60.0f, 0.0f, 0.0f, 1.0f); }
void focusRing (juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    g.setColour (chrome);
    g.drawRoundedRectangle (r.expanded (2.0f), radius + 2.0f, 1.2f);
}
}

juce::Font fontOf (float height, bool bold)
{
    static const juce::Typeface::Ptr regular = juce::Typeface::createSystemTypefaceFor (BinaryData::InterRegular_otf, BinaryData::InterRegular_otfSize);
    static const juce::Typeface::Ptr strong = juce::Typeface::createSystemTypefaceFor (BinaryData::InterBold_otf, BinaryData::InterBold_otfSize);
    const auto& face = bold ? strong : regular;
    if (face != nullptr)
        return juce::Font (juce::FontOptions (face).withHeight (height));
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

void drawDial (juce::Graphics& g, juce::Rectangle<float> dial, float start, float end, float pos, juce::Colour accent, float alpha, bool hover, bool focus)
{
    const float d = juce::jmin (dial.getWidth(), dial.getHeight());
    const float thick = juce::jmax (2.5f, d * 0.055f);
    dial = juce::Rectangle<float> (d, d).withCentre (dial.getCentre()).reduced (thick * 0.5f + 2.0f); // the stroke is drawn on the arc's centre line
    const auto c = dial.getCentre();
    const float rad = dial.getWidth() * 0.5f;
    pos = juce::jlimit (0.0f, 1.0f, pos);
    juce::Path track, value;
    track.addCentredArc (c.x, c.y, rad, rad, 0.0f, start, end, true);
    value.addCentredArc (c.x, c.y, rad, rad, 0.0f, start, start + (end - start) * pos, true);
    g.setColour (border.brighter (0.15f).withAlpha (alpha));
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (pos > 0.002f)
    {
        g.setColour (accent.withAlpha (alpha));
        g.strokePath (value, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    const float capR = rad - thick * 1.8f;
    g.setColour (juce::Colour (0xff1b1e28).withAlpha (alpha));
    g.fillEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);
    g.setColour ((hover ? chrome.withAlpha (0.55f) : border.brighter (0.3f)).withAlpha (alpha));
    g.drawEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f, 1.0f);
    const float a = start + (end - start) * pos;
    g.setColour (chrome.withAlpha (alpha));
    g.drawLine (c.x + std::sin (a) * capR * 0.35f, c.y - std::cos (a) * capR * 0.35f, c.x + std::sin (a) * capR * 0.9f, c.y - std::cos (a) * capR * 0.9f, 2.0f);
    if (focus)
        focusRing (g, dial, rad);
}

// ---- ProfileCard -----------------------------------------------------------------------------------------------------------------------

void ProfileCard::bind (int p, bool s, bool l)
{
    if (p != profile || s != selected || l != live)
    {
        profile = p;
        selected = s;
        live = l;
        setVisible (profile >= 0);
        repaint();
    }
}

void ProfileCard::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    if (profile < 0)
        return;
    const auto& info = FactoryPresets::info (profile);
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (selected ? violet.withAlpha (0.18f) : (highlighted || down ? panelTop.brighter (0.08f) : panelTop));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (selected ? violet : (highlighted ? border.brighter (0.4f) : border));
    g.drawRoundedRectangle (r, 5.0f, selected ? 1.6f : 1.0f);
    if (hasKeyboardFocus (true))
        focusRing (g, r, 5.0f);
    auto area = r.reduced (12.0f, 6.0f);
    g.setColour (textBright);
    g.setFont (fontOf (13.5f, true));
    g.drawFittedText (info.name, area.removeFromTop (area.getHeight() * 0.5f).toNearestInt(), juce::Justification::bottomLeft, 1, 0.85f);
    g.setColour (textDim);
    g.setFont (fontOf (11.5f));
    g.drawFittedText (info.tagline, area.toNearestInt(), juce::Justification::topLeft, 1, 0.85f);
    if (live) // the profile that is loaded right now
    {
        g.setColour (lime);
        g.fillEllipse (r.getRight() - 14.0f, r.getY() + 8.0f, 6.0f, 6.0f);
    }
}

// ---- NoteKey ---------------------------------------------------------------------------------------------------------------------------

bool NoteKey::tick (float dt)
{
    if (pulseT <= 0.0f)
        return false;
    pulseT = juce::jmax (0.0f, pulseT - dt * 3.2f);
    repaint();
    return pulseT > 0.0f;
}

void NoteKey::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const bool sharp = getButtonText().contains ("#");
    const auto base = selected ? violet : (sharp ? juce::Colour (0xff0e1016) : juce::Colour (0xff1a1d27));
    g.setColour (down || highlighted ? base.brighter (0.15f) : base);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (selected ? neon : (highlighted ? border.brighter (0.4f) : border));
    g.drawRoundedRectangle (r, 5.0f, selected ? 1.5f : 1.0f);
    if (pulseT > 0.0f)
    {
        g.setColour (neon.withAlpha (pulseT * 0.8f));
        g.drawRoundedRectangle (r.expanded ((1.0f - pulseT) * 5.0f), 7.0f, 1.2f);
    }
    if (hasKeyboardFocus (true))
        focusRing (g, r, 5.0f);
    g.setColour (selected ? chrome : textBright);
    g.setFont (fontOf (16.0f, true));
    g.drawText (getButtonText(), r.withTrimmedBottom (10.0f), juce::Justification::centred, false);
    if (inScale) // this note is allowed by the chosen scale
    {
        g.setColour (selected ? chrome : neon);
        g.fillEllipse (r.getCentreX() - 2.5f, r.getBottom() - 11.0f, 5.0f, 5.0f);
    }
}

// ---- ScopeStrip ------------------------------------------------------------------------------------------------------------------------

void ScopeStrip::push (float peak)
{
    bars[(size_t) head] = peakToLevel (peak);
    head = (head + 1) % numBars;
    repaint();
}

void ScopeStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0e1016));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (border);
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    g.setColour (border.withAlpha (0.7f));
    g.fillRect (r.getX() + 4.0f, r.getCentreY(), r.getWidth() - 8.0f, 1.0f);
    const float step = (r.getWidth() - 8.0f) / (float) numBars;
    for (int i = 0; i < numBars; ++i)
    {
        const float v = bars[(size_t) ((head + i) % numBars)]; // oldest on the left, newest on the right
        const float h = juce::jmax (1.5f, v * (r.getHeight() - 8.0f));
        g.setColour (cyan.withAlpha (0.25f + 0.65f * v));
        g.fillRect (r.getX() + 4.0f + (float) i * step, r.getCentreY() - h * 0.5f, juce::jmax (1.2f, step - 1.0f), h);
    }
}

// ---- BuildButton -----------------------------------------------------------------------------------------------------------------------

void BuildButton::setState (AutoState s, float p)
{
    p = juce::jlimit (0.0f, 1.0f, std::isfinite (p) ? p : 0.0f);
    if (s != state || std::abs (p - progress) > 0.002f)
    {
        state = s;
        progress = p;
        repaint();
    }
}

void BuildButton::tick (float dt)
{
    if (motion && state == AutoState::waiting)
    {
        sweep = std::fmod (sweep + dt * 0.8f, 1.0f);
        repaint();
    }
}

void BuildButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const float alpha = isEnabled() ? 1.0f : 0.45f;
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const auto dark = juce::Colour (0xff141a0a);
    const bool solid = state == AutoState::idle || state == AutoState::ready;
    juce::Colour textCol = dark;
    if (solid)
    {
        g.setColour ((down ? lime.darker (0.2f) : highlighted ? lime.brighter (0.15f) : lime).withAlpha (alpha));
        g.fillRoundedRectangle (r, 8.0f);
    }
    else
    {
        g.setColour (juce::Colour (0xff171d0e).withAlpha (alpha));
        g.fillRoundedRectangle (r, 8.0f);
        const auto edge = state == AutoState::error ? pink : lime;
        if (state == AutoState::working)
        {
            g.setColour (lime.withAlpha (0.9f * alpha));
            g.fillRoundedRectangle (r.withWidth (r.getWidth() * juce::jmax (0.02f, progress)), 8.0f);
            textCol = dark;
        }
        else if (state == AutoState::waiting)
        {
            const float cx = r.getX() + r.getWidth() * (motion ? sweep : 0.5f);
            g.setColour (lime.withAlpha (0.28f * alpha));
            g.fillRoundedRectangle (juce::Rectangle<float> (r.getWidth() * 0.22f, r.getHeight()).withCentre ({ cx, r.getCentreY() }).getIntersection (r), 8.0f);
            textCol = lime;
        }
        else
            textCol = pink;
        g.setColour (edge.withAlpha (alpha));
        g.drawRoundedRectangle (r, 8.0f, 1.5f);
    }
    if (hasKeyboardFocus (false))
        focusRing (g, r, 8.0f);
    g.setFont (fontOf (juce::jlimit (14.0f, 24.0f, r.getHeight() * 0.3f), true));
    if (state == AutoState::working && progress < 0.98f)
    {
        // dark text over the lime part, lime text over the dark part
        const int split = (int) (r.getX() + r.getWidth() * juce::jmax (0.02f, progress));
        {
            const juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (0, 0, split, getHeight());
            g.setColour (dark);
            g.drawText (getButtonText(), r, juce::Justification::centred, false);
        }
        const juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (split, 0, getWidth() - split, getHeight());
        g.setColour (lime);
        g.drawText (getButtonText(), r, juce::Justification::centred, false);
    }
    else
    {
        g.setColour (textCol.withAlpha (alpha));
        g.drawText (getButtonText(), r, juce::Justification::centred, false);
    }
}

// ---- ChainStrip ------------------------------------------------------------------------------------------------------------------------

namespace
{
const char* const nodeNames[6] = { "INPUT", "TUNE", "TONE", "DYNAMICS", "SPACE", "OUTPUT" };
}

juce::Rectangle<int> ChainStrip::nodeBounds (int node) const
{
    const int gap = 30, w = (getWidth() - 5 * gap) / 6;
    return { node * (w + gap), 0, w, getHeight() - 14 }; // the strip below is room for the hidden-effects note
}

int ChainStrip::nodeAt (juce::Point<int> p) const
{
    for (int i = 0; i < 6; ++i)
        if (nodeBounds (i).contains (p))
            return i;
    return -1;
}

void ChainStrip::setState (const std::array<bool, numStages>& a, float inPeak, float outPeak, int sel, bool m, const juce::String& legacyNote)
{
    // levels fall smoothly (about 45 dB/s) so the bar is readable at 30 Hz
    auto fall = [] (float shown, float peak) { const float t = peakToLevel (peak); return t > shown ? t : juce::jmax (t, shown - 0.025f); };
    const float newIn = fall (inLevel, inPeak), newOut = fall (outLevel, outPeak);
    const bool changed = a != active || sel != selected || m != motion || legacyNote != legacy || std::abs (newIn - inLevel) > 0.004f || std::abs (newOut - outLevel) > 0.004f;
    active = a;
    selected = sel;
    motion = m;
    legacy = legacyNote;
    inLevel = newIn;
    outLevel = newOut;
    signalPresent = inLevel > 0.05f;
    setTooltip (legacy.isNotEmpty() ? "Hidden effects from an older session are still switched on: " + legacy + ". BUILD MY VOCAL MIX switches them off."
                                    : "The real signal path, left to right. A lit dot means the stage is doing something right now. Click a stage to open it in MODIFY.");
    if (changed)
        repaint();
}

void ChainStrip::tick (float dt)
{
    if (motion && signalPresent)
    {
        phase = std::fmod (phase + dt * 0.7f, 1.0f);
        repaint();
    }
}

void ChainStrip::mouseMove (const juce::MouseEvent& e)
{
    const int n = nodeAt (e.getPosition());
    setMouseCursor (n >= 1 && n <= numStages ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void ChainStrip::mouseUp (const juce::MouseEvent& e)
{
    const int n = nodeAt (e.getPosition());
    if (n >= 1 && n <= numStages && onStage != nullptr && e.getDistanceFromDragStart() < 5)
        onStage (n - 1);
}

void ChainStrip::paint (juce::Graphics& g)
{
    // connecting line with arrow heads
    const float cy = (float) (getHeight() - 14) * 0.5f;
    g.setColour (border.brighter (0.2f));
    for (int i = 0; i < 5; ++i)
    {
        const auto a = nodeBounds (i), b = nodeBounds (i + 1);
        const float x0 = (float) a.getRight() + 3.0f, x1 = (float) b.getX() - 3.0f;
        g.drawLine (x0, cy, x1, cy, 1.0f);
        g.drawLine (x1 - 4.0f, cy - 3.5f, x1, cy, 1.0f);
        g.drawLine (x1 - 4.0f, cy + 3.5f, x1, cy, 1.0f);
    }
    if (motion && signalPresent) // a real signal is arriving: one small dot travels the path
    {
        const float x0 = (float) nodeBounds (0).getCentreX(), x1 = (float) nodeBounds (5).getCentreX();
        g.setColour (cyan.withAlpha (0.85f));
        g.fillEllipse (x0 + (x1 - x0) * phase - 2.5f, cy - 2.5f, 5.0f, 5.0f);
    }
    for (int i = 0; i < 6; ++i)
    {
        const auto r = nodeBounds (i).toFloat();
        const bool stage = i >= 1 && i <= numStages;
        const bool on = stage && active[(size_t) (i - 1)];
        const bool sel = stage && selected == i - 1;
        g.setColour (sel ? violet.withAlpha (0.2f) : panelTop);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (sel ? violet : border);
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, sel ? 1.5f : 1.0f);
        g.setFont (fontOf (11.5f, true));
        g.setColour (stage && ! on ? textDim : textBright);
        if (stage)
        {
            g.drawText (nodeNames[i], r.withTrimmedLeft (22.0f), juce::Justification::centredLeft, false);
            const auto dot = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ r.getX() + 12.0f, r.getCentreY() });
            if (on)
            {
                g.setColour (lime);
                g.fillEllipse (dot);
            }
            else
            {
                g.setColour (textDim.withAlpha (0.7f));
                g.drawEllipse (dot.reduced (0.5f), 1.0f);
            }
        }
        else
        {
            g.drawText (nodeNames[i], r.withTrimmedLeft (10.0f).withTrimmedBottom (10.0f), juce::Justification::centredLeft, false);
            const float level = i == 0 ? inLevel : outLevel;
            const auto bar = juce::Rectangle<float> (r.getX() + 10.0f, r.getBottom() - 11.0f, r.getWidth() - 20.0f, 4.0f);
            g.setColour (juce::Colour (0xff0e1016));
            g.fillRoundedRectangle (bar, 2.0f);
            g.setColour (level > 0.98f ? errorRed : cyan);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * level), 2.0f);
        }
    }
    if (legacy.isNotEmpty())
    {
        g.setColour (pink);
        g.setFont (fontOf (10.5f, true));
        g.drawText ("OLDER-SESSION EFFECTS STILL ON: " + legacy, juce::Rectangle<float> (0.0f, (float) getHeight() - 13.0f, (float) getWidth(), 13.0f), juce::Justification::centredRight, false);
    }
}

// ---- PlaygroundLookAndFeel -------------------------------------------------------------------------------------------------------------

PlaygroundLookAndFeel::PlaygroundLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, obsidian);
    setColour (juce::Label::textColourId, textBright);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1a1d27));
    setColour (juce::TextButton::buttonOnColourId, violet);
    setColour (juce::TextButton::textColourOffId, textBright);
    setColour (juce::TextButton::textColourOnId, chrome);
    setColour (juce::Slider::textBoxTextColourId, textBright);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff0e1016));
    setColour (juce::Slider::textBoxOutlineColourId, border);
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff0e1016));
    setColour (juce::ComboBox::outlineColourId, border.brighter (0.2f));
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff14161d));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, violet);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff1a1d27));
    setColour (juce::TooltipWindow::textColourId, textBright);
    setColour (juce::TooltipWindow::outlineColourId, border.brighter (0.3f));
}

juce::Font PlaygroundLookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return fontOf (juce::jlimit (12.0f, 16.0f, (float) h * 0.4f), true); }
juce::Font PlaygroundLookAndFeel::getLabelFont (juce::Label& l) { return fontOf (l.getFont().getHeight(), l.getFont().isBold()); }
juce::Font PlaygroundLookAndFeel::getComboBoxFont (juce::ComboBox& b) { return fontOf (juce::jlimit (12.0f, 15.0f, (float) b.getHeight() * 0.4f), false); }
juce::Font PlaygroundLookAndFeel::getPopupMenuFont() { return fontOf (14.0f, false); }

void PlaygroundLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const float alpha = b.isEnabled() ? 1.0f : 0.4f;
    auto fill = on ? b.findColour (juce::TextButton::buttonOnColourId) : bg;
    fill = down ? fill.darker (0.15f) : highlighted ? fill.brighter (0.12f) : fill;
    g.setColour (fill.withAlpha (alpha));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour ((on ? fill.brighter (0.3f) : border.brighter (highlighted ? 0.35f : 0.1f)).withAlpha (alpha));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    if (b.hasKeyboardFocus (true))
        focusRing (g, r, 5.0f);
}

void PlaygroundLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const float alpha = b.isEnabled() ? 1.0f : 0.4f;
    const bool on = b.getToggleState();
    const auto sw = juce::Rectangle<float> (34.0f, 18.0f).withCentre ({ 20.0f, (float) b.getHeight() * 0.5f });
    g.setColour ((on ? lime.withAlpha (0.2f) : juce::Colour (0xff0e1016)).withAlpha (alpha));
    g.fillRoundedRectangle (sw, 9.0f);
    g.setColour ((on ? lime : (highlighted ? border.brighter (0.5f) : border.brighter (0.2f))).withAlpha (alpha));
    g.drawRoundedRectangle (sw, 9.0f, 1.2f);
    g.setColour ((on ? lime : disabled).withAlpha (alpha));
    g.fillEllipse (on ? sw.getRight() - 15.0f : sw.getX() + 3.0f, sw.getY() + 2.0f, 14.0f, 14.0f);
    g.setColour ((on ? textBright : textDim).withAlpha (alpha));
    g.setFont (fontOf (juce::jlimit (12.0f, 14.0f, (float) b.getHeight() * 0.4f), true));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().withTrimmedLeft (46), juce::Justification::centredLeft, 1, 0.8f);
    if (b.hasKeyboardFocus (true))
        focusRing (g, sw, 9.0f);
}

void PlaygroundLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider& s)
{
    drawDial (g, juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h), startAngle, endAngle, pos,
              s.findColour (juce::Slider::rotarySliderFillColourId), s.isEnabled() ? 1.0f : 0.4f, s.isMouseOverOrDragging(), s.hasKeyboardFocus (false));
}

void PlaygroundLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float minPos, float maxPos, juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, minPos, maxPos, style, s);
        return;
    }
    const auto accent = s.findColour (juce::Slider::trackColourId);
    const float alpha = s.isEnabled() ? 1.0f : 0.4f;
    const float cy = (float) y + (float) h * 0.5f;
    g.setColour (border.brighter (0.15f).withAlpha (alpha));
    g.fillRoundedRectangle ((float) x, cy - 2.0f, (float) w, 4.0f, 2.0f);
    g.setColour (accent.withAlpha (alpha));
    g.fillRoundedRectangle ((float) x, cy - 2.0f, pos - (float) x, 4.0f, 2.0f);
    g.setColour (chrome.withAlpha (alpha));
    g.fillEllipse (pos - 7.0f, cy - 7.0f, 14.0f, 14.0f);
    g.setColour (accent.withAlpha (alpha));
    g.drawEllipse (pos - 7.0f, cy - 7.0f, 14.0f, 14.0f, 1.5f);
    if (s.hasKeyboardFocus (false))
        focusRing (g, { pos - 7.0f, cy - 7.0f, 14.0f, 14.0f }, 7.0f);
}
}
