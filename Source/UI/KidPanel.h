#pragma once
#include "../PluginProcessor.h"
#include "../Kid/KidDirector.h"
#include "../Kid/KidSpeech.h"

namespace glitchkid::ui
{
// The KID page (the Booth): talk or type to KID, read the session, review KID's take and Apply it. Nothing changes the sound until Apply; Apply goes through
// GlitchKidProcessor::applyParameterChanges (host gestures) and the editor's one-step UNDO. Speech is push-to-talk on a separate command microphone.
class KidPanel : public juce::Component, private juce::Timer
{
public:
    struct Hooks
    {
        std::function<void (const SoundState& before)> beforeApply; // the editor stores it for UNDO
        std::function<void (const juce::String&)> status;
        std::function<void (int stage)> openStage;                  // THE PATH: open the page that holds this stage's controls
    };
    KidPanel (GlitchKidProcessor&, Hooks);
    ~KidPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void visibilityChanged() override;

    // tests
    void sendForTest (const juce::String& text) { send (text, false); }
    void tickForTest() { timerCallback(); }
    const kid::Director& directorForTest() const { return director; }
    juce::String phaseNameForTest() const { return phaseLabel(); }
    juce::String lastKidLineForTest() const;
    void applyForTest() { apply(); }
    void cancelForTest() { cancelTake(); }
    void setSelectedForTest (const juce::String& id, bool on) { director.setSelected (id, on); }
    bool micEnabledForTest() const { return mic.isEnabled(); }

private:
    enum class Phase { ready, recording, transcribing, analysing, review, downloading };
    enum class Tab { take, read, limits };
    struct Line { bool kid; juce::String text; bool spoken = false; };

    struct MicButton : juce::Component
    {
        std::function<void()> onPress, onRelease;
        bool live = false;
        juce::uint32 downAt = 0;
        bool tapMode = false;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        bool keyPressed (const juce::KeyPress&) override;
    };

    GlitchKidProcessor& proc;
    Hooks hooks;
    kid::Director director;
    kid::CommandMic commandMic;
    kid::Recogniser recogniser;
    kid::ModelDownloader downloader;
    Phase phase = Phase::ready;
    Tab tab = Tab::take;
    std::vector<Line> lines;
    juce::String errorText, pendingText;
    bool pendingSpoken = false;
    int analysisStartCount = 0;
    juce::uint32 analysisStartedMs = 0, recordStartedMs = 0;
    bool haveFacts = false;
    kid::AnalysisFacts facts;
    analysis::VocalAnalysis lastAnalysis;
    SoundState lastSeen;
    int tick = 0;
    float scriptScroll = 0.0f, takeScroll = 0.0f, scriptMax = 0.0f, takeMax = 0.0f;
    std::array<float, 96> env {};      // the voiceprint's level history (from the output display tap)
    std::array<float, 72> micLevels {}; // the recording waveform
    std::vector<float> tapScratch;
    bool signalPresent = false;
    juce::uint32 lastWriteCount = 0;

    MicButton mic;
    juce::TextEditor input;
    juce::TextButton sendButton { "SEND" }, setupButton { "SET UP SPEECH" }, applyButton { "APPLY" }, cancelButton { "CANCEL" }, analyseButton { "ANALYSE MY VOCAL" };
    juce::TextButton tabButtons[3];
    juce::OwnedArray<juce::TextButton> chips;

    juce::Rectangle<int> stageBox, voiceBox, scriptBox, takeBox, takeBody, cmdBox, waveBox, pathBox;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> rowHits; // take-row checkboxes
    std::array<juce::Rectangle<int>, 17> stationHits;

    void send (const juce::String& text, bool spoken);
    void finishRequest();
    void apply();
    void cancelTake();
    void say (const juce::String& kidLine) { lines.push_back ({ true, kidLine }); scriptScroll = 1.0e9f; }
    void startRecording();
    void stopRecording (bool cancel);
    void refreshChips();
    void refreshButtons();
    juce::StringArray suggestions() const;
    juce::String phaseLabel() const;
    juce::String phaseHint() const;
    std::set<int> kidStages() const;
    bool stageOn (int stage) const;
    void readTap();
    void timerCallback() override;
    void paintVoice (juce::Graphics&, juce::Rectangle<float>);
    void paintScript (juce::Graphics&, juce::Rectangle<int>);
    void paintTake (juce::Graphics&, juce::Rectangle<int>);
    void paintRead (juce::Graphics&, juce::Rectangle<int>);
    void paintLimits (juce::Graphics&, juce::Rectangle<int>);
    void paintCommand (juce::Graphics&);
    void paintPath (juce::Graphics&);
    float value (const char* id) const;
};
}
