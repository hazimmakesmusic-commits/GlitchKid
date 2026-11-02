#pragma once
#include "../PluginProcessor.h"
#include "../Mastering/MasterRender.h"
#include "NeonKit.h"
#include <array>
#include <functional>
#include <memory>

namespace glitchkid::ui
{
// MASTER, the Mastering Lab: a separate workspace for a finished stereo mix (GlitchKid on the master bus in Mastering mode, or a mix file rendered
// offline), inside the editor's frame (its ORIGINAL / PROCESSED switch the level-matched original and the master).
//   top     - the Mastering mode switch and what it means;
//   input   - the mix's real loudness, peaks, clipping and stereo correlation, and whether it is ready to master (the live input, or a loaded file);
//   style   - four original starting points (real processing configurations, see master::resolve);
//   output  - loudness (BS.1770), true peak against the -1 dBTP ceiling, limiting, clipping, output gain;
//   dials   - Intensity, Loudness, Tone, Width;
//   footer  - Load mix, Export master (a 24-bit WAV rendered offline with these settings), reset the meters.
// UI only; every control is a parameter or a real action. Message thread, 15 Hz, only while showing.
class MasteringScreen : public juce::Component, private juce::Timer
{
public:
    struct Hooks { std::function<void()> back; };
    MasteringScreen (GlitchKidProcessor&, Hooks);
    ~MasteringScreen() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

    // tests
    void tickForTest() { timerCallback(); }
    void pressForTest (const juce::String& buttonText);
    void loadForTest (const juce::File& f) { load (f); }
    void exportForTest (const juce::File& f) { exportTo (f); }
    bool busyForTest() const { return job.isRunning() || waiting; }
    juce::String statusForTest() const { return status; }
    juce::String verdictForTest() const { return verdict; }
    juce::Slider& dialForTest (int i) { return dials[(size_t) i].slider; }
    bool exportEnabledForTest() const { return exportButton.isEnabled(); }

private:
    struct Dial
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
        juce::String name;
        std::function<juce::String (float)> text;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
        bool large = true;
    };
    // A style: its name and what it really does.
    struct Tile : juce::Button
    {
        Tile() : juce::Button ("style") { setWantsKeyboardFocus (true); setClickingTogglesState (false); }
        juce::String title, description;
        void paintButton (juce::Graphics&, bool over, bool down) override;
    };

    GlitchKidProcessor& proc;
    Hooks hooks;
    nk::Knob knob;
    std::array<Dial, 5> dials; // intensity, loudness, tone, width, output
    std::array<Tile, master::numStyles> tiles;
    nk::Pill modeButton { "Mastering mode: off" },
               loadButton { "Load mix" }, liveButton { "Use live input" }, exportButton { "Export master" }, resetButton { "Reset meters" };
    master::MasterJob job;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File mixFile;
    master::MixReport fileReport;
    bool waiting = false;
    juce::String status, verdict;
    juce::Rectangle<int> modeLine, inputBox, styleBox, outputBox, dialRow, footer;

    // what the meters show (copied from the audio thread's atomics on the timer)
    float inSt = master::LoudnessMeter::silence, inInt = master::LoudnessMeter::silence, inTp = master::LoudnessMeter::silence, inPk = master::LoudnessMeter::silence, inCorr = 1.0f;
    float outSt = master::LoudnessMeter::silence, outInt = master::LoudnessMeter::silence, outTp = master::LoudnessMeter::silence, gr = 0.0f, comp = 0.0f, match = 0.0f;
    int inClips = 0, outClips = 0;

    void setParam (const char* id, float plain);
    float value (const char* id) const;
    void load (const juce::File&);
    void exportTo (const juce::File&);
    void sync();
    void timerCallback() override;
    void paintInput (juce::Graphics&);
    void paintOutput (juce::Graphics&);
};
}
