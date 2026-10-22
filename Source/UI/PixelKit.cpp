#include "PixelKit.h"

namespace glitchkid::ui
{
namespace
{
struct Glyph { char c; unsigned char rows[7]; }; // each row: 5 bits, bit 4 = leftmost pixel

constexpr Glyph glyphs[] = {
    { 'A', { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 } }, { 'B', { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E } },
    { 'C', { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E } }, { 'D', { 0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E } },
    { 'E', { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F } }, { 'F', { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 } },
    { 'G', { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F } }, { 'H', { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 } },
    { 'I', { 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E } }, { 'J', { 0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C } },
    { 'K', { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 } }, { 'L', { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F } },
    { 'M', { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 } }, { 'N', { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 } },
    { 'O', { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } }, { 'P', { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 } },
    { 'Q', { 0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D } }, { 'R', { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 } },
    { 'S', { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E } }, { 'T', { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 } },
    { 'U', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } }, { 'V', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04 } },
    { 'W', { 0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11 } }, { 'X', { 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11 } },
    { 'Y', { 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04 } }, { 'Z', { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F } },
    { '0', { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E } }, { '1', { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E } },
    { '2', { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F } }, { '3', { 0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E } },
    { '4', { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 } }, { '5', { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E } },
    { '6', { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E } }, { '7', { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 } },
    { '8', { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E } }, { '9', { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C } },
    { '.', { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C } }, { ',', { 0x00, 0x00, 0x00, 0x00, 0x0C, 0x04, 0x08 } },
    { ':', { 0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00 } }, { '-', { 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00 } },
    { '+', { 0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00 } }, { '/', { 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10 } },
    { '%', { 0x19, 0x1A, 0x02, 0x04, 0x08, 0x0B, 0x13 } }, { '#', { 0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A } },
    { '(', { 0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02 } }, { ')', { 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08 } },
    { '<', { 0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02 } }, { '>', { 0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08 } },
    { '!', { 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04 } }, { '?', { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04 } },
    { '&', { 0x0C, 0x12, 0x14, 0x08, 0x15, 0x12, 0x0D } }, { '=', { 0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00 } },
    { '_', { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F } }, { '\'', { 0x04, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00 } },
    { '*', { 0x00, 0x15, 0x0E, 0x1F, 0x0E, 0x15, 0x00 } }, { ' ', { 0, 0, 0, 0, 0, 0, 0 } },
};

const Glyph& glyphFor (juce::juce_wchar ch)
{
    const char c = ch < 128 ? (char) std::toupper ((int) ch) : '?';
    for (const auto& g : glyphs)
        if (g.c == c)
            return g;
    for (const auto& g : glyphs)
        if (g.c == '?')
            return g;
    return glyphs[0];
}

juce::Image makeDitherTile()
{
    juce::Image img (juce::Image::ARGB, 2, 2, true);
    img.setPixelAt (0, 0, juce::Colours::white);
    img.setPixelAt (1, 1, juce::Colours::white);
    return img;
}

juce::Image makeScanTile()
{
    juce::Image img (juce::Image::ARGB, 1, 3, true);
    img.setPixelAt (0, 0, juce::Colours::black);
    return img;
}
}

int pixelTextWidth (const juce::String& text, int px)
{
    return text.isEmpty() ? 0 : text.length() * 6 * px - px;
}

void drawPixelText (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area, int px, juce::Justification just, juce::Colour colour)
{
    if (text.isEmpty() || px < 1)
        return;
    const int w = pixelTextWidth (text, px), h = 7 * px;
    int x = area.getX();
    if (just.testFlags (juce::Justification::horizontallyCentred))
        x = area.getCentreX() - w / 2;
    else if (just.testFlags (juce::Justification::right))
        x = area.getRight() - w;
    int y = area.getY();
    if (just.testFlags (juce::Justification::verticallyCentred))
        y = area.getCentreY() - h / 2;
    else if (just.testFlags (juce::Justification::bottom))
        y = area.getBottom() - h;
    g.setColour (colour);
    int cx = x;
    for (auto it = text.begin(); it != text.end(); ++it)
    {
        const auto& gl = glyphFor (*it);
        for (int row = 0; row < 7; ++row)
        {
            int col = 0;
            while (col < 5)
            {
                if ((gl.rows[row] >> (4 - col)) & 1)
                {
                    int run = 1;
                    while (col + run < 5 && ((gl.rows[row] >> (4 - col - run)) & 1))
                        ++run;
                    g.fillRect (cx + col * px, y + row * px, run * px, px);
                    col += run;
                }
                else
                    ++col;
            }
        }
        cx += 6 * px;
    }
}

juce::Path pixelRectPath (juce::Rectangle<float> r, float n)
{
    n = juce::jmin (n, r.getWidth() * 0.5f, r.getHeight() * 0.5f);
    juce::Path p;
    p.startNewSubPath (r.getX() + n, r.getY());
    p.lineTo (r.getRight() - n, r.getY());
    p.lineTo (r.getRight() - n, r.getY() + n);
    p.lineTo (r.getRight(), r.getY() + n);
    p.lineTo (r.getRight(), r.getBottom() - n);
    p.lineTo (r.getRight() - n, r.getBottom() - n);
    p.lineTo (r.getRight() - n, r.getBottom());
    p.lineTo (r.getX() + n, r.getBottom());
    p.lineTo (r.getX() + n, r.getBottom() - n);
    p.lineTo (r.getX(), r.getBottom() - n);
    p.lineTo (r.getX(), r.getY() + n);
    p.lineTo (r.getX() + n, r.getY() + n);
    p.closeSubPath();
    return p;
}

void drawPixelBox (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill, juce::Colour outline, float notch, float shadow)
{
    if (shadow > 0.0f)
    {
        g.setColour (juce::Colour (0xff030206));
        g.fillPath (pixelRectPath (r.translated (shadow, shadow), notch));
    }
    const auto shape = pixelRectPath (r, notch);
    g.setColour (fill);
    g.fillPath (shape);
    g.setColour (outline);
    g.strokePath (pixelRectPath (r.reduced (1.0f), juce::jmax (0.0f, notch - 1.0f)), juce::PathStrokeType (2.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
}

void drawDither (juce::Graphics& g, juce::Rectangle<float> r, float alpha)
{
    static const juce::Image tile = makeDitherTile();
    const juce::Graphics::ScopedSaveState keep (g);
    g.reduceClipRegion (r.toNearestInt());
    g.setTiledImageFill (tile, 0, 0, alpha);
    g.fillRect (r);
}

void drawScanlines (juce::Graphics& g, juce::Rectangle<int> r, float alpha)
{
    static const juce::Image tile = makeScanTile();
    const juce::Graphics::ScopedSaveState keep (g);
    g.reduceClipRegion (r);
    g.setTiledImageFill (tile, 0, 0, alpha);
    g.fillRect (r);
}

// ---- PixelBar --------------------------------------------------------------------------------------------------------------------------

float PixelBar::peakToMeter (float peak)
{
    if (! std::isfinite (peak) || peak <= 0.0f)
        return 0.0f;
    return juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (peak, -60.0f) + 60.0f) / 60.0f);
}

void PixelBar::setValue (float level01, bool clipping)
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

void PixelBar::setActive (bool a)
{
    if (a != active)
    {
        active = a;
        repaint();
    }
}

void PixelBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0c0910));
    g.fillRect (r);
    g.setColour (border);
    g.drawRect (r, 2.0f);
    r = r.reduced (4.0f);
    const int seg = juce::jmax (4, (int) (r.getWidth() / 8.0f));
    const float w = r.getWidth() / (float) seg;
    const int lit = (int) std::lround (level * (float) seg);
    for (int i = 0; i < seg; ++i)
    {
        const bool on = i < lit && active;
        juce::Colour c = ! on ? border.withAlpha (0.45f) : (clip && i == seg - 1 ? errorRed : (i >= seg * 3 / 4 ? pink : violet));
        g.setColour (c);
        g.fillRect (r.getX() + (float) i * w, r.getY(), w - 2.0f, r.getHeight());
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
    drawPixelBox (g, r, juce::Colour (0xff0c0910), border, 3.0f);
    auto inner = getLocalBounds().reduced (10, 8);
    auto legend = inner.removeFromTop (14);
    g.setColour (violet);
    g.fillRect (legend.getX(), legend.getY() + 2, 8, 8);
    g.setColour (pink);
    g.fillRect (legend.getX() + 70, legend.getY() + 2, 8, 8);
    g.setFont (fontOf (11.0f, true));
    g.setColour (textDim);
    g.drawText ("VOCAL", legend.getX() + 12, legend.getY(), 54, 12, juce::Justification::centredLeft, false);
    g.drawText ("BEAT", legend.getX() + 82, legend.getY(), 54, 12, juce::Justification::centredLeft, false);
    auto labels = inner.removeFromBottom (16);
    inner.removeFromTop (10);
    const int colW = inner.getWidth() / numBands;
    float top = 0.01f;
    if (valid)
        for (int i = 0; i < numBands; ++i)
            top = juce::jmax (top, vocal[(size_t) i], beat[(size_t) i]);
    for (int i = 0; i < numBands; ++i)
    {
        auto col = juce::Rectangle<int> (inner.getX() + i * colW, inner.getY(), colW, inner.getHeight()).reduced (6, 0);
        g.setColour (textDim);
        g.setFont (fontOf (10.5f, true));
        g.drawText (names[i], labels.getX() + i * colW, labels.getY(), colW, labels.getHeight(), juce::Justification::centred, false);
        const int segs = juce::jmax (4, col.getHeight() / 6);
        for (int k = 0; k < 2; ++k)
        {
            auto bar = k == 0 ? col.removeFromLeft (col.getWidth() / 2 - 1) : col.withTrimmedLeft (2);
            const float v = valid ? (k == 0 ? vocal[(size_t) i] : beat[(size_t) i]) / top : 0.0f;
            const int lit = (int) std::lround (juce::jlimit (0.0f, 1.0f, v) * (float) segs);
            for (int s = 0; s < segs; ++s)
            {
                const float sy = (float) bar.getBottom() - (float) (s + 1) * (float) bar.getHeight() / (float) segs;
                g.setColour (s < lit ? (k == 0 ? violet : pink) : border.withAlpha (0.4f));
                g.fillRect ((float) bar.getX(), sy, (float) bar.getWidth(), (float) bar.getHeight() / (float) segs - 2.0f);
            }
        }
        if (valid && risk[(size_t) i] > 0.35f)
            drawPixelText (g, "!", { inner.getX() + i * colW, inner.getY() - 10, colW, 10 }, 1, juce::Justification::centred, pink);
    }
    if (! valid)
        drawPixelText (g, "NOT ANALYSED", getLocalBounds(), 2, juce::Justification::centred, textDim);
}

// ---- LevelCard -------------------------------------------------------------------------------------------------------------------------

void LevelCard::setContent (const juce::String& n, const juce::String& t, const juce::String& d)
{
    if (n != number || t != title || d != description)
    {
        number = n;
        title = t;
        description = d;
        setButtonText (t); // the name tests and screen readers see
        repaint();
    }
}

void LevelCard::setStatus (const juce::String& statusText, LevelStatus k)
{
    if (statusText != status || k != kind)
    {
        status = statusText;
        kind = k;
        repaint();
    }
}

void LevelCard::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (3.0f).withTrimmedRight (4.0f).withTrimmedBottom (4.0f);
    if (down)
        r = r.translated (2.0f, 2.0f);
    const auto accent = kind == LevelStatus::problem ? errorRed : kind == LevelStatus::active ? pink : kind == LevelStatus::ready ? violet : textDim;
    drawPixelBox (g, r, highlighted || down ? panelTop.brighter (0.06f) : panelBottom, highlighted ? neon : (kind == LevelStatus::idle ? border : accent), 6.0f, down ? 0.0f : 4.0f);
    drawDither (g, r.reduced (3.0f), 0.05f);
    auto in = r.toNearestInt().reduced (18, 14);
    drawPixelText (g, number, in.removeFromTop (14), 2, juce::Justification::centredLeft, violet);
    in.removeFromTop (6);
    drawPixelText (g, title, in.removeFromTop (24), 3, juce::Justification::centredLeft, chrome);
    in.removeFromTop (8);
    g.setColour (textDim);
    g.setFont (fontOf (13.0f));
    g.drawFittedText (description, in.removeFromTop (juce::jmax (36, in.getHeight() - 26)), juce::Justification::topLeft, 3, 0.9f);
    auto line = in.removeFromBottom (22);
    g.setColour (accent);
    g.fillRect (line.getX(), line.getCentreY() - 5, 10, 10);
    g.setFont (fontOf (13.0f, true));
    g.drawFittedText (status, line.withTrimmedLeft (18), juce::Justification::centredLeft, 1, 0.8f);
    if (hasKeyboardFocus (true))
    {
        g.setColour (chrome);
        g.strokePath (pixelRectPath (r.expanded (3.0f), 6.0f), juce::PathStrokeType (1.5f));
    }
}
}
