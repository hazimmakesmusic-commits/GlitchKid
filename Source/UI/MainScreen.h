#pragma once
#include "../PluginProcessor.h"
#include "NeonKit.h"
#include <array>
#include <functional>
#include <memory>

namespace glitchkid::ui
{
// START: the GlitchKid instrument, inside the editor's frame (the rail, ORIGINAL / PROCESSED, UNDO and the preset bar are the editor's). Three zones:
//   guide    - the next step (Choose a sound, which opens SOUNDS) and Kid;
//   ribbon   - one central visual: the level of what comes in (a faint line) and what GlitchKid sends out (the neon ribbon), read from the plug-in's own
//              display taps; nothing is drawn that is not measured;
//   controls - seven large dials in three sections, each bound to existing parameters or the existing macros (TUNE, CLEAN; TONE, PUNCH, SPACE; MIX, OUTPUT).
// UI only: no audio, DSP, parameter or state change. Message thread, 30 Hz, only while showing.
class MainScreen : public juce::Component, private juce::Timer
{
public:
    struct Hooks
    {
        std::function<void()> openSounds, openKid;
    };
    static constexpr int numDials = 7;

    MainScreen (GlitchKidProcessor&, Hooks);
    ~MainScreen() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

    // tests
    juce::Slider& dialForTest (int i) { return dials[(size_t) i].slider; }
    juce::String dialNameForTest (int i) const { return dials[(size_t) i].name; }
    juce::String dialTextForTest (int i) const { return dials[(size_t) i].text ((float) dials[(size_t) i].slider.getValue()); }
    void tickForTest() { timerCallback(); }
    bool signalForTest() const { return signalOut; }
    bool mandalaForTest() const { return mandala; }
    void pressForTest (const juce::String& buttonText);

private:
    struct Dial
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
        juce::String name, what;
        std::function<float()> get;
        std::function<void (float)> set;
        std::function<juce::String (float)> text;
        std::unique_ptr<juce::SliderParameterAttachment> attachment; // single-parameter dials: JUCE's own gesture-correct binding
        bool large = true;
    };
    using Knob = nk::Knob;
    using Pill = nk::Pill;

    GlitchKidProcessor& proc;
    Hooks hooks;
    Knob knob;
    std::array<Dial, numDials> dials;
    juce::ComboBox keyBox, scaleBox;
    std::unique_ptr<juce::ComboBoxParameterAttachment> keyAtt, scaleAtt;
    Pill soundsButton { "Choose a sound" }, kidButton { "Ask Kid" }, mandalaButton { "Mandala" }; // Mandala: the live view as a kaleidoscope (uiMandala)

    std::array<float, 240> envIn {}, envOut {}; // level history, newest first (0..1 on a 60 dB scale)
    std::vector<float> scratch;
    juce::uint32 lastIn = 0, lastOut = 0;
    bool signalOut = false;
    float inDb = -100.0f, outDb = -100.0f;
    int tick = 0;
    bool motion = true;  // the editor's MOTION switch (uiMotion): off = the bloom holds still
    float bloom = 0.0f;
    bool mandala = false; // the live view drawn as a mirrored, 8-fold kaleidoscope of the same real levels
    float spin = 0.0f;    // the mandala's slow turn: only while audio plays and MOTION is on  // the output level, slowly smoothed (about 1 s); lights the bloom behind the ribbon
    juce::Rectangle<int> guide, ribbon, controls;
    std::array<juce::Rectangle<int>, 3> sections;

    void setupDials();
    void setParam (const char* id, float plain);
    float value (const char* id) const;
    void sync();
    void timerCallback() override;
    void paintRibbon (juce::Graphics&);
    void paintMandala (juce::Graphics&);
};
}
