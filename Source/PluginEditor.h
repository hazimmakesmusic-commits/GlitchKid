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

// Gain-reduction meter: grows downward from the top, 0 to 24 dB, falls at a fixed rate.
class GainReductionMeter : public juce::Component
{
public:
    void pushReduction (float db);
    void paint (juce::Graphics&) override;

private:
    static constexpr float maxDb = 24.0f;
    float shownDb = 0.0f;
};

// A rotary knob with a name label, bound to one parameter.
class ParamKnob
{
public:
    ParamKnob (juce::AudioProcessorValueTreeState&, const char* paramId, const juce::String& name,
               const juce::String& suffix, juce::Colour colour);
    void addTo (juce::Component& parent);
    void setBounds (juce::Rectangle<int> r);
    void setEnabled (bool);

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
    static constexpr int designW = 1060, designH = 1112; // layout size; the window scales it to fit
    juce::Component content; // declared first so it outlives the controls inside it
    float scaleFactor = 1.0f;
    void layoutContent();
    void timerCallback() override;

    GlitchKidProcessor& glitchProcessor;
    LevelMeter inMeter, outMeter;
    GainReductionMeter grMeter;
    juce::Label compTitle { {}, "COMPRESSOR" }, grLabel { {}, "GR" };
    int compDividerX = 0;
    juce::Label title { {}, "GLITCHKID" };
    juce::Label tagline { {}, "A playground for your sound." };
    juce::Label inLabel { {}, "IN" }, outLabel { {}, "OUT" };
    juce::Label bandNames[ParametricEq::numBands];
    juce::ToggleButton bypassButton { "Bypass" };
    juce::Label stutterTitle { {}, "GLITCH STUTTER" }, stutterRateLabel { {}, "RATE" }, tempoLabel;
    juce::ToggleButton stutterOnButton { "ON" };
    juce::ComboBox stutterRateBox;
    juce::OwnedArray<ParamKnob> stutterMixKnob;
    juce::String shownTempoText;

    // Pitch Tune row: ON, key, scale, strength, speed, FORMANT (independent of ON), and a read-out of the sung note and the note it is pulled to.
    juce::Label tuneTitle { {}, "PITCH TUNE  +  FORMANT" }, tuneKeyLabel { {}, "KEY" }, tuneScaleLabel { {}, "SCALE" }, tuneReadout;
    juce::ToggleButton tuneOnButton { "ON" };
    juce::ComboBox tuneKeyBox, tuneScaleBox;
    juce::OwnedArray<ParamKnob> tuneKnobs; // strength, speed, formant
    juce::String shownTuneText;
    int tuneDividerY = 0;

    // Vocal Mutation row: ON, a read-out of what the settings do (type + shift in Hz), MUTATION, CHARACTER, BLEND.
    juce::Label mutTitle { {}, "VOCAL MUTATION" }, mutReadout;
    juce::ToggleButton mutOnButton { "ON" };
    juce::OwnedArray<ParamKnob> mutKnobs; // mutation, character, blend
    juce::String shownMutText;
    int mutDividerY = 0;

    // Liquid Echo row: ON, SYNC, division, time, feedback, tone, width, mix.
    juce::Label echoTitle { {}, "LIQUID ECHO" }, echoDivLabel { {}, "DIVISION" };
    juce::ToggleButton echoOnButton { "ON" }, echoSyncButton { "SYNC" };
    juce::ComboBox echoDivBox;
    juce::OwnedArray<ParamKnob> echoKnobs; // time, feedback, tone, width, mix
    int echoDividerY = 0;

    // ALTER EGOS + MOOD ENGINE (see AlterEgos.h). The lit Alter Ego is the one whose values all match the controls.
    // CHAOS and SURPRISE ME write the normal parameters; BLEND is the ordinary "mix" parameter.
    juce::Label egoTitle { {}, "ALTER EGOS" }, egoInfo, chaosLabel { {}, "CHAOS" }, blendLabel { {}, "BLEND" };
    juce::TextButton egoButtons[AlterEgos::count];
    juce::Slider chaosSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider blendSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::TextButton surpriseButton { "SURPRISE ME" };
    bool updatingChaosFromCode = false;
    juce::String shownEgoText;
    void refreshAlterEgos();

    // Dimension row: ON, width, detune, depth, mix.
    juce::Label dimTitle { {}, "DIMENSION" };
    juce::ToggleButton dimOnButton { "ON" };
    juce::OwnedArray<ParamKnob> dimKnobs;
    int dimDividerY = 0;

    juce::OwnedArray<ParamKnob> gainKnob;
    juce::OwnedArray<ParamKnob> driveKnob;
    juce::OwnedArray<ParamKnob> compKnobs;
    juce::OwnedArray<ParamKnob> eqKnobs[ParametricEq::numBands];
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment stutterOnAttachment;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment stutterRateAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment echoOnAttachment, echoSyncAttachment;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment echoDivAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment dimOnAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment tuneOnAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment mutOnAttachment;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment tuneKeyAttachment, tuneScaleAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment blendAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlitchKidEditor)
};
