#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Stage 1A: stereo/mono pass-through with one gain parameter and a bypass parameter.
class GlitchKidProcessor : public juce::AudioProcessor
{
public:
    static constexpr const char* gainId   = "gain";   // dB; IDs are stored in saved sessions, never rename
    static constexpr const char* bypassId = "bypass";

    GlitchKidProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    using AudioProcessor::processBlock; // keep the double-precision overload visible
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GlitchKid"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float>* gainDb = nullptr;
    std::atomic<float>* bypass = nullptr;
    juce::SmoothedValue<float> smoothedGain { 1.0f }; // linear; also ramps to 1.0 on bypass, so no clicks

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlitchKidProcessor)
};
