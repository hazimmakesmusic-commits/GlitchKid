#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// GLITCHKID colour profiles: the one place the accent colours are defined. The surfaces (near-black, graphite) never change; a profile only changes the
// accents. The three live accent tokens of the design system (ui::violet = primary, ui::pink = secondary, ui::neon = soft; PlaygroundWidgets.h) are set
// from the current profile, so every component that draws with them follows. Visual only: no audio, parameter, preset, macro, automation or A/B state is
// read or written here.
namespace glitchkid::ui::theme
{
struct Profile
{
    const char* id;          // stored in the user's settings ("magenta")
    const char* name;        // shown ("MAGENTA")
    juce::Colour primary;    // fills that carry white text, selected states (white text >= 4.5:1), accents on the background (>= 3:1)
    juce::Colour secondary;  // the bright end: value arcs, activity, highlights, accent text (>= 4.5:1 on the background)
    juce::Colour soft;       // a light tint: hover, focus rings, quiet highlighted text
    juce::Colour deep;       // the dark end of gradients and glows
};

enum Index { viridian = 0, magenta, sapphire, amber, voidProfile, numProfiles };
constexpr int defaultProfile = magenta;

const Profile& profile (int index);              // clamped
int current();
const Profile& active();
const Profile& previous();                       // the profile before the last change (to retint colours stored in components)
void apply (int index);                          // sets the live tokens and tells every open GlitchKid window (message thread)

// The profile gradients. Primary: secondary -> primary (e.g. hot pink -> ultraviolet). Secondary: primary -> deep.
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
