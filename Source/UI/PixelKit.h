#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include "PlaygroundWidgets.h"

// The 8-bit look: a 5x7 pixel font for headings, stepped-corner boxes, a dither tile, scanlines, a segmented meter and the level cards. Everything draws from
// the same locked palette as the rest of the interface (obsidian, violet, pink, chrome, divider violet; red only for clipping or an error). It runs on the
// message thread only. Nothing here invents data: the meters and the band chart show only what the caller passes in, and draw nothing while it passes nothing.
namespace glitchkid::ui
{
// 5x7 pixel text, `px` screen pixels per font pixel. Upper case only (other letters are upper-cased); an unknown character is drawn as "?".
int pixelTextWidth (const juce::String&, int px);
void drawPixelText (juce::Graphics&, const juce::String&, juce::Rectangle<int> area, int px, juce::Justification, juce::Colour);

// A rectangle with its four corners cut by one `notch` (the stepped 8-bit corner).
juce::Path pixelRectPath (juce::Rectangle<float>, float notch);
// Fill + 2 px outline, optionally with a hard, offset shadow block (no blur).
void drawPixelBox (juce::Graphics&, juce::Rectangle<float>, juce::Colour fill, juce::Colour outline, float notch = 3.0f, float shadow = 0.0f);
// Faint checkerboard dither over a rectangle, and horizontal scanlines over a whole area.
void drawDither (juce::Graphics&, juce::Rectangle<float>, float alpha);
void drawScanlines (juce::Graphics&, juce::Rectangle<int>, float alpha);

// A heading in the pixel font. Text only; it takes no clicks.
class PixelLabel : public juce::Component
{
public:
    explicit PixelLabel (const juce::String& initial = {}, int pixelSize = 2, juce::Colour c = textDim) : text (initial), px (pixelSize), colour (c) { setInterceptsMouseClicks (false, false); }
    void setText (const juce::String& t) { if (t != text) { text = t; repaint(); } }
    void setColour (juce::Colour c) { if (c != colour) { colour = c; repaint(); } }
    void setJustification (juce::Justification j) { just = j; }
    const juce::String& getText() const { return text; }
    void paint (juce::Graphics& g) override { drawPixelText (g, text, getLocalBounds(), px, just, colour); }

private:
    juce::String text;
    int px;
    juce::Colour colour;
    juce::Justification just = juce::Justification::centredLeft;
};

// A horizontal segmented meter, 0..1. It draws segments only for what it is given: with no signal source it is empty (setActive (false) also dims it), and
// 0 dBFS or above shows the last segment in red. The caller converts a real peak with peakToMeter().
class PixelBar : public juce::Component, public juce::SettableTooltipClient
{
public:
    void setValue (float level01, bool clipping);
    void setActive (bool);
    void paint (juce::Graphics&) override;
    static float peakToMeter (float linearPeak); // -60..0 dBFS -> 0..1 (0 for silence or NaN)

private:
    float level = 0.0f;
    bool clip = false, active = true;
};

// Five frequency bands, the vocal's energy share next to the beat's, as segmented columns. Only drawn from a real, valid BEAT LOCK analysis; otherwise it says
// NOT ANALYSED and draws no data. The marker above a band means the beat is likely to cover the voice there (an estimate from five coarse bands).
class BandCompare : public juce::Component, public juce::SettableTooltipClient
{
public:
    static constexpr int numBands = 5;
    void setData (bool valid, const std::array<float, numBands>& vocal, const std::array<float, numBands>& beat, const std::array<float, numBands>& risk);
    void paint (juce::Graphics&) override;

private:
    bool valid = false;
    std::array<float, numBands> vocal {}, beat {}, risk {};
};

// What a level card's status line means (colour only; the words always come from the real state).
enum class LevelStatus { idle, ready, active, problem };

// One level of the level-select screen. It always opens its level (nothing is locked) and its status is whatever the caller reads from the real plug-in state.
class LevelCard : public juce::Button
{
public:
    LevelCard() : juce::Button ("level") { setWantsKeyboardFocus (true); }
    void setContent (const juce::String& number, const juce::String& title, const juce::String& description);
    void setStatus (const juce::String& text, LevelStatus);
    const juce::String& getStatusText() const { return status; }
    const juce::String& getTitle() const { return title; }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    juce::String number, title, description, status;
    LevelStatus kind = LevelStatus::idle;
};
}
