#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MasterProcessor.h"
#include "../UI/PlaygroundWidgets.h"

// GLITCHKID MASTER interface (1120 x 680 design size, scaled as a whole when the window is resized). Every number and bar is read from the audio thread's
// meters (BS.1770 loudness, true peak, gain reduction, correlation, clipping) or is a parameter; a reading that is not available yet says so ("-").
// There is no Export button: the master is exported by the DAW (the panel says how), because the plug-in only ever sees the audio the host plays through it.
// Message thread only; 30 Hz while the editor exists.
class MasterEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MasterEditor (MasterProcessor&);
    ~MasterEditor() override;
    void paint (juce::Graphics&) override {}
    void resized() override;

    // tests
    void tickForTest() { timerCallback(); }
    juce::String statusForTest() const { return status; }
    void pressForTest (const juce::String& buttonText);
    juce::Slider& knobForTest (int i) { return knobs[(size_t) i]->slider; }
    juce::String knobNameForTest (int i) const { return knobs[(size_t) i]->name; }
    int numKnobsForTest() const { return (int) knobs.size(); }
    bool exportHelpShownForTest() const { return exportHelp.isVisible(); }
    juce::String readoutForTest (bool output, int row) const { return (output ? outRows : inRows)[(size_t) row]; } // 0 M, 1 S, 2 I, 3 TP, 4 peak, 5 corr, 6 clipped
    float grHistoryForTest (int age) const { return grLim[(size_t) ((grPos - 1 - age + histLen) % histLen)]; }

private:
    struct Knob : juce::Component
    {
        Knob (MasterProcessor&, const char* id, const juce::String& name, const juce::String& tip);
        void resized() override;
        juce::String name;
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    };
    struct ExportHelp : juce::Component
    {
        void paint (juce::Graphics&) override;
        juce::TextButton close { "Close" };
        ExportHelp() { addAndMakeVisible (close); }
        void resized() override { close.setBounds (getWidth() - 96, getHeight() - 44, 80, 28); }
    };
    struct Content : juce::Component
    {
        explicit Content (MasterEditor& e) : editor (e) {}
        void paint (juce::Graphics&) override;
        MasterEditor& editor;
    };
    void timerCallback() override;
    void layout();
    void paintContent (juce::Graphics&);
    void paintMeters (juce::Graphics&, juce::Rectangle<int>, bool output);
    void paintHistory (juce::Graphics&, juce::Rectangle<int>);

    MasterProcessor& proc;
    glitchkid::ui::PlaygroundLookAndFeel lnf;
    Content content { *this };
    std::vector<std::unique_ptr<Knob>> knobs;
    std::array<juce::TextButton, glitchkid::master::numStyles> styleTiles;
    juce::TextButton bypass { "Bypass" }, reset { "Reset" }, setA { "Set A" }, setB { "Set B" }, recallA { "A" }, recallB { "B" },
                     original { "Original" }, processed { "Processed" }, resetMeters { "Reset meters" }, howExport { "How to export" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt;
    ExportHelp exportHelp;
    juce::TooltipWindow tooltips { this, 500 };

    // what is shown (copied from the audio thread on the timer)
    static constexpr int histLen = 240; // 8 s at 30 Hz
    std::array<float, histLen> grLim {}, grGlue {};
    int grPos = 0;
    std::array<float, 4> bars {};       // in L, in R, out L, out R: peak, falling 20 dB/s
    std::array<juce::String, 7> inRows, outRows;
    float outTp = glitchkid::master::LoudnessMeter::silence, ceiling = -1.0f, matchDb = 0.0f;
    juce::String status, styleText;
    juce::Colour statusColour;
    juce::uint32 lastBlocks = 0;
    int stillTicks = 1000, lastNonFinite = 0, nonFiniteTicks = 0;
    juce::Rectangle<int> inPanel, outPanel, historyBox, stylesBox, dialsBox, compareBox;
};
