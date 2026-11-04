#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// The neon look shared by the instrument and the Mastering Lab: one primary accent (ultraviolet), one secondary (electric pink, used sparingly), dark
// machined dials with a glowing value ring, and quiet pill buttons. Drawing only; no state.
namespace glitchkid::ui::nk
{
const juce::Colour bgTop (0xff0b0910), bgBottom (0xff060509), surface (0xff121117), surfaceHi (0xff1b1922), hairline (0xff25222d),
                   uv (0xff9b5cff), uvSoft (0xffc4a6ff), pinkNeon (0xffff4fb8), cyan (0xff5ee6ff) /* sparingly: the input signal only */, white (0xfff4f2f7), dim (0xff8d8798), faint (0xff5d5868),
                   good (0xff62e0a6), warn (0xffffb54c), bad (0xffff5c6c);

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
