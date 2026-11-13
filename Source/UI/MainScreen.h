#pragma once
#include "../PluginProcessor.h"
#include "NeonKit.h"
#include <array>
#include <functional>
#include <memory>

namespace glitchkid::ui
{
// HOME: the GlitchKid vocal instrument. It fills the whole window (the editor's rail, info line, SAVE / DELETE and A/B stay in ADVANCED MIXING); the
// editor lays its own preset selector, UNDO, MOTION and ORIGINAL / PROCESSED buttons on top of it, so they are the same buttons as on every page.
//   top     - the preset, Choose a sound, Ask Kid, ADVANCED MIXING (opens the detailed pages);
//   visual  - the live vocal: input and output levels from the plug-in's own display taps, the limiter ceiling, the pitch read-out; press and hold it to
//             hear the ORIGINAL (the Bypass parameter, the same as the header's button). Nothing is drawn that is not measured or set;
//   modules - AUTOTUNE, CLEAN LAB, AIR LAB, LIMITER: an on/off switch and one main amount each, bound to their existing parameters, with their real status;
//   basics  - TONE, PUNCH, SPACE (the existing macros), PRESENCE (AIR LAB's presence), MIX, OUTPUT, and ORIGINAL / PROCESSED.
// UI only: no audio, DSP or parameter is added; every control writes the same parameters as the detailed pages, so both always agree.
// Message thread, 30 Hz, only while showing.
class MainScreen : public juce::Component, private juce::Timer
{
public:
    struct Hooks
    {
        std::function<void()> openSounds, openKid, openAdvanced;
        std::function<void (bool)> holdOriginal;            // press and hold the visual: true = ORIGINAL, false = back to PROCESSED
        std::function<bool (int& tonic, bool& minor)> fileKey; // a reliable key from a vocal file analysed by Kid, if any
    };
    enum Dial { tune, clean, air, ceiling, tone, punch, space, presence, mix, output, numDials };
    enum Module { autotune, cleanLab, airLab, limiter, numModules };

    MainScreen (GlitchKidProcessor&, Hooks);
    ~MainScreen() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    void setLimiterReduction (float db) { limGrDb = db; } // from the editor, which consumes the processor's limiter reading once per tick
    juce::Rectangle<int> topBar() const { return { 0, 0, getWidth(), 52 }; }
    juce::Rectangle<int> compareArea() const { return compare; } // where the editor puts ORIGINAL / PROCESSED

    // tests
    juce::Slider& dialForTest (int i) { return dials[(size_t) i].slider; }
    juce::String dialNameForTest (int i) const { return dials[(size_t) i].name; }
    juce::String dialTextForTest (int i) const { return dials[(size_t) i].text ((float) dials[(size_t) i].slider.getValue()); }
    juce::Button& moduleSwitchForTest (int m) { return switches[(size_t) m]; }
    juce::String moduleStatusForTest (int m) const { return status[(size_t) m]; }
    juce::String headlineForTest() const { return headline; }
    juce::String beatForTest() const { return beatText; }
    juce::Rectangle<int> visualForTest() const { return ribbon; }
    void tickForTest() { timerCallback(); }
    bool signalForTest() const { return signalOut; }
    bool mandalaForTest() const { return mandala; }
    void pressForTest (const juce::String& buttonText);
    void holdForTest (juce::Point<int> at) { press (at); }
    void releaseForTest() { release(); }
    void dragForTest (int dial, double v) // a hand gesture: drag start, the move, drag end
    {
        auto& s = dials[(size_t) dial].slider;
        if (s.onDragStart) s.onDragStart();
        s.setValue (v, juce::sendNotificationSync);
        if (s.onDragEnd) s.onDragEnd();
    }
    bool useKeyShownForTest() const { return useKeyButton.isVisible(); }

private:
    struct DialItem
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
        juce::String name, what;
        std::function<float()> get;                  // macro dials only (read back from the parameters)
        std::function<void (float)> set;
        std::function<juce::String (float)> text;
        std::unique_ptr<juce::SliderParameterAttachment> attachment; // single-parameter dials: JUCE's own gesture-correct binding
        const char* switchOn = nullptr;              // dragging the amount of a module that is off switches the module on
        bool dragging = false;                       // a hand gesture is moving it (AIR LAB's LINK follows hand moves only)
    };
    using Knob = nk::Knob;
    using Pill = nk::Pill;

    GlitchKidProcessor& proc;
    Hooks hooks;
    Knob knob;
    std::array<DialItem, numDials> dials;
    std::array<Pill, numModules> switches { Pill ("OFF"), Pill ("OFF"), Pill ("OFF"), Pill ("OFF") };
    std::array<std::unique_ptr<juce::ButtonParameterAttachment>, numModules> switchAtt;
    juce::ComboBox keyBox, scaleBox;
    std::unique_ptr<juce::ComboBoxParameterAttachment> keyAtt, scaleAtt;
    Pill soundsButton { "Choose a sound" }, kidButton { "Ask Kid" }, advancedButton { "ADVANCED MIXING" }, mandalaButton { "Mandala" }, useKeyButton { "Use" };

    std::array<float, 240> envIn {}, envOut {}; // level history, newest first (0..1 on a 60 dB scale)
    std::vector<float> scratch;
    juce::uint32 lastIn = 0, lastOut = 0;
    int stillTicks = 1000;          // ticks since the plug-in last processed audio (the taps stopped moving)
    bool signalOut = false, holding = false;
    float inDb = -100.0f, outDb = -100.0f, limGrDb = 0.0f;
    int tick = 0;
    bool motion = true;             // the editor's MOTION switch (uiMotion): off = the bloom and drift hold still
    float bloom = 0.0f;
    bool mandala = false;           // the live view drawn as a mirrored, 8-fold kaleidoscope of the same real levels (uiMandala)
    float spin = 0.0f;              // the mandala's slow turn: only while audio plays and MOTION is on
    int suggestedKey = -1;          // from Kid's file analysis: tonic 0..11, + 12 when minor; -1 = none
    juce::String headline, beatText, pitchText, keyText;
    std::array<juce::String, numModules> status;
    std::array<bool, numModules> active {};
    juce::Rectangle<int> visual, ribbon, modulesArea, basics, compare; // visual: the whole live area; ribbon: the drawing inside it
    std::array<juce::Rectangle<int>, numModules> modules;

    void setupDials();
    void setParam (const char* id, float plain);
    float value (const char* id) const;
    bool on (const char* id) const { return value (id) >= 0.5f; }
    void sync();
    void refreshText();
    void timerCallback() override;
    void paintRibbon (juce::Graphics&);
    void paintMandala (juce::Graphics&);
    void paintOverlays (juce::Graphics&);
    void press (juce::Point<int>);
    void release();
};
}
