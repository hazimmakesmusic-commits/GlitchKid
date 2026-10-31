#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace glitchkid::ui
{
// LAB: the hands-on controls for the four creative effects (Formant #2, Harmony #3, Mutation #4, Stutter #12). Each control is bound to its real parameter;
// moving them never changes their place in the signal chain. KID can propose these effects; this page is where you adjust them yourself.
class LabPage : public juce::Component
{
public:
    explicit LabPage (juce::AudioProcessorValueTreeState&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    struct Switch
    {
        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
    };
    struct Choice
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
    };
    juce::AudioProcessorValueTreeState& state;
    Knob formant, harmLevel, mutAmount, mutCharacter, mutBlend, stutterMix;
    Switch harmOn, mutOn, stutterOn;
    Choice harmInterval, stutterRate;
    juce::Label notes[4];
    void bind (Knob&, const char* id, const juce::String& name, const juce::String& tip);
    void bind (Switch&, const char* id, const juce::String& name, const juce::String& tip);
    void bind (Choice&, const char* id, const juce::String& name, const juce::String& tip);
    std::array<juce::Rectangle<int>, 4> cards;
};
}
