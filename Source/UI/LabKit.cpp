#include "LabKit.h"

namespace glitchkid::ui
{
// Inter Bold set slightly condensed and tracked out: no other face may be bundled, so the "technical" character comes from proportion and spacing.
juce::Font labFont (float height) { return fontOf (height, true).withHorizontalScale (0.94f).withExtraKerningFactor (0.12f); }

void LabHeading::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.setColour (pink);
    g.fillRect (r.getX(), r.getY() + 2, 3, r.getHeight() - 4);
    g.setColour (colour);
    g.setFont (labFont (fontSize));
    g.drawText (text.toUpperCase(), r.withTrimmedLeft (10), juce::Justification::centredLeft, false);
}

// ---- LevelBar --------------------------------------------------------------------------------------------------------------------------

float LevelBar::peakToMeter (float peak)
{
    if (! std::isfinite (peak) || peak <= 0.0f)
        return 0.0f;
    return juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (peak, -60.0f) + 60.0f) / 60.0f);
}

void LevelBar::setValue (float level01, bool clipping)
{
    const float next = juce::jlimit (0.0f, 1.0f, std::isfinite (level01) ? level01 : 0.0f);
    const float shown = next > level ? next : juce::jmax (next, level - 0.05f); // falls at a fixed rate
    if (! juce::approximatelyEqual (shown, level) || clipping != clip)
    {
        level = shown;
        clip = clipping;
        repaint();
    }
}

void LevelBar::setActive (bool a)
{
    if (a != active)
    {
        active = a;
        repaint();
    }
}

void LevelBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0c0910));
    g.fillRect (r);
    g.setColour (border);
    g.drawRect (r, 1.0f);
    r = r.reduced (2.0f);
    for (int i = 1; i < 6; ++i) // faint 10 dB divisions
    {
        g.setColour (border.withAlpha (0.8f));
        g.fillRect (r.getX() + r.getWidth() * (float) i / 6.0f, r.getY(), 1.0f, r.getHeight());
    }
    if (! active || level <= 0.0f)
        return;
    const auto fill = r.withWidth (r.getWidth() * level);
    juce::ColourGradient grad (violet, r.getX(), 0.0f, pink, r.getRight(), 0.0f, false);
    g.setGradientFill (grad);
    g.setOpacity (0.95f);
    g.fillRect (fill);
    if (clip)
    {
        g.setColour (errorRed);
        g.fillRect (r.getRight() - 4.0f, r.getY(), 4.0f, r.getHeight());
    }
}

// ---- BandCompare -----------------------------------------------------------------------------------------------------------------------

void BandCompare::setData (bool v, const std::array<float, numBands>& voc, const std::array<float, numBands>& bt, const std::array<float, numBands>& rk)
{
    valid = v;
    vocal = voc;
    beat = bt;
    risk = rk;
    repaint();
}

void BandCompare::paint (juce::Graphics& g)
{
    static const char* names[numBands] = { "LOW", "LOW-MID", "MID", "PRESENCE", "AIR" };
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0c0910));
    g.fillPath (chamferPath (r, 6.0f));
    g.setColour (border);
    g.strokePath (chamferPath (r.reduced (0.5f), 6.0f), juce::PathStrokeType (1.0f));
    auto inner = getLocalBounds().reduced (10, 8);
    auto legend = inner.removeFromTop (14);
    g.setColour (violet);
    g.fillRect (legend.getX(), legend.getY() + 3, 8, 8);
    g.setColour (pink);
    g.fillRect (legend.getX() + 70, legend.getY() + 3, 8, 8);
    g.setFont (fontOf (11.0f, true));
    g.setColour (textDim);
    g.drawText ("VOCAL", legend.getX() + 12, legend.getY(), 54, 14, juce::Justification::centredLeft, false);
    g.drawText ("BEAT", legend.getX() + 82, legend.getY(), 54, 14, juce::Justification::centredLeft, false);
    auto labels = inner.removeFromBottom (16);
    inner.removeFromTop (10);
    const int colW = inner.getWidth() / numBands;
    float top = 0.01f;
    if (valid)
        for (int i = 0; i < numBands; ++i)
            top = juce::jmax (top, vocal[(size_t) i], beat[(size_t) i]);
    for (int i = 0; i < numBands; ++i)
    {
        auto col = juce::Rectangle<int> (inner.getX() + i * colW, inner.getY(), colW, inner.getHeight()).reduced (8, 0);
        g.setColour (textDim);
        g.setFont (fontOf (10.5f, true));
        g.drawText (names[i], labels.getX() + i * colW, labels.getY(), colW, labels.getHeight(), juce::Justification::centred, false);
        for (int k = 0; k < 2; ++k)
        {
            auto bar = k == 0 ? col.removeFromLeft (col.getWidth() / 2 - 1) : col.withTrimmedLeft (2);
            g.setColour (border.withAlpha (0.5f));
            g.fillRect (bar);
            const float v = valid ? juce::jlimit (0.0f, 1.0f, (k == 0 ? vocal[(size_t) i] : beat[(size_t) i]) / top) : 0.0f;
            if (v > 0.0f)
            {
                g.setColour (k == 0 ? violet : pink);
                g.fillRect (bar.withTop (bar.getBottom() - juce::roundToInt ((float) bar.getHeight() * v)));
            }
        }
        if (valid && risk[(size_t) i] > 0.35f)
        {
            g.setColour (pink);
            g.setFont (fontOf (11.0f, true));
            g.drawText ("!", inner.getX() + i * colW, inner.getY() - 12, colW, 12, juce::Justification::centred, false);
        }
    }
    if (! valid)
    {
        g.setColour (textDim);
        g.setFont (labFont (13.0f));
        g.drawText ("NOT ANALYSED", getLocalBounds(), juce::Justification::centred, false);
    }
}

// ---- WorkspaceTile ---------------------------------------------------------------------------------------------------------------------

void WorkspaceTile::setContent (const juce::String& t, const juce::String& d, bool p)
{
    title = t;
    description = d;
    prominent = p;
    setButtonText (t);
    repaint();
}

void WorkspaceTile::setStatus (const juce::String& statusText, TileStatus k)
{
    if (statusText != status || k != kind)
    {
        status = statusText;
        kind = k;
        repaint();
    }
}

namespace
{
// Schematic pictograms for the HOME workspace tiles. They are decoration drawn from fixed shapes; none of them represents a measurement.
void drawTileGlyph (juce::Graphics& g, int index, juce::Rectangle<float> b, juce::Colour c, bool lit)
{
    g.setColour (c);
    const float x = b.getX(), y = b.getY(), w = b.getWidth(), h = b.getHeight();
    juce::Path p;
    switch (index)
    {
        case 0: // CLEAN LAB: a response line with a notch taken out of it
            p.startNewSubPath (x, y + h * 0.35f); p.lineTo (x + w * 0.30f, y + h * 0.35f); p.lineTo (x + w * 0.42f, y + h * 0.88f); p.lineTo (x + w * 0.54f, y + h * 0.35f); p.lineTo (x + w, y + h * 0.35f);
            break;
        case 1: // BEAT LOCK: two waves, one riding the other
            for (int i = 0; i <= 20; ++i)
            {
                const float t = (float) i / 20.0f;
                const float yy = y + h * (0.30f + 0.20f * std::sin (t * 12.0f));
                i == 0 ? p.startNewSubPath (x, yy) : p.lineTo (x + w * t, yy);
            }
            for (int i = 0; i <= 20; ++i)
            {
                const float t = (float) i / 20.0f;
                const float yy = y + h * (0.72f + 0.14f * std::sin (t * 12.0f + 1.6f));
                i == 0 ? p.startNewSubPath (x, yy) : p.lineTo (x + w * t, yy);
            }
            break;
        case 2: // BUILD: three rising bars
            for (int i = 0; i < 3; ++i)
                p.addRectangle (x + w * (0.08f + 0.32f * (float) i), y + h * (0.62f - 0.28f * (float) i), w * 0.2f, h * (0.38f + 0.28f * (float) i));
            g.fillPath (p);
            return;
        default: // FINAL TOUCH: three small dials
            for (int i = 0; i < 3; ++i)
            {
                const float cx = x + w * (0.17f + 0.33f * (float) i), cy = y + h * 0.55f, rr = h * 0.34f;
                p.addCentredArc (cx, cy, rr, rr, 0.0f, -2.2f, 2.2f, true);
                p.startNewSubPath (cx, cy); p.lineTo (cx + rr * 0.7f * std::sin (-0.6f + 0.6f * (float) i), cy - rr * 0.7f * std::cos (-0.6f + 0.6f * (float) i));
            }
            break;
    }
    if (lit)
    {
        g.setColour (c.withAlpha (0.25f));
        g.strokePath (p, juce::PathStrokeType (3.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c);
    }
    g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
}

void WorkspaceTile::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    if (down)
        r = r.translated (0.0f, 1.0f);
    const auto accent = kind == TileStatus::problem ? errorRed : kind == TileStatus::active ? pink : kind == TileStatus::ready ? violet : textDim;
    const auto shape = chamferPath (r, 12.0f);
    if (prominent || highlighted)
        drawGlow (g, shape, prominent ? violet.withAlpha (0.9f) : violet.withAlpha (0.55f), 6.0f);
    g.setGradientFill (juce::ColourGradient (prominent ? juce::Colour (0xff3a1363) : (highlighted ? panelTop.brighter (0.12f) : panelTop), 0.0f, r.getY(),
                                             prominent ? juce::Colour (0xff1a0a2e) : panelBottom.darker (0.25f), 0.0f, r.getBottom(), false));
    g.fillPath (shape);
    g.setColour (chrome.withAlpha (prominent ? 0.16f : 0.08f)); // the lit top edge
    g.fillRect (r.getX() + 14.0f, r.getY() + 1.0f, r.getWidth() - 28.0f, 1.0f);
    g.setColour (prominent ? violet : (highlighted ? neon : border.brighter (0.15f)));
    g.strokePath (shape, juce::PathStrokeType (prominent ? 2.0f : 1.3f));
    g.setGradientFill (juce::ColourGradient (accent, r.getX() + 14.0f, 0.0f, accent.withAlpha (0.0f), r.getX() + 80.0f, 0.0f, false)); // the status light
    g.fillRect (r.getX() + 14.0f, r.getBottom() - 4.0f, 66.0f, 3.0f);
    auto in = r.toNearestInt().reduced (16, 12);
    if (glyph >= 0)
    {
        const auto box = juce::Rectangle<float> (40.0f, 26.0f).withRightX (r.getRight() - 14.0f).withY (r.getY() + 12.0f);
        drawTileGlyph (g, glyph, box, prominent ? violet.brighter (0.4f) : accent.interpolatedWith (textDim, 0.4f).withAlpha (highlighted ? 1.0f : 0.8f), prominent || highlighted);
    }
    g.setColour (chrome);
    g.setFont (labFont (prominent ? 18.0f : 15.0f));
    g.drawFittedText (title.toUpperCase(), in.removeFromTop (prominent ? 26 : 22).withTrimmedRight (glyph >= 0 ? 46 : 0), juce::Justification::centredLeft, 1, 0.7f);
    in.removeFromTop (4);
    g.setColour (textDim);
    g.setFont (fontOf (12.0f));
    g.drawFittedText (description, in.removeFromTop (juce::jmax (30, in.getHeight() - 24)), juce::Justification::topLeft, 3, 0.9f);
    g.setColour (accent);
    g.setFont (fontOf (11.5f, true));
    g.drawFittedText (status.toUpperCase(), in.removeFromBottom (18), juce::Justification::centredLeft, 1, 0.7f);
    if (hasKeyboardFocus (true))
    {
        g.setColour (chrome);
        g.strokePath (chamferPath (r.expanded (2.0f), 12.0f), juce::PathStrokeType (1.4f));
    }
}

// ---- SpectrumView ----------------------------------------------------------------------------------------------------------------------

void SpectrumView::setSampleRate (double sr)
{
    if (! juce::approximatelyEqual (sr, sampleRate))
    {
        sampleRate = sr;
        geometryDirty = true;
        changed = true;
    }
}

void SpectrumView::setBackdrop (const juce::Image& image, float strength)
{
    backdrop = image;
    backdropScaled = {};
    backdropStrength = strength;
    geometryDirty = true;
    changed = true;
    commit();
}

void SpectrumView::setSculpture (bool on)
{
    if (on != sculpture)
    {
        sculpture = on;
        geometryDirty = true;
        changed = true;
    }
}

void SpectrumView::setActivity (float dt, float activity, bool motionAllowed)
{
    const float target = motionAllowed ? juce::jlimit (0.0f, 1.0f, activity) : 0.0f;
    const float before = activitySmoothed;
    activitySmoothed += (target - activitySmoothed) * juce::jmin (1.0f, dt * (target > activitySmoothed ? 6.0f : 2.0f));
    if (activitySmoothed < 0.004f)
        activitySmoothed = 0.0f;
    if (activitySmoothed > 0.0f)
        driftPhase += dt * (0.10f + 0.55f * activitySmoothed); // radians per second: a slow drift that quickens a little with level
    if (activitySmoothed > 0.0f || before > 0.0f)
        changed = true; // when it reaches zero the artwork stops and so does the redrawing
}

void SpectrumView::setShowWave (bool b)
{
    if (b != showWave)
    {
        showWave = b;
        geometryDirty = true;
        changed = true;
    }
}

void SpectrumView::setTrace (int index, const Levels& levels, bool shown)
{
    auto& t = traces[(size_t) index];
    if (shown != shownTrace[(size_t) index] || (shown && ! std::equal (t.begin(), t.end(), levels.begin())))
        changed = true;
    shownTrace[(size_t) index] = shown;
    if (shown)
        t = levels;
}

void SpectrumView::setWave (const analysis::WaveColumns& in, const analysis::WaveColumns& out)
{
    if (! showWave)
        return;
    if (in.hasSignal != waveIn.hasSignal || out.hasSignal != waveOut.hasSignal || (in.hasSignal && (in.lo != waveIn.lo || in.hi != waveIn.hi))
        || (out.hasSignal && (out.lo != waveOut.lo || out.hi != waveOut.hi)))
        changed = true;
    waveIn = in;
    waveOut = out;
}

void SpectrumView::commit()
{
    if (changed)
    {
        changed = false;
        ++repaintRequests;
        repaint();
    }
}

void SpectrumView::rebuildGeometry()
{
    using DS = analysis::DisplaySpectrum;
    geometryDirty = false;
    frame = getLocalBounds().toFloat();
    framePath = chamferPath (frame, 10.0f);
    plate = sculpture && backdrop.isValid() ? frame.reduced (plateInsetX, plateInsetY) : frame;
    platePath = chamferPath (plate, 8.0f);
    auto area = plate.reduced (12.0f, 10.0f);
    waveArea = {};
    if (showWave)
    {
        waveArea = area.removeFromBottom (juce::jmin (64.0f, area.getHeight() * 0.28f));
        area.removeFromBottom (6.0f);
    }
    labelRow = area.removeFromBottom (14.0f);
    legendRow = area.removeFromTop (14.0f);
    plot = area.withTrimmedTop (2.0f);
    const float topHz = (float) std::min (20000.0, 0.45 * sampleRate);
    auto xOf = [&] (float hz) { return plot.getX() + plot.getWidth() * std::log (hz / 20.0f) / std::log (topHz / 20.0f); };
    for (int c = 0; c < DS::columns; ++c)
        columnX[(size_t) c] = xOf (DS::columnHz (c, sampleRate));
    gridPath.clear();
    gridLabels.clear();
    for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        if (hz >= topHz)
            continue;
        const float x = xOf (hz);
        gridPath.addRectangle (x, plot.getY(), 1.0f, plot.getHeight());
        gridLabels.push_back ({ hz >= 1000.0f ? juce::String (hz / 1000.0f, 0) + "k" : juce::String ((int) hz), { x - 20.0f, labelRow.getY(), 40.0f, labelRow.getHeight() }, false });
    }
    for (float db : { -72.0f, -54.0f, -36.0f, -18.0f })
    {
        if (plot.getHeight() < 100.0f && db > -72.0f && db < -18.0f)
            continue;
        const float y = plot.getBottom() - plot.getHeight() * (db + 90.0f) / 84.0f;
        gridPath.addRectangle (plot.getX(), y, plot.getWidth(), 1.0f);
        gridLabels.push_back ({ juce::String ((int) db), { plot.getX() + 2.0f, y - 12.0f, 28.0f, 12.0f }, true });
    }
    linePath.preallocateSpace (3 * DS::columns + 16);
    fillPath.preallocateSpace (3 * DS::columns + 16);
}

void SpectrumView::paint (juce::Graphics& g)
{
    using DS = analysis::DisplaySpectrum;
    ++paintCount;
    if (geometryDirty)
        rebuildGeometry();
    g.setColour (juce::Colour (0xff0a070f));
    g.fillPath (framePath);
    const bool art = backdrop.isValid();
    if (art)
    {
        // pre-scaled once per size so every frame is a 1:1 copy; the drift (only while audio plays) moves it by a few pixels
        const float pixelScale = g.getInternalContext().getPhysicalPixelScaleFactor();
        const int pw = std::max (1, (int) std::ceil (frame.getWidth() * pixelScale)), ph = std::max (1, (int) std::ceil (frame.getHeight() * pixelScale));
        if (! backdropScaled.isValid() || backdropScaled.getWidth() != pw || backdropScaled.getHeight() != ph)
            backdropScaled = backdrop.rescaled (pw, ph, juce::Graphics::highResamplingQuality);
        const juce::Graphics::ScopedSaveState keep (g);
        g.reduceClipRegion (framePath);
        const float dx = 9.0f * std::sin (driftPhase) * activitySmoothed, dy = 3.0f * std::sin (driftPhase * 0.7f + 1.0f) * activitySmoothed;
        g.setOpacity (juce::jmin (1.0f, backdropStrength * (1.0f + 0.12f * activitySmoothed)));
        g.drawImage (backdropScaled, frame.translated (std::round (dx), std::round (dy)));
    }
    if (art && sculpture)
    {
        // the display: a dark glass plate set into the chrome, with a lit chrome rim
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff140e1c).withAlpha (plateGlass - 0.06f), 0.0f, plate.getY(), juce::Colour (0xff06040a).withAlpha (plateGlass + 0.06f), 0.0f, plate.getBottom(), false));
        g.fillPath (platePath);
        g.setGradientFill (juce::ColourGradient (chrome.withAlpha (0.07f), plate.getX(), plate.getY(), chrome.withAlpha (0.0f), plate.getX() + plate.getWidth() * 0.45f, plate.getY() + plate.getHeight() * 0.7f, false)); // a faint sheen: it reads as glass
        g.fillPath (platePath);
        juce::ColourGradient rim (chrome.withAlpha (0.85f), 0.0f, plate.getY(), pink.withAlpha (0.7f), 0.0f, plate.getBottom(), false);
        rim.addColour (0.35, violet.withAlpha (0.8f));
        g.setGradientFill (rim);
        g.strokePath (platePath, juce::PathStrokeType (1.6f));
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.strokePath (chamferPath (plate.reduced (1.6f), 7.0f), juce::PathStrokeType (1.0f));
    }
    g.setColour (border);
    g.strokePath (framePath, juce::PathStrokeType (1.2f));

    g.setColour (border.withAlpha (0.6f));
    g.fillPath (gridPath);
    g.setFont (fontOf (10.5f));
    g.setColour (textDim.withAlpha (0.85f));
    for (const auto& l : gridLabels)
        g.drawText (l.text, l.box, l.left ? juce::Justification::centredLeft : juce::Justification::centred, false);

    static const juce::Colour colours[3] = { violet, pink, juce::Colour (0xff9b91a5) };
    static const char* names[3] = { "VOCAL INPUT", "OUTPUT", "INSTRUMENTAL INPUT" };
    bool any = false;
    for (int t : { 2, 0, 1 }) // the beat behind, then the vocal, then the output
    {
        if (! shownTrace[(size_t) t])
            continue;
        any = true;
        linePath.clear();
        for (int c = 0; c < DS::columns; ++c)
        {
            const float x = columnX[(size_t) c], y = plot.getBottom() - plot.getHeight() * traces[(size_t) t][(size_t) c];
            c == 0 ? linePath.startNewSubPath (x, y) : linePath.lineTo (x, y);
        }
        fillPath = linePath;
        fillPath.lineTo (plot.getRight(), plot.getBottom());
        fillPath.lineTo (plot.getX(), plot.getBottom());
        fillPath.closeSubPath();
        g.setColour (colours[t].withAlpha (t == 1 ? 0.08f : 0.2f));
        g.fillPath (fillPath);
        if (art && ! sculpture) // a dark under-stroke keeps the line legible over the chrome
        {
            g.setColour (juce::Colour (0xff08060d).withAlpha (0.55f));
            g.strokePath (linePath, juce::PathStrokeType (t == 1 ? 3.6f : 4.0f, juce::PathStrokeType::curved));
        }
        g.setColour (colours[t].withAlpha (0.95f));
        g.strokePath (linePath, juce::PathStrokeType (t == 1 ? 1.4f : 1.8f, juce::PathStrokeType::curved));
    }
    float legendX = legendRow.getX();
    for (int t : { 0, 1, 2 })
        if (shownTrace[(size_t) t])
        {
            g.setColour (colours[t]);
            g.fillRect (legendX, legendRow.getY() + 3.0f, 8.0f, 8.0f);
            g.setFont (labFont (10.5f));
            g.drawText (names[t], juce::Rectangle<float> (legendX + 12.0f, legendRow.getY(), 150.0f, 14.0f), juce::Justification::centredLeft, false);
            legendX += 24.0f + (float) juce::GlyphArrangement::getStringWidth (labFont (10.5f), names[t]);
        }

    // waveform strip: the vocal input (violet) and the output (pink), newest on the right
    bool wave = false;
    if (showWave)
    {
        g.setColour (juce::Colour (0xff0c0910));
        g.fillRect (waveArea);
        g.setColour (border.withAlpha (0.8f));
        g.drawRect (waveArea, 1.0f);
        const float cy = waveArea.getCentreY(), half = waveArea.getHeight() * 0.5f - 2.0f;
        wave = waveIn.hasSignal || waveOut.hasSignal;
        if (wave) // an empty strip stays empty: no line that could look like a measurement
        {
            g.setColour (border);
            g.fillRect (waveArea.getX(), cy, waveArea.getWidth(), 1.0f);
        }
        if (wave)
        {
            const float step = waveArea.getWidth() / (float) analysis::WaveColumns::columns;
            for (int c = 0; c < analysis::WaveColumns::columns; ++c)
            {
                const float x = waveArea.getX() + (float) c * step;
                g.setColour (pink.withAlpha (0.55f));
                g.fillRect (x, cy - waveOut.hi[(size_t) c] * half, juce::jmax (1.0f, step - 0.5f), juce::jmax (1.0f, (waveOut.hi[(size_t) c] - waveOut.lo[(size_t) c]) * half));
                g.setColour (violet.withAlpha (0.9f));
                g.fillRect (x, cy - waveIn.hi[(size_t) c] * half, juce::jmax (1.0f, step - 0.5f), juce::jmax (1.0f, (waveIn.hi[(size_t) c] - waveIn.lo[(size_t) c]) * half));
            }
        }
    }
    if (! any && ! wave)
    {
        // idle: a still, quiet plate that says plainly that nothing is arriving; no trace, no baseline, nothing that looks like analysis
        const juce::String head = "NO SIGNAL", sub = showWave ? "PLAY AUDIO THROUGH THE PLUG-IN" : "";
        const auto headFont = labFont (showWave ? 15.0f : 12.0f), subFont = fontOf (10.5f);
        const float tw = (float) juce::jmax (juce::GlyphArrangement::getStringWidth (headFont, head), sub.isEmpty() ? 0 : juce::GlyphArrangement::getStringWidth (subFont, sub)) + 44.0f;
        const auto pill = juce::Rectangle<float> (tw, sub.isEmpty() ? 26.0f : 46.0f).withCentre (plot.getCentre().translated (0.0f, -6.0f));
        g.setColour (juce::Colour (0xff08060d).withAlpha (0.86f)); // dark enough to stay readable over the artwork
        g.fillPath (chamferPath (pill, 6.0f));
        g.setColour (border.brighter (0.2f));
        g.strokePath (chamferPath (pill, 6.0f), juce::PathStrokeType (1.0f));
        g.setColour (chrome.withAlpha (0.22f)); // the lit top edge
        g.fillRect (pill.getX() + 8.0f, pill.getY() + 1.0f, pill.getWidth() - 16.0f, 1.0f);
        g.setColour (textBright.withAlpha (0.92f));
        g.setFont (headFont);
        g.drawText (head, sub.isEmpty() ? pill : pill.withTrimmedBottom (18.0f), juce::Justification::centred, false);
        if (sub.isNotEmpty())
        {
            g.setColour (textDim);
            g.setFont (subFont);
            g.drawText (sub, pill.withTrimmedTop (26.0f), juce::Justification::centred, false);
        }
    }
}
}
