#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PlaygroundWidgets.h"

// The neon look shared by the instrument and the Mastering Lab: one primary accent (ultraviolet), one secondary (electric pink, used sparingly), dark
// machined dials with a glowing value ring, and quiet pill buttons. Drawing only; no state.
namespace glitchkid::ui::nk
{
// The accents are the shared palette's (PlaygroundWidgets.h): one design system for HOME and Advanced Mixing.
const juce::Colour bgTop (0xff0b0910), bgBottom (0xff060509), surface (0xff121117), surfaceHi (0xff1b1922), hairline (0xff25222d),
                   cyan (0xff5ee6ff) /* sparingly: the input signal only */, white (textBright), dim (0xff8d8798), faint (0xff5d5868),
                   good (0xff62e0a6), warn (0xffffb54c), bad (errorRed);
inline const juce::Colour& uv = violet;     // the live accents of the current colour profile
inline const juce::Colour& uvSoft = neon;
inline const juce::Colour& pinkNeon = pink;

struct Knob : juce::LookAndFeel_V4
{
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
};

// A pill button. Toggle state = on (ultraviolet; set the "secondary" property for pink).
struct Pill : juce::Button
{
    explicit Pill (const juce::String& label) : juce::Button (label) { setWantsKeyboardFocus (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
};
}
