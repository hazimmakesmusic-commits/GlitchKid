#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "BeatLockProcessor.h"
#include "../Analysis/EqAnalyzer.h"
#include "../UI/PlaygroundWidgets.h"

// GLITCHKID BEAT LOCK interface (1120 x 680 design size, scaled as a whole when the window is resized). Everything drawn comes from the real signal or the real
// DSP state: the instrumental's input and output spectra, the vocal sidechain's spectrum, the engine's live per-band cuts and the vocal-activity gate.
// Message thread only; the audio thread is read through atomics, lock-free display taps and the analysis FIFO.
namespace glitchkid::beatlock::ui
{
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& name, const juce::String& tooltip);
    void resized() override;
    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
private:
    juce::Label label;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

class Graph : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Graph (BeatLockProcessor&);
    void tick (float dtSeconds);  // spectra and live state, once per UI frame
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    int bandAt (juce::Point<float>) const;
    juce::Rectangle<float> plotArea() const;
    float xOf (float hz) const;
    float hzOf (float x) const;
    float cutY (float db) const;
    float specY (float db) const;
private:
    BeatLockProcessor& proc;
    glitchkid::analysis::EqAnalyzer specIn, specOut, specVocal;
    std::array<float, numBands> cut {};
    float gate = 0.0f;
    SidechainStatus status = SidechainStatus::notConnected;
    int dragBand = -1;
    juce::RangedAudioParameter* param (const juce::String& id) const { return proc.apvts.getParameter (id); }
};

class BandEditor : public juce::Component
{
public:
    explicit BandEditor (BeatLockProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh (const Engine&); // live cut readouts
private:
    struct Column
    {
        juce::Label title, cutNow;
        juce::ToggleButton on { "On" };
        juce::Slider freq, q, maxCut;
        juce::Label freqL, qL, maxCutL;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAtt, qAtt, maxAtt;
    };
    std::array<Column, numBands> cols;
};

// LEARN / ANALYZE results, over the graph. LEARN shows current -> proposed and applies only on APPLY; ANALYZE changes nothing.
class AnalysisPanel : public juce::Component
{
public:
    AnalysisPanel();
    void paint (juce::Graphics&) override;
    void resized() override;
    void showRunning (ConflictAnalysis::Mode, float progress, float active, float elapsed);
    void showMessage (const juce::String& title, const juce::String& text);
    void showAnalysis (const AnalysisResult&);
    void showLearn (const AnalysisResult&, const LearnProposal&, const std::array<float, numBands>& currentFreqs, float currentSensitivity);
    std::function<void()> onApply, onClose;
    juce::TextButton apply { "Apply" }, discard { "Discard" }, close { "Close" };
private:
    enum class View { message, running, analysis, learn } view = View::message;
    juce::String title, text;
    AnalysisResult result;
    LearnProposal proposal;
    std::array<float, numBands> current {};
    float currentSens = 0.0f, progress = 0.0f;
};

class RoutingPanel : public juce::Component
{
public:
    RoutingPanel();
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::TextButton close { "Close" };
};
}

class BeatLockEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BeatLockEditor (BeatLockProcessor&);
    ~BeatLockEditor() override;
    void paint (juce::Graphics&) override {}
    void resized() override;

    // tests
    void tickForTest() { timerCallback(); }
    juce::String statusTextForTest() const { return statusText; }
    void startAnalysisForTest (bool learnMode) { startAnalysis (learnMode ? glitchkid::beatlock::ConflictAnalysis::Mode::learn : glitchkid::beatlock::ConflictAnalysis::Mode::analyze); }
    bool analysisRunningForTest() const { return analysis.running(); }
    void applyLearnForTest() { applyLearn(); }
    void closePanelForTest() { closePanel(); }
    bool panelShownForTest() const { return panel.isVisible(); }
    const glitchkid::beatlock::LearnProposal& proposalForTest() const { return proposal; }
    void setAdvancedForTest (bool a) { advanced.setToggleState (a, juce::sendNotificationSync); }
    void setRoutingForTest (bool r) { if (r) routing.setVisible (true); else routing.setVisible (false); }

private:
    struct Content : public juce::Component
    {
        explicit Content (BeatLockEditor& e) : editor (e) {}
        void paint (juce::Graphics&) override;
        BeatLockEditor& editor;
    };
    void timerCallback() override;
    void layout();
    void startAnalysis (glitchkid::beatlock::ConflictAnalysis::Mode);
    void finishAnalysis();
    void applyLearn();
    void closePanel();

    BeatLockProcessor& proc;
    glitchkid::ui::PlaygroundLookAndFeel lnf;
    Content content { *this };
    glitchkid::beatlock::ui::Graph graph { proc };
    glitchkid::beatlock::ui::BandEditor bands { proc };
    glitchkid::beatlock::ui::AnalysisPanel panel;
    glitchkid::beatlock::ui::RoutingPanel routing;
    std::vector<std::unique_ptr<glitchkid::beatlock::ui::Knob>> knobs;
    juce::TextButton bypass { "Bypass" }, reset { "Reset" }, learn { "Learn" }, analyze { "Analyze" }, advanced { "Advanced" }, howTo { "How to route" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt;
    juce::TooltipWindow tooltips { this, 500 };

    juce::String statusText;
    juce::Colour statusColour;
    float meterDb = -90.0f;
    glitchkid::beatlock::ConflictAnalysis analysis;
    glitchkid::beatlock::LearnProposal proposal;
    std::vector<float> drainV, drainI, drainG;
    friend struct Content;
};
