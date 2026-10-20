#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>

// The GlitchKid "playground" look: original, lightweight vector widgets (no images, fonts or network). Everything here runs on the message thread;
// nothing touches the audio thread. Animations are driven by the editor's single 30 Hz timer through tick(); when motion is switched off they
// jump straight to their end state, and nothing repaints unless something changed.
namespace glitchkid::ui
{
// Palette (design tokens). Matte black surfaces; electric purple is the brand colour; lime is reserved for active states, toggles and important feedback;
// cyan and pink only help tell effects apart; red means clipping or an error.
const juce::Colour obsidian (0xff08090d), panelBottom (0xff101219), panelTop (0xff151923), border (0xff252b38),
                   violet (0xff963bff), neon (0xffb56aff), lime (0xffb7ff3c), cyan (0xff24d8f2), pink (0xffff4fa3),
                   textDim (0xff9299aa), textBright (0xfff4f3f8), disabled (0xff454957), errorRed (0xffff496a),
                   chrome (0xfff4f3f8), chromeDark (0xff454957);

// Inter (SIL Open Font License, bundled in Assets/Fonts and compiled in, so it is the same on every machine). Falls back to the system font if loading fails.
juce::Font fontOf (float height, bool bold = false);

// A soft coloured halo behind a rounded rectangle: three translucent layers (cheap; no blur).
void drawGlow (juce::Graphics&, juce::Rectangle<float> r, juce::Colour c, float radius, float strength);
void drawGlowCircle (juce::Graphics&, juce::Rectangle<float> r, juce::Colour c, float radius, float strength); // the same halo around an ellipse

// The GlitchKid dial: a ring of 27 LED segments (lit up to `pos`, 0..1) around a dark knurled metal cap with a glowing needle and a small lens.
// `start`/`end` are JUCE rotary angles (radians from 12 o'clock). Used by every rotary control, so all knobs look alike.
void drawLedDial (juce::Graphics&, juce::Rectangle<float> dial, float start, float end, float pos, juce::Colour accent, float alpha, float hover, bool focus);

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

// A Sound World: a hull pod with a status light. Click (or Enter/Space when focused) loads it.
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
    float hover = 0.0f, glow = 0.0f, phase = 0.0f;
    int frame = 0;
};

// One card for a starting profile: an original alien "sigil" (drawn from the profile's number, so every card has its own), the profile name, a few words about
// it, and a status: selected (lime frame) or live (the loaded sound). No artwork of any artist is used.
class ProfileCard : public juce::Button
{
public:
    ProfileCard() : juce::Button ("profile") { setWantsKeyboardFocus (true); }
    void bind (int profileIndex, bool selected, bool live);
    int getProfile() const { return profile; }
    void setMotion (bool on) { motion = on; }
    void tick (float dt);              // eases hover / selection glow; repaints only while something is still moving
    std::function<void()> onDoubleClick;
    void mouseDoubleClick (const juce::MouseEvent&) override { if (onDoubleClick) onDoubleClick(); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    int profile = -1;
    bool selected = false, live = false, motion = true;
    float glow = 0.0f, hover = 0.0f;
};

// One key of the TUNE note pad (C ... B). Selected = lime; a small dot marks the notes the chosen scale allows (read from the real scale table).
// Choosing a key sends a short ring of light outwards (pulse), which tick() fades; tick() returns true while it is still moving, so nothing repaints at rest.
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

// What the main action is doing. Shown by the reactor button's ring; every state comes from the real analyzer state (nothing is simulated).
enum class AutoState { idle, waiting, working, ready, error };

// The big ANALYZE VOCAL button: a lime "core" inside a ring of 24 segments. idle = dim ring; waiting for audio = a bright chase around the ring;
// working = segments light up with the real progress; ready = full lime ring; error = pink ring. It is an ordinary TextButton (text, clicks, focus).
class ReactorButton : public juce::TextButton
{
public:
    ReactorButton() : juce::TextButton ("BUILD MY VOCAL MIX") {}
    void setState (AutoState, float progress01);
    void setMotion (bool on) { motion = on; }
    void tick (float dtSeconds);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    AutoState state = AutoState::idle;
    float progress = 0.0f, spin = 0.0f;
    bool motion = true;
};

// A hull plate outline: chamfered corners (shape "plate") or a slanted tab (shape "tab"). Buttons pick the shape with the component property "shape".
juce::Path makePlatePath (juce::Rectangle<float> r, bool tab);

// A small vector icon for each effect (juce effect index, see EffectModel.h), drawn with strokes in `colour` inside `area`.
void drawEffectIcon (juce::Graphics&, int effect, juce::Rectangle<float> area, juce::Colour colour);
// The crown mark used with the wordmark.
void drawCrown (juce::Graphics&, juce::Rectangle<float> area, juce::Colour colour);

// A tile in the EFFECTS grid: icon, name, a state light. `state`: -1 = always in the chain and neutral/shaped (dot only), 0 = off, 1 = on.
class EffectTile : public juce::Button
{
public:
    EffectTile (int effect, juce::Colour identity);
    void setSelected (bool s) { if (s != selected) { selected = s; repaint(); } }
    void setState (int s) { if (s != state) { state = s; repaint(); } }
    int getEffect() const { return effect; }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    int effect, state = 0;
    juce::Colour identity;
    bool selected = false;
};

// One row of the FX CHAIN panel (in processing order). Clicking the switch at the left toggles the effect (only where it has a real on/off parameter);
// clicking anywhere else selects it.
class ChainRow : public juce::Button
{
public:
    ChainRow (int effect, juce::Colour identity);
    void setSelected (bool s) { if (s != selected) { selected = s; repaint(); } }
    void setState (bool active) { if (active != isOn) { isOn = active; repaint(); } }
    int getEffect() const { return effect; }
    std::function<void()> onSelect, onToggle;
    using juce::Button::clicked;
    void mouseDown (const juce::MouseEvent&) override;
    void clicked (const juce::ModifierKeys&) override;
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    int effect, downX = 0;
    juce::Colour identity;
    bool selected = false, isOn = false;
};

// The settings gear (drawn, not a font glyph).
class GearButton : public juce::Button
{
public:
    GearButton() : juce::Button ("Settings") { setTooltip ("Settings"); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

// A compact horizontal level indicator (8 segments) for the top bar, fed with real peaks.
class ActivityMeter : public juce::Component
{
public:
    void push (float linearPeak);
    void paint (juce::Graphics&) override;

private:
    float shownDb = -60.0f;
};

// Lights red when a peak reached full scale, and stays lit for about a second.
class ClipLed : public juce::Component
{
public:
    void push (float linearPeak);
    void paint (juce::Graphics&) override;

private:
    int hold = 0;
};

// One look for every existing control too: glossy rounded buttons, chrome-capped rotary knobs with a coloured arc, and slim linear sliders.
// Colours come from each control's own colour ids, so every knob keeps its meaning (lime / violet / pink).
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
