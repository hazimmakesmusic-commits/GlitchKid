#include "PluginEditor.h"

GlitchKidEditor::GlitchKidEditor (GlitchKidProcessor& p)
    : AudioProcessorEditor (&p),
      gainAttachment (p.apvts, GlitchKidProcessor::gainId, gainSlider),
      bypassAttachment (p.apvts, GlitchKidProcessor::bypassId, bypassButton)
{
    title.setFont (juce::FontOptions (32.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, juce::Colour (0xffb6ff00)); // acid lime
    title.setJustificationType (juce::Justification::centred);

    tagline.setFont (juce::FontOptions (13.0f));
    tagline.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    tagline.setJustificationType (juce::Justification::centred);

    gainLabel.setJustificationType (juce::Justification::centred);
    gainSlider.setTextValueSuffix (" dB");
    gainSlider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffff2d95)); // hot pink
    bypassButton.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff8a2be2));      // violet

    for (auto* c : std::initializer_list<juce::Component*> { &title, &tagline, &gainSlider, &gainLabel, &bypassButton })
        addAndMakeVisible (c);

    setSize (320, 340);
}

void GlitchKidEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff121214)); // obsidian
}

void GlitchKidEditor::resized()
{
    auto r = getLocalBounds().reduced (16);
    title.setBounds (r.removeFromTop (44));
    tagline.setBounds (r.removeFromTop (20));
    r.removeFromTop (12);
    gainLabel.setBounds (r.removeFromTop (20));
    bypassButton.setBounds (r.removeFromBottom (32));
    gainSlider.setBounds (r.reduced (8));
}
