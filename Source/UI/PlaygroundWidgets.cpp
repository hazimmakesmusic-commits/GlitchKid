#include "PlaygroundWidgets.h"
#include <BinaryData.h>
#include "../Presets/FactoryPresets.h"
#include "Theme.h"
#include "../DSP/EqParamIds.h"
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
    const float R = box.getWidth() * 0.5f, r = R - 5.0f, thick = juce::jmax (2.5f, R * 0.07f);
    pos = juce::jlimit (0.0f, 1.0f, pos);
    const float angle = start + pos * (end - start);
    const bool hot = hover || focus;

    juce::Path track;
    track.addCentredArc (c.x, c.y, r, r, 0.0f, start, end, true);
    g.setColour (panelTop.brighter (0.05f).withAlpha (alpha));
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (pos > 0.002f)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, r, r, 0.0f, start, angle, true);
        const juce::Point<float> p0 (c.x + std::sin (start) * r, c.y - std::cos (start) * r), p1 (c.x + std::sin (angle) * r, c.y - std::cos (angle) * r);
        const auto far = accent == violet ? pink : accent;
        const auto tip = violet.interpolatedWith (far, juce::jlimit (0.0f, 1.0f, pos * 1.2f));
        if (alpha >= 1.0f)
            for (auto [width, a] : { std::pair { thick * 4.0f, 0.10f }, std::pair { thick * 2.3f, 0.20f } }) // a soft neon halo, brighter under the pointer
            {
                const float k = hot ? 1.6f : 1.0f;
                g.setGradientFill (juce::ColourGradient (violet.withAlpha (a * k), p0, tip.withAlpha (a * k), p1, false));
                g.strokePath (arc, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        g.setGradientFill (juce::ColourGradient ((hot ? neon : violet).withAlpha (alpha), p0, (hot ? tip.brighter (0.3f) : tip).withAlpha (alpha), p1, false));
        g.strokePath (arc, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // the body: a dark machined cap, lit from above
    const float br = r * 0.74f;
    const auto body = juce::Rectangle<float> (2.0f * br, 2.0f * br).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.55f * alpha));
    g.fillEllipse (body.translated (0.0f, 3.0f).expanded (1.0f));
    g.setGradientFill (juce::ColourGradient (panelTop.brighter (0.25f).withAlpha (alpha), c.x, body.getY(), obsidian.withAlpha (alpha), c.x, body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (juce::Colours::white.withAlpha ((hot ? 0.16f : 0.09f) * alpha));
    g.drawEllipse (body.reduced (0.5f), 1.0f);
    const auto cap = body.reduced (br * 0.22f);
    g.setGradientFill (juce::ColourGradient (panelBottom, c.x, cap.getY(), panelTop, c.x, cap.getBottom(), false));
    g.fillEllipse (cap);
    const juce::Point<float> a (c.x + std::sin (angle) * br * 0.38f, c.y - std::cos (angle) * br * 0.38f), b (c.x + std::sin (angle) * br * 0.84f, c.y - std::cos (angle) * br * 0.84f);
    g.setColour (alpha >= 1.0f ? textBright : disabled);
    g.drawLine ({ a, b }, juce::jmax (1.8f, R * 0.055f));
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
    const auto base = selected ? violet : (sharp ? well : panelTop);
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
    g.setColour (well);
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
    g.setColour (well);
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
    g.setColour ((solid ? (down ? violet.darker (0.35f) : highlighted ? violet.brighter (0.12f) : violet) : violet.interpolatedWith (obsidian, 0.78f)).withAlpha (alpha));
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
    // a quiet group: a slightly lifted surface and a hairline edge (light comes from the controls, not the frames)
    g.setColour (panelBottom.withAlpha (0.55f));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (border.withAlpha (0.55f));
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
}

namespace
{
constexpr float eqMinHz = 20.0f, eqMaxHz = 20000.0f;
const char* const typeNames[8] = { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch", "Band Pass", "Tilt" };
const char* const placeLetters[5] = { "", "L", "R", "M", "S" };
const float ranges[] = { 6.0f, 12.0f, 15.0f, 24.0f, 30.0f };
const float averages[] = { 0.1f, 0.35f, 1.2f };
const char* const averageNames[] = { "Fast", "Medium", "Slow" };
juce::String hzText (float hz) { return hz >= 1000.0f ? juce::String (hz / 1000.0f, hz < 10000.0f ? 2 : 1) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz"; }
}

EqGraph::EqGraph (juce::AudioProcessorValueTreeState& a, std::function<double()> sr) : apvts (a), sampleRate (std::move (sr))
{
    setTooltip ("EQ LAB. The curve is every band's real response; the shaded area is the audio entering the EQ and the line the audio leaving it (real, measured). "
                "Drag a point: frequency and gain. Wheel on a point: Q. Right-click a point: type, slope, channels, solo, duplicate, delete. "
                "Double-click empty space: add a band. Wheel on empty space: zoom; drag empty space: move the view. Delete key: remove the selected band.");
    setWantsKeyboardFocus (true);
    for (auto* b : { &rangeButton, &fitButton, &avgButton, &resButton, &freezeButton })
    {
        addAndMakeVisible (*b);
        b->setWantsKeyboardFocus (false);
    }
    rangeButton.setTooltip ("The graph's dB range (vertical zoom).");
    fitButton.setTooltip ("Show the whole range, 20 Hz to 20 kHz.");
    avgButton.setTooltip ("Analyser averaging: how quickly the measured spectrum follows the audio.");
    resButton.setTooltip ("Analyser resolution (FFT size): higher shows more detail in the lows, but reacts a little slower.");
    freezeButton.setTooltip ("Freeze the analyser to study the spectrum. The audio keeps playing and the EQ keeps working.");
    freezeButton.setClickingTogglesState (true);
    rangeButton.onClick = [this]
    {
        int i = 0;
        while (i < 4 && ! juce::approximatelyEqual (ranges[i], rangeDb)) ++i;
        rangeDb = ranges[(i + 1) % 5];
        haveCurve = haveClean = false;
        updateStrip();
        repaint();
    };
    fitButton.onClick = [this] { setView (eqMinHz, eqMaxHz); };
    avgButton.onClick = [this]
    {
        int i = 0;
        while (i < 2 && ! juce::approximatelyEqual (averages[i], specOut.getAveragingSeconds())) ++i;
        for (auto* sp : { &specIn, &specOut })
            sp->setAveragingSeconds (averages[(i + 1) % 3]);
        updateStrip();
    };
    resButton.onClick = [this]
    {
        const int order = specOut.getResolution() >= 13 ? 11 : specOut.getResolution() + 1;
        for (auto* sp : { &specIn, &specOut })
            sp->setResolution (order);
        updateStrip();
    };
    freezeButton.onClick = [this]
    {
        for (auto* sp : { &specIn, &specOut })
            sp->setFrozen (freezeButton.getToggleState());
        updateStrip();
        repaint();
    };
    updateStrip();
}

EqGraph::~EqGraph() = default;

void EqGraph::updateStrip()
{
    rangeButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xc2\xb1")) + juce::String ((int) rangeDb) + " dB");
    fitButton.setButtonText ("Fit");
    int i = 0;
    while (i < 2 && ! juce::approximatelyEqual (averages[i], specOut.getAveragingSeconds())) ++i;
    avgButton.setButtonText (juce::String ("Avg ") + averageNames[i]);
    resButton.setButtonText ("FFT " + juce::String (1 << specOut.getResolution()));
    freezeButton.setButtonText (specOut.isFrozen() ? "Frozen" : "Freeze");
}

void EqGraph::setSelectedBand (int b)
{
    b = juce::jlimit (0, numBands - 1, b);
    if (b != selected)
    {
        selected = b;
        haveCurve = false;
        repaint();
    }
}

void EqGraph::readAll (EqBandSettings* bands) const
{
    for (int b = 0; b < ParametricEq::numBands; ++b)
        bands[b] = eqBandFrom (b, [this] (const char* id, float fallback)
        {
            const auto* v = apvts.getRawParameterValue (id);
            return v != nullptr ? v->load() : fallback;
        });
}

void EqGraph::setView (float lo, float hi)
{
    lo = juce::jlimit (eqMinHz, eqMaxHz / 4.0f, lo);
    hi = juce::jlimit (lo * 4.0f, eqMaxHz, hi); // at least two octaves
    if (hi > eqMaxHz)
        hi = eqMaxHz;
    viewLo = lo;
    viewHi = hi;
    haveCurve = haveClean = false;
    repaint();
}

float EqGraph::xOf (float hz) const { return plot.getX() + plot.getWidth() * std::log (juce::jlimit (1.0f, 1.0e5f, hz) / viewLo) / std::log (viewHi / viewLo); }
float EqGraph::yOf (float db) const { return plot.getCentreY() - juce::jlimit (-rangeDb, rangeDb, db) / rangeDb * plot.getHeight() * 0.5f; }
float EqGraph::hzAt (float x) const { return viewLo * std::exp ((x - plot.getX()) / plot.getWidth() * std::log (viewHi / viewLo)); }
float EqGraph::dbAt (float y) const { return juce::jlimit (-rangeDb, rangeDb, (plot.getCentreY() - y) / (plot.getHeight() * 0.5f) * rangeDb); }
float EqGraph::yOfLevel (float dbfs) const { return plot.getBottom() - plot.getHeight() * juce::jlimit (0.0f, 1.0f, (dbfs + 90.0f) / 90.0f); }

void EqGraph::resized()
{
    plot = getLocalBounds().toFloat().withTrimmedLeft (34.0f).withTrimmedBottom (20.0f).withTrimmedTop (8.0f).withTrimmedRight (10.0f);
    int x = (int) plot.getX() + 6;
    const int y = (int) plot.getY() + 4;
    for (auto [b, w] : { std::pair<juce::TextButton*, int> { &rangeButton, 56 }, { &fitButton, 34 }, { &avgButton, 84 }, { &resButton, 72 }, { &freezeButton, 58 } })
    {
        b->setBounds (x, y, w, 18);
        x += w + 4;
    }
    haveCurve = haveClean = false;
}

juce::Point<float> EqGraph::pointOf (int band) const
{
    const auto& s = shown[juce::jlimit (0, numBands - 1, band)];
    return { xOf (s.freqHz), yOf (ParametricEq::isGainType (s.type) ? s.gainDb : 0.0f) };
}

void EqGraph::rebuildCurve()
{
    readAll (shown);
    const double fs0 = sampleRate ? sampleRate() : 48000.0, fs = fs0 > 1000.0 ? fs0 : 48000.0;
    curve.clear();
    selectedCurve.clear();
    const int n = juce::jmax (8, (int) plot.getWidth() / 2);
    for (int i = 0; i <= n; ++i)
    {
        const float x = plot.getX() + plot.getWidth() * (float) i / (float) n, hz = hzAt (x);
        const float y = yOf ((float) ParametricEq::responseDb (shown, numBands, hz, fs)); // every band, from the audio's own coefficients
        const float ys = yOf ((float) ParametricEq::bandResponseDb (shown[selected], hz, fs));
        if (i == 0) { curve.startNewSubPath (x, y); selectedCurve.startNewSubPath (x, ys); }
        else { curve.lineTo (x, y); selectedCurve.lineTo (x, ys); }
    }
    for (int b = 0; b < numBands; ++b)
    {
        const auto& s = shown[b];
        const float v[7] = { s.freqHz, s.gainDb, s.q, s.on ? 1.0f : 0.0f, (float) s.type, (float) s.slopeDb, (float) s.placement };
        std::copy (v, v + 7, shownAll.begin() + b * 7);
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

void EqGraph::setCleanApplied (const CleanLab::Applied& a)
{
    clean = a;
    refresh();
}

void EqGraph::analyse (const analysis::DisplayTap& in, const analysis::DisplayTap& out, double sr, float dt)
{
    const bool a = specIn.update (in, sr, dt), b = specOut.update (out, sr, dt);
    inLive = specIn.hasSignal();
    outLive = specOut.hasSignal();
    if (a || b)
    {
        float hz = 0.0f, db = -120.0f;
        if (specOut.peak (viewLo, juce::jmin (viewHi, (float) (0.45 * sr)), hz, db))
        {
            peakHz = hz;
            peakDb = db;
        }
        else
            peakHz = 0.0f;
        repaint();
    }
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
    bool changed = ! haveCurve;
    if (! changed)
    {
        EqBandSettings all[numBands];
        readAll (all);
        for (int b = 0; b < numBands && ! changed; ++b)
        {
            const auto& s = all[b];
            const float v[7] = { s.freqHz, s.gainDb, s.q, s.on ? 1.0f : 0.0f, (float) s.type, (float) s.slopeDb, (float) s.placement };
            for (int k = 0; k < 7; ++k)
                changed = changed || ! juce::exactlyEqual (v[k], shownAll[(size_t) (b * 7 + k)]);
        }
    }
    if (changed)
    {
        rebuildCurve();
        repaint();
    }
    // dynamic EQ: the curve as it is right now (each dynamic band's gain plus its live change), only while a change is happening
    if (dynamicDbOf)
    {
        bool moved = changed, any = false;
        for (int b = 0; b < numBands; ++b)
        {
            const float d = shown[b].on && shown[b].dynamic ? dynamicDbOf (b) : 0.0f;
            moved = moved || std::abs (d - dynNow[(size_t) b]) > 0.05f;
            any = any || std::abs (d) > 0.05f;
            dynNow[(size_t) b] = d;
        }
        if (moved || any != haveLive)
        {
            haveLive = any;
            liveCurve.clear();
            if (any)
            {
                EqBandSettings live[numBands];
                std::copy (shown, shown + numBands, live);
                for (int b = 0; b < numBands; ++b)
                    live[b].gainDb += dynNow[(size_t) b];
                const double fs0 = sampleRate ? sampleRate() : 48000.0, fs = fs0 > 1000.0 ? fs0 : 48000.0;
                const int n = juce::jmax (8, (int) plot.getWidth() / 2);
                for (int i = 0; i <= n; ++i)
                {
                    const float x = plot.getX() + plot.getWidth() * (float) i / (float) n;
                    const float y = yOf ((float) ParametricEq::responseDb (live, numBands, hzAt (x), fs));
                    if (i == 0) liveCurve.startNewSubPath (x, y); else liveCurve.lineTo (x, y);
                }
            }
            repaint();
        }
    }
}

void EqGraph::paint (juce::Graphics& g)
{
    if (! haveCurve)
        rebuildCurve();
    if (! haveClean)
        refresh();
    g.setColour (well);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);
    g.setColour (border);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 3.0f, 1.0f);
    g.reduceClipRegion (getLocalBounds().reduced (1));

    // grid: dB lines for the chosen range, frequency lines (1-2-5 per decade) inside the view
    g.setFont (fontOf (10.5f));
    const float step = rangeDb <= 6.0f ? 2.0f : rangeDb <= 15.0f ? 6.0f : 12.0f;
    for (float db = -std::floor (rangeDb / step) * step; db <= rangeDb + 0.01f; db += step)
    {
        const float y = yOf (db);
        g.setColour (juce::approximatelyEqual (db, 0.0f) ? border.brighter (0.5f) : border.withAlpha (0.8f));
        g.fillRect (plot.getX(), y, plot.getWidth(), 1.0f);
        g.setColour (textDim);
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), juce::Rectangle<float> (2.0f, y - 7.0f, 28.0f, 14.0f), juce::Justification::centredRight, false);
    }
    for (float decade = 10.0f; decade <= 10000.0f; decade *= 10.0f)
        for (float m : { 1.0f, 2.0f, 5.0f })
        {
            const float hz = decade * m;
            if (hz < viewLo || hz > viewHi)
                continue;
            const float x = xOf (hz);
            g.setColour (border.withAlpha (m > 1.0f ? 0.5f : 0.9f));
            g.fillRect (x, plot.getY(), 1.0f, plot.getHeight());
            g.setColour (textDim);
            g.drawText (hz >= 1000.0f ? juce::String ((int) (hz / 1000.0f)) + "k" : juce::String ((int) hz), juce::Rectangle<float> (x - 18.0f, plot.getBottom() + 3.0f, 36.0f, 14.0f),
                        juce::Justification::centred, false);
        }

    // the analyser: what enters the EQ (shaded) and what leaves it (line), measured, per 2-pixel column
    {
        juce::Path inP, outP;
        bool anyIn = false, anyOut = false;
        inP.startNewSubPath (plot.getX(), plot.getBottom());
        for (float x = plot.getX(); x <= plot.getRight(); x += 2.0f)
        {
            const float f0 = hzAt (x - 1.0f), f1 = hzAt (x + 1.0f);
            const float li = specIn.levelDb (f0, f1), lo = specOut.levelDb (f0, f1);
            anyIn = anyIn || li > -90.0f;
            anyOut = anyOut || lo > -90.0f;
            inP.lineTo (x, yOfLevel (li));
            if (x <= plot.getX()) outP.startNewSubPath (x, yOfLevel (lo)); else outP.lineTo (x, yOfLevel (lo));
        }
        inP.lineTo (plot.getRight(), plot.getBottom());
        inP.closeSubPath();
        if (anyIn)
        {
            g.setColour (textDim.withAlpha (0.16f));
            g.fillPath (inP);
        }
        if (anyOut)
        {
            g.setColour (pink.withAlpha (0.55f));
            g.strokePath (outP, juce::PathStrokeType (1.0f));
        }
        if (! anyIn && ! anyOut)
        {
            g.setColour (textDim.withAlpha (0.8f));
            g.setFont (fontOf (11.0f));
            g.drawText ("Analyser: play audio to see the spectrum", juce::Rectangle<float> (plot.getX() + 6.0f, plot.getBottom() - 18.0f, 260.0f, 16.0f),
                        juce::Justification::centredLeft, false);
        }
    }

    // the response: the selected band's own shape, then the sum of every band
    juce::Path fill (curve);
    fill.lineTo (plot.getRight(), plot.getCentreY());
    fill.lineTo (plot.getX(), plot.getCentreY());
    fill.closeSubPath();
    g.setColour (violet.withAlpha (0.16f));
    g.fillPath (fill);
    if (shown[selected].on)
    {
        g.setColour (neon.withAlpha (0.45f));
        g.strokePath (selectedCurve, juce::PathStrokeType (1.0f));
    }
    g.setColour (violet.withAlpha (0.28f));
    g.strokePath (curve, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); // a restrained glow under the line
    g.setColour (neon);
    g.strokePath (curve, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (haveLive) // dynamic EQ, live: the curve right now, and on each moving band a stroke from its set gain to where it is now
    {
        g.setColour (pink.withAlpha (0.9f));
        g.strokePath (liveCurve, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        for (int b = 0; b < numBands; ++b)
            if (shown[b].on && std::abs (dynNow[(size_t) b]) > 0.05f)
            {
                const auto p = pointOf (b);
                const float y1 = yOf (shown[b].gainDb + dynNow[(size_t) b]);
                g.fillRect (p.x - 1.5f, juce::jmin (p.y, y1), 3.0f, std::abs (y1 - p.y));
                g.fillEllipse (p.x - 3.0f, y1 - 3.0f, 6.0f, 6.0f);
            }
    }
    // CLEAN LAB's applied corrections (real, from the DSP's own smoothed values), only while any is applied
    if (shownClean.mudDb > 0.05f || shownClean.rumbleAmount > 0.02f || shownClean.resDb[0] > 0.05f || shownClean.resDb[1] > 0.05f || shownClean.resDb[2] > 0.05f)
    {
        g.setColour (pink.withAlpha (0.95f));
        g.strokePath (cleanCurve, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setFont (fontOf (10.5f, true));
        g.drawText ("Clean Lab", juce::Rectangle<float> (plot.getRight() - 80.0f, plot.getY() + 24.0f, 78.0f, 13.0f), juce::Justification::centredRight, false);
    }

    // one handle per band that is on: its number, its channel letter (L / R / M / S), solo shown as a ring
    for (int b = 0; b < numBands; ++b)
    {
        const auto& s = shown[b];
        if (! s.on)
            continue;
        const auto p = pointOf (b);
        const bool sel = b == selected, hot = b == dragging || b == hovered;
        g.setColour (obsidian);
        g.fillEllipse (p.x - 8.0f, p.y - 8.0f, 16.0f, 16.0f);
        g.setColour (sel ? pink : (hot ? neon : violet));
        g.drawEllipse (p.x - 8.0f, p.y - 8.0f, 16.0f, 16.0f, sel ? 2.4f : 1.6f);
        if (b == soloBand)
            g.drawEllipse (p.x - 12.0f, p.y - 12.0f, 24.0f, 24.0f, 1.2f);
        g.setColour (sel ? textBright : textDim);
        g.setFont (fontOf (9.5f, true));
        g.drawText (juce::String (b + 1), juce::Rectangle<float> (p.x - 8.0f, p.y - 7.0f, 16.0f, 14.0f), juce::Justification::centred, false);
        if (s.placement != EqPlacement::stereo)
            g.drawText (placeLetters[(int) s.placement], juce::Rectangle<float> (p.x - 8.0f, p.y + 9.0f, 16.0f, 11.0f), juce::Justification::centred, false);
    }

    // read-outs: the band under the pointer (or being dragged), the frequency under the pointer, the loudest peak leaving the EQ
    g.setFont (fontOf (11.0f));
    const auto info = juce::Rectangle<float> (plot.getRight() - 300.0f, plot.getY() + 4.0f, 296.0f, 16.0f);
    const int focusBand = dragging >= 0 ? dragging : hovered;
    if (focusBand >= 0)
    {
        const auto& s = shown[focusBand];
        juce::String t = "Band " + juce::String (focusBand + 1) + "  " + typeNames[(int) s.type] + "  " + hzText (s.freqHz);
        if (ParametricEq::isGainType (s.type))
            t << "  " << juce::String (s.gainDb, 1) << " dB";
        if (s.type == EqType::lowCut || s.type == EqType::highCut)
            t << "  " << s.slopeDb << " dB/oct";
        t << "  Q " << juce::String (s.q, 2);
        if (s.dynamic && ParametricEq::isGainType (s.type))
            t << "  dyn " << juce::String (dynNow[(size_t) focusBand], 1) << " dB";
        g.setColour (textBright);
        g.drawText (t, info, juce::Justification::centredRight, false);
    }
    else if (plot.contains (mouse))
    {
        const float hz = hzAt (mouse.x);
        g.setColour (textDim);
        g.drawText (hzText (hz) + "  " + analysis::EqAnalyzer::noteName (hz) + "   EQ " + juce::String (dbAt (mouse.y), 1) + " dB", info, juce::Justification::centredRight, false);
    }
    if (peakHz > 0.0f && (outLive || specOut.isFrozen()))
    {
        const float x = xOf (peakHz), y = yOfLevel (peakDb);
        g.setColour (pink);
        g.fillEllipse (x - 2.5f, y - 2.5f, 5.0f, 5.0f);
        g.setColour (textDim);
        g.drawText ("Peak " + hzText (peakHz) + " (" + analysis::EqAnalyzer::noteName (peakHz) + ")  " + juce::String (peakDb, 1) + " dBFS",
                    juce::Rectangle<float> (plot.getRight() - 300.0f, plot.getBottom() - 18.0f, 296.0f, 16.0f), juce::Justification::centredRight, false);
    }
}

int EqGraph::bandNear (juce::Point<float> p) const
{
    int best = -1;
    float bestD = 16.0f * 16.0f;
    for (int i = 0; i < numBands; ++i)
    {
        if (! shown[i].on)
            continue;
        const float d = pointOf (i).getDistanceSquaredFrom (p);
        if (d < bestD)
        {
            bestD = d;
            best = i;
        }
    }
    return best;
}

void EqGraph::setParam (const char* id, float value, bool gesture)
{
    if (auto* prm = apvts.getParameter (id))
    {
        if (gesture) prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
        if (gesture) prm->endChangeGesture();
    }
}

int EqGraph::addBandAt (float hz, float db)
{
    EqBandSettings all[numBands];
    readAll (all);
    int b = -1;
    for (int i : { 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3 }) // the new bands first; the original four only when they are switched off
        if (! all[i].on) { b = i; break; }
    if (b < 0)
        return -1;
    const auto& id = eqIds[b];
    setParam (id.type, (float) EqType::bell);
    setParam (id.freq, hz);
    setParam (id.gain, db);
    setParam (id.q, 1.0f);
    setParam (id.slope, 1.0f);
    setParam (id.stereo, 0.0f);
    setParam (id.on, 1.0f);
    setSelectedBand (b);
    if (onBandSelected) onBandSelected (b);
    refresh();
    return b;
}

int EqGraph::duplicateBand (int src)
{
    EqBandSettings all[numBands];
    readAll (all);
    int b = -1;
    for (int i : { 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3 })
        if (! all[i].on && i != src) { b = i; break; }
    if (b < 0 || src < 0)
        return -1;
    const auto& s = all[src];
    const auto& id = eqIds[b];
    setParam (id.type, (float) s.type);
    setParam (id.freq, s.freqHz * 1.12f);
    setParam (id.gain, s.gainDb);
    setParam (id.q, s.q);
    setParam (id.slope, (float) ParametricEq::slopeIndex (s.slopeDb));
    setParam (id.stereo, (float) s.placement);
    setParam (id.on, 1.0f);
    setSelectedBand (b);
    if (onBandSelected) onBandSelected (b);
    refresh();
    return b;
}

void EqGraph::removeBand (int b)
{
    if (b < 0 || b >= numBands)
        return;
    setParam (eqIds[b].on, 0.0f);
    if (soloBand == b)
    {
        soloBand = -1;
        if (onSolo) onSolo (-1);
    }
    refresh();
}

void EqGraph::showBandMenu (int b)
{
    const auto& s = shown[b];
    juce::PopupMenu m, slopes, channels;
    m.addSectionHeader ("Band " + juce::String (b + 1));
    for (int t = 0; t < 8; ++t)
        m.addItem (100 + t, typeNames[t], true, (int) s.type == t);
    for (int i = 0; i < ParametricEq::numSlopes; ++i)
        slopes.addItem (200 + i, juce::String (ParametricEq::slopes[i]) + " dB/oct", true, ParametricEq::slopes[i] == s.slopeDb);
    const char* chNames[5] = { "Stereo", "Left", "Right", "Mid", "Side" };
    for (int i = 0; i < 5; ++i)
        channels.addItem (300 + i, chNames[i], true, (int) s.placement == i);
    m.addSubMenu ("Slope", slopes, s.type == EqType::lowCut || s.type == EqType::highCut);
    m.addSubMenu ("Channels", channels);
    m.addSeparator();
    m.addItem (400, "Solo (hear this band's part)", true, soloBand == b);
    m.addItem (401, "Duplicate");
    m.addItem (402, "Delete");
    juce::Component::SafePointer<EqGraph> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, b] (int r)
    {
        if (safe == nullptr || r == 0)
            return;
        auto& self = *safe;
        const auto& id = eqIds[b];
        if (r >= 100 && r < 108) self.setParam (id.type, (float) (r - 100));
        else if (r >= 200 && r < 200 + ParametricEq::numSlopes) self.setParam (id.slope, (float) (r - 200));
        else if (r >= 300 && r < 305) self.setParam (id.stereo, (float) (r - 300));
        else if (r == 400)
        {
            self.soloBand = self.soloBand == b ? -1 : b;
            if (self.onSolo) self.onSolo (self.soloBand);
        }
        else if (r == 401) self.duplicateBand (b);
        else if (r == 402) self.removeBand (b);
        self.refresh();
        self.repaint();
    });
}

void EqGraph::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const int b = bandNear (e.position);
    if (e.mods.isPopupMenu())
    {
        if (b >= 0)
        {
            setSelectedBand (b);
            if (onBandSelected) onBandSelected (b);
            showBandMenu (b);
        }
        return;
    }
    dragging = b;
    if (b >= 0)
    {
        setSelectedBand (b);
        if (onBandSelected) onBandSelected (b);
        for (auto* id : { eqIds[b].freq, eqIds[b].gain })
            if (auto* prm = apvts.getParameter (id))
                prm->beginChangeGesture();
    }
    else if (plot.contains (e.position) && (viewLo > eqMinHz * 1.001f || viewHi < eqMaxHz * 0.999f))
    {
        panning = true;
        panStartLo = viewLo;
        panStartX = e.position.x;
    }
    repaint();
}

void EqGraph::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging >= 0)
    {
        setParam (eqIds[dragging].freq, hzAt (e.position.x), false);
        if (ParametricEq::isGainType (shown[dragging].type))
            setParam (eqIds[dragging].gain, dbAt (e.position.y), false);
        refresh();
    }
    else if (panning)
    {
        const float span = std::log (viewHi / viewLo), shift = (e.position.x - panStartX) / plot.getWidth() * span;
        float lo = panStartLo * std::exp (-shift);
        lo = juce::jlimit (eqMinHz, eqMaxHz / std::exp (span), lo);
        setView (lo, lo * std::exp (span));
    }
}

void EqGraph::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
        for (auto* id : { eqIds[dragging].freq, eqIds[dragging].gain })
            if (auto* prm = apvts.getParameter (id))
                prm->endChangeGesture();
    dragging = -1;
    panning = false;
    repaint();
}

void EqGraph::mouseMove (const juce::MouseEvent& e)
{
    mouse = e.position;
    const int h = bandNear (e.position);
    if (h != hovered)
    {
        hovered = h;
        setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }
    repaint();
}

void EqGraph::mouseExit (const juce::MouseEvent&)
{
    mouse = { -1.0f, -1.0f };
    hovered = -1;
    repaint();
}

void EqGraph::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int b = bandNear (e.position);
    if (b >= 0)
    {
        if (ParametricEq::isGainType (shown[b].type))
            setParam (eqIds[b].gain, 0.0f);
    }
    else if (plot.contains (e.position))
        addBandAt (hzAt (e.position.x), dbAt (e.position.y));
    refresh();
}

void EqGraph::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int b = bandNear (e.position);
    if (b >= 0)
    {
        const float q = apvts.getRawParameterValue (eqIds[b].q)->load() * std::pow (1.12f, w.deltaY > 0.0f ? 1.0f : -1.0f);
        setParam (eqIds[b].q, q);
        refresh();
        return;
    }
    if (! plot.contains (e.position) || juce::approximatelyEqual (w.deltaY, 0.0f))
        return;
    // zoom the frequency axis around the pointer
    const float hz = hzAt (e.position.x), frac = (e.position.x - plot.getX()) / plot.getWidth();
    const float span = juce::jlimit (std::log (4.0f), std::log (eqMaxHz / eqMinHz), std::log (viewHi / viewLo) * (w.deltaY > 0.0f ? 0.85f : 1.0f / 0.85f));
    const float lo = juce::jlimit (eqMinHz, eqMaxHz / std::exp (span), hz / std::exp (frac * span));
    setView (lo, lo * std::exp (span));
}

bool EqGraph::keyPressed (const juce::KeyPress& k)
{
    if ((k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) && shown[selected].on)
    {
        removeBand (selected);
        return true;
    }
    return false;
}

// ---- PlaygroundLookAndFeel -------------------------------------------------------------------------------------------------------------

PlaygroundLookAndFeel::PlaygroundLookAndFeel()
{
    setColour (juce::TextButton::textColourOnId, chrome);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack); // a knob's value is a plain number under it, not a boxed field
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    applyTheme();
}

void PlaygroundLookAndFeel::applyTheme() // everything that follows the profile: accents and the foundation
{
    setColour (juce::ResizableWindow::backgroundColourId, obsidian);
    setColour (juce::Label::textColourId, textBright);
    setColour (juce::TextButton::buttonColourId, panelTop);
    setColour (juce::TextButton::textColourOffId, textBright);
    setColour (juce::Slider::textBoxTextColourId, textBright);
    setColour (juce::ComboBox::backgroundColourId, well);
    setColour (juce::ComboBox::outlineColourId, border.brighter (0.2f));
    setColour (juce::PopupMenu::backgroundColourId, panelBottom);
    setColour (juce::PopupMenu::textColourId, textBright);
    setColour (juce::TextButton::buttonOnColourId, violet);
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
        const bool small = (bool) b.getProperties().getWithDefault ("railSmall", false), back = (bool) b.getProperties().getWithDefault ("railBack", false);
        if (back) // HOME: a chevron back to the instrument
        {
            juce::Path chev;
            const float cx = r.getX() + 16.0f, cy = r.getCentreY();
            chev.startNewSubPath (cx + 2.5f, cy - 5.0f);
            chev.lineTo (cx - 2.5f, cy);
            chev.lineTo (cx + 2.5f, cy + 5.0f);
            g.setColour (neon);
            g.strokePath (chev, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setFont (fontOf (small ? 12.0f : 13.5f, ! small));
        g.setColour (b.getToggleState() ? textBright : back ? neon : small ? textDim : textDim.brighter (0.3f));
        g.drawText (b.getProperties().getWithDefault ("railText", displayCase (b.getButtonText())).toString(), r.withTrimmedLeft (small ? 28.0f : 32.0f), juce::Justification::centredLeft, false);
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
    // a knob's or slider's value: plain text under or beside it; the field appears only while a value is being typed
    const bool valueText = dynamic_cast<juce::Slider*> (l.getParentComponent()) != nullptr && ! l.isBeingEdited();
    if (! valueText)
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
    if (! valueText)
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
        if (sel) // the selected stage: a soft wash of the profile and a gradient bar at its edge
        {
            g.setGradientFill (juce::ColourGradient (violet.withAlpha (0.16f), r.getX(), r.getY(), violet.withAlpha (0.0f), r.getRight(), r.getY(), false));
            g.fillRoundedRectangle (r.reduced (0.0f, 1.0f), 5.0f);
            g.setGradientFill (theme::primaryGradient ({ r.getX(), r.getY() + 5.0f }, { r.getX(), r.getBottom() - 5.0f }));
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
    if (on && b.findColour (juce::TextButton::buttonOnColourId) == violet) // the profile's fill: primary into its deep end (white text keeps AA)
    {
        const auto top = down ? violet.darker (0.2f) : highlighted ? violet.brighter (0.12f) : violet;
        g.setGradientFill (juce::ColourGradient (top.withAlpha (alpha), r.getX(), r.getY(), top.interpolatedWith (theme::active().deep, 0.45f).withAlpha (alpha),
                                                 r.getRight(), r.getBottom(), false));
    }
    else
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
    if (on)
        g.setGradientFill (theme::primaryGradient ({ sw.getRight(), sw.getY() }, { sw.getX(), sw.getBottom() }, alpha));
    else
        g.setColour ((highlighted ? panelTop.brighter (0.2f) : panelTop).withAlpha (alpha));
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
        if (accent == violet) // the profile's gradient along the travel
            g.setGradientFill (theme::primaryGradient ({ groove.getRight(), cy }, { groove.getX(), cy }, alpha));
        else
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
