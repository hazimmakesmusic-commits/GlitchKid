#include "PlaygroundWidgets.h"
#include <BinaryData.h>
#include "../Presets/FactoryPresets.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <complex>

namespace glitchkid::ui
{
juce::Path chamferPath (juce::Rectangle<float> r, float c)
{
    c = juce::jmin (c, r.getWidth() * 0.5f, r.getHeight() * 0.5f);
    juce::Path p;
    p.startNewSubPath (r.getX() + c, r.getY());
    p.lineTo (r.getRight(), r.getY());
    p.lineTo (r.getRight(), r.getBottom() - c);
    p.lineTo (r.getRight() - c, r.getBottom());
    p.lineTo (r.getX(), r.getBottom());
    p.lineTo (r.getX(), r.getY() + c);
    p.closeSubPath();
    return p;
}

void drawGlow (juce::Graphics& g, const juce::Path& shape, juce::Colour c, float radius)
{
    // A cheap glow (no blur, so it is safe to draw on many controls every frame): a few wide, faint strokes of the outline, widest and faintest first.
    constexpr int steps = 4;
    for (int i = steps; i >= 1; --i)
    {
        g.setColour (c.withMultipliedAlpha (0.30f / (float) i));
        g.strokePath (shape, juce::PathStrokeType (1.0f + radius * (float) i / (float) steps * 1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

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
    const auto box = juce::Rectangle<float> (d, d).withCentre (dial.getCentre()).reduced (2.0f);
    const auto c = box.getCentre();
    const float R = box.getWidth() * 0.5f;
    pos = juce::jlimit (0.0f, 1.0f, pos);
    auto fillGradient = [&] (const juce::ColourGradient& grad) { g.setGradientFill (grad); g.setOpacity (alpha); };

    // 1. the bezel: dark machined metal lit from the top left, with a thin rim light
    fillGradient (juce::ColourGradient (juce::Colour (0xff342a40), c.x - R * 0.7f, c.y - R * 0.9f, juce::Colour (0xff09070d), c.x + R * 0.7f, c.y + R * 0.95f, false));
    g.fillEllipse (box);
    {
        juce::Path rim;
        rim.addCentredArc (c.x, c.y, R - 0.5f, R - 0.5f, 0.0f, -2.5f, -0.6f, true);
        g.setColour (chrome.withAlpha (0.20f * alpha));
        g.strokePath (rim, juce::PathStrokeType (1.2f));
        g.setColour (juce::Colour (0xff050308).withAlpha (alpha));
        g.drawEllipse (box, 1.0f);
    }

    // 2. the groove and the value arc, with a glow only where the control really is turned up
    const float trackR = R * 0.80f, thick = juce::jmax (2.6f, R * 0.115f);
    juce::Path track, value;
    track.addCentredArc (c.x, c.y, trackR, trackR, 0.0f, start, end, true);
    g.setColour (juce::Colour (0xff060409).withAlpha (alpha));
    g.strokePath (track, juce::PathStrokeType (thick + 1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (border.brighter (0.1f).withAlpha (alpha));
    g.strokePath (track, juce::PathStrokeType (thick * 0.55f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (pos > 0.004f)
    {
        value.addCentredArc (c.x, c.y, trackR, trackR, 0.0f, start, start + (end - start) * pos, true);
        g.setColour (accent.withAlpha ((hover ? 0.34f : 0.22f) * alpha));
        g.strokePath (value, juce::PathStrokeType (thick + 5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (accent.withAlpha (alpha));
        g.strokePath (value, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (accent.brighter (0.6f).withAlpha (0.9f * alpha));
        g.strokePath (value, juce::PathStrokeType (thick * 0.32f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // 3. the cap: dark brushed chrome with a specular crescent
    const float capR = R * 0.58f;
    fillGradient (juce::ColourGradient (juce::Colour (0xff564a66), c.x - capR * 0.3f, c.y - capR * 0.55f, juce::Colour (0xff0f0b15), c.x + capR * 0.5f, c.y + capR * 0.9f, false));
    g.fillEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);
    g.setColour (juce::Colour (0xff050308).withAlpha (alpha));
    g.drawEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f, 1.0f);
    {
        juce::Path crescent;
        crescent.addCentredArc (c.x, c.y, capR * 0.84f, capR * 0.84f, 0.0f, -2.7f, -1.0f, true);
        g.setColour (chrome.withAlpha (0.26f * alpha));
        g.strokePath (crescent, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // 4. the indicator: a lit notch from the middle of the cap to its edge
    const float a = start + (end - start) * pos;
    const auto dir = juce::Point<float> (std::sin (a), -std::cos (a));
    const auto from = c + dir * (capR * 0.30f), to = c + dir * (capR * 0.90f);
    const auto notch = accent.interpolatedWith (chrome, 0.6f);
    g.setColour (accent.withAlpha (0.35f * alpha));
    g.drawLine (juce::Line<float> (from, to), 4.5f);
    g.setColour (notch.withAlpha (alpha));
    g.drawLine (juce::Line<float> (from, to), 2.2f);

    if (hover)
    {
        g.setColour (accent.withAlpha (0.45f * alpha));
        g.drawEllipse (box.expanded (1.0f), 1.2f);
    }
    if (focus)
        focusRing (g, box, R);
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
    g.fillPath (chamferPath (r, 4.0f));
    g.setColour (selected ? violet : (highlighted ? border.brighter (0.4f) : border));
    g.strokePath (chamferPath (r, 4.0f), juce::PathStrokeType (selected ? 1.6f : 1.0f));
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
        g.setColour (pink);
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
    const auto base = selected ? violet : (sharp ? juce::Colour (0xff0c0910) : juce::Colour (0xff1d1626));
    g.setColour (down || highlighted ? base.brighter (0.15f) : base);
    g.fillPath (chamferPath (r, 4.0f));
    g.setColour (selected ? neon : (highlighted ? border.brighter (0.4f) : border));
    g.strokePath (chamferPath (r, 4.0f), juce::PathStrokeType (selected ? 1.5f : 1.0f));
    if (target) // the note Pitch Tune is pulling the voice to right now (real detector output)
    {
        g.setColour (pink);
        g.strokePath (chamferPath (r.expanded (1.0f), 4.0f), juce::PathStrokeType (2.0f));
    }
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
    const float v = peakToLevel (peak);
    const bool wasEmpty = nonZero == 0;
    nonZero += (v > 0.001f ? 1 : 0) - (bars[(size_t) head] > 0.001f ? 1 : 0);
    bars[(size_t) head] = v;
    head = (head + 1) % numBars;
    if (nonZero > 0 || ! wasEmpty) // an empty strip that stays empty looks identical: no repaint
        repaint();
}

void ScopeStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0c0910));
    g.fillPath (chamferPath (r, 4.0f));
    g.setColour (border);
    g.strokePath (chamferPath (r.reduced (0.5f), 4.0f), juce::PathStrokeType (1.0f));
    g.setColour (border.withAlpha (0.7f));
    g.fillRect (r.getX() + 4.0f, r.getCentreY(), r.getWidth() - 8.0f, 1.0f);
    const float step = (r.getWidth() - 8.0f) / (float) numBars;
    for (int i = 0; i < numBars; ++i)
    {
        const float v = bars[(size_t) ((head + i) % numBars)]; // oldest on the left, newest on the right
        const float h = juce::jmax (1.5f, v * (r.getHeight() - 8.0f));
        g.setColour (violet.interpolatedWith (pink, v * v).withAlpha (0.3f + 0.6f * v));
        g.fillRect (r.getX() + 4.0f + (float) i * step, r.getCentreY() - h * 0.5f, juce::jmax (1.2f, step - 1.0f), h);
    }
}

// ---- ReductionBar ----------------------------------------------------------------------------------------------------------------------

void ReductionBar::push (float db)
{
    db = juce::jlimit (0.0f, maxReduction, std::isfinite (db) ? db : 0.0f);
    const float next = db > shown ? db : juce::jmax (db, shown - 0.4f); // falls about 12 dB/s at 30 Hz
    if (! juce::approximatelyEqual (next, shown))
    {
        shown = next;
        repaint();
    }
}

void ReductionBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0c0910));
    g.fillPath (chamferPath (r, 4.0f));
    g.setColour (border);
    g.strokePath (chamferPath (r.reduced (0.5f), 4.0f), juce::PathStrokeType (1.0f));
    g.setColour (pink);
    g.fillRoundedRectangle (r.withWidth (r.getWidth() * shown / maxReduction).reduced (2.0f, 2.0f), 2.0f);
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
    const auto r = getLocalBounds().toFloat().reduced (1.5f);
    const auto shape = chamferPath (r, 10.0f);
    const bool solid = state == AutoState::idle || state == AutoState::ready;
    g.setColour ((solid ? (down ? violet.darker (0.35f) : highlighted ? violet.brighter (0.12f) : violet) : juce::Colour (0xff1f0d2b)).withAlpha (alpha));
    g.fillPath (shape);
    {
        const juce::Graphics::ScopedSaveState keep (g);
        g.reduceClipRegion (shape);
        if (state == AutoState::working)
        {
            g.setColour (violet.withAlpha (0.92f * alpha));
            g.fillRect (r.withWidth (r.getWidth() * juce::jmax (0.02f, progress)));
            const float x = r.getX() + r.getWidth() * juce::jmax (0.02f, progress); // the scan line sits on the real analysis progress
            g.setColour (pink.withAlpha (0.25f * alpha));
            g.fillRect (x - 5.0f, r.getY(), 5.0f, r.getHeight());
            g.setColour (pink.withAlpha (alpha));
            g.fillRect (x - 1.5f, r.getY(), 2.0f, r.getHeight());
        }
        else if (state == AutoState::waiting)
        {
            const float cx = r.getX() + r.getWidth() * (motion ? sweep : 0.5f);
            juce::ColourGradient band (violet.withAlpha (0.0f), cx - r.getWidth() * 0.16f, 0.0f, violet.withAlpha (0.0f), cx + r.getWidth() * 0.16f, 0.0f, false);
            band.addColour (0.5, pink.withAlpha (0.45f * alpha));
            g.setGradientFill (band);
            g.fillRect (r);
        }
        // a thin pink detail along the bottom edge: short when idle, full width once a mix is applied
        g.setColour (pink.withAlpha ((state == AutoState::error ? 0.0f : 1.0f) * alpha));
        g.fillRect (r.getX(), r.getBottom() - 3.0f, state == AutoState::ready ? r.getWidth() : 34.0f, 3.0f);
    }
    g.setColour ((state == AutoState::error ? errorRed : (highlighted && solid ? pink : (solid ? violet.brighter (0.35f) : violet))).withAlpha (alpha));
    g.strokePath (shape, juce::PathStrokeType (highlighted && solid ? 2.0f : 1.4f));
    if (hasKeyboardFocus (false))
        focusRing (g, r, 8.0f);
    g.setFont (fontOf (juce::jlimit (14.0f, 24.0f, r.getHeight() * 0.3f), true));
    g.setColour ((state == AutoState::error ? errorRed : chrome).withAlpha (alpha));
    g.drawText (getButtonText(), r, juce::Justification::centred, false);
}


// ---- panel and EQ graph ----------------------------------------------------------------------------------------------------------------

void drawPanel (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colour (0xff030206).withAlpha (0.45f)); // a short shadow under the plate
    g.fillPath (chamferPath (r.translated (0.0f, 2.5f), 5.0f));
    g.setGradientFill (juce::ColourGradient (panelTop, 0.0f, r.getY(), panelBottom.darker (0.15f), 0.0f, r.getBottom(), false));
    g.fillPath (chamferPath (r, 5.0f));
    g.setColour (border);
    g.strokePath (chamferPath (r.reduced (0.5f), 5.0f), juce::PathStrokeType (1.0f));
    g.setColour (chrome.withAlpha (0.07f)); // the lit top edge
    g.fillRect (r.getX() + 6.0f, r.getY() + 1.0f, r.getWidth() - 12.0f, 1.0f);
    g.setColour (violet);
    const float L = 12.0f;
    g.fillRect (r.getX(), r.getY(), L, 2.0f);
    g.fillRect (r.getX(), r.getY(), 2.0f, L);
    g.fillRect (r.getRight() - L, r.getBottom() - 2.0f, L, 2.0f);
    g.fillRect (r.getRight() - 2.0f, r.getBottom() - L, 2.0f, L);
}

namespace
{
constexpr float eqMinHz = 20.0f, eqMaxHz = 20000.0f, eqRangeDb = 15.0f;
const char* const eqFreqIds[4] = { "eq_lo_freq", "eq_b1_freq", "eq_b2_freq", "eq_hi_freq" };
const char* const eqGainIds[4] = { "eq_lo_gain", "eq_b1_gain", "eq_b2_gain", "eq_hi_gain" };
const char* const eqQIds[4] = { nullptr, "eq_b1_q", "eq_b2_q", nullptr };
const char* const eqNames[4] = { "LOW", "BODY", "PRESENCE", "AIR" };
}

EqGraph::EqGraph (juce::AudioProcessorValueTreeState& a, std::function<double()> sr) : apvts (a), sampleRate (std::move (sr))
{
    setTooltip ("The EQ response, calculated from your four EQ bands. Drag a point to change its frequency and gain, use the mouse wheel to change a bell's Q, double-click a point to flatten it. There is no live spectrum.");
}

void EqGraph::setSelectedBand (int b)
{
    b = juce::jlimit (0, numBands - 1, b);
    if (b != selected)
    {
        selected = b;
        repaint();
    }
}

void EqGraph::readParams (float* f, float* g, float* q) const
{
    for (int i = 0; i < numBands; ++i)
    {
        f[i] = apvts.getRawParameterValue (eqFreqIds[i])->load();
        g[i] = apvts.getRawParameterValue (eqGainIds[i])->load();
        q[i] = eqQIds[i] != nullptr ? apvts.getRawParameterValue (eqQIds[i])->load() : 0.7071f; // shelves use a fixed slope
    }
}

// RBJ Audio EQ Cookbook, the same formulas as ParametricEq::updateCoefficients; |H(e^jw)| in dB at one frequency, summed over the bands.
float EqGraph::responseDb (const float freq[numBands], const float gain[numBands], const float q[numBands], float atHz, double fs)
{
    double total = 0.0;
    const double w = 2.0 * juce::MathConstants<double>::pi * (double) atHz / fs;
    const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
    for (int i = 0; i < numBands; ++i)
    {
        if (std::abs (gain[i]) < 0.05f) // the DSP skips a band within 0.05 dB of flat
            continue;
        const double f = juce::jmin ((double) juce::jlimit (10.0f, 20000.0f, freq[i]), 0.45 * fs);
        const double A = std::pow (10.0, (double) gain[i] / 40.0);
        const double w0 = 2.0 * juce::MathConstants<double>::pi * f / fs, cosw = std::cos (w0), sinw = std::sin (w0);
        double b0, b1, b2, a0, a1, a2;
        if (i == 1 || i == 2)
        {
            const double alpha = sinw / (2.0 * (double) juce::jlimit (0.1f, 20.0f, q[i]));
            b0 = 1 + alpha * A;  b1 = -2 * cosw;  b2 = 1 - alpha * A;
            a0 = 1 + alpha / A;  a1 = -2 * cosw;  a2 = 1 - alpha / A;
        }
        else
        {
            const double alpha = sinw / (2.0 * 0.7071), k = 2.0 * std::sqrt (A) * alpha;
            if (i == 0)
            {
                b0 = A * ((A + 1) - (A - 1) * cosw + k);  b1 = 2 * A * ((A - 1) - (A + 1) * cosw);  b2 = A * ((A + 1) - (A - 1) * cosw - k);
                a0 = (A + 1) + (A - 1) * cosw + k;        a1 = -2 * ((A - 1) + (A + 1) * cosw);     a2 = (A + 1) + (A - 1) * cosw - k;
            }
            else
            {
                b0 = A * ((A + 1) + (A - 1) * cosw + k);  b1 = -2 * A * ((A - 1) + (A + 1) * cosw); b2 = A * ((A + 1) + (A - 1) * cosw - k);
                a0 = (A + 1) - (A - 1) * cosw + k;        a1 = 2 * ((A - 1) - (A + 1) * cosw);      a2 = (A + 1) - (A - 1) * cosw - k;
            }
        }
        const auto num = b0 + b1 * z1 + b2 * z2, den = a0 + a1 * z1 + a2 * z2;
        total += 20.0 * std::log10 (std::abs (num) / std::abs (den));
    }
    return (float) total;
}

float EqGraph::xOf (float hz) const { return plot.getX() + plot.getWidth() * std::log (juce::jlimit (eqMinHz, eqMaxHz, hz) / eqMinHz) / std::log (eqMaxHz / eqMinHz); }
float EqGraph::yOf (float db) const { return plot.getCentreY() - juce::jlimit (-eqRangeDb, eqRangeDb, db) / eqRangeDb * plot.getHeight() * 0.5f; }
float EqGraph::hzAt (float x) const { return eqMinHz * std::exp (juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / plot.getWidth()) * std::log (eqMaxHz / eqMinHz)); }
float EqGraph::dbAt (float y) const { return juce::jlimit (-eqRangeDb, eqRangeDb, (plot.getCentreY() - y) / (plot.getHeight() * 0.5f) * eqRangeDb); }

void EqGraph::resized()
{
    plot = getLocalBounds().toFloat().withTrimmedLeft (34.0f).withTrimmedBottom (20.0f).withTrimmedTop (8.0f).withTrimmedRight (10.0f);
    haveCurve = false;
}

juce::Point<float> EqGraph::pointOf (int band) const
{
    float f[numBands], g[numBands], q[numBands];
    readParams (f, g, q);
    return { xOf (f[band]), yOf (g[band]) };
}

void EqGraph::rebuildCurve()
{
    float f[numBands], g[numBands], q[numBands];
    readParams (f, g, q);
    const double fs = sampleRate ? sampleRate() : 48000.0;
    curve.clear();
    const int n = juce::jmax (8, (int) plot.getWidth() / 2);
    for (int i = 0; i <= n; ++i)
    {
        const float x = plot.getX() + plot.getWidth() * (float) i / (float) n;
        const float y = yOf (responseDb (f, g, q, hzAt (x), fs > 1000.0 ? fs : 48000.0));
        if (i == 0)
            curve.startNewSubPath (x, y);
        else
            curve.lineTo (x, y);
    }
    for (int i = 0; i < numBands; ++i)
    {
        shownF[i] = f[i];
        shownG[i] = g[i];
        shownQ[i] = q[i];
    }
    haveCurve = true;
}

namespace
{
std::complex<double> peakingH (double f, double q, double gainDb, double atHz, double fs)
{
    const double A = std::pow (10.0, gainDb / 40.0), w0 = 2.0 * juce::MathConstants<double>::pi * juce::jmin (f, 0.45 * fs) / fs, alpha = std::sin (w0) / (2.0 * q), c = std::cos (w0);
    const double b0 = 1 + alpha * A, b1 = -2 * c, b2 = 1 - alpha * A, a0 = 1 + alpha / A, a1 = -2 * c, a2 = 1 - alpha / A;
    const double w = 2.0 * juce::MathConstants<double>::pi * atHz / fs;
    const auto z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
    return (b0 + b1 * z1 + b2 * z2) / (a0 + a1 * z1 + a2 * z2);
}

std::complex<double> highpassH (double f, double q, double atHz, double fs)
{
    const double w0 = 2.0 * juce::MathConstants<double>::pi * f / fs, c = std::cos (w0), alpha = std::sin (w0) / (2.0 * q), a0 = 1 + alpha;
    const double b0 = (1 + c) * 0.5, b1 = -(1 + c), b2 = b0, a1 = -2 * c, a2 = 1 - alpha;
    const double w = 2.0 * juce::MathConstants<double>::pi * atHz / fs;
    const auto z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
    return (b0 + b1 * z1 + b2 * z2) / (a0 + a1 * z1 + a2 * z2);
}
}

// The same filters as CleanLab (rumble: x + mix * (highpass(x) - x); bells: RBJ peaking), multiplied: this is what the corrective stage does to a steady tone.
float EqGraph::cleanResponseDb (const CleanLab::Applied& a, float atHz, double fs)
{
    std::complex<double> h (1.0, 0.0);
    if (a.rumbleAmount > 0.001f)
        h *= 1.0 + (double) a.rumbleAmount * (highpassH (70.0, 0.7071, atHz, fs) - 1.0);
    if (a.mudDb > 0.01f)
        h *= peakingH (a.mudHz, a.mudQ, -a.mudDb, atHz, fs);
    for (int i = 0; i < CleanLab::numRes; ++i)
        if (a.resDb[i] > 0.01f)
            h *= peakingH (a.resHz[i], a.resQ, -a.resDb[i], atHz, fs);
    return (float) (20.0 * std::log10 (std::abs (h) + 1.0e-12));
}

void EqGraph::setSpectrum (const float* levels, int n, float topHz)
{
    const bool had = ! spec.empty();
    if (levels == nullptr || n <= 1)
    {
        spec.clear();
        if (had)
            repaint();
        return;
    }
    if ((int) spec.size() == n && juce::approximatelyEqual (specTop, topHz) && std::equal (spec.begin(), spec.end(), levels))
        return; // nothing changed: nothing to redraw
    spec.assign (levels, levels + n);
    specTop = topHz;
    repaint();
}

void EqGraph::setCleanApplied (const CleanLab::Applied& a)
{
    clean = a;
    refresh();
}

void EqGraph::refresh()
{
    {
        bool changed = ! haveClean || ! juce::approximatelyEqual (clean.rumbleAmount, shownClean.rumbleAmount) || ! juce::approximatelyEqual (clean.mudDb, shownClean.mudDb)
                       || ! juce::approximatelyEqual (clean.mudHz, shownClean.mudHz);
        for (int i = 0; i < CleanLab::numRes; ++i)
            changed = changed || ! juce::approximatelyEqual (clean.resDb[i], shownClean.resDb[i]) || ! juce::approximatelyEqual (clean.resHz[i], shownClean.resHz[i]);
        if (changed && ! plot.isEmpty())
        {
            cleanCurve.clear();
            const double fsr = sampleRate ? sampleRate() : 48000.0;
            const int n = juce::jmax (8, (int) plot.getWidth() / 2);
            for (int i = 0; i <= n; ++i)
            {
                const float x = plot.getX() + plot.getWidth() * (float) i / (float) n;
                const float y = yOf (cleanResponseDb (clean, hzAt (x), fsr > 1000.0 ? fsr : 48000.0));
                if (i == 0)
                    cleanCurve.startNewSubPath (x, y);
                else
                    cleanCurve.lineTo (x, y);
            }
            shownClean = clean;
            haveClean = true;
            repaint();
        }
    }
    float f[numBands], g[numBands], q[numBands];
    readParams (f, g, q);
    bool changed = ! haveCurve;
    for (int i = 0; i < numBands; ++i)
        changed = changed || ! juce::exactlyEqual (f[i], shownF[i]) || ! juce::exactlyEqual (g[i], shownG[i]) || ! juce::exactlyEqual (q[i], shownQ[i]);
    if (changed)
    {
        rebuildCurve();
        repaint();
    }
}

void EqGraph::paint (juce::Graphics& g)
{
    if (! haveCurve)
        rebuildCurve();
    g.setColour (juce::Colour (0xff0c0910));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);
    g.setColour (border);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 3.0f, 1.0f);
    g.reduceClipRegion (getLocalBounds().reduced (1));
    // grid and labels
    g.setFont (fontOf (10.5f));
    for (float db : { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f })
    {
        const float y = yOf (db);
        g.setColour (juce::approximatelyEqual (db, 0.0f) ? border.brighter (0.5f) : border.withAlpha (0.8f));
        g.fillRect (plot.getX(), y, plot.getWidth(), 1.0f);
        g.setColour (textDim);
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), juce::Rectangle<float> (2.0f, y - 7.0f, 28.0f, 14.0f), juce::Justification::centredRight, false);
    }
    for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xOf (hz);
        g.setColour (border.withAlpha (0.8f));
        g.fillRect (x, plot.getY(), 1.0f, plot.getHeight());
        g.setColour (textDim);
        g.drawText (hz >= 1000.0f ? juce::String (hz / 1000.0f, hz < 10000.0f && ! juce::approximatelyEqual (std::fmod (hz, 1000.0f), 0.0f) ? 1 : 0) + "k" : juce::String ((int) hz),
                    juce::Rectangle<float> (x - 18.0f, plot.getBottom() + 3.0f, 36.0f, 14.0f), juce::Justification::centred, false);
    }
    // the real spectrum of the vocal input (only while a signal arrives)
    if (spec.size() > 1)
    {
        juce::Path sp;
        sp.startNewSubPath (xOf (20.0f), plot.getBottom());
        for (size_t i = 0; i < spec.size(); ++i)
        {
            const float hz = 20.0f * std::pow (specTop / 20.0f, (float) i / (float) (spec.size() - 1));
            sp.lineTo (xOf (hz), plot.getBottom() - plot.getHeight() * 0.85f * juce::jlimit (0.0f, 1.0f, spec[i]));
        }
        sp.lineTo (xOf (specTop), plot.getBottom());
        sp.closeSubPath();
        g.setColour (violet.withAlpha (0.13f));
        g.fillPath (sp);
        g.setColour (violet.withAlpha (0.45f));
        g.strokePath (sp, juce::PathStrokeType (1.0f));
    }
    // the response: filled toward the 0 dB line, then the curve itself
    juce::Path fill (curve);
    fill.lineTo (plot.getRight(), plot.getCentreY());
    fill.lineTo (plot.getX(), plot.getCentreY());
    fill.closeSubPath();
    g.setColour (violet.withAlpha (0.16f));
    g.fillPath (fill);
    g.setColour (violet.withAlpha (0.28f));
    g.strokePath (curve, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); // a restrained glow under the line
    g.setColour (neon);
    g.strokePath (curve, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // CLEAN LAB's applied corrections (real, from the DSP's own smoothed values), only while any is applied
    if (shownClean.mudDb > 0.05f || shownClean.rumbleAmount > 0.02f || shownClean.resDb[0] > 0.05f || shownClean.resDb[1] > 0.05f || shownClean.resDb[2] > 0.05f)
    {
        g.setColour (pink.withAlpha (0.95f));
        g.strokePath (cleanCurve, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setFont (fontOf (10.5f, true));
        g.drawText ("CLEAN LAB", juce::Rectangle<float> (plot.getRight() - 80.0f, plot.getY() + 2.0f, 78.0f, 13.0f), juce::Justification::centredRight, false);
    }
    // the four points
    for (int i = 0; i < numBands; ++i)
    {
        const auto p = juce::Point<float> (xOf (shownF[i]), yOf (shownG[i]));
        const bool sel = i == selected, hot = i == dragging || i == hovered;
        g.setColour (obsidian);
        g.fillEllipse (p.x - 7.0f, p.y - 7.0f, 14.0f, 14.0f);
        g.setColour (sel ? pink : (hot ? neon : violet));
        g.drawEllipse (p.x - 7.0f, p.y - 7.0f, 14.0f, 14.0f, sel ? 2.4f : 1.6f);
        g.setColour (sel ? pink : violet);
        g.setFont (fontOf (10.5f, true));
        g.drawText (eqNames[i], juce::Rectangle<float> (p.x - 30.0f, p.y + (shownG[i] > 0.05f ? -22.0f : 9.0f), 60.0f, 13.0f), juce::Justification::centred, false);
    }
}

int EqGraph::bandNear (juce::Point<float> p) const
{
    int best = -1;
    float bestD = 16.0f * 16.0f;
    for (int i = 0; i < numBands; ++i)
    {
        const auto c = pointOf (i);
        const float d = c.getDistanceSquaredFrom (p);
        if (d < bestD)
        {
            bestD = d;
            best = i;
        }
    }
    return best;
}

void EqGraph::setParam (const char* id, float value)
{
    if (auto* prm = apvts.getParameter (id))
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
}

void EqGraph::mouseDown (const juce::MouseEvent& e)
{
    dragging = bandNear (e.position);
    if (dragging >= 0)
    {
        selected = dragging;
        if (onBandSelected)
            onBandSelected (selected);
        for (auto* id : { eqFreqIds[dragging], eqGainIds[dragging] })
            if (auto* prm = apvts.getParameter (id))
                prm->beginChangeGesture();
    }
    repaint();
}

void EqGraph::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;
    setParam (eqFreqIds[dragging], hzAt (e.position.x));
    setParam (eqGainIds[dragging], dbAt (e.position.y));
    refresh();
}

void EqGraph::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
        for (auto* id : { eqFreqIds[dragging], eqGainIds[dragging] })
            if (auto* prm = apvts.getParameter (id))
                prm->endChangeGesture();
    dragging = -1;
    repaint();
}

void EqGraph::mouseMove (const juce::MouseEvent& e)
{
    const int h = bandNear (e.position);
    if (h != hovered)
    {
        hovered = h;
        setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void EqGraph::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int b = bandNear (e.position);
    if (b < 0)
        return;
    if (auto* prm = apvts.getParameter (eqGainIds[b]))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (0.0f));
        prm->endChangeGesture();
    }
    refresh();
}

void EqGraph::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int b = bandNear (e.position) >= 0 ? bandNear (e.position) : selected;
    if (eqQIds[b] == nullptr)
        return; // shelves have a fixed slope
    if (auto* prm = apvts.getParameter (eqQIds[b]))
    {
        const float q = apvts.getRawParameterValue (eqQIds[b])->load() * std::pow (1.12f, w.deltaY > 0.0f ? 1.0f : -1.0f);
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (q));
        prm->endChangeGesture();
    }
    refresh();
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
                                    : "The real signal path, left to right. A lit dot means the stage is doing something right now. Click a stage to open it in FINAL TOUCH.");
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
        g.setColour (pink.withAlpha (0.9f));
        g.fillEllipse (x0 + (x1 - x0) * phase - 2.5f, cy - 2.5f, 5.0f, 5.0f);
    }
    for (int i = 0; i < 6; ++i)
    {
        const auto r = nodeBounds (i).toFloat();
        const bool stage = i >= 1 && i <= numStages;
        const bool on = stage && active[(size_t) (i - 1)];
        const bool sel = stage && selected == i - 1;
        if (sel)
            drawGlow (g, chamferPath (r, 4.0f), violet.withAlpha (0.4f), 5.0f);
        g.setGradientFill (juce::ColourGradient (sel ? juce::Colour (0xff35135a) : panelTop.brighter (0.05f), 0.0f, r.getY(), sel ? juce::Colour (0xff1a0a2c) : panelBottom.darker (0.2f), 0.0f, r.getBottom(), false));
        g.fillPath (chamferPath (r, 4.0f));
        g.setColour (chrome.withAlpha (0.07f));
        g.fillRect (r.getX() + 4.0f, r.getY() + 1.0f, r.getWidth() - 8.0f, 1.0f);
        g.setColour (sel ? violet : border);
        g.strokePath (chamferPath (r.reduced (0.5f), 4.0f), juce::PathStrokeType (sel ? 1.5f : 1.0f));
        g.setFont (fontOf (11.5f, true));
        g.setColour (stage && ! on ? textDim : textBright);
        if (stage)
        {
            g.drawText (nodeNames[i], r.withTrimmedLeft (22.0f), juce::Justification::centredLeft, false);
            const auto dot = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ r.getX() + 12.0f, r.getCentreY() });
            if (on)
            {
                g.setColour (pink.withAlpha (0.28f)); // a lit LED: real stage activity only
                g.fillEllipse (dot.expanded (3.5f));
                g.setColour (pink);
                g.fillEllipse (dot);
                g.setColour (chrome.withAlpha (0.7f));
                g.fillEllipse (dot.getX() + 1.5f, dot.getY() + 1.2f, 2.2f, 2.2f);
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
            g.setColour (juce::Colour (0xff0c0910));
            g.fillPath (chamferPath (bar, 4.0f));
            g.setColour (level > 0.98f ? errorRed : violet);
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
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1d1626));
    setColour (juce::TextButton::buttonOnColourId, violet);
    setColour (juce::TextButton::textColourOffId, textBright);
    setColour (juce::TextButton::textColourOnId, chrome);
    setColour (juce::Slider::textBoxTextColourId, textBright);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff0c0910));
    setColour (juce::Slider::textBoxOutlineColourId, border);
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff0c0910));
    setColour (juce::ComboBox::outlineColourId, border.brighter (0.2f));
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff150f1c));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, violet);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff1d1626));
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
    const bool tab = (bool) b.getProperties().getWithDefault ("tab", false);
    const float alpha = b.isEnabled() ? 1.0f : 0.4f;
    auto fill = on ? b.findColour (juce::TextButton::buttonOnColourId) : (tab ? juce::Colour (0xff130f1b) : bg);
    fill = down ? fill.darker (0.2f) : highlighted ? fill.brighter (0.1f) : fill;
    if (down)
        r = r.translated (0.0f, 1.0f);
    const auto shape = chamferPath (r, 4.0f);
    if (on && b.isEnabled() && (highlighted || tab || fill == violet)) // a restrained glow on a selected control
        drawGlow (g, shape, fill.withAlpha (tab ? 0.8f : 0.55f), tab ? 7.0f : 5.0f);
    g.setGradientFill (juce::ColourGradient (fill.brighter (on ? 0.28f : (tab ? 0.06f : 0.16f)).withAlpha (alpha), 0.0f, r.getY(), fill.darker (on ? 0.28f : 0.3f).withAlpha (alpha), 0.0f, r.getBottom(), false));
    g.fillPath (shape);
    g.setColour (chrome.withAlpha ((on ? 0.34f : (tab ? 0.04f : 0.10f)) * alpha)); // the lit top edge
    g.fillRect (r.getX() + 4.0f, r.getY() + 1.0f, r.getWidth() - 8.0f, 1.0f);
    g.setColour ((on ? fill.brighter (0.45f) : border.brighter (highlighted ? 0.45f : 0.12f)).withAlpha (alpha));
    g.strokePath (shape, juce::PathStrokeType (1.0f));
    if (tab && on) // the selected workspace tab carries a lit underline
    {
        g.setGradientFill (juce::ColourGradient (pink, r.getX(), 0.0f, violet, r.getRight(), 0.0f, false));
        g.fillRect (r.getX() + 6.0f, r.getBottom() - 3.0f, r.getWidth() - 12.0f, 2.0f);
    }
    if (b.hasKeyboardFocus (true))
        focusRing (g, r, 5.0f);
}

void PlaygroundLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const float alpha = b.isEnabled() ? 1.0f : 0.4f;
    const bool on = b.getToggleState();
    const auto sw = juce::Rectangle<float> (36.0f, 18.0f).withCentre ({ 21.0f, (float) b.getHeight() * 0.5f });
    const auto shape = chamferPath (sw, 4.0f);
    if (on && b.isEnabled())
        drawGlow (g, shape, violet.withAlpha (0.45f), 5.0f);
    g.setGradientFill (juce::ColourGradient ((on ? juce::Colour (0xff3a1260) : juce::Colour (0xff05030a)).withAlpha (alpha), 0.0f, sw.getY(),
                                             (on ? juce::Colour (0xff22093b) : juce::Colour (0xff120d19)).withAlpha (alpha), 0.0f, sw.getBottom(), false));
    g.fillPath (shape);
    g.setColour ((on ? violet : (highlighted ? border.brighter (0.5f) : border.brighter (0.2f))).withAlpha (alpha));
    g.strokePath (shape, juce::PathStrokeType (1.2f));
    const auto knob = juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ on ? sw.getRight() - 10.0f : sw.getX() + 10.0f, sw.getCentreY() });
    g.setGradientFill (juce::ColourGradient ((on ? chrome : juce::Colour (0xff8a7f9c)).withAlpha (alpha), 0.0f, knob.getY(), (on ? juce::Colour (0xff6a2aa0) : juce::Colour (0xff2c2438)).withAlpha (alpha), 0.0f, knob.getBottom(), false));
    g.fillPath (chamferPath (knob, 3.0f));
    g.setColour (juce::Colour (0xff050308).withAlpha (alpha));
    g.strokePath (chamferPath (knob, 3.0f), juce::PathStrokeType (0.8f));
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
    const bool hot = s.isMouseOverOrDragging();
    const float cy = (float) y + (float) h * 0.5f;
    const auto groove = juce::Rectangle<float> ((float) x, cy - 3.0f, (float) w, 6.0f);
    g.setColour (juce::Colour (0xff060409).withAlpha (alpha)); // an inset groove
    g.fillPath (chamferPath (groove.expanded (0.0f, 1.0f), 3.0f));
    g.setColour (border.withAlpha (alpha));
    g.strokePath (chamferPath (groove.expanded (0.0f, 1.0f), 3.0f), juce::PathStrokeType (0.8f));
    const auto fill = groove.withRight (juce::jmax (groove.getX(), pos)).reduced (0.0f, 1.0f);
    if (fill.getWidth() > 1.0f)
    {
        if (hot)
            drawGlow (g, chamferPath (fill, 2.0f), accent.withAlpha (0.45f), 4.0f);
        g.setGradientFill (juce::ColourGradient (accent.darker (0.5f).withAlpha (alpha), fill.getX(), 0.0f, accent.brighter (0.15f).withAlpha (alpha), fill.getRight(), 0.0f, false));
        g.fillPath (chamferPath (fill, 2.0f));
    }
    // the thumb: a small chrome bar with a hard horizon
    const auto thumb = juce::Rectangle<float> (11.0f, 21.0f).withCentre ({ pos, cy });
    g.setGradientFill (juce::ColourGradient (chrome.withAlpha (alpha), 0.0f, thumb.getY(), juce::Colour (0xff8a7f9c).withAlpha (alpha), 0.0f, thumb.getCentreY(), false));
    g.fillRect (thumb.withBottom (thumb.getCentreY()));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c1426).withAlpha (alpha), 0.0f, thumb.getCentreY(), juce::Colour (0xff5a4c70).withAlpha (alpha), 0.0f, thumb.getBottom(), false));
    g.fillRect (thumb.withTop (thumb.getCentreY()));
    g.setColour ((hot ? accent : juce::Colour (0xff050308)).withAlpha (alpha));
    g.drawRect (thumb, 1.2f);
    g.setColour (juce::Colour (0xff050308).withAlpha (0.55f * alpha));
    g.fillRect (thumb.getCentreX() - 0.5f, thumb.getY() + 5.0f, 1.0f, thumb.getHeight() - 10.0f);
    if (s.hasKeyboardFocus (false))
        focusRing (g, thumb, 3.0f);
}
}
