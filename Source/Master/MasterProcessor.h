#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MasterParams.h"
#include "../Mastering/MasteringEngine.h"
#include "../DSP/FixedDelay.h"

// GLITCHKID MASTER: a separate VST3 for a finished stereo mix on the master bus. Its own processor, parameters (gkm_*), state and editor; the DSP is the
// shared, tested glitchkid::master::MasteringEngine (also behind the vocal plug-in's old Mastering mode, which this plug-in never touches).
//   in -> NaN/Inf guard -> INPUT trim -> [engine: input meter -> EQ (style + tone) -> glue compressor (threshold follows the mix's integrated loudness)
//   -> saturation (4x oversampled) -> width (mid untouched, no widening below 120 Hz) -> true-peak limiter (LOUDNESS = drive, CEILING) -> OUTPUT trim
//   -> output meter] -> bypass crossfade -> out
// Main bus: mono or stereo, in = out. Latency: constant for a given sample rate (saturation 6 + limiter look-ahead 2 ms + detector), reported to the host.
// Bypass (the host's, or the button): the untouched input delayed by exactly that latency, crossfaded over 20 ms, so a bypass switch never jumps in time.
// ORIGINAL (compare): the engine's level-matched original, so the louder version never wins. It is a listening switch, not saved with the session.
class MasterProcessor : public juce::AudioProcessor
{
public:
    MasterProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "GlitchKid Master"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorValueTreeState apvts;

    // ---- for the editor (message thread) ----
    const glitchkid::master::MasteringEngine& getEngine() const { return engine; }
    glitchkid::master::Controls readControls() const;
    void setCompareOriginal (bool original) { compare.store (original, std::memory_order_relaxed); }
    bool isCompareOriginal() const { return compare.load (std::memory_order_relaxed); }
    bool isBypassed() const { return raw.bypass->load() >= 0.5f; }
    void requestMeterReset() { meterReset.store (true, std::memory_order_relaxed); } // the audio thread clears the meters at its next block
    struct Peaks { float inL = 0, inR = 0, outL = 0, outR = 0, limiterGrDb = 0, glueGrDb = 0; };
    Peaks consumePeaks(); // the largest values since the last call (live meters and the gain-reduction history)
    juce::uint32 blocksProcessed() const { return blocks.load (std::memory_order_relaxed); } // stops moving when the host stops calling the plug-in
    int nonFiniteBlocks() const { return nonFinite.load (std::memory_order_relaxed); }     // blocks in which NaN / Inf input was replaced by silence
    int numChannelsNow() const { return channels.load (std::memory_order_relaxed); }

    // A/B: two stored settings (every parameter except bypass), saved with the session. Message thread.
    void storeSlot (int slot);
    bool recallSlot (int slot);           // false when the slot is empty
    bool hasSlot (int slot) const { return slots[(size_t) (slot & 1)].isValid(); }
    int activeSlot() const { return active; } // -1 none yet
    void resetToDefaults();               // every parameter (bypass too) to its default, with host notification

private:
    struct Raw { std::atomic<float>* style = nullptr, *intensity = nullptr, *input = nullptr, *loudness = nullptr, *tone = nullptr, *width = nullptr, *ceiling = nullptr, *output = nullptr, *bypass = nullptr; } raw;
    juce::AudioProcessorParameter* bypassParam = nullptr;

    glitchkid::master::MasteringEngine engine;
    static constexpr int chunk = 1024; // the engine is fed at most this many samples at a time: fixed scratch whatever the host's block size
    juce::AudioBuffer<float> dry { 2, chunk };
    FixedDelay bypassDelay;
    juce::SmoothedValue<float> inputGain { 1.0f }, bypassMix { 0.0f };
    std::atomic<bool> compare { false }, meterReset { false };
    std::atomic<float> pkInL { 0 }, pkInR { 0 }, pkOutL { 0 }, pkOutR { 0 }, pkLim { 0 }, pkGlue { 0 };
    std::atomic<juce::uint32> blocks { 0 };
    std::atomic<int> nonFinite { 0 }, channels { 2 };
    std::array<juce::ValueTree, 2> slots;
    int active = -1;

    static void raise (std::atomic<float>& a, float v) { float prev = a.load (std::memory_order_relaxed); while (v > prev && ! a.compare_exchange_weak (prev, v, std::memory_order_relaxed)) {} }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterProcessor)
};
