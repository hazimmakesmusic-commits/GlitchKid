#include "PlaygroundWidgets.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace glitchkid::ui
{
namespace
{
constexpr float pi = 3.14159265f;
float approach (float current, float target, float rate) { return current + (target - current) * rate; }
bool settled (float a, float b) { return std::abs (a - b) < 0.002f; }
}

void drawGlow (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float radius, float strength)
{
    if (strength <= 0.01f)
        return;
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (c.withAlpha (0.07f * strength * (float) (4 - i)));
        g.fillRoundedRectangle (r.expanded (radius * (float) i / 3.0f), radius * 0.8f + 6.0f);
    }
}

void drawGlowCircle (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float radius, float strength)
{
    if (strength <= 0.01f)
        return;
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (c.withAlpha (0.07f * strength * (float) (4 - i)));
        g.fillEllipse (r.expanded (radius * (float) i / 3.0f));
    }
}

// ---- Mascot ----------------------------------------------------------------------------------------------------------------------------

void Mascot::setMood (Mood m) { if (m != mood) { mood = m; repaint(); } }
void Mascot::setLevel (float v) { level = juce::jlimit (0.0f, 1.0f, std::isfinite (v) ? v : 0.0f); }
void Mascot::setLook (float x) { look = juce::jlimit (-1.0f, 1.0f, x); }
void Mascot::cheer() { if (motion) { bounce = 1.0f; bounceTime = 0.0f; } repaint(); }

void Mascot::tick (float dt)
{
    const float targetLevel = std::sqrt (level); // a gentle curve so quiet vocals still move the eyes
    const float prevLevel = shownLevel, prevLook = shownLook, prevBlink = blink, prevBounce = bounce;
    shownLevel = motion ? approach (shownLevel, targetLevel, 0.35f) : targetLevel;
    shownLook = motion ? approach (shownLook, look, 0.2f) : look;
    if (motion)
    {
        clock += dt;
        blinkTimer -= dt;
        if (blinkTimer <= 0.0f) { blink = 1.0f; blinkTimer = 2.5f + 3.0f * std::fmod (clock * 7.31f, 1.0f); }
        blink = juce::jmax (0.0f, blink - dt * 6.0f);
        if (bounce > 0.0f) { bounceTime += dt; bounce = juce::jmax (0.0f, 1.0f - bounceTime / 0.7f); }
    }
    else
        blink = bounce = 0.0f;
    const bool idleSway = motion; // antennae sway slowly: repaint at the timer rate while motion is on
    if (idleSway || ! settled (prevLevel, shownLevel) || ! settled (prevLook, shownLook) || ! settled (prevBlink, blink) || ! settled (prevBounce, bounce))
        repaint();
}

void Mascot::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (2.0f);
    const auto accent = mood == Mood::error ? pink : (mood == Mood::idle ? violet : lime);
    const float hop = motion ? -14.0f * bounce * std::abs (std::sin (bounceTime * pi * 3.0f)) : 0.0f;
    const float squash = 1.0f + (motion ? 0.10f * bounce * std::sin (bounceTime * pi * 6.0f) : 0.0f);

    auto body = juce::Rectangle<float> (b.getWidth() * 0.82f, b.getHeight() * 0.66f).withCentre ({ b.getCentreX(), b.getBottom() - b.getHeight() * 0.36f + hop });
    body = body.withSizeKeepingCentre (body.getWidth() / squash, body.getHeight() * squash);

    drawGlowCircle (g, body, accent, 16.0f, 0.5f + 0.8f * shownLevel + (mood == Mood::ready ? 0.4f : 0.0f));

    // antennae (two), swaying a little
    const float sway = motion ? std::sin (clock * 2.2f) * 3.0f : 0.0f;
    for (int i = 0; i < 2; ++i)
    {
        const float dir = i == 0 ? -1.0f : 1.0f;
        const juce::Point<float> root (body.getCentreX() + dir * body.getWidth() * 0.22f, body.getY() + 4.0f);
        const juce::Point<float> tip (root.x + dir * 9.0f + sway * dir, root.y - 17.0f - 4.0f * shownLevel);
        g.setColour (chromeDark);
        g.drawLine (root.x, root.y, tip.x, tip.y, 2.0f);
        g.setColour (accent.withAlpha (0.35f + 0.65f * (mood == Mood::idle ? 0.5f : 1.0f)));
        g.fillEllipse (tip.x - 4.5f, tip.y - 4.5f, 9.0f, 9.0f);
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.fillEllipse (tip.x - 2.5f, tip.y - 3.0f, 3.0f, 3.0f);
    }

    // liquid-chrome body
    juce::ColourGradient grad (chrome, body.getX(), body.getY(), obsidian, body.getRight(), body.getBottom(), false);
    grad.addColour (0.35, violet.brighter (0.2f));
    grad.addColour (0.7, violet.darker (0.8f));
    g.setGradientFill (grad);
    g.fillEllipse (body);
    g.setColour (accent.withAlpha (0.9f));
    g.drawEllipse (body, 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.28f));
    g.fillEllipse (body.getX() + body.getWidth() * 0.14f, body.getY() + body.getHeight() * 0.08f, body.getWidth() * 0.34f, body.getHeight() * 0.2f);

    // eyes: pupils grow with the real input level and look toward the selected world
    const float eyeR = body.getWidth() * 0.15f;
    const float eyeY = body.getY() + body.getHeight() * 0.42f;
    const float open = mood == Mood::ready && bounce > 0.0f ? 0.35f : 1.0f - blink * 0.9f;
    for (int i = 0; i < 2; ++i)
    {
        const float ex = body.getCentreX() + (i == 0 ? -1.0f : 1.0f) * body.getWidth() * 0.2f;
        auto eye = juce::Rectangle<float> (eyeR * 2.0f, eyeR * 2.0f * open).withCentre ({ ex, eyeY });
        g.setColour (juce::Colours::white);
        g.fillEllipse (eye);
        const float pr = eyeR * (0.42f + 0.38f * shownLevel);
        g.setColour (mood == Mood::error ? pink.darker (0.6f) : obsidian);
        g.fillEllipse (ex + shownLook * eyeR * 0.45f - pr, eyeY - pr * open, pr * 2.0f, pr * 2.0f * open);
    }

    // mouth
    const float mx = body.getCentreX(), my = body.getY() + body.getHeight() * 0.72f, mw = body.getWidth() * 0.2f;
    g.setColour (obsidian);
    if (mood == Mood::listening)
    {
        const float h = 4.0f + 10.0f * shownLevel;
        g.fillEllipse (mx - mw * 0.5f, my - h * 0.5f, mw, h);
    }
    else
    {
        juce::Path mouth;
        if (mood == Mood::error)
        {
            mouth.startNewSubPath (mx - mw, my + 2.0f);
            for (int i = 1; i <= 4; ++i)
                mouth.lineTo (mx - mw + mw * 0.5f * (float) i, my + (i % 2 ? -3.0f : 3.0f));
        }
        else
        {
            const float smile = mood == Mood::ready ? 9.0f : 4.0f;
            mouth.startNewSubPath (mx - mw, my - 1.0f);
            mouth.quadraticTo (mx, my + smile * 2.0f, mx + mw, my - 1.0f);
        }
        g.strokePath (mouth, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

// ---- WorldCard -------------------------------------------------------------------------------------------------------------------------

WorldCard::WorldCard (const juce::String& t, const juce::String& tag, Icon i, juce::Colour c)
    : title (t), tagline (tag), icon (i), accent (c)
{
    setWantsKeyboardFocus (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void WorldCard::setSelected (bool s, bool e)
{
    if (s != selected || e != edited)
    {
        selected = s;
        edited = e;
        repaint();
    }
}

void WorldCard::tick (float)
{
    const float prevHover = hover, prevGlow = glow;
    hover = motion ? approach (hover, hovering ? 1.0f : 0.0f, 0.3f) : (hovering ? 1.0f : 0.0f);
    glow = motion ? approach (glow, selected ? 1.0f : 0.0f, 0.2f) : (selected ? 1.0f : 0.0f);
    if (! settled (prevHover, hover) || ! settled (prevGlow, glow))
        repaint();
}

void WorldCard::mouseUp (const juce::MouseEvent& e)
{
    const bool click = pressed && getLocalBounds().contains (e.getPosition());
    pressed = false;
    repaint();
    if (click && onClick)
        onClick();
}

bool WorldCard::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::returnKey || k == juce::KeyPress::spaceKey)
    {
        if (onClick) onClick();
        return true;
    }
    return false;
}

void WorldCard::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (6.0f);
    if (pressed)
        r = r.reduced (2.0f);
    drawGlow (g, r, accent, 14.0f, 0.9f * glow + 0.5f * hover);

    juce::ColourGradient fill (accent.withMultipliedBrightness (0.38f + 0.18f * hover + 0.15f * glow), r.getX(), r.getY(), panelBottom, r.getX(), r.getBottom(), false);
    g.setGradientFill (fill);
    g.fillRoundedRectangle (r, 16.0f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.42f).reduced (3.0f, 2.0f), 13.0f); // gloss
    g.setColour (accent.withAlpha (0.45f + 0.55f * juce::jmax (glow, hover)));
    g.drawRoundedRectangle (r, 16.0f, selected ? 3.0f : 1.6f);
    if (hasKeyboardFocus (true))
    {
        g.setColour (chrome);
        g.drawRoundedRectangle (r.expanded (3.0f), 18.0f, 1.6f);
    }

    // icon (original, drawn): planet / heart / glitch blocks
    auto ic = juce::Rectangle<float> (46.0f, 46.0f).withCentre ({ r.getRight() - 46.0f, r.getY() + 44.0f });
    g.setColour (accent.brighter (0.4f));
    if (icon == Icon::planet)
    {
        g.fillEllipse (ic.reduced (8.0f));
        g.setColour (chrome.withAlpha (0.9f));
        juce::Path ring;
        ring.addEllipse (ic.getX() - 2.0f, ic.getCentreY() - 6.0f, ic.getWidth() + 4.0f, 12.0f);
        g.strokePath (ring, juce::PathStrokeType (2.0f), juce::AffineTransform::rotation (-0.45f, ic.getCentreX(), ic.getCentreY()));
    }
    else if (icon == Icon::heart)
    {
        juce::Path h;
        const auto c = ic.getCentre();
        h.startNewSubPath (c.x, c.y + 17.0f);
        h.cubicTo (c.x - 30.0f, c.y - 4.0f, c.x - 14.0f, c.y - 26.0f, c.x, c.y - 10.0f);
        h.cubicTo (c.x + 14.0f, c.y - 26.0f, c.x + 30.0f, c.y - 4.0f, c.x, c.y + 17.0f);
        g.fillPath (h);
        g.setColour (obsidian);
        g.drawLine (c.x - 3.0f, c.y - 8.0f, c.x + 2.0f, c.y, 2.0f);   // a crack: it's a broken heart
        g.drawLine (c.x + 2.0f, c.y, c.x - 2.0f, c.y + 9.0f, 2.0f);
    }
    else
    {
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                if ((i + j) % 2 == 0)
                    g.fillRect (ic.getX() + (float) i * 15.0f + (j == 1 ? 3.0f : 0.0f), ic.getY() + (float) j * 15.0f, 13.0f, 13.0f);
    }

    g.setColour (textBright);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText (title, r.getX() + 16.0f, r.getY() + 14.0f, r.getWidth() - 80.0f, 28.0f, juce::Justification::centredLeft);
    g.setColour (textBright.withAlpha (0.82f));
    g.setFont (juce::FontOptions (14.0f));
    g.drawText (tagline, r.getX() + 16.0f, r.getY() + 46.0f, r.getWidth() - 32.0f, 20.0f, juce::Justification::centredLeft);
    g.setColour (accent.brighter (0.5f).withAlpha (0.9f));
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (selected ? (edited ? "LOADED (EDITED)" : "LOADED") : "TAP TO LOAD: " + loads, r.getX() + 16.0f, r.getBottom() - 28.0f, r.getWidth() - 32.0f, 18.0f, juce::Justification::centredLeft);
}

// ---- MacroDial -------------------------------------------------------------------------------------------------------------------------

MacroDial::MacroDial (const juce::String& n, juce::Colour c) : name (n), accent (c)
{
    setWantsKeyboardFocus (true);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void MacroDial::setValue (float v)
{
    if (dragging)
        return;
    value = juce::jlimit (0.0f, 1.0f, std::isfinite (v) ? v : 0.0f);
    if (! motion)
    {
        shown = value;
        repaint();
    }
}

void MacroDial::setEnabledLook (bool en, const juce::String& whyNot)
{
    if (en != isEnabled())
    {
        setEnabled (en);
        repaint();
    }
    setTooltip (en ? juce::String() : whyNot);
}

void MacroDial::tick (float)
{
    const float prevShown = shown, prevHover = hover;
    shown = motion ? approach (shown, value, 0.35f) : value;
    hover = motion ? approach (hover, hovering || dragging ? 1.0f : 0.0f, 0.3f) : (hovering ? 1.0f : 0.0f);
    if (! settled (prevShown, shown) || ! settled (prevHover, hover))
        repaint();
}

void MacroDial::change (float v)
{
    value = juce::jlimit (0.0f, 1.0f, v);
    if (onChange)
        onChange (value);
    repaint();
}

void MacroDial::mouseDown (const juce::MouseEvent&)
{
    dragging = isEnabled();
    dragStartValue = value;
    grabKeyboardFocus();
    repaint();
}

void MacroDial::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging)
        change (dragStartValue - (float) e.getDistanceFromDragStartY() / (e.mods.isShiftDown() ? 800.0f : 180.0f));
}

void MacroDial::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (isEnabled() && ! dragging)
        change (value + (w.deltaY > 0.0f ? 0.02f : (w.deltaY < 0.0f ? -0.02f : 0.0f)));
}

bool MacroDial::keyPressed (const juce::KeyPress& k)
{
    if (! isEnabled())
        return false;
    if (k == juce::KeyPress::upKey || k == juce::KeyPress::rightKey)   { change (value + 0.05f); return true; }
    if (k == juce::KeyPress::downKey || k == juce::KeyPress::leftKey)  { change (value - 0.05f); return true; }
    if (k == juce::KeyPress::homeKey) { change (0.0f); return true; }
    if (k == juce::KeyPress::endKey)  { change (1.0f); return true; }
    return false;
}

void MacroDial::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float alpha = isEnabled() ? 1.0f : 0.4f;
    g.setColour (textBright.withAlpha (alpha));
    g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    g.drawText (name, r.removeFromTop (22.0f), juce::Justification::centred);

    auto valueArea = r.removeFromBottom (24.0f);
    const float d = juce::jmin (r.getWidth(), r.getHeight()) - 10.0f;
    auto dial = juce::Rectangle<float> (d, d).withCentre (r.getCentre());
    drawGlowCircle (g, dial.reduced (d * 0.1f), accent, 14.0f, alpha * (0.35f + 0.7f * hover + 0.5f * shown));

    const float a0 = pi * 1.25f, a1 = pi * 2.75f; // 270 degrees, gap at the bottom
    juce::Path track, fillArc;
    track.addCentredArc (dial.getCentreX(), dial.getCentreY(), d * 0.5f - 6.0f, d * 0.5f - 6.0f, 0.0f, a0, a1, true);
    g.setColour (juce::Colour (0xff2a2a36).withAlpha (alpha));
    g.strokePath (track, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (shown > 0.005f)
    {
        fillArc.addCentredArc (dial.getCentreX(), dial.getCentreY(), d * 0.5f - 6.0f, d * 0.5f - 6.0f, 0.0f, a0, a0 + (a1 - a0) * shown, true);
        g.setColour (accent.withAlpha (alpha));
        g.strokePath (fillArc, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // chrome cap with a pointer
    auto cap = dial.reduced (d * 0.2f);
    juce::ColourGradient grad (chrome.withAlpha (alpha), cap.getX(), cap.getY(), chromeDark.darker (0.6f).withAlpha (alpha), cap.getRight(), cap.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (cap);
    g.setColour (juce::Colours::black.withAlpha (0.5f * alpha));
    g.drawEllipse (cap, 1.5f);
    const float ang = a0 + (a1 - a0) * shown;
    const juce::Point<float> c = cap.getCentre();
    const float rr = cap.getWidth() * 0.5f;
    g.setColour (obsidian.withAlpha (alpha));
    g.drawLine (c.x + std::cos (ang - pi * 0.5f) * rr * 0.35f, c.y + std::sin (ang - pi * 0.5f) * rr * 0.35f,
                c.x + std::cos (ang - pi * 0.5f) * rr * 0.85f, c.y + std::sin (ang - pi * 0.5f) * rr * 0.85f, 4.0f);
    if (hasKeyboardFocus (false))
    {
        g.setColour (chrome);
        g.drawEllipse (dial.reduced (1.0f), 1.5f);
    }

    g.setColour (accent.brighter (0.3f).withAlpha (alpha));
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText (isEnabled() ? juce::String (juce::roundToInt (shown * 100.0f)) + " %" : juce::String ("PICK AN ALTER EGO"), valueArea, juce::Justification::centred);
}

// ---- ScopeStrip ------------------------------------------------------------------------------------------------------------------------

void ScopeStrip::push (float peak)
{
    const float db = juce::Decibels::gainToDecibels (std::isfinite (peak) ? peak : 0.0f, -60.0f);
    bars[(size_t) head] = juce::jmap (juce::jlimit (-60.0f, 0.0f, db), -60.0f, 0.0f, 0.0f, 1.0f);
    head = (head + 1) % numBars;
    repaint();
}

void ScopeStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff12121a));
    g.fillRoundedRectangle (r, 10.0f);
    const float step = r.getWidth() / (float) numBars;
    for (int i = 0; i < numBars; ++i)
    {
        const float v = bars[(size_t) ((head + i) % numBars)]; // oldest on the left, newest on the right
        const float h = juce::jmax (2.0f, v * r.getHeight() * 0.9f);
        g.setColour (violet.interpolatedWith (lime, v).withAlpha (0.35f + 0.65f * (float) i / (float) numBars));
        g.fillRoundedRectangle (r.getX() + (float) i * step + 0.5f, r.getCentreY() - h * 0.5f, juce::jmax (1.5f, step - 1.5f), h, 1.5f);
    }
}

// ---- StatusRing ------------------------------------------------------------------------------------------------------------------------

void StatusRing::setState (State s, float p)
{
    p = juce::jlimit (0.0f, 1.0f, std::isfinite (p) ? p : 0.0f);
    if (s != state || ! settled (p, progress))
    {
        state = s;
        progress = p;
        repaint();
    }
}

void StatusRing::tick (float dt)
{
    if (motion && state == State::waiting)
    {
        spin += dt * 4.0f;
        repaint();
    }
}

void StatusRing::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (6.0f);
    const auto c = r.getCentre();
    const float rad = r.getWidth() * 0.5f;
    const auto col = state == State::error ? pink : (state == State::idle ? chromeDark : (state == State::ready ? lime : violet));
    juce::Path ring;
    ring.addCentredArc (c.x, c.y, rad, rad, 0.0f, 0.0f, 2.0f * pi, true);
    g.setColour (juce::Colour (0xff2a2a36));
    g.strokePath (ring, juce::PathStrokeType (6.0f));

    juce::Path arc;
    switch (state)
    {
        case State::idle:
            g.setColour (chromeDark);
            g.fillEllipse (c.x - 4.0f, c.y - 4.0f, 8.0f, 8.0f);
            break;
        case State::waiting:
            arc.addCentredArc (c.x, c.y, rad, rad, 0.0f, spin, spin + 1.2f, true);
            g.setColour (col);
            g.strokePath (arc, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            break;
        case State::working:
            if (progress > 0.01f)
            {
                arc.addCentredArc (c.x, c.y, rad, rad, 0.0f, 0.0f, 2.0f * pi * progress, true);
                g.setColour (col);
                g.strokePath (arc, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            break;
        case State::ready:
        {
            g.setColour (col);
            g.strokePath (ring, juce::PathStrokeType (6.0f));
            juce::Path tick2;
            tick2.startNewSubPath (c.x - rad * 0.4f, c.y);
            tick2.lineTo (c.x - rad * 0.1f, c.y + rad * 0.32f);
            tick2.lineTo (c.x + rad * 0.45f, c.y - rad * 0.32f);
            g.strokePath (tick2, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            break;
        }
        case State::error:
            g.setColour (col);
            g.strokePath (ring, juce::PathStrokeType (6.0f));
            g.drawLine (c.x - rad * 0.35f, c.y - rad * 0.35f, c.x + rad * 0.35f, c.y + rad * 0.35f, 5.0f);
            g.drawLine (c.x + rad * 0.35f, c.y - rad * 0.35f, c.x - rad * 0.35f, c.y + rad * 0.35f, 5.0f);
            break;
    }
}

// ---- PlaygroundLookAndFeel -------------------------------------------------------------------------------------------------------------

PlaygroundLookAndFeel::PlaygroundLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, obsidian);
    setColour (juce::Label::textColourId, textBright);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1c1c26));
    setColour (juce::TextButton::buttonOnColourId, lime);
    setColour (juce::TextButton::textColourOffId, textBright);
    setColour (juce::TextButton::textColourOnId, obsidian);
    setColour (juce::Slider::textBoxTextColourId, textBright);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff14141b));
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff2e2e3b));
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff14141b));
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3a3a4a));
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff16161e));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, violet);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff1c1c26));
    setColour (juce::TooltipWindow::textColourId, textBright);
    setColour (juce::TooltipWindow::outlineColourId, violet);
}

juce::Font PlaygroundLookAndFeel::getTextButtonFont (juce::TextButton&, int h)
{
    return juce::Font (juce::FontOptions (juce::jlimit (11.0f, 17.0f, (float) h * 0.46f), juce::Font::bold));
}

void PlaygroundLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const float alpha = b.isEnabled() ? 1.0f : 0.4f;
    const auto base = on ? b.findColour (juce::TextButton::buttonOnColourId) : bg;
    if (on || highlighted)
        drawGlow (g, r.reduced (2.0f), base, 6.0f, (on ? 0.8f : 0.4f) * alpha);
    if (down)
        r = r.reduced (1.5f);
    juce::ColourGradient grad (base.brighter (highlighted ? 0.35f : 0.18f).withAlpha (alpha), r.getX(), r.getY(),
                               base.darker (down ? 0.5f : 0.25f).withAlpha (alpha), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 9.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f * alpha));
    g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.45f).reduced (2.0f, 1.0f), 7.0f);
    g.setColour ((on ? base.brighter (0.4f) : chromeDark).withAlpha (alpha));
    g.drawRoundedRectangle (r, 9.0f, 1.3f);
    if (b.hasKeyboardFocus (true))
    {
        g.setColour (chrome);
        g.drawRoundedRectangle (r.expanded (2.0f), 10.0f, 1.4f);
    }
}

void PlaygroundLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider& s)
{
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    const float alpha = s.isEnabled() ? 1.0f : 0.4f;
    auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float d = juce::jmin (area.getWidth(), area.getHeight());
    auto dial = juce::Rectangle<float> (d, d).withCentre (area.getCentre());
    const float thick = juce::jmax (3.0f, d * 0.09f), rad = d * 0.5f - thick;
    const auto c = dial.getCentre();
    juce::Path track, value;
    track.addCentredArc (c.x, c.y, rad, rad, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff2a2a36).withAlpha (alpha));
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float ang = startAngle + (endAngle - startAngle) * pos;
    if (pos > 0.003f)
    {
        value.addCentredArc (c.x, c.y, rad, rad, 0.0f, startAngle, ang, true);
        g.setColour (accent.withAlpha (alpha * 0.25f));
        g.strokePath (value, juce::PathStrokeType (thick + 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (accent.withAlpha (alpha));
        g.strokePath (value, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    auto cap = dial.reduced (d * 0.22f);
    juce::ColourGradient grad (chrome.withAlpha (alpha), cap.getX(), cap.getY(), chromeDark.darker (0.7f).withAlpha (alpha), cap.getRight(), cap.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (cap);
    g.setColour (juce::Colours::black.withAlpha (0.45f * alpha));
    g.drawEllipse (cap, 1.2f);
    const float r2 = cap.getWidth() * 0.5f;
    g.setColour (obsidian.withAlpha (alpha));
    g.drawLine (c.x + std::cos (ang - pi * 0.5f) * r2 * 0.3f, c.y + std::sin (ang - pi * 0.5f) * r2 * 0.3f,
                c.x + std::cos (ang - pi * 0.5f) * r2 * 0.88f, c.y + std::sin (ang - pi * 0.5f) * r2 * 0.88f, juce::jmax (2.0f, d * 0.045f));
    if (s.hasKeyboardFocus (false))
    {
        g.setColour (chrome);
        g.drawEllipse (dial.reduced (0.5f), 1.2f);
    }
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
    g.setColour (juce::Colour (0xff2a2a36).withAlpha (alpha));
    g.fillRoundedRectangle ((float) x, cy - 3.0f, (float) w, 6.0f, 3.0f);
    g.setColour (accent.withAlpha (alpha));
    g.fillRoundedRectangle ((float) x, cy - 3.0f, pos - (float) x, 6.0f, 3.0f);
    drawGlowCircle (g, juce::Rectangle<float> (pos - 6.0f, cy - 6.0f, 12.0f, 12.0f), accent, 6.0f, alpha * 0.6f);
    juce::ColourGradient grad (chrome.withAlpha (alpha), pos - 8.0f, cy - 8.0f, chromeDark.withAlpha (alpha), pos + 8.0f, cy + 8.0f, false);
    g.setGradientFill (grad);
    g.fillEllipse (pos - 8.0f, cy - 8.0f, 16.0f, 16.0f);
    g.setColour (accent.withAlpha (alpha));
    g.drawEllipse (pos - 8.0f, cy - 8.0f, 16.0f, 16.0f, 1.6f);
}
}
