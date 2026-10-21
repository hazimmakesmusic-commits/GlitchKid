#pragma once
#include "PluginProcessor.h"
#include "Presets/PresetController.h"
#include "Macros/MacroEngine.h"
#include "AutoMode/MixEngine.h"
#include "UI/PlaygroundWidgets.h"

// Simple vertical peak meter, -60 dBFS to +6 dBFS; red above 0 dBFS. Falls at a fixed rate.
class LevelMeter : public juce::Component
{
public:
    void pushPeak (float linearPeak);
    void paint (juce::Graphics&) override;

private:
    static constexpr float minDb = -60.0f, maxDb = 6.0f;
    float shownDb = minDb;
};

// Gain-reduction meter: grows downward from the top, 0 to 24 dB, falls at a fixed rate.
class GainReductionMeter : public juce::Component
{
public:
    void pushReduction (float db);
    void paint (juce::Graphics&) override;

private:
    static constexpr float maxDb = 24.0f;
    float shownDb = 0.0f;
};

// A rotary knob with a name label, bound to one parameter.
class ParamKnob
{
public:
    ParamKnob (juce::AudioProcessorValueTreeState&, const char* paramId, const juce::String& name,
               const juce::String& suffix, juce::Colour colour, const juce::String& tooltip = {});
    void addTo (juce::Component& parent);
    void setBounds (juce::Rectangle<int> r);
    void setEnabled (bool);
    void setVisible (bool);

private:
    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label label;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

// The three workspaces: TUNE (key, scale, tuning style), AUTO (pick a profile, BUILD MY VOCAL MIX, read the result) and MODIFY (Pitch & Key, Tone, Dynamics, Space).
// A header (name, workspaces, DRY / PROCESSED, MOTION), the real processing chain and the preset bar are always shown.
class GlitchKidEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GlitchKidEditor (GlitchKidProcessor&);
    ~GlitchKidEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // Layout size; the window scales it to fit. Every control that is not part of the current page is hidden.
    static constexpr int designW = 960, designH = 600;
    enum class Page { tune = 0, autoMix = 1, modify = 2 };
    static constexpr int numCategories = 5; // PITCH & KEY, TONE, DYNAMICS, SPACE, CLEAN
    Page page = Page::tune;
    int category = 0;
    bool motionOn = true;
    glitchkid::ui::PlaygroundLookAndFeel lookAndFeel; // declared first: it must outlive every control that uses it
    juce::Component content; // declared before the controls so it outlives them
    float scaleFactor = 1.0f;
    void layoutContent();
    void showPage (Page, bool remember);
    void showCategory (int, bool remember);
    std::vector<juce::Rectangle<int>> panelRects;
    void timerCallback() override;
    void refreshState (float peak, float outPeak);
    void setBypassFromUi (bool bypassed);
    juce::TooltipWindow tooltipWindow { this, 500 }; // the controls' tooltips need one to be shown at all

    float pageFade = 1.0f; // 0..1: a short fade-in whenever the page or category changes
    void startFade();

    juce::TextButton navButtons[3], categoryButtons[5];
    juce::TextButton nextButton { "NEXT: AUTO  >" }, modifyButton { "MODIFY  >" }, backButton { "<  BACK TO AUTO" };
    juce::TextButton motionButton, resetButton { "RESET" }, savePresetButton { "SAVE PRESET" };
    glitchkid::ui::ScopeStrip scope;
    glitchkid::ui::ChainStrip chain;
    juce::TextButton dryButton { "DRY" }, processedButton { "PROCESSED" };
    juce::String shownCompareText;
    float inPeakShown = 0.0f, outPeakShown = 0.0f;
    juce::Label brandLabel { {}, "GLITCHKID" }, brandSub { {}, "VOCAL LAB" }, scopeCaption { {}, "INPUT SIGNAL" }, keyLabel { {}, "KEY" }, scaleCaption { {}, "SCALE" };

    // TUNE note pad and scale chips: they drive the same Tune Key / Tune Scale parameters as the boxes on MODIFY.
    juce::OwnedArray<glitchkid::ui::NoteKey> noteKeys;
    juce::TextButton scaleChips[6];
    int shownKey = -1, shownScale = -1;
    void refreshTunePad();
    juce::TextButton styleButtons[glitchkid::macros::numTuneStyles];
    juce::Label tuneSubline, tuneSummary, tuneOnCaption { {}, "TUNING" }, styleCaption { {}, "TUNING STYLE" }, tuneKeyLabel { {}, "KEY" }, tuneScaleLabel { {}, "SCALE" }, tuneReadout;
    juce::ToggleButton tuneOnButton { "ON" };
    juce::ComboBox tuneKeyBox, tuneScaleBox;
    juce::OwnedArray<ParamKnob> tuneKnobs; // strength, speed
    juce::String shownTuneText;

    // AUTO: the eight profiles, the build button, the result
    juce::OwnedArray<glitchkid::ui::ProfileCard> profileCards;
    juce::Label profileHeading { {}, "1  STARTING PROFILE" }, buildHeading { {}, "2  BUILD" }, resultHeading { {}, "3  RESULT" };
    juce::Label detailDesc, detailSettings, detailStart, sourceLine, autoStatus, resultTitle, resultBody, autoStyleLabel { {}, "MIX INTENSITY" };
    glitchkid::ui::BuildButton autoAnalyzeButton;   // BUILD MY VOCAL MIX (listens, then builds)
    juce::TextButton autoApplyButton { "WHAT CHANGED" }, loadButton { "LOAD PROFILE" }, backToMineButton { "BACK TO MY SOUND" };
    juce::Slider autoIntensitySlider;
    int browsedProfile = -1;
    juce::String shownDetailKey, shownAutoText, shownResultText;
    bool autoWaiting = false, autoSawListening = false;
    int signalTicks = 0; // frames since the input last carried a signal (for the "signal detected" status)
    struct Undo { glitchkid::SoundState state; juce::String name; bool valid = false; } undo; // your sound from before the last build or profile load

    // BUILD MY VOCAL MIX: the big button listens to the vocal (the host must be playing), then the mix engine builds a proposal from the selected profile and the
    // measurements and it is applied to the real parameters. BACK TO MY SOUND restores the sound from before the build.
    enum class BuildState { idle, listening, built, failed };
    BuildState buildState = BuildState::idle;
    int buildStartCount = 0; // the analyzer's completed-analysis counter when BUILD was pressed
    glitchkid::automode::MixProposal lastProposal;
    juce::String buildMessage, resultSummary; // resultSummary: the concise list of what the last build really changed
    void runBuild();
    void showWhatChanged();
    void refreshAutoMode();
    void selectProfile (int);
    void loadBrowsed();
    void backToMine();
    void refreshProfiles();

    // MODIFY: one high-level control per category (TONE, PUNCH, SPACE; PITCH & KEY has the tuning styles) and the detailed controls (every one is a real parameter)
    juce::Slider macroSliders[3];
    juce::Label macroName, macroLow, macroHigh;
    void applyMacroSlider (int);
    std::unique_ptr<glitchkid::ui::EqGraph> eqGraph; // the real EQ response (TONE)
    juce::TextButton bandButtons[glitchkid::ui::EqGraph::numBands]; // LOW, BODY, PRESENCE, AIR
    juce::OwnedArray<ParamKnob> eqKnobs[glitchkid::ui::EqGraph::numBands]; // each band's frequency, gain and (bells) Q; only the selected band's are shown
    juce::OwnedArray<ParamKnob> warmthKnob;
    juce::Label eqNote;
    // AIR LAB strip (TONE): switch, LINK, PRESENCE and AIR (the two main controls), MIX and OUTPUT TRIM
    juce::ToggleButton airOnButton { "AIR LAB" }, airLinkButton { "LINK" };
    juce::Slider airPresenceSlider, airAirSlider, airMixSlider, airTrimSlider;
    juce::Label airPresenceLabel { {}, "PRESENCE" }, airAirLabel { {}, "AIR" }, airMixLabel { {}, "MIX" }, airTrimLabel { {}, "TRIM" }, airHint;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> airPresenceAtt, airAirAtt, airMixAtt, airTrimAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> airOnAtt, airLinkAtt;
    bool airLinkGuard = false;
    // CLEAN (category 5): master switch and amount, a trim and live read-outs per section (everything shown comes from the DSP's own telemetry)
    juce::ToggleButton cleanOnButton { "CLEAN LAB" };
    juce::Slider cleanAmountSlider, cleanToneSlider, cleanResSlider, cleanEssSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cleanAmountAtt, cleanToneAtt, cleanResAtt, cleanEssAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> cleanOnAtt;
    juce::Label cleanAmountLabel { {}, "CLEAN" }, cleanHint, cleanLearn, cleanLevel, toneHead { {}, "CORRECTIVE EQ" }, resHead { {}, "RESONANCE" }, essHead { {}, "DE-ESSER" },
                toneTrimLabel { {}, "AMOUNT" }, resTrimLabel { {}, "AMOUNT" }, essTrimLabel { {}, "AMOUNT" }, toneRead[2], resRead[3], essRead;
    juce::TextButton cleanResetButton { "RESET CLEAN" };
    glitchkid::ui::ReductionBar essBar;
    void refreshClean (bool cleanOn);

    void linkAir (juce::Slider& moved, juce::Slider& other);
    int eqBand = 2;
    void selectBand (int);
    void applyContentTransform (float dx = 0.0f);
    juce::OwnedArray<ParamKnob> compKnobs; // threshold, ratio, attack, release, makeup
    juce::OwnedArray<ParamKnob> gainKnob;
    juce::OwnedArray<ParamKnob> echoKnobs; // time, feedback, tone, mix
    juce::OwnedArray<ParamKnob> dimKnobs;  // width, mix
    juce::OwnedArray<ParamKnob> dblKnobs;  // amount, width
    LevelMeter inMeter, outMeter;
    GainReductionMeter grMeter;
    juce::Label inLabel { {}, "IN" }, outLabel { {}, "OUT" }, grLabel { {}, "GR" }, echoTitle { {}, "ECHO" }, echoDivLabel { {}, "DIVISION" }, dimTitle { {}, "WIDTH" },
                dblTitle { {}, "DOUBLER" };
    juce::ToggleButton echoOnButton { "ON" }, echoSyncButton { "SYNC" }, dimOnButton { "ON" }, dblOnButton { "ON" };
    juce::ComboBox echoDivBox;
    juce::ToggleButton bypassButton { "Bypass" }; // (attached to the Bypass parameter; never shown: DRY / PROCESSED is the interface to it)

    GlitchKidProcessor& glitchProcessor;

    // PRESETS + A/B bar. PRESET opens a menu (factory default, then your saved presets); SAVE names and stores the current sound; DELETE removes
    // the loaded user preset (after a confirmation). A/B: SET A / SET B capture the current sound into a slot, A / B recall it.
    juce::TextButton presetPrevButton { "<" }, presetNextButton { ">" }, presetMenuButton, presetSaveButton { "SAVE" }, presetDeleteButton { "DELETE" };
    juce::TextButton abSetA { "SET A" }, abSetB { "SET B" }, abRecallA { "A" }, abRecallB { "B" };
    juce::Label presetStatus, abLabel { {}, "A/B" };
    juce::String shownPresetText, shownAbText;
    int statusTicks = 0;
    void refreshPresetBar();
    void showStatus (const juce::String&);
    void openPresetMenu();
    void askSavePreset();
    void trySave (const juce::String& name, bool overwrite);
    void askDeletePreset();
    void askYesNo (const juce::String& heading, const juce::String& message, const juce::String& yesText, std::function<void()> onYes);

    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment echoOnAttachment, echoSyncAttachment;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment echoDivAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment dimOnAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment tuneOnAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment dblOnAttachment;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment tuneKeyAttachment, tuneScaleAttachment;
    glitchkid::PresetController presets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlitchKidEditor)
};
