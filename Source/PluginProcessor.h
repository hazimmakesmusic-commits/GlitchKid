#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/ParametricEq.h"
#include "DSP/Compressor.h"
#include "DSP/Saturation.h"
#include "DSP/FixedDelay.h"
#include "DSP/GlitchStutter.h"
#include "DSP/LiquidEcho.h"

// Signal path: input -> 4-band EQ -> compressor -> saturation -> glitch stutter -> liquid echo -> smoothed gain -> output, with a 20 ms crossfade to the dry
// input when Bypass is on, and a Mix control blends the processed signal with the same dry path. The saturation oversampler adds a constant 6 samples of latency; the dry
// path is delayed by the same amount so bypass stays time-aligned. Peak meters tap the input and output.
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

    // Compressor parameter IDs (same rule: never rename).
    static constexpr const char* compThresholdId = "comp_thresh";
    static constexpr const char* compRatioId     = "comp_ratio";
    static constexpr const char* compAttackId    = "comp_attack";
    static constexpr const char* compReleaseId   = "comp_release";
    static constexpr const char* compKneeId      = "comp_knee";
    static constexpr const char* compMakeupId    = "comp_makeup";

    static constexpr const char* satDriveId = "sat_drive"; // dB, 0 = off
    static constexpr const char* mixId      = "mix";       // percent processed (100) vs dry (0)
    static constexpr const char* stutterOnId   = "stutter_on";   // Glitch Stutter on/off
    static constexpr const char* stutterRateId = "stutter_rate"; // 0..3 = 1/4, 1/8, 1/16, 1/32 note
    static constexpr const char* stutterMixId  = "stutter_mix";  // percent loop (100) vs live (0)

    // Liquid Echo (tempo-aware delay). Defaults: off, so a fresh instance does not change the sound.
    static constexpr const char* echoOnId    = "echo_on";
    static constexpr const char* echoSyncId  = "echo_sync";  // true = note division, false = echo_time
    static constexpr const char* echoTimeId  = "echo_time";  // ms, used when sync is off
    static constexpr const char* echoDivId   = "echo_div";   // index into LiquidEcho::divisionName()
    static constexpr const char* echoFbId    = "echo_fb";    // percent, 0..90
    static constexpr const char* echoToneId  = "echo_tone";  // Hz, low-pass on the repeats
    static constexpr const char* echoWidthId = "echo_width"; // percent
    static constexpr const char* echoMixId   = "echo_mix";   // percent repeats (100) vs dry (0)

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
    float consumeGainReductionDb() { return gainReductionPeak.exchange (0.0f); } // positive dB

    // For the UI (and tests): tempo used by Glitch Stutter, and whether it came from the host.
    float getStutterTempoBpm() const { return shownTempo.load(); }
    bool isStutterTempoFromHost() const { return tempoFromHost.load(); }
    int getStutterLoopLength() const { return shownLoopLength.load(); } // 0 when not repeating

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void processChunk (juce::AudioBuffer<float>&);

    std::atomic<float>* gainDb = nullptr;
    std::atomic<float>* bypass = nullptr;
    struct EqParams { std::atomic<float>* freq; std::atomic<float>* gain; std::atomic<float>* q; };
    EqParams eqParams[ParametricEq::numBands] {};

    std::atomic<float>* compParams[6] {}; // threshold, ratio, attack, release, knee, makeup
    CompressorSettings readCompressorSettings() const;

    ParametricEq eq;
    Compressor compressor;
    Saturation saturation;
    GlitchStutter stutter;
    std::atomic<float>* stutterOn = nullptr;
    std::atomic<float>* stutterRate = nullptr;
    std::atomic<float>* stutterMix = nullptr;
    float currentBpm = GlitchStutter::fallbackBpm; // resolved once per block from the host play head
    std::atomic<float> shownTempo { GlitchStutter::fallbackBpm };
    std::atomic<bool> tempoFromHost { false };
    std::atomic<int> shownLoopLength { 0 };
    LiquidEcho echo;
    std::atomic<float>* echoParams[8] {}; // on, sync, time, division, feedback, tone, width, mix
    EchoSettings readEchoSettings() const;
    FixedDelay dryDelay; // delays the dry (bypass) path by the saturation latency
    std::atomic<float>* satDrive = nullptr;
    std::atomic<float>* mixPercent = nullptr;
    juce::AudioBuffer<float> dryBuffer; // preallocated in prepareToPlay, used for the bypass crossfade
    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f }, gainReductionPeak { 0.0f };
    juce::SmoothedValue<float> smoothedGain { 1.0f }; // linear
    juce::SmoothedValue<float> wetMix { 1.0f };       // 1 = processed, 0 = dry (bypass)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlitchKidProcessor)
};
