#include "PlaygroundWidgets.h"
#include <BinaryData.h>
#include "../Presets/FactoryPresets.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <complex>

namespace glitchkid::ui
{
juce::Path chamferPath (juce::Rectangle<float> r, float c)
{
    juce::Path p;
    p.addRoundedRectangle (r, juce::jlimit (2.0f, 6.0f, c * 0.5f + 1.5f));
    return p;
}

juce::String displayCase (const juce::String& text, bool sentence)
{
    static const char* small[] = { "OF", "TO", "THE", "AND", "OR", "A", "AN", "ON", "AT", "VS" };
    bool firstWord = true, afterStop = false;
    static const char* keep[] = { "EQ", "A/B", "FX", "LFO", "BPM", "MIDI", "CPU", "DSP", "VST3", "UI", "OK", "AI", "IO", "ADSR", "MS", "HZ", "DB" };
    juce::String out;
    int i = 0;
    const int n = text.length();
    while (i < n)
    {
        if (! juce::CharacterFunctions::isLetter (text[i]))
        {
            if (juce::CharacterFunctions::isDigit (text[i]))
                firstWord = false; // a leading number counts as the first word
            if (text[i] == '.' || text[i] == '!' || text[i] == '?')
                afterStop = true; // the next word starts a sentence
            out << text[i++];
            continue;
        }
        int j = i;
        while (j < n && (juce::CharacterFunctions::isLetterOrDigit (text[j]) || text[j] == '\'' || text[j] == '/'))
            ++j;
        const auto word = text.substring (i, j);
        bool allCaps = word.length() >= 2 && word == word.toUpperCase() && word.containsAnyOf ("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
        for (auto* k : keep)
            if (word.equalsIgnoreCase (k) && word == word.toUpperCase())
                allCaps = false;
        // units after a number ("-18 dB") are already mixed-case in the code; a single capital letter (a key name such as C or F#) is left alone
        if (allCaps)
        {
            bool lower = sentence && ! firstWord && ! afterStop;
            for (auto* k : small)
                if (word == k && ! firstWord)
                    lower = true;
            out << (lower ? word.toLowerCase() : word.substring (0, 1) + word.substring (1).toLowerCase());
        }
        else
            out << word;
        firstWord = false;
        afterStop = false;
        i = j;
    }
    return out;
}

void drawGlow (juce::Graphics&, const juce::Path&, juce::Colour, float) {} // intentionally empty: no glow in this design

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

    // the track and the value arc: one thin stroke each
    const float trackR = R * 0.86f, thick = juce::jmax (2.5f, R * 0.10f);
    juce::Path track;
    track.addCentredArc (c.x, c.y, trackR, trackR, 0.0f, start, end, true);
    g.setColour (border.withAlpha (alpha));
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (pos > 0.004f)
    {
        juce::Path value;
        value.addCentredArc (c.x, c.y, trackR, trackR, 0.0f, start, start + (end - start) * pos, true);
        g.setColour ((hover ? accent.brighter (0.15f) : accent).withAlpha (alpha));
        g.strokePath (value, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // the cap: a flat raised disc with a thin edge and a top highlight
    const float capR = R * 0.62f;
    g.setGradientFill (juce::ColourGradient (panelTop.brighter (hover ? 0.25f : 0.12f).withAlpha (alpha), c.x, c.y - capR, panelBottom.withAlpha (alpha), c.x, c.y + capR, false));
    g.fillEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);
    g.setColour (border.brighter (0.25f).withAlpha (alpha));
    g.drawEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f, 1.0f);

    // the indicator: a short line from the middle of the cap to its edge
    const float a = start + (end - start) * pos;
    const auto dir = juce::Point<float> (std::sin (a), -std::cos (a));
    g.setColour (textBright.withAlpha (alpha));
    g.drawLine (juce::Line<float> (c + dir * (capR * 0.35f), c + dir * (capR * 0.88f)), 2.2f);

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
    g.drawFittedText (displayCase (info.name), area.removeFromTop (area.getHeight() * 0.5f).toNearestInt(), juce::Justification::bottomLeft, 1, 0.85f);
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
    const auto base = selected ? violet : (sharp ? juce::Colour (0xff0d0d11) : juce::Colour (0xff25222d));
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
    g.setColour (juce::Colour (0xff0d0d11));
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
    g.setColour (juce::Colour (0xff0d0d11));
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
    g.drawText (displayCase (getButtonText()), r, juce::Justification::centred, false);
}


// ---- panel and EQ graph ----------------------------------------------------------------------------------------------------------------

void drawPanel (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (panelBottom);
    g.fillPath (chamferPath (r, 5.0f));
    g.setColour (border);
    g.strokePath (chamferPath (r.reduced (0.5f), 5.0f), juce::PathStrokeType (1.0f));
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
    setTooltip ("The EQ response, calculated from your four EQ bands. Drag a point to change its frequency and gain, use the mouse wheel to change a bell's Q, double-click a point to flatten it. The curve is calculated from your settings; the shaded area behind it is the vocal's real spectrum while audio plays.");
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
    g.setColour (juce::Colour (0xff0d0d11));
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
        g.drawText ("Clean Lab", juce::Rectangle<float> (plot.getRight() - 80.0f, plot.getY() + 2.0f, 78.0f, 13.0f), juce::Justification::centredRight, false);
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

// ---- PlaygroundLookAndFeel -------------------------------------------------------------------------------------------------------------

PlaygroundLookAndFeel::PlaygroundLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, obsidian);
    setColour (juce::Label::textColourId, textBright);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff25222d));
    setColour (juce::TextButton::buttonOnColourId, violet);
    setColour (juce::TextButton::textColourOffId, textBright);
    setColour (juce::TextButton::textColourOnId, chrome);
    setColour (juce::Slider::textBoxTextColourId, textBright);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff0d0d11));
    setColour (juce::Slider::textBoxOutlineColourId, border);
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff0d0d11));
    setColour (juce::ComboBox::outlineColourId, border.brighter (0.2f));
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1b1921));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, violet);
}

void PlaygroundLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    if ((bool) b.getProperties().getWithDefault ("rail", false))
    {
        // a signal-rail row: an optional status dot (1 = stage idle, 2 = stage really processing) and the workspace name
        const int led = (int) b.getProperties().getWithDefault ("led", 0);
        const auto r = b.getLocalBounds().toFloat();
        if (led > 0)
        {
            const auto dot = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ r.getX() + 16.0f, r.getCentreY() });
            if (led == 2)
            {
                g.setColour (neon);
                g.fillEllipse (dot);
            }
            else
            {
                g.setColour (textDim.withAlpha (0.7f));
                g.drawEllipse (dot.reduced (0.5f), 1.0f);
            }
        }
        g.setFont (fontOf (13.0f, true));
        g.setColour (b.getToggleState() ? textBright : textDim.brighter (0.3f));
        g.drawText (b.getProperties().getWithDefault ("railText", displayCase (b.getButtonText())).toString(), r.withTrimmedLeft (32.0f), juce::Justification::centredLeft, false);
        return;
    }
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId).withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.5f));
    const auto text = (bool) b.getProperties().getWithDefault ("rawText", false) ? b.getButtonText() : displayCase (b.getButtonText());
    g.drawFittedText (text, b.getLocalBounds().reduced (6, juce::jmin (4, b.proportionOfHeight (0.3f))), juce::Justification::centred, 2);
}

namespace
{
// Text with a lowercase word of three letters or more is a sentence ("Vocal INPUT before processing"): only its first word gets a capital.
bool isSentence (const juce::String& t)
{
    for (const auto& w : juce::StringArray::fromTokens (t, " ,.:;()", ""))
        if (w.length() >= 3 && w == w.toLowerCase() && w.containsOnly ("abcdefghijklmnopqrstuvwxyz"))
            return true;
    return false;
}
}

void PlaygroundLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& l)
{
    g.fillAll (l.findColour (juce::Label::backgroundColourId));
    if (! l.isBeingEdited())
    {
        const float alpha = l.isEnabled() ? 1.0f : 0.5f;
        const auto font = getLabelFont (l);
        g.setColour (l.findColour (juce::Label::textColourId).withMultipliedAlpha (alpha));
        g.setFont (font);
        const bool raw = l.isEditable() || (bool) l.getProperties().getWithDefault ("rawText", false) || dynamic_cast<juce::ComboBox*> (l.getParentComponent()) != nullptr
                         || dynamic_cast<juce::Slider*> (l.getParentComponent()) != nullptr;
        const auto textArea = getLabelBorderSize (l).subtractedFrom (l.getLocalBounds());
        g.drawFittedText (raw ? l.getText() : displayCase (l.getText(), isSentence (l.getText())), textArea, l.getJustificationType(), juce::jmax (1, (int) ((float) textArea.getHeight() / font.getHeight())), l.getMinimumHorizontalScale());
        g.setColour (l.findColour (juce::Label::outlineColourId).withMultipliedAlpha (alpha));
    }
    else if (l.isEnabled())
        g.setColour (l.findColour (juce::Label::outlineColourId));
    g.drawRect (l.getLocalBounds());
}

juce::Font PlaygroundLookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return fontOf (juce::jlimit (12.0f, 16.0f, (float) h * 0.4f), true); }
juce::Font PlaygroundLookAndFeel::getLabelFont (juce::Label& l) { return fontOf (l.getFont().getHeight(), l.getFont().isBold()); }
juce::Font PlaygroundLookAndFeel::getComboBoxFont (juce::ComboBox& b) { return fontOf (juce::jlimit (12.0f, 15.0f, (float) b.getHeight() * 0.4f), false); }
juce::Font PlaygroundLookAndFeel::getPopupMenuFont() { return fontOf (14.0f, false); }

void PlaygroundLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool highlighted, bool down)
{
    if ((bool) b.getProperties().getWithDefault ("rail", false))
    {
        const auto r = b.getLocalBounds().toFloat();
        const bool sel = b.getToggleState();
        if (sel || highlighted || down)
        {
            g.setColour ((sel ? panelTop : panelTop.withAlpha (0.55f)).withMultipliedAlpha (down ? 0.8f : 1.0f));
            g.fillRoundedRectangle (r.reduced (0.0f, 1.0f), 5.0f);
        }
        if (sel)
        {
            g.setColour (violet);
            g.fillRoundedRectangle (juce::Rectangle<float> (r.getX(), r.getY() + 5.0f, 3.0f, r.getHeight() - 10.0f), 1.5f);
        }
        if (b.hasKeyboardFocus (true))
            focusRing (g, r.reduced (1.0f), 5.0f);
        return;
    }
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const bool tab = (bool) b.getProperties().getWithDefault ("tab", false);
    const float alpha = b.isEnabled() ? 1.0f : 0.4f;
    // raised surface -> violet when selected; an unselected workspace tab is quieter (a plain surface), a selected one is solid violet
    auto fill = on ? b.findColour (juce::TextButton::buttonOnColourId) : (tab ? panelBottom : panelTop);
    (void) bg;
    fill = down ? fill.darker (0.2f) : highlighted ? fill.brighter (on ? 0.12f : 0.18f) : fill;
    const auto shape = chamferPath (r, 5.0f);
    g.setColour (fill.withAlpha (alpha));
    g.fillPath (shape);
    if (! on)
    {
        g.setColour (chrome.withAlpha (0.06f * alpha)); // a faint lit top edge on a raised surface
        g.fillRect (r.getX() + 4.0f, r.getY() + 1.0f, r.getWidth() - 8.0f, 1.0f);
    }
    g.setColour ((on ? fill.brighter (0.25f) : border.brighter (highlighted ? 0.35f : 0.0f)).withAlpha (alpha));
    g.strokePath (shape, juce::PathStrokeType (1.0f));
    if (b.hasKeyboardFocus (true))
        focusRing (g, r, 5.0f);
}

void PlaygroundLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const float alpha = b.isEnabled() ? 1.0f : 0.4f;
    const bool on = b.getToggleState();
    const auto sw = juce::Rectangle<float> (34.0f, 18.0f).withCentre ({ 20.0f, (float) b.getHeight() * 0.5f });
    juce::Path track;
    track.addRoundedRectangle (sw, 9.0f);
    g.setColour ((on ? violet : (highlighted ? panelTop.brighter (0.2f) : panelTop)).withAlpha (alpha));
    g.fillPath (track);
    g.setColour ((on ? violet.brighter (0.25f) : border.brighter (highlighted ? 0.3f : 0.0f)).withAlpha (alpha));
    g.strokePath (track, juce::PathStrokeType (1.0f));
    const auto knob = juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ on ? sw.getRight() - 9.0f : sw.getX() + 9.0f, sw.getCentreY() });
    g.setColour ((on ? textBright : textDim).withAlpha (alpha));
    g.fillEllipse (knob);
    g.setColour ((on ? textBright : textDim).withAlpha (alpha));
    g.setFont (fontOf (juce::jlimit (12.0f, 14.0f, (float) b.getHeight() * 0.4f), true));
    g.drawFittedText (displayCase (b.getButtonText()), b.getLocalBounds().withTrimmedLeft (44), juce::Justification::centredLeft, 1, 0.8f);
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
    const auto groove = juce::Rectangle<float> ((float) x, cy - 2.5f, (float) w, 5.0f);
    g.setColour (border.withAlpha (alpha));
    g.fillPath (chamferPath (groove, 2.0f));
    const auto fill = groove.withRight (juce::jmax (groove.getX(), pos));
    if (fill.getWidth() > 1.0f)
    {
        g.setColour ((hot ? accent.brighter (0.15f) : accent).withAlpha (alpha));
        g.fillPath (chamferPath (fill, 2.0f));
    }
    // the thumb: a flat pill with a thin edge
    const auto thumb = juce::Rectangle<float> (12.0f, 20.0f).withCentre ({ pos, cy });
    g.setColour (textBright.withAlpha (alpha));
    g.fillPath (chamferPath (thumb, 4.0f));
    g.setColour ((hot ? accent : panelBottom).withAlpha (alpha));
    g.strokePath (chamferPath (thumb, 4.0f), juce::PathStrokeType (1.2f));
    if (s.hasKeyboardFocus (false))
        focusRing (g, thumb, 3.0f);
}
}
