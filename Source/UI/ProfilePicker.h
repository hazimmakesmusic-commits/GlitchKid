#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

// The colour-profile selector: a small swatch in the top bar of both workspaces opens five preview tiles (a silk-wave of each profile's gradient on the
// near-black surface, and its name). Choosing a tile applies the profile at once to every open GlitchKid window and keeps it as the user's preference.
// Visual only.
namespace glitchkid::ui
{
void paintProfileTile (juce::Graphics&, juce::Rectangle<float>, int profile, bool selected, bool hover); // also used for the brand swatches

class ProfileSwatch : public juce::Button, private juce::ChangeListener
{
public:
    ProfileSwatch();
    ~ProfileSwatch() override;
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
};

class ProfilePicker : public juce::Component, private juce::ChangeListener
{
public:
    ProfilePicker();
    ~ProfilePicker() override;
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void choose (int profile); // apply now and remember it (the user's choice)
    int tileAt (juce::Point<int>) const;
    juce::Rectangle<float> tile (int profile) const;

    static constexpr int tileW = 92, tileH = 96, pad = 12;

private:
    int hover = -1;
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
};
}
