#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// GLITCHKID colour profiles: the one place the interface colours are defined. A profile sets the accents (ui::violet = primary, ui::pink = secondary,
// ui::neon = soft; PlaygroundWidgets.h) and the dark foundation under them (window, panels, borders, text: ui:: and HOME's nk:: names), so every component
// that draws with the tokens follows. VIRIDIAN, the default, has its own teal-black foundation; the other four share the original graphite. Visual only:
// no audio, parameter, preset, macro, automation or A/B state is read or written here.
namespace glitchkid::ui::theme
{
struct Foundation
{
    juce::Colour window;      // the window (VIRIDIAN: VOID)
    juce::Colour windowLow;   // the bottom of HOME's window gradient
    juce::Colour panel;       // panels (SURFACE)
    juce::Colour raised;      // raised sections and controls (ELEVATED)
    juce::Colour border;      // separation (BORDER)
    juce::Colour hairline;    // the quieter separation inside HOME's panels
    juce::Colour well;        // inset wells: fields, scopes, unselected keys
    juce::Colour text;        // primary text
    juce::Colour textDim;     // secondary text
    juce::Colour faint;       // hints and disabled text
};

struct Profile
{
    const char* id;          // stored in the user's settings ("magenta")
    const char* name;        // shown ("MAGENTA")
    juce::Colour primary;    // fills that carry white text, selected states (white text >= 4.5:1), accents on the background (>= 3:1)
    juce::Colour secondary;  // the bright end: value arcs, activity, highlights, accent text (>= 4.5:1 on the background)
    juce::Colour soft;       // a light tint: hover, focus rings, quiet highlighted text
    juce::Colour deep;       // the dark end of gradients and glows
    juce::Colour mid;        // the primary gradient's centre stop
    Foundation base;         // the surfaces and text this profile sits on
};

enum Index { viridian = 0, magenta, sapphire, amber, voidProfile, numProfiles };
constexpr int defaultProfile = viridian; // used only when no valid saved choice exists (loadSaved)

const Profile& profile (int index);              // clamped
int current();
const Profile& active();
const Profile& previous();                       // the profile before the last change (to retint colours stored in components)
void apply (int index);                          // sets the live tokens and tells every open GlitchKid window (message thread)

// The profile gradients. Primary: secondary -> mid (48 %) -> primary (VIRIDIAN: #00DDA5 -> #00B894 -> #08776F, the signature gradient). Secondary: primary -> deep.
juce::ColourGradient primaryGradient (juce::Point<float> from, juce::Point<float> to, float alpha = 1.0f);
juce::ColourGradient secondaryGradient (juce::Point<float> from, juce::Point<float> to, float alpha = 1.0f);

// The user's choice, kept outside any project (an appearance preference): Application Support/GlitchKid/Settings.xml.
int loadSaved();
void save (int index);
void setSettingsFile (const juce::File&);        // tests: never touch the real user's settings
juce::File settingsFile();

// Every GlitchKid window listens; the change message is sent synchronously after the tokens change.
juce::ChangeBroadcaster& changes();

// Colours a component stored at construction (setColour) that equal an old profile token are replaced by the new one, alpha kept.
void retint (juce::Component& root, const Profile& from, const Profile& to);
}
