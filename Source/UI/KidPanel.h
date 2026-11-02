#pragma once
#include "../PluginProcessor.h"
#include "../Kid/KidDirector.h"
#include "../Kid/KidSpeech.h"
#include "../Analysis/VocalFileAnalysis.h"

namespace glitchkid::ui
{
// KID, the vocal producer: GlitchKid's main screen (the whole window). Four areas only: the top bar (wordmark and Kid's status), the vocal (load a file,
// its real waveform or the live output, ORIGINAL / PROCESSED at matched loudness), the conversation with Kid, and one main action (BUILD MY MIX, then
// UPDATE MY MIX). The detailed pages are a small row of links at the foot.
//
// Kid's mix is applied straight away through GlitchKidProcessor::applyParameterChanges (host gestures, automation, saved state as usual) and can be undone
// step by step. The plan is always rebuilt from the sound the session started with, so feedback reshapes the same mix. Controls moved by hand are left alone.
// A loaded vocal file is ANALYSED on a worker thread, never played (playback stays in the DAW). Everything here runs on the message thread, at 30 Hz,
// only while the screen is showing.
class KidPanel : public juce::Component, private juce::Timer
{
public:
    struct Hooks
    {
        std::function<void (const SoundState& before)> beforeApply; // the editor stores it for its own UNDO on the detailed pages
        std::function<void (const juce::String&)> status;
        std::function<void (int stage)> openStage;                  // the detailed controls for a stage (1..17)
        std::function<void()> back;                                  // back to the instrument
    };
    enum class Status { ready, listening, analyzing, processing };

    KidPanel (GlitchKidProcessor&, Hooks);
    ~KidPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void visibilityChanged() override;

    // tests
    void sendForTest (const juce::String& text) { send (text, false); }
    void tickForTest() { timerCallback(); }
    const kid::Director& directorForTest() const { return director; }
    Status statusForTest() const { return status(); }
    juce::String statusTextForTest() const { return statusText(); }
    bool waitingForTest() const { return phase == Phase::analysing; }
    juce::String lastKidLineForTest() const;
    bool micEnabledForTest() const { return mic.isEnabled(); }
    void loadFileForTest (const juce::File& f) { loadFile (f); }
    bool fileBusyForTest() const { return fileJob.isRunning() || fileWaiting; }
    const analysis::VocalFileReport& fileReportForTest() const { return fileReport; }
    void compareForTest (bool original) { setCompare (original); }
    void pressForTest (const juce::String& buttonText);
    juce::String mainActionTextForTest() const { return mainButton.getButtonText(); }
    bool builtForTest() const { return built; }
    int undoDepthForTest() const { return (int) history.size(); }
    const std::set<juce::String>& manualForTest() const { return manualIds; }

private:
    enum class Phase { ready, recording, transcribing, analysing, downloading };
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
    // One flat, quiet button style for the whole screen: text, a hairline when idle, violet when on, a soft glow for the main action.
    struct Pill : juce::Button
    {
        explicit Pill (const juce::String& label, bool primary = false) : juce::Button (label), main (primary) { setWantsKeyboardFocus (true); }
        bool main = false;
        void paintButton (juce::Graphics&, bool over, bool down) override;
    };

    GlitchKidProcessor& proc;
    Hooks hooks;
    kid::Director director;
    kid::CommandMic commandMic;
    kid::Recogniser recogniser;
    kid::ModelDownloader downloader;
    analysis::VocalFileJob fileJob;
    analysis::VocalFileReport fileReport;
    std::unique_ptr<juce::FileChooser> chooser;
    Phase phase = Phase::ready;
    std::vector<Line> lines;
    juce::String pendingText;
    bool pendingSpoken = false, fileWaiting = false;
    int analysisStartCount = 0, chosenProfile = 0;
    juce::uint32 analysisStartedMs = 0, recordStartedMs = 0, appliedAtMs = 0;
    bool haveFacts = false;
    kid::AnalysisFacts facts;
    analysis::VocalAnalysis lastAnalysis;

    // the session: the sound it started from, what Kid last applied, the controls moved by hand, and the undo steps (sound + Kid's memory)
    bool built = false;
    SoundState lastApplied;
    std::set<juce::String> manualIds;
    std::vector<std::pair<SoundState, kid::Director>> history;

    int tick = 0;
    float scroll = 0.0f, scrollMax = 0.0f;
    std::array<float, 160> env {}; // the live output level history (the display tap)
    std::array<float, 72> micLevels {};
    std::vector<float> tapScratch;
    bool signalPresent = false;
    juce::uint32 lastWriteCount = 0;

    MicButton mic;
    juce::TextEditor input;
    Pill sendButton { "Send" }, setupButton { "Set up speech" }, loadButton { "Load vocal" }, originalButton { "Original" }, processedButton { "Processed" },
         mainButton { "BUILD MY MIX", true }, undoButton { "Undo" }, backButton { "Instrument" };
    Pill advanced[5] { Pill ("Tune"), Pill ("Clean"), Pill ("Tone"), Pill ("Dynamics"), Pill ("Space") };
    juce::OwnedArray<Pill> chips;

    juce::Rectangle<int> topBar, vocalBox, waveArea, chatBox, inputBox, footBox;

    void send (const juce::String& text, bool spoken);
    void startListening();
    void finishRequest();
    void run (kid::IntentFrame);
    void applyPlan (const SoundState& before, const kid::Director& memoryBefore);
    void undoStep();
    void say (const juce::String& kidLine) { lines.push_back ({ true, kidLine }); scroll = 1.0e9f; repaint (chatBox); }
    void startRecording();
    void stopRecording (bool cancel);
    void chooseFile();
    void loadFile (const juce::File&);
    void fileDone (const analysis::VocalFileReport&);
    void setCompare (bool original);
    void refreshChips();
    void refreshButtons();
    juce::StringArray suggestions() const;
    Status status() const;
    juce::String statusText() const;
    void readTap();
    void watchManualMoves();
    void timerCallback() override;
    void paintVocal (juce::Graphics&);
    void paintChat (juce::Graphics&);
    float value (const char* id) const;
    void setParam (const char* id, float plain);
};
}
