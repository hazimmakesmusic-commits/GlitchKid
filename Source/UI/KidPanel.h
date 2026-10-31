#pragma once
#include "../PluginProcessor.h"
#include "../Kid/KidDirector.h"
#include "../Kid/KidSpeech.h"
#include "../Analysis/VocalFileAnalysis.h"

namespace glitchkid::ui
{
// The KID page, the Vocal Producer: the first screen. Talk or type to KID, pick a profile, Build My Mix or Clean My Vocal, review KID's take and Apply it.
// Nothing changes the sound until Apply; Apply goes through GlitchKidProcessor::applyParameterChanges (host gestures) and the editor's one-step UNDO.
// The six big dials are bound to real parameters (or the existing macros) and move them directly, like any other control.
// Speech is push-to-talk on a separate command microphone. A loaded vocal file is ANALYSED on a worker thread; it is not played (playback stays in the DAW).
// Everything here runs on the message thread at 30 Hz, only while the page is showing.
class KidPanel : public juce::Component, private juce::Timer
{
public:
    struct Hooks
    {
        std::function<void (const SoundState& before)> beforeApply; // the editor stores it for UNDO
        std::function<void (const juce::String&)> status;
        std::function<void (int stage)> openStage;                  // ADVANCED: open the page that holds this stage's controls
    };
    enum class Mood { idle, listening, analysing, building, previewing, complete, error };
    static constexpr int numDials = 6;

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
    void loadFileForTest (const juce::File& f) { loadFile (f); }
    bool fileBusyForTest() const { return fileJob.isRunning() || fileWaiting; }
    const analysis::VocalFileReport& fileReportForTest() const { return fileReport; }
    Mood moodForTest() const { return mood(); }
    juce::Slider& dialForTest (int i) { return dials[(size_t) i].slider; }
    juce::String dialTextForTest (int i) const { return dials[(size_t) i].text (dials[(size_t) i].get()); }
    void compareForTest (bool original) { setCompare (original); }
    void pressForTest (const juce::String& buttonText);

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

    // One big dial: a real parameter (or an existing macro), read back every tick so it always agrees with the detailed controls.
    struct Dial
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
        juce::String name, what;
        juce::Colour accent;
        int stage = 0;                                   // ADVANCED page for this dial
        std::function<float()> get;
        std::function<void (float)> set;
        std::function<juce::String (float)> text;
        bool dragging = false;
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
    Tab tab = Tab::take;
    std::vector<Line> lines;
    juce::String errorText, pendingText;
    bool pendingSpoken = false, fileWaiting = false, levelMatch = true;
    int analysisStartCount = 0, chosenProfile = 0, cleanIntensity = 1;
    juce::uint32 analysisStartedMs = 0, recordStartedMs = 0, builtAtMs = 0, appliedAtMs = 0, errorAtMs = 0;
    bool haveFacts = false;
    kid::AnalysisFacts facts;
    analysis::VocalAnalysis lastAnalysis;
    SoundState lastSeen;
    int tick = 0;
    float scriptScroll = 0.0f, takeScroll = 0.0f, scriptMax = 0.0f, takeMax = 0.0f;
    std::array<float, 96> env {};      // the live level history (from the output display tap)
    std::array<float, 72> micLevels {}; // the recording waveform
    std::vector<float> tapScratch;
    bool signalPresent = false;
    juce::uint32 lastWriteCount = 0;

    MicButton mic;
    juce::TextEditor input;
    juce::TextButton sendButton { "SEND" }, setupButton { "SET UP SPEECH" }, applyButton { "APPLY" }, cancelButton { "CANCEL" }, analyseButton { "LISTEN LIVE" },
                     loadButton { "LOAD VOCAL FILE" }, buildButton { "BUILD MY MIX" }, cleanButton { "CLEAN MY VOCAL" },
                     originalButton { "ORIGINAL" }, processedButton { "PROCESSED" }, matchButton { "LEVEL MATCH" };
    juce::TextButton tabButtons[3], profileButtons[kid::numProfiles], intensityButtons[3], advancedButtons[6];
    juce::OwnedArray<juce::TextButton> chips;
    std::array<Dial, numDials> dials;

    juce::Rectangle<int> srcBox, waveArea, dispBox, mascotBox, scriptBox, takeBox, takeBody, dialBox, compareBox, cmdBox, waveBox, actionBox;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> rowHits; // take-row checkboxes

    void send (const juce::String& text, bool spoken);
    void finishRequest();
    void apply();
    void cancelTake();
    void say (const juce::String& kidLine) { lines.push_back ({ true, kidLine }); scriptScroll = 1.0e9f; }
    void fail (const juce::String& kidLine) { say (kidLine); errorAtMs = juce::Time::getMillisecondCounter(); }
    void startRecording();
    void stopRecording (bool cancel);
    void chooseFile();
    void loadFile (const juce::File&);
    void fileDone (const analysis::VocalFileReport&);
    void setCompare (bool original);
    void refreshChips();
    void refreshButtons();
    void setupDials();
    void syncDials();
    void setParam (const char* id, float plain); // one host gesture per call, like applyParameterChanges
    juce::StringArray suggestions() const;
    juce::String phaseLabel() const;
    juce::String phaseHint() const;
    Mood mood() const;
    void readTap();
    void timerCallback() override;
    void paintSource (juce::Graphics&);
    void paintDisplay (juce::Graphics&);
    void paintMascot (juce::Graphics&, juce::Rectangle<float>, Mood);
    void paintScript (juce::Graphics&, juce::Rectangle<int>);
    void paintTake (juce::Graphics&, juce::Rectangle<int>);
    void paintRead (juce::Graphics&, juce::Rectangle<int>);
    void paintLimits (juce::Graphics&, juce::Rectangle<int>);
    void paintDials (juce::Graphics&);
    void paintCompare (juce::Graphics&);
    void paintCommand (juce::Graphics&);
    void paintActions (juce::Graphics&);
    float value (const char* id) const;
};
}
