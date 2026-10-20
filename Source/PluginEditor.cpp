#include "PluginEditor.h"
#include "DSP/PitchCorrection/ScaleQuantizer.h"
#include "AutoMode/SourceRelationship.h"

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
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1c1c26));
        b.setColour (juce::TextButton::buttonOnColourId, ui::lime);
        b.setColour (juce::TextButton::textColourOffId, text);
        b.setColour (juce::TextButton::textColourOnId, ui::obsidian);
        b.setTooltip (tip);
        content.addAndMakeVisible (b);
    };
    auto primary = [&style] (juce::TextButton& b, const juce::String& tip)
    {
        style (b, tip);
        b.setColour (juce::TextButton::buttonColourId, ui::lime.darker (0.15f));
        b.setColour (juce::TextButton::textColourOffId, ui::obsidian);
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

    // ---- the preset bar and A/B (always available) ----
    style (presetMenuButton, "Choose the clean default, one of the starting profiles or one of your saved presets.");
    style (presetPrevButton, "Previous preset (starting profiles first, then yours). Replaces the current sound: press SET A first to keep it.");
    style (presetNextButton, "Next preset (starting profiles first, then yours). Replaces the current sound: press SET A first to keep it.");
    style (presetSaveButton, "Save the current sound as a named preset of your own. Existing presets are never replaced without asking.", ui::lime);
    style (presetDeleteButton, "Delete the loaded user preset (asks first). Starting profiles cannot be deleted.", ui::pink);
    style (abSetA, "Capture the current sound into slot A.", ui::neon);
    style (abSetB, "Capture the current sound into slot B.", ui::neon);
    style (abRecallA, "Switch to the sound stored in A.");
    style (abRecallB, "Switch to the sound stored in B.");
    style (abCopyAB, "Copy slot A into slot B.", ui::textDim);
    style (abCopyBA, "Copy slot B into slot A.", ui::textDim);
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
    abCopyAB.onClick = [this] { if (glitchProcessor.copyAbSlot (0, 1)) showStatus ("A copied to B."); refreshPresetBar(); };
    abCopyBA.onClick = [this] { if (glitchProcessor.copyAbSlot (1, 0)) showStatus ("B copied to A."); refreshPresetBar(); };
    label (abLabel, 13.0f, true, ui::violet, juce::Justification::centredRight);
    label (presetStatus, 11.0f, false, ui::lime);

    // ---- navigation ----
    static const char* navText[3] = { "1  TUNE", "2  AUTO", "3  MODIFY" };
    static const char* navTip[3] = { "Step 1: tell GlitchKid the key and scale of your beat.", "Step 2: pick a starting profile, then press BUILD MY VOCAL MIX.",
                                     "Step 3: fine-tune the result: pitch, tone, dynamics and space." };
    for (int i = 0; i < 3; ++i)
    {
        navButtons[i].setButtonText (navText[i]);
        style (navButtons[i], navTip[i]);
        navButtons[i].getProperties().set ("shape", "tab");
        navButtons[i].onClick = [this, i] { showPage ((Page) i, true); };
    }
    static const char* catText[numCategories] = { "PITCH & KEY", "TONE", "DYNAMICS", "SPACE" };
    static const char* catTip[numCategories] = { "Pitch Tune: on/off, key, scale, tuning style, strength and speed.", "Four EQ gains and warmth (saturation).",
                                                 "The compressor and the output level.", "Echo, width (Dimension) and the Doubler." };
    for (int i = 0; i < numCategories; ++i)
    {
        categoryButtons[i].setButtonText (catText[i]);
        style (categoryButtons[i], catTip[i]);
        categoryButtons[i].onClick = [this, i] { showCategory (i, true); };
    }
    primary (nextButton, "Continue to step 2: pick a profile and build your mix.");
    nextButton.onClick = [this] { showPage (Page::autoMix, true); };
    primary (modifyButton, "Go to step 3: fine-tune the sound, one category at a time.");
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
    style (savePresetButton, "Save the current sound as your own preset. Existing presets are never replaced without asking.", ui::lime);
    savePresetButton.onClick = [this] { askSavePreset(); };
    style (motionButton, "Switch animations on or off. Off = a calm, static interface (meters and levels still update).");
    motionButton.onClick = [this]
    {
        motionOn = ! motionOn;
        glitchProcessor.setUiFlag ("uiMotion", motionOn);
        shownCompareText = {};
    };
    style (dryButton, "Hear the untouched input. This is the real bypass (delay-matched and crossfaded over 20 ms), not the Mix knob.");
    style (processedButton, "Hear the processed sound.");
    dryButton.setColour (juce::TextButton::buttonOnColourId, ui::pink);
    processedButton.setColour (juce::TextButton::buttonOnColourId, ui::lime);
    dryButton.onClick = [this] { setBypassFromUi (true); };
    processedButton.onClick = [this] { setBypassFromUi (false); };
    label (compareCaption, 14.0f, true, ui::lime);

    // ---- TUNE: note pad, scale chips, tuning style ----
    choices (tuneKeyBox, GlitchKidProcessor::tuneKeyId);
    choices (tuneScaleBox, GlitchKidProcessor::tuneScaleId);
    label (tuneSubline, 15.0f, false, ui::textDim, juce::Justification::centredTop);
    tuneSubline.setText ("GlitchKid does not detect your key. Choose the key and scale of your beat; Pitch Tune pulls your voice toward those notes.", juce::dontSendNotification);
    label (tuneSummary, 16.0f, true, ui::lime, juce::Justification::centred);
    label (tuneOnCaption, 12.0f, true, ui::textDim);
    label (styleCaption, 12.0f, true, ui::textDim);
    label (tuneKeyLabel, 11.0f, true, ui::textDim);
    label (tuneScaleLabel, 11.0f, true, ui::textDim);
    label (tuneReadout, 14.0f, true, ui::lime, juce::Justification::centred);
    tuneOnButton.setColour (juce::ToggleButton::tickColourId, ui::lime);
    tuneOnButton.setTooltip ("Vocal pitch correction for a single voice (adds about 31 ms of latency while on). Switch it off for a vocal that is already in tune.");
    tuneKeyBox.setTooltip ("The key (root note) of the song. GlitchKid does not detect your key.");
    tuneScaleBox.setTooltip ("Which notes the voice may be pulled to. Chromatic = any semitone.");
    addAll ({ &tuneOnButton, &tuneKeyBox, &tuneScaleBox });
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
            mascot.cheer();
        };
    }
    tuneKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::tuneStrengthId, "STRENGTH", " %", ui::violet, "How much of the distance to the target note is corrected. 100 % = every note lands on the scale."));
    tuneKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::tuneSpeedId, "SPEED", " ms", ui::violet, "How fast the correction glides. 0 ms = instant (the hard-tune sound)."));
    for (auto* k : tuneKnobs)
        k->addTo (content);
    addAll ({ &scope, &mascot });

    // ---- AUTO: the eight profiles, BUILD MY VOCAL MIX, audition ----
    for (int i = 0; i < FactoryPresets::count; ++i)
    {
        auto* card = profileCards.add (new ui::ProfileCard());
        card->bind (i, false, false);
        card->setTooltip (juce::String (FactoryPresets::info (i).description) + " Click to read about it; double-click to load it.");
        card->onClick = [this, i] { selectProfile (i); };
        card->onDoubleClick = [this, i] { selectProfile (i); loadBrowsed(); };
        content.addAndMakeVisible (card);
    }
    for (auto* l : { &detailDesc, &detailSettings, &detailStart })
        label (*l, 12.0f, false, ui::textDim, juce::Justification::topLeft);
    detailDesc.setFont (ui::fontOf (15.0f, true));
    detailDesc.setColour (juce::Label::textColourId, ui::textBright);
    label (sourceLine, 11.0f, true, ui::textDim);
    sourceLine.setTooltip ("What GlitchKid can actually hear. The lead is this track's input. The instrumental is optional and needs a sidechain/aux send from the beat's track (see docs/SOURCES.md).");
    label (autoStatus, 12.5f, false, ui::textDim);
    autoStatus.setMinimumHorizontalScale (0.85f);
    label (autoStyleLabel, 11.0f, true, ui::neon);
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
    style (autoAnalyzeButton, "BUILD MY VOCAL MIX: listens to your vocal for a few seconds (press play in your host and let it rap or sing), then builds a starting mix from the selected profile and what it heard, and applies it. Press BACK TO MY SOUND to undo.", ui::lime);
    style (autoApplyButton, "Shows exactly which settings the last build changed, why, and what was not available.", ui::lime);
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
    primary (loadButton, "Load the selected profile into the plug-in as saved, so you hear it before building (use DRY / PROCESSED to compare). Your previous sound is kept: BACK TO MY SOUND returns to it.");
    loadButton.onClick = [this] { loadBrowsed(); };
    style (backToMineButton, "Put back the sound you had before you built a mix or loaded a profile.");
    backToMineButton.onClick = [this] { backToMine(); };
    backToMineButton.setEnabled (false);
    for (int i = 0; i < macros::numCommands; ++i)
    {
        const auto c = (macros::Command) i;
        commandButtons[i].setButtonText (macros::commandLabel (c));
        style (commandButtons[i], macros::commandDescription (c));
        commandButtons[i].onClick = [this, c]
        {
            auto s = glitchProcessor.captureSoundState();
            macros::apply (c, s);
            const int n = glitchProcessor.applyParameterChanges (s);
            showStatus (juce::String (macros::commandLabel (c)) + ": " + (n > 0 ? juce::String (n) + " controls moved." : juce::String ("already at its limit.")));
            mascot.cheer();
        };
    }
    label (talkCaption, 12.0f, true, ui::textDim);

    // ---- MODIFY: only real, audible controls ----
    toneKnobs.add (new ParamKnob (p.apvts, "eq_lo_gain", "LOW", " dB", ui::violet, "Low shelf (100 Hz): more weight or less boom."));
    toneKnobs.add (new ParamKnob (p.apvts, "eq_b1_gain", "BODY", " dB", ui::violet, "Bell around 400 Hz: cut a little if the vocal sounds muddy or boxy."));
    toneKnobs.add (new ParamKnob (p.apvts, "eq_b2_gain", "PRESENCE", " dB", ui::neon, "Bell around 3 kHz: the words come forward."));
    toneKnobs.add (new ParamKnob (p.apvts, "eq_hi_gain", "AIR", " dB", ui::neon, "High shelf (8 kHz): brightness and breath. Lower it if the S sounds are harsh."));
    toneKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::satDriveId, "WARMTH", " dB", ui::lime, "Saturation: a little warmth and grit. 0 = clean."));
    for (auto* k : toneKnobs)
        k->addTo (content);
    label (toneTitle, 12.0f, true, ui::textDim);
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compThresholdId, "THRESHOLD", " dB", ui::violet, "The level above which the compressor starts working. Lower = more compression."));
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compRatioId, "RATIO", ":1", ui::violet, "How firmly the level is held down above the threshold. 1:1 = off."));
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compAttackId, "ATTACK", " ms", ui::violet, "How quickly it reacts. Slower lets the start of words through."));
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compReleaseId, "RELEASE", " ms", ui::violet, "How quickly it lets go after a loud word."));
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compMakeupId, "MAKEUP", " dB", ui::neon, "Makes the compressed vocal louder again."));
    for (auto* k : compKnobs)
        k->addTo (content);
    gainKnob.add (new ParamKnob (p.apvts, GlitchKidProcessor::gainId, "OUTPUT GAIN", " dB", ui::neon, "The final level of the vocal."));
    gainKnob[0]->addTo (content);
    label (inLabel, 11.0f, true, ui::textDim, juce::Justification::centred);
    label (outLabel, 11.0f, true, ui::textDim, juce::Justification::centred);
    label (grLabel, 11.0f, true, ui::textDim, juce::Justification::centred);
    label (compTitle, 12.0f, true, ui::textDim);
    addAll ({ &inMeter, &outMeter, &grMeter });
    choices (echoDivBox, GlitchKidProcessor::echoDivId);
    echoDivBox.setTooltip ("The echo's length as a note value of the host tempo.");
    echoOnButton.setColour (juce::ToggleButton::tickColourId, ui::lime);
    echoSyncButton.setColour (juce::ToggleButton::tickColourId, ui::lime);
    echoSyncButton.setTooltip ("On: the echo follows the host tempo (DIVISION). Off: it uses TIME.");
    echoKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::echoTimeId, "TIME", " ms", ui::cyan, "Echo time when SYNC is off."));
    echoKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::echoFbId, "FEEDBACK", " %", ui::cyan, "How many repeats."));
    echoKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::echoToneId, "TONE", " Hz", ui::cyan, "Darkens the repeats so they sit behind the voice."));
    echoKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::echoMixId, "ECHO MIX", " %", ui::cyan, "How loud the echo is."));
    for (auto* k : echoKnobs)
        k->addTo (content);
    label (echoTitle, 12.0f, true, ui::textDim);
    label (echoDivLabel, 11.0f, true, ui::textDim, juce::Justification::centred);
    dimOnButton.setColour (juce::ToggleButton::tickColourId, ui::lime);
    dimKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::dimWidthId, "WIDTH", " %", ui::cyan, "Stereo spread of the thickening."));
    dimKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::dimMixId, "AMOUNT", " %", ui::cyan, "How much of the widened sound is added."));
    for (auto* k : dimKnobs)
        k->addTo (content);
    label (dimTitle, 12.0f, true, ui::textDim);
    dblOnButton.setColour (juce::ToggleButton::tickColourId, ui::lime);
    dblOnButton.setTooltip ("A synthetic double of the vocal: two slightly late, slightly unsteady copies for thickness. Not a second recorded take. No latency.");
    dblKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::dblAmountId, "DOUBLE", " %", ui::cyan, "How loud the double is."));
    dblKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::dblWidthId, "WIDTH", " %", ui::cyan, "How wide the double sits. 100 % is mono-safe."));
    for (auto* k : dblKnobs)
        k->addTo (content);
    label (dblTitle, 12.0f, true, ui::textDim);
    addAll ({ &echoOnButton, &echoSyncButton, &echoDivBox, &dimOnButton, &dblOnButton });

    // Everything lives in `content`, which is laid out at the design size and scaled to fit the window.
    addAndMakeVisible (content);
    setResizable (true, true);
    setResizeLimits (designW * 3 / 5, designH * 3 / 5, designW * 3 / 2, designH * 3 / 2); // below ~60 % the text is too small to read
    getConstrainer()->setFixedAspectRatio ((double) designW / (double) designH);
    refreshProfiles();
    setSize (designW * 84 / 100, designH * 84 / 100);
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

juce::String GlitchKidEditor::currentTitle() const
{
    if (page == Page::tune)
        return "SET YOUR KEY";
    if (page == Page::modify)
    {
        static const char* categoryNames[] = { "PITCH & KEY", "TONE", "DYNAMICS", "SPACE" };
        return categoryNames[juce::jlimit (0, numCategories - 1, category)];
    }
    if (browsedProfile >= 0)
        return glitchkid::FactoryPresets::info (browsedProfile).name;
    const int loaded = glitchkid::FactoryPresets::indexOfName (glitchProcessor.getPresetName());
    return loaded >= 0 ? juce::String (glitchkid::FactoryPresets::info (loaded).name) : juce::String ("PICK A PROFILE");
}

// One 30 Hz tick: real input level into the scope and mascot, the TUNE read-outs from the real parameters, the dry/processed switch from the Bypass parameter.
// Nothing here touches the audio thread.
void GlitchKidEditor::refreshState (float peak)
{
    constexpr float dt = 1.0f / 30.0f;
    scope.push (peak);
    refreshTunePad();
    for (auto* k : noteKeys)
        k->tick (dt);
    if (pageFade < 1.0f)
    {
        pageFade = juce::jmin (1.0f, pageFade + 0.2f);
        content.setAlpha (0.3f + 0.7f * pageFade);
    }
    mascot.setMotion (motionOn);
    mascot.setLevel (peak);
    mascot.tick (dt);
    autoAnalyzeButton.setMotion (motionOn);
    autoAnalyzeButton.tick (dt);

    const int loaded = glitchkid::FactoryPresets::indexOfName (glitchProcessor.getPresetName());
    mascot.setLook (0.0f);
    for (auto* card : profileCards)
    {
        card->setMotion (motionOn);
        const int pi = card->getProfile();
        if (pi >= 0)
            card->bind (pi, pi == browsedProfile, pi == loaded);
        card->tick (dt);
    }

    // TUNE: the key summary and the selected tuning style are read from the real parameters (no detection is claimed)
    {
        const auto all = glitchProcessor.captureSoundState();
        const int tstyle = glitchkid::macros::readTuneStyle (all);
        for (int i = 0; i < glitchkid::macros::numTuneStyles; ++i)
            styleButtons[i].setToggleState (i == tstyle, juce::dontSendNotification);
        juce::String key, scale;
        if (auto* k = dynamic_cast<juce::AudioParameterChoice*> (glitchProcessor.apvts.getParameter (GlitchKidProcessor::tuneKeyId)))
            key = k->getCurrentChoiceName();
        if (auto* s = dynamic_cast<juce::AudioParameterChoice*> (glitchProcessor.apvts.getParameter (GlitchKidProcessor::tuneScaleId)))
            scale = s->getCurrentChoiceName();
        const bool on = all.get ("tune_on", 0.0f) >= 0.5f;
        const auto summary = on ? "YOUR KEY: " + key + " " + scale + "  |  STYLE: " + (tstyle >= 0 ? juce::String (glitchkid::macros::tuneStyleLabel ((glitchkid::macros::TuneStyle) tstyle)) : juce::String ("CUSTOM"))
                                : juce::String ("TUNING IS OFF: your voice is not being pulled to any key");
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
        compareCaption.setText (dry ? "HEARING: DRY ORIGINAL" : "HEARING: PROCESSED", juce::dontSendNotification);
        compareCaption.setColour (juce::Label::textColourId, dry ? ui::pink : ui::lime);
        motionButton.setButtonText (motionOn ? "MOTION: ON" : "MOTION: OFF");
    }

    // the title follows the page (no scrambling: it is simply the current name)
    const auto target = currentTitle();
    if (target != titleShown)
    {
        titleShown = target;
        repaint();
    }
}

void GlitchKidEditor::showStatus (const juce::String& text)
{
    presetStatus.setText (text, juce::dontSendNotification);
    statusTicks = 30 * 5; // about five seconds
}

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
    autoStatus.setTooltip (lastProposal.evidence.joinIntoString ("\n") + "\n" + lastProposal.unavailable.joinIntoString ("\n"));
    showStatus (lastProposal.message);
    shownPresetText = {};
    refreshPresetBar();
    refreshProfiles();
    mascot.cheer();
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
    auto mood = ui::Mascot::Mood::idle;
    if (listening)
    {
        ring = ui::AutoState::working;
        ringProgress = an.liveActiveSeconds() / glitchkid::analysis::VocalAnalyzer::maxActiveSeconds;
        mood = ui::Mascot::Mood::listening;
        text = "Listening to the vocal: " + juce::String (an.liveActiveSeconds(), 1) + " s heard (needs 2 s, stops at 8 s). Press BUILD NOW to stop early.";
    }
    else if (autoWaiting)
    {
        ring = ui::AutoState::waiting;
        text = "Waiting for audio. Press play in your host so the vocal reaches GlitchKid.";
    }
    else if (buildState == BuildState::built)
    {
        ring = ui::AutoState::ready;
        mood = ui::Mascot::Mood::ready;
        text = buildMessage + " Audition with DRY / PROCESSED; WHAT CHANGED shows why; BACK TO MY SOUND undoes it.";
    }
    else if (buildState == BuildState::failed)
    {
        ring = ui::AutoState::error;
        mood = ui::Mascot::Mood::error;
        text = buildMessage;
    }
    else
        text = "Pick a profile (or none for a clean start), press BUILD MY VOCAL MIX, then play the vocal. It works without the beat.";

    // what GlitchKid can really hear right now (never claims an analysis that did not happen)
    {
        using SS = GlitchKidProcessor::SourceStatus;
        const auto st = glitchProcessor.getInstrumentalStatus();
        const bool beatAnalysed = glitchProcessor.getInstrumentalAnalyzer().fetch().status == S::ready;
        const auto leadStatus = an.fetch().status;
        const juce::String lead = listening ? "LISTENING" : leadStatus == S::ready ? "ANALYSED" : "NOT ANALYSED YET";
        const juce::String beat = st == SS::notConnected ? "NOT CONNECTED (optional)" : st == SS::connectedSilent ? "ON, BUT NO SIGNAL"
                                                                                       : beatAnalysed ? "LIVE, ANALYSED" : "LIVE, NOT ANALYSED YET";
        const auto line = "VOCAL: " + lead + "   |   INSTRUMENTAL: " + beat;
        if (line != sourceLine.getText())
            sourceLine.setText (line, juce::dontSendNotification);
        sourceLine.setColour (juce::Label::textColourId, st == SS::live ? ui::lime : ui::textDim);
    }
    autoAnalyzeButton.setState (ring, ringProgress);
    mascot.setMood (mood);
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
            detailDesc.setText (in.description, juce::dontSendNotification);
            detailSettings.setText ("WHAT IT DOES: " + juce::String (in.settings), juce::dontSendNotification);
            detailStart.setText ("START WITH: " + juce::String (in.startWith), juce::dontSendNotification);
        }
        else
        {
            detailDesc.setText ("Choose a starting profile below.", juce::dontSendNotification);
            detailSettings.setText ("Each one is an original starting point. Tap a card to read about it, LOAD PROFILE to hear it as saved, or press BUILD MY VOCAL MIX to fit it to your vocal.",
                                    juce::dontSendNotification);
            detailStart.setText (loaded >= 0 ? "LOADED NOW: " + juce::String (glitchkid::FactoryPresets::info (loaded).name) : juce::String(), juce::dontSendNotification);
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
    mascot.cheer();
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
        abCopyAB.setEnabled (hasA);
        abCopyBA.setEnabled (hasB);
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
    const float inPeak = glitchProcessor.consumeInputPeak();
    inMeter.pushPeak (inPeak);
    refreshState (inPeak);
    outMeter.pushPeak (glitchProcessor.consumeOutputPeak());
    grMeter.pushReduction (glitchProcessor.consumeGainReductionDb());

    const bool synced = glitchProcessor.apvts.getRawParameterValue (GlitchKidProcessor::echoSyncId)->load() >= 0.5f;
    echoKnobs[0]->setEnabled (! synced);
    echoDivBox.setEnabled (synced);
    echoDivLabel.setAlpha (synced ? 1.0f : 0.4f);

    // Pitch Tune read-out: the sung note, how far off it is, and the note it is pulled to.
    juce::String tuneText = "Pitch Tune is off";
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
}

// ---- painting ---------------------------------------------------------------------------------------------------------------------------

void GlitchKidEditor::paint (juce::Graphics& g)
{
    constexpr float W = (float) designW, H = (float) designH;
    g.fillAll (ui::obsidian);
    g.addTransform (juce::AffineTransform::scale (scaleFactor));

    // atmosphere behind the hull
    juce::ColourGradient haze (ui::violet.withAlpha (0.22f), 140.0f, 30.0f, ui::violet.withAlpha (0.0f), 800.0f, 420.0f, true);
    g.setGradientFill (haze);
    g.fillRect (0, 0, designW, designH);

    // the hull: a dark body with violet edge lighting and a light strip along the top
    const juce::Rectangle<float> body (6.0f, 6.0f, W - 12.0f, H - 12.0f);
    juce::ColourGradient bodyFill (ui::panelTop, 0.0f, body.getY(), ui::panelBottom.darker (0.3f), 0.0f, body.getBottom(), false);
    g.setGradientFill (bodyFill);
    g.fillRoundedRectangle (body, 40.0f);
    g.setColour (ui::violet.withAlpha (0.6f));
    g.drawRoundedRectangle (body.reduced (0.5f), 40.0f, 2.0f);
    juce::ColourGradient edge (ui::violet.withAlpha (0.0f), 60.0f, 0.0f, ui::violet.withAlpha (0.0f), W - 60.0f, 0.0f, false);
    edge.addColour (0.3, ui::violet);
    edge.addColour (0.5, ui::neon);
    edge.addColour (0.7, ui::violet);
    g.setGradientFill (edge);
    g.fillRect (60.0f, 9.0f, W - 120.0f, 2.0f);

    // the glass screen: deep violet glass with fine scanlines and a bezel
    const auto s = screenRect.toFloat();
    juce::ColourGradient glass (juce::Colour (0xff24103a), s.getCentreX(), s.getY(), juce::Colour (0xff0c0710), s.getCentreX(), s.getBottom(), false);
    g.setGradientFill (glass);
    g.fillRoundedRectangle (s, 26.0f);
    g.setColour (juce::Colours::black.withAlpha (0.16f));
    for (float y = s.getY() + 3.0f; y < s.getBottom() - 3.0f; y += 3.0f)
        g.fillRect (s.getX() + 8.0f, y, s.getWidth() - 16.0f, 1.0f);
    g.setColour (juce::Colour (0xff3a3a4c));
    g.drawRoundedRectangle (s, 26.0f, 1.5f);

    // the title: simply the current name
    const bool tall = page != Page::modify;
    const juce::Rectangle<float> titleRect (screenRect.getX() + 24.0f, screenRect.getY() + (tall ? 50.0f : 56.0f), 740.0f, tall ? 58.0f : 40.0f);
    const float titleSize = tall ? juce::jlimit (28.0f, 46.0f, 720.0f / ((float) juce::jmax (1, titleShown.length()) * 0.64f)) : 32.0f;
    g.setFont (ui::fontOf (titleSize, true));
    g.setColour (ui::violet.withAlpha (0.55f));
    g.drawText (titleShown, titleRect.translated (2.0f, 2.0f), juce::Justification::centredLeft, false);
    g.setColour (juce::Colour (0xfff6ffd0));
    g.drawText (titleShown, titleRect, juce::Justification::centredLeft, false);

    // the brand plate across the lower edge of the tall screen, flanked by vents
    if (tall)
    {
        const juce::Rectangle<float> plate (W * 0.5f - 120.0f, s.getBottom() - 10.0f, 240.0f, 36.0f);
        juce::ColourGradient rim (ui::violet, plate.getX(), plate.getY(), ui::neon, plate.getRight(), plate.getBottom(), false);
        g.setGradientFill (rim);
        g.fillPath (ui::makePlatePath (plate, false));
        juce::ColourGradient inner (juce::Colour (0xff23232f), 0.0f, plate.getY(), juce::Colour (0xff10101a), 0.0f, plate.getBottom(), false);
        g.setGradientFill (inner);
        g.fillPath (ui::makePlatePath (plate.reduced (2.0f), false));
        g.setColour (ui::lime);
        g.setFont (ui::fontOf (22.0f, true));
        g.drawText ("GLITCHKID", plate, juce::Justification::centred, false);
        for (int side = 0; side < 2; ++side)
        {
            const float vx = side == 0 ? plate.getX() - 120.0f : plate.getRight() + 50.0f;
            g.setColour (juce::Colour (0xff05050a));
            g.fillRoundedRectangle (vx, plate.getY() + 8.0f, 70.0f, 20.0f, 4.0f);
            g.setColour (juce::Colour (0xff2b2b3a));
            for (int i = 0; i < 9; ++i)
                g.drawLine (vx + 4.0f + (float) i * 7.5f, plate.getY() + 26.0f, vx + 12.0f + (float) i * 7.5f, plate.getY() + 10.0f, 2.0f);
        }
    }

    // the rounded panel(s) behind the current page's controls
    for (const auto& r : panelRects)
    {
        const auto f = r.toFloat();
        juce::ColourGradient fill (ui::panelTop.withAlpha (0.92f), f.getX(), f.getY(), ui::panelBottom.withAlpha (0.92f), f.getX(), f.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (f, 16.0f);
        g.setColour (ui::border);
        g.drawRoundedRectangle (f.reduced (0.5f), 16.0f, 1.2f);
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

    // Hide every control that is not part of the always-visible screen frame and preset bar; the current page then shows what it needs.
    const std::vector<juce::Component*> persistent = { &mascot, &motionButton, &navButtons[0], &navButtons[1], &navButtons[2], &presetPrevButton, &presetMenuButton,
                                                       &presetNextButton, &presetSaveButton, &presetDeleteButton, &abSetA, &abSetB, &abRecallA, &abRecallB,
                                                       &abCopyAB, &abCopyBA, &abLabel, &presetStatus };
    for (auto* child : content.getChildren())
        if (std::find (persistent.begin(), persistent.end(), child) == persistent.end())
            child->setVisible (false);
    panelRects.clear();

    // ---- the glass screen, with the step tabs and the mascot ----
    screenRect = page == Page::modify ? Rect (40, 26, 980, 104) : page == Page::tune ? Rect (40, 26, 980, 296) : Rect (40, 26, 980, 316);
    for (int i = 0; i < 3; ++i)
        show (navButtons[i], { screenRect.getX() + 18 + i * 150, screenRect.getY() + 14, 144, 38 });
    show (motionButton, { page == Page::modify ? screenRect.getRight() - 290 : screenRect.getRight() - 150, screenRect.getY() + 16, 120, 30 });
    mascotBase = page == Page::autoMix ? Rect (878, 76, 104, 104) : page == Page::tune ? Rect (806, 96, 176, 176) : Rect (894, 30, 98, 98);
    show (mascot, mascotBase);

    // ---- bottom bar: presets and A/B (always available) ----
    Rect bar (16, 662, designW - 32, 30);
    show (presetPrevButton, bar.removeFromLeft (28));
    bar.removeFromLeft (4);
    show (presetMenuButton, bar.removeFromLeft (290));
    bar.removeFromLeft (4);
    show (presetNextButton, bar.removeFromLeft (28));
    bar.removeFromLeft (6);
    show (presetSaveButton, bar.removeFromLeft (64));
    bar.removeFromLeft (6);
    show (presetDeleteButton, bar.removeFromLeft (76));
    bar.removeFromLeft (12);
    show (abRecallB, bar.removeFromRight (46));
    bar.removeFromRight (6);
    show (abRecallA, bar.removeFromRight (46));
    bar.removeFromRight (10);
    show (abSetB, bar.removeFromRight (62));
    bar.removeFromRight (6);
    show (abSetA, bar.removeFromRight (62));
    bar.removeFromRight (8);
    show (abLabel, bar.removeFromRight (38));
    show (presetStatus, bar);

    // ---- step 1: TUNE ----
    if (page == Page::tune)
    {
        const int sx = screenRect.getX() + 24, sy = screenRect.getY();
        show (tuneSubline, { sx, sy + 100, 740, 22 });
        for (int i = 0; i < noteKeys.size(); ++i)
            show (*noteKeys[i], { sx + i * 56, sy + 134, 54, 52 });
        for (int i = 0; i < 6; ++i)
            show (scaleChips[i], { sx + i * 112, sy + 196, 108, 32 });
        show (tuneOnButton, { sx, sy + 240, 110, 30 });
        for (int i = 0; i < glitchkid::macros::numTuneStyles; ++i)
            show (styleButtons[i], { sx + 134 + i * 124, sy + 240, 118, 30 });
        show (tuneSummary, { 150, 354, 760, 22 });
        show (scope, { 150, 382, 760, 40 });
        show (tuneReadout, { 150, 428, 760, 30 });
        show (nextButton, { 380, 476, 300, 64 });
    }

    // ---- step 2: AUTO ----
    else if (page == Page::autoMix)
    {
        show (detailDesc, { 64, 118, 700, 22 });
        show (detailSettings, { 64, 146, 700, 40 });
        show (detailStart, { 64, 190, 700, 38 });
        show (autoStatus, { 64, 236, 660, 42 });
        show (sourceLine, { 64, 286, 660, 16 });
        show (scope, { 64, 306, 660, 14 });
        show (autoStyleLabel, { 744, 190, 266, 14 });
        show (autoAnalyzeButton, { 744, 210, 112, 112 });
        show (autoIntensitySlider, { 862, 222, 148, 24 });
        show (autoApplyButton, { 862, 256, 148, 34 });
        for (int i = 0; i < profileCards.size(); ++i)
            show (*profileCards[i], { 40 + (i % 4) * 247, 372 + (i / 4) * 78, 239, 70 });
        show (loadButton, { 40, 540, 200, 44 });
        show (backToMineButton, { 246, 540, 200, 44 });
        show (dryButton, { 460, 540, 108, 44 });
        show (processedButton, { 572, 540, 128, 44 });
        show (compareCaption, { 706, 540, 150, 44 });
        show (modifyButton, { 864, 540, 156, 44 });
    }

    // ---- step 3: MODIFY (one category at a time) ----
    else
    {
        constexpr int oy = 16;
        const int catW = (designW - 80 - 3 * 8) / numCategories;
        for (int i = 0; i < numCategories; ++i)
        {
            show (categoryButtons[i], { 40 + i * (catW + 8), 138, catW, 38 });
            categoryButtons[i].setToggleState (i == category, juce::dontSendNotification);
        }
        panelRects.push_back ({ 16, 184, designW - 32, 372 });

        // quick tweaks + footer
        show (talkCaption, { 24, 562, 150, 32 });
        for (int i = 0; i < glitchkid::macros::numCommands; ++i)
            show (commandButtons[i], { 172 + i * 156, 560, 148, 36 });
        show (backButton, { 16, 606, 200, 44 });
        show (resetButton, { 226, 606, 110, 44 });
        show (savePresetButton, { 346, 606, 160, 44 });
        show (dryButton, { 560, 606, 120, 44 });
        show (processedButton, { 680, 606, 150, 44 });
        show (compareCaption, { 840, 606, 204, 44 });

        switch (category)
        {
            case 0: // PITCH & KEY
                show (tuneOnCaption, { 30, 178 + oy, 100, 16 });
                show (tuneOnButton, { 30, 196 + oy, 100, 38 });
                show (tuneKeyLabel, { 152, 178 + oy, 120, 16 });
                show (tuneKeyBox, { 152, 196 + oy, 140, 38 });
                show (tuneScaleLabel, { 312, 178 + oy, 150, 16 });
                show (tuneScaleBox, { 312, 196 + oy, 220, 38 });
                show (styleCaption, { 556, 178 + oy, 200, 16 });
                for (int i = 0; i < glitchkid::macros::numTuneStyles; ++i)
                    show (styleButtons[i], { 556 + i * 156, 196 + oy, 148, 38 });
                for (int i = 0; i < tuneKnobs.size(); ++i)
                    showK (*tuneKnobs[i], { 30 + i * 190, 262 + oy, 180, 210 });
                show (tuneSummary, { 430, 290 + oy, 680, 36 });
                show (tuneReadout, { 430, 340 + oy, 680, 40 });
                break;

            case 1: // TONE
                show (toneTitle, { 30, 178 + oy, 500, 16 });
                toneTitle.setText ("EQ AND WARMTH", juce::dontSendNotification);
                for (int i = 0; i < toneKnobs.size(); ++i)
                    showK (*toneKnobs[i], { 40 + i * 210, 210 + oy, 190, 240 });
                break;

            case 2: // DYNAMICS
                show (compTitle, { 30, 176 + oy, 520, 18 });
                for (int i = 0; i < compKnobs.size(); ++i)
                    showK (*compKnobs[i], { 30 + (i % 3) * 180, 200 + (i / 3) * 168 + oy, 170, 160 });
                show (grMeter, { 580, 200 + oy, 28, 300 });
                show (grLabel, { 574, 504 + oy, 40, 16 });
                showK (*gainKnob[0], { 650, 220 + oy, 200, 220 });
                show (inMeter, { 890, 200 + oy, 28, 300 });
                show (outMeter, { 940, 200 + oy, 28, 300 });
                show (inLabel, { 884, 504 + oy, 40, 16 });
                show (outLabel, { 934, 504 + oy, 40, 16 });
                break;

            default: // SPACE
                show (echoTitle, { 30, 176 + oy, 140, 18 });
                show (echoOnButton, { 30, 198 + oy, 100, 30 });
                show (echoSyncButton, { 30, 234 + oy, 100, 30 });
                show (echoDivLabel, { 150, 198 + oy, 130, 14 });
                show (echoDivBox, { 150, 216 + oy, 130, 32 });
                for (int i = 0; i < echoKnobs.size(); ++i)
                    showK (*echoKnobs[i], { 310 + i * 180, 190 + oy, 160, 108 });
                show (dimTitle, { 30, 308 + oy, 260, 18 });
                show (dimOnButton, { 30, 330 + oy, 100, 30 });
                for (int i = 0; i < dimKnobs.size(); ++i)
                    showK (*dimKnobs[i], { 310 + i * 180, 306 + oy, 160, 108 });
                show (dblTitle, { 30, 426 + oy, 200, 18 });
                show (dblOnButton, { 30, 448 + oy, 100, 30 });
                for (int i = 0; i < dblKnobs.size(); ++i)
                    showK (*dblKnobs[i], { 310 + i * 180, 422 + oy, 160, 108 });
                break;
        }
    }
}
