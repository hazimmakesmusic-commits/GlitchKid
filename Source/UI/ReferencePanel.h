#pragma once
#include "../PluginProcessor.h"
#include "../Analysis/ReferenceAnalyzer.h"
#include "../AutoMode/ReferenceMatch.h"
#include "LabKit.h"

// The REFERENCE workspace ("Reference Match"; its result is called Reference-Inspired EQ, never an exact vocal match). Message thread only. It owns the reference
// file analysis job (off the audio thread, cancellable), measures the user's vocal from the existing input tap (no audio-thread change), shows the proposal as a
// list of ticked changes with the reason for each, and applies only the ticked ones, with the editor's normal UNDO. Nothing is applied without a press of APPLY.
namespace glitchkid::ui
{
// Reference minus the vocal as it sounds now, per third-octave band (real numbers from the two analyses; nothing is drawn without both).
class ToneDiffView : public juce::Component, public juce::SettableTooltipClient
{
public:
    void set (const std::array<float, analysis::SpectralProfile::numBands>& diffDb, int bands, bool valid);
    void paint (juce::Graphics&) override;

private:
    std::array<float, analysis::SpectralProfile::numBands> diff {};
    int bands = 0;
    bool valid = false;
};

class ReferencePanel : public juce::Component
{
public:
    struct Hooks
    {
        std::function<void (const SoundState& before)> beforeApply; // the editor stores it for UNDO
        std::function<void()> requestUndo;                          // the editor's UNDO
        std::function<void (const juce::String&)> status;
    };
    ReferencePanel (GlitchKidProcessor&, Hooks);
    ~ReferencePanel() override;

    void tick (float dt);        // ~60 Hz while visible: feeds the vocal measurement, shows progress, refreshes the proposal when something it depends on changed
    void paint (juce::Graphics&) override;
    void resized() override;
    void cancelEverything();     // stops a running job and a running vocal measurement (the editor calls it on close)

    // tests and profiling only
    void setProfilesForTest (const analysis::SpectralProfile& reference, const analysis::SpectralProfile& vocal);
    void setReferenceProfileForTest (const analysis::SpectralProfile& reference) { referenceProfile = reference; propose(); }
    const automode::MatchProposal& proposalForTest() const { return proposal; }
    void applyForTest() { applySelected(); }
    void setIntensityForTest (float v) { intensity.setValue (v * 100.0, juce::sendNotificationSync); }
    void tickForTest() { tick (1.0f / 30.0f); }
    bool isApplyEnabledForTest() const { return applyButton.isEnabled(); }
    int visibleRowsForTest() const { int n = 0; for (int i = 0; i < rows; ++i) n += rowTitle[i].isVisible() ? 1 : 0; return n; }
    void setTickForTest (int row, bool on) { if ((size_t) row < ticked.size()) { ticked[(size_t) row] = on; rowTick[row].setToggleState (on, juce::dontSendNotification); } }
    bool isConfidenceHighForTest() const { return proposal.confidence == automode::Confidence::high; }
    juce::String statusTextForTest() const { return refStatus.getText() + " | " + vocalStatus.getText(); }
    void startVocalForTest() { startVocal(); }
    void stopVocalForTest() { finishVocal(); }
    bool vocalListeningForTest() const { return vocalListening; }

private:
    void chooseFile();
    void analyseReference();
    void onReferenceDone (const analysis::ReferenceResult&);
    void startVocal();
    void finishVocal();
    void propose();
    void refreshRows();
    void applySelected();
    void resetAll();
    juce::uint32 eqSignature() const;
    float eqResponseDb (const SoundState&, float hz) const;

    GlitchKidProcessor& processor;
    Hooks hooks;

    // state
    juce::File file;
    double fileSeconds = 0.0;
    analysis::ReferenceJob job;
    analysis::ReferenceResult refResult;
    analysis::SpectralProfile referenceProfile, vocalProfile;
    std::unique_ptr<analysis::SpectralAccumulator> vocalAcc;
    bool vocalListening = false;
    juce::uint32 vocalLastCount = 0, signature = 0;
    double vocalRate = 48000.0;
    float vocalSecondsHeard = 0.0f;
    automode::MatchProposal proposal;
    std::vector<bool> ticked;
    bool proposalFresh = false, applied = false;
    std::unique_ptr<juce::FileChooser> chooser;

    // widgets
    static constexpr int rows = 3;
    LabHeading title { "Reference Match", 17.0f }, step1 { "1  Reference file", 13.0f }, step2 { "2  Your vocal", 13.0f }, step3 { "3  Reference-Inspired EQ", 13.0f };
    juce::Label notice, fileName, refStatus, vocalStatus, confidence, confidenceWhy, intensityLabel, resultStatus, diffCaption;
    juce::TextButton chooseButton { "Choose file" }, analyseRefButton { "Analyze reference" }, cancelRefButton { "Cancel" }, vocalButton { "Analyze my vocal" }, applyButton { "Apply selected" },
                     cancelButton { "Cancel" }, resetButton { "Reset" }, undoButton { "Undo Reference EQ" };
    juce::ToggleButton autoSection { "Use the loudest 20 seconds" };
    juce::Slider sectionStart, intensity;
    LevelBar refProgress, vocalProgress;
    ToneDiffView diffView;
    juce::ToggleButton rowTick[rows];
    juce::Label rowTitle[rows], rowReason[rows];
};
}
