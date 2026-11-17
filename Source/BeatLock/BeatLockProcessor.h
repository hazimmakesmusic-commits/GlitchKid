#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "BeatLockParams.h"
#include "BeatLockEngine.h"
#include "BeatLockAnalysis.h"
#include "../Analysis/DisplayTap.h"

// GLITCHKID BEAT LOCK: a separate VST3 plug-in. It goes on the INSTRUMENTAL. The vocal arrives on the auxiliary (sidechain) input and is only listened to:
// it never reaches the output. Main bus: mono or stereo in = out. Sidechain bus "Vocal Sidechain": disabled, mono or stereo (enabled by default).
// Zero latency. Independent parameters (bl_*) and state from the GlitchKid vocal plug-in.
class BeatLockProcessor : public juce::AudioProcessor
{
public:
    BeatLockProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "GlitchKid Beat Lock"; }
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
    const glitchkid::beatlock::Engine& getEngine() const { return engine; }
    glitchkid::beatlock::SidechainStatus getSidechainStatus() const { return (glitchkid::beatlock::SidechainStatus) status.load (std::memory_order_relaxed); }
    float consumeSidechainPeak() { return scPeak.exchange (0.0f, std::memory_order_relaxed); }
    const glitchkid::analysis::DisplayTap& getInstrumentalInTap() const { return instInTap; }
    const glitchkid::analysis::DisplayTap& getInstrumentalOutTap() const { return instOutTap; }
    const glitchkid::analysis::DisplayTap& getVocalTap() const { return vocalTap; }
    glitchkid::beatlock::AnalysisFifo& getAnalysisFifo() { return fifo; }
    double getSampleRateNow() const { return rate.load (std::memory_order_relaxed); }
    void resetToDefaults(); // every parameter to its default (with host notification)

    // tests: the settings the engine would use now
    glitchkid::beatlock::Settings readSettings() const;

private:
    struct Raw
    {
        std::atomic<float>* amount = nullptr, *maxCut = nullptr, *sensitivity = nullptr, *focus[3] {}, *attack = nullptr, *release = nullptr, *bypass = nullptr;
        std::atomic<float>* on[glitchkid::beatlock::numBands] {}, *freq[glitchkid::beatlock::numBands] {}, *q[glitchkid::beatlock::numBands] {}, *maxCutBand[glitchkid::beatlock::numBands] {};
    } raw;
    juce::AudioProcessorParameter* bypassParam = nullptr;

    glitchkid::beatlock::Engine engine;
    glitchkid::beatlock::AnalysisFifo fifo;
    glitchkid::analysis::DisplayTap instInTap, instOutTap, vocalTap;
    static constexpr int chunk = 512; // the engine is fed in chunks of at most this, so scratch memory is fixed whatever the host's block size
    juce::AudioBuffer<float> scratch { 3, chunk };   // vocal mono, gate flag, instrumental mono
    std::atomic<int> status { 0 };
    std::atomic<float> scPeak { 0.0f };
    std::atomic<double> rate { 48000.0 };
    double silentSeconds = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BeatLockProcessor)
};
