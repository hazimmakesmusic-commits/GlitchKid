#pragma once
#include "Analysis/DisplayTap.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/ParametricEq.h"
#include "DSP/Compressor.h"
#include "DSP/Saturation.h"
#include "DSP/AirLab.h"
#include "DSP/CleanLab.h"
#include "DSP/FixedDelay.h"
#include "DSP/GlitchStutter.h"
#include "DSP/LiquidEcho.h"
#include "DSP/Dimension.h"
#include "DSP/VocalMutation.h"
#include "DSP/VocalDoubling/VocalDoubler.h"
#include "DSP/Harmony/HarmonyVoice.h"
#include "DSP/PitchCorrection/PitchCorrector.h"
#include "DSP/FormantShifting/FormantShifter.h"
#include "AlterEgos.h"
#include "Presets/SoundState.h"
#include "Analysis/VocalAnalyzer.h"
#include "AutoMode/AutoMix.h"

// Signal path: input -> pitch tune (optional) -> formant shift (optional) -> harmony voice (optional, adds latency while on) -> vocal mutation (optional, zero latency) -> vocal doubler (optional, zero latency) -> 4-band EQ -> compressor -> saturation -> glitch stutter -> liquid echo -> dimension -> smoothed gain -> output, with a 20 ms crossfade to the dry
// input when Bypass is on, and a Mix control blends the processed signal with the same dry path. The saturation oversampler adds a constant 6 samples of latency; the dry
// path is delayed by the same amount so bypass stays time-aligned. Pitch Tune (off by default) adds its own constant delay (PitchCorrector::latencyFor, about 31 ms)
// only while it is switched on, and Formant (off at 0 semitones) adds its own (FormantShifter::latencyFor, about 21 ms) only while it is away from 0: for each, the dry
// path and the reported latency follow it, with a 20 ms crossfade when it is switched. Peak meters tap the input and output.
class GlitchKidProcessor : public juce::AudioProcessor, private juce::Timer
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

    // AIR LAB (presence and air enhancement, after the saturation). Appended last in the layout, so older sessions keep every host automation index.
    static constexpr const char* airOnId       = "air_on";       // off by default: an old session or preset sounds exactly as before
    static constexpr const char* airPresenceId = "air_presence"; // percent
    static constexpr const char* airAirId      = "air_air";      // percent
    static constexpr const char* airLinkId     = "air_link";     // UI behaviour only (PRESENCE and AIR move together); no DSP effect
    static constexpr const char* airMixId      = "air_mix";      // percent of the added content
    static constexpr const char* airTrimId     = "air_trim";     // dB

    // CLEAN LAB (rumble / low-mid / resonance cleanup after the user EQ, de-esser after the compressor). Appended after AIR LAB; off by default.
    static constexpr const char* cleanOnId     = "clean_on";
    static constexpr const char* cleanAmountId = "clean_amount"; // percent: how much of each section is allowed
    static constexpr const char* cleanToneId   = "clean_tone";   // percent trim of the corrective EQ section
    static constexpr const char* cleanResId    = "clean_res";    // percent trim of the resonance section
    static constexpr const char* cleanEssId    = "clean_ess";    // percent trim of the de-esser
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

    // Dimension (stereo thickening). Default off, so a fresh instance does not change the sound.
    static constexpr const char* dimOnId     = "dim_on";
    static constexpr const char* dimWidthId  = "dim_width";  // percent, 0 = centred thickening, 100 = stereo spread
    static constexpr const char* dimDetuneId = "dim_detune"; // cents (peak pitch swing of the modulation)
    static constexpr const char* dimDepthId  = "dim_depth";  // percent, level of the thickening voices
    static constexpr const char* dimMixId    = "dim_mix";    // percent, dry (0) to dry + full effect (100)

    // Pitch Tune (vocal pitch correction). Default off, so a fresh instance does not change the sound or add latency.
    static constexpr const char* tuneOnId       = "tune_on";
    static constexpr const char* tuneKeyId      = "tune_key";      // 0 = C ... 11 = B
    static constexpr const char* tuneScaleId    = "tune_scale";    // index into glitchkid::pitch::ScaleQuantizer::Scale
    static constexpr const char* tuneStrengthId = "tune_strength"; // percent of the distance to the target note that is corrected
    static constexpr const char* tuneSpeedId    = "tune_speed";    // ms, how fast the correction glides; 0 = instant

    // Formant (vocal "size"): moves the spectral envelope of a voice without changing its notes. -12..+12 semitones, 0 = off and neutral
    // (exactly bit-exact, no latency). Negative = bigger / darker voice, positive = smaller / brighter. Independent of Pitch Tune.
    static constexpr const char* formantShiftId = "formant_shift";

    // Vocal Mutation (single-sideband frequency shifter / ring modulator: robotic and alien textures). Default off, so a fresh instance does not change the sound.
    // Zero latency. Sits after Pitch Tune and FORMANT, before the EQ.
    static constexpr const char* mutOnId        = "mut_on";
    static constexpr const char* mutAmountId    = "mut_amount";    // percent, 0 = no change; sets the carrier / shift in Hz (0..600, squared curve)
    static constexpr const char* mutCharacterId = "mut_character"; // percent, 0 = DOWN, 50 = ROBOT (ring modulation), 100 = UP
    static constexpr const char* mutBlendId     = "mut_blend";     // percent, dry (0) to fully mutated (100)

    // Vocal Doubler (a synthetic double: two drifting, delayed, slightly darker copies). Default off. Zero latency. After Vocal Mutation, before the EQ.
    static constexpr const char* dblOnId     = "dbl_on";
    static constexpr const char* dblAmountId = "dbl_amount"; // percent, level of the doubled voices
    static constexpr const char* dblWidthId  = "dbl_width";  // percent, 0 = doubles in the centre, 100 = stereo spread (mono-safe)
    static constexpr const char* dblDetuneId = "dbl_detune"; // cents, the largest pitch wobble of a double
    static constexpr const char* dblTimingId = "dbl_timing"; // ms, how far behind the lead the doubles sit

    // Harmony (ONE fixed-interval voice from the lead; prototype quality, see HarmonyVoice.h). Default off. Adds the Pitch Tune latency while on
    // (reported to the host; dry path and Mix follow). After Pitch Tune / FORMANT (it harmonises the corrected voice), before Vocal Mutation.
    static constexpr const char* harmOnId       = "harm_on";
    static constexpr const char* harmIntervalId = "harm_interval"; // index into glitchkid::harmony::intervalInfo()
    static constexpr const char* harmLevelId    = "harm_level";    // dB, level of the harmony voice relative to the lead

    GlitchKidProcessor();
    ~GlitchKidProcessor() override;

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

    // ALTER EGOS + MOOD ENGINE (message thread only). These write the normal parameters (so the knobs show what you hear and
    // the host sees automation); nothing here runs on the audio thread. BLEND is the ordinary "mix" parameter.
    // The selection (which Alter Ego, Chaos, Surprise seed) is saved in the plug-in state as three ValueTree properties.
    struct AlterEgoSelection { int ego = -1; float chaos = 0.0f; std::uint32_t seed = 0; }; // ego -1 = none
    AlterEgoSelection getAlterEgoSelection() const;
    int applyAlterEgo (int ego, float chaos, std::uint32_t seed); // returns the number of parameters set
    int setChaos (float chaos);                                   // re-applies the selected Alter Ego at this Chaos (keeps the variation)
    int surprise (std::uint32_t seed);                            // a new seeded variation of the selected Alter Ego
    bool isAlterEgoActive() const;                                // true while every owned parameter still matches the selection

    // PRESETS and A/B (message thread only; never called from the audio thread, which only ever sees ordinary parameter changes).
    // A "sound" (glitchkid::SoundState) is every parameter value plus the Alter Ego selection; Bypass is not part of a sound. The live
    // sound always stays in the parameters (apvts); a SoundState is just a copy of it. Applying one sets every parameter through the normal
    // parameter calls (host gestures, each effect's own smoothing), so every intermediate state is a legal sound and nothing clicks.
    glitchkid::SoundState captureSoundState() const;
    int applySoundState (const glitchkid::SoundState&);   // parameters missing from the state get their default; returns how many were set
    // Sets only the parameters whose value in `s` differs from the live one (beyond one step), each with a host gesture. For drags and quick
    // commands, so a macro does not re-send all 54 parameters on every mouse move. Returns how many were set. Message thread only.
    int applyParameterChanges (const glitchkid::SoundState& s);
    // Small UI preferences kept in the session (never part of a sound, a preset or A/B): motion on/off, the detailed LAB panel open/closed.
    bool getUiFlag (const char* key, bool fallback) const;
    void setUiFlag (const char* key, bool value);
    int getUiInt (const char* key, int fallback) const;
    juce::String getUiString (const char* key) const;
    void setUiString (const char* key, const juce::String& value);
    void setUiInt (const char* key, int value);
    glitchkid::SoundState factoryDefaultState() const;    // every parameter at its default, no Alter Ego
    bool matchesSoundState (const glitchkid::SoundState&) const; // true if every parameter in it equals the live value (within one step)

    // User presets: the name shown in the window and whether the live sound still equals what was loaded or saved.
    // Loading a preset (or the factory default) replaces the live sound, keeps both A/B snapshots untouched and leaves neither A nor B
    // marked active (so you can capture the new sound into a slot and compare it with the old one).
    void applyPreset (const juce::String& name, const glitchkid::SoundState&);
    void notePresetSaved (const juce::String& name, const glitchkid::SoundState&); // after a successful save: the live sound is the baseline
    // AUTOMODE (message thread only, except the analyzer tap which the audio thread feeds). ANALYZE listens to the incoming vocal for a few
    // seconds; APPLY then turns the finished analysis into new control values once, through the normal parameter calls. Nothing is changed
    // while listening, and nothing is ever applied automatically.
    glitchkid::analysis::VocalAnalyzer& getAnalyzer() { return analyzer; }

    // SOURCES. The lead vocal is this track's own input. The instrumental is an OPTIONAL second input bus ("Instrumental", a sidechain-style input):
    // it is only measured, never mixed into the output. The host must route something to it (see docs/SOURCES.md); a disabled bus is "not connected".
    // "Connected, silent" means the bus is enabled but carries nothing: hosts deliver silence for an unrouted sidechain, so the two cannot be told apart.
    enum class SourceStatus { notConnected, connectedSilent, live };
    SourceStatus getInstrumentalStatus() const;
    glitchkid::analysis::VocalAnalyzer& getInstrumentalAnalyzer() { return beatAnalyzer; }
    void startSourceAnalysis(); // starts the lead and (if enabled) the instrumental analysis together
    void stopSourceAnalysis();
    float getAutoIntensity() const;                      // 0..1, saved in the DAW session (default 0.6)
    void setAutoIntensity (float);
    glitchkid::automode::RangeLookup makeRangeLookup() const; // legal range + step of every parameter (for the AutoMode and BUILD MY VOCAL MIX engines)
    glitchkid::automode::Result previewAutoMode (float intensity) const; // what APPLY would do with the last analysis (changes nothing)
    glitchkid::automode::Result applyAutoMode (float intensity);         // computes and applies it; the sound is then a normal, editable "AutoMode" sound
    static constexpr const char* autoModePresetName = "AutoMode ASTRO MODE";

    juce::String getPresetName() const;                  // "" if none; saved in the DAW session
    void clearPresetName();                              // after the loaded preset's file was deleted: the sound stays, the name goes
    bool isPresetModified() const;                       // false if no preset was loaded or saved since this instance was restored

    // A/B comparison: two temporary snapshots kept in the plug-in state (so they survive a DAW save/reopen). Only the explicit actions
    // below change a slot; switching (recall) never does.
    static constexpr int numAbSlots = 2;
    bool hasAbSlot (int slot) const;
    glitchkid::SoundState getAbSlot (int slot) const;
    void captureAbSlot (int slot);                       // store the live sound in the slot and mark it active
    bool recallAbSlot (int slot);                        // false if the slot is empty; otherwise applies it and marks it active
    bool copyAbSlot (int from, int to);                  // false if `from` is empty
    int getAbActive() const;                             // 0 = A, 1 = B, -1 = neither
    bool isAbSlotModified() const;                       // the live sound differs from the active slot's snapshot

    // Peak meters: the audio thread raises these (linear, pre-/post-processing); the UI reads and resets them.
    float consumeInputPeak()  { return inputPeak.exchange (0.0f); }
    float consumeOutputPeak() { return outputPeak.exchange (0.0f); }
    // The newest audio for the interface's waveform and spectrum views (the vocal input before processing, the output after it, and the Instrumental input).
    const glitchkid::analysis::DisplayTap& getInputTap() const { return inputTap; }
    const glitchkid::analysis::DisplayTap& getOutputTap() const { return outputTap; }
    const glitchkid::analysis::DisplayTap& getBeatTap() const { return beatTap; }
    float consumeBeatPeak() { return beatPeak.exchange (0.0f); } // peak of the Instrumental input since the last call (0 when none is connected)
    CleanLab::Applied getCleanApplied() const { return cleanLab.applied(); } // what CLEAN LAB is really applying right now (UI telemetry)
    float consumeGainReductionDb() { return gainReductionPeak.exchange (0.0f); } // positive dB

    // For the UI (and tests): tempo used by Glitch Stutter, and whether it came from the host.
    float getStutterTempoBpm() const { return shownTempo.load(); }
    bool isStutterTempoFromHost() const { return tempoFromHost.load(); }
    int getStutterLoopLength() const { return shownLoopLength.load(); } // 0 when not repeating

    // Pitch Tune read-outs for the UI: MIDI note numbers (fractional), -1 when no voice is being corrected.
    float getTuneDetectedMidi() const { return shownDetectedMidi.load(); }
    float getTuneTargetMidi() const { return shownTargetMidi.load(); }

    // The latency this instance should report right now (6 samples, plus the Pitch Tune delay while it is on, plus the Formant delay while it is away from 0, plus the Harmony delay while it is on). A timer
    // on the message thread calls updateReportedLatency(); tests call it directly.
    int getWantedLatencySamples() const;
    void updateReportedLatency();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override { updateReportedLatency(); }
    void processChunk (juce::AudioBuffer<float>&);
    glitchkid::analysis::VocalAnalyzer analyzer;
    glitchkid::analysis::VocalAnalyzer beatAnalyzer;
    std::atomic<bool> beatBusOn { false };
    std::atomic<float> beatSilentSeconds { 0.0f };
    double beatSilentSamples = 0.0, sourceRate = 48000.0; // audio thread
    static constexpr const char* autoIntensityKey = "autoIntensity";
    void formantListen (const juce::AudioBuffer<float>&, int numChannels, int dryChannels, int numSamples);

    std::atomic<float>* gainDb = nullptr;
    std::atomic<float>* bypass = nullptr;
    struct EqParams { std::atomic<float>* freq; std::atomic<float>* gain; std::atomic<float>* q; };
    EqParams eqParams[ParametricEq::numBands] {};

    std::atomic<float>* compParams[6] {}; // threshold, ratio, attack, release, knee, makeup
    CompressorSettings readCompressorSettings() const;

    ParametricEq eq;
    Compressor compressor;
    Saturation saturation;
    AirLab airLab;
    CleanLab cleanLab;
    std::atomic<float>* cleanParams[5] {}; // on, amount, tone, res, ess
    std::atomic<float>* airParams[6] {}; // on, presence, air, link, mix, trim
    GlitchStutter stutter;
    std::atomic<float>* stutterOn = nullptr;
    std::atomic<float>* stutterRate = nullptr;
    std::atomic<float>* stutterMix = nullptr;
    float currentBpm = GlitchStutter::fallbackBpm; // resolved once per block from the host play head
    std::atomic<float> shownTempo { GlitchStutter::fallbackBpm };
    std::atomic<bool> tempoFromHost { false };
    std::atomic<int> shownLoopLength { 0 };
    LiquidEcho echo;
    Dimension dimension;
    glitchkid::SoundState presetBaseline;  // message thread only
    bool hasPresetBaseline = false;
    std::atomic<float>* dimParams[5] {}; // on, width, detune, depth, mix
    VocalDoubler doubler;
    std::atomic<float>* dblParams[5] {}; // on, amount, width, detune, timing
    DoublerSettings readDoublerSettings() const;
    glitchkid::harmony::HarmonyVoice harmony;
    std::atomic<float>* harmParams[3] {}; // on, interval, level
    glitchkid::harmony::HarmonyVoice::Settings readHarmonySettings() const;
    FixedDelay harmonyDryDelay;           // extra dry-path delay for Harmony (used while it is on)
    juce::AudioBuffer<float> dryBuffer4;  // preallocated
    std::vector<float> harmonyBlendBuffer;
    juce::SmoothedValue<float> harmonyBlend { 0.0f }; // 1 = Harmony path (delayed), 0 = straight through
    int harmonyLatency = 0;
    VocalMutation mutation;
    std::atomic<float>* mutParams[4] {}; // on, amount, character, blend
    MutationSettings readMutationSettings() const;
    DimensionSettings readDimensionSettings() const;
    std::atomic<float>* echoParams[8] {}; // on, sync, time, division, feedback, tone, width, mix
    EchoSettings readEchoSettings() const;
    glitchkid::pitch::PitchCorrector pitch;
    glitchkid::pitch::PitchCorrector::Settings readTuneSettings() const;
    AirLab::Settings readAirSettings() const;
    CleanLab::Settings readCleanSettings() const;
    std::atomic<float>* tuneParams[5] {}; // on, key, scale, strength, speed
    FixedDelay pitchDryDelay;             // extra dry-path delay for Pitch Tune (used while it is on)
    juce::AudioBuffer<float> dryBuffer2, undelayedBuffer; // preallocated in prepareToPlay
    std::vector<float> blendBuffer;       // per-sample Pitch Tune blend for the current chunk
    juce::SmoothedValue<float> tuneBlend { 0.0f }; // 1 = Pitch Tune path (delayed), 0 = straight through
    int pitchLatency = 0;

    glitchkid::formant::FormantShifter formant;
    std::atomic<float>* formantShift = nullptr;
    FixedDelay formantDryDelay;           // extra dry-path delay for Formant (used while it is away from 0)
    juce::AudioBuffer<float> dryBuffer3, scratchBuffer; // preallocated; scratch keeps the shifter listening while it is not used
    std::vector<float> formantBlendBuffer;
    juce::SmoothedValue<float> formantBlend { 0.0f }; // 1 = Formant path (delayed), 0 = straight through
    int formantLatency = 0;
    std::atomic<float> shownDetectedMidi { -1.0f }, shownTargetMidi { -1.0f };
    FixedDelay dryDelay; // delays the dry (bypass) path by the saturation latency
    std::atomic<float>* satDrive = nullptr;
    std::atomic<float>* mixPercent = nullptr;
    juce::AudioBuffer<float> dryBuffer; // preallocated in prepareToPlay, used for the bypass crossfade
    glitchkid::analysis::DisplayTap inputTap, outputTap, beatTap;
    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f }, gainReductionPeak { 0.0f }, beatPeak { 0.0f };
    juce::SmoothedValue<float> smoothedGain { 1.0f }; // linear
    juce::SmoothedValue<float> wetMix { 1.0f };       // 1 = processed, 0 = dry (bypass)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlitchKidProcessor)
};
