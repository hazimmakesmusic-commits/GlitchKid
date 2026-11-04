#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace glitchkid::ui
{
// CREATIVE: the hands-on controls for the four creative effects (Formant #2, Harmony #3, Mutation #4, Stutter #12). Each control is bound to its real parameter;
// moving them never changes their place in the signal chain. A card is lit only while its effect is really on; Mutation's finer controls sit behind MORE.
class LabPage : public juce::Component, private juce::Timer
{
public:
    explicit LabPage (juce::AudioProcessorValueTreeState&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override { if (isShowing()) startTimerHz (8); else stopTimer(); }
    bool cardOnForTest (int i) const { return isOn (i); }
    void toggleMoreForTest() { more.triggerClick(); }
    bool moreShownForTest() const { return mutCharacter.slider.isVisible(); }
    void resetForTest (int card) { resets[(size_t) card].triggerClick(); }

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
    juce::TextButton more { "MORE" };
    std::array<juce::TextButton, 4> resets; // each card's "Reset": its parameters back to their defaults (host-visible gestures)
    bool showMore = false;
    std::array<bool, 4> shownOn {};
    bool isOn (int card) const;
    void timerCallback() override;
};
}
