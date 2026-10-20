#pragma once
#include "PluginProcessor.h"
#include "Presets/PresetController.h"
#include "Macros/MacroEngine.h"
#include "Presets/PresetBrowser.h"
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
               const juce::String& suffix, juce::Colour colour);
    void addTo (juce::Component& parent);
    void setBounds (juce::Rectangle<int> r);
    void setEnabled (bool);
    void setVisible (bool);

private:
    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label label;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

class GlitchKidEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GlitchKidEditor (GlitchKidProcessor&);
    ~GlitchKidEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // Layout size; the window scales it to fit. The interface is three steps, one page each: TUNE (key + scale), AUTO (sound + analysis), MODIFY (advanced,
    // one category at a time). A header (brand, step buttons, mascot) and the preset bar are always shown; everything else belongs to exactly one page.
    static constexpr int designW = 1060, designH = 700;
    enum class Page { tune = 0, autoMix = 1, modify = 2 };
    enum class Category { pitch = 0, tone, dynamics, space, character };
    static constexpr int numCategories = 5;
    Page page = Page::tune;
    int category = 0;
    bool motionOn = true;
    glitchkid::ui::PlaygroundLookAndFeel lookAndFeel; // declared first: it must outlive every control that uses it
    juce::Component content; // declared before the controls so it outlives them
    float scaleFactor = 1.0f;
    void layoutContent();
    void showPage (Page, bool remember);
    void showCategory (int, bool remember);
    void buildPageControls();
    std::vector<juce::Rectangle<int>> panelRects;
    void timerCallback() override;
    void refreshHome (float peak);
    void setBypassFromUi (bool bypassed);
    juce::TooltipWindow tooltipWindow { this, 500 }; // the controls' tooltips need one to be shown at all

    // HOME screen: mascot + live input scope, Sound Worlds, the main AutoMode action, macros, quick commands, dry/processed switch.
    glitchkid::ui::Mascot mascot;
    glitchkid::ui::ScopeStrip scope;
    // AUTO: the artist-preset gallery. Eight card slots are rebound to the presets of the current page (50 cards are never on screen at once).
    juce::OwnedArray<glitchkid::ui::ArtistCard> artistCards;
    juce::TextButton filterChips[glitchkid::browser::numFilters], sortButton, favOnlyButton { "FAVOURITES" }, favButton { "FAVOURITE" },
                     pagePrevButton { "<" }, pageNextButton { ">" }, loadButton { "LOAD SOUND" }, backToMineButton { "BACK TO MY SOUND" };
    juce::TextEditor searchBox;
    juce::Label sourceLine;
    juce::Label galleryCount, detailDesc, detailTags, detailSettings, detailStart;
    glitchkid::browser::Query browseQuery;
    std::vector<int> galleryList;
    int galleryPage = 0, browsedPreset = -1;
    juce::String shownDetailKey;
    std::uint64_t favourites = 0;
    struct Undo { glitchkid::SoundState state; juce::String name; bool valid = false; } undo; // your sound from before the last artist preset was loaded
    static constexpr int cardsPerPage = 8;
    void refreshGallery();
    void selectArtist (int preset);
    void loadBrowsed();
    void backToMine();
    void toggleFavourite();
    juce::OwnedArray<glitchkid::ui::MacroDial> macroDials; // SPACE, CHARACTER, PUNCH
    std::unique_ptr<glitchkid::ui::MacroDial> chaosDial;    // the Alter Ego CHAOS amount (needs an Alter Ego)
    juce::TextButton commandButtons[glitchkid::macros::numCommands];
    juce::Label talkCaption { {}, "QUICK TWEAKS" }, talkNote, compareCaption;
    juce::TextButton dryButton { "DRY" }, processedButton { "PROCESSED" }, motionButton;

    // Navigation and page controls
    juce::TextButton navButtons[3], categoryButtons[numCategories], nextButton { "NEXT  >" }, modifyButton { "MODIFY  >" },
                     backButton { "<  BACK TO AUTO" }, resetButton { "RESET" }, savePresetButton { "SAVE PRESET" },
                     styleButtons[glitchkid::macros::numTuneStyles];
    // TUNE note pad and scale chips: they drive the same Tune Key / Tune Scale parameters as the boxes on MODIFY.
    juce::OwnedArray<glitchkid::ui::NoteKey> noteKeys;
    juce::TextButton scaleChips[6];
    int shownKey = -1, shownScale = -1;
    float pageFade = 1.0f; // 0..1: a short fade-in whenever the page or category changes
    void refreshTunePad();
    void startFade();
    juce::Label tuneSubline, tuneSummary, tuneOnCaption { {}, "TUNING" }, styleCaption { {}, "TUNING STYLE" };

    // The glass screen: its title is painted (not a label) so it can scramble and glitch. Glitch bursts are short (about a third of a second), happen
    // at random moments and on every click, and are skipped when MOTION is off.
    juce::Rectangle<int> screenRect, mascotBase;
    juce::String titleTarget, titleShown;
    int scrambleFrames = 0, glitchFrames = 0, nextBurstIn = 70;
    unsigned glitchSeed = 1;
    juce::Random glitchRng { 7 };
    juce::String currentTitle() const;
    void startBurst();
    void mouseDown (const juce::MouseEvent&) override;
    juce::String shownCompareText;

    GlitchKidProcessor& glitchProcessor;
    LevelMeter inMeter, outMeter;
    GainReductionMeter grMeter;
    juce::Label compTitle { {}, "COMPRESSOR" }, grLabel { {}, "GR" };
    juce::Label title { {}, "GLITCHKID" };
    juce::Label tagline { {}, "A playground for your sound." };
    juce::Label inLabel { {}, "IN" }, outLabel { {}, "OUT" };
    juce::Label bandNames[ParametricEq::numBands];
    juce::ToggleButton bypassButton { "Bypass" };
    juce::Label stutterTitle { {}, "GLITCH STUTTER" }, stutterRateLabel { {}, "RATE" }, tempoLabel;
    juce::ToggleButton stutterOnButton { "ON" };
    juce::ComboBox stutterRateBox;
    juce::OwnedArray<ParamKnob> stutterMixKnob;
    juce::String shownTempoText;

    // Pitch Tune row: ON, key, scale, strength, speed, FORMANT (independent of ON), and a read-out of the sung note and the note it is pulled to.
    juce::Label tuneTitle { {}, "PITCH TUNE  +  FORMANT" }, tuneKeyLabel { {}, "KEY" }, tuneScaleLabel { {}, "SCALE" }, tuneReadout;
    juce::ToggleButton tuneOnButton { "ON" };
    juce::ComboBox tuneKeyBox, tuneScaleBox;
    juce::OwnedArray<ParamKnob> tuneKnobs; // strength, speed, formant
    juce::String shownTuneText;

    // Vocal Mutation row: ON, a read-out of what the settings do (type + shift in Hz), MUTATION, CHARACTER, BLEND.
    juce::Label mutTitle { {}, "VOCAL MUTATION" }, mutReadout;
    juce::ToggleButton mutOnButton { "ON" };
    juce::OwnedArray<ParamKnob> mutKnobs; // mutation, character, blend
    juce::String shownMutText;

    // DOUBLE / HARMONY row: the Vocal Doubler (DOUBLE ON, DOUBLE amount, WIDTH, DETUNE, TIMING) and the fixed-interval Harmony voice (HARMONY ON, INTERVAL, LEVEL).
    juce::Label dhTitle { {}, "DOUBLE / HARMONY" }, harmIntervalLabel { {}, "INTERVAL" }, harmNote;
    juce::ToggleButton dblOnButton { "DOUBLE ON" }, harmOnButton { "HARMONY ON" };
    juce::ComboBox harmIntervalBox;
    juce::OwnedArray<ParamKnob> dhKnobs; // double, width, detune, timing, harmony level
    juce::String shownHarmNote;

    // Liquid Echo row: ON, SYNC, division, time, feedback, tone, width, mix.
    juce::Label echoTitle { {}, "LIQUID ECHO" }, echoDivLabel { {}, "DIVISION" };
    juce::ToggleButton echoOnButton { "ON" }, echoSyncButton { "SYNC" };
    juce::ComboBox echoDivBox;
    juce::OwnedArray<ParamKnob> echoKnobs; // time, feedback, tone, width, mix

    // PRESETS + A/B bar. PRESET opens a menu (factory default, then your saved presets); SAVE names and stores the current sound; DELETE removes the
    // loaded user preset (after a confirmation). A/B: SET A / SET B capture the current sound into a slot, A / B recall it, A>B / B>A copy a slot.
    juce::TextButton presetPrevButton { "<" }, presetNextButton { ">" }, presetMenuButton, presetSaveButton { "SAVE" }, presetDeleteButton { "DELETE" };
    juce::TextButton abSetA { "SET A" }, abSetB { "SET B" }, abRecallA { "A" }, abRecallB { "B" }, abCopyAB { "A>B" }, abCopyBA { "B>A" };
    juce::Label presetStatus, abLabel { {}, "A/B" };
    juce::String shownPresetText, shownAbText;

    // AUTOMODE row: ANALYZE listens to the vocal, APPLY commits the analysis once. See PluginProcessor.h.
    glitchkid::ui::ReactorButton autoAnalyzeButton;
    juce::TextButton autoApplyButton { "APPLY SOUND" };
    juce::Label autoStyleLabel { {}, "AUTO-MIX: ASTRO MODE" }, autoIntensityLabel { {}, "INTENSITY" }, autoStatus;
    juce::Slider autoIntensitySlider;
    bool autoWaiting = false, autoSawListening = false;
    juce::String shownAutoText;
    void refreshAutoMode();
    int statusTicks = 0;
    void refreshPresetBar();
    void showStatus (const juce::String&);
    void openPresetMenu();
    void askSavePreset();
    void trySave (const juce::String& name, bool overwrite);
    void askDeletePreset();
    void askYesNo (const juce::String& heading, const juce::String& message, const juce::String& yesText, std::function<void()> onYes);

    // ALTER EGOS + MOOD ENGINE (see AlterEgos.h). The lit Alter Ego is the one whose values all match the controls.
    // CHAOS and SURPRISE ME write the normal parameters; BLEND is the ordinary "mix" parameter.
    juce::Label egoTitle { {}, "ALTER EGOS" }, egoInfo, chaosLabel { {}, "CHAOS" }, blendLabel { {}, "BLEND" };
    juce::TextButton egoButtons[AlterEgos::count];
    juce::Slider chaosSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider blendSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::TextButton surpriseButton { "SURPRISE ME" };
    bool updatingChaosFromCode = false;
    juce::String shownEgoText;
    void refreshAlterEgos();

    // Dimension row: ON, width, detune, depth, mix.
    juce::Label dimTitle { {}, "DIMENSION" };
    juce::ToggleButton dimOnButton { "ON" };
    juce::OwnedArray<ParamKnob> dimKnobs;

    juce::OwnedArray<ParamKnob> gainKnob;
    juce::OwnedArray<ParamKnob> driveKnob;
    juce::OwnedArray<ParamKnob> compKnobs;
    juce::OwnedArray<ParamKnob> eqKnobs[ParametricEq::numBands];
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment stutterOnAttachment;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment stutterRateAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment echoOnAttachment, echoSyncAttachment;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment echoDivAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment dimOnAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment tuneOnAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment mutOnAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment dblOnAttachment, harmOnAttachment;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment harmIntervalAttachment;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment tuneKeyAttachment, tuneScaleAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment blendAttachment;
    glitchkid::PresetController presets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlitchKidEditor)
};
