#include "PluginEditor.h"
#include "DSP/PitchCorrection/ScaleQuantizer.h"
#include "AutoMode/SourceRelationship.h"
#include "UI/EffectModel.h"

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
                      const juce::String& suffix, juce::Colour colour, const juce::String& tooltip)
    : attachment (apvts, paramId, slider)
{
    slider.setTextValueSuffix (suffix);
    slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 16);
    slider.setDoubleClickReturnValue (true, apvts.getParameter (paramId)->convertFrom0to1 (apvts.getParameter (paramId)->getDefaultValue())); // double-click resets
    slider.setTooltip (tooltip);
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


GlitchKidEditor::GlitchKidEditor (GlitchKidProcessor& p)
    : AudioProcessorEditor (&p),
      glitchProcessor (p),
      bypassAttachment (p.apvts, GlitchKidProcessor::bypassId, bypassButton),
      echoOnAttachment (p.apvts, GlitchKidProcessor::echoOnId, echoOnButton),
      echoSyncAttachment (p.apvts, GlitchKidProcessor::echoSyncId, echoSyncButton),
      echoDivAttachment (p.apvts, GlitchKidProcessor::echoDivId, echoDivBox),
      dimOnAttachment (p.apvts, GlitchKidProcessor::dimOnId, dimOnButton),
      tuneOnAttachment (p.apvts, GlitchKidProcessor::tuneOnId, tuneOnButton),
      dblOnAttachment (p.apvts, GlitchKidProcessor::dblOnId, dblOnButton),
      tuneKeyAttachment (p.apvts, GlitchKidProcessor::tuneKeyId, tuneKeyBox),
      tuneScaleAttachment (p.apvts, GlitchKidProcessor::tuneScaleId, tuneScaleBox),
      presets (p)
{
    using namespace glitchkid;
    setLookAndFeel (&lookAndFeel);
    motionOn = p.getUiFlag ("uiMotion", true);
    page = (Page) juce::jlimit (0, 2, p.getUiInt ("uiPage", 0)); // first launch: TUNE
    category = juce::jlimit (0, numCategories - 1, p.getUiInt ("uiCategory", 0));
    browsedProfile = p.getUiInt ("uiProfile", -1);
    if (browsedProfile < 0 || browsedProfile >= FactoryPresets::count)
        browsedProfile = -1;

    auto label = [this] (juce::Label& l, float size, bool bold, juce::Colour c, juce::Justification j = juce::Justification::centredLeft)
    {
        l.setFont (ui::fontOf (size, bold));
        l.setColour (juce::Label::textColourId, c);
        l.setJustificationType (j);
        l.setMinimumHorizontalScale (1.0f); // wrap long text instead of squeezing it
        content.addAndMakeVisible (l);
    };
    // A flat graphite button: violet when selected. Lime is only for the primary action.
    auto style = [this] (juce::TextButton& b, const juce::String& tip, juce::Colour text = ui::textBright)
    {
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1a1d27));
        b.setColour (juce::TextButton::buttonOnColourId, ui::violet);
        b.setColour (juce::TextButton::textColourOffId, text);
        b.setColour (juce::TextButton::textColourOnId, ui::chrome);
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

    // ---- header: name, workspaces, dry / processed, motion ----
    label (brandLabel, 19.0f, true, ui::textBright);
    label (brandSub, 11.0f, true, ui::violet);
    static const char* navText[3] = { "TUNE", "AUTO", "MODIFY" };
    static const char* navTip[3] = { "Set the key and scale of your beat and how firmly the voice is pulled to it.", "Pick a starting profile, then press BUILD MY VOCAL MIX.",
                                     "Refine the result: pitch, tone, dynamics and space." };
    for (int i = 0; i < 3; ++i)
    {
        navButtons[i].setButtonText (navText[i]);
        style (navButtons[i], navTip[i]);
        navButtons[i].onClick = [this, i] { showPage ((Page) i, true); };
    }
    style (motionButton, "Switch animations on or off. Off = a still interface (meters and levels still update).");
    motionButton.onClick = [this]
    {
        motionOn = ! motionOn;
        glitchProcessor.setUiFlag ("uiMotion", motionOn);
        shownCompareText = {};
    };
    style (dryButton, "Hear the untouched input. This is the real bypass (delay-matched and crossfaded over 20 ms), not the Mix knob.");
    style (processedButton, "Hear the processed sound.");
    dryButton.setColour (juce::TextButton::buttonOnColourId, ui::pink.darker (0.5f));
    processedButton.setColour (juce::TextButton::buttonOnColourId, ui::lime);
    processedButton.setColour (juce::TextButton::textColourOnId, ui::obsidian);
    dryButton.onClick = [this] { setBypassFromUi (true); };
    processedButton.onClick = [this] { setBypassFromUi (false); };
    content.addAndMakeVisible (chain);
    chain.onStage = [this] (int stage)
    {
        showCategory (stage, true);
        showPage (Page::modify, true);
    };

    // ---- the preset bar and A/B (always available) ----
    style (presetMenuButton, "Choose the clean default, one of the starting profiles or one of your saved presets.");
    style (presetPrevButton, "Previous preset (starting profiles first, then yours). Replaces the current sound: press SET A first to keep it.");
    style (presetNextButton, "Next preset (starting profiles first, then yours). Replaces the current sound: press SET A first to keep it.");
    style (presetSaveButton, "Save the current sound as a named preset of your own. Existing presets are never replaced without asking.");
    style (presetDeleteButton, "Delete the loaded user preset (asks first). Starting profiles cannot be deleted.", ui::pink);
    style (abSetA, "Capture the current sound into slot A.");
    style (abSetB, "Capture the current sound into slot B.");
    style (abRecallA, "Switch to the sound stored in A.");
    style (abRecallB, "Switch to the sound stored in B.");
    presetMenuButton.onClick = [this] { openPresetMenu(); };
    for (auto* b : { &presetPrevButton, &presetNextButton })
        b->onClick = [this, b]
        {
            showStatus (presets.step (b == &presetNextButton ? 1 : -1).message);
            shownPresetText = {};
            refreshPresetBar();
        };
    presetSaveButton.onClick = [this] { askSavePreset(); };
    presetDeleteButton.onClick = [this] { askDeletePreset(); };
    abSetA.onClick = [this] { glitchProcessor.captureAbSlot (0); showStatus ("Current sound captured in A."); refreshPresetBar(); };
    abSetB.onClick = [this] { glitchProcessor.captureAbSlot (1); showStatus ("Current sound captured in B."); refreshPresetBar(); };
    abRecallA.onClick = [this] { glitchProcessor.recallAbSlot (0); refreshPresetBar(); };
    abRecallB.onClick = [this] { glitchProcessor.recallAbSlot (1); refreshPresetBar(); };
    label (abLabel, 12.0f, true, ui::textDim, juce::Justification::centredRight);
    label (presetStatus, 11.5f, false, ui::textDim);

    // ---- MODIFY navigation and actions ----
    static const char* catText[numCategories] = { "PITCH & KEY", "TONE", "DYNAMICS", "SPACE" };
    static const char* catTip[numCategories] = { "Pitch Tune: on/off, key, scale, tuning style, strength and speed.", "A darker-to-brighter control, four EQ gains and warmth (saturation).",
                                                 "A softer-to-more-upfront control, the compressor and the output level.", "A dry-to-spacious control, echo, width (Dimension) and the Doubler." };
    for (int i = 0; i < numCategories; ++i)
    {
        categoryButtons[i].setButtonText (catText[i]);
        style (categoryButtons[i], catTip[i]);
        categoryButtons[i].onClick = [this, i] { showCategory (i, true); };
    }
    style (nextButton, "Continue: pick a profile and build your mix.");
    nextButton.onClick = [this] { showPage (Page::autoMix, true); };
    style (modifyButton, "Refine the sound, one category at a time.");
    modifyButton.onClick = [this] { showPage (Page::modify, true); };
    style (backButton, "Back to the profiles and BUILD MY VOCAL MIX.");
    backButton.onClick = [this] { showPage (Page::autoMix, true); };
    style (resetButton, "Set every control back to its default (Factory Default). Asks first. Your A/B snapshots are kept.", ui::pink);
    resetButton.onClick = [this]
    {
        juce::Component::SafePointer<GlitchKidEditor> safe (this);
        askYesNo ("Reset everything?", "This sets every control to its default (the Factory Default sound). Press SET A first if you want to be able to come back to the current sound.",
                  "Reset", [safe]
                  {
                      if (safe == nullptr)
                          return;
                      safe->showStatus (safe->presets.loadFactoryDefault().message);
                      safe->shownPresetText = {};
                      safe->refreshPresetBar();
                  });
    };
    style (savePresetButton, "Save the current sound as your own preset. Existing presets are never replaced without asking.");
    savePresetButton.onClick = [this] { askSavePreset(); };

    // ---- TUNE: note pad, scale chips, tuning style ----
    choices (tuneKeyBox, GlitchKidProcessor::tuneKeyId);
    choices (tuneScaleBox, GlitchKidProcessor::tuneScaleId);
    label (tuneSubline, 12.0f, false, ui::textDim);
    tuneSubline.setText ("GlitchKid does not detect your key. Choose the key and scale of your beat; Pitch Tune pulls your voice toward those notes.", juce::dontSendNotification);
    label (tuneSummary, 17.0f, true, ui::textBright);
    label (tuneOnCaption, 11.0f, true, ui::textDim);
    label (styleCaption, 11.0f, true, ui::textDim);
    label (tuneKeyLabel, 11.0f, true, ui::textDim);
    label (tuneScaleLabel, 11.0f, true, ui::textDim);
    label (keyLabel, 11.0f, true, ui::textDim);
    label (scaleCaption, 11.0f, true, ui::textDim);
    label (scopeCaption, 11.0f, true, ui::textDim);
    label (tuneReadout, 14.0f, true, ui::cyan);
    tuneOnButton.setColour (juce::ToggleButton::tickColourId, ui::lime);
    tuneOnButton.setTooltip ("Vocal pitch correction for a single voice (adds about 31 ms of latency while on). Switch it off for a vocal that is already in tune.");
    tuneKeyBox.setTooltip ("The key (root note) of the song. GlitchKid does not detect your key.");
    tuneScaleBox.setTooltip ("Which notes the voice may be pulled to. Chromatic = any semitone.");
    scope.setTooltip ("The real input level over the last few seconds. Nothing is drawn unless audio is arriving.");
    addAll ({ &tuneOnButton, &tuneKeyBox, &tuneScaleBox, &scope });
    if (auto* keyParam = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (GlitchKidProcessor::tuneKeyId)))
        for (int i = 0; i < keyParam->choices.size(); ++i)
        {
            auto* k = noteKeys.add (new ui::NoteKey (keyParam->choices[i]));
            k->setTooltip ("Set the key of your beat to " + keyParam->choices[i] + ".");
            k->onClick = [this, i]
            {
                tuneKeyBox.setSelectedItemIndex (i, juce::sendNotificationSync);
                refreshTunePad();
                noteKeys[i]->pulse (motionOn);
            };
            content.addAndMakeVisible (k);
        }
    if (auto* scaleParam = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (GlitchKidProcessor::tuneScaleId)))
        for (int i = 0; i < juce::jmin (6, scaleParam->choices.size()); ++i)
        {
            style (scaleChips[i], "Pull the voice toward the notes of the " + scaleParam->choices[i] + " scale.");
            scaleChips[i].setButtonText (scaleParam->choices[i]);
            scaleChips[i].onClick = [this, i]
            {
                tuneScaleBox.setSelectedItemIndex (i, juce::sendNotificationSync);
                refreshTunePad();
                for (auto* k : noteKeys)
                    k->pulse (motionOn);
            };
        }
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
        };
    }
    tuneKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::tuneStrengthId, "STRENGTH", " %", ui::violet, "How much of the distance to the target note is corrected. 100 % = every note lands on the scale."));
    tuneKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::tuneSpeedId, "SPEED", " ms", ui::violet, "How fast the correction glides. 0 ms = instant (the hard-tune sound)."));
    for (auto* k : tuneKnobs)
        k->addTo (content);

    // ---- AUTO: the eight profiles, BUILD MY VOCAL MIX, the result ----
    for (int i = 0; i < FactoryPresets::count; ++i)
    {
        auto* card = profileCards.add (new ui::ProfileCard());
        card->bind (i, false, false);
        card->setTooltip (juce::String (FactoryPresets::info (i).description) + " Click to read about it; double-click to load it.");
        card->onClick = [this, i] { selectProfile (i); };
        card->onDoubleClick = [this, i] { selectProfile (i); loadBrowsed(); };
        content.addAndMakeVisible (card);
    }
    for (auto* l : { &profileHeading, &buildHeading, &resultHeading })
        label (*l, 11.0f, true, ui::textDim);
    for (auto* l : { &detailDesc, &detailSettings, &detailStart })
        label (*l, 12.0f, false, ui::textDim, juce::Justification::topLeft);
    detailDesc.setFont (ui::fontOf (14.0f, true));
    detailDesc.setColour (juce::Label::textColourId, ui::textBright);
    label (sourceLine, 11.5f, false, ui::textDim, juce::Justification::topLeft);
    sourceLine.setTooltip ("What GlitchKid can actually hear. The vocal is this track's input. The instrumental is optional and needs a sidechain/aux send from the beat's track (see docs/SOURCES.md).");
    label (autoStatus, 12.5f, false, ui::textBright, juce::Justification::topLeft);
    label (resultTitle, 13.0f, true, ui::textDim);
    resultTitle.setMinimumHorizontalScale (0.8f);
    label (resultBody, 12.0f, false, ui::textDim, juce::Justification::topLeft);
    label (autoStyleLabel, 11.0f, true, ui::textDim);
    autoStyleLabel.setTooltip ("How much of the profile's effect amounts (echo, doubler, saturation...) a build uses: 100 % = the profile as saved, 0 % = half of it.");
    autoIntensitySlider.setSliderStyle (juce::Slider::LinearHorizontal);
    autoIntensitySlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 22);
    autoIntensitySlider.setRange (0.0, 100.0, 1.0);
    autoIntensitySlider.setTextValueSuffix (" %");
    autoIntensitySlider.setValue (glitchProcessor.getAutoIntensity() * 100.0, juce::dontSendNotification);
    autoIntensitySlider.setColour (juce::Slider::trackColourId, ui::violet);
    autoIntensitySlider.setTooltip ("MIX INTENSITY: how much of the profile's effect amounts a build uses (100 % = as saved).");
    autoIntensitySlider.onValueChange = [this] { glitchProcessor.setAutoIntensity ((float) autoIntensitySlider.getValue() / 100.0f); };
    content.addAndMakeVisible (autoIntensitySlider);
    autoAnalyzeButton.setTooltip ("BUILD MY VOCAL MIX: listens to your vocal for a few seconds (press play in your host and let it rap or sing), then builds a starting mix from the selected profile and what it heard, and applies it. Press BACK TO MY SOUND to undo.");
    content.addAndMakeVisible (autoAnalyzeButton);
    style (autoApplyButton, "Shows exactly which settings the last build changed, why, and what was not available.");
    autoAnalyzeButton.onClick = [this]
    {
        if (glitchProcessor.getAnalyzer().isListening())
            glitchProcessor.stopSourceAnalysis(); // finish early with what has been heard; the build runs when the analysis ends
        else
        {
            buildStartCount = glitchProcessor.getAnalyzer().completedCount();
            glitchProcessor.startSourceAnalysis();
            buildState = BuildState::listening;
            buildMessage = {};
            autoWaiting = true;
            autoSawListening = false;
        }
        shownAutoText = {};
        refreshAutoMode();
    };
    autoApplyButton.onClick = [this] { showWhatChanged(); };
    style (loadButton, "Load the selected profile as saved, so you hear it before building (use DRY / PROCESSED to compare). Your previous sound is kept: BACK TO MY SOUND returns to it.");
    loadButton.onClick = [this] { loadBrowsed(); };
    style (backToMineButton, "Put back the sound you had before you built a mix or loaded a profile.");
    backToMineButton.onClick = [this] { backToMine(); };
    backToMineButton.setEnabled (false);

    // ---- MODIFY: one high-level control per category, then the detailed controls ----
    for (int m = 0; m < macros::numMacros; ++m)
    {
        auto& sl = macroSliders[m];
        sl.setSliderStyle (juce::Slider::LinearHorizontal);
        sl.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        sl.setRange (0.0, 1.0, 0.01);
        sl.setColour (juce::Slider::trackColourId, ui::violet);
        sl.setTooltip (macros::macroDescription ((macros::Macro) m));
        sl.setDoubleClickReturnValue (true, 0.5);
        sl.onValueChange = [this, m] { applyMacroSlider (m); };
        content.addAndMakeVisible (sl);
    }
    label (macroName, 13.0f, true, ui::textBright);
    label (macroLow, 12.0f, false, ui::textDim, juce::Justification::centredRight);
    label (macroHigh, 12.0f, false, ui::textDim);
    toneKnobs.add (new ParamKnob (p.apvts, "eq_lo_gain", "LOW", " dB", ui::violet, "Low shelf (100 Hz): more weight or less boom."));
    toneKnobs.add (new ParamKnob (p.apvts, "eq_b1_gain", "BODY", " dB", ui::violet, "Bell around 400 Hz: cut a little if the vocal sounds muddy or boxy."));
    toneKnobs.add (new ParamKnob (p.apvts, "eq_b2_gain", "PRESENCE", " dB", ui::violet, "Bell around 3 kHz: the words come forward."));
    toneKnobs.add (new ParamKnob (p.apvts, "eq_hi_gain", "AIR", " dB", ui::violet, "High shelf (8 kHz): brightness and breath. Lower it if the S sounds are harsh."));
    toneKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::satDriveId, "WARMTH", " dB", ui::violet, "Saturation: a little warmth and grit. 0 = clean."));
    for (auto* k : toneKnobs)
        k->addTo (content);
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compThresholdId, "THRESHOLD", " dB", ui::violet, "The level above which the compressor starts working. Lower = more compression."));
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compRatioId, "RATIO", ":1", ui::violet, "How firmly the level is held down above the threshold. 1:1 = off."));
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compAttackId, "ATTACK", " ms", ui::violet, "How quickly it reacts. Slower lets the start of words through."));
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compReleaseId, "RELEASE", " ms", ui::violet, "How quickly it lets go after a loud word."));
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compMakeupId, "MAKEUP", " dB", ui::violet, "Makes the compressed vocal louder again."));
    for (auto* k : compKnobs)
        k->addTo (content);
    gainKnob.add (new ParamKnob (p.apvts, GlitchKidProcessor::gainId, "OUTPUT GAIN", " dB", ui::cyan, "The final level of the vocal."));
    gainKnob[0]->addTo (content);
    for (auto* l : { &inLabel, &outLabel, &grLabel })
        label (*l, 11.0f, true, ui::textDim, juce::Justification::centred);
    addAll ({ &inMeter, &outMeter, &grMeter });
    choices (echoDivBox, GlitchKidProcessor::echoDivId);
    echoDivBox.setTooltip ("The echo's length as a note value of the host tempo.");
    for (auto* t : { &echoOnButton, &echoSyncButton, &dimOnButton, &dblOnButton })
        t->setColour (juce::ToggleButton::tickColourId, ui::lime);
    echoSyncButton.setTooltip ("On: the echo follows the host tempo (DIVISION). Off: it uses TIME.");
    echoKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::echoTimeId, "TIME", " ms", ui::violet, "Echo time when SYNC is off."));
    echoKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::echoFbId, "FEEDBACK", " %", ui::violet, "How many repeats."));
    echoKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::echoToneId, "TONE", " Hz", ui::violet, "Darkens the repeats so they sit behind the voice."));
    echoKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::echoMixId, "ECHO MIX", " %", ui::violet, "How loud the echo is."));
    for (auto* k : echoKnobs)
        k->addTo (content);
    dimKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::dimWidthId, "WIDTH", " %", ui::violet, "Stereo spread of the thickening."));
    dimKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::dimMixId, "AMOUNT", " %", ui::violet, "How much of the widened sound is added."));
    for (auto* k : dimKnobs)
        k->addTo (content);
    dblOnButton.setTooltip ("A synthetic double of the vocal: two slightly late, slightly unsteady copies for thickness. Not a second recorded take. No latency.");
    dblKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::dblAmountId, "DOUBLE", " %", ui::violet, "How loud the double is."));
    dblKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::dblWidthId, "WIDTH", " %", ui::violet, "How wide the double sits. 100 % is mono-safe."));
    for (auto* k : dblKnobs)
        k->addTo (content);
    for (auto* l : { &echoTitle, &dimTitle, &dblTitle, &echoDivLabel })
        label (*l, 11.0f, true, ui::textDim);
    addAll ({ &echoOnButton, &echoSyncButton, &echoDivBox, &dimOnButton, &dblOnButton });

    // Everything lives in `content`, which is laid out at the design size and scaled to fit the window.
    addAndMakeVisible (content);
    setResizable (true, true);
    setResizeLimits (designW * 3 / 5, designH * 3 / 5, designW * 3 / 2, designH * 3 / 2); // below ~60 % the text is too small to read
    getConstrainer()->setFixedAspectRatio ((double) designW / (double) designH);
    refreshProfiles();
    setSize (designW, designH);
    showPage (page, false);
    timerCallback(); // fill the read-outs immediately
    startTimerHz (30);
}

GlitchKidEditor::~GlitchKidEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void GlitchKidEditor::startFade()
{
    pageFade = motionOn ? 0.0f : 1.0f;
    content.setAlpha (motionOn ? 0.3f : 1.0f);
}

void GlitchKidEditor::refreshTunePad()
{
    const int key = tuneKeyBox.getSelectedItemIndex(), scale = tuneScaleBox.getSelectedItemIndex();
    const bool changed = key != shownKey || scale != shownScale;
    for (int i = 0; i < noteKeys.size(); ++i)
    {
        noteKeys[i]->setSelected (i == key);
        noteKeys[i]->setInScale (scale >= 0 && key >= 0 && glitchkid::pitch::ScaleQuantizer::isInScale (60 + i, key, (glitchkid::pitch::ScaleQuantizer::Scale) scale));
    }
    for (int i = 0; i < 6; ++i)
        scaleChips[i].setToggleState (i == scale, juce::dontSendNotification);
    if (changed && shownKey >= 0 && key != shownKey && juce::isPositiveAndBelow (key, noteKeys.size()))
        noteKeys[key]->pulse (motionOn); // a change from automation or a preset pulses too
    shownKey = key;
    shownScale = scale;
}


void GlitchKidEditor::showPage (Page p, bool remember)
{
    if (p != page)
        startFade();
    page = p;
    if (remember)
        glitchProcessor.setUiInt ("uiPage", (int) page);
    for (int i = 0; i < 3; ++i)
        navButtons[i].setToggleState (i == (int) page, juce::dontSendNotification);
    layoutContent();
    repaint();
}

void GlitchKidEditor::showCategory (int c, bool remember)
{
    if (juce::jlimit (0, numCategories - 1, c) != category)
        startFade();
    category = juce::jlimit (0, numCategories - 1, c);
    if (remember)
        glitchProcessor.setUiInt ("uiCategory", category);
    layoutContent();
    repaint();
}

void GlitchKidEditor::applyMacroSlider (int m)
{
    auto s = glitchProcessor.captureSoundState();
    glitchkid::macros::apply ((glitchkid::macros::Macro) m, (float) macroSliders[m].getValue(), s);
    glitchProcessor.applyParameterChanges (s);
}

// One 30 Hz tick: the real input level into the scope and the chain, the TUNE read-outs and the processing chain from the real parameters, the dry/processed
// switch from the Bypass parameter. Nothing here touches the audio thread.
void GlitchKidEditor::refreshState (float peak, float outPeak)
{
    namespace fx = glitchkid::fx;
    constexpr float dt = 1.0f / 30.0f;
    scope.push (peak);
    signalTicks = peak > 0.001f ? 0 : juce::jmin (signalTicks + 1, 10000);
    refreshTunePad();
    for (auto* k : noteKeys)
        k->tick (dt);
    if (pageFade < 1.0f)
    {
        pageFade = juce::jmin (1.0f, pageFade + 0.2f);
        content.setAlpha (0.3f + 0.7f * pageFade);
    }
    autoAnalyzeButton.setMotion (motionOn);
    autoAnalyzeButton.tick (dt);

    const int loaded = glitchkid::FactoryPresets::indexOfName (glitchProcessor.getPresetName());
    for (auto* card : profileCards)
    {
        const int pi = card->getProfile();
        if (pi >= 0)
            card->bind (pi, pi == browsedProfile, pi == loaded);
    }

    const bool dry = glitchProcessor.apvts.getRawParameterValue (GlitchKidProcessor::bypassId)->load() >= 0.5f;
    const auto all = glitchProcessor.captureSoundState();

    // the processing chain: each stage lit only if it really does something right now (nothing is processed while DRY)
    {
        std::array<bool, ui::ChainStrip::numStages> act {};
        if (! dry)
        {
            act[0] = fx::isActive (fx::pitchTune, all);
            act[1] = fx::isActive (fx::eq, all) || fx::isActive (fx::saturation, all);
            act[2] = fx::isActive (fx::compressor, all);
            act[3] = fx::isActive (fx::echo, all) || fx::isActive (fx::dimension, all) || fx::isActive (fx::doubler, all);
        }
        juce::StringArray legacy; // effects the interface no longer offers but an older session may still have switched on
        for (int e : { (int) fx::formant, (int) fx::harmony, (int) fx::mutation, (int) fx::stutter })
            if (fx::isActive (e, all))
                legacy.add (juce::String (fx::info (e).name).toUpperCase());
        chain.setState (act, peak, outPeak, page == Page::modify ? category : -1, motionOn, legacy.joinIntoString (", "));
        chain.tick (dt);
    }

    // MODIFY: the high-level sliders follow the real parameters (unless being dragged)
    for (int m = 0; m < glitchkid::macros::numMacros; ++m)
        if (macroSliders[m].isVisible() && ! macroSliders[m].isMouseButtonDown())
            macroSliders[m].setValue (glitchkid::macros::read ((glitchkid::macros::Macro) m, all), juce::dontSendNotification);

    // TUNE: the key summary and the selected tuning style are read from the real parameters (no detection is claimed)
    {
        const int tstyle = glitchkid::macros::readTuneStyle (all);
        for (int i = 0; i < glitchkid::macros::numTuneStyles; ++i)
            styleButtons[i].setToggleState (i == tstyle, juce::dontSendNotification);
        juce::String key, scale;
        if (auto* k = dynamic_cast<juce::AudioParameterChoice*> (glitchProcessor.apvts.getParameter (GlitchKidProcessor::tuneKeyId)))
            key = k->getCurrentChoiceName();
        if (auto* s = dynamic_cast<juce::AudioParameterChoice*> (glitchProcessor.apvts.getParameter (GlitchKidProcessor::tuneScaleId)))
            scale = s->getCurrentChoiceName();
        const bool on = all.get ("tune_on", 0.0f) >= 0.5f;
        const auto summary = on ? "Key: " + key + " " + scale + "   |   Style: " + (tstyle >= 0 ? juce::String (glitchkid::macros::tuneStyleLabel ((glitchkid::macros::TuneStyle) tstyle)) : juce::String ("Custom"))
                                : juce::String ("Tuning is off: your voice is not being pulled to any key");
        if (summary != tuneSummary.getText())
            tuneSummary.setText (summary, juce::dontSendNotification);
    }

    const auto compareText = juce::String (dry ? "D" : "P") + (motionOn ? "m" : "s");
    if (compareText != shownCompareText)
    {
        shownCompareText = compareText;
        dryButton.setToggleState (dry, juce::dontSendNotification);
        processedButton.setToggleState (! dry, juce::dontSendNotification);
        motionButton.setButtonText (motionOn ? "MOTION: ON" : "MOTION: OFF");
    }
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


void GlitchKidEditor::showStatus (const juce::String& text)
{
    presetStatus.setText (text, juce::dontSendNotification);
    statusTicks = 30 * 5; // about five seconds
}

// ---- BUILD MY VOCAL MIX ----------------------------------------------------------------------------------------------------------------------

// ---- BUILD MY VOCAL MIX ----------------------------------------------------------------------------------------------------------------------

void GlitchKidEditor::runBuild()
{
    namespace am = glitchkid::automode;
    using SS = GlitchKidProcessor::SourceStatus;
    am::MixInput in;
    in.profile = browsedProfile >= 0 ? browsedProfile : glitchkid::FactoryPresets::indexOfName (glitchProcessor.getPresetName());
    in.intensity = (float) autoIntensitySlider.getValue() / 100.0f;
    in.current = glitchProcessor.captureSoundState();
    in.factoryDefault = glitchProcessor.factoryDefaultState();
    in.lead = glitchProcessor.getAnalyzer().fetch();
    in.beat = glitchProcessor.getInstrumentalAnalyzer().fetch();
    in.allowProfileOnly = true; // too little vocal heard: still give the profile as a starting mix, clearly labelled
    const auto st = glitchProcessor.getInstrumentalStatus();
    in.instrumental = st == SS::live ? am::InstrumentalInput::live : st == SS::connectedSilent ? am::InstrumentalInput::connectedSilent : am::InstrumentalInput::notConnected;

    lastProposal = am::buildMix (in, glitchProcessor.makeRangeLookup());
    buildMessage = lastProposal.message;
    if (! lastProposal.usable)
    {
        buildState = BuildState::failed;
        return;
    }
    const bool rebuilding = glitchProcessor.getPresetName().startsWith ("Built: ") && ! glitchProcessor.isPresetModified();
    if (! (rebuilding && undo.valid)) // keep the ORIGINAL sound while you build again and again
        undo = { in.current, glitchProcessor.getPresetName(), true };
    glitchProcessor.applyPreset ("Built: " + lastProposal.profileName, lastProposal.sound); // every value goes through the normal parameter calls
    buildState = BuildState::built;

    // the concise result: the first few settings fitted to this vocal, then how many came from the profile
    resultSummary = {};
    int shown = 0, more = 0, fromProfile = 0;
    for (const auto& c : lastProposal.changes)
    {
        if (c.reason.startsWith ("From the "))
        {
            ++fromProfile;
            continue;
        }
        if (shown >= 3)
        {
            ++more;
            continue;
        }
        auto* prm = glitchProcessor.apvts.getParameter (c.id);
        resultSummary << (prm != nullptr ? prm->getName (24) : c.id) << ":  " << juce::String (c.from, 1) << " -> " << juce::String (c.to, 1)
                      << (prm != nullptr && prm->getLabel().isNotEmpty() ? " " + prm->getLabel() : juce::String()) << "\n";
        ++shown;
    }
    if (shown == 0)
        resultSummary << "No setting needed fitting: the profile suits this vocal as saved.\n";
    if (more > 0)
        resultSummary << "+ " << more << " more fitted to your vocal\n";
    resultSummary << fromProfile << " settings come from the " << lastProposal.profileName << " profile";
    autoStatus.setTooltip (lastProposal.evidence.joinIntoString ("\n") + "\n" + lastProposal.unavailable.joinIntoString ("\n"));
    showStatus ("Mix applied from " + lastProposal.profileName + ".");
    shownPresetText = {};
    refreshPresetBar();
    refreshProfiles();
}

void GlitchKidEditor::showWhatChanged()
{
    if (! lastProposal.usable)
        return;
    juce::String text = lastProposal.message + "\n\n";
    for (const auto& e : lastProposal.evidence)
        text << "- " << e << "\n";
    int fromProfile = 0;
    juce::String shown;
    for (const auto& c : lastProposal.changes)
    {
        if (c.reason.startsWith ("From the "))
        {
            ++fromProfile;
            continue;
        }
        auto* prm = glitchProcessor.apvts.getParameter (c.id);
        shown << "  " << (prm != nullptr ? prm->getName (32) : c.id) << ": " << juce::String (c.from, 1) << " -> " << juce::String (c.to, 1) << "  (" << c.reason << ")\n";
    }
    text << "\nFitted to your vocal" << (lastProposal.mode == glitchkid::automode::MixMode::beatInformed ? " and beat" : "") << ":\n"
         << (shown.isEmpty() ? juce::String ("  (nothing: the profile fitted as saved)\n") : shown);
    text << "\n" << fromProfile << " more settings differ from your previous sound because of the " << lastProposal.profileName << " profile.\n\nNot available:\n";
    for (const auto& u : lastProposal.unavailable)
        text << "- " << u << "\n";
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "What the build changed", text, "OK", this);
}


void GlitchKidEditor::refreshAutoMode()
{
    auto& an = glitchProcessor.getAnalyzer();
    const bool listening = an.isListening();
    if (autoWaiting && listening)
        autoSawListening = true;
    if (autoWaiting && autoSawListening && ! listening)
        autoWaiting = false;
    if (buildState == BuildState::listening && ! listening && an.completedCount() > buildStartCount)
    {
        autoWaiting = false;
        runBuild(); // the analysis this press started has ended: build (or explain why not)
    }

    using S = glitchkid::analysis::VocalAnalysis::Status;
    juce::String text;
    auto ring = ui::AutoState::idle;
    float ringProgress = 0.0f;
    const bool applied = glitchProcessor.getPresetName().startsWith ("Built: ");
    if (listening)
    {
        ring = ui::AutoState::working;
        ringProgress = an.liveActiveSeconds() / glitchkid::analysis::VocalAnalyzer::maxActiveSeconds;
        text = "Analysing the vocal: " + juce::String (an.liveActiveSeconds(), 1) + " s heard (needs 2 s, stops at 8 s). Press BUILD NOW to stop early.";
    }
    else if (autoWaiting)
    {
        ring = ui::AutoState::waiting;
        text = "Waiting for audio. Press play in your host so the vocal reaches GlitchKid.";
    }
    else if (buildState == BuildState::built)
    {
        ring = ui::AutoState::ready;
        text = applied ? "Mix applied. Compare with DRY / PROCESSED, then refine it in MODIFY." : "Your earlier sound is back. Press BUILD AGAIN to build a mix.";
    }
    else if (buildState == BuildState::failed)
    {
        ring = ui::AutoState::error;
        text = "Nothing was changed.";
    }
    else
        text = "Ready. Pick a profile (or none for a clean start), press BUILD MY VOCAL MIX, then play the vocal. It works without the beat.";

    // what GlitchKid can really hear right now (never claims an analysis that did not happen)
    {
        using SS = GlitchKidProcessor::SourceStatus;
        const auto st = glitchProcessor.getInstrumentalStatus();
        const bool beatAnalysed = glitchProcessor.getInstrumentalAnalyzer().fetch().status == S::ready;
        const auto leadStatus = an.fetch().status;
        const juce::String input = signalTicks < 45 ? "signal detected" : "no signal yet";
        const juce::String lead = listening ? "analysing" : leadStatus == S::ready ? "analysed" : "not analysed yet";
        const juce::String beat = st == SS::notConnected ? "not connected (optional)" : st == SS::connectedSilent ? "on, but no signal"
                                                                                       : beatAnalysed ? "live, analysed" : "live, not analysed yet";
        const auto line = "Input: " + input + "   |   Vocal: " + lead + "\nInstrumental: " + beat;
        if (line != sourceLine.getText())
            sourceLine.setText (line, juce::dontSendNotification);
    }
    autoAnalyzeButton.setState (ring, ringProgress);

    // the result block
    juce::String title, body;
    auto titleColour = ui::textDim;
    if (listening)
        title = "BUILDING...";
    else if (buildState == BuildState::built && applied)
    {
        const auto& m = lastProposal.message;
        title = juce::String (m.startsWith ("STARTING MIX") ? "PROFILE-ONLY MIX APPLIED" : m.startsWith ("BEAT INFORMED") ? "BEAT-INFORMED MIX APPLIED" : "VOCAL-ONLY MIX APPLIED")
                + (glitchProcessor.isPresetModified() ? " (edited since)" : "");
        body = resultSummary;
        titleColour = ui::lime;
    }
    else if (buildState == BuildState::built)
    {
        title = "THAT MIX IS NOT ACTIVE";
        body = "You went back to your earlier sound or loaded another preset.";
    }
    else if (buildState == BuildState::failed)
    {
        title = "MIX NOT BUILT";
        body = buildMessage;
        titleColour = ui::pink;
    }
    else
    {
        title = "NO MIX BUILT YET";
        body = "The result is a starting mix for you to refine, not a finished one.";
    }
    const auto resultKey = title + "|" + body;
    if (resultKey != shownResultText)
    {
        shownResultText = resultKey;
        resultTitle.setText (title, juce::dontSendNotification);
        resultTitle.setColour (juce::Label::textColourId, titleColour);
        resultBody.setText (body, juce::dontSendNotification);
    }

    if (text != shownAutoText)
    {
        shownAutoText = text;
        autoStatus.setText (text, juce::dontSendNotification);
        autoAnalyzeButton.setButtonText (listening ? "BUILD NOW" : buildState == BuildState::failed ? "TRY AGAIN" : buildState == BuildState::built ? "BUILD AGAIN" : "BUILD MY VOCAL MIX");
        autoApplyButton.setEnabled (lastProposal.usable);
    }

    // the profile details and the undo button
    const int loaded = glitchkid::FactoryPresets::indexOfName (glitchProcessor.getPresetName());
    const auto key = juce::String (browsedProfile) + "|" + juce::String (loaded) + "|" + (undo.valid ? "u" : "-");
    if (key != shownDetailKey)
    {
        shownDetailKey = key;
        if (browsedProfile >= 0)
        {
            const auto& in = glitchkid::FactoryPresets::info (browsedProfile);
            detailDesc.setText (in.name, juce::dontSendNotification);
            detailSettings.setText (juce::String (in.description) + "\nWhat it does: " + juce::String (in.settings), juce::dontSendNotification);
            detailStart.setText ("Start with: " + juce::String (in.startWith), juce::dontSendNotification);
        }
        else
        {
            detailDesc.setText ("Choose a starting profile", juce::dontSendNotification);
            detailSettings.setText ("Each one is an original starting point. Select a card to read about it, LOAD PROFILE to hear it as saved, or press BUILD MY VOCAL MIX to fit it to your vocal.",
                                    juce::dontSendNotification);
            detailStart.setText (loaded >= 0 ? "Loaded now: " + juce::String (glitchkid::FactoryPresets::info (loaded).name) : juce::String(), juce::dontSendNotification);
        }
        backToMineButton.setEnabled (undo.valid);
    }
}

// ---- the profiles ----------------------------------------------------------------------------------------------------------------------

void GlitchKidEditor::refreshProfiles()
{
    const int loaded = glitchkid::FactoryPresets::indexOfName (glitchProcessor.getPresetName());
    for (auto* card : profileCards)
        if (card->getProfile() >= 0)
            card->bind (card->getProfile(), card->getProfile() == browsedProfile, card->getProfile() == loaded);
    shownDetailKey = {};
}

void GlitchKidEditor::selectProfile (int profile)
{
    browsedProfile = profile;
    glitchProcessor.setUiInt ("uiProfile", profile); // kept in the session, so AUTO and MODIFY (and a reopened window) agree
    refreshProfiles();
    shownCompareText = {};
}

void GlitchKidEditor::loadBrowsed()
{
    if (browsedProfile < 0)
    {
        showStatus ("Pick a profile first.");
        return;
    }
    const auto current = glitchProcessor.getPresetName();
    const bool onCleanProfile = glitchkid::FactoryPresets::indexOfName (current) >= 0 && ! glitchProcessor.isPresetModified();
    if (! (onCleanProfile && undo.valid)) // keep the ORIGINAL sound while you audition several profiles in a row
        undo = { glitchProcessor.captureSoundState(), current, true };
    const auto outcome = presets.loadFactory (browsedProfile);
    showStatus (outcome.message);
    if (! outcome.ok)
        return;
    shownPresetText = {};
    refreshPresetBar();
    refreshProfiles();
}

void GlitchKidEditor::backToMine()
{
    if (! undo.valid)
        return;
    glitchProcessor.applyPreset (undo.name, undo.state);
    undo.valid = false;
    showStatus ("Back to your sound.");
    shownPresetText = {};
    refreshPresetBar();
    refreshProfiles();
}


void GlitchKidEditor::refreshPresetBar()
{
    const auto name = glitchProcessor.getPresetName();
    const auto kind = presets.kindOf (name);
    const auto tag = kind == glitchkid::PresetController::Kind::factory ? "PROFILE:  " : kind == glitchkid::PresetController::Kind::user ? "USER:  " : "PRESET:  ";
    const auto presetText = tag + (name.isEmpty() ? juce::String ("(none)") : name) + (glitchProcessor.isPresetModified() ? "  *" : "") + "   v";
    if (presetText != shownPresetText)
    {
        shownPresetText = presetText;
        presetMenuButton.setButtonText (presetText);
        presetDeleteButton.setEnabled (presets.isUserPreset (name)); // looks at the preset folder, so only when the name changed or after an action
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
        abRecallA.setTooltip (hasA ? "Switch to the sound stored in A." : "Slot A is empty: press SET A to capture the current sound.");
        abRecallB.setTooltip (hasB ? "Switch to the sound stored in B." : "Slot B is empty: press SET B to capture the current sound.");
    }
}


void GlitchKidEditor::openPresetMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("STARTING PROFILES");
    menu.addItem (1, glitchkid::PresetController::factoryDefaultName);
    for (int i = 0; i < glitchkid::FactoryPresets::count; ++i)
        menu.addItem (2 + i, glitchkid::FactoryPresets::info (i).name, true,
                      juce::String (glitchkid::FactoryPresets::info (i).name).equalsIgnoreCase (glitchProcessor.getPresetName()));
    menu.addSeparator();
    menu.addSectionHeader ("YOUR PRESETS");
    const auto names = presets.getStore().listUserPresets();
    if (names.isEmpty())
        menu.addItem (-1, "(none saved yet: press SAVE)", false);
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
    refreshPresetBar();
    refreshAutoMode();
    if (statusTicks > 0 && --statusTicks == 0)
        presetStatus.setText ({}, juce::dontSendNotification);
    const float inPeak = glitchProcessor.consumeInputPeak(), outPeak = glitchProcessor.consumeOutputPeak();
    inMeter.pushPeak (inPeak);
    outMeter.pushPeak (outPeak);
    refreshState (inPeak, outPeak);
    grMeter.pushReduction (glitchProcessor.consumeGainReductionDb());

    const bool synced = glitchProcessor.apvts.getRawParameterValue (GlitchKidProcessor::echoSyncId)->load() >= 0.5f;
    echoKnobs[0]->setEnabled (! synced);
    echoDivBox.setEnabled (synced);
    echoDivLabel.setAlpha (synced ? 1.0f : 0.4f);

    // Pitch Tune read-out: the sung note, how far off it is, and the note it is pulled to (real detector output; nothing while Pitch Tune is off).
    juce::String tuneText = "Pitch Tune is off";
    if (glitchProcessor.apvts.getRawParameterValue (GlitchKidProcessor::tuneOnId)->load() >= 0.5f)
    {
        const float sung = glitchProcessor.getTuneDetectedMidi(), target = glitchProcessor.getTuneTargetMidi();
        if (sung < 0.0f || target < 0.0f)
            tuneText = "Listening for a sung note...";
        else
        {
            const int cents = (int) std::lround ((sung - std::round (sung)) * 100.0f);
            tuneText = "Sung " + noteName (sung) + (cents >= 0 ? " +" : " ") + juce::String (cents) + " cents   ->   pulled to " + noteName (target);
        }
    }
    if (tuneText != shownTuneText)
    {
        shownTuneText = tuneText;
        tuneReadout.setText (tuneText, juce::dontSendNotification);
    }
}

// ---- painting ---------------------------------------------------------------------------------------------------------------------------

void GlitchKidEditor::paint (juce::Graphics& g)
{
    constexpr float W = (float) designW;
    g.fillAll (ui::obsidian);
    g.addTransform (juce::AffineTransform::scale (scaleFactor));

    // thin technical dividers: under the header and above the preset bar
    g.setColour (ui::border);
    g.fillRect (0.0f, 43.0f, W, 1.0f);
    g.fillRect (0.0f, 562.0f, W, 1.0f);
    if (page == Page::autoMix)
        g.fillRect (586.0f, 104.0f, 1.0f, 444.0f);

    for (const auto& r : panelRects)
    {
        const auto f = r.toFloat();
        g.setColour (ui::panelBottom);
        g.fillRoundedRectangle (f, 6.0f);
        g.setColour (ui::border);
        g.drawRoundedRectangle (f.reduced (0.5f), 6.0f, 1.0f);
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
    using Rect = juce::Rectangle<int>;
    auto show = [] (juce::Component& c, Rect r) { c.setBounds (r); c.setVisible (true); };
    auto showK = [] (ParamKnob& k, Rect r) { k.setBounds (r); k.setVisible (true); };

    // Hide every control that is not part of the always-visible header, chain and preset bar; the current page then shows what it needs.
    const std::vector<juce::Component*> persistent = { &brandLabel, &brandSub, &motionButton, &navButtons[0], &navButtons[1], &navButtons[2], &dryButton, &processedButton, &chain,
                                                       &presetPrevButton, &presetMenuButton, &presetNextButton, &presetSaveButton, &presetDeleteButton, &abSetA, &abSetB,
                                                       &abRecallA, &abRecallB, &abLabel, &presetStatus };
    for (auto* child : content.getChildren())
        if (std::find (persistent.begin(), persistent.end(), child) == persistent.end())
            child->setVisible (false);
    panelRects.clear();

    // ---- header, chain ----
    show (brandLabel, { 16, 5, 170, 26 });
    show (brandSub, { 17, 27, 170, 14 });
    for (int i = 0; i < 3; ++i)
        show (navButtons[i], { 200 + i * 100, 7, 94, 30 });
    show (dryButton, { 672, 7, 64, 30 });
    show (processedButton, { 740, 7, 100, 30 });
    show (motionButton, { 848, 7, 96, 30 });
    show (chain, { 16, 46, 928, 52 });

    // ---- bottom bar: presets and A/B (always available) ----
    Rect bar (16, 568, designW - 32, 26);
    show (presetPrevButton, bar.removeFromLeft (28));
    bar.removeFromLeft (4);
    show (presetMenuButton, bar.removeFromLeft (270));
    bar.removeFromLeft (4);
    show (presetNextButton, bar.removeFromLeft (28));
    bar.removeFromLeft (6);
    show (presetSaveButton, bar.removeFromLeft (60));
    bar.removeFromLeft (6);
    show (presetDeleteButton, bar.removeFromLeft (70));
    bar.removeFromLeft (12);
    show (abRecallB, bar.removeFromRight (40));
    bar.removeFromRight (4);
    show (abRecallA, bar.removeFromRight (40));
    bar.removeFromRight (8);
    show (abSetB, bar.removeFromRight (60));
    bar.removeFromRight (4);
    show (abSetA, bar.removeFromRight (60));
    bar.removeFromRight (6);
    show (abLabel, bar.removeFromRight (34));
    show (presetStatus, bar);

    // ---- TUNE ----
    if (page == Page::tune)
    {
        show (keyLabel, { 24, 102, 200, 16 });
        for (int i = 0; i < noteKeys.size(); ++i)
            show (*noteKeys[i], { 24 + i * 76, 120, 72, 54 });
        show (scaleCaption, { 24, 186, 200, 16 });
        for (int i = 0; i < 6; ++i)
            show (scaleChips[i], { 24 + i * 153, 204, 145, 34 });
        show (tuneOnCaption, { 24, 250, 100, 16 });
        show (tuneOnButton, { 24, 268, 100, 34 });
        show (styleCaption, { 140, 250, 200, 16 });
        for (int i = 0; i < glitchkid::macros::numTuneStyles; ++i)
            show (styleButtons[i], { 140 + i * 130, 268, 124, 34 });
        show (tuneSummary, { 24, 322, 912, 28 });
        show (tuneReadout, { 24, 352, 912, 22 });
        show (tuneSubline, { 24, 378, 912, 18 });
        show (scopeCaption, { 24, 408, 200, 16 });
        show (scope, { 24, 426, 912, 60 });
        show (nextButton, { 764, 500, 172, 44 });
    }

    // ---- AUTO ----
    else if (page == Page::autoMix)
    {
        show (profileHeading, { 24, 102, 548, 16 });
        for (int i = 0; i < profileCards.size(); ++i)
            show (*profileCards[i], { 24 + (i % 2) * 278, 122 + (i / 2) * 62, 270, 54 });
        show (detailDesc, { 24, 372, 548, 20 });
        show (detailSettings, { 24, 394, 548, 44 });
        show (detailStart, { 24, 440, 548, 34 });
        show (loadButton, { 24, 486, 200, 40 });
        show (backToMineButton, { 232, 486, 200, 40 });

        show (buildHeading, { 600, 102, 336, 16 });
        show (autoAnalyzeButton, { 600, 122, 336, 80 });
        show (autoStatus, { 600, 208, 336, 50 });
        show (sourceLine, { 600, 260, 336, 32 });
        show (autoStyleLabel, { 600, 300, 336, 14 });
        show (autoIntensitySlider, { 600, 314, 336, 24 });
        show (resultHeading, { 600, 348, 336, 16 });
        show (resultTitle, { 600, 368, 336, 20 });
        show (resultBody, { 600, 390, 336, 84 });
        show (autoApplyButton, { 600, 482, 160, 34 });
        show (modifyButton, { 776, 504, 160, 44 });
    }

    // ---- MODIFY (one category at a time) ----
    else
    {
        for (int i = 0; i < numCategories; ++i)
        {
            show (categoryButtons[i], { 24 + i * 158, 100, 150, 34 });
            categoryButtons[i].setToggleState (i == category, juce::dontSendNotification);
        }
        panelRects.push_back ({ 24, 144, 912, 352 });
        show (backButton, { 24, 506, 160, 40 });
        show (resetButton, { 192, 506, 100, 40 });
        show (savePresetButton, { 300, 506, 140, 40 });

        // categories 1..3 start with their high-level slider
        if (category >= 1)
        {
            static const glitchkid::macros::Macro categoryMacro[numCategories] = { glitchkid::macros::Macro::tone, glitchkid::macros::Macro::tone, glitchkid::macros::Macro::punch, glitchkid::macros::Macro::space };
            static const char* lows[numCategories] = { "", "DARKER", "SOFTER", "DRY" };
            static const char* highs[numCategories] = { "", "BRIGHTER", "MORE UPFRONT", "SPACIOUS" };
            const auto m = categoryMacro[category];
            macroName.setText (glitchkid::macros::macroName (m), juce::dontSendNotification);
            macroLow.setText (lows[category], juce::dontSendNotification);
            macroHigh.setText (highs[category], juce::dontSendNotification);
            show (macroName, { 36, 154, 90, 30 });
            show (macroLow, { 130, 154, 76, 30 });
            show (macroSliders[(int) m], { 214, 154, 540, 30 });
            show (macroHigh, { 764, 154, 150, 30 });
        }

        switch (category)
        {
            case 0: // PITCH & KEY
                show (tuneOnCaption, { 36, 154, 100, 16 });
                show (tuneOnButton, { 36, 172, 100, 34 });
                show (tuneKeyLabel, { 156, 154, 120, 16 });
                show (tuneKeyBox, { 156, 172, 130, 34 });
                show (tuneScaleLabel, { 306, 154, 150, 16 });
                show (tuneScaleBox, { 306, 172, 210, 34 });
                show (styleCaption, { 540, 154, 200, 16 });
                for (int i = 0; i < glitchkid::macros::numTuneStyles; ++i)
                    show (styleButtons[i], { 540 + i * 128, 172, 122, 34 });
                for (int i = 0; i < tuneKnobs.size(); ++i)
                    showK (*tuneKnobs[i], { 60 + i * 190, 232, 170, 210 });
                show (tuneSummary, { 440, 290, 480, 30 });
                show (tuneReadout, { 440, 326, 480, 24 });
                break;

            case 1: // TONE
                for (int i = 0; i < toneKnobs.size(); ++i)
                    showK (*toneKnobs[i], { 36 + i * 176, 204, 168, 240 });
                break;

            case 2: // DYNAMICS
                for (int i = 0; i < compKnobs.size(); ++i)
                    showK (*compKnobs[i], { 36 + i * 126, 204, 120, 200 });
                show (grMeter, { 680, 204, 22, 230 });
                show (grLabel, { 672, 438, 38, 16 });
                showK (*gainKnob[0], { 724, 204, 130, 220 });
                show (inMeter, { 872, 204, 22, 230 });
                show (outMeter, { 904, 204, 22, 230 });
                show (inLabel, { 864, 438, 38, 16 });
                show (outLabel, { 896, 438, 38, 16 });
                break;

            default: // SPACE
            {
                const int y0 = 196, y1 = 296, y2 = 396;
                show (echoTitle, { 36, y0, 140, 16 });
                show (echoOnButton, { 36, y0 + 20, 100, 28 });
                show (echoSyncButton, { 36, y0 + 52, 100, 28 });
                show (echoDivLabel, { 150, y0, 130, 14 });
                show (echoDivBox, { 150, y0 + 18, 130, 30 });
                for (int i = 0; i < echoKnobs.size(); ++i)
                    showK (*echoKnobs[i], { 300 + i * 158, y0, 150, 98 });
                show (dimTitle, { 36, y1, 260, 16 });
                show (dimOnButton, { 36, y1 + 20, 100, 28 });
                for (int i = 0; i < dimKnobs.size(); ++i)
                    showK (*dimKnobs[i], { 300 + i * 158, y1, 150, 98 });
                show (dblTitle, { 36, y2, 200, 16 });
                show (dblOnButton, { 36, y2 + 20, 100, 28 });
                for (int i = 0; i < dblKnobs.size(); ++i)
                    showK (*dblKnobs[i], { 300 + i * 158, y2, 150, 98 });
                break;
            }
        }
    }
}
