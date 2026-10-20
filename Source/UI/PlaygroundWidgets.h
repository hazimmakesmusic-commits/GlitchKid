#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>

// The GlitchKid "Vocal Lab" look: original, lightweight vector widgets (no images, no network). Everything here runs on the message thread; nothing touches
// the audio thread. Animation is driven by the editor's single 30 Hz timer through tick(); with motion off everything jumps to its end state, and nothing
// repaints unless something changed.
namespace glitchkid::ui
{
// Palette (design tokens). Graphite surfaces; muted ultraviolet marks selection and secondary accents; acid lime is reserved for the primary action, active
// processing and genuine success; cyan is only for signal information; pink marks "dry" and warnings; red means clipping or an error.
const juce::Colour obsidian (0xff0a0b0f), panelBottom (0xff12141a), panelTop (0xff161922), border (0xff2a2e3a),
                   violet (0xff8a5cf0), neon (0xffa98bff), lime (0xffb7ff3c), cyan (0xff36c6dc), pink (0xffff6b8f),
                   textDim (0xff9aa2b5), textBright (0xffeef0f6), disabled (0xff454957), errorRed (0xffff496a),
                   chrome (0xffeef0f6), chromeDark (0xff454957);

// Inter (SIL Open Font License, bundled in Assets/Fonts and compiled in, so it is the same on every machine). Falls back to the system font if loading fails.
juce::Font fontOf (float height, bool bold = false);

// The GlitchKid dial: a thin value arc over a track around a dark cap with a pointer. `start`/`end` are JUCE rotary angles; pos is 0..1.
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
    void pulse (bool motion) { pulseT = motion ? 1.0f : 0.0f; repaint(); }
    bool tick (float dt);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    bool selected = false, inScale = false;
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
    int head = 0;
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

// The processing chain INPUT -> TUNE -> TONE -> DYNAMICS -> SPACE -> OUTPUT. The four middle stages are the real stages of the engine and their dots show whether
// each is really doing something (from the actual parameters); clicking one opens its MODIFY category. INPUT and OUTPUT show the real peak level. While a
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
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
};
}
