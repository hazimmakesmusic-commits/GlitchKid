#pragma once
#include "../PluginProcessor.h"
#include "NeonKit.h"
#include <array>
#include <functional>
#include <memory>

namespace glitchkid::ui
{
// The GlitchKid instrument: the main screen (the whole window). Three zones only:
//   header   - the wordmark, the preset selector (the editor's own preset logic), ORIGINAL / PROCESSED (the real bypass), Kid and the detailed pages;
//   ribbon   - one central visual: the level of what comes in (a faint line) and what GlitchKid sends out (the neon ribbon), read from the plug-in's own
//              display taps; nothing is drawn that is not measured;
//   controls - seven large dials in three sections, each bound to existing parameters or the existing macros (TUNE, CLEAN; TONE, PUNCH, SPACE; MIX, OUTPUT).
// UI only: no audio, DSP, parameter or state change. Message thread, 30 Hz, only while showing.
class MainScreen : public juce::Component, private juce::Timer
{
public:
    struct Hooks
    {
        std::function<void (juce::Component& anchor)> presetMenu;  // the editor's preset menu, opened under `anchor`
        std::function<void (int direction)> presetStep;            // previous (-1) / next (+1) preset
        std::function<void()> openKid, openAdvanced, openMastering;
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
    void pressForTest (const juce::String& buttonText);
    juce::String presetTextForTest() const { return presetButton.getButtonText(); }

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
    Pill prevButton { "<" }, presetButton { "Preset" }, nextButton { ">" }, originalButton { "Original" }, processedButton { "Processed" },
         kidButton { "Kid" }, advancedButton { "Advanced" }, masteringButton { "Mastering" };

    std::array<float, 240> envIn {}, envOut {}; // level history, newest first (0..1 on a 60 dB scale)
    std::vector<float> scratch;
    juce::uint32 lastIn = 0, lastOut = 0;
    bool signalOut = false;
    float inDb = -100.0f, outDb = -100.0f;
    int tick = 0;
    juce::Rectangle<int> header, ribbon, controls;
    std::array<juce::Rectangle<int>, 3> sections;

    void setupDials();
    void setParam (const char* id, float plain);
    float value (const char* id) const;
    void sync();
    void timerCallback() override;
    void paintRibbon (juce::Graphics&);
};
}
