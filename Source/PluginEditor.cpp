#include "PluginEditor.h"
#include "UI/EffectModel.h"

#ifndef GLITCHKID_VERSION
 #define GLITCHKID_VERSION "dev"
#endif

namespace
{
namespace ui = glitchkid::ui;

// "A3" for MIDI note 57 (middle C = C4 = 60).
juce::String noteName (float midi)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int n = (int) std::lround (midi);
    return juce::String (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1); // (voice range only: n is never negative)
}

// Purple is the brand; cyan and pink only tell a few effects apart.
juce::Colour identityOf (int effect)
{
    using namespace glitchkid::fx;
    switch (effect)
    {
        case mutation: case stutter:                   return ui::pink;
        case doubler: case echo: case dimension:       return ui::cyan;
        case formant: case harmony: case saturation:   return ui::neon;
        default:                                       return ui::violet;
    }
}

// What is worth knowing about an effect beyond its one-line description.
const char* noteFor (int effect)
{
    using namespace glitchkid::fx;
    switch (effect)
    {
        case pitchTune:   return "Works best on one dry voice.";
        case saturation:  return "It runs oversampled to keep the added harmonics clean.";
        default:          return "";
    }
}
}

void LevelMeter::pushPeak (float linearPeak)
{
    const float db = juce::Decibels::gainToDecibels (linearPeak, minDb);
    const float next = db > shownDb ? db : juce::jmax (db, shownDb - 1.5f); // ~45 dB/s at 30 Hz
    if (! juce::approximatelyEqual (next, shownDb))
    {
        shownDb = next;
        repaint();
    }
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0b0d13));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (ui::border);
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);

    const float h = r.getHeight() * juce::jmap (shownDb, minDb, maxDb, 0.0f, 1.0f);
    g.setColour (shownDb > 0.0f ? ui::errorRed : ui::violet);
    g.fillRoundedRectangle (r.withTop (r.getBottom() - h).reduced (2.0f, 0.0f), 2.0f);
    // the 0 dBFS tick
    const float y0 = r.getBottom() - r.getHeight() * juce::jmap (0.0f, minDb, maxDb, 0.0f, 1.0f);
    g.setColour (ui::textDim.withAlpha (0.7f));
    g.fillRect (r.getX(), y0, r.getWidth(), 1.0f);
}

void GainReductionMeter::pushReduction (float db)
{
    const float next = db > shownDb ? db : juce::jmax (db, shownDb - 0.5f); // ~15 dB/s at 30 Hz
    if (! juce::approximatelyEqual (next, shownDb))
    {
        shownDb = next;
        repaint();
    }
}

void GainReductionMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0b0d13));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (ui::border);
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
    g.setColour (ui::lime);
    g.fillRoundedRectangle (r.withHeight (r.getHeight() * juce::jlimit (0.0f, 1.0f, shownDb / maxDb)).reduced (2.0f, 0.0f), 2.0f);
}

ParamKnob::ParamKnob (juce::AudioProcessorValueTreeState& apvts, const char* paramId, const juce::String& name,
                      const juce::String& suffix, juce::Colour colour)
    : attachment (apvts, paramId, slider)
{
    slider.setTextValueSuffix (suffix);
    slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 16);
    slider.setDoubleClickReturnValue (true, apvts.getParameter (paramId)->convertFrom0to1 (apvts.getParameter (paramId)->getDefaultValue())); // double-click resets
    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (ui::fontOf (11.0f, true));
    label.setColour (juce::Label::textColourId, ui::textDim);
}

void ParamKnob::addTo (juce::Component& parent)
{
    parent.addAndMakeVisible (slider);
    parent.addAndMakeVisible (label);
}

void ParamKnob::setEnabled (bool shouldBeEnabled)
{
    slider.setEnabled (shouldBeEnabled);
    label.setAlpha (shouldBeEnabled ? 1.0f : 0.4f);
}

void ParamKnob::setVisible (bool shouldBeVisible)
{
    slider.setVisible (shouldBeVisible);
    label.setVisible (shouldBeVisible);
}

void ParamKnob::setBounds (juce::Rectangle<int> r)
{
    label.setBounds (r.removeFromTop (14));
    slider.setBounds (r);
}

// ---- the preset list on the PRESETS page ----------------------------------------------------------------------------------------------------

void GlitchKidEditor::PresetListModel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, editor.presetNames.size()))
        return;
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (2.0f, 1.0f);
    if (selected)
    {
        g.setColour (ui::violet.withAlpha (0.28f));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (ui::neon);
        g.fillRoundedRectangle (r.getX(), r.getY() + 5.0f, 3.0f, r.getHeight() - 10.0f, 1.5f);
    }
    g.setColour (selected ? ui::textBright : ui::textDim);
    g.setFont (ui::fontOf (13.0f, selected));
    g.drawText (editor.presetNames[row], r.withTrimmedLeft (12.0f).withTrimmedRight (70.0f), juce::Justification::centredLeft, true);
    const bool user = row < (int) editor.presetKinds.size() && editor.presetKinds[(size_t) row] == 1;
    g.setColour (user ? ui::lime : ui::textDim.withAlpha (0.7f));
    g.setFont (ui::fontOf (10.0f, true));
    g.drawText (user ? "USER" : "FACTORY", r.withTrimmedRight (8.0f), juce::Justification::centredRight, false);
}

void GlitchKidEditor::PresetListModel::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (! juce::isPositiveAndBelow (row, editor.presetNames.size()))
        return;
    editor.showStatus (editor.presets.loadByName (editor.presetNames[row]).message);
    editor.refreshAlterEgos();
    editor.shownPresetText = {};
    editor.refreshPresetBar();
}

GlitchKidEditor::GlitchKidEditor (GlitchKidProcessor& p)
    : AudioProcessorEditor (&p),
      glitchProcessor (p),
      bypassAttachment (p.apvts, GlitchKidProcessor::bypassId, bypassButton),
      stutterOnAttachment (p.apvts, GlitchKidProcessor::stutterOnId, stutterOnButton),
      stutterRateAttachment (p.apvts, GlitchKidProcessor::stutterRateId, stutterRateBox),
      echoOnAttachment (p.apvts, GlitchKidProcessor::echoOnId, echoOnButton),
      echoSyncAttachment (p.apvts, GlitchKidProcessor::echoSyncId, echoSyncButton),
      echoDivAttachment (p.apvts, GlitchKidProcessor::echoDivId, echoDivBox),
      dimOnAttachment (p.apvts, GlitchKidProcessor::dimOnId, dimOnButton),
      tuneOnAttachment (p.apvts, GlitchKidProcessor::tuneOnId, tuneOnButton),
      mutOnAttachment (p.apvts, GlitchKidProcessor::mutOnId, mutOnButton),
      dblOnAttachment (p.apvts, GlitchKidProcessor::dblOnId, dblOnButton),
      harmOnAttachment (p.apvts, GlitchKidProcessor::harmOnId, harmOnButton),
      harmIntervalAttachment (p.apvts, GlitchKidProcessor::harmIntervalId, harmIntervalBox),
      tuneKeyAttachment (p.apvts, GlitchKidProcessor::tuneKeyId, tuneKeyBox),
      tuneScaleAttachment (p.apvts, GlitchKidProcessor::tuneScaleId, tuneScaleBox),
      blendAttachment (p.apvts, GlitchKidProcessor::mixId, blendSlider),
      presets (p)
{
    using namespace glitchkid;
    setLookAndFeel (&lookAndFeel);
    motionOn = p.getUiFlag ("uiMotion", true);
    page = (Page) juce::jlimit (0, 3, p.getUiInt ("uiPage", 0));
    selectedEffect = juce::jlimit (0, fx::numEffects - 1, p.getUiInt ("uiEffect", 0));

    auto label = [this] (juce::Label& l, float size, bool bold, juce::Colour c, juce::Justification j = juce::Justification::centredLeft)
    {
        l.setFont (ui::fontOf (size, bold));
        l.setColour (juce::Label::textColourId, c);
        l.setJustificationType (j);
        content.addAndMakeVisible (l);
    };
    // A recessed plate button: purple on hover, lime when active.
    auto style = [this] (juce::TextButton& b, const juce::String& tip, juce::Colour text = ui::textBright)
    {
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1a1e29));
        b.setColour (juce::TextButton::buttonOnColourId, ui::lime);
        b.setColour (juce::TextButton::textColourOffId, text);
        b.setColour (juce::TextButton::textColourOnId, ui::obsidian);
        b.setTooltip (tip);
        content.addAndMakeVisible (b);
    };
    auto addAll = [this] (std::initializer_list<juce::Component*> cs) { for (auto* c : cs) content.addAndMakeVisible (c); };
    auto choices = [&p] (juce::ComboBox& box, const char* id)
    {
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (id)))
        {
            box.addItemList (c->choices, 1);
            box.setSelectedItemIndex (c->getIndex(), juce::dontSendNotification);
        }
    };

    // ---- zone A: top bar -------------------------------------------------------------------------------------------------------------
    style (presetMenuButton, "Choose the factory default sound or one of your saved presets.");
    style (presetPrevButton, "Previous preset (factory presets first, then yours). Replaces the current sound: press SET A first to keep it.");
    style (presetNextButton, "Next preset (factory presets first, then yours). Replaces the current sound: press SET A first to keep it.");
    presetMenuButton.onClick = [this] { openPresetMenu(); };
    for (auto* b : { &presetPrevButton, &presetNextButton })
        b->onClick = [this, b]
        {
            showStatus (presets.step (b == &presetNextButton ? 1 : -1).message);
            refreshAlterEgos();
            shownPresetText = {};
            refreshPresetBar();
        };
    style (abSetA, "Capture the current sound into slot A.", ui::neon);
    style (abSetB, "Capture the current sound into slot B.", ui::neon);
    style (abRecallA, "Switch to the sound stored in A.");
    style (abRecallB, "Switch to the sound stored in B.");
    style (abCopyAB, "Copy slot A into slot B.", ui::textDim);
    style (abCopyBA, "Copy slot B into slot A.", ui::textDim);
    abSetA.onClick = [this] { glitchProcessor.captureAbSlot (0); showStatus ("Current sound captured in A."); refreshPresetBar(); };
    abSetB.onClick = [this] { glitchProcessor.captureAbSlot (1); showStatus ("Current sound captured in B."); refreshPresetBar(); };
    abRecallA.onClick = [this] { if (glitchProcessor.recallAbSlot (0)) refreshAlterEgos(); refreshPresetBar(); };
    abRecallB.onClick = [this] { if (glitchProcessor.recallAbSlot (1)) refreshAlterEgos(); refreshPresetBar(); };
    abCopyAB.onClick = [this] { if (glitchProcessor.copyAbSlot (0, 1)) showStatus ("A copied to B."); refreshPresetBar(); };
    abCopyBA.onClick = [this] { if (glitchProcessor.copyAbSlot (1, 0)) showStatus ("B copied to A."); refreshPresetBar(); };
    label (abLabel, 12.0f, true, ui::textDim, juce::Justification::centredRight);

    gearButton.onClick = [this] { showPage (Page::settings, true); };
    addAll ({ &inActivity, &outActivity, &gearButton });

    // ---- zone B: input / output section ----------------------------------------------------------------------------------------------
    label (inputCaption, 11.0f, true, ui::textDim);
    label (inLabel, 10.0f, true, ui::textDim, juce::Justification::centred);
    label (outLabel, 10.0f, true, ui::textDim, juce::Justification::centred);
    addAll ({ &inMeter, &outMeter, &clipLed });
    // GlitchKid has no input-gain stage; this is the real output gain. (It is labelled as such: no fake controls.)
    gainKnob.add (new ParamKnob (p.apvts, GlitchKidProcessor::gainId, "OUTPUT GAIN", " dB", ui::violet));
    gainKnob[0]->addTo (content);
    style (dryButton, "Hear the untouched input. This is the real bypass (delay-matched and crossfaded over 20 ms), not the Mix knob.");
    style (processedButton, "Hear the processed sound.");
    dryButton.setColour (juce::TextButton::buttonOnColourId, ui::pink);
    processedButton.setColour (juce::TextButton::buttonOnColourId, ui::lime);
    dryButton.onClick = [this] { setBypassFromUi (true); };
    processedButton.onClick = [this] { setBypassFromUi (false); };

    // ---- zone C: navigation and the pages --------------------------------------------------------------------------------------------
    static const char* navText[4] = { "HOME", "EFFECTS", "PRESETS", "SETTINGS" };
    static const char* navTip[4] = { "Sound worlds, AutoMode and the main macros.", "Pick an effect and shape it.", "Browse, load and save presets; Alter Egos.", "Animations, preset folder and information." };
    for (int i = 0; i < 4; ++i)
    {
        navButtons[i].setButtonText (navText[i]);
        style (navButtons[i], navTip[i]);
        navButtons[i].onClick = [this, i] { showPage ((Page) i, true); };
    }

    for (int i = 0; i < fx::numEffects; ++i)
    {
        auto* tile = effectTiles.add (new ui::EffectTile (i, identityOf (i)));
        tile->onClick = [this, i] { selectEffect (i, false); };
        content.addAndMakeVisible (tile);

        auto* row = chainRows.add (new ui::ChainRow (i, identityOf (i)));
        row->onSelect = [this, i] { selectEffect (i, true); };
        row->onToggle = [this, i] { toggleEffect (i); };
        content.addAndMakeVisible (row);
    }
    label (fxNote, 10.0f, false, ui::textDim);
    fxNote.setText ("Top to bottom is the real processing order. It is fixed by the engine, so it cannot be reordered.", juce::dontSendNotification);
    label (effectNote, 12.0f, false, ui::textDim);
    effectNote.setMinimumHorizontalScale (0.9f);

    // HOME: Sound Worlds, macros, quick tweaks, AutoMode, input scope
    struct WorldInfo { const char* title; const char* tagline; ui::WorldCard::Icon icon; juce::Colour colour; };
    const WorldInfo worlds[FactoryPresets::count] = {
        { "ASTRO MODE", "Dark, spacious, psychedelic", ui::WorldCard::Icon::planet, ui::violet },
        { "HEARTBREAK", "Intimate, emotional, close", ui::WorldCard::Icon::heart, ui::pink },
        { "DIGITAL ALTER EGO", "Playful, futuristic, glitchy", ui::WorldCard::Icon::glitch, ui::cyan } };
    for (int i = 0; i < FactoryPresets::count; ++i)
    {
        auto* card = worldCards.add (new ui::WorldCard (worlds[i].title, worlds[i].tagline, worlds[i].icon, worlds[i].colour));
        card->setLoads (FactoryPresets::info (i).name);
        card->setTooltip (juce::String (FactoryPresets::info (i).description) + ". Loads the built-in preset; every control stays editable.");
        card->onClick = [this, i]
        {
            showStatus (presets.loadFactory (i).message);
            refreshAlterEgos();
            shownPresetText = {};
            refreshPresetBar();
            mascot.cheer();
        };
        content.addAndMakeVisible (card);
    }

    const juce::Colour macroColours[macros::numMacros] = { ui::violet, ui::lime, ui::neon };
    for (int i = 0; i < macros::numMacros; ++i)
    {
        const auto m = (macros::Macro) i;
        auto* dial = macroDials.add (new ui::MacroDial (macros::macroName (m), macroColours[i]));
        dial->setTooltip (juce::String (macros::macroDescription (m)) + " Drag up or down (Shift = fine), mouse wheel, arrow keys; double-click resets.");
        dial->onChange = [this, m] (float v)
        {
            auto s = glitchProcessor.captureSoundState();
            macros::apply (m, v, s);
            glitchProcessor.applyParameterChanges (s);
        };
        content.addAndMakeVisible (dial);
    }
    chaosDial = std::make_unique<ui::MacroDial> ("CHAOS", ui::pink);
    chaosDial->setTooltip ("How far the selected Alter Ego is pushed. Needs an Alter Ego (PRESETS > MOODS).");
    chaosDial->onChange = [this] (float v) { glitchProcessor.setChaos (v); };
    content.addAndMakeVisible (*chaosDial);

    for (int i = 0; i < macros::numCommands; ++i)
    {
        const auto c = (macros::Command) i;
        auto& b = commandButtons[i];
        b.setButtonText (macros::commandLabel (c));
        style (b, macros::commandDescription (c));
        b.onClick = [this, c]
        {
            auto s = glitchProcessor.captureSoundState();
            macros::apply (c, s);
            const int n = glitchProcessor.applyParameterChanges (s);
            showStatus (juce::String (macros::commandLabel (c)) + ": " + (n > 0 ? juce::String (n) + " controls moved." : juce::String ("already at its limit.")));
            mascot.cheer();
        };
    }
    label (talkCaption, 11.0f, true, ui::textDim);
    talkCaption.setTooltip ("Fixed buttons, not chat: each makes one bounded change you can undo with A/B.");

    style (autoAnalyzeButton, "Listen to the vocal for a few seconds (press play in your host). Nothing is changed until you press APPLY.", ui::lime);
    style (autoApplyButton, "Set the controls from the analysis, once, at the chosen intensity. Press SET A first to keep your current sound; every control stays editable.", ui::lime);
    autoAnalyzeButton.onClick = [this]
    {
        if (glitchProcessor.getAnalyzer().isListening())
            glitchProcessor.getAnalyzer().stop();
        else
        {
            glitchProcessor.getAnalyzer().start();
            autoWaiting = true;
            autoSawListening = false;
        }
        shownAutoText = {};
        refreshAutoMode();
    };
    autoApplyButton.onClick = [this]
    {
        const auto result = glitchProcessor.applyAutoMode ((float) autoIntensitySlider.getValue() / 100.0f);
        showStatus (result.message);
        if (result.usable)
            mascot.cheer();
        autoStatus.setTooltip (result.notes.joinIntoString ("\n"));
        refreshAlterEgos();
        shownPresetText = {};
        refreshPresetBar();
    };
    label (autoStyleLabel, 11.0f, true, ui::neon);
    autoStyleLabel.setTooltip ("The production direction. ASTRO MODE: dark, atmospheric melodic rap. Original settings, not an exact copy of anyone's sound.");
    autoIntensityLabel.setVisible (false); // the slider's own tooltip and value say it
    autoIntensitySlider.setSliderStyle (juce::Slider::LinearHorizontal);
    autoIntensitySlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 22);
    autoIntensitySlider.setRange (0.0, 100.0, 1.0);
    autoIntensitySlider.setTextValueSuffix (" %");
    autoIntensitySlider.setValue (glitchProcessor.getAutoIntensity() * 100.0, juce::dontSendNotification);
    autoIntensitySlider.setColour (juce::Slider::trackColourId, ui::violet);
    autoIntensitySlider.setTooltip ("INTENSITY: how strongly AUTOMODE moves the controls when you press APPLY.");
    autoIntensitySlider.onValueChange = [this] { glitchProcessor.setAutoIntensity ((float) autoIntensitySlider.getValue() / 100.0f); };
    label (autoStatus, 12.0f, false, ui::textDim);
    autoStatus.setMinimumHorizontalScale (0.85f);
    content.addAndMakeVisible (autoIntensitySlider);
    label (tuneSummary, 12.0f, true, ui::lime);
    addAll ({ &scope, &mascot });

    // PRESETS page
    style (presetSaveButton, "Save the current sound (all controls and the Alter Ego) as a named preset. Existing presets are never replaced without asking.", ui::lime);
    style (presetDeleteButton, "Delete the loaded user preset (asks first). Factory presets cannot be deleted.", ui::pink);
    style (resetButton, "Set every control back to its default (Factory Default). Asks first. Your A/B snapshots are kept.", ui::pink);
    presetSaveButton.onClick = [this] { askSavePreset(); };
    presetDeleteButton.onClick = [this] { askDeletePreset(); };
    resetButton.onClick = [this]
    {
        juce::Component::SafePointer<GlitchKidEditor> safe (this);
        askYesNo ("Reset everything?", "This sets every control to its default (the Factory Default sound). Press SET A first if you want to be able to come back to the current sound.",
                  "Reset", [safe]
                  {
                      if (safe == nullptr)
                          return;
                      safe->showStatus (safe->presets.loadFactoryDefault().message);
                      safe->refreshAlterEgos();
                      safe->shownPresetText = {};
                      safe->refreshPresetBar();
                  });
    };
    presetList.setModel (&presetListModel);
    presetList.setRowHeight (30);
    presetList.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff0b0d13));
    presetList.setColour (juce::ListBox::outlineColourId, ui::border);
    presetList.setOutlineThickness (1);
    presetList.setTooltip ("Click a preset to load it. Factory presets are built in; USER presets are yours.");
    content.addAndMakeVisible (presetList);

    // MOODS (Alter Egos): each button applies a curated identity; every control stays editable.
    label (egoTitle, 11.0f, true, ui::textDim);
    egoTitle.setText ("MOODS (ALTER EGOS)", juce::dontSendNotification);
    label (egoInfo, 11.0f, false, ui::textDim);
    for (int i = 0; i < AlterEgos::count; ++i)
    {
        auto& b = egoButtons[i];
        b.setButtonText (AlterEgos::info (i).name);
        b.setClickingTogglesState (false);
        const bool available = AlterEgos::isAvailable (i);
        style (b, available ? juce::String (AlterEgos::info (i).description)
                            : juce::String ("Unavailable: one of the effects this Alter Ego needs is not in this build."));
        b.setEnabled (available);
        b.onClick = [this, i]
        {
            const auto s = glitchProcessor.getAlterEgoSelection();
            glitchProcessor.applyAlterEgo (i, s.chaos, 0); // keeps your Chaos setting, drops any Surprise variation
            refreshAlterEgos();
        };
    }
    for (auto* l : { &chaosLabel, &blendLabel })
        label (*l, 11.0f, true, ui::textDim);
    chaosSlider.setRange (0.0, 100.0, 1.0);
    chaosSlider.setTextValueSuffix (" %");
    chaosSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18);
    chaosSlider.setColour (juce::Slider::trackColourId, ui::pink);
    chaosSlider.setColour (juce::Slider::thumbColourId, ui::pink);
    chaosSlider.setTooltip ("Intensity of the selected Alter Ego: 0 is its base sound, 100 pushes its character further (within safe limits).");
    chaosSlider.onValueChange = [this]
    {
        if (! updatingChaosFromCode)
        {
            glitchProcessor.setChaos ((float) chaosSlider.getValue() * 0.01f);
            refreshAlterEgos();
        }
    };
    blendSlider.setTextValueSuffix (" %");
    blendSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18);
    blendSlider.setColour (juce::Slider::trackColourId, ui::violet);
    blendSlider.setColour (juce::Slider::thumbColourId, ui::violet);
    blendSlider.setTooltip ("Dry original (0) to the fully processed sound (100). This is the plug-in's main Mix control.");
    style (surpriseButton, "A new seeded variation of the selected Alter Ego (small, safe changes). Click again for another.", ui::pink);
    surpriseButton.onClick = [this]
    {
        const auto s = glitchProcessor.getAlterEgoSelection();
        glitchProcessor.surprise (AlterEgos::nextSeed (s.seed ^ (std::uint32_t) juce::Time::getMillisecondCounter()));
        refreshAlterEgos();
    };
    addAll ({ &chaosSlider, &blendSlider });

    // SETTINGS page
    style (motionButton, "Switch animations on or off. Off = a calm, static interface (meters and levels still update).");
    motionButton.onClick = [this]
    {
        motionOn = ! motionOn;
        glitchProcessor.setUiFlag ("uiMotion", motionOn);
        shownCompareText = {};
    };
    style (showFolderButton, "Open the folder where your saved presets live.");
    showFolderButton.onClick = [this]
    {
        auto dir = presets.getStore().getDirectory();
        dir.createDirectory();
        dir.revealToUser();
    };
    label (settingsInfo, 12.0f, false, ui::textDim, juce::Justification::topLeft);
    settingsInfo.setMinimumHorizontalScale (1.0f);

    // ---- the effect panels (shown one at a time on the EFFECTS page) -----------------------------------------------------------------
    // Glitch Stutter
    choices (stutterRateBox, GlitchKidProcessor::stutterRateId);
    label (stutterRateLabel, 11.0f, true, ui::textDim);
    label (tempoLabel, 11.0f, false, ui::textDim);
    stutterMixKnob.add (new ParamKnob (p.apvts, GlitchKidProcessor::stutterMixId, "STUTTER MIX", " %", ui::pink));
    stutterMixKnob[0]->addTo (content);
    addAll ({ &stutterOnButton, &stutterRateBox });
    stutterOnButton.setColour (juce::ToggleButton::tickColourId, ui::lime);

    // Liquid Echo. TIME is used when SYNC is off, DIVISION when it is on (the unused one is dimmed).
    choices (echoDivBox, GlitchKidProcessor::echoDivId);
    label (echoDivLabel, 11.0f, true, ui::textDim);
    echoOnButton.setColour (juce::ToggleButton::tickColourId, ui::lime);
    echoSyncButton.setColour (juce::ToggleButton::tickColourId, ui::lime);
    echoKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::echoTimeId, "TIME", " ms", ui::cyan));
    echoKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::echoFbId, "FEEDBACK", " %", ui::cyan));
    echoKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::echoToneId, "TONE", " Hz", ui::cyan));
    echoKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::echoWidthId, "WIDTH", " %", ui::cyan));
    echoKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::echoMixId, "ECHO MIX", " %", ui::cyan));
    for (auto* k : echoKnobs)
        k->addTo (content);
    addAll ({ &echoOnButton, &echoSyncButton, &echoDivBox });

    // Pitch Tune: pulls the sung note to the nearest note of the chosen key and scale. Adds about 31 ms of latency while ON.
    choices (tuneKeyBox, GlitchKidProcessor::tuneKeyId);
    choices (tuneScaleBox, GlitchKidProcessor::tuneScaleId);
    label (tuneKeyLabel, 11.0f, true, ui::textDim);
    label (tuneScaleLabel, 11.0f, true, ui::textDim);
    label (tuneOnCaption, 11.0f, true, ui::textDim);
    label (styleCaption, 11.0f, true, ui::textDim);
    label (tuneReadout, 14.0f, true, ui::lime, juce::Justification::centredLeft);
    tuneOnButton.setColour (juce::ToggleButton::tickColourId, ui::lime);
    tuneOnButton.setTooltip ("Vocal pitch correction for a single voice (adds about 31 ms of latency while on). Sing or play a dry vocal.");
    tuneKeyBox.setTooltip ("The key (root note) of the song. GlitchKid does not detect your key.");
    tuneScaleBox.setTooltip ("Which notes the voice may be pulled to. Chromatic = any semitone.");
    tuneKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::tuneStrengthId, "STRENGTH", " %", ui::violet));
    tuneKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::tuneSpeedId, "SPEED", " ms", ui::violet));
    // FORMANT (vocal "size"): works on its own, whether or not Pitch Tune is on. Negative = bigger / darker, positive = smaller / brighter.
    tuneKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::formantShiftId, "FORMANT", " st", ui::neon));
    for (auto* k : tuneKnobs)
        k->addTo (content);
    addAll ({ &tuneOnButton, &tuneKeyBox, &tuneScaleBox });
    for (int i = 0; i < macros::numTuneStyles; ++i)
    {
        const auto s = (macros::TuneStyle) i;
        styleButtons[i].setButtonText (macros::tuneStyleLabel (s));
        style (styleButtons[i], macros::tuneStyleDescription (s));
        styleButtons[i].onClick = [this, s]
        {
            auto st = glitchProcessor.captureSoundState();
            macros::apply (s, st);
            glitchProcessor.applyParameterChanges (st);
            mascot.cheer();
        };
    }

    // Vocal Mutation: a frequency shifter / ring modulator, not a distortion. No latency.
    label (mutReadout, 13.0f, true, ui::lime, juce::Justification::centredLeft);
    mutOnButton.setColour (juce::ToggleButton::tickColourId, ui::lime);
    mutOnButton.setTooltip ("Turns the voice robotic or alien by moving every partial by a fixed number of Hz (a frequency shifter / ring modulator). No latency.");
    mutKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::mutAmountId, "MUTATION", " %", ui::pink));
    mutKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::mutCharacterId, "CHARACTER", " %", ui::pink));
    mutKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::mutBlendId, "MUTATION BLEND", " %", ui::pink));
    for (auto* k : mutKnobs)
        k->addTo (content);
    content.addAndMakeVisible (mutOnButton);

    // DOUBLER and HARMONY
    dblOnButton.setColour (juce::ToggleButton::tickColourId, ui::lime);
    harmOnButton.setColour (juce::ToggleButton::tickColourId, ui::lime);
    dblOnButton.setTooltip ("A synthetic double of the vocal: two slightly late, slightly unsteady copies for thickness and width. Not a second recorded take. No latency.");
    harmOnButton.setTooltip ("One extra voice at a fixed interval above or below the lead (prototype quality, not scale-aware). Adds about 31 ms of latency while on.");
    choices (harmIntervalBox, GlitchKidProcessor::harmIntervalId);
    harmIntervalBox.setTooltip ("How far above (UP) or below (DOWN) the lead the harmony sings, in semitones. A fixed interval: it does not know your key.");
    label (harmIntervalLabel, 11.0f, true, ui::textDim);
    label (harmNote, 11.0f, false, ui::textDim);
    dhKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::dblAmountId, "DOUBLE", " %", ui::cyan));
    dhKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::dblWidthId, "WIDTH", " %", ui::cyan));
    dhKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::dblDetuneId, "DETUNE", " cents", ui::cyan));
    dhKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::dblTimingId, "TIMING", " ms", ui::cyan));
    dhKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::harmLevelId, "HARMONY LEVEL", " dB", ui::neon));
    for (auto* k : dhKnobs)
        k->addTo (content);
    addAll ({ &dblOnButton, &harmOnButton, &harmIntervalBox });

    // Dimension
    dimOnButton.setColour (juce::ToggleButton::tickColourId, ui::lime);
    dimKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::dimWidthId, "WIDTH", " %", ui::cyan));
    dimKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::dimDetuneId, "DETUNE", " cents", ui::cyan));
    dimKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::dimDepthId, "DEPTH", " %", ui::cyan));
    dimKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::dimMixId, "DIMENSION MIX", " %", ui::cyan));
    for (auto* k : dimKnobs)
        k->addTo (content);
    content.addAndMakeVisible (dimOnButton);

    driveKnob.add (new ParamKnob (p.apvts, GlitchKidProcessor::satDriveId, "SATURATION DRIVE", " dB", ui::neon));
    driveKnob[0]->addTo (content);

    const char* bands[] = { "LOW SHELF", "BELL 1", "BELL 2", "HIGH SHELF" };
    for (int b = 0; b < ParametricEq::numBands; ++b)
    {
        bandNames[b].setText (bands[b], juce::dontSendNotification);
        label (bandNames[b], 11.0f, true, ui::neon, juce::Justification::centred);

        const auto& ids = GlitchKidProcessor::eqIds[b];
        eqKnobs[b].add (new ParamKnob (p.apvts, ids.freq, "FREQ", " Hz", ui::violet));
        eqKnobs[b].add (new ParamKnob (p.apvts, ids.gain, "GAIN", " dB", ui::violet));
        if (ids.q != nullptr)
            eqKnobs[b].add (new ParamKnob (p.apvts, ids.q, "Q", "", ui::violet));
        for (auto* k : eqKnobs[b])
            k->addTo (content);
    }

    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compThresholdId, "THRESHOLD", " dB", ui::violet));
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compRatioId, "RATIO", ":1", ui::violet));
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compMakeupId, "MAKEUP", " dB", ui::violet));
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compAttackId, "ATTACK", " ms", ui::violet));
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compReleaseId, "RELEASE", " ms", ui::violet));
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compKneeId, "KNEE", " dB", ui::violet));
    for (auto* k : compKnobs)
        k->addTo (content);
    label (grLabel, 10.0f, true, ui::textDim, juce::Justification::centred);
    content.addAndMakeVisible (grMeter);

    // ---- zone E: status strip ---------------------------------------------------------------------------------------------------------
    label (presetStatus, 11.0f, false, ui::neon, juce::Justification::centred);
    label (statusRight, 11.0f, false, ui::textDim, juce::Justification::centredRight);

    // Everything lives in `content`, which is laid out at the design size and scaled to fit the window.
    addAndMakeVisible (content);
    setResizable (true, true);
    setResizeLimits (designW * 3 / 5, designH * 3 / 5, designW * 3 / 2, designH * 3 / 2); // below ~60 % the text is too small to read
    getConstrainer()->setFixedAspectRatio ((double) designW / (double) designH);
    refreshAlterEgos();
    setSize (designW * 84 / 100, designH * 84 / 100);
    selectEffect (selectedEffect, false);
    showPage (page, false);
    timerCallback(); // fill the read-outs immediately
    startTimerHz (30);
}

GlitchKidEditor::~GlitchKidEditor()
{
    stopTimer();
    presetList.setModel (nullptr);
    setLookAndFeel (nullptr);
}

void GlitchKidEditor::refreshAlterEgos()
{
    const auto sel = glitchProcessor.getAlterEgoSelection();
    const bool active = glitchProcessor.isAlterEgoActive();

    juce::String text;
    if (active)
    {
        text = AlterEgos::info (sel.ego).description;
        if (sel.seed != 0)
            text << "   SURPRISE VARIATION #" << juce::String ((int) (sel.seed % 1000u)).paddedLeft ('0', 3);
    }
    else
    {
        text = "CUSTOM. An Alter Ego sets the EQ, compressor, saturation, echo, Dimension and Vocal Mutation; Output Gain, Blend and Stutter are never touched.";
        if (sel.ego >= 0)
            text << " (Last: " << AlterEgos::info (sel.ego).name << ", edited.)";
    }

    const auto key = text + juce::String (sel.ego) + (active ? "1" : "0") + juce::String (sel.chaos, 3);
    if (key == shownEgoText)
        return;
    shownEgoText = key;

    for (int i = 0; i < AlterEgos::count; ++i)
        egoButtons[i].setToggleState (active && i == sel.ego, juce::dontSendNotification);
    egoInfo.setText (text, juce::dontSendNotification);

    updatingChaosFromCode = true;
    chaosSlider.setValue (sel.chaos * 100.0, juce::dontSendNotification);
    updatingChaosFromCode = false;
    const bool hasEgo = sel.ego >= 0;
    chaosSlider.setEnabled (hasEgo);
    chaosLabel.setAlpha (hasEgo ? 1.0f : 0.4f);
    surpriseButton.setEnabled (hasEgo);
}

// ---- pages, selection ---------------------------------------------------------------------------------------------------------------------

void GlitchKidEditor::showPage (Page p, bool remember)
{
    page = p;
    if (remember)
        glitchProcessor.setUiInt ("uiPage", (int) page);
    for (int i = 0; i < 4; ++i)
        navButtons[i].setToggleState (i == (int) page, juce::dontSendNotification);
    if (page == Page::presets)
    {
        shownPresetText = {};
        refreshPresetBar();
    }
    layoutContent();
    repaint();
}

void GlitchKidEditor::selectEffect (int effect, bool openEffects)
{
    selectedEffect = juce::jlimit (0, glitchkid::fx::numEffects - 1, effect);
    glitchProcessor.setUiInt ("uiEffect", selectedEffect);
    for (int i = 0; i < effectTiles.size(); ++i)
    {
        effectTiles[i]->setSelected (i == selectedEffect);
        chainRows[i]->setSelected (i == selectedEffect);
    }
    effectNote.setText (juce::String (glitchkid::fx::info (selectedEffect).description) + " " + noteFor (selectedEffect), juce::dontSendNotification);
    if (openEffects && page != Page::effects)
        showPage (Page::effects, true);
    else
    {
        layoutContent();
        repaint();
    }
}

void GlitchKidEditor::toggleEffect (int effect)
{
    if (! glitchkid::fx::canToggle (effect))
        return;
    auto s = glitchProcessor.captureSoundState();
    glitchkid::fx::setEnabled (effect, ! glitchkid::fx::isActive (effect, s), s);
    glitchProcessor.applyParameterChanges (s);
}

void GlitchKidEditor::setBypassFromUi (bool bypassed)
{
    if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (glitchProcessor.apvts.getParameter (GlitchKidProcessor::bypassId)))
    {
        r->beginChangeGesture();
        r->setValueNotifyingHost (bypassed ? 1.0f : 0.0f);
        r->endChangeGesture();
    }
    shownCompareText = {};
}

// One 30 Hz tick: real input level into the scope and mascot, the dials read back from the controls, the chain and tiles from the real parameters,
// the selected Sound World from the loaded preset, the dry/processed switch from the Bypass parameter. Nothing here touches the audio thread.
void GlitchKidEditor::refreshHome (float peak)
{
    constexpr float dt = 1.0f / 30.0f;
    scope.push (peak);
    mascot.setMotion (motionOn);
    mascot.setLevel (peak);

    const auto presetName = glitchProcessor.getPresetName();
    const int world = glitchkid::FactoryPresets::indexOfName (presetName);
    const bool edited = glitchProcessor.isPresetModified();
    mascot.setLook (world < 0 ? 0.0f : (float) (world - 1));
    for (int i = 0; i < worldCards.size(); ++i)
    {
        worldCards[i]->setMotion (motionOn);
        worldCards[i]->setSelected (i == world, i == world && edited);
        worldCards[i]->tick (dt);
    }

    glitchkid::SoundState small; // just the four parameters the macros read back from
    for (const char* id : { "echo_on", "echo_mix", "sat_drive", "comp_ratio" })
        small.set (id, glitchProcessor.apvts.getRawParameterValue (id)->load());
    for (int i = 0; i < macroDials.size(); ++i)
    {
        macroDials[i]->setMotion (motionOn);
        macroDials[i]->setValue (glitchkid::macros::read ((glitchkid::macros::Macro) i, small));
        macroDials[i]->tick (dt);
    }
    const auto sel = glitchProcessor.getAlterEgoSelection();
    chaosDial->setMotion (motionOn);
    chaosDial->setEnabledLook (sel.ego >= 0, "CHAOS scales the selected Alter Ego. Pick one first: PRESETS > MOODS.");
    chaosDial->setValue (sel.chaos);
    chaosDial->tick (dt);

    autoAnalyzeButton.setMotion (motionOn);
    autoAnalyzeButton.tick (dt);
    mascot.tick (dt);

    // every effect's state, from the real parameters (the chain and the tiles always agree)
    const auto all = glitchProcessor.captureSoundState();
    for (int i = 0; i < glitchkid::fx::numEffects; ++i)
    {
        const bool active = glitchkid::fx::isActive (i, all);
        chainRows[i]->setState (active);
        effectTiles[i]->setState (active ? 1 : (glitchkid::fx::canToggle (i) ? 0 : -1));
    }

    // TUNE: the key summary and the selected tuning style are read from the real parameters (no detection is claimed)
    {
        const int style = glitchkid::macros::readTuneStyle (all);
        for (int i = 0; i < glitchkid::macros::numTuneStyles; ++i)
            styleButtons[i].setToggleState (i == style, juce::dontSendNotification);
        juce::String key, scale;
        if (auto* k = dynamic_cast<juce::AudioParameterChoice*> (glitchProcessor.apvts.getParameter (GlitchKidProcessor::tuneKeyId)))
            key = k->getCurrentChoiceName();
        if (auto* s = dynamic_cast<juce::AudioParameterChoice*> (glitchProcessor.apvts.getParameter (GlitchKidProcessor::tuneScaleId)))
            scale = s->getCurrentChoiceName();
        const bool on = all.get ("tune_on", 0.0f) >= 0.5f;
        const auto summary = on ? "KEY: " + key + " " + scale + "   STYLE: " + (style >= 0 ? juce::String (glitchkid::macros::tuneStyleLabel ((glitchkid::macros::TuneStyle) style)) : juce::String ("CUSTOM"))
                                : juce::String ("Tuning is off: your voice is not being pulled to any key.");
        if (summary != tuneSummary.getText())
            tuneSummary.setText (summary, juce::dontSendNotification);
    }

    const bool dry = glitchProcessor.apvts.getRawParameterValue (GlitchKidProcessor::bypassId)->load() >= 0.5f;
    const auto compareText = juce::String (dry ? "D" : "P") + (motionOn ? "m" : "s");
    if (compareText != shownCompareText)
    {
        shownCompareText = compareText;
        dryButton.setToggleState (dry, juce::dontSendNotification);
        processedButton.setToggleState (! dry, juce::dontSendNotification);
        motionButton.setButtonText (motionOn ? "ANIMATIONS: ON" : "ANIMATIONS: OFF");
    }
}

void GlitchKidEditor::showStatus (const juce::String& text)
{
    presetStatus.setText (text, juce::dontSendNotification);
    statusTicks = 30 * 5; // about five seconds
}

void GlitchKidEditor::refreshAutoMode()
{
    auto& an = glitchProcessor.getAnalyzer();
    const bool listening = an.isListening();
    if (autoWaiting && listening)
        autoSawListening = true;
    if (autoWaiting && autoSawListening && ! listening)
        autoWaiting = false;

    using S = glitchkid::analysis::VocalAnalysis::Status;
    juce::String text;
    bool canApply = false;
    auto ring = glitchkid::ui::AutoState::idle;
    float ringProgress = 0.0f;
    auto mood = glitchkid::ui::Mascot::Mood::idle;
    if (listening)
    {
        ring = glitchkid::ui::AutoState::working;
        ringProgress = an.liveActiveSeconds() / glitchkid::analysis::VocalAnalyzer::maxActiveSeconds;
        mood = glitchkid::ui::Mascot::Mood::listening;
    }
    else if (autoWaiting)
        ring = glitchkid::ui::AutoState::waiting;
    if (listening)
        text = "Listening... " + juce::String (an.liveActiveSeconds(), 1) + " s of vocal heard (needs 2 s, stops at 8 s). Press STOP to finish early.";
    else if (autoWaiting)
        text = "Waiting for audio. Press play in your host so the vocal reaches GlitchKid.";
    else
    {
        const auto a = an.fetch();
        switch (a.status)
        {
            case S::ready:
                canApply = true;
                ring = glitchkid::ui::AutoState::ready;
                mood = glitchkid::ui::Mascot::Mood::ready;
                text = "Ready: " + juce::String (a.activeSeconds, 1) + " s of vocal, peak " + juce::String (a.peakDb, 1) + " dB, level " + juce::String (a.rmsDb, 1)
                       + " dB" + (a.clippedSamples >= 3 ? ", CLIPPING found" : "") + ". Set INTENSITY, then APPLY.";
                break;
            case S::tooShort: ring = glitchkid::ui::AutoState::error; mood = glitchkid::ui::Mascot::Mood::error;  text = "Too short: only " + juce::String (a.activeSeconds, 1) + " s of vocal heard (needs 2 s). Nothing changed. Try again."; break;
            case S::tooQuiet: ring = glitchkid::ui::AutoState::error; mood = glitchkid::ui::Mascot::Mood::error;  text = "No usable vocal heard (silent or very quiet input). Nothing changed."; break;
            case S::invalid: ring = glitchkid::ui::AutoState::error; mood = glitchkid::ui::Mascot::Mood::error;   text = "The input contained invalid audio. Nothing changed. Try again."; break;
            case S::idle:
            case S::listening: text = "Press ANALYZE, then play the dry vocal for a few seconds."; break;
        }
    }
    autoAnalyzeButton.setState (ring, ringProgress);
    mascot.setMood (mood);
    if (text != shownAutoText)
    {
        shownAutoText = text;
        autoStatus.setText (text, juce::dontSendNotification);
        autoAnalyzeButton.setButtonText (listening ? "STOP" : "ANALYZE VOCAL");
        autoApplyButton.setEnabled (canApply);
    }
}

void GlitchKidEditor::refreshPresetList()
{
    auto names = presets.allPresetNames();
    std::vector<int> kinds;
    for (const auto& n : names)
        kinds.push_back (presets.kindOf (n) == glitchkid::PresetController::Kind::user ? 1 : 0);
    if (names != presetNames || kinds != presetKinds)
    {
        presetNames = names;
        presetKinds = kinds;
        presetList.updateContent();
    }
    const int row = presetNames.indexOf (glitchProcessor.getPresetName(), true);
    if (row >= 0)
        presetList.selectRow (row, true, true);
    else
        presetList.deselectAllRows();
    presetList.repaint();
}

void GlitchKidEditor::refreshPresetBar()
{
    const auto name = glitchProcessor.getPresetName();
    const auto kind = presets.kindOf (name);
    const auto tag = kind == glitchkid::PresetController::Kind::factory ? "FACTORY:  " : kind == glitchkid::PresetController::Kind::user ? "USER:  " : "PRESET:  ";
    const auto presetText = tag + (name.isEmpty() ? juce::String ("(none)") : name) + (glitchProcessor.isPresetModified() ? "  *" : "");
    if (presetText != shownPresetText)
    {
        shownPresetText = presetText;
        presetMenuButton.setButtonText (presetText);
        presetDeleteButton.setEnabled (presets.isUserPreset (name)); // looks at the preset folder, so only when the name changed or after an action
        refreshPresetList();
    }

    const int active = glitchProcessor.getAbActive();
    const bool hasA = glitchProcessor.hasAbSlot (0), hasB = glitchProcessor.hasAbSlot (1);
    const bool edited = glitchProcessor.isAbSlotModified();
    const auto abText = juce::String (active) + (hasA ? "a" : "-") + (hasB ? "b" : "-") + (edited ? "e" : "-");
    if (abText != shownAbText)
    {
        shownAbText = abText;
        abRecallA.setEnabled (hasA);
        abRecallB.setEnabled (hasB);
        abRecallA.setToggleState (active == 0, juce::dontSendNotification);
        abRecallB.setToggleState (active == 1, juce::dontSendNotification);
        abRecallA.setButtonText (active == 0 && edited ? "A *" : "A");
        abRecallB.setButtonText (active == 1 && edited ? "B *" : "B");
        abCopyAB.setEnabled (hasA);
        abCopyBA.setEnabled (hasB);
        abRecallA.setTooltip (hasA ? "Switch to the sound stored in A." : "Slot A is empty: press SET A to capture the current sound.");
        abRecallB.setTooltip (hasB ? "Switch to the sound stored in B." : "Slot B is empty: press SET B to capture the current sound.");
    }
}

void GlitchKidEditor::openPresetMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("FACTORY");
    menu.addItem (1, glitchkid::PresetController::factoryDefaultName);
    for (int i = 0; i < glitchkid::FactoryPresets::count; ++i)
        menu.addItem (2 + i, glitchkid::FactoryPresets::info (i).name, true,
                      juce::String (glitchkid::FactoryPresets::info (i).name).equalsIgnoreCase (glitchProcessor.getPresetName()));
    menu.addSeparator();
    menu.addSectionHeader ("YOUR PRESETS");
    const auto names = presets.getStore().listUserPresets();
    if (names.isEmpty())
        menu.addItem (-1, "(none saved yet: see PRESETS > SAVE PRESET)", false);
    for (int i = 0; i < names.size(); ++i)
        menu.addItem (100 + i, names[i], true, names[i].equalsIgnoreCase (glitchProcessor.getPresetName()));

    juce::Component::SafePointer<GlitchKidEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetMenuButton), [safe, names] (int choice)
    {
        if (safe == nullptr || choice <= 0)
            return;
        const auto outcome = choice == 1 ? safe->presets.loadFactoryDefault()
                           : choice < 100 ? safe->presets.loadFactory (choice - 2)
                                          : safe->presets.loadUser (names[choice - 100]);
        safe->showStatus (outcome.message);
        safe->refreshAlterEgos();
        safe->shownPresetText = {};
        safe->refreshPresetBar();
    });
}

void GlitchKidEditor::askSavePreset()
{
    const auto current = glitchProcessor.getPresetName();
    auto* w = new juce::AlertWindow ("Save preset", "Name this sound (letters, numbers and spaces work best). Built-in presets are never changed: your version is saved as a new preset.", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", glitchkid::PresetController::variationName (current), "Name:");
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<GlitchKidEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (result == 1 && safe != nullptr)
            safe->trySave (w->getTextEditorContents ("name"), false);
    }), true);
}

void GlitchKidEditor::trySave (const juce::String& name, bool overwrite)
{
    const auto outcome = presets.saveAs (name, overwrite);
    if (outcome.needsOverwriteConfirmation)
    {
        juce::Component::SafePointer<GlitchKidEditor> safe (this);
        askYesNo ("Replace preset?", "A preset called \"" + outcome.name + "\" already exists. Replace it with the current sound?", "Replace",
                  [safe, name] { if (safe != nullptr) safe->trySave (name, true); });
        return;
    }
    showStatus (outcome.message);
    shownPresetText = {};
    refreshPresetBar();
}

void GlitchKidEditor::askDeletePreset()
{
    const auto name = glitchProcessor.getPresetName();
    if (! presets.isUserPreset (name))
        return;
    juce::Component::SafePointer<GlitchKidEditor> safe (this);
    askYesNo ("Delete preset?", "Delete \"" + name + "\"? This cannot be undone. Your current sound is not changed.", "Delete",
             [safe, name]
             {
                 if (safe == nullptr)
                     return;
                 safe->showStatus (safe->presets.deleteUser (name).message);
                 safe->shownPresetText = {};
                 safe->refreshPresetBar();
             });
}

void GlitchKidEditor::askYesNo (const juce::String& heading, const juce::String& message, const juce::String& yesText, std::function<void()> onYes)
{
    auto* w = new juce::AlertWindow (heading, message, juce::MessageBoxIconType::QuestionIcon, this);
    w->addButton (yesText, 1);
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey)); // the safe choice is the one the Escape key picks
    w->enterModalState (true, juce::ModalCallbackFunction::create ([onYes] (int result)
    {
        if (result == 1)
            onYes();
    }), true);
}

void GlitchKidEditor::timerCallback()
{
    refreshAlterEgos();
    refreshPresetBar();
    refreshAutoMode();
    if (statusTicks > 0 && --statusTicks == 0)
        presetStatus.setText ({}, juce::dontSendNotification);
    const float inPeak = glitchProcessor.consumeInputPeak();
    const float outPeak = glitchProcessor.consumeOutputPeak();
    inMeter.pushPeak (inPeak);
    outMeter.pushPeak (outPeak);
    inActivity.push (inPeak);
    outActivity.push (outPeak);
    clipLed.push (juce::jmax (inPeak, outPeak));
    refreshHome (inPeak);
    grMeter.pushReduction (glitchProcessor.consumeGainReductionDb());

    const bool synced = glitchProcessor.apvts.getRawParameterValue (GlitchKidProcessor::echoSyncId)->load() >= 0.5f;
    echoKnobs[0]->setEnabled (! synced);
    echoDivBox.setEnabled (synced);
    echoDivLabel.setAlpha (synced ? 1.0f : 0.4f);

    // Pitch Tune read-out: the sung note, how far off it is, and the note it is pulled to.
    juce::String tuneText; // empty while off: the summary above already says so
    if (glitchProcessor.apvts.getRawParameterValue (GlitchKidProcessor::tuneOnId)->load() >= 0.5f)
    {
        const float sung = glitchProcessor.getTuneDetectedMidi(), target = glitchProcessor.getTuneTargetMidi();
        if (sung < 0.0f || target < 0.0f)
            tuneText = "listening...";
        else
        {
            const int cents = (int) std::lround ((sung - std::round (sung)) * 100.0f);
            tuneText = "SUNG " + noteName (sung) + (cents >= 0 ? " +" : " ") + juce::String (cents) + "c   ->   " + noteName (target);
        }
    }
    if (tuneText != shownTuneText)
    {
        shownTuneText = tuneText;
        tuneReadout.setText (tuneText, juce::dontSendNotification);
    }

    // Vocal Mutation read-out: computed from the same mapping the DSP uses, so it always matches what is heard.
    juce::String mutText; // empty while off: the switch says so
    if (glitchProcessor.apvts.getRawParameterValue (GlitchKidProcessor::mutOnId)->load() >= 0.5f)
    {
        const float amount = glitchProcessor.apvts.getRawParameterValue (GlitchKidProcessor::mutAmountId)->load();
        const float character = glitchProcessor.apvts.getRawParameterValue (GlitchKidProcessor::mutCharacterId)->load();
        const double hz = VocalMutation::carrierHzFor (amount);
        if (hz <= 0.0)
            mutText = "no change";
        else
        {
            const char* kind = character <= 15.0f ? "DOWN" : character < 45.0f ? "DOWN ~ ROBOT" : character <= 55.0f ? "ROBOT"
                               : character < 85.0f ? "ROBOT ~ UP" : "UP";
            mutText = juce::String (kind) + "   " + (hz < 10.0 ? juce::String (hz, 1) : juce::String (juce::roundToInt (hz))) + " Hz";
        }
    }
    if (mutText != shownMutText)
    {
        shownMutText = mutText;
        mutReadout.setText (mutText, juce::dontSendNotification);
    }

    // Harmony note: be honest about the delay it adds.
    juce::String harmText;
    {
        const double sr = glitchProcessor.getSampleRate() > 0.0 ? glitchProcessor.getSampleRate() : 48000.0;
        const double ms = 1000.0 * (double) glitchkid::harmony::HarmonyVoice::latencyFor (sr) / sr;
        const bool on = glitchProcessor.apvts.getRawParameterValue (GlitchKidProcessor::harmOnId)->load() >= 0.5f;
        const auto msText = juce::String (juce::roundToInt (ms));
        harmText = on ? "Adds " + msText + " ms delay while on. A fixed interval: it does not know your key." : "Adds " + msText + " ms delay when on.";
    }
    if (harmText != shownHarmNote)
    {
        shownHarmNote = harmText;
        harmNote.setText (harmText, juce::dontSendNotification);
    }

    // Always say where the stutter tempo comes from: the host, or the 120 BPM fallback.
    const auto text = glitchProcessor.isStutterTempoFromHost()
                          ? "HOST TEMPO " + juce::String (glitchProcessor.getStutterTempoBpm(), 1) + " BPM"
                          : juce::String ("NO HOST TEMPO: 120 BPM");
    if (text != shownTempoText)
    {
        shownTempoText = text;
        tempoLabel.setText (text, juce::dontSendNotification);
    }

    // status strip (right) and the SETTINGS information
    const double sr = glitchProcessor.getSampleRate();
    const double latencyMs = sr > 0.0 ? 1000.0 * (double) glitchProcessor.getLatencySamples() / sr : 0.0;
    const bool dry = glitchProcessor.apvts.getRawParameterValue (GlitchKidProcessor::bypassId)->load() >= 0.5f;
    const auto right = juce::String (dry ? "DRY (BYPASSED)" : "PROCESSING") + "   |   latency " + juce::String (latencyMs, 1) + " ms   |   v" + GLITCHKID_VERSION;
    if (right != statusRight.getText())
        statusRight.setText (right, juce::dontSendNotification);

    const auto info = juce::String ("GlitchKid v") + GLITCHKID_VERSION + "\n\n"
                      + "Sample rate: " + (sr > 0.0 ? juce::String (sr, 0) + " Hz" : juce::String ("not running")) + "\n"
                      + "Reported latency: " + juce::String (latencyMs, 1) + " ms (Pitch Tune, Harmony and Formant add delay only while they are in use)\n"
                      + "Processing: everything runs locally on your computer.\n\n"
                      + "Saved presets: " + presets.getStore().getDirectory().getFullPathName() + "\n\n"
                      + "The order of the effects is fixed by the engine, so the FX CHAIN shows the real order but cannot reorder it.\n"
                      + "GlitchKid has no input-gain stage; the gain knob on the left is the output gain.";
    if (info != settingsInfo.getText())
        settingsInfo.setText (info, juce::dontSendNotification);
}

// ---- painting ---------------------------------------------------------------------------------------------------------------------------

void GlitchKidEditor::paint (juce::Graphics& g)
{
    constexpr float W = (float) designW;
    g.fillAll (ui::obsidian);
    g.addTransform (juce::AffineTransform::scale (scaleFactor));

    auto zone = [&g] (juce::Rectangle<float> r, bool raised)
    {
        g.setColour (raised ? ui::panelTop : ui::panelBottom);
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (ui::border);
        g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);
    };
    zone ({ 12.0f, 10.0f, W - 24.0f, 54.0f }, false);   // A: top bar
    zone ({ 12.0f, 76.0f, 124.0f, 592.0f }, false);     // B: input / output
    zone ({ 148.0f, 76.0f, 772.0f, 592.0f }, false);    // C: workspace
    zone ({ 932.0f, 76.0f, 216.0f, 592.0f }, false);    // D: FX CHAIN
    zone ({ 12.0f, 676.0f, W - 24.0f, 34.0f }, false);  // E: status strip

    // B: a quiet purple ring of light behind the meters
    {
        juce::ColourGradient glow (ui::violet.withAlpha (0.14f), 74.0f, 260.0f, ui::violet.withAlpha (0.0f), 74.0f, 130.0f, true);
        g.setGradientFill (glow);
        g.fillRoundedRectangle (14.0f, 78.0f, 120.0f, 588.0f, 12.0f);
    }

    // A: crown and wordmark (a slanted, lightly "tagged" logotype with a lime drip underline)
    ui::drawCrown (g, { 24.0f, 16.0f, 30.0f, 26.0f }, ui::neon);
    {
        const juce::Graphics::ScopedSaveState state (g);
        g.addTransform (juce::AffineTransform::shear (-0.2f, 0.0f).translated (0.2f * 28.0f, 0.0f));
        juce::AttributedString word;
        word.append ("GLITCH", ui::fontOf (27.0f, true), ui::textBright);
        word.append ("KID", ui::fontOf (27.0f, true), ui::neon);
        word.setJustification (juce::Justification::centredLeft);
        word.draw (g, juce::Rectangle<float> (60.0f, 12.0f, 200.0f, 32.0f));
    }
    g.setColour (ui::lime);
    g.fillRoundedRectangle (62.0f, 46.0f, 128.0f, 2.5f, 1.2f);
    for (const float x : { 76.0f, 118.0f, 160.0f })
        g.fillRoundedRectangle (x, 46.0f, 2.5f, 6.0f + (x > 100.0f ? 2.0f : 0.0f), 1.2f);

    g.setColour (ui::textDim);
    g.setFont (ui::fontOf (10.0f, true));
    g.drawText ("IN", 896, 18, 28, 12, juce::Justification::centredRight, false);
    g.drawText ("OUT", 896, 34, 28, 12, juce::Justification::centredRight, false);

    // B
    g.drawText ("MONITOR", 20, 552, 100, 12, juce::Justification::centred, false);

    // D
    g.setFont (ui::fontOf (12.0f, true));
    g.drawText ("FX CHAIN", 946, 84, 120, 18, juce::Justification::centredLeft, false);

    // E
    g.setColour (ui::textDim);
    g.setFont (ui::fontOf (11.0f, true));
    g.drawText ("MAKE YOUR VOICE WEIRD.", 62, 679, 220, 28, juce::Justification::centredLeft, false);

    // C: the raised plates behind the current page's controls
    for (const auto& r : panelRects)
    {
        const auto f = r.toFloat();
        g.setColour (ui::panelTop);
        g.fillRoundedRectangle (f, 10.0f);
        g.setColour (ui::border);
        g.drawRoundedRectangle (f.reduced (0.5f), 10.0f, 1.0f);
    }
    if (page == Page::effects)
    {
        const auto col = identityOf (selectedEffect);
        g.setColour (col);
        g.fillRoundedRectangle (164.0f, 274.0f, 4.0f, 22.0f, 2.0f);
        g.setColour (ui::textBright);
        g.setFont (ui::fontOf (18.0f, true));
        g.drawText (glitchkid::fx::info (selectedEffect).name, 176, 272, 560, 26, juce::Justification::centredLeft, false);
    }
}

void GlitchKidEditor::resized()
{
    scaleFactor = juce::jmin ((float) getWidth() / (float) designW, (float) getHeight() / (float) designH);
    content.setTransform (juce::AffineTransform::scale (scaleFactor));
    content.setBounds (0, 0, designW, designH);
    repaint();
}

void GlitchKidEditor::layoutContent()
{
    namespace fx = glitchkid::fx;
    using Rect = juce::Rectangle<int>;
    auto show = [] (juce::Component& c, Rect r) { c.setBounds (r); c.setVisible (true); };
    auto showK = [] (ParamKnob& k, Rect r) { k.setBounds (r); k.setVisible (true); };

    // Hide every control that is not part of the always-visible frame; the current page then shows what it needs.
    std::vector<juce::Component*> persistent = { &mascot, &gearButton, &inActivity, &outActivity, &clipLed, &inMeter, &outMeter, &inLabel, &outLabel, &inputCaption,
                                                 &dryButton, &processedButton, &presetPrevButton, &presetMenuButton, &presetNextButton, &abSetA, &abSetB,
                                                 &abRecallA, &abRecallB, &abLabel, &presetStatus, &statusRight, &fxNote };
    for (auto& n : navButtons)
        persistent.push_back (&n);
    for (auto* r : chainRows)
        persistent.push_back (r);
    for (auto* child : content.getChildren())
        if (std::find (persistent.begin(), persistent.end(), child) == persistent.end())
            child->setVisible (false);
    panelRects.clear();

    // ---- A: top bar ----
    show (presetPrevButton, { 270, 18, 30, 34 });
    show (presetMenuButton, { 304, 18, 260, 34 });
    show (presetNextButton, { 568, 18, 30, 34 });
    show (abLabel, { 606, 18, 34, 34 });
    show (abSetA, { 644, 18, 54, 34 });
    show (abRecallA, { 702, 18, 40, 34 });
    show (abRecallB, { 746, 18, 40, 34 });
    show (abSetB, { 790, 18, 54, 34 });
    show (inActivity, { 930, 19, 76, 10 });
    show (outActivity, { 930, 35, 76, 10 });
    show (gearButton, { 1098, 16, 38, 38 });

    // ---- B: input / output ----
    show (inputCaption, { 22, 84, 76, 18 });
    show (clipLed, { 104, 85, 16, 16 });
    show (inMeter, { 30, 112, 26, 290 });
    show (outMeter, { 82, 112, 26, 290 });
    show (inLabel, { 22, 404, 42, 14 });
    show (outLabel, { 74, 404, 42, 14 });
    showK (*gainKnob[0], { 20, 424, 100, 124 });
    show (dryButton, { 20, 568, 100, 38 });
    show (processedButton, { 20, 612, 100, 38 });

    // ---- C: workspace navigation ----
    for (int i = 0; i < 4; ++i)
    {
        show (navButtons[i], { 160 + i * 126, 84, 120, 34 });
        navButtons[i].setToggleState (i == (int) page, juce::dontSendNotification);
    }

    // ---- D: FX CHAIN ----
    for (int i = 0; i < chainRows.size(); ++i)
        show (*chainRows[i], { 940, 108 + i * 46, 200, 44 });
    show (fxNote, { 944, 616, 196, 46 });

    // ---- E: status strip ----
    show (mascot, { 20, 678, 34, 30 });
    show (presetStatus, { 300, 681, 480, 24 });
    show (statusRight, { 760, 681, 376, 24 });

    switch (page)
    {
        case Page::home:
        {
            panelRects.push_back ({ 402, 128, 244, 400 });
            panelRects.push_back ({ 652, 128, 264, 400 });
            panelRects.push_back ({ 152, 408, 246, 120 });
            panelRects.push_back ({ 152, 534, 764, 124 });
            for (int i = 0; i < worldCards.size(); ++i)
                show (*worldCards[i], { 160, 134 + i * 92, 236, 84 });
            show (talkCaption, { 164, 414, 228, 18 });
            talkCaption.setText ("QUICK TWEAKS", juce::dontSendNotification);
            for (int i = 0; i < glitchkid::macros::numCommands; ++i)
                show (commandButtons[i], { 164 + (i % 2) * 116, 436 + (i / 2) * 40, 112, 36 });

            show (autoStyleLabel, { 414, 136, 230, 16 });
            show (autoStatus, { 414, 156, 226, 62 });
            show (autoAnalyzeButton, { 440, 222, 174, 174 });
            show (autoIntensitySlider, { 414, 404, 226, 26 });
            show (autoApplyButton, { 414, 438, 226, 40 });
            for (int i = 0; i < macroDials.size(); ++i)
                show (*macroDials[i], { 660 + (i % 2) * 126, 134 + (i / 2) * 188, 122, 178 });
            show (*chaosDial, { 660 + 126, 134 + 188, 122, 178 });
            show (tuneSummary, { 166, 540, 736, 20 });
            show (scope, { 166, 564, 736, 88 });
            break;
        }

        case Page::effects:
        {
            for (int i = 0; i < effectTiles.size(); ++i)
                show (*effectTiles[i], { 160 + (i % 6) * 124, 128 + (i / 6) * 66, 118, 60 });
            panelRects.push_back ({ 148, 262, 772, 402 });
            show (effectNote, { 164, 300, 740, 34 });
            constexpr int x0 = 164, top = 344;

            switch (selectedEffect)
            {
                case fx::pitchTune:
                    show (tuneOnCaption, { x0, top, 90, 14 });
                    tuneOnCaption.setText ("TUNING", juce::dontSendNotification);
                    show (tuneOnButton, { x0, top + 18, 90, 34 });
                    show (tuneKeyLabel, { x0 + 106, top, 120, 14 });
                    tuneKeyLabel.setText ("KEY", juce::dontSendNotification);
                    show (tuneKeyBox, { x0 + 106, top + 18, 110, 34 });
                    show (tuneScaleLabel, { x0 + 232, top, 120, 14 });
                    tuneScaleLabel.setText ("SCALE", juce::dontSendNotification);
                    show (tuneScaleBox, { x0 + 232, top + 18, 190, 34 });
                    show (styleCaption, { x0 + 440, top, 200, 14 });
                    styleCaption.setText ("TUNING STYLE", juce::dontSendNotification);
                    for (int i = 0; i < glitchkid::macros::numTuneStyles; ++i)
                        show (styleButtons[i], { x0 + 440 + i * 100, top + 18, 96, 34 });
                    showK (*tuneKnobs[0], { x0, top + 76, 130, 150 });
                    showK (*tuneKnobs[1], { x0 + 150, top + 76, 130, 150 });
                    show (tuneSummary, { x0 + 320, top + 100, 420, 22 });
                    show (tuneReadout, { x0 + 320, top + 134, 420, 26 });
                    break;

                case fx::formant:
                    showK (*tuneKnobs[2], { x0, top + 10, 160, 190 });
                    break;

                case fx::harmony:
                    show (harmOnButton, { x0, top, 130, 34 });
                    show (harmIntervalLabel, { x0 + 150, top, 170, 14 });
                    harmIntervalLabel.setText ("INTERVAL", juce::dontSendNotification);
                    show (harmIntervalBox, { x0 + 150, top + 18, 190, 34 });
                    showK (*dhKnobs[4], { x0 + 370, top - 6, 140, 150 });
                    show (harmNote, { x0, top + 76, 340, 40 });
                    break;

                case fx::mutation:
                    show (mutOnButton, { x0, top, 100, 34 });
                    show (mutReadout, { x0, top + 52, 220, 26 });
                    for (int i = 0; i < mutKnobs.size(); ++i)
                        showK (*mutKnobs[i], { x0 + 240 + i * 150, top - 6, 140, 150 });
                    break;

                case fx::doubler:
                    show (dblOnButton, { x0, top, 130, 34 });
                    for (int i = 0; i < 4; ++i)
                        showK (*dhKnobs[i], { x0 + 150 + i * 145, top - 6, 135, 150 });
                    break;

                case fx::eq:
                    for (int b = 0; b < ParametricEq::numBands; ++b)
                    {
                        show (bandNames[b], { x0 + b * 185, top - 8, 175, 16 });
                        for (int k = 0; k < eqKnobs[b].size(); ++k)
                            showK (*eqKnobs[b][k], { x0 + b * 185 + 20, top + 12 + k * 98, 135, 96 });
                    }
                    break;

                case fx::compressor:
                    for (int i = 0; i < compKnobs.size(); ++i)
                        showK (*compKnobs[i], { x0 + (i % 3) * 150, top - 6 + (i / 3) * 152, 140, 146 });
                    show (grMeter, { x0 + 500, top, 28, 280 });
                    show (grLabel, { x0 + 494, top + 284, 40, 14 });
                    grLabel.setText ("GAIN RED.", juce::dontSendNotification);
                    break;

                case fx::saturation:
                    showK (*driveKnob[0], { x0, top + 10, 180, 200 });
                    break;

                case fx::stutter:
                    show (stutterOnButton, { x0, top, 90, 34 });
                    show (stutterRateLabel, { x0 + 110, top, 110, 14 });
                    stutterRateLabel.setText ("RATE", juce::dontSendNotification);
                    show (stutterRateBox, { x0 + 110, top + 18, 130, 34 });
                    show (tempoLabel, { x0, top + 70, 260, 20 });
                    showK (*stutterMixKnob[0], { x0 + 290, top - 6, 160, 150 });
                    break;

                case fx::echo:
                    show (echoOnButton, { x0, top, 90, 34 });
                    show (echoSyncButton, { x0, top + 42, 90, 34 });
                    show (echoDivLabel, { x0 + 110, top, 140, 14 });
                    echoDivLabel.setText ("DIVISION", juce::dontSendNotification);
                    show (echoDivBox, { x0 + 110, top + 18, 150, 34 });
                    for (int i = 0; i < echoKnobs.size(); ++i)
                        showK (*echoKnobs[i], { x0 + i * 148, top + 100, 140, 150 });
                    break;

                case fx::dimension:
                    show (dimOnButton, { x0, top, 100, 34 });
                    for (int i = 0; i < dimKnobs.size(); ++i)
                        showK (*dimKnobs[i], { x0 + 130 + i * 148, top - 6, 140, 150 });
                    break;

                default: break;
            }
            break;
        }

        case Page::presets:
        {
            panelRects.push_back ({ 152, 128, 342, 432 });
            panelRects.push_back ({ 502, 128, 414, 432 });
            show (presetList, { 160, 136, 326, 416 });
            show (presetSaveButton, { 516, 140, 190, 40 });
            show (presetDeleteButton, { 714, 140, 194, 40 });
            show (resetButton, { 516, 188, 190, 40 });
            show (abCopyAB, { 714, 188, 94, 40 });
            show (abCopyBA, { 814, 188, 94, 40 });
            show (egoTitle, { 516, 246, 300, 18 });
            for (int i = 0; i < AlterEgos::count; ++i)
                show (egoButtons[i], { 516 + (i % 3) * 132, 270 + (i / 3) * 38, 126, 34 });
            show (egoInfo, { 516, 354, 392, 56 });
            show (chaosLabel, { 516, 424, 60, 26 });
            show (chaosSlider, { 580, 424, 328, 26 });
            show (blendLabel, { 516, 458, 60, 26 });
            show (blendSlider, { 580, 458, 328, 26 });
            show (surpriseButton, { 516, 498, 190, 38 });
            break;
        }

        case Page::settings:
        {
            panelRects.push_back ({ 152, 128, 764, 260 });
            show (motionButton, { 164, 140, 250, 40 });
            show (showFolderButton, { 426, 140, 250, 40 });
            show (settingsInfo, { 168, 196, 736, 184 });
            break;
        }
    }
}
