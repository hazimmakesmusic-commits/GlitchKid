#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <functional>
#include <vector>
#include "../DSP/CleanLab.h"

// The GlitchKid "Vocal Lab" look: original, lightweight vector widgets (no images, no network). Everything here runs on the message thread; nothing touches
// the audio thread. Animation is driven by the editor's single 30 Hz timer through tick(); with motion off everything jumps to its end state, and nothing
// repaints unless something changed.
namespace glitchkid::ui
{
// A rectangle with small rounded corners (the radius follows `c`, capped at 6 px: the interface's one corner shape). drawGlow is kept so old call sites compile
// but draws nothing: selection is shown by fill and border, not by glow.
juce::Path chamferPath (juce::Rectangle<float>, float c);
void drawGlow (juce::Graphics&, const juce::Path&, juce::Colour, float radius);

// Palette (design tokens). Near-black window, raised panels and secondary surfaces in two graphite steps, off-white text and a softer secondary text, violet for
// selection and the main action, magenta sparingly for real processing activity and highlights, a neutral purple-grey for borders. Red means clipping or an error.
// Chrome is reserved for the wordmark and the idle emblem. The other colours are derived shades of these (hover, disabled, depth).
const juce::Colour obsidian (0xff101014), panelBottom (0xff1b1921), panelTop (0xff25222d), border (0xff3a3644),
                   violet (0xffa83bee), neon (0xffc26cf6), pink (0xffe14cbb),
                   textDim (0xffa7a0b2), textBright (0xfff0ecf5), disabled (0xff5a5463), errorRed (0xffff5c6c),
                   chrome (0xffe8ddf2), chromeDark (0xff4a4152);

// Display case: words written in capitals in the code ("CLEAN LAB", "NO SIGNAL") are drawn in Title Case at draw time, so the interface is not shouty. Acronyms and
// units in the keep list stay as they are, mixed-case words are left alone, and a component with the "rawText" property (user text such as a preset name) is never changed.
juce::String displayCase (const juce::String&, bool sentence = false); // sentence = only the first word is capitalised (statuses, hints)

// Inter (SIL Open Font License, bundled in Assets/Fonts and compiled in, so it is the same on every machine). Falls back to the system font if loading fails.
juce::Font fontOf (float height, bool bold = false);

// The GlitchKid dial: a thin violet value arc over a track around a flat cap with a pointer. `start`/`end` are JUCE rotary angles; pos is 0..1.
void drawDial (juce::Graphics&, juce::Rectangle<float> dial, float start, float end, float pos, juce::Colour accent, float alpha, bool hover, bool focus);

// One starting profile: name and a few words. Selected = violet frame; live (the profile currently loaded) = a small lime dot.
class ProfileCard : public juce::Button
{
public:
    ProfileCard() : juce::Button ("profile") { setWantsKeyboardFocus (true); }
    void bind (int profileIndex, bool selected, bool live);
    int getProfile() const { return profile; }
    std::function<void()> onDoubleClick;
    void mouseDoubleClick (const juce::MouseEvent&) override { if (onDoubleClick) onDoubleClick(); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    int profile = -1;
    bool selected = false, live = false;
};

// One key of the TUNE note pad (C ... B). Selected = violet; a small dot marks the notes the chosen scale allows (read from the real scale table).
// A change pulses the outline for about 0.3 s; tick() fades it and returns true while it is still moving.
class NoteKey : public juce::Button
{
public:
    explicit NoteKey (const juce::String& name) : juce::Button (name) { setWantsKeyboardFocus (true); }
    void setSelected (bool s) { if (s != selected) { selected = s; repaint(); } }
    void setInScale (bool s) { if (s != inScale) { inScale = s; repaint(); } }
    void setTarget (bool t) { if (t != target) { target = t; repaint(); } } // the note Pitch Tune is really pulling the voice to right now
    void pulse (bool motion) { pulseT = motion ? 1.0f : 0.0f; repaint(); }
    bool tick (float dt);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    bool selected = false, inScale = false, target = false;
    float pulseT = 0.0f;
};

// Recent input peaks as mirrored bars: a real, rolling view of what is coming in (about 4 seconds at 30 Hz). It shows nothing but measured peaks.
class ScopeStrip : public juce::Component, public juce::SettableTooltipClient
{
public:
    void push (float linearPeak);
    void paint (juce::Graphics&) override;

private:
    static constexpr int numBars = 120;
    std::array<float, numBars> bars {};
    int head = 0, nonZero = 0;
};

// A horizontal bar for a measured gain reduction (dB, positive), 0..maxDb, falling at a fixed rate; it shows only what the DSP reports.
class ReductionBar : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ReductionBar (float maxDb = 12.0f) : maxReduction (maxDb) {}
    void push (float reductionDb);
    void paint (juce::Graphics&) override;

private:
    float maxReduction, shown = 0.0f;
};

// What the main action is doing. Every state comes from the real analyzer / build state (nothing is simulated).
enum class AutoState { idle, waiting, working, ready, error };

// BUILD MY VOCAL MIX: the strongest element of AUTO. idle/ready = solid lime; waiting for audio = a sweeping highlight; working = a progress fill that follows the
// real analysis time; error = pink outline. It is an ordinary TextButton (text, clicks, focus).
class BuildButton : public juce::TextButton
{
public:
    BuildButton() : juce::TextButton ("BUILD MY VOCAL MIX") {}
    void setState (AutoState, float progress01);
    void setMotion (bool on) { motion = on; }
    void tick (float dtSeconds);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    AutoState state = AutoState::idle;
    float progress = 0.0f, sweep = 0.0f;
    bool motion = true;
};

// A thin technical panel: dark fill, divider-violet outline and small ultraviolet corner brackets (top-left and bottom-right).
void drawPanel (juce::Graphics&, juce::Rectangle<float> r);

// The EQ response graph and its four points, built on the real EQ parameters (eq_*): the curve is the combined RBJ biquad response of the four bands, calculated
// from the current parameter values with the same formulas the DSP uses (a test compares it with the real DSP output). There is no spectrum analyser in
// GlitchKid, so none is drawn. Dragging a point writes the real frequency and gain parameters (with host change gestures); the mouse wheel changes the
// selected bell's Q; double-click flattens a band. Changes made elsewhere (knobs, presets, automation, BUILD) move the points and curve via refresh().
class EqGraph : public juce::Component, public juce::SettableTooltipClient
{
public:
    static constexpr int numBands = 4; // LOW (shelf), BODY (bell), PRESENCE (bell), AIR (shelf)
    EqGraph (juce::AudioProcessorValueTreeState&, std::function<double()> sampleRate);
    int getSelectedBand() const { return selected; }
    void setSelectedBand (int);
    void refresh();                         // call from the editor's timer: repaints only if a parameter changed
    // CLEAN LAB's corrections that are really being applied (rumble high-pass, low-mid bell, up to three resonance bells), drawn as a second, pink curve.
    // Nothing is drawn while none is applied. cleanResponseDb() uses the same filter formulas as CleanLab.
    void setCleanApplied (const CleanLab::Applied&);
    static float cleanResponseDb (const CleanLab::Applied&, float atHz, double sampleRate);
    // The real spectrum of the vocal input, drawn faintly behind the curve: `n` levels (0..1) at log-spaced frequencies from 20 Hz to `topHz`. Pass n = 0 (or call
    // clearSpectrum) when there is no signal: nothing is drawn.
    void setSpectrum (const float* levels01, int n, float topHz);
    void clearSpectrum() { setSpectrum (nullptr, 0, 20000.0f); }
    std::function<void (int)> onBandSelected;
    juce::Point<float> pointOf (int band) const;   // component coordinates of a band's handle
    static float responseDb (const float freq[numBands], const float gain[numBands], const float q[numBands], float atHz, double sampleRate);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    float xOf (float hz) const;
    float yOf (float db) const;
    float hzAt (float x) const;
    float dbAt (float y) const;
    int bandNear (juce::Point<float>) const;
    void readParams (float* f, float* g, float* q) const;
    void setParam (const char* id, float value);
    void rebuildCurve();
    juce::AudioProcessorValueTreeState& apvts;
    std::function<double()> sampleRate;
    juce::Rectangle<float> plot;
    juce::Path curve, cleanCurve;
    std::vector<float> spec;
    float specTop = 20000.0f;
    CleanLab::Applied clean, shownClean;
    bool haveClean = false;
    float shownF[numBands] {}, shownG[numBands] {}, shownQ[numBands] {};
    int selected = 2, dragging = -1, hovered = -1;
    bool haveCurve = false;
};

// The processing chain INPUT -> TUNE -> TONE -> DYNAMICS -> SPACE -> OUTPUT. The four middle stages are the real stages of the engine and their dots show whether
// each is really doing something (from the actual parameters); clicking one opens its FINAL TOUCH category. INPUT and OUTPUT show the real peak level. While a
// signal is really arriving and motion is on, a small dot travels along the line.
class ChainStrip : public juce::Component, public juce::SettableTooltipClient
{
public:
    static constexpr int numStages = 4; // TUNE, TONE, DYNAMICS, SPACE
    void setState (const std::array<bool, numStages>& active, float inPeak, float outPeak, int selectedStage, bool motion, const juce::String& legacyNote);
    void tick (float dtSeconds);
    std::function<void (int)> onStage;
    juce::Rectangle<int> nodeBounds (int node) const; // 0 = INPUT, 1..4 = stages, 5 = OUTPUT
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    int nodeAt (juce::Point<int>) const;
    std::array<bool, numStages> active {};
    float inLevel = 0.0f, outLevel = 0.0f, phase = 0.0f; // levels 0..1 on a -60..0 dB scale
    int selected = -1;
    bool motion = true, signalPresent = false;
    juce::String legacy;
};

// One look for every control: flat graphite buttons with a thin outline, violet when selected, thin-arc rotary knobs, slim sliders and small switches.
class PlaygroundLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PlaygroundLookAndFeel();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
    // Tooltips: narrow, small, set beside the pointer (never under it), so a long tip does not blanket the controls around it.
    juce::Rectangle<int> getTooltipBounds (const juce::String& tip, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
};
}
