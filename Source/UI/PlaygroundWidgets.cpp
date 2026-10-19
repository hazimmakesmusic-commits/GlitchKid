#include "PlaygroundWidgets.h"
#include "EffectModel.h"
#include <BinaryData.h>
#include "../Presets/FactoryPresets.h"
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

void drawLedDial (juce::Graphics& g, juce::Rectangle<float> dial, float start, float end, float pos, juce::Colour accent, float alpha, float hover, bool focus)
{
    constexpr int numSegments = 27;
    const float d = juce::jmin (dial.getWidth(), dial.getHeight());
    dial = juce::Rectangle<float> (d, d).withCentre (dial.getCentre());
    const auto c = dial.getCentre();
    const float segLen = d * 0.085f, segW = d * 0.036f, outer = d * 0.5f;
    const int lit = juce::roundToInt (juce::jlimit (0.0f, 1.0f, pos) * (float) numSegments);

    juce::Path seg;
    seg.addRoundedRectangle (c.x - segW * 0.5f, c.y - outer, segW, segLen, segW * 0.4f);
    for (int i = 0; i < numSegments; ++i)
    {
        const float ang = start + (end - start) * (float) i / (float) (numSegments - 1);
        const auto rot = juce::AffineTransform::rotation (ang, c.x, c.y);
        if (i < lit)
        {
            juce::Path glow;
            glow.addRoundedRectangle (c.x - segW * 1.05f, c.y - outer - segW * 0.4f, segW * 2.1f, segLen + segW * 0.8f, segW * 0.8f);
            g.setColour (accent.withAlpha (0.20f * alpha));
            g.fillPath (glow, rot);
            g.setColour (accent.withAlpha (alpha));
        }
        else
            g.setColour (juce::Colour (0xff232433).withAlpha (alpha));
        g.fillPath (seg, rot);
    }

    // bezel and cap
    const float bezelR = d * 0.345f, capR = d * 0.31f, faceR = d * 0.235f;
    g.setColour (juce::Colour (0xff0a0a11).withAlpha (alpha));
    g.fillEllipse (c.x - bezelR, c.y - bezelR, bezelR * 2.0f, bezelR * 2.0f);
    g.setColour (juce::Colour (0xff34374a).withAlpha (alpha));
    g.drawEllipse (c.x - bezelR, c.y - bezelR, bezelR * 2.0f, bezelR * 2.0f, 1.2f);

    juce::ColourGradient capFill (juce::Colour (0xff4b5268).withAlpha (alpha), c.x, c.y - capR, juce::Colour (0xff14161f).withAlpha (alpha), c.x, c.y + capR, false);
    g.setGradientFill (capFill);
    g.fillEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);
    g.setColour ((hover > 0.5f ? accent : juce::Colour (0xff6a7390)).withAlpha (alpha));
    g.drawEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f, 1.2f);

    g.setColour (juce::Colour (0xff0a0b12).withAlpha (0.75f * alpha)); // knurl
    for (int i = 0; i < 30; ++i)
    {
        const float a = (float) i * 2.0f * pi / 30.0f;
        g.drawLine (c.x + std::sin (a) * capR * 0.82f, c.y - std::cos (a) * capR * 0.82f, c.x + std::sin (a) * capR * 0.96f, c.y - std::cos (a) * capR * 0.96f, 1.4f);
    }
    juce::ColourGradient face (juce::Colour (0xff323648).withAlpha (alpha), c.x - faceR * 0.3f, c.y - faceR * 0.5f, juce::Colour (0xff0d0e15).withAlpha (alpha), c.x + faceR, c.y + faceR, true);
    g.setGradientFill (face);
    g.fillEllipse (c.x - faceR, c.y - faceR, faceR * 2.0f, faceR * 2.0f);

    const float ang = start + (end - start) * juce::jlimit (0.0f, 1.0f, pos);
    const juce::Point<float> n0 (c.x + std::sin (ang) * faceR * 0.34f, c.y - std::cos (ang) * faceR * 0.34f),
                             n1 (c.x + std::sin (ang) * faceR * 0.86f, c.y - std::cos (ang) * faceR * 0.86f);
    g.setColour (accent.withAlpha (0.3f * alpha));
    g.drawLine (n0.x, n0.y, n1.x, n1.y, juce::jmax (4.0f, d * 0.06f));
    g.setColour (accent.withAlpha (alpha));
    g.drawLine (n0.x, n0.y, n1.x, n1.y, juce::jmax (2.0f, d * 0.03f));
    const float lensR = d * 0.05f;
    g.setColour (juce::Colour (0xff05060a).withAlpha (alpha));
    g.fillEllipse (c.x - lensR, c.y - lensR, lensR * 2.0f, lensR * 2.0f);
    g.setColour (accent.withAlpha (alpha));
    g.drawEllipse (c.x - lensR, c.y - lensR, lensR * 2.0f, lensR * 2.0f, 1.2f);
    g.setColour (juce::Colours::white.withAlpha (0.08f * alpha));
    g.fillEllipse (c.x - capR * 0.75f, c.y - capR * 0.85f, capR * 1.0f, capR * 0.45f);
    if (focus)
    {
        g.setColour (chrome);
        g.drawEllipse (dial.reduced (0.5f), 1.2f);
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

void drawEffectIcon (juce::Graphics& g, int effect, juce::Rectangle<float> area, juce::Colour c)
{
    const float s = juce::jmin (area.getWidth(), area.getHeight());
    const auto o = juce::Point<float> (area.getCentreX() - s * 0.5f, area.getCentreY() - s * 0.5f);
    auto P = [&] (float x, float y) { return juce::Point<float> (o.x + x * s, o.y + y * s); };
    const juce::PathStrokeType stroke (juce::jmax (1.4f, s * 0.075f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (c);
    juce::Path p;
    auto line = [&] (std::initializer_list<juce::Point<float>> pts, bool close = false)
    {
        bool first = true;
        for (const auto& q : pts)
        {
            if (first) p.startNewSubPath (q);
            else p.lineTo (q);
            first = false;
        }
        if (close) p.closeSubPath();
    };
    switch (effect)
    {
        case fx::pitchTune:   line ({ P (.08f, .72f), P (.30f, .72f), P (.30f, .46f), P (.56f, .46f), P (.56f, .24f), P (.92f, .24f) }); break;
        case fx::formant:     p.startNewSubPath (P (.06f, .8f)); p.cubicTo (P (.16f, .15f), P (.32f, .15f), P (.40f, .6f)); p.cubicTo (P (.46f, .28f), P (.58f, .28f), P (.64f, .62f)); p.cubicTo (P (.7f, .4f), P (.84f, .4f), P (.94f, .8f)); break;
        case fx::harmony:     p.startNewSubPath (P (.06f, .38f)); p.cubicTo (P (.2f, .08f), P (.34f, .08f), P (.5f, .38f)); p.cubicTo (P (.66f, .68f), P (.8f, .68f), P (.94f, .38f));
                              p.startNewSubPath (P (.06f, .68f)); p.cubicTo (P (.2f, .38f), P (.34f, .38f), P (.5f, .68f)); p.cubicTo (P (.66f, .98f), P (.8f, .98f), P (.94f, .68f)); break;
        case fx::mutation:    line ({ P (.58f, .06f), P (.26f, .56f), P (.48f, .56f), P (.40f, .94f), P (.76f, .42f), P (.54f, .42f) }, true); break;
        case fx::doubler:     p.addEllipse (o.x + .08f * s, o.y + .26f * s, .5f * s, .48f * s); p.addEllipse (o.x + .42f * s, o.y + .26f * s, .5f * s, .48f * s); break;
        case fx::eq:          for (int i = 0; i < 3; ++i) { const float x = .22f + i * .28f; p.startNewSubPath (P (x, .12f)); p.lineTo (P (x, .88f)); const float ky = i == 1 ? .34f : (i == 0 ? .62f : .5f); p.addEllipse (P (x, ky).x - .08f * s, P (x, ky).y - .08f * s, .16f * s, .16f * s); } break;
        case fx::compressor:  line ({ P (.22f, .14f), P (.5f, .4f), P (.78f, .14f) }); line ({ P (.22f, .86f), P (.5f, .6f), P (.78f, .86f) }); line ({ P (.34f, .5f), P (.66f, .5f) }); break;
        case fx::saturation:  line ({ P (.06f, .66f), P (.2f, .3f), P (.4f, .26f), P (.5f, .5f), P (.62f, .74f), P (.8f, .74f), P (.94f, .36f) }); break;
        case fx::stutter:     for (int i = 0; i < 4; ++i) { const float x = .16f + i * .22f, h = .7f - i * .16f; p.startNewSubPath (P (x, .5f - h * .5f)); p.lineTo (P (x, .5f + h * .5f)); } break;
        case fx::echo:        for (int i = 0; i < 3; ++i) p.addCentredArc (o.x + .16f * s, o.y + .5f * s, (.22f + i * .2f) * s, (.22f + i * .2f) * s, 0.0f, juce::MathConstants<float>::halfPi - 0.95f, juce::MathConstants<float>::halfPi + 0.95f, true); break;
        case fx::dimension:   line ({ P (.5f, .06f), P (.9f, .28f), P (.9f, .72f), P (.5f, .94f), P (.1f, .72f), P (.1f, .28f) }, true); line ({ P (.1f, .28f), P (.5f, .5f), P (.9f, .28f) }); line ({ P (.5f, .5f), P (.5f, .94f) }); break;
        default: break;
    }
    g.strokePath (p, stroke);
}

void drawCrown (juce::Graphics& g, juce::Rectangle<float> a, juce::Colour c)
{
    const float s = juce::jmin (a.getWidth(), a.getHeight());
    const auto o = juce::Point<float> (a.getCentreX() - s * 0.5f, a.getCentreY() - s * 0.5f);
    auto P = [&] (float x, float y) { return juce::Point<float> (o.x + x * s, o.y + y * s); };
    juce::Path crown;
    crown.startNewSubPath (P (.08f, .78f));
    crown.lineTo (P (.08f, .32f));
    crown.lineTo (P (.30f, .55f));
    crown.lineTo (P (.5f, .16f));
    crown.lineTo (P (.70f, .55f));
    crown.lineTo (P (.92f, .32f));
    crown.lineTo (P (.92f, .78f));
    crown.closeSubPath();
    g.setColour (c);
    g.fillPath (crown);
    for (const auto& q : { P (.08f, .26f), P (.5f, .1f), P (.92f, .26f) })
        g.fillEllipse (q.x - s * 0.06f, q.y - s * 0.06f, s * 0.12f, s * 0.12f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRect (o.x + .08f * s, o.y + .72f * s, .84f * s, .06f * s);
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
    const auto b = getLocalBounds().toFloat();
    const float w = b.getWidth(), h = b.getHeight(), cx = w * 0.5f;
    const auto accent = mood == Mood::error ? pink : (mood == Mood::idle ? violet : lime);
    const float hop = motion ? -9.0f * bounce * std::abs (std::sin (bounceTime * pi * 3.0f)) : 0.0f;
    const float bob = motion ? std::sin (clock * 2.0f) * 2.0f : 0.0f;
    const float lean = motion && bounce > 0.0f ? 0.05f * std::sin (bounceTime * pi * 5.0f) : 0.0f;
    const juce::Graphics::ScopedSaveState state (g);
    g.addTransform (juce::AffineTransform::rotation (lean, cx, h * 0.7f).translated (0.0f, hop + bob));

    // glow under the saucer, brighter with the real input level
    drawGlowCircle (g, { w * 0.18f, h * 0.88f, w * 0.64f, h * 0.06f }, accent, 8.0f, 0.5f + 0.9f * shownLevel);

    // the alien (inside the dome): two antennae, a lime head, big slanted eyes, a small mouth
    const auto head = juce::Rectangle<float> (w * 0.38f, h * 0.40f).withCentre ({ cx, h * 0.47f });
    const float sway = motion ? std::sin (clock * 2.4f) * 1.6f : 0.0f;
    for (int i = 0; i < 2; ++i)
    {
        const float dir = i == 0 ? -1.0f : 1.0f;
        const juce::Point<float> root (cx + dir * w * 0.07f, head.getY() + h * 0.04f), tip (cx + dir * (w * 0.15f + sway), h * 0.20f - 3.0f * shownLevel);
        g.setColour (juce::Colour (0xff9be000));
        g.drawLine (root.x, root.y, tip.x, tip.y, 1.6f);
        g.setColour (pink);
        g.fillEllipse (tip.x - w * 0.03f, tip.y - w * 0.03f, w * 0.06f, w * 0.06f);
    }
    juce::ColourGradient skin (juce::Colour (0xffeaff9e), head.getX() + head.getWidth() * 0.3f, head.getY(), juce::Colour (0xff3a6200), head.getRight(), head.getBottom(), false);
    skin.addColour (0.45, juce::Colour (0xff9be000));
    g.setGradientFill (skin);
    g.fillEllipse (head);
    g.setColour (juce::Colour (0xff2a4700));
    g.drawEllipse (head, 1.2f);

    const float open = mood == Mood::ready && bounce > 0.0f ? 0.4f : 1.0f - blink * 0.9f;
    for (int i = 0; i < 2; ++i)
    {
        const float side = i == 0 ? -1.0f : 1.0f;
        const float ex = cx + side * head.getWidth() * 0.25f, ey = head.getY() + head.getHeight() * 0.50f;
        const float ew = head.getWidth() * 0.36f, eh = ew * 0.62f * open;
        juce::Path eye;
        eye.addEllipse (ex - ew * 0.5f, ey - eh * 0.5f, ew, eh);
        eye.applyTransform (juce::AffineTransform::rotation (-side * 0.35f, ex, ey));
        g.setColour (juce::Colour (0xff05020a));
        g.fillPath (eye);
        const float pr = ew * (0.16f + 0.10f * shownLevel);
        g.setColour (mood == Mood::error ? pink : juce::Colour (0xffb56bff));
        g.fillEllipse (ex + shownLook * ew * 0.12f - pr, ey - pr * open, pr * 2.0f, pr * 2.0f * open);
        g.setColour (juce::Colours::white.withAlpha (0.9f * open));
        g.fillEllipse (ex + shownLook * ew * 0.12f - pr * 0.8f, ey - pr * 0.9f * open, pr * 0.6f, pr * 0.6f * open);
    }
    const float my = head.getY() + head.getHeight() * 0.80f, mw = head.getWidth() * 0.20f;
    g.setColour (juce::Colour (0xff1d3300));
    if (mood == Mood::listening)
    {
        const float mh = 3.0f + 6.0f * shownLevel;
        g.fillEllipse (cx - mw * 0.5f, my - mh * 0.5f, mw, mh);
    }
    else
    {
        juce::Path mouth;
        if (mood == Mood::error)
        {
            mouth.startNewSubPath (cx - mw, my + 1.0f);
            for (int i = 1; i <= 4; ++i)
                mouth.lineTo (cx - mw + mw * 0.5f * (float) i, my + (i % 2 ? -2.5f : 2.5f));
        }
        else
        {
            mouth.startNewSubPath (cx - mw, my - 1.0f);
            mouth.quadraticTo (cx, my + (mood == Mood::ready ? 9.0f : 5.0f), cx + mw, my - 1.0f);
        }
        g.strokePath (mouth, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // glass dome
    juce::Path dome;
    dome.addCentredArc (cx, h * 0.68f, w * 0.40f, h * 0.52f, 0.0f, -pi * 0.5f, pi * 0.5f, true);
    dome.closeSubPath();
    juce::ColourGradient glass (juce::Colour (0xffd9c2ff).withAlpha (0.38f), cx, h * 0.16f, violet.withAlpha (0.10f), cx, h * 0.68f, false);
    g.setGradientFill (glass);
    g.fillPath (dome);
    g.setColour (juce::Colour (0xffcfd8ea).withAlpha (0.65f));
    g.strokePath (dome, juce::PathStrokeType (1.4f));
    juce::Path shine;
    shine.addCentredArc (cx, h * 0.68f, w * 0.33f, h * 0.44f, 0.0f, -pi * 0.42f, -pi * 0.12f, true);
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.strokePath (shine, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // saucer hull with six blinking lights (all lime when ready, all pink on error)
    const auto hull = juce::Rectangle<float> (w * 0.96f, h * 0.32f).withCentre ({ cx, h * 0.74f });
    juce::ColourGradient hullFill (juce::Colour (0xffe6ecf6), cx, hull.getY(), juce::Colour (0xff252a38), cx, hull.getBottom(), false);
    hullFill.addColour (0.4, juce::Colour (0xff8a94aa));
    g.setGradientFill (hullFill);
    g.fillEllipse (hull);
    g.setColour (juce::Colour (0xff10121a));
    g.drawEllipse (hull, 1.2f);
    g.setColour (juce::Colour (0xff0b0d14).withAlpha (0.5f));
    g.fillEllipse (hull.getX() + w * 0.02f, hull.getY() + h * 0.01f, hull.getWidth() - w * 0.04f, h * 0.07f);
    static const juce::Colour cycle[3] = { lime, pink, violet.brighter (0.5f) };
    for (int i = 0; i < 6; ++i)
    {
        const float t = (float) i / 5.0f;
        const float lx = hull.getX() + hull.getWidth() * (0.14f + 0.72f * t);
        const float ly = hull.getCentreY() + h * 0.045f + std::sin (t * pi) * h * 0.04f;
        const auto col = mood == Mood::ready ? lime : (mood == Mood::error ? pink : cycle[i % 3]);
        const float on = motion ? 0.55f + 0.45f * std::sin (clock * 5.0f + (float) i * 1.1f) : 1.0f;
        g.setColour (col.withAlpha (0.35f * on));
        g.fillEllipse (lx - w * 0.04f, ly - w * 0.04f, w * 0.08f, w * 0.08f);
        g.setColour (col.withAlpha (0.55f + 0.45f * on));
        g.fillEllipse (lx - w * 0.022f, ly - w * 0.022f, w * 0.044f, w * 0.044f);
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

void WorldCard::tick (float dt)
{
    const float prevHover = hover, prevGlow = glow;
    bool ambient = false;
    if (motion && selected) // the loaded world has a quiet ambient motion (about 15 repaints a second, only for that card)
    {
        phase += dt;
        ambient = (++frame & 1) == 0;
    }
    hover = motion ? approach (hover, hovering ? 1.0f : 0.0f, 0.3f) : (hovering ? 1.0f : 0.0f);
    glow = motion ? approach (glow, selected ? 1.0f : 0.0f, 0.2f) : (selected ? 1.0f : 0.0f);
    if (ambient || ! settled (prevHover, hover) || ! settled (prevGlow, glow))
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
    auto r = getLocalBounds().toFloat().reduced (4.0f);
    if (pressed)
        r = r.reduced (2.0f);
    const auto plate = makePlatePath (r, false);

    drawGlow (g, r.reduced (3.0f), accent, 12.0f, 0.9f * glow + 0.45f * hover);
    juce::ColourGradient fill (accent.withMultipliedBrightness (0.55f + 0.2f * hover + 0.15f * glow), r.getX(), r.getY(), accent.withMultipliedBrightness (0.18f), r.getX(), r.getBottom(), false);
    g.setGradientFill (fill);
    g.fillPath (plate);
    {
        const juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (plate, {});
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRect (r.withHeight (r.getHeight() * 0.42f));
    }
    g.setColour ((selected ? lime : accent.brighter (0.2f)).withAlpha (selected ? 1.0f : 0.55f + 0.4f * hover));
    g.strokePath (plate, juce::PathStrokeType (selected ? 2.6f : 1.4f));
    if (hasKeyboardFocus (true))
    {
        g.setColour (chrome);
        g.strokePath (makePlatePath (r.expanded (3.0f), false), juce::PathStrokeType (1.4f));
    }

    // artwork: a small drawn scene on the right of the pod. It wakes up (brightens, shifts) when the world is loaded.
    {
        const juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (plate, {});
        const auto art = juce::Rectangle<float> (r.getX() + r.getWidth() * 0.52f, r.getY(), r.getWidth() * 0.48f, r.getHeight());
        const float a = 0.30f + 0.55f * glow + 0.15f * hover;
        if (icon == Icon::planet)
        {
            for (int i = 0; i < 14; ++i) // stars, twinkling slowly
            {
                const float sx = art.getX() + std::fmod ((float) i * 37.7f, art.getWidth()), sy = art.getY() + std::fmod ((float) i * 23.3f, art.getHeight() * 0.8f);
                const float tw = 0.5f + 0.5f * std::sin (phase * 1.7f + (float) i);
                g.setColour (chrome.withAlpha (a * (0.35f + 0.5f * tw)));
                g.fillRect (sx, sy, 1.6f, 1.6f);
            }
            const auto c = juce::Point<float> (art.getCentreX() + 14.0f + 3.0f * std::sin (phase * 0.6f) * glow, art.getBottom() - 4.0f);
            const float R = 30.0f + 3.0f * glow;
            juce::ColourGradient pl (neon.withAlpha (a), c.x - R * 0.5f, c.y - R, accent.darker (0.6f).withAlpha (a), c.x + R, c.y + R, false);
            g.setGradientFill (pl);
            g.fillEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f);
            g.setColour (chrome.withAlpha (a * 0.7f));
            juce::Path ring;
            ring.addEllipse (c.x - R * 1.7f, c.y - R * 0.28f, R * 3.4f, R * 0.56f);
            g.strokePath (ring, juce::PathStrokeType (1.4f), juce::AffineTransform::rotation (-0.35f, c.x, c.y));
        }
        else if (icon == Icon::heart)
        {
            for (int i = 0; i < 9; ++i) // rain
            {
                const float x = art.getX() + (float) i * 17.0f + 6.0f;
                const float y = art.getY() + std::fmod ((float) i * 19.0f + phase * 38.0f, art.getHeight() + 14.0f) - 14.0f;
                g.setColour (chrome.withAlpha (a * 0.4f));
                g.drawLine (x, y, x - 3.0f, y + 11.0f, 1.0f);
            }
            juce::Path h;
            const auto c = juce::Point<float> (art.getRight() - 40.0f, art.getCentreY() + 3.0f);
            const float k = 1.0f + 0.04f * std::sin (phase * 2.4f) * glow;
            h.startNewSubPath (c.x, c.y + 22.0f * k);
            h.cubicTo (c.x - 34.0f * k, c.y - 4.0f * k, c.x - 16.0f * k, c.y - 30.0f * k, c.x, c.y - 11.0f * k);
            h.cubicTo (c.x + 16.0f * k, c.y - 30.0f * k, c.x + 34.0f * k, c.y - 4.0f * k, c.x, c.y + 22.0f * k);
            g.setColour (accent.brighter (0.4f).withAlpha (a * 0.8f));
            g.fillPath (h);
            g.setColour (obsidian.withAlpha (0.8f));
            g.drawLine (c.x - 3.0f, c.y - 10.0f, c.x + 3.0f, c.y - 1.0f, 1.8f);
            g.drawLine (c.x + 3.0f, c.y - 1.0f, c.x - 2.0f, c.y + 10.0f, 1.8f);
        }
        else
        {
            for (int i = 0; i < 6; ++i) // pixel blocks that shuffle while the world is awake
                for (int j = 0; j < 3; ++j)
                {
                    const int seed = (i * 7 + j * 13 + (int) (phase * 2.0f)) % 5;
                    if (seed < 2)
                        continue;
                    g.setColour ((seed == 4 ? lime : accent.brighter (0.5f)).withAlpha (a * (0.3f + 0.12f * (float) seed)));
                    g.fillRect (art.getX() + 10.0f + (float) i * 17.0f, art.getY() + 8.0f + (float) j * 17.0f, 13.0f, 13.0f);
                }
            const float y = art.getY() + std::fmod (phase * 26.0f, art.getHeight());
            g.setColour (lime.withAlpha (a * 0.55f * glow));
            g.fillRect (art.getX(), y, art.getWidth(), 1.5f);
        }
    }

    // status light: lit lime when this pod is loaded
    const juce::Point<float> led (r.getRight() - 22.0f, r.getY() + 20.0f);
    if (selected)
    {
        drawGlowCircle (g, { led.x - 5.0f, led.y - 5.0f, 10.0f, 10.0f }, lime, 8.0f, 0.9f);
        g.setColour (lime);
    }
    else
        g.setColour (juce::Colour (0xff0b0b10));
    g.fillEllipse (led.x - 5.0f, led.y - 5.0f, 10.0f, 10.0f);
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawEllipse (led.x - 5.0f, led.y - 5.0f, 10.0f, 10.0f, 1.2f);

    // icon (original, drawn): planet / broken heart / glitch blocks, at the left
    auto ic = juce::Rectangle<float> (34.0f, 34.0f).withCentre ({ r.getX() + 36.0f, r.getCentreY() });
    g.setColour (accent.brighter (0.5f));
    if (icon == Icon::planet)
    {
        g.fillEllipse (ic.reduced (6.0f));
        g.setColour (chrome.withAlpha (0.9f));
        juce::Path ring;
        ring.addEllipse (ic.getX() - 2.0f, ic.getCentreY() - 4.5f, ic.getWidth() + 4.0f, 9.0f);
        g.strokePath (ring, juce::PathStrokeType (1.6f), juce::AffineTransform::rotation (-0.45f, ic.getCentreX(), ic.getCentreY()));
    }
    else if (icon == Icon::heart)
    {
        juce::Path h;
        const auto c = ic.getCentre();
        h.startNewSubPath (c.x, c.y + 13.0f);
        h.cubicTo (c.x - 22.0f, c.y - 3.0f, c.x - 10.0f, c.y - 19.0f, c.x, c.y - 7.0f);
        h.cubicTo (c.x + 10.0f, c.y - 19.0f, c.x + 22.0f, c.y - 3.0f, c.x, c.y + 13.0f);
        g.fillPath (h);
        g.setColour (obsidian);
        g.drawLine (c.x - 2.0f, c.y - 6.0f, c.x + 1.5f, c.y, 1.6f);
        g.drawLine (c.x + 1.5f, c.y, c.x - 1.5f, c.y + 7.0f, 1.6f);
    }
    else
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                if ((i + j) % 2 == 0)
                    g.fillRect (ic.getX() + (float) i * 11.5f + (j == 1 ? 2.0f : 0.0f), ic.getY() + (float) j * 11.5f, 10.0f, 10.0f);

    const float tx = r.getX() + 66.0f, tw = r.getWidth() - 66.0f - 40.0f;
    g.setColour (textBright);
    g.setFont (fontOf (16.0f, true));
    g.drawFittedText (title, juce::Rectangle<float> (tx, r.getY() + 5.0f, tw, 22.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.75f);
    g.setColour (textBright.withAlpha (0.82f));
    g.setFont (fontOf (12.0f));
    g.drawText (tagline, tx, r.getY() + 26.0f, tw + 30.0f, 16.0f, juce::Justification::centredLeft);
    g.setColour (accent.brighter (0.6f).withAlpha (0.95f));
    g.setFont (fontOf (10.5f, true));
    g.drawText (selected ? (edited ? "LOADED (EDITED)" : "LOADED") : "LOADS: " + loads, tx, r.getY() + 43.0f, tw + 30.0f, 14.0f, juce::Justification::centredLeft);
}

// ---- ArtistCard ------------------------------------------------------------------------------------------------------------------------

void ArtistCard::bind (int presetIndex, bool s, bool l, bool f)
{
    if (presetIndex != preset || s != selected || l != live || f != favourite)
    {
        const bool newlySelected = s && ! selected;
        preset = presetIndex;
        selected = s;
        live = l;
        favourite = f;
        if (newlySelected && motion)
            glow = 0.0f; // the frame lights up with a short ramp
        setVisible (preset >= 0);
        repaint();
    }
}

void ArtistCard::tick (float)
{
    const float h = hover, g = glow;
    hover = motion ? approach (hover, isMouseOver() ? 1.0f : 0.0f, 0.3f) : (isMouseOver() ? 1.0f : 0.0f);
    glow = motion ? approach (glow, selected ? 1.0f : 0.0f, 0.25f) : (selected ? 1.0f : 0.0f);
    if (! settled (h, hover) || ! settled (g, glow))
        repaint();
}

void ArtistCard::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    if (preset < 0)
        return;
    const auto& info = FactoryPresets::info (preset);
    const auto accent = info.region == FactoryPresets::Region::international ? neon : juce::Colour (0xffffa23a);
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    if (down)
        r = r.reduced (1.0f);
    juce::ColourGradient fill (panelTop.interpolatedWith (accent, 0.10f + 0.08f * glow + 0.05f * hover), r.getX(), r.getY(), panelBottom, r.getX(), r.getBottom(), false);
    g.setGradientFill (fill);
    g.fillRoundedRectangle (r, 8.0f);
    if (glow > 0.01f)
        drawGlow (g, r.reduced (2.0f), lime, 7.0f, 0.55f * glow);
    g.setColour (selected ? lime : (highlighted ? accent : border));
    g.drawRoundedRectangle (r, 8.0f, selected ? 2.0f : 1.2f);
    if (hasKeyboardFocus (true))
    {
        g.setColour (chrome);
        g.drawRoundedRectangle (r.expanded (2.0f), 10.0f, 1.2f);
    }

    // sigil: five strokes between points on a ring, picked from the preset number (a tiny alien glyph, different for every card)
    const auto sg = juce::Rectangle<float> (26.0f, 26.0f).withCentre ({ r.getX() + 20.0f, r.getCentreY() });
    g.setColour (accent.withAlpha (selected ? 1.0f : 0.8f));
    g.drawEllipse (sg, 1.0f);
    juce::Random rng (preset * 7919 + 13);
    juce::Point<float> prev;
    for (int k = 0; k < 5; ++k)
    {
        const float a = (float) rng.nextInt (8) * juce::MathConstants<float>::pi / 4.0f;
        const juce::Point<float> pt (sg.getCentreX() + std::sin (a) * 9.0f, sg.getCentreY() - std::cos (a) * 9.0f);
        if (k > 0)
            g.drawLine (prev.x, prev.y, pt.x, pt.y, 1.5f);
        prev = pt;
    }
    g.fillEllipse (sg.getCentreX() - 1.5f, sg.getCentreY() - 1.5f, 3.0f, 3.0f);

    auto area = r.withTrimmedLeft (38.0f).withTrimmedRight (4.0f);
    g.setColour (selected ? textBright : textBright.withAlpha (0.9f));
    g.setFont (fontOf (11.5f, true));
    g.drawFittedText (info.artist, area.withTrimmedBottom (14.0f).toNearestInt(), juce::Justification::centredLeft, 2, 0.85f);
    int bit = 0;
    while (bit < FactoryPresets::numTags - 1 && ! ((info.tags >> bit) & 1u))
        ++bit;
    g.setColour (textDim);
    g.setFont (fontOf (9.5f, true));
    g.drawText (FactoryPresets::tagName (bit), area.removeFromBottom (13.0f), juce::Justification::centredLeft, true);
    if (live)
    {
        g.setColour (lime);
        g.fillEllipse (r.getRight() - 12.0f, r.getY() + 5.0f, 6.0f, 6.0f);
    }
    if (favourite)
    {
        juce::Path star;
        star.addStar ({ r.getRight() - 11.0f, r.getBottom() - 10.0f }, 5, 2.4f, 5.2f);
        g.setColour (neon);
        g.fillPath (star);
    }
}

// ---- NoteKey ---------------------------------------------------------------------------------------------------------------------------

bool NoteKey::tick (float dt)
{
    if (pulseT <= 0.0f)
        return false;
    pulseT = juce::jmax (0.0f, pulseT - dt * 3.2f); // about 0.3 s
    repaint();
    return pulseT > 0.0f;
}

void NoteKey::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    if (down)
        r = r.reduced (1.5f);
    const bool sharp = getButtonText().contains ("#");
    if (pulseT > 0.0f)
        drawGlow (g, r.expanded ((1.0f - pulseT) * 7.0f), lime, 10.0f, pulseT * 0.9f);
    else if (selected)
        drawGlow (g, r.reduced (3.0f), lime, 7.0f, 0.55f);
    const auto base = selected ? lime : (sharp ? juce::Colour (0xff0b0c12) : juce::Colour (0xff1d2030));
    juce::ColourGradient fill (base.brighter (highlighted ? 0.3f : 0.12f), r.getX(), r.getY(), base.darker (0.3f), r.getX(), r.getBottom(), false);
    g.setGradientFill (fill);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (selected ? lime.brighter (0.4f) : (highlighted ? neon : chromeDark));
    g.drawRoundedRectangle (r, 6.0f, selected ? 2.0f : 1.2f);
    if (hasKeyboardFocus (true))
    {
        g.setColour (chrome);
        g.drawRoundedRectangle (r.expanded (2.0f), 8.0f, 1.2f);
    }
    g.setColour (selected ? obsidian : textBright);
    g.setFont (fontOf (16.0f, true));
    g.drawText (getButtonText(), r.withTrimmedBottom (8.0f), juce::Justification::centred, false);
    if (inScale) // this note is allowed by the chosen scale
    {
        g.setColour (selected ? obsidian : neon);
        g.fillEllipse (r.getCentreX() - 2.5f, r.getBottom() - 9.0f, 5.0f, 5.0f);
    }
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
    g.setFont (fontOf (16.0f, true));
    g.drawText (name, r.removeFromTop (22.0f), juce::Justification::centred);

    auto valueArea = r.removeFromBottom (24.0f);
    drawLedDial (g, r.reduced (2.0f), pi * 1.25f, pi * 2.75f, shown, accent, alpha, hover, hasKeyboardFocus (false));

    g.setColour (accent.brighter (0.3f).withAlpha (alpha));
    g.setFont (fontOf (15.0f, true));
    g.drawText (isEnabled() ? juce::String (juce::roundToInt (shown * 100.0f)) + " %" : juce::String ("NEEDS ALTER EGO"), valueArea, juce::Justification::centred);
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

// ---- plates and the reactor button -----------------------------------------------------------------------------------------------------

juce::Path makePlatePath (juce::Rectangle<float> r, bool tab)
{
    const float c = juce::jmin (9.0f, r.getHeight() * 0.3f);
    juce::Path p;
    if (tab)
    {
        p.startNewSubPath (r.getX() + c, r.getY());
        p.lineTo (r.getRight(), r.getY());
        p.lineTo (r.getRight() - c, r.getBottom());
        p.lineTo (r.getX(), r.getBottom());
    }
    else
    {
        p.startNewSubPath (r.getX() + c, r.getY());
        p.lineTo (r.getRight() - c, r.getY());
        p.lineTo (r.getRight(), r.getY() + c);
        p.lineTo (r.getRight(), r.getBottom() - c);
        p.lineTo (r.getRight() - c, r.getBottom());
        p.lineTo (r.getX() + c, r.getBottom());
        p.lineTo (r.getX(), r.getBottom() - c);
        p.lineTo (r.getX(), r.getY() + c);
    }
    p.closeSubPath();
    return p;
}

void ReactorButton::setState (AutoState s, float p)
{
    p = juce::jlimit (0.0f, 1.0f, std::isfinite (p) ? p : 0.0f);
    if (s != state || ! settled (p, progress))
    {
        state = s;
        progress = p;
        repaint();
    }
}

void ReactorButton::tick (float dt)
{
    if (motion && state == AutoState::waiting)
    {
        spin += dt * 9.0f;
        repaint();
    }
}

void ReactorButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const float alpha = isEnabled() ? 1.0f : 0.45f;
    const auto area = getLocalBounds().toFloat();
    const float d = juce::jmin (area.getWidth(), area.getHeight());
    const auto c = area.getCentre();
    const float outer = d * 0.5f - 2.0f, segLen = d * 0.085f, segW = d * 0.052f;
    constexpr int numSegments = 24;
    const auto ringColour = state == AutoState::error ? pink : lime;

    juce::Path seg;
    seg.addRoundedRectangle (c.x - segW * 0.5f, c.y - outer, segW, segLen, segW * 0.35f);
    for (int i = 0; i < numSegments; ++i)
    {
        float level = 0.30f; // dim
        switch (state)
        {
            case AutoState::idle:    level = 0.30f; break;
            case AutoState::waiting: { const float head = std::fmod (spin, (float) numSegments); float dist = head - (float) i; if (dist < 0.0f) dist += (float) numSegments; level = dist < 6.0f ? 1.0f - dist / 7.0f : 0.20f; if (! motion) level = i < 6 ? 0.9f : 0.2f; break; }
            case AutoState::working: level = (float) (i + 1) / (float) numSegments <= progress + 0.001f ? 1.0f : 0.20f; break;
            case AutoState::ready:   level = 1.0f; break;
            case AutoState::error:   level = 1.0f; break;
        }
        const auto rot = juce::AffineTransform::rotation (2.0f * pi * (float) i / (float) numSegments, c.x, c.y);
        if (level > 0.6f)
        {
            juce::Path glow;
            glow.addRoundedRectangle (c.x - segW, c.y - outer - segW * 0.4f, segW * 2.0f, segLen + segW * 0.8f, segW * 0.8f);
            g.setColour (ringColour.withAlpha (0.22f * alpha * level));
            g.fillPath (glow, rot);
        }
        g.setColour ((level > 0.25f ? ringColour : juce::Colour (0xff1c2412)).withAlpha (alpha * juce::jlimit (0.25f, 1.0f, level)));
        g.fillPath (seg, rot);
    }

    // the core: a lime sphere that sinks when pressed
    const float coreR = d * 0.5f - segLen - d * 0.09f;
    const float lift = down ? 2.0f : 7.0f;
    g.setColour (juce::Colour (0xff3f5b00).withAlpha (alpha));
    g.fillEllipse (c.x - coreR, c.y - coreR + lift, coreR * 2.0f, coreR * 2.0f);
    const float oy = down ? 5.0f : 0.0f;
    juce::ColourGradient core (juce::Colour (0xfff1ffb8).withAlpha (alpha), c.x - coreR * 0.3f, c.y - coreR * 0.5f + oy, juce::Colour (0xff5f8a00).withAlpha (alpha), c.x + coreR, c.y + coreR + oy, true);
    core.addColour (0.45, lime.withAlpha (alpha));
    g.setGradientFill (core);
    g.fillEllipse (c.x - coreR, c.y - coreR + oy, coreR * 2.0f, coreR * 2.0f);
    if (highlighted || hasKeyboardFocus (false))
    {
        g.setColour (hasKeyboardFocus (false) ? chrome : juce::Colours::white.withAlpha (0.5f));
        g.drawEllipse (c.x - coreR - 2.0f, c.y - coreR - 2.0f + oy, coreR * 2.0f + 4.0f, coreR * 2.0f + 4.0f, 1.4f);
    }
    g.setColour (juce::Colour (0xff0b1400).withAlpha (alpha));
    g.setFont (fontOf (juce::jlimit (11.0f, 17.0f, d * 0.115f), true));
    g.drawFittedText (getButtonText().replaceFirstOccurrenceOf (" ", "\n"), juce::Rectangle<int> ((int) (c.x - coreR), (int) (c.y - coreR * 0.5f + oy), (int) (coreR * 2.0f), (int) coreR), juce::Justification::centred, 2);
}

// ---- effect tiles, chain rows, meters ---------------------------------------------------------------------------------------------------

EffectTile::EffectTile (int e, juce::Colour c) : juce::Button (fx::info (e).name), effect (e), identity (c)
{
    setTooltip (fx::info (e).description);
    setWantsKeyboardFocus (true);
}

void EffectTile::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    if (down)
        r = r.reduced (1.0f);
    juce::ColourGradient fill ((selected ? violet.withAlpha (0.30f) : panelTop), r.getX(), r.getY(), (selected ? violet.withAlpha (0.10f) : panelBottom), r.getX(), r.getBottom(), false);
    g.setGradientFill (fill);
    g.fillRoundedRectangle (r, 9.0f);
    g.setColour (selected ? neon : (highlighted ? violet : border));
    g.drawRoundedRectangle (r, 9.0f, selected ? 2.0f : 1.2f);
    if (hasKeyboardFocus (true))
    {
        g.setColour (chrome);
        g.drawRoundedRectangle (r.expanded (2.0f), 11.0f, 1.2f);
    }

    const bool lit = state == 1;
    drawEffectIcon (g, effect, { r.getX() + 8.0f, r.getCentreY() - 11.0f, 22.0f, 22.0f }, lit ? lime : (state == 0 ? textDim : identity));
    g.setColour (lit || selected ? textBright : textDim);
    g.setFont (fontOf (11.0f, true));
    g.drawFittedText (getButtonText(), juce::Rectangle<float> (r.getX() + 34.0f, r.getY() + 4.0f, r.getWidth() - 34.0f - 6.0f, r.getHeight() - 4.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);

    const juce::Point<float> led (r.getRight() - 12.0f, r.getY() + 12.0f);
    const bool shaped = state == -1;
    if (lit)
        drawGlowCircle (g, { led.x - 3.0f, led.y - 3.0f, 6.0f, 6.0f }, lime, 5.0f, 0.8f);
    g.setColour (lit ? lime : (shaped ? neon.withAlpha (0.4f) : disabled));
    g.fillEllipse (led.x - 3.5f, led.y - 3.5f, 7.0f, 7.0f);
}

ChainRow::ChainRow (int e, juce::Colour c) : juce::Button (fx::info (e).name), effect (e), identity (c)
{
    setTooltip (juce::String (fx::info (e).description) + (fx::canToggle (e) ? " Click the switch to turn it on or off." : " (Always in the chain: no on/off switch.)"));
    setWantsKeyboardFocus (true);
}

void ChainRow::mouseDown (const juce::MouseEvent& e)
{
    downX = e.x;
    juce::Button::mouseDown (e);
}

void ChainRow::clicked (const juce::ModifierKeys&)
{
    if (fx::canToggle (effect) && downX < 48)
    {
        if (onToggle)
            onToggle();
    }
    else if (onSelect)
        onSelect();
}

void ChainRow::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    if (selected || highlighted)
    {
        g.setColour (selected ? violet.withAlpha (0.22f) : juce::Colours::white.withAlpha (0.04f));
        g.fillRoundedRectangle (r, 8.0f);
    }
    if (selected)
    {
        g.setColour (neon);
        g.fillRoundedRectangle (r.getX(), r.getY() + 6.0f, 3.0f, r.getHeight() - 12.0f, 1.5f);
    }
    if (hasKeyboardFocus (true))
    {
        g.setColour (chrome);
        g.drawRoundedRectangle (r, 8.0f, 1.2f);
    }

    // switch (real on/off) or a dot (always in the chain)
    const auto sw = juce::Rectangle<float> (28.0f, 14.0f).withCentre ({ r.getX() + 26.0f, r.getCentreY() });
    if (fx::canToggle (effect))
    {
        g.setColour (isOn ? lime.withAlpha (0.25f) : juce::Colour (0xff05060a));
        g.fillRoundedRectangle (sw, 7.0f);
        g.setColour (isOn ? lime : border);
        g.drawRoundedRectangle (sw, 7.0f, 1.2f);
        g.setColour (isOn ? lime : disabled);
        g.fillEllipse (isOn ? sw.getRight() - 12.0f : sw.getX() + 2.0f, sw.getY() + 2.0f, 10.0f, 10.0f);
    }
    else
    {
        g.setColour (isOn ? lime : disabled);
        g.fillEllipse (sw.getCentreX() - 3.5f, sw.getCentreY() - 3.5f, 7.0f, 7.0f);
    }
    drawEffectIcon (g, effect, { r.getX() + 50.0f, r.getCentreY() - 10.0f, 20.0f, 20.0f }, isOn ? lime : identity);
    g.setColour (isOn || selected ? textBright : textDim);
    g.setFont (fontOf (12.0f, true));
    g.drawText (getButtonText(), r.getX() + 78.0f, r.getY(), r.getWidth() - 82.0f, r.getHeight(), juce::Justification::centredLeft, true);
}

void GearButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (highlighted ? panelTop.brighter (0.1f) : panelBottom);
    g.fillRoundedRectangle (r, 9.0f);
    g.setColour (hasKeyboardFocus (true) ? chrome : (highlighted ? violet : border));
    g.drawRoundedRectangle (r, 9.0f, 1.2f);
    const auto c = r.getCentre();
    const float R = r.getWidth() * 0.27f + (down ? -0.5f : 0.0f);
    juce::Path gear;
    for (int i = 0; i < 8; ++i)
    {
        const float a = (float) i * juce::MathConstants<float>::pi / 4.0f;
        juce::Path tooth;
        tooth.addRoundedRectangle (c.x - 2.0f, c.y - R - 3.5f, 4.0f, 6.0f, 1.2f);
        gear.addPath (tooth, juce::AffineTransform::rotation (a, c.x, c.y));
    }
    gear.addEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f);
    g.setColour (highlighted ? neon : textDim);
    g.fillPath (gear);
    g.setColour (highlighted ? panelTop : panelBottom);
    g.fillEllipse (c.x - R * 0.42f, c.y - R * 0.42f, R * 0.84f, R * 0.84f);
}

void ActivityMeter::push (float peak)
{
    const float db = juce::Decibels::gainToDecibels (std::isfinite (peak) ? peak : 0.0f, -60.0f);
    const float next = db > shownDb ? db : juce::jmax (db, shownDb - 2.0f);
    if (! juce::approximatelyEqual (next, shownDb))
    {
        shownDb = next;
        repaint();
    }
}

void ActivityMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    constexpr int n = 8;
    const float segW = (r.getWidth() - (float) (n - 1) * 2.0f) / (float) n;
    const int lit = juce::roundToInt (juce::jmap (juce::jlimit (-60.0f, 0.0f, shownDb), -60.0f, 0.0f, 0.0f, (float) n));
    for (int i = 0; i < n; ++i)
    {
        g.setColour (i < lit ? (i >= n - 1 ? errorRed : (i >= n - 3 ? lime : violet)) : juce::Colour (0xff1b1f2b));
        g.fillRoundedRectangle (r.getX() + (float) i * (segW + 2.0f), r.getY(), segW, r.getHeight(), 1.5f);
    }
}

void ClipLed::push (float peak)
{
    const bool was = hold > 0;
    if (std::isfinite (peak) && peak >= 0.99f)
        hold = 30;
    else if (hold > 0)
        --hold;
    if ((hold > 0) != was)
        repaint();
}

void ClipLed::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    if (hold > 0)
        drawGlowCircle (g, r.reduced (3.0f), errorRed, 4.0f, 0.9f);
    g.setColour (hold > 0 ? errorRed : juce::Colour (0xff2a1d22));
    g.fillEllipse (r.reduced (2.0f));
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
    return fontOf (juce::jlimit (11.0f, 16.0f, (float) h * 0.42f), true);
}

juce::Font PlaygroundLookAndFeel::getLabelFont (juce::Label& l) { return fontOf (l.getFont().getHeight(), l.getFont().isBold()); }
juce::Font PlaygroundLookAndFeel::getComboBoxFont (juce::ComboBox& b) { return fontOf (juce::jlimit (12.0f, 15.0f, (float) b.getHeight() * 0.4f), false); }
juce::Font PlaygroundLookAndFeel::getPopupMenuFont() { return fontOf (14.0f, false); }

void PlaygroundLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const float alpha = b.isEnabled() ? 1.0f : 0.4f;
    const auto base = on ? b.findColour (juce::TextButton::buttonOnColourId) : bg;
    const bool tab = b.getProperties().getWithDefault ("shape", "plate").toString() == "tab";
    if (on || highlighted)
        drawGlow (g, r.reduced (3.0f), base, 6.0f, (on ? 0.7f : 0.35f) * alpha);
    if (down)
        r = r.reduced (1.5f);
    const auto plate = makePlatePath (r, tab);
    juce::ColourGradient grad (base.brighter (highlighted ? 0.35f : 0.18f).withAlpha (alpha), r.getX(), r.getY(),
                               base.darker (down ? 0.5f : 0.25f).withAlpha (alpha), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillPath (plate);
    {
        const juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (plate, {});
        g.setColour (juce::Colours::white.withAlpha (0.08f * alpha));
        g.fillRect (r.withHeight (r.getHeight() * 0.45f));
    }
    g.setColour ((on ? base.brighter (0.4f) : chromeDark).withAlpha (alpha));
    g.strokePath (plate, juce::PathStrokeType (1.4f));
    if (b.hasKeyboardFocus (true))
    {
        g.setColour (chrome);
        g.strokePath (makePlatePath (r.expanded (2.5f), tab), juce::PathStrokeType (1.4f));
    }
}

void PlaygroundLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const float alpha = b.isEnabled() ? 1.0f : 0.4f;
    const bool on = b.getToggleState();
    const auto sw = juce::Rectangle<float> (36.0f, 18.0f).withCentre ({ 22.0f, (float) b.getHeight() * 0.5f });
    g.setColour ((on ? lime.withAlpha (0.22f) : juce::Colour (0xff05060a)).withAlpha (alpha));
    g.fillRoundedRectangle (sw, 9.0f);
    g.setColour ((on ? lime : (highlighted ? violet : border)).withAlpha (alpha));
    g.drawRoundedRectangle (sw, 9.0f, 1.3f);
    if (on)
        drawGlowCircle (g, { sw.getRight() - 15.0f, sw.getY() + 1.0f, 14.0f, 14.0f }, lime, 5.0f, 0.5f * alpha);
    g.setColour ((on ? lime : disabled).withAlpha (alpha));
    g.fillEllipse (on ? sw.getRight() - 15.0f : sw.getX() + 3.0f, sw.getY() + 2.0f, 14.0f, 14.0f);
    g.setColour ((on ? textBright : textDim).withAlpha (alpha));
    g.setFont (fontOf (juce::jlimit (11.0f, 14.0f, (float) b.getHeight() * 0.4f), true));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().withTrimmedLeft (48), juce::Justification::centredLeft, 1, 0.8f);
    if (b.hasKeyboardFocus (true))
    {
        g.setColour (chrome);
        g.drawRoundedRectangle (sw.expanded (2.0f), 11.0f, 1.2f);
    }
}

void PlaygroundLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider& s)
{
    drawLedDial (g, juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f), startAngle, endAngle, pos,
                 s.findColour (juce::Slider::rotarySliderFillColourId), s.isEnabled() ? 1.0f : 0.4f, s.isMouseOverOrDragging() ? 1.0f : 0.0f, s.hasKeyboardFocus (false));
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
