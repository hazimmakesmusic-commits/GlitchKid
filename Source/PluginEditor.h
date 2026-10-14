#pragma once
#include "PluginProcessor.h"

class GlitchKidEditor : public juce::AudioProcessorEditor
{
public:
    explicit GlitchKidEditor (GlitchKidProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Label title { {}, "GLITCHKID" };
    juce::Label tagline { {}, "A playground for your sound." };
    juce::Slider gainSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label gainLabel { {}, "GAIN" };
    juce::ToggleButton bypassButton { "Bypass" };

    juce::AudioProcessorValueTreeState::SliderAttachment gainAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlitchKidEditor)
};
