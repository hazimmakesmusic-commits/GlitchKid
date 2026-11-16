#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <functional>
#include <vector>
#include "../DSP/CleanLab.h"
#include "../DSP/ParametricEq.h"
#include "../Analysis/EqAnalyzer.h"

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
// ONE palette for both workspaces (HOME's nk:: names are aliases of these): ultraviolet and magenta are the GlitchKid accents.
// Everything below is LIVE: the current colour profile (Theme.h) sets the accents and the dark foundation they sit on. VIRIDIAN is the default (its
// teal-black foundation); the other profiles sit on the original graphite. The initial values here are VIRIDIAN's; Theme.cpp is the source of truth.
inline juce::Colour obsidian { 0xff080d0d },    // the window (VOID)
                    panelBottom { 0xff111a18 }, // panels (SURFACE)
                    panelTop { 0xff1b2925 },    // raised sections and controls (ELEVATED)
                    border { 0xff293a34 },      // separation (BORDER)
                    well { 0xff080d0d },        // inset wells inside a panel: fields, scopes, unselected keys
                    textDim { 0xff91a69c }, textBright { 0xfff1f7f4 }, disabled { 0xff53615c };
const juce::Colour errorRed (0xffff5c6c), chrome (0xffe8ddf2), chromeDark (0xff4a4152); // meaning, not brand: never profiled
inline juce::Colour violet { 0xff08776f }, // primary: selection fills (white text at AA on it), the main action
                    pink { 0xff00dda5 },   // secondary: the bright accent (value arcs, real activity, accent text)
                    neon { 0xff00b894 };   // soft: hover, focus, quiet highlights

// Display case: words written in capitals in the code ("CLEAN LAB", "NO SIGNAL") are drawn in Title Case at draw time, so the interface is not shouty. Acronyms and
// units in the keep list stay as they are, mixed-case words are left alone, and a component with the "rawText" property (user text such as a preset name) is never changed.
juce::String displayCase (const juce::String&, bool sentence = false); // sentence = only the first word is capitalised (statuses, hints)

// Inter (SIL Open Font License, bundled in Assets/Fonts and compiled in, so it is the same on every machine). Falls back to the system font if loading fails.
juce::Font fontOf (float height, bool bold = false);

// The GlitchKid dial, the same on HOME and in Advanced Mixing: a neon value arc that sweeps from ultraviolet to `accent` (to magenta when the accent is the
// ultraviolet itself), a soft halo, a dark machined cap lit from above and a white pointer. `start`/`end` are JUCE rotary angles; pos is 0..1; alpha < 1 = disabled.
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

// BUILD MY MIX: the strongest element of AUTO. idle/ready = solid lime; waiting for audio = a sweeping highlight; working = a progress fill that follows the
// real analysis time; error = pink outline. It is an ordinary TextButton (text, clicks, focus).
class BuildButton : public juce::TextButton
{
public:
    BuildButton() : juce::TextButton ("BUILD MY MIX") {}
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

// EQ LAB's graph. The curve is every band's real response (ParametricEq::responseDb: the audio's own coefficients), the shaded area and the line behind it are
// the real spectrum of the audio entering and leaving the EQ (EqAnalyzer on taps around the EQ: nothing synthetic), and every band that is on has a handle.
//   drag a handle: frequency and gain (gain types) | wheel on a handle: Q | double-click a handle: flatten | right-click a handle: type, slope, channels,
//   solo, duplicate, delete | double-click empty space: add a band there | wheel on empty space: zoom the frequency axis | drag empty space: pan |
//   Delete key: remove the selected band. The strip in the corner sets the dB range, fits the view, and sets the analyser's averaging, resolution and freeze.
// Every edit writes the real parameters with host gestures; changes made elsewhere (knobs, presets, automation, BUILD) move the handles via refresh().
class EqGraph : public juce::Component, public juce::SettableTooltipClient
{
public:
    static constexpr int numBands = ParametricEq::numBands;
    EqGraph (juce::AudioProcessorValueTreeState&, std::function<double()> sampleRate);
    ~EqGraph() override;
    int getSelectedBand() const { return selected; }
    void setSelectedBand (int);
    void refresh();                         // call from the editor's timer: repaints only if a parameter changed
    // CLEAN LAB's corrections that are really being applied (rumble high-pass, low-mid bell, up to three resonance bells), drawn as a second curve.
    // Nothing is drawn while none is applied. cleanResponseDb() uses the same filter formulas as CleanLab.
    void setCleanApplied (const CleanLab::Applied&);
    static float cleanResponseDb (const CleanLab::Applied&, float atHz, double sampleRate);
    // The analyser: the audio just before and just after the EQ (the processor's EQ taps). Call from the editor's visual timer.
    void analyse (const analysis::DisplayTap& eqIn, const analysis::DisplayTap& eqOut, double sampleRate, float dtSeconds);
    std::function<void (int)> onBandSelected;
    std::function<void (int)> onSolo;           // a band (or -1): the editor tells the processor; monitoring only
    std::function<float (int)> dynamicDbOf;     // a band's dynamic gain change right now (the processor's); drawn live
    void setSoloBand (int b) { soloBand = b; repaint(); }
    int addBandAt (float hz, float db);         // switches on a free band there (bands 5-16 first); -1 when all 16 are in use
    int duplicateBand (int band);               // copies a band to a free one, a little higher; -1 when none is free
    void removeBand (int band);                 // switches it off
    juce::Point<float> pointOf (int band) const;   // component coordinates of a band's handle
    void setView (float loHz, float hiHz);      // the visible frequency span (clamped to 20 Hz - 20 kHz, at least 2 octaves)
    juce::Range<float> getView() const { return { viewLo, viewHi }; }
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    float xOf (float hz) const;
    float yOf (float db) const;
    float hzAt (float x) const;
    float dbAt (float y) const;
    float yOfLevel (float dbfs) const;          // the analyser's scale: -90 .. 0 dBFS over the plot
    int bandNear (juce::Point<float>) const;
    void readAll (EqBandSettings* bands) const; // all 16 EQ LAB bands, as the audio sees them
    void setParam (const char* id, float value, bool gesture = true);
    void rebuildCurve();
    void showBandMenu (int band);
    void updateStrip();
    juce::AudioProcessorValueTreeState& apvts;
    std::function<double()> sampleRate;
    juce::Rectangle<float> plot;
    juce::Path curve, selectedCurve, cleanCurve, liveCurve;
    std::array<float, ParametricEq::numBands> dynNow {};
    bool haveLive = false;
    CleanLab::Applied clean, shownClean;
    bool haveClean = false;
    EqBandSettings shown[numBands];
    std::array<float, numBands * 7> shownAll {}; // every band's values when the curve was drawn
    int selected = 2, dragging = -1, hovered = -1, soloBand = -1;
    bool haveCurve = false, panning = false;
    float viewLo = 20.0f, viewHi = 20000.0f, rangeDb = 15.0f, panStartLo = 20.0f, panStartX = 0.0f;
    juce::Point<float> mouse { -1.0f, -1.0f };
    analysis::EqAnalyzer specIn, specOut;
    bool inLive = false, outLive = false;
    float peakHz = 0.0f, peakDb = -120.0f;
    juce::TextButton rangeButton, fitButton, avgButton, resButton, freezeButton;
};

// One look for every control: flat graphite buttons with a thin outline, violet when selected, thin-arc rotary knobs, slim sliders and small switches.
class PlaygroundLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PlaygroundLookAndFeel();
    void applyTheme(); // the colours taken from the current colour profile
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
};
}
