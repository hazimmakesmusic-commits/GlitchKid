#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>

// The GlitchKid "playground" look: original, lightweight vector widgets (no images, fonts or network). Everything here runs on the message thread;
// nothing touches the audio thread. Animations are driven by the editor's single 30 Hz timer through tick(); when motion is switched off they
// jump straight to their end state, and nothing repaints unless something changed.
namespace glitchkid::ui
{
// Palette. Obsidian base; acid lime = primary/active; electric violet = atmosphere; hot pink = chaos, mutation, errors; chrome = highlights.
const juce::Colour obsidian (0xff0b0b10), panelTop (0xff1d1d27), panelBottom (0xff101016), lime (0xffb6ff00), violet (0xff8a2be2),
                   pink (0xffff2d95), chrome (0xffe3ebf7), chromeDark (0xff5b6478), textBright (0xfff2f2f8), textDim (0xff9a9aa8);

// A soft coloured halo behind a rounded rectangle: three translucent layers (cheap; no blur).
void drawGlow (juce::Graphics&, juce::Rectangle<float> r, juce::Colour c, float radius, float strength);
void drawGlowCircle (juce::Graphics&, juce::Rectangle<float> r, juce::Colour c, float radius, float strength); // the same halo around an ellipse

// The creature. It reacts to real state: mood (from the AutoMode state), input level (the real input peak) and the selected Sound World.
class Mascot : public juce::Component
{
public:
    enum class Mood { idle, listening, ready, error };
    void setMood (Mood);
    void setLevel (float linearPeak);  // real input peak, 0..1+
    void setLook (float x);            // -1..1: where the eyes look (the selected Sound World)
    void setMotion (bool on) { motion = on; }
    void cheer();                      // a short hop when something was applied
    void tick (float dtSeconds);
    void paint (juce::Graphics&) override;

private:
    Mood mood = Mood::idle;
    float level = 0.0f, shownLevel = 0.0f, look = 0.0f, shownLook = 0.0f;
    float bounce = 0.0f, bounceTime = 0.0f, blinkTimer = 2.5f, blink = 0.0f, clock = 0.0f;
    bool motion = true;
};

// A Sound World: a big card. Click (or Enter/Space when focused) selects it.
class WorldCard : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Icon { planet, heart, glitch };
    WorldCard (const juce::String& title, const juce::String& tagline, Icon, juce::Colour accent);
    void setLoads (const juce::String& presetName) { loads = presetName; repaint(); }
    void setSelected (bool, bool edited);
    void setMotion (bool on) { motion = on; }
    void tick (float dtSeconds);
    std::function<void()> onClick;
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { hovering = true; if (! motion) { hover = 1.0f; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hovering = false; pressed = false; if (! motion) { hover = 0.0f; repaint(); } }
    void mouseDown (const juce::MouseEvent&) override { pressed = true; repaint(); }
    void mouseUp (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }

private:
    juce::String title, tagline, loads;
    Icon icon;
    juce::Colour accent;
    bool selected = false, edited = false, hovering = false, pressed = false, motion = true;
    float hover = 0.0f, glow = 0.0f;
};

// A large dial for a macro. Drag up/down (Shift = fine), mouse wheel, arrow keys, double-click resets. Reports the new 0..1 value.
class MacroDial : public juce::Component, public juce::SettableTooltipClient
{
public:
    MacroDial (const juce::String& name, juce::Colour accent);
    void setValue (float v01);         // from the model; ignored while the user is dragging
    float getValue() const { return value; }
    bool isDragging() const { return dragging; }
    void setDefault (float v01) { defaultValue = v01; }
    void setEnabledLook (bool enabled, const juce::String& whyNot);
    void setMotion (bool on) { motion = on; }
    void tick (float dtSeconds);
    std::function<void (float)> onChange;
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { hovering = true; if (! motion) { hover = 1.0f; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hovering = false; if (! motion) { hover = 0.0f; repaint(); } }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = false; repaint(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (isEnabled()) change (defaultValue); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }

private:
    void change (float v);
    juce::String name;
    juce::Colour accent;
    float value = 0.0f, shown = 0.0f, defaultValue = 0.0f, dragStartValue = 0.0f, hover = 0.0f;
    bool dragging = false, hovering = false, motion = true;
};

// Recent input peaks as mirrored bars: a real, rolling view of what is coming in (about 4 seconds at 30 Hz).
class ScopeStrip : public juce::Component
{
public:
    void push (float linearPeak);
    void paint (juce::Graphics&) override;

private:
    static constexpr int numBars = 120;
    std::array<float, numBars> bars {};
    int head = 0;
};

// A status ring for the main action: idle / waiting for audio / working (real progress) / ready / error.
class StatusRing : public juce::Component
{
public:
    enum class State { idle, waiting, working, ready, error };
    void setState (State, float progress01);
    void setMotion (bool on) { motion = on; }
    void tick (float dtSeconds);
    void paint (juce::Graphics&) override;

private:
    State state = State::idle;
    float progress = 0.0f, spin = 0.0f;
    bool motion = true;
};

// One look for every existing control too: glossy rounded buttons, chrome-capped rotary knobs with a coloured arc, and slim linear sliders.
// Colours come from each control's own colour ids, so every knob keeps its meaning (lime / violet / pink).
class PlaygroundLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PlaygroundLookAndFeel();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
};
}
