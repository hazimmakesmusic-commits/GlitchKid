#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <vector>
#include "PlaygroundWidgets.h"
#include "../Analysis/DisplaySpectrum.h"

// Experimental Lab components: bold tracked headings, a smooth level bar, the vocal/beat band chart, the workspace tiles of the HOME screen and the real-time
// spectrum + waveform view. Message thread only. Nothing here invents data: every view draws only what the caller passes in and shows NO SIGNAL otherwise.
namespace glitchkid::ui
{
// Bold, upper-case, letter-spaced face for headings and technical labels (Inter Bold, tracked out).
juce::Font labFont (float height);

// A section heading: a short pink tick and tracked bold capitals. Text only; it takes no clicks.
class LabHeading : public juce::Component
{
public:
    explicit LabHeading (const juce::String& initial = {}, float size = 13.0f, juce::Colour c = chrome) : text (initial), fontSize (size), colour (c) { setInterceptsMouseClicks (false, false); }
    void setText (const juce::String& t) { if (t != text) { text = t; repaint(); } }
    void setColour (juce::Colour c) { if (c != colour) { colour = c; repaint(); } }
    const juce::String& getText() const { return text; }
    void paint (juce::Graphics&) override;

private:
    juce::String text;
    float fontSize;
    juce::Colour colour;
};

// A horizontal level bar, 0..1, rising at once and falling at a fixed rate. Violet to pink; red only at full scale. Empty (and dimmed with setActive (false)) when
// there is no source. The caller converts a real peak with peakToMeter().
class LevelBar : public juce::Component, public juce::SettableTooltipClient
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

// Five broad bands: the vocal's share of energy next to the beat's, from a valid BEAT LOCK analysis only (otherwise: NOT ANALYSED, no data drawn). A marker above a
// band means the beat is likely to cover the voice there (an estimate from five coarse bands).
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

enum class TileStatus { idle, ready, active, problem };

// One workspace shortcut on the HOME screen. Its status line is whatever the caller reads from real plug-in state.
class WorkspaceTile : public juce::Button
{
public:
    WorkspaceTile() : juce::Button ("tile") { setWantsKeyboardFocus (true); }
    void setContent (const juce::String& title, const juce::String& description, bool prominent = false);
    void setStatus (const juce::String& text, TileStatus);
    // A small schematic pictogram of what the workspace is (0 CLEAN LAB, 1 BEAT LOCK, 2 BUILD, 3 FINAL TOUCH). Decoration: it never shows a measurement.
    void setGlyph (int index) { glyph = index; repaint(); }
    const juce::String& getStatusText() const { return status; }
    const juce::String& getTitle() const { return title; }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    juce::String title, description, status;
    TileStatus kind = TileStatus::idle;
    bool prominent = false;
    int glyph = -1;
};

// The central real-time display: spectrum traces (vocal input before processing, output after it, and the Instrumental input only when it is really live) and a
// waveform strip of the vocal input and the output. All data comes from DisplaySpectrum / WaveColumns (the audio taps); with no signal it says so.
// Cost control: setTrace / setWave only store (and note whether anything changed); commit() repaints once, and only if something did change, so a silent or
// frozen display costs nothing. The frequency-axis positions and the grid are computed once per size / sample rate and reused by every paint.
class SpectrumView : public juce::Component, public juce::SettableTooltipClient
{
public:
    using Levels = std::array<float, analysis::DisplaySpectrum::columns>;
    void setTrace (int index, const Levels& levels, bool shown); // 0 = vocal input, 1 = output, 2 = instrumental input
    void setWave (const analysis::WaveColumns& input, const analysis::WaveColumns& output);
    void commit();
    void setSampleRate (double sr);
    void setShowWave (bool b);
    // The decorative backdrop (the Glitch Relic), drawn dimmed behind the grid and traces. It carries no data; pass an empty image for none.
    void setBackdrop (const juce::Image& image, float strength);
    // Sculpture mode: the display becomes a dark glass plate set into the artwork, so the chrome shows around it (and faintly through it) instead of sitting behind everything.
    static constexpr float plateInsetX = 56.0f, plateInsetY = 8.0f, plateGlass = 0.88f;
    void setSculpture (bool on);
    // Playback-driven drift of the artwork. 'activity' (0..1) must come from the real, already displayed signal; with 0 (or motion off) the artwork stands still.
    void setActivity (float dt, float activity, bool motionAllowed);
    float getDriftForTest() const { return driftPhase; }
    int getPaintCountForTest() const { return paintCount; }
    int getRepaintRequestsForTest() const { return repaintRequests; } // how many times a change asked for a redraw
    void paint (juce::Graphics&) override;
    void resized() override { geometryDirty = true; }

private:
    void rebuildGeometry();
    std::array<Levels, 3> traces {};
    std::array<bool, 3> shownTrace { { false, false, false } };
    analysis::WaveColumns waveIn, waveOut;
    double sampleRate = 48000.0;
    bool showWave = true, changed = false, geometryDirty = true;
    int paintCount = 0, repaintRequests = 0;
    juce::Image backdrop, backdropScaled;
    float backdropStrength = 0.0f, activitySmoothed = 0.0f, driftPhase = 0.0f;
    bool sculpture = false;
    juce::Rectangle<float> plate;
    juce::Path platePath;
    // cached geometry (rebuilt only when the size, the sample rate or the wave strip changes)
    juce::Rectangle<float> frame, plot, labelRow, legendRow, waveArea;
    std::array<float, analysis::DisplaySpectrum::columns> columnX {};
    juce::Path gridPath, framePath;
    struct GridLabel { juce::String text; juce::Rectangle<float> box; bool left; };
    std::vector<GridLabel> gridLabels;
    juce::Path linePath, fillPath; // reused by every trace, every paint
};
}
