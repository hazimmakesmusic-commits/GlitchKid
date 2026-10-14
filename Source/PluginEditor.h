#pragma once
#include "PluginProcessor.h"

// Simple vertical peak meter, -60 dBFS to +6 dBFS; red above 0 dBFS. Falls at a fixed rate.
class LevelMeter : public juce::Component
{
public:
    void pushPeak (float linearPeak);
    void paint (juce::Graphics&) override;

private:
    static constexpr float minDb = -60.0f, maxDb = 6.0f;
    float shownDb = minDb;
};

// A rotary knob with a name label, bound to one parameter.
class ParamKnob
{
public:
    ParamKnob (juce::AudioProcessorValueTreeState&, const char* paramId, const juce::String& name,
               const juce::String& suffix, juce::Colour colour);
    void addTo (juce::Component& parent);
    void setBounds (juce::Rectangle<int> r);

private:
    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label label;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

class GlitchKidEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GlitchKidEditor (GlitchKidProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    GlitchKidProcessor& glitchProcessor;
    LevelMeter inMeter, outMeter;
    juce::Label title { {}, "GLITCHKID" };
    juce::Label tagline { {}, "A playground for your sound." };
    juce::Label inLabel { {}, "IN" }, outLabel { {}, "OUT" }, gainLabel { {}, "OUTPUT GAIN" };
    juce::Label bandNames[ParametricEq::numBands];
    juce::ToggleButton bypassButton { "Bypass" };

    juce::OwnedArray<ParamKnob> gainKnob;
    juce::OwnedArray<ParamKnob> eqKnobs[ParametricEq::numBands];
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlitchKidEditor)
};
