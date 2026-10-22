#include "LabKit.h"

namespace glitchkid::ui
{
juce::Font labFont (float height) { return fontOf (height, true).withExtraKerningFactor (0.1f); }

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

void WorkspaceTile::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const auto accent = kind == TileStatus::problem ? errorRed : kind == TileStatus::active ? pink : kind == TileStatus::ready ? violet : textDim;
    const auto shape = chamferPath (r, 12.0f);
    if (prominent || highlighted)
        drawGlow (g, shape, prominent ? violet.withAlpha (0.55f) : violet.withAlpha (0.3f), 8.0f);
    g.setColour (prominent ? juce::Colour (0xff2a1238) : (highlighted || down ? panelTop.brighter (0.05f) : panelBottom));
    g.fillPath (shape);
    g.setColour (prominent ? violet : (highlighted ? neon : border.brighter (0.15f)));
    g.strokePath (shape, juce::PathStrokeType (prominent ? 2.0f : 1.3f));
    g.setColour (accent);
    g.fillRect (r.getX() + 14.0f, r.getBottom() - 4.0f, 28.0f, 3.0f);
    auto in = r.toNearestInt().reduced (16, 12);
    g.setColour (chrome);
    g.setFont (labFont (prominent ? 18.0f : 15.0f));
    g.drawFittedText (title.toUpperCase(), in.removeFromTop (prominent ? 26 : 22), juce::Justification::centredLeft, 1, 0.75f);
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
    auto area = frame.reduced (12.0f, 10.0f);
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
        g.setColour (border);
        g.fillRect (waveArea.getX(), cy, waveArea.getWidth(), 1.0f);
        wave = waveIn.hasSignal || waveOut.hasSignal;
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
        g.setColour (textDim);
        g.setFont (labFont (showWave ? 13.0f : 12.0f));
        g.drawText (showWave ? "NO SIGNAL - PLAY AUDIO THROUGH THE PLUG-IN" : "NO SIGNAL", plot, juce::Justification::centred, false);
    }
}
}
