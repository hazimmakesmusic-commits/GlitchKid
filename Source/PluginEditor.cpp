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
    g.setColour (juce::Colour (0xff0c0910));
    g.fillRect (r);
    g.setColour (ui::border);
    g.drawRect (r, 1.0f);
    const float h = r.getHeight() * juce::jmap (shownDb, minDb, maxDb, 0.0f, 1.0f);
    juce::ColourGradient grad (ui::pink, 0.0f, r.getY(), ui::violet, 0.0f, r.getBottom(), false);
    g.setGradientFill (grad);
    if (shownDb > 0.0f)
        g.setColour (ui::errorRed);
    g.fillRect (r.withTop (r.getBottom() - h).reduced (2.0f, 0.0f));
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
    g.setColour (juce::Colour (0xff0c0910));
    g.fillRect (r);
    g.setColour (ui::border);
    g.drawRect (r, 1.0f);
    g.setColour (ui::pink);
    g.fillRect (r.withHeight (r.getHeight() * juce::jlimit (0.0f, 1.0f, shownDb / maxDb)).reduced (2.0f, 0.0f));
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
    page = (Page) juce::jlimit (0, 4, p.getUiInt ("uiLevel", 0)); // first launch: HOME
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

    // ---- the persistent bar: back to the levels, the stage (drawn in paint), the real vocal / output levels, dry / processed, undo, motion ----
    static const char* navText[5] = { "HOME", "CLEAN LAB", "BEAT LOCK", "BUILD MY MIX", "FINAL TOUCH" };
    static const char* navTip[5] = { "The session: the real-time signal and shortcuts.", "Corrective EQ, resonance suppression and de-essing for the vocal itself.",
                                     "How the vocal sits against your beat (needs the beat on the Instrumental input).", "Pick a starting profile and build a starting mix from your vocal.",
                                     "Pitch & key, tone, dynamics and space, control by control." };
    for (int i = 0; i < 5; ++i)
    {
        navButtons[i].setButtonText (navText[i]);
        style (navButtons[i], juce::String (navTip[i]) + " Any workspace can be opened at any time; switching never changes your sound.");
        navButtons[i].onClick = [this, i] { showPage ((Page) i, true); };
    }
    style (undoButton, "Put back the sound you had before the last build, profile load or BEAT LOCK change.");
    undoButton.onClick = [this] { backToMine(); };
    undoButton.setEnabled (false);
    label (ioLabel, 11.0f, true, ui::textDim);
    ioLabel.setTooltip ("The real level of the vocal reaching GlitchKid and of the output (digital peaks). CLIP appears when the output really reached full scale.");
    motionButton.setButtonText ("MOTION");
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
    processedButton.setColour (juce::TextButton::buttonOnColourId, ui::violet);
    dryButton.onClick = [this] { setBypassFromUi (true); };
    processedButton.onClick = [this] { setBypassFromUi (false); };
    content.addAndMakeVisible (chain);
    chain.onStage = [this] (int stage)
    {
        showCategory (stage, true);
        showPage (Page::finalTouch, true);
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

    // ---- FINAL TOUCH navigation and actions ----
    static const char* catText[numCategories] = { "PITCH & KEY", "TONE", "DYNAMICS", "SPACE" };
    static const char* catTip[numCategories] = { "Pitch Tune: on/off, key, scale, tuning style, strength and speed.", "A darker-to-brighter control, four EQ gains and warmth (saturation).",
                                                 "A softer-to-more-upfront control, the compressor and the output level.", "A dry-to-spacious control, echo, width (Dimension) and the Doubler." };
    for (int i = 0; i < numCategories; ++i)
    {
        categoryButtons[i].setButtonText (catText[i]);
        style (categoryButtons[i], catTip[i]);
        categoryButtons[i].onClick = [this, i] { showCategory (i, true); };
    }
    style (finalButton, "Refine the sound, one category at a time.");
    finalButton.onClick = [this] { showPage (Page::finalTouch, true); };
    style (backButton, "Back to the profiles and BUILD MY VOCAL MIX.");
    backButton.onClick = [this] { showPage (Page::build, true); };
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
    label (tuneReadout, 14.0f, true, ui::pink);
    tuneOnButton.setColour (juce::ToggleButton::tickColourId, ui::violet);
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
        content.addAndMakeVisible (*l);
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
    eqGraph = std::make_unique<ui::EqGraph> (p.apvts, [&p] { return p.getSampleRate(); });
    content.addAndMakeVisible (*eqGraph);
    eqGraph->onBandSelected = [this] (int b) { selectBand (b); };
    static const char* bandNames[ui::EqGraph::numBands] = { "LOW", "BODY", "PRESENCE", "AIR" };
    static const char* bandTips[ui::EqGraph::numBands] = { "Low shelf: more weight or less boom.", "Bell: cut a little if the vocal sounds muddy or boxy.",
                                                         "Bell: the words come forward.", "High shelf: brightness and breath. Lower it if the S sounds are harsh." };
    for (int b = 0; b < ui::EqGraph::numBands; ++b)
    {
        bandButtons[b].setButtonText (bandNames[b]);
        style (bandButtons[b], juce::String (bandTips[b]) + " Select it to edit its frequency, gain and Q.");
        bandButtons[b].onClick = [this, b] { selectBand (b); };
        const auto& ids = GlitchKidProcessor::eqIds[b];
        eqKnobs[b].add (new ParamKnob (p.apvts, ids.freq, "FREQ", " Hz", ui::violet, "The band's centre or corner frequency."));
        eqKnobs[b].add (new ParamKnob (p.apvts, ids.gain, "GAIN", " dB", ui::violet, "Boost or cut at this band."));
        if (ids.q != nullptr)
            eqKnobs[b].add (new ParamKnob (p.apvts, ids.q, "Q", "", ui::violet, "How narrow the bell is. Higher = narrower."));
        for (auto* k : eqKnobs[b])
            k->addTo (content);
    }
    warmthKnob.add (new ParamKnob (p.apvts, GlitchKidProcessor::satDriveId, "WARMTH", " dB", ui::violet, "Saturation: a little warmth and grit. 0 = clean."));
    warmthKnob[0]->addTo (content);
    label (eqNote, 11.0f, false, ui::textDim, juce::Justification::topLeft);
    eqNote.setText ("Violet: your EQ. Pink: what CLEAN LAB is applying. Both are calculated from real settings; there is no live spectrum.", juce::dontSendNotification);

    // AIR LAB strip
    auto airSlider = [this] (juce::Slider& sl, const juce::String& tip, int box)
    {
        sl.setSliderStyle (juce::Slider::LinearHorizontal);
        sl.setTextBoxStyle (juce::Slider::TextBoxRight, false, box, 20);
        sl.setColour (juce::Slider::trackColourId, ui::violet);
        sl.setTooltip (tip);
        content.addAndMakeVisible (sl);
    };
    airSlider (airPresenceSlider, "PRESENCE: lifts the 2.5-5 kHz detail that makes words come forward. Reduced automatically on sibilant S sounds.", 52);
    airSlider (airAirSlider, "AIR: openness above 9 kHz: a gentle lift plus new harmonics made from the presence band. Reduced automatically on sibilant S sounds.", 52);
    airSlider (airMixSlider, "MIX: how much of the added presence and air is blended in. 0 % = your signal untouched.", 52);
    airSlider (airTrimSlider, "OUTPUT TRIM: compensates for the loudness change (applies while AIR LAB is on).", 60);
    airPresenceSlider.setDoubleClickReturnValue (true, 30.0);
    airAirSlider.setDoubleClickReturnValue (true, 30.0);
    airMixSlider.setDoubleClickReturnValue (true, 100.0);
    airTrimSlider.setDoubleClickReturnValue (true, 0.0);
    airPresenceAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, GlitchKidProcessor::airPresenceId, airPresenceSlider);
    airAirAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, GlitchKidProcessor::airAirId, airAirSlider);
    airMixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, GlitchKidProcessor::airMixId, airMixSlider);
    airTrimAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, GlitchKidProcessor::airTrimId, airTrimSlider);
    airPresenceSlider.setTextValueSuffix (" %");
    airAirSlider.setTextValueSuffix (" %");
    airMixSlider.setTextValueSuffix (" %");
    airTrimSlider.setTextValueSuffix (" dB");
    airOnButton.setTooltip ("Switch AIR LAB on or off. Off = your signal passes through untouched (a click-free 20 ms crossfade). Use it to compare.");
    airLinkButton.setTooltip ("LINK: moving PRESENCE also moves AIR, and the reverse.");
    airOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, GlitchKidProcessor::airOnId, airOnButton);
    airLinkAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, GlitchKidProcessor::airLinkId, airLinkButton);
    for (auto* t : { &airOnButton, &airLinkButton })
    {
        t->setColour (juce::ToggleButton::tickColourId, ui::violet);
        content.addAndMakeVisible (*t);
    }
    for (auto* l : { &airPresenceLabel, &airAirLabel, &airMixLabel, &airTrimLabel })
        label (*l, 11.0f, true, ui::textDim);
    label (airHint, 11.0f, false, ui::textDim);
    airHint.setText ("Presence and air that ease off on S sounds. Zero latency.", juce::dontSendNotification);
    // ---- CLEAN (category 5) ----
    auto cleanSlider = [this] (juce::Slider& sl, const juce::String& tip, int box)
    {
        sl.setSliderStyle (juce::Slider::LinearHorizontal);
        sl.setTextBoxStyle (juce::Slider::TextBoxRight, false, box, 20);
        sl.setColour (juce::Slider::trackColourId, ui::violet);
        sl.setTextValueSuffix (" %");
        sl.setTooltip (tip);
        content.addAndMakeVisible (sl);
    };
    cleanSlider (cleanAmountSlider, "CLEAN: how much cleanup is allowed. 0 = CLEAN LAB does nothing. It never exceeds 3.5 dB on the low-mids, 6 dB per resonance or 9 dB of de-essing.", 56);
    cleanSlider (cleanToneSlider, "Corrective EQ: scales the rumble and low-mid corrections (0 = off).", 52);
    cleanSlider (cleanResSlider, "Resonance: scales the narrow resonance cuts (0 = off).", 52);
    cleanSlider (cleanEssSlider, "De-esser: scales the S-sound reduction (0 = off).", 52);
    cleanAmountAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, GlitchKidProcessor::cleanAmountId, cleanAmountSlider);
    cleanToneAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, GlitchKidProcessor::cleanToneId, cleanToneSlider);
    cleanResAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, GlitchKidProcessor::cleanResId, cleanResSlider);
    cleanEssAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, GlitchKidProcessor::cleanEssId, cleanEssSlider);
    cleanAmountSlider.setDoubleClickReturnValue (true, 50.0);
    for (auto* sl : { &cleanToneSlider, &cleanResSlider, &cleanEssSlider })
        sl->setDoubleClickReturnValue (true, 100.0);
    cleanOnButton.setTooltip ("Switch CLEAN LAB on or off (a click-free 20 ms crossfade). Off = your signal passes through untouched. It learns your voice while it plays, so corrections appear after a few seconds.");
    cleanOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, GlitchKidProcessor::cleanOnId, cleanOnButton);
    cleanOnButton.setColour (juce::ToggleButton::tickColourId, ui::violet);
    content.addAndMakeVisible (cleanOnButton);
    label (cleanAmountLabel, 12.0f, true, ui::textBright);
    for (auto* l : { &toneHead, &resHead, &essHead })
        content.addAndMakeVisible (*l);
    for (auto* l : { &toneTrimLabel, &resTrimLabel, &essTrimLabel })
        label (*l, 11.0f, true, ui::textDim);
    for (auto* l : { &toneRead[0], &toneRead[1], &resRead[0], &resRead[1], &resRead[2], &essRead, &cleanLearn, &cleanLevel })
        label (*l, 12.0f, false, ui::textBright);
    label (cleanHint, 11.5f, false, ui::textDim, juce::Justification::topLeft);
    cleanHint.setText ("CLEAN LAB measures your vocal while it plays and only acts on what it finds: rumble, a low-mid build-up, narrow resonances that persist while the voice moves, and S sounds. "
                       "It sits after your EQ and before the compressor (the de-esser after it). Switch it off and on to hear before and after.", juce::dontSendNotification);
    cleanLevel.setTooltip ("The level change CLEAN LAB is making right now. Use OUTPUT TRIM or Gain to compare at the same loudness; a quieter version can sound worse for that reason alone.");
    style (cleanResetButton, "Put CLEAN LAB back to its defaults (off, 50 %, all sections at 100 %). Your other settings are not touched.");
    cleanResetButton.onClick = [this]
    {
        for (const char* id : { GlitchKidProcessor::cleanOnId, GlitchKidProcessor::cleanAmountId, GlitchKidProcessor::cleanToneId, GlitchKidProcessor::cleanResId, GlitchKidProcessor::cleanEssId })
            if (auto* prm = glitchProcessor.apvts.getParameter (id))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (prm->getDefaultValue());
                prm->endChangeGesture();
            }
        showStatus ("CLEAN LAB reset to its defaults.");
    };
    content.addAndMakeVisible (cleanAdvButton);
    cleanAdvButton.setColour (juce::ToggleButton::tickColourId, ui::violet);
    cleanAdvButton.setTooltip ("Show the three section trims (corrective EQ, resonance, de-esser). They keep their values when hidden.");
    cleanAdvanced = glitchProcessor.getUiFlag ("uiCleanAdv", false);
    cleanAdvButton.setToggleState (cleanAdvanced, juce::dontSendNotification);
    cleanAdvButton.onClick = [this]
    {
        cleanAdvanced = cleanAdvButton.getToggleState();
        glitchProcessor.setUiFlag ("uiCleanAdv", cleanAdvanced);
        layoutContent();
    };
    content.addAndMakeVisible (essBar);
    airPresenceSlider.onValueChange = [this] { linkAir (airPresenceSlider, airAirSlider); };
    airAirSlider.onValueChange = [this] { linkAir (airAirSlider, airPresenceSlider); };
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compThresholdId, "THRESHOLD", " dB", ui::violet, "The level above which the compressor starts working. Lower = more compression."));
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compRatioId, "RATIO", ":1", ui::violet, "How firmly the level is held down above the threshold. 1:1 = off."));
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compAttackId, "ATTACK", " ms", ui::violet, "How quickly it reacts. Slower lets the start of words through."));
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compReleaseId, "RELEASE", " ms", ui::violet, "How quickly it lets go after a loud word."));
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compMakeupId, "MAKEUP", " dB", ui::violet, "Makes the compressed vocal louder again."));
    for (auto* k : compKnobs)
        k->addTo (content);
    gainKnob.add (new ParamKnob (p.apvts, GlitchKidProcessor::gainId, "OUTPUT GAIN", " dB", ui::pink, "The final level of the vocal."));
    gainKnob[0]->addTo (content);
    for (auto* l : { &inLabel, &outLabel, &grLabel })
        label (*l, 11.0f, true, ui::textDim, juce::Justification::centred);
    addAll ({ &inMeter, &outMeter, &grMeter });
    choices (echoDivBox, GlitchKidProcessor::echoDivId);
    echoDivBox.setTooltip ("The echo's length as a note value of the host tempo.");
    for (auto* t : { &echoOnButton, &echoSyncButton, &dimOnButton, &dblOnButton })
        t->setColour (juce::ToggleButton::tickColourId, ui::violet);
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
    label (echoDivLabel, 11.0f, true, ui::textDim);
    for (auto* l : { &echoTitle, &dimTitle, &dblTitle })
        content.addAndMakeVisible (*l);
    addAll ({ &echoOnButton, &echoSyncButton, &echoDivBox, &dimOnButton, &dblOnButton });

    // ---- HOME / SESSION ----
    {
        static const char* names[4] = { "CLEAN LAB", "BEAT LOCK", "BUILD MY VOCAL MIX", "FINAL TOUCH" };
        static const char* descs[4] = {
            "Rumble, mud, harsh resonances and sharp S sounds, found while your vocal plays.",
            "How your vocal sits against the beat, with optional, explained changes.",
            "Pick a profile; GlitchKid builds a starting mix from what it hears.",
            "Pitch & key, tone, dynamics and space, control by control." };
        static const int targets[4] = { 1, 2, 3, 4 };
        for (int i = 0; i < 4; ++i)
        {
            tiles[i].setContent (names[i], descs[i], i == 2);
            tiles[i].setStatus ("...", ui::TileStatus::idle);
            tiles[i].setTooltip (juce::String ("Open ") + names[i] + ". The line at the bottom is read from the plug-in's real state.");
            tiles[i].onClick = [this, i] { showPage ((Page) targets[i], true); };
            content.addAndMakeVisible (tiles[i]);
        }
        addAll ({ &homeTitle, &homeSource, &homeView });
        label (homeHint, 11.5f, false, ui::textDim, juce::Justification::centredRight);
        homeView.setTooltip ("Spectrum (log frequency) and waveform of the audio this plug-in really receives (VOCAL INPUT, before processing) and returns (OUTPUT). The instrumental trace appears only "
                             "when a beat is really arriving on the Instrumental input. Nothing is drawn without a signal.");
    }

    // ---- BEAT LOCK ----
    {
        addAll ({ &beatHeading, &suggestHeading, &vocalBar, &beatBar, &bandCompare, &beatView });
        beatView.setTooltip ("Spectrum of the vocal input (violet) and, only while a beat really arrives on the Instrumental input, the beat (grey). Nothing is drawn without a signal.");
        label (beatStatus, 12.5f, false, ui::textBright, juce::Justification::topLeft);
        label (beatLevels, 11.5f, false, ui::textDim);
        label (beatRouting, 11.0f, false, ui::textDim, juce::Justification::topLeft);
        label (beatProgress, 12.0f, true, ui::pink, juce::Justification::centredLeft);
        label (beatChanges, 11.5f, false, ui::textBright, juce::Justification::topLeft);
        label (vocalBarLabel, 11.0f, true, ui::textDim);
        label (beatBarLabel, 11.0f, true, ui::textDim);
        beatRouting.setText ("The beat is heard only on the optional Instrumental input (docs/SOURCES.md); a track that already mixes vocal and beat counts as the vocal. "
                             "BEAT LOCK changes only this vocal's own controls: no beat processing, no mixer faders, no reverb.", juce::dontSendNotification);
        bandCompare.setTooltip ("How much of each frequency range holds the vocal's energy and the beat's energy (five coarse bands, not a spectrum analyser). A marker means the beat is likely to cover the voice there.");
        vocalBar.setTooltip ("The real peak level of this track's input.");
        beatBar.setTooltip ("The real peak level of the Instrumental input. Empty and dimmed while nothing is connected.");
        style (analyzeBeatButton, "Listens to the vocal and the beat for a few seconds (press play in your host and let both play), then compares them. Press again to stop early.");
        style (applyBeatButton, "Apply the ticked suggestions to the vocal's own controls. UNDO puts everything back.");
        style (undoBeatButton, "Put back the sound you had before BEAT LOCK changed it.");
        style (vocalOnlyButton, "No beat to compare with: BUILD MY VOCAL MIX works from the vocal alone.");
        analyzeBeatButton.onClick = [this]
        {
            if (beatListening)
                glitchProcessor.stopSourceAnalysis(); // finish early with what has been heard
            else
                startBeatAnalysis();
        };
        applyBeatButton.onClick = [this] { applyBeatSelection(); };
        undoBeatButton.onClick = [this] { backToMine(); };
        vocalOnlyButton.onClick = [this] { showPage (Page::build, true); };
        for (int i = 0; i < beatRows; ++i)
        {
            beatTicks[i].setColour (juce::ToggleButton::tickColourId, ui::violet);
            beatTicks[i].setTooltip ("Include this suggestion when you press APPLY SELECTED. Untick it to reject it.");
            beatTicks[i].onClick = [this, i]
            {
                if ((size_t) i < beatSelected.size())
                    beatSelected[(size_t) i] = beatTicks[i].getToggleState();
            };
            content.addAndMakeVisible (beatTicks[i]);
            label (beatTitles[i], 12.0f, true, ui::textBright);
            label (beatReasons[i], 11.0f, false, ui::textDim, juce::Justification::topLeft);
        }
    }

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
        glitchProcessor.setUiInt ("uiLevel", (int) page);
    for (int i = 0; i < 5; ++i)
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

// The CLEAN read-outs: every line comes from what the DSP reports it is applying (nothing is shown that was not measured and applied).
void GlitchKidEditor::refreshClean (bool cleanOn)
{
    const auto a = glitchProcessor.getCleanApplied();
    auto setText = [] (juce::Label& l, const juce::String& t, juce::Colour c)
    {
        if (l.getText() != t)
            l.setText (t, juce::dontSendNotification);
        l.setColour (juce::Label::textColourId, c);
    };
    auto kHz = [] (float hz) { return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + " kHz" : juce::String ((int) std::lround (hz)) + " Hz"; };
    auto cut = [&kHz] (float db, float hz) { return "-" + juce::String (db, 1) + " dB at " + kHz (hz); };
    if (! cleanOn)
    {
        for (auto* l : { &toneRead[0], &toneRead[1], &resRead[0], &resRead[1], &resRead[2], &essRead, &cleanLearn, &cleanLevel })
            setText (*l, l == &cleanLearn ? "CLEAN LAB is off: your signal passes through untouched." : juce::String(), ui::textDim);
        essBar.push (0.0f);
        return;
    }
    setText (toneRead[0], a.rumbleAmount > 0.02f ? "Rumble: 70 Hz high-pass, " + juce::String (juce::roundToInt (a.rumbleAmount * 100.0f)) + " % blended in" : "Rumble: none found", a.rumbleAmount > 0.02f ? ui::pink : ui::textDim);
    setText (toneRead[1], a.mudDb > 0.05f ? "Low-mid build-up: " + cut (a.mudDb, a.mudHz) : "Low-mid build-up: none found", a.mudDb > 0.05f ? ui::pink : ui::textDim);
    int shown = 0;
    for (int i = 0; i < CleanLab::numRes; ++i)
        if (a.resDb[i] > 0.05f && shown < 3)
            setText (resRead[shown++], "Resonance: " + cut (a.resDb[i], a.resHz[i]), ui::pink);
    if (shown == 0)
        setText (resRead[shown++], a.learnedSeconds < 3.0f ? "Resonance: still listening to your voice..." : "Resonance: none found", ui::textDim);
    for (int i = shown; i < 3; ++i)
        setText (resRead[i], {}, ui::textDim);
    essBar.push (a.essGrDb);
    setText (essRead, "S sounds: " + (a.essGrDb > 0.05f ? "-" + juce::String (a.essGrDb, 1) + " dB now" : juce::String ("no reduction right now")), a.essGrDb > 0.05f ? ui::pink : ui::textDim);
    setText (cleanLearn, a.learnedSeconds < 3.0f ? "Learning your voice: " + juce::String (a.learnedSeconds, 1) + " s heard (it needs a moving voice)"
                                                  : "Learned from at least 3 s of voiced audio; it keeps adapting slowly", ui::textDim);
    setText (cleanLevel, "Net level change: " + juce::String (a.levelChangeDb, 1) + " dB (compare at matched loudness)", ui::textDim);
}

// With LINK on, moving one of the two main controls by hand moves the other to the same value (host automation never triggers it).
void GlitchKidEditor::linkAir (juce::Slider& moved, juce::Slider& other)
{
    if (airLinkGuard || ! airLinkButton.getToggleState() || ! moved.isMouseOverOrDragging())
        return;
    airLinkGuard = true;
    other.setValue (moved.getValue(), juce::sendNotificationSync);
    airLinkGuard = false;
}

void GlitchKidEditor::selectBand (int b)
{
    eqBand = juce::jlimit (0, ui::EqGraph::numBands - 1, b);
    eqGraph->setSelectedBand (eqBand);
    layoutContent();
}

void GlitchKidEditor::applyContentTransform (float dx)
{
    content.setTransform (juce::AffineTransform::scale (scaleFactor).translated (dx * scaleFactor, 0.0f));
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
    {
        int targetClass = -1; // the note Pitch Tune is really pulling the voice to (real detector output; none while it is off)
        if (glitchProcessor.apvts.getRawParameterValue (GlitchKidProcessor::tuneOnId)->load() >= 0.5f && glitchProcessor.getTuneTargetMidi() >= 0.0f)
            targetClass = ((int) std::lround (glitchProcessor.getTuneTargetMidi()) % 12 + 12) % 12;
        for (int i = 0; i < noteKeys.size(); ++i)
            noteKeys[i]->setTarget (i == targetClass);
    }
    for (auto* k : noteKeys)
        k->tick (dt);
    if (pageFade < 1.0f)
    {
        pageFade = juce::jmin (1.0f, pageFade + 0.2f);
        content.setAlpha (0.3f + 0.7f * pageFade);
        applyContentTransform (pageFade < 1.0f ? (1.0f - pageFade) * 8.0f * ((int) (pageFade * 10.0f) % 2 == 0 ? 1.0f : -1.0f) : 0.0f); // a very short horizontal glitch
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
        const bool cleanOn = all.get ("clean_on", 0.0f) >= 0.5f && all.get ("clean_amount", 0.0f) > 0.05f;
        const auto ca = glitchProcessor.getCleanApplied();
        const bool cleanTone = cleanOn && (ca.rumbleAmount > 0.02f || ca.mudDb > 0.05f || ca.resDb[0] > 0.05f || ca.resDb[1] > 0.05f || ca.resDb[2] > 0.05f);
        const bool cleanEss = cleanOn && all.get ("clean_ess", 0.0f) > 0.05f;
        if (! dry)
        {
            act[0] = fx::isActive (fx::pitchTune, all);
            act[1] = fx::isActive (fx::eq, all) || fx::isActive (fx::saturation, all) || all.get ("air_on", 0.0f) >= 0.5f || cleanTone;
            act[2] = fx::isActive (fx::compressor, all) || cleanEss;
            act[3] = fx::isActive (fx::echo, all) || fx::isActive (fx::dimension, all) || fx::isActive (fx::doubler, all);
        }
        juce::StringArray legacy; // effects the interface no longer offers but an older session may still have switched on
        for (int e : { (int) fx::formant, (int) fx::harmony, (int) fx::mutation, (int) fx::stutter })
            if (fx::isActive (e, all))
                legacy.add (juce::String (fx::info (e).name).toUpperCase());
        stageActive = act;
        chain.setState (act, peak, outPeak, page == Page::finalTouch ? category : -1, motionOn, legacy.joinIntoString (", "));
        chain.tick (dt);
    }

    {
        const float a = all.get ("air_on", 0.0f) >= 0.5f ? 1.0f : 0.55f; // the controls stay usable while it is off; they just look inactive
        for (auto* c : std::initializer_list<juce::Component*> { &airPresenceSlider, &airAirSlider, &airMixSlider, &airTrimSlider, &airPresenceLabel, &airAirLabel, &airMixLabel, &airTrimLabel })
            c->setAlpha (a);
    }

    {
        const bool cleanOn = all.get ("clean_on", 0.0f) >= 0.5f;
        const float a = cleanOn ? 1.0f : 0.55f;
        for (auto* c : std::initializer_list<juce::Component*> { &cleanAmountSlider, &cleanToneSlider, &cleanResSlider, &cleanEssSlider, &cleanAmountLabel })
            c->setAlpha (a);
        if (cleanAmountSlider.isVisible() || eqGraph->isVisible())
        {
            eqGraph->setCleanApplied (cleanOn && ! dry ? glitchProcessor.getCleanApplied() : CleanLab::Applied {});
            if (cleanAmountSlider.isVisible())
                refreshClean (cleanOn);
        }
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
        motionButton.setToggleState (motionOn, juce::dontSendNotification);
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
        text = applied ? "Mix applied. Compare with DRY / PROCESSED, then refine it in FINAL TOUCH." : "Your earlier sound is back. Press BUILD AGAIN to build a mix.";
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
        titleColour = ui::pink;
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
        titleColour = ui::errorRed;
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
    glitchProcessor.setUiInt ("uiProfile", profile); // kept in the session, so BUILD and FINAL TOUCH (and a reopened window) agree
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
    beatApplied = false;
    showStatus ("Back to your sound.");
    shownPresetText = {};
    refreshPresetBar();
    refreshProfiles();
}


// ---- level cards, the persistent bar ---------------------------------------------------------------------------------------------------------

// Everything shown here is read from the real plug-in state: the held peaks, the CLEAN telemetry, the instrumental input status, the build result and the chain.
void GlitchKidEditor::refreshStatus (float inPeak, float outPeak)
{
    using SS = GlitchKidProcessor::SourceStatus;
    namespace am = glitchkid::automode;
    const bool clipping = outPeak >= 0.999f;
    clipTicks = clipping ? 45 : juce::jmax (0, clipTicks - 1);
    auto hold = [] (float shown, float peak) { const float db = juce::Decibels::gainToDecibels (peak, -100.0f); return db > shown ? db : juce::jmax (db, shown - 1.5f); };
    inDbShown = hold (inDbShown, inPeak);
    outDbShown = hold (outDbShown, outPeak);
    auto db = [] (float v) { return v > -80.0f ? juce::String (juce::roundToInt (v)) + " dB" : juce::String ("--"); };
    const auto line1 = signalTicks > 30 ? juce::String ("VOCAL IN: NO SIGNAL") : "VOCAL IN: " + db (inDbShown);
    const auto line2 = clipTicks > 0 ? juce::String ("OUT: CLIPPING") : "OUT: " + db (outDbShown);
    if (const auto t = line1 + "\n" + line2; t != ioLabel.getText())
        ioLabel.setText (t, juce::dontSendNotification);
    ioLabel.setColour (juce::Label::textColourId, clipTicks > 0 ? ui::errorRed : ui::textDim);
    undoButton.setEnabled (undo.valid);

    const bool cleanOn = glitchProcessor.apvts.getRawParameterValue (GlitchKidProcessor::cleanOnId)->load() >= 0.5f;
    {
        const auto ca = glitchProcessor.getCleanApplied();
        const bool applied = ca.rumbleAmount > 0.02f || ca.mudDb > 0.05f || ca.resDb[0] > 0.05f || ca.resDb[1] > 0.05f || ca.resDb[2] > 0.05f || ca.essGrDb > 0.05f;
        if (! cleanOn)
            tiles[0].setStatus ("OFF - SIGNAL UNTOUCHED", ui::TileStatus::idle);
        else if (ca.learnedSeconds < 0.05f && signalTicks > 30)
            tiles[0].setStatus ("ON - WAITING FOR AUDIO", ui::TileStatus::ready);
        else if (ca.learnedSeconds < 3.0f)
            tiles[0].setStatus ("ON - LEARNING YOUR VOICE", ui::TileStatus::ready);
        else
            tiles[0].setStatus (applied ? "ON - CLEANING NOW" : "ON - NOTHING TO FIX FOUND", applied ? ui::TileStatus::active : ui::TileStatus::ready);
    }
    {
        const auto st = glitchProcessor.getInstrumentalStatus();
        if (beatListening)
            tiles[1].setStatus ("ANALYZING...", ui::TileStatus::active);
        else if (beatApplied)
            tiles[1].setStatus ("APPLIED - " + juce::String (beatAppliedCount) + " CHANGE" + (beatAppliedCount == 1 ? "" : "S"), ui::TileStatus::active);
        else if (beatResult.valid)
            tiles[1].setStatus ("ANALYSED - " + juce::String ((int) beatResult.items.size() - 1) + " SUGGESTION" + (beatResult.items.size() == 2 ? "" : "S"), ui::TileStatus::ready);
        else if (st == SS::notConnected)
            tiles[1].setStatus ("NO BEAT INPUT CONNECTED", ui::TileStatus::idle);
        else if (st == SS::connectedSilent)
            tiles[1].setStatus ("BEAT INPUT IS SILENT", ui::TileStatus::idle);
        else
            tiles[1].setStatus (beatResult.status == am::BeatLockResult::Status::beatNotAnalysed ? "BEAT NOT ANALYSED - TRY AGAIN" : "BEAT ARRIVING - READY TO ANALYZE", ui::TileStatus::ready);
    }
    {
        const bool built = glitchProcessor.getPresetName().startsWith ("Built: ");
        if (buildState == BuildState::listening)
            tiles[2].setStatus ("LISTENING TO YOUR VOCAL", ui::TileStatus::active);
        else if (buildState == BuildState::built && built)
            tiles[2].setStatus ("BUILT - " + juce::String ((int) lastProposal.changes.size()) + " CHANGES, " + (lastProposal.mode == am::MixMode::beatInformed ? "BEAT-AWARE" : "VOCAL ONLY"), ui::TileStatus::active);
        else if (buildState == BuildState::failed)
            tiles[2].setStatus ("BUILD FAILED - NOTHING CHANGED", ui::TileStatus::problem);
        else
            tiles[2].setStatus ("NOT BUILT YET", ui::TileStatus::idle);
    }
    {
        int n = 0;
        for (bool b : stageActive)
            n += b ? 1 : 0;
        const bool dry = glitchProcessor.apvts.getRawParameterValue (GlitchKidProcessor::bypassId)->load() >= 0.5f;
        tiles[3].setStatus (dry ? "DRY - NO STAGE IS PROCESSING" : juce::String (n) + " OF 4 STAGES ACTIVE", n > 0 ? ui::TileStatus::ready : ui::TileStatus::idle);
    }
}

// The real-time views: the newest audio from the processor's taps (the vocal input before processing, the output after it, the Instrumental input), a windowed FFT
// and a min/max waveform, all computed here on the message thread from bounded copies. Nothing is drawn without a signal.
void GlitchKidEditor::refreshDisplays()
{
    using SS = GlitchKidProcessor::SourceStatus;
    const bool onHome = page == Page::home, onBeat = page == Page::beat, onClean = page == Page::clean, onTone = page == Page::finalTouch && category == 1;
    if (! (onHome || onBeat || onClean || onTone))
        return;
    const double sr = glitchProcessor.getSampleRate() > 8000.0 ? glitchProcessor.getSampleRate() : 48000.0;
    specIn.update (glitchProcessor.getInputTap(), sr);
    if (onClean || onTone)
    {
        const float top = (float) juce::jmin (20000.0, 0.45 * sr);
        if (specIn.hasSignal())
            eqGraph->setSpectrum (specIn.levels().data(), glitchkid::analysis::DisplaySpectrum::columns, top);
        else
            eqGraph->clearSpectrum();
    }
    if (onHome)
    {
        specOut.update (glitchProcessor.getOutputTap(), sr);
        waveIn.update (glitchProcessor.getInputTap(), 2400);
        waveOut.update (glitchProcessor.getOutputTap(), 2400);
        const bool beatLive = glitchProcessor.getInstrumentalStatus() == SS::live;
        if (beatLive)
            specBeat.update (glitchProcessor.getBeatTap(), sr);
        homeView.setSampleRate (sr);
        homeView.setTrace (0, specIn.levels(), specIn.hasSignal());
        homeView.setTrace (1, specOut.levels(), specOut.hasSignal());
        homeView.setTrace (2, specBeat.levels(), beatLive && specBeat.hasSignal());
        homeView.setWave (waveIn, waveOut);
        const juce::String hint = beatLive ? "VOCAL INPUT before processing, OUTPUT after it, INSTRUMENTAL INPUT as received" : "VOCAL INPUT before processing, OUTPUT after it";
        if (hint != homeHint.getText())
            homeHint.setText (hint, juce::dontSendNotification);
    }
    if (onBeat)
    {
        const bool beatLive = glitchProcessor.getInstrumentalStatus() == SS::live;
        if (beatLive)
            specBeat.update (glitchProcessor.getBeatTap(), sr);
        beatView.setSampleRate (sr);
        beatView.setShowWave (false);
        beatView.setTrace (0, specIn.levels(), specIn.hasSignal());
        beatView.setTrace (1, specOut.levels(), false);
        beatView.setTrace (2, specBeat.levels(), beatLive && specBeat.hasSignal());
        beatView.setWave (glitchkid::analysis::WaveColumns {}, glitchkid::analysis::WaveColumns {});
    }
}

// ---- BEAT LOCK -------------------------------------------------------------------------------------------------------------------------------

namespace
{
glitchkid::automode::InstrumentalInput instrumentalInput (GlitchKidProcessor& p)
{
    using SS = GlitchKidProcessor::SourceStatus;
    const auto st = p.getInstrumentalStatus();
    return st == SS::live ? glitchkid::automode::InstrumentalInput::live : st == SS::connectedSilent ? glitchkid::automode::InstrumentalInput::connectedSilent
                                                                                                      : glitchkid::automode::InstrumentalInput::notConnected;
}
}

void GlitchKidEditor::startBeatAnalysis()
{
    namespace am = glitchkid::automode;
    beatApplied = false;
    beatSelected.clear();
    const auto input = instrumentalInput (glitchProcessor);
    if (input != am::InstrumentalInput::live)
    {
        beatResult = am::analyseBeatLock ({}, {}, input); // nothing to listen to: say so (no analysis is started, nothing is invented)
        rebuildBeatRows();
        return;
    }
    beatResult = {};
    beatStartCount = glitchProcessor.getAnalyzer().completedCount();
    glitchProcessor.startSourceAnalysis();
    beatListening = true;
    beatStopSent = false;
    rebuildBeatRows();
}

void GlitchKidEditor::finishBeatAnalysis()
{
    namespace am = glitchkid::automode;
    beatResult = am::analyseBeatLock (glitchProcessor.getAnalyzer().fetch(), glitchProcessor.getInstrumentalAnalyzer().fetch(), instrumentalInput (glitchProcessor));
    beatSelected.assign (beatResult.items.size(), true);
    rebuildBeatRows();
}

// Lists the suggestions: a tick (advice-only rows have none), the name with the real current -> proposed value, and the reason.
void GlitchKidEditor::rebuildBeatRows()
{
    namespace am = glitchkid::automode;
    const auto cur = glitchProcessor.captureSoundState();
    for (int i = 0; i < beatRows; ++i)
    {
        if (! beatResult.valid || (size_t) i >= beatResult.items.size())
        {
            beatTitles[i].setText ({}, juce::dontSendNotification);
            beatReasons[i].setText ({}, juce::dontSendNotification);
            continue;
        }
        const auto& it = beatResult.items[(size_t) i];
        juce::String title = it.advisory ? "ADVICE - " + it.title : juce::String (it.move.category) + " - " + it.title;
        if (! it.advisory)
        {
            std::vector<bool> only (beatResult.items.size(), false);
            only[(size_t) i] = true;
            std::vector<am::MixChange> ch;
            am::applyBeatLock (beatResult, only, cur, glitchProcessor.makeRangeLookup(), &ch);
            if (! ch.empty())
            {
                auto* prm = glitchProcessor.apvts.getParameter (ch[0].id);
                title << ":  " << juce::String (ch[0].from, 1) << " -> " << juce::String (ch[0].to, 1) << (prm != nullptr && prm->getLabel().isNotEmpty() ? " " + prm->getLabel() : juce::String());
            }
            else
                title << ":  no change (already at its limit, or the effect is off)";
        }
        beatTitles[i].setText (title, juce::dontSendNotification);
        beatReasons[i].setText (juce::String (it.move.reason), juce::dontSendNotification);
        beatTicks[i].setToggleState ((size_t) i < beatSelected.size() && beatSelected[(size_t) i], juce::dontSendNotification);
    }
    shownBeatText = {};
    layoutContent();
}

void GlitchKidEditor::applyBeatSelection()
{
    namespace am = glitchkid::automode;
    if (! beatResult.valid || beatApplied)
        return;
    std::vector<am::MixChange> changes;
    const auto cur = glitchProcessor.captureSoundState();
    const auto after = am::applyBeatLock (beatResult, beatSelected, cur, glitchProcessor.makeRangeLookup(), &changes);
    if (changes.empty())
    {
        beatChanges.setText ("Nothing was changed: no adjustable suggestion is ticked.", juce::dontSendNotification);
        showStatus ("BEAT LOCK: nothing ticked, nothing changed.");
        return;
    }
    undo = { cur, glitchProcessor.getPresetName(), true };
    glitchProcessor.applyParameterChanges (after);
    beatApplied = true;
    beatAppliedCount = (int) changes.size();
    juce::String text = "APPLIED " + juce::String (beatAppliedCount) + " CHANGE" + (beatAppliedCount == 1 ? "" : "S") + ": the values listed above are now set on this vocal's own controls. "
                        "UNDO BEAT LOCK (or UNDO in the bar) puts your previous sound back.";
    beatChanges.setText (text, juce::dontSendNotification);
    showStatus ("BEAT LOCK: " + juce::String (beatAppliedCount) + " change(s) applied. UNDO puts them back.");
    shownPresetText = {};
    refreshPresetBar();
    layoutContent();
}

// Runs every tick (an analysis continues while another page is open); the labels are only refreshed while the page is visible.
void GlitchKidEditor::refreshBeatLock (float inPeak, float beatPk)
{
    namespace am = glitchkid::automode;
    auto& an = glitchProcessor.getAnalyzer();
    auto& ban = glitchProcessor.getInstrumentalAnalyzer();
    const auto input = instrumentalInput (glitchProcessor);
    beatVocalPeakDb = inPeak > 0.0f ? juce::Decibels::gainToDecibels (inPeak, -100.0f) : juce::jmax (-100.0f, beatVocalPeakDb - 1.5f);
    beatBeatPeakDb = beatPk > 0.0f ? juce::Decibels::gainToDecibels (beatPk, -100.0f) : juce::jmax (-100.0f, beatBeatPeakDb - 1.5f);
    if (beatListening && an.completedCount() > beatStartCount && ! an.isListening())
    {
        if (ban.isListening() && input == am::InstrumentalInput::live && ! beatStopSent)
        {
            ban.stop(); // the vocal has been heard: end the beat's listening with what it heard too
            beatStopSent = true;
        }
        else if (! ban.isListening() || input != am::InstrumentalInput::live)
        {
            beatListening = false;
            finishBeatAnalysis();
        }
    }
    if (page != Page::beat)
        return;

    vocalBar.setValue (ui::LevelBar::peakToMeter (inPeak), inPeak >= 0.999f);
    beatBar.setValue (ui::LevelBar::peakToMeter (beatPk), beatPk >= 0.999f);
    beatBar.setActive (input != am::InstrumentalInput::notConnected);
    vocalBar.setActive (signalTicks <= 30);
    auto db = [] (float v) { return v > -80.0f ? juce::String (v, 1) + " dB" : juce::String ("no signal"); };
    juce::String text = "Vocal peak: " + db (beatVocalPeakDb) + "     Beat peak: " + (input == am::InstrumentalInput::notConnected ? juce::String ("no input") : db (beatBeatPeakDb));
    juce::String status;
    if (beatListening)
        status = "Listening: " + juce::String (an.liveActiveSeconds(), 1) + " s of vocal heard (it needs 2 s and stops at 8 s). Keep the vocal and the beat playing together.";
    else if (beatResult.message.isNotEmpty())
        status = beatResult.message;
    else if (input != am::InstrumentalInput::live)
        status = am::analyseBeatLock ({}, {}, input).message;
    else
        status = "A beat is arriving on the Instrumental input. Press ANALYZE BEAT, then play the vocal and the beat together.";
    const auto key = status + "|" + text + "|" + juce::String ((int) beatListening) + juce::String ((int) beatApplied) + juce::String ((int) input) + juce::String ((int) beatResult.valid);
    if (key != shownBeatText)
    {
        shownBeatText = key;
        beatStatus.setText (status, juce::dontSendNotification);
        beatLevels.setText (text, juce::dontSendNotification);
        beatProgress.setText (beatListening ? juce::String (an.liveActiveSeconds(), 1) + " s heard" : juce::String(), juce::dontSendNotification);
        analyzeBeatButton.setButtonText (beatListening ? "STOP NOW" : (beatResult.valid ? "ANALYZE AGAIN" : "ANALYZE BEAT"));
        applyBeatButton.setEnabled (beatResult.valid && ! beatApplied);
        undoBeatButton.setEnabled (beatApplied && undo.valid);
        bandCompare.setData (beatResult.valid, beatResult.vocalBands, beatResult.beatBands, beatResult.maskRisk);
    }
    else if (beatListening)
        beatProgress.setText (juce::String (an.liveActiveSeconds(), 1) + " s heard", juce::dontSendNotification);
    if (! beatApplied && beatChanges.getText().isNotEmpty())
        beatChanges.setText ({}, juce::dontSendNotification);
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
    refreshBeatLock (inPeak, glitchProcessor.consumeBeatPeak());
    refreshStatus (inPeak, outPeak);
    refreshDisplays();
    grMeter.pushReduction (glitchProcessor.consumeGainReductionDb());
    if (eqGraph->isVisible())
        eqGraph->refresh();

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

    // a faint technical grid behind the workspace
    g.setColour (ui::border.withAlpha (0.22f));
    for (float x = 16.0f; x < W; x += 32.0f)
        g.fillRect (x, 102.0f, 1.0f, 458.0f);
    for (float y = 102.0f; y < 562.0f; y += 32.0f)
        g.fillRect (0.0f, y, W, 1.0f);

    // the persistent bar: fine scanlines, then an ultraviolet-to-pink signal line along its lower edge
    g.setColour (ui::violet.withAlpha (0.05f));
    for (float y = 2.0f; y < 43.0f; y += 3.0f)
        g.fillRect (0.0f, y, W, 1.0f);
    juce::ColourGradient line (ui::violet, 0.0f, 0.0f, ui::pink.withAlpha (0.0f), W, 0.0f, false);
    line.addColour (0.55, ui::pink.withAlpha (0.8f));
    g.setGradientFill (line);
    g.fillRect (0.0f, 43.0f, W, 2.0f);
    g.setColour (ui::border);
    g.fillRect (0.0f, 562.0f, W, 1.0f);

    // the wordmark: bold, tracked out, with a slight chromatic fringe (pink left, violet right)
    {
        const juce::Rectangle<float> r (14.0f, 4.0f, 160.0f, 24.0f);
        g.setFont (ui::labFont (18.0f));
        g.setColour (ui::pink.withAlpha (0.6f));
        g.drawText ("GLITCHKID", r.translated (-1.4f, 0.0f), juce::Justification::centredLeft, false);
        g.setColour (ui::violet.withAlpha (0.6f));
        g.drawText ("GLITCHKID", r.translated (1.4f, 0.0f), juce::Justification::centredLeft, false);
        g.setColour (ui::textBright);
        g.drawText ("GLITCHKID", r, juce::Justification::centredLeft, false);
        g.setFont (ui::labFont (9.5f));
        g.setColour (ui::violet);
        g.drawText ("VOCAL LAB", juce::Rectangle<float> (15.0f, 27.0f, 120.0f, 13.0f), juce::Justification::centredLeft, false);
    }

    if (page == Page::build)
    {
        g.setColour (ui::border);
        g.fillRect (586.0f, 104.0f, 1.0f, 444.0f);
        g.setColour (ui::violet);
        g.fillRect (586.0f, 104.0f, 1.0f, 12.0f); // a short ultraviolet tick at the top of the divider
    }

    for (const auto& r : panelRects)
        ui::drawPanel (g, r.toFloat());
}

void GlitchKidEditor::resized()
{
    scaleFactor = juce::jmin ((float) getWidth() / (float) designW, (float) getHeight() / (float) designH);
    applyContentTransform();
    content.setBounds (0, 0, designW, designH);
    repaint();
}

void GlitchKidEditor::layoutContent()
{
    using Rect = juce::Rectangle<int>;
    auto show = [] (juce::Component& c, Rect r) { c.setBounds (r); c.setVisible (true); };
    auto showK = [] (ParamKnob& k, Rect r) { k.setBounds (r); k.setVisible (true); };

    // Hide every control that is not part of the always-visible bar, chain and preset bar; the current page then shows what it needs.
    const std::vector<juce::Component*> persistent = { &navButtons[0], &navButtons[1], &navButtons[2], &navButtons[3], &navButtons[4], &undoButton, &ioLabel, &motionButton, &dryButton, &processedButton, &chain,
                                                       &presetPrevButton, &presetMenuButton, &presetNextButton, &presetSaveButton, &presetDeleteButton, &abSetA, &abSetB,
                                                       &abRecallA, &abRecallB, &abLabel, &presetStatus };
    for (auto* child : content.getChildren())
        if (std::find (persistent.begin(), persistent.end(), child) == persistent.end())
            child->setVisible (false);
    panelRects.clear();

    // ---- the persistent bar, the chain ----
    {
        static const int navW[5] = { 62, 92, 90, 118, 100 };
        int x = 168;
        for (int i = 0; i < 5; ++i)
        {
            show (navButtons[i], { x, 8, navW[i], 28 });
            x += navW[i] + 4;
        }
    }
    show (dryButton, { 658, 8, 46, 28 });
    show (processedButton, { 708, 8, 86, 28 });
    show (undoButton, { 800, 8, 46, 28 });
    show (motionButton, { 852, 8, 92, 28 });
    show (chain, { 16, 50, 770, 48 });
    show (ioLabel, { 796, 50, 148, 48 });

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

    // ---- HOME / SESSION ----
    if (page == Page::home)
    {
        show (homeTitle, { 24, 106, 300, 18 });
        show (homeSource, { 330, 106, 200, 18 });
        show (homeHint, { 540, 106, 396, 18 });
        show (homeView, { 24, 128, 912, 236 });
        static const int tx[4] = { 24, 242, 460, 720 }, tw[4] = { 212, 212, 254, 216 };
        for (int i = 0; i < 4; ++i)
            show (tiles[i], { tx[i], 378, tw[i], 112 });
    }

    // ---- CLEAN LAB ----
    else if (page == Page::clean)
    {
        show (cleanOnButton, { 24, 106, 130, 28 });
        show (cleanAmountLabel, { 166, 106, 60, 28 });
        show (cleanAmountSlider, { 228, 106, 400, 28 });
        show (cleanAdvButton, { 640, 106, 110, 28 });
        show (cleanResetButton, { 820, 106, 116, 28 });
        show (cleanHint, { 24, 140, 912, 30 });
        show (*eqGraph, { 24, 176, 590, 250 });
        // what CLEAN LAB is really doing (from the DSP's telemetry)
        show (toneHead, { 630, 178, 300, 16 });
        show (toneRead[0], { 630, 198, 306, 20 });
        show (toneRead[1], { 630, 220, 306, 20 });
        show (resHead, { 630, 246, 300, 16 });
        for (int i = 0; i < 3; ++i)
            show (resRead[i], { 630, 266 + i * 21, 306, 20 });
        show (essHead, { 630, 334, 300, 16 });
        show (essBar, { 630, 354, 300, 14 });
        show (essRead, { 630, 372, 306, 20 });
        show (cleanLearn, { 630, 396, 306, 30 });
        show (cleanLevel, { 24, 432, 590, 20 });
        show (eqNote, { 24, 454, 590, 30 });
        if (cleanAdvanced)
        {
            panelRects.push_back ({ 24, 492, 912, 62 });
            juce::Component* trimHeads[3] = { &toneTrimLabel, &resTrimLabel, &essTrimLabel };
            juce::Slider* sliders[3] = { &cleanToneSlider, &cleanResSlider, &cleanEssSlider };
            static const char* names[3] = { "CORRECTIVE EQ", "RESONANCE", "DE-ESSER" };
            const int cx[3] = { 36, 346, 656 };
            for (int c = 0; c < 3; ++c)
            {
                static_cast<juce::Label*> (trimHeads[c])->setText (names[c], juce::dontSendNotification);
                show (*trimHeads[c], { cx[c], 498, 270, 18 });
                show (*sliders[c], { cx[c], 520, 270, 26 });
            }
        }
    }

    // ---- BEAT LOCK ----
    else if (page == Page::beat)
    {
        show (beatHeading, { 24, 106, 300, 16 });
        show (beatStatus, { 24, 126, 446, 40 });
        show (vocalBarLabel, { 24, 170, 60, 16 });
        show (vocalBar, { 88, 168, 382, 18 });
        show (beatBarLabel, { 24, 192, 60, 16 });
        show (beatBar, { 88, 190, 382, 18 });
        show (beatLevels, { 24, 212, 446, 16 });
        show (beatView, { 24, 232, 446, 120 });
        show (bandCompare, { 24, 358, 446, 112 });
        show (analyzeBeatButton, { 24, 478, 200, 36 });
        show (beatProgress, { 234, 478, 236, 36 });
        show (beatRouting, { 24, 518, 446, 42 });

        show (suggestHeading, { 486, 106, 400, 16 });
        for (int i = 0; i < beatRows; ++i)
        {
            const bool used = beatResult.valid && (size_t) i < beatResult.items.size();
            if (! used)
                continue;
            const int y = 128 + i * 47;
            if (! beatResult.items[(size_t) i].advisory)
                show (beatTicks[i], { 486, y + 1, 40, 24 });
            show (beatTitles[i], { 534, y, 402, 18 });
            show (beatReasons[i], { 534, y + 18, 402, 28 });
        }
        show (applyBeatButton, { 486, 466, 196, 40 });
        show (undoBeatButton, { 690, 466, 160, 40 });
        if (beatApplied)
            show (beatChanges, { 486, 510, 450, 52 });
        else if (! beatResult.valid && ! beatListening)
            show (vocalOnlyButton, { 486, 466, 300, 40 }); // no beat to compare with: the honest alternative
        if (! beatResult.valid)
        {
            applyBeatButton.setVisible (false);
            undoBeatButton.setVisible (false);
        }
        else
            vocalOnlyButton.setVisible (false);
    }

    // ---- BUILD MY VOCAL MIX (profiles, build, result) ----
    else if (page == Page::build)
    {
        show (profileHeading, { 24, 106, 548, 14 });
        for (int i = 0; i < profileCards.size(); ++i)
            show (*profileCards[i], { 24 + (i % 2) * 278, 126 + (i / 2) * 60, 270, 54 });
        show (detailDesc, { 24, 372, 548, 20 });
        show (detailSettings, { 24, 394, 548, 44 });
        show (detailStart, { 24, 440, 548, 34 });
        show (loadButton, { 24, 486, 200, 40 });
        show (backToMineButton, { 232, 486, 200, 40 });

        show (buildHeading, { 600, 106, 336, 14 });
        show (autoAnalyzeButton, { 600, 126, 336, 80 });
        show (autoStatus, { 600, 212, 336, 48 });
        show (sourceLine, { 600, 262, 336, 32 });
        show (autoStyleLabel, { 600, 300, 336, 14 });
        show (autoIntensitySlider, { 600, 314, 336, 24 });
        show (resultHeading, { 600, 350, 336, 14 });
        show (resultTitle, { 600, 370, 336, 20 });
        show (resultBody, { 600, 392, 336, 84 });
        show (autoApplyButton, { 600, 482, 160, 34 });
        show (finalButton, { 776, 504, 160, 44 });
    }

    // ---- FINAL TOUCH (one category at a time) ----
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
        if (category >= 1 && category <= 3)
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
            case 0: // PITCH & KEY: the note pad (key), the scale, on/off, the tuning style, strength and speed
                show (keyLabel, { 36, 150, 100, 14 });
                for (int i = 0; i < noteKeys.size(); ++i)
                    show (*noteKeys[i], { 36 + i * 72, 166, 68, 44 });
                show (scaleCaption, { 36, 216, 100, 14 });
                for (int i = 0; i < 6; ++i)
                    show (scaleChips[i], { 36 + i * 146, 232, 140, 30 });
                show (tuneOnCaption, { 36, 270, 100, 14 });
                show (tuneOnButton, { 36, 288, 100, 30 });
                show (styleCaption, { 156, 270, 200, 14 });
                for (int i = 0; i < glitchkid::macros::numTuneStyles; ++i)
                    show (styleButtons[i], { 156 + i * 130, 288, 124, 30 });
                for (int i = 0; i < tuneKnobs.size(); ++i)
                    showK (*tuneKnobs[i], { 40 + i * 150, 336, 140, 150 });
                show (tuneSummary, { 340, 336, 590, 28 });
                show (tuneReadout, { 340, 366, 590, 22 });
                show (tuneSubline, { 340, 392, 590, 34 });
                show (scopeCaption, { 340, 430, 200, 14 });
                show (scope, { 340, 446, 590, 42 });
                break;

            case 1: // TONE: the real EQ response, the four bands, the selected band's knobs, warmth
                show (*eqGraph, { 36, 192, 590, 204 });
                show (airOnButton, { 44, 404, 120, 24 });
                show (airLinkButton, { 176, 404, 76, 24 });
                show (airHint, { 262, 404, 360, 24 });
                show (airPresenceLabel, { 44, 432, 72, 24 });
                show (airPresenceSlider, { 120, 432, 216, 24 });
                show (airMixLabel, { 350, 432, 34, 24 });
                show (airMixSlider, { 386, 432, 236, 24 });
                show (airAirLabel, { 44, 460, 72, 24 });
                show (airAirSlider, { 120, 460, 216, 24 });
                show (airTrimLabel, { 350, 460, 34, 24 });
                show (airTrimSlider, { 386, 460, 236, 24 });
                for (int b = 0; b < ui::EqGraph::numBands; ++b)
                {
                    show (bandButtons[b], { 646 + b * 70, 192, 64, 30 });
                    bandButtons[b].setToggleState (b == eqBand, juce::dontSendNotification);
                }
                for (int i = 0; i < eqKnobs[eqBand].size(); ++i)
                    showK (*eqKnobs[eqBand][i], { 646 + i * 92, 236, 88, 130 });
                showK (*warmthKnob[0], { 646, 376, 120, 112 });
                show (eqNote, { 780, 384, 144, 96 });
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
                show (echoTitle, { 36, y0, 140, 14 });
                show (echoOnButton, { 36, y0 + 20, 100, 28 });
                show (echoSyncButton, { 36, y0 + 52, 100, 28 });
                show (echoDivLabel, { 150, y0, 130, 14 });
                show (echoDivBox, { 150, y0 + 18, 130, 30 });
                for (int i = 0; i < echoKnobs.size(); ++i)
                    showK (*echoKnobs[i], { 300 + i * 158, y0, 150, 98 });
                show (dimTitle, { 36, y1, 200, 14 });
                show (dimOnButton, { 36, y1 + 20, 100, 28 });
                for (int i = 0; i < dimKnobs.size(); ++i)
                    showK (*dimKnobs[i], { 300 + i * 158, y1, 150, 98 });
                show (dblTitle, { 36, y2, 200, 14 });
                show (dblOnButton, { 36, y2 + 20, 100, 28 });
                for (int i = 0; i < dblKnobs.size(); ++i)
                    showK (*dblKnobs[i], { 300 + i * 158, y2, 150, 98 });
                break;
            }
        }
    }
}
