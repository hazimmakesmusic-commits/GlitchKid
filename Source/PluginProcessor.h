#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/ParametricEq.h"

// Signal path: input -> 4-band EQ -> smoothed gain -> output, with a 20 ms crossfade to the dry
// input when Bypass is on. Peak meters tap the input and output.
class GlitchKidProcessor : public juce::AudioProcessor
{
public:
    // Parameter IDs are stored in saved sessions and presets: never rename or reuse them.
    static constexpr const char* gainId   = "gain";   // dB
    static constexpr const char* bypassId = "bypass";

    struct EqIds { const char* freq; const char* gain; const char* q; }; // q is null for shelves
    static constexpr EqIds eqIds[ParametricEq::numBands] = {
        { "eq_lo_freq", "eq_lo_gain", nullptr },
        { "eq_b1_freq", "eq_b1_gain", "eq_b1_q" },
        { "eq_b2_freq", "eq_b2_gain", "eq_b2_q" },
        { "eq_hi_freq", "eq_hi_gain", nullptr } };

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

    // Peak meters: the audio thread raises these (linear, pre-/post-processing); the UI reads and resets them.
    float consumeInputPeak()  { return inputPeak.exchange (0.0f); }
    float consumeOutputPeak() { return outputPeak.exchange (0.0f); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void processChunk (juce::AudioBuffer<float>&);

    std::atomic<float>* gainDb = nullptr;
    std::atomic<float>* bypass = nullptr;
    struct EqParams { std::atomic<float>* freq; std::atomic<float>* gain; std::atomic<float>* q; };
    EqParams eqParams[ParametricEq::numBands] {};

    ParametricEq eq;
    juce::AudioBuffer<float> dryBuffer; // preallocated in prepareToPlay, used for the bypass crossfade
    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f };
    juce::SmoothedValue<float> smoothedGain { 1.0f }; // linear
    juce::SmoothedValue<float> wetMix { 1.0f };       // 1 = processed, 0 = dry (bypass)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlitchKidProcessor)
};
