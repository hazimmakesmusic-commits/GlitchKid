#include "PluginEditor.h"

namespace
{
const juce::Colour lime (0xffb6ff00), pink (0xffff2d95), violet (0xff8a2be2);

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
    g.setColour (juce::Colour (0xff1e1e22));
    g.fillRoundedRectangle (r, 3.0f);

    const float h = r.getHeight() * juce::jmap (shownDb, minDb, maxDb, 0.0f, 1.0f);
    g.setColour (shownDb > 0.0f ? juce::Colour (0xffff3030) : lime);
    g.fillRoundedRectangle (r.withTop (r.getBottom() - h), 3.0f);
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
    g.setColour (juce::Colour (0xff1e1e22));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (pink);
    g.fillRoundedRectangle (r.withHeight (r.getHeight() * juce::jlimit (0.0f, 1.0f, shownDb / maxDb)), 3.0f);
}

ParamKnob::ParamKnob (juce::AudioProcessorValueTreeState& apvts, const char* paramId, const juce::String& name,
                      const juce::String& suffix, juce::Colour colour)
    : attachment (apvts, paramId, slider)
{
    slider.setTextValueSuffix (suffix);
    slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (11.0f));
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
    setLookAndFeel (&lookAndFeel);
    motionOn = p.getUiFlag ("uiMotion", true);
    page = (Page) juce::jlimit (0, 2, p.getUiInt ("uiPage", 0)); // first launch: TUNE
    category = juce::jlimit (0, numCategories - 1, p.getUiInt ("uiCategory", 0));
    title.setFont (juce::FontOptions (32.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, lime);
    title.setJustificationType (juce::Justification::centred);

    tagline.setFont (juce::FontOptions (13.0f));
    tagline.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    tagline.setJustificationType (juce::Justification::centred);

    for (auto* l : { &inLabel, &outLabel })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setFont (juce::FontOptions (11.0f));
    }

    bypassButton.setColour (juce::ToggleButton::tickColourId, violet);
    stutterOnButton.setColour (juce::ToggleButton::tickColourId, pink);

    // Glitch Stutter: ON switch, rate choice (items come from the parameter's choices), Mix knob, tempo source.
    if (auto* rate = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (GlitchKidProcessor::stutterRateId)))
    {
        stutterRateBox.addItemList (rate->choices, 1);
        stutterRateBox.setSelectedItemIndex (rate->getIndex(), juce::dontSendNotification);
    }
    stutterTitle.setJustificationType (juce::Justification::centred);
    stutterTitle.setColour (juce::Label::textColourId, violet);
    stutterRateLabel.setJustificationType (juce::Justification::centred);
    stutterRateLabel.setFont (juce::FontOptions (11.0f));
    tempoLabel.setJustificationType (juce::Justification::centred);
    tempoLabel.setFont (juce::FontOptions (10.0f));
    tempoLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    stutterMixKnob.add (new ParamKnob (p.apvts, GlitchKidProcessor::stutterMixId, "STUTTER MIX", " %", pink));
    stutterMixKnob[0]->addTo (content);
    for (auto* c : std::initializer_list<juce::Component*> { &stutterTitle, &stutterRateLabel, &tempoLabel,
                                                              &stutterOnButton, &stutterRateBox })
        content.addAndMakeVisible (c);

    // PRESETS + A/B bar.
    auto styleBarButton = [this] (juce::TextButton& b, juce::Colour text, const juce::String& tip)
    {
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1e1e22));
        b.setColour (juce::TextButton::buttonOnColourId, lime);
        b.setColour (juce::TextButton::textColourOffId, text);
        b.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff121214));
        b.setTooltip (tip);
        content.addAndMakeVisible (b);
    };
    styleBarButton (presetMenuButton, juce::Colours::white, "Choose the factory default sound or one of your saved presets.");
    styleBarButton (presetPrevButton, juce::Colours::white, "Previous preset (factory presets first, then yours). Replaces the current sound: press SET A first to keep it.");
    styleBarButton (presetNextButton, juce::Colours::white, "Next preset (factory presets first, then yours). Replaces the current sound: press SET A first to keep it.");
    styleBarButton (presetSaveButton, lime, "Save the current sound (all controls and the Alter Ego) as a named preset. Existing presets are never replaced without asking.");
    styleBarButton (presetDeleteButton, pink, "Delete the loaded user preset (asks first). The factory default cannot be deleted.");
    styleBarButton (abSetA, violet, "Capture the current sound into slot A.");
    styleBarButton (abSetB, violet, "Capture the current sound into slot B.");
    styleBarButton (abRecallA, juce::Colours::white, "Switch to the sound stored in A.");
    styleBarButton (abRecallB, juce::Colours::white, "Switch to the sound stored in B.");
    styleBarButton (abCopyAB, juce::Colours::lightgrey, "Copy slot A into slot B.");
    styleBarButton (abCopyBA, juce::Colours::lightgrey, "Copy slot B into slot A.");
    presetMenuButton.setClickingTogglesState (false);
    presetMenuButton.onClick = [this] { openPresetMenu(); };
    for (auto* b : { &presetPrevButton, &presetNextButton })
        b->onClick = [this, b]
        {
            showStatus (presets.step (b == &presetNextButton ? 1 : -1).message);
            refreshAlterEgos();
            shownPresetText = {};
            refreshPresetBar();
        };
    presetSaveButton.onClick = [this] { askSavePreset(); };
    presetDeleteButton.onClick = [this] { askDeletePreset(); };
    abSetA.onClick = [this] { glitchProcessor.captureAbSlot (0); showStatus ("Current sound captured in A."); refreshPresetBar(); };
    abSetB.onClick = [this] { glitchProcessor.captureAbSlot (1); showStatus ("Current sound captured in B."); refreshPresetBar(); };
    abRecallA.onClick = [this] { if (glitchProcessor.recallAbSlot (0)) refreshAlterEgos(); refreshPresetBar(); };
    abRecallB.onClick = [this] { if (glitchProcessor.recallAbSlot (1)) refreshAlterEgos(); refreshPresetBar(); };
    abCopyAB.onClick = [this] { if (glitchProcessor.copyAbSlot (0, 1)) showStatus ("A copied to B."); refreshPresetBar(); };
    abCopyBA.onClick = [this] { if (glitchProcessor.copyAbSlot (1, 0)) showStatus ("B copied to A."); refreshPresetBar(); };
    // AUTOMODE row.
    styleBarButton (autoAnalyzeButton, lime, "Listen to the vocal for a few seconds (press play in your host). Nothing is changed until you press APPLY.");
    styleBarButton (autoApplyButton, lime, "Set the controls from the analysis, once, at the chosen intensity. Press SET A first to keep your current sound; every control stays editable.");
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
    autoStyleLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    autoStyleLabel.setColour (juce::Label::textColourId, violet);
    autoStyleLabel.setTooltip ("The production direction. ASTRO MODE: dark, atmospheric melodic rap. Original settings, not an exact copy of anyone's sound.");
    autoIntensityLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    autoIntensityLabel.setJustificationType (juce::Justification::centredRight);
    autoIntensitySlider.setSliderStyle (juce::Slider::LinearHorizontal);
    autoIntensitySlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 22);
    autoIntensitySlider.setRange (0.0, 100.0, 1.0);
    autoIntensitySlider.setTextValueSuffix (" %");
    autoIntensitySlider.setValue (glitchProcessor.getAutoIntensity() * 100.0, juce::dontSendNotification);
    autoIntensitySlider.setColour (juce::Slider::trackColourId, violet);
    autoIntensitySlider.setTooltip ("How strongly AUTOMODE moves the controls when you press APPLY.");
    autoIntensitySlider.onValueChange = [this] { glitchProcessor.setAutoIntensity ((float) autoIntensitySlider.getValue() / 100.0f); };
    autoStatus.setFont (juce::FontOptions (12.5f));
    autoStatus.setJustificationType (juce::Justification::centredLeft);
    autoStatus.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    autoStatus.setMinimumHorizontalScale (0.8f);
    for (auto* c : std::initializer_list<juce::Component*> { &autoStyleLabel, &autoIntensityLabel, &autoIntensitySlider, &autoStatus })
        content.addAndMakeVisible (c);

    presetStatus.setJustificationType (juce::Justification::centredLeft);
    presetStatus.setFont (juce::FontOptions (11.0f));
    presetStatus.setColour (juce::Label::textColourId, lime);
    abLabel.setJustificationType (juce::Justification::centredRight);
    abLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    abLabel.setColour (juce::Label::textColourId, violet);
    content.addAndMakeVisible (presetStatus);
    content.addAndMakeVisible (abLabel);

    // ALTER EGOS: each button applies a curated identity; every control below stays editable.
    egoTitle.setJustificationType (juce::Justification::centredLeft);
    egoTitle.setColour (juce::Label::textColourId, violet);
    egoTitle.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    egoInfo.setJustificationType (juce::Justification::centredLeft);
    egoInfo.setFont (juce::FontOptions (11.0f));
    egoInfo.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    content.addAndMakeVisible (egoTitle);
    content.addAndMakeVisible (egoInfo);
    for (int i = 0; i < AlterEgos::count; ++i)
    {
        auto& b = egoButtons[i];
        b.setButtonText (AlterEgos::info (i).name);
        b.setClickingTogglesState (false);
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1e1e22));
        b.setColour (juce::TextButton::buttonOnColourId, lime);
        b.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        b.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff121214));
        const bool available = AlterEgos::isAvailable (i);
        b.setEnabled (available);
        b.setTooltip (available ? juce::String (AlterEgos::info (i).description)
                                : juce::String ("Unavailable: one of the effects this Alter Ego needs is not in this build."));
        b.onClick = [this, i]
        {
            const auto s = glitchProcessor.getAlterEgoSelection();
            glitchProcessor.applyAlterEgo (i, s.chaos, 0); // keeps your Chaos setting, drops any Surprise variation
            refreshAlterEgos();
        };
        content.addAndMakeVisible (b);
    }

    // MOOD ENGINE: CHAOS (intensity of the selected Alter Ego), BLEND (the main dry/processed Mix), SURPRISE ME (seeded variation).
    for (auto* l : { &chaosLabel, &blendLabel })
    {
        l->setJustificationType (juce::Justification::centredLeft);
        l->setFont (juce::FontOptions (11.0f, juce::Font::bold));
        content.addAndMakeVisible (l);
    }
    chaosSlider.setRange (0.0, 100.0, 1.0);
    chaosSlider.setTextValueSuffix (" %");
    chaosSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18);
    chaosSlider.setColour (juce::Slider::trackColourId, pink);
    chaosSlider.setColour (juce::Slider::thumbColourId, pink);
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
    blendSlider.setColour (juce::Slider::trackColourId, violet);
    blendSlider.setColour (juce::Slider::thumbColourId, violet);
    blendSlider.setTooltip ("Dry original (0) to the fully processed sound (100). This is the plug-in's main Mix control.");
    surpriseButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1e1e22));
    surpriseButton.setColour (juce::TextButton::textColourOffId, pink);
    surpriseButton.setTooltip ("A new seeded variation of the selected Alter Ego (small, safe changes). Click again for another.");
    surpriseButton.onClick = [this]
    {
        const auto s = glitchProcessor.getAlterEgoSelection();
        glitchProcessor.surprise (AlterEgos::nextSeed (s.seed ^ (std::uint32_t) juce::Time::getMillisecondCounter()));
        refreshAlterEgos();
    };
    for (auto* c : std::initializer_list<juce::Component*> { &chaosSlider, &blendSlider, &surpriseButton })
        content.addAndMakeVisible (c);

    // Liquid Echo. TIME is used when SYNC is off, DIVISION when it is on (the unused one is dimmed).
    if (auto* div = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (GlitchKidProcessor::echoDivId)))
    {
        echoDivBox.addItemList (div->choices, 1);
        echoDivBox.setSelectedItemIndex (div->getIndex(), juce::dontSendNotification);
    }
    echoTitle.setJustificationType (juce::Justification::centred);
    echoTitle.setColour (juce::Label::textColourId, violet);
    echoDivLabel.setJustificationType (juce::Justification::centred);
    echoDivLabel.setFont (juce::FontOptions (11.0f));
    echoOnButton.setColour (juce::ToggleButton::tickColourId, lime);
    echoSyncButton.setColour (juce::ToggleButton::tickColourId, violet);
    echoKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::echoTimeId, "TIME", " ms", lime));
    echoKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::echoFbId, "FEEDBACK", " %", pink));
    echoKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::echoToneId, "TONE", " Hz", violet));
    echoKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::echoWidthId, "WIDTH", " %", lime));
    echoKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::echoMixId, "ECHO MIX", " %", pink));
    for (auto* k : echoKnobs)
        k->addTo (content);
    for (auto* c : std::initializer_list<juce::Component*> { &echoTitle, &echoDivLabel, &echoOnButton, &echoSyncButton, &echoDivBox })
        content.addAndMakeVisible (c);

    // Pitch Tune: pulls the sung note to the nearest note of the chosen key and scale. STRENGTH = how much of the distance is
    // corrected; SPEED = how fast the correction glides (0 = instant, the "hard tune" sound). Adds about 31 ms of latency while ON.
    for (const char* id : { GlitchKidProcessor::tuneKeyId, GlitchKidProcessor::tuneScaleId })
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (id)))
        {
            auto& box = id == GlitchKidProcessor::tuneKeyId ? tuneKeyBox : tuneScaleBox;
            box.addItemList (choice->choices, 1);
            box.setSelectedItemIndex (choice->getIndex(), juce::dontSendNotification);
        }
    tuneTitle.setJustificationType (juce::Justification::centred);
    tuneTitle.setColour (juce::Label::textColourId, violet);
    for (auto* l : { &tuneKeyLabel, &tuneScaleLabel })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setFont (juce::FontOptions (11.0f));
        content.addAndMakeVisible (l);
    }
    tuneReadout.setJustificationType (juce::Justification::centredTop);
    tuneReadout.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    tuneReadout.setColour (juce::Label::textColourId, lime);
    tuneOnButton.setColour (juce::ToggleButton::tickColourId, lime);
    tuneOnButton.setTooltip ("Vocal pitch correction for a single voice (adds about 31 ms of latency while on). Sing or play a dry vocal.");
    tuneKeyBox.setTooltip ("The key (root note) of the song.");
    tuneScaleBox.setTooltip ("Which notes the voice may be pulled to. Chromatic = any semitone.");
    tuneKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::tuneStrengthId, "STRENGTH", " %", lime));
    tuneKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::tuneSpeedId, "SPEED", " ms", pink));
    // FORMANT (vocal "size"): works on its own, whether or not Pitch Tune is on. Negative = bigger / darker voice, positive = smaller / brighter.
    // The notes stay the same. Adds about 21 ms of latency while it is away from 0.
    tuneKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::formantShiftId, "FORMANT", " st", violet));
    for (auto* k : tuneKnobs)
        k->addTo (content);
    for (auto* c : std::initializer_list<juce::Component*> { &tuneTitle, &tuneReadout, &tuneOnButton, &tuneKeyBox, &tuneScaleBox })
        content.addAndMakeVisible (c);

    // Vocal Mutation: a frequency shifter / ring modulator, not a distortion. MUTATION = how far every partial of the voice is moved in Hz
    // (0 = no change); CHARACTER = DOWN (alien, dark) / ROBOT (ring modulation) / UP (alien, bright); BLEND = dry to fully mutated. No latency.
    mutTitle.setJustificationType (juce::Justification::centred);
    mutTitle.setColour (juce::Label::textColourId, violet);
    mutReadout.setJustificationType (juce::Justification::centredTop);
    mutReadout.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    mutReadout.setColour (juce::Label::textColourId, lime);
    mutOnButton.setColour (juce::ToggleButton::tickColourId, lime);
    mutOnButton.setTooltip ("Turns the voice robotic or alien by moving every partial by a fixed number of Hz (a frequency shifter / ring modulator). No latency.");
    mutKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::mutAmountId, "MUTATION", " %", pink));
    mutKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::mutCharacterId, "CHARACTER", " %", violet));
    mutKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::mutBlendId, "MUTATION BLEND", " %", lime));
    for (auto* k : mutKnobs)
        k->addTo (content);
    for (auto* c : std::initializer_list<juce::Component*> { &mutTitle, &mutReadout, &mutOnButton })
        content.addAndMakeVisible (c);

    // DOUBLE / HARMONY. DOUBLE: a synthetic double (two drifting, delayed copies) for thickness and width; WIDTH 100 is mono-safe; DETUNE = the largest pitch
    // wobble of a double; TIMING = how far behind the lead the doubles sit. No latency. HARMONY: one voice at a FIXED interval (UP/DOWN is in the name); it is not
    // scale-aware (a third is always a major or always a minor third); adds the Pitch Tune delay while on.
    dhTitle.setJustificationType (juce::Justification::centred);
    dhTitle.setColour (juce::Label::textColourId, violet);
    dblOnButton.setColour (juce::ToggleButton::tickColourId, lime);
    harmOnButton.setColour (juce::ToggleButton::tickColourId, pink);
    dblOnButton.setTooltip ("A synthetic double of the vocal: two slightly late, slightly unsteady copies for thickness and width. Not a second recorded take. No latency.");
    harmOnButton.setTooltip ("One extra voice at a fixed interval above or below the lead (prototype quality, not scale-aware). Adds about 31 ms of latency while on.");
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (GlitchKidProcessor::harmIntervalId)))
    {
        harmIntervalBox.addItemList (choice->choices, 1);
        harmIntervalBox.setSelectedItemIndex (choice->getIndex(), juce::dontSendNotification);
    }
    harmIntervalBox.setTooltip ("How far above (UP) or below (DOWN) the lead the harmony sings, in semitones. A fixed interval: it does not know your key.");
    harmIntervalLabel.setJustificationType (juce::Justification::centred);
    harmIntervalLabel.setFont (juce::FontOptions (11.0f));
    harmNote.setJustificationType (juce::Justification::centredTop);
    harmNote.setFont (juce::FontOptions (10.0f));
    harmNote.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    dhKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::dblAmountId, "DOUBLE", " %", lime));
    dhKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::dblWidthId, "WIDTH", " %", violet));
    dhKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::dblDetuneId, "DETUNE", " cents", pink));
    dhKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::dblTimingId, "TIMING", " ms", lime));
    dhKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::harmLevelId, "HARMONY LEVEL", " dB", pink));
    for (auto* k : dhKnobs)
        k->addTo (content);
    for (auto* c : std::initializer_list<juce::Component*> { &dhTitle, &harmIntervalLabel, &harmNote, &dblOnButton, &harmOnButton, &harmIntervalBox })
        content.addAndMakeVisible (c);

    // Dimension: stereo thickening. WIDTH 0 = centred thickening, 100 = stereo spread; DETUNE = pitch swing in cents.
    dimTitle.setJustificationType (juce::Justification::centred);
    dimTitle.setColour (juce::Label::textColourId, violet);
    dimOnButton.setColour (juce::ToggleButton::tickColourId, lime);
    dimKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::dimWidthId, "WIDTH", " %", lime));
    dimKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::dimDetuneId, "DETUNE", " cents", pink));
    dimKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::dimDepthId, "DEPTH", " %", violet));
    dimKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::dimMixId, "DIMENSION MIX", " %", pink));
    for (auto* k : dimKnobs)
        k->addTo (content);
    content.addAndMakeVisible (dimTitle);
    content.addAndMakeVisible (dimOnButton);

    gainKnob.add (new ParamKnob (p.apvts, GlitchKidProcessor::gainId, "OUTPUT GAIN", " dB", pink));
    gainKnob[0]->addTo (content);
    driveKnob.add (new ParamKnob (p.apvts, GlitchKidProcessor::satDriveId, "SATURATION DRIVE", " dB", lime));
    driveKnob[0]->addTo (content);

    const char* names[] = { "LOW SHELF", "BELL 1", "BELL 2", "HIGH SHELF" };
    for (int b = 0; b < ParametricEq::numBands; ++b)
    {
        bandNames[b].setText (names[b], juce::dontSendNotification);
        bandNames[b].setJustificationType (juce::Justification::centred);
        bandNames[b].setColour (juce::Label::textColourId, violet);
        content.addAndMakeVisible (bandNames[b]);

        const auto& ids = GlitchKidProcessor::eqIds[b];
        eqKnobs[b].add (new ParamKnob (p.apvts, ids.freq, "FREQ", " Hz", lime));
        eqKnobs[b].add (new ParamKnob (p.apvts, ids.gain, "GAIN", " dB", pink));
        if (ids.q != nullptr)
            eqKnobs[b].add (new ParamKnob (p.apvts, ids.q, "Q", "", violet));
        for (auto* k : eqKnobs[b])
            k->addTo (content);
    }

    // Compressor: threshold, ratio, makeup on the first row; attack, release, knee on the second.
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compThresholdId, "THRESHOLD", " dB", lime));
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compRatioId, "RATIO", ":1", pink));
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compMakeupId, "MAKEUP", " dB", violet));
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compAttackId, "ATTACK", " ms", lime));
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compReleaseId, "RELEASE", " ms", pink));
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compKneeId, "KNEE", " dB", violet));
    for (auto* k : compKnobs)
        k->addTo (content);

    compTitle.setJustificationType (juce::Justification::centred);
    compTitle.setColour (juce::Label::textColourId, violet);
    grLabel.setJustificationType (juce::Justification::centred);
    grLabel.setFont (juce::FontOptions (11.0f));

    for (auto* c : std::initializer_list<juce::Component*> { &title, &tagline, &bypassButton, &inMeter, &outMeter,
                                                              &inLabel, &outLabel, &grMeter, &compTitle, &grLabel })
        content.addAndMakeVisible (c);

    // ---- HOME screen -------------------------------------------------------------------------------------------------------------
    title.setVisible (false); // the title is painted (with a gradient) by paint(); the old labels stay only so nothing else changes
    tagline.setVisible (false);
    struct WorldInfo { const char* title; const char* tagline; glitchkid::ui::WorldCard::Icon icon; juce::Colour colour; };
    const WorldInfo worlds[glitchkid::FactoryPresets::count] = {
        { "ASTRO MODE", "Dark, spacious, psychedelic", glitchkid::ui::WorldCard::Icon::planet, violet },
        { "HEARTBREAK", "Intimate, emotional, close", glitchkid::ui::WorldCard::Icon::heart, pink },
        { "DIGITAL ALTER EGO", "Playful, futuristic, glitchy", glitchkid::ui::WorldCard::Icon::glitch, lime } };
    for (int i = 0; i < glitchkid::FactoryPresets::count; ++i)
    {
        auto* card = worldCards.add (new glitchkid::ui::WorldCard (worlds[i].title, worlds[i].tagline, worlds[i].icon, worlds[i].colour));
        card->setLoads (glitchkid::FactoryPresets::info (i).name);
        card->setTooltip (juce::String (glitchkid::FactoryPresets::info (i).description) + ". Loads the built-in preset; every control stays editable.");
        card->onClick = [this, i]
        {
            const auto outcome = presets.loadFactory (i);
            showStatus (outcome.message);
            refreshAlterEgos();
            shownPresetText = {};
            refreshPresetBar();
            mascot.cheer();
        };
        content.addAndMakeVisible (card);
    }

    const juce::Colour macroColours[glitchkid::macros::numMacros] = { violet, lime, glitchkid::ui::chrome };
    for (int i = 0; i < glitchkid::macros::numMacros; ++i)
    {
        const auto m = (glitchkid::macros::Macro) i;
        auto* dial = macroDials.add (new glitchkid::ui::MacroDial (glitchkid::macros::macroName (m), macroColours[i]));
        dial->setTooltip (juce::String (glitchkid::macros::macroDescription (m)) + " Drag up or down (Shift = fine), mouse wheel, arrow keys; double-click resets.");
        dial->onChange = [this, m] (float v)
        {
            auto s = glitchProcessor.captureSoundState();
            glitchkid::macros::apply (m, v, s);
            glitchProcessor.applyParameterChanges (s);
        };
        content.addAndMakeVisible (dial);
    }
    chaosDial = std::make_unique<glitchkid::ui::MacroDial> ("CHAOS", pink);
    chaosDial->setTooltip ("How far the selected Alter Ego is pushed (the same Chaos as in the LAB). Needs an Alter Ego.");
    chaosDial->onChange = [this] (float v) { glitchProcessor.setChaos (v); };
    content.addAndMakeVisible (*chaosDial);

    for (int i = 0; i < glitchkid::macros::numCommands; ++i)
    {
        const auto c = (glitchkid::macros::Command) i;
        auto& b = commandButtons[i];
        b.setButtonText (glitchkid::macros::commandLabel (c));
        b.setTooltip (glitchkid::macros::commandDescription (c));
        b.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        b.onClick = [this, c]
        {
            auto s = glitchProcessor.captureSoundState();
            glitchkid::macros::apply (c, s);
            const int n = glitchProcessor.applyParameterChanges (s);
            showStatus (juce::String (glitchkid::macros::commandLabel (c)) + ": " + (n > 0 ? juce::String (n) + " controls moved." : juce::String ("already at its limit.")));
            mascot.cheer();
        };
        content.addAndMakeVisible (b);
    }
    talkNote.setText ("Fixed buttons, not chat: each makes one bounded change you can undo with A/B. Typed requests are not implemented yet.", juce::dontSendNotification);
    talkNote.setFont (juce::FontOptions (11.0f));
    talkNote.setColour (juce::Label::textColourId, glitchkid::ui::textDim);
    talkCaption.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    talkCaption.setColour (juce::Label::textColourId, glitchkid::ui::textDim);
    compareCaption.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    compareCaption.setJustificationType (juce::Justification::centredLeft);

    styleBarButton (dryButton, juce::Colours::white, "Hear the untouched input. This is the real bypass (delay-matched and crossfaded over 20 ms), not the Mix knob.");
    styleBarButton (processedButton, juce::Colours::white, "Hear the processed sound.");
    dryButton.setColour (juce::TextButton::buttonOnColourId, pink);
    processedButton.setColour (juce::TextButton::buttonOnColourId, lime);
    dryButton.onClick = [this] { setBypassFromUi (true); };
    processedButton.onClick = [this] { setBypassFromUi (false); };
    styleBarButton (motionButton, juce::Colours::white, "Switch animations on or off. Off = a calm, static interface (meters and levels still update).");
    motionButton.onClick = [this]
    {
        motionOn = ! motionOn;
        glitchProcessor.setUiFlag ("uiMotion", motionOn);
        shownCompareText = {};
    };
    buildPageControls();
    for (auto* c : std::initializer_list<juce::Component*> { &mascot, &scope, &talkCaption, &talkNote, &compareCaption })
        content.addAndMakeVisible (c);

    // Everything lives in `content`, which is laid out at the design size and scaled to fit the window.
    addAndMakeVisible (content);
    addMouseListener (this, true); // any click anywhere fires a glitch burst (mouseDown)
    setResizable (true, true);
    setResizeLimits (designW * 3 / 5, designH * 3 / 5, designW * 3 / 2, designH * 3 / 2); // below ~60 % the text is too small to read
    getConstrainer()->setFixedAspectRatio ((double) designW / (double) designH);
    refreshAlterEgos();
    setSize (designW * 84 / 100, designH * 84 / 100);
    showPage (page, false);
    timerCallback(); // fill the tempo line immediately
    startTimerHz (30);
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

// ---- Presets and A/B -----------------------------------------------------------------------------------------------------------------

GlitchKidEditor::~GlitchKidEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void GlitchKidEditor::buildPageControls()
{
    auto style = [this] (juce::TextButton& b, const juce::String& tip)
    {
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1c1c26));
        b.setColour (juce::TextButton::buttonOnColourId, lime);
        b.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        b.setColour (juce::TextButton::textColourOnId, glitchkid::ui::obsidian);
        b.setTooltip (tip);
        content.addAndMakeVisible (b);
    };
    auto primary = [this, &style] (juce::TextButton& b, const juce::String& tip)
    {
        style (b, tip);
        b.setColour (juce::TextButton::buttonColourId, lime.darker (0.15f));
        b.setColour (juce::TextButton::textColourOffId, glitchkid::ui::obsidian);
    };

    static const char* navText[3] = { "1  TUNE", "2  AUTO", "3  MODIFY" };
    static const char* navTip[3] = { "Step 1: tell GlitchKid the key and scale of your beat.", "Step 2: choose a sound world and let AutoMode set up the effects.",
                                     "Step 3: fine-tune one category at a time." };
    for (int i = 0; i < 3; ++i)
    {
        navButtons[i].setButtonText (navText[i]);
        style (navButtons[i], navTip[i]);
        navButtons[i].getProperties().set ("shape", "tab");
        navButtons[i].onClick = [this, i] { showPage ((Page) i, true); };
    }
    static const char* catText[numCategories] = { "PITCH & KEY", "TONE", "DYNAMICS", "SPACE", "CHARACTER" };
    static const char* catTip[numCategories] = { "Pitch Tune, key, scale, tuning style and FORMANT.", "EQ and saturation.", "Compressor, output gain and level meters.",
                                                 "Echo, Dimension, Doubler and Harmony.", "Alter Egos, Chaos, Vocal Mutation and Glitch Stutter." };
    for (int i = 0; i < numCategories; ++i)
    {
        categoryButtons[i].setButtonText (catText[i]);
        style (categoryButtons[i], catTip[i]);
        categoryButtons[i].onClick = [this, i] { showCategory (i, true); };
    }

    primary (nextButton, "Continue to step 2: choose a sound world.");
    nextButton.onClick = [this] { showPage (Page::autoMix, true); };
    primary (modifyButton, "Go to step 3: fine-tune the sound, one category at a time.");
    modifyButton.onClick = [this] { showPage (Page::modify, true); };
    style (backButton, "Back to the sound worlds and AutoMode.");
    backButton.onClick = [this] { showPage (Page::autoMix, true); };
    style (resetButton, "Set every control back to its default (Factory Default). Asks first. Your A/B snapshots are kept.");
    resetButton.setColour (juce::TextButton::textColourOffId, pink);
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
    style (savePresetButton, "Save the current sound as your own preset. Existing presets are never replaced without asking.");
    savePresetButton.setColour (juce::TextButton::textColourOffId, lime);
    savePresetButton.onClick = [this] { askSavePreset(); };

    for (int i = 0; i < glitchkid::macros::numTuneStyles; ++i)
    {
        const auto s = (glitchkid::macros::TuneStyle) i;
        styleButtons[i].setButtonText (glitchkid::macros::tuneStyleLabel (s));
        style (styleButtons[i], glitchkid::macros::tuneStyleDescription (s));
        styleButtons[i].onClick = [this, s]
        {
            auto st = glitchProcessor.captureSoundState();
            glitchkid::macros::apply (s, st);
            glitchProcessor.applyParameterChanges (st);
            mascot.cheer();
        };
    }

    tuneSubline.setText ("GlitchKid does not detect your key. Choose the key and scale of your beat, and Pitch Tune pulls your voice toward those notes.", juce::dontSendNotification);
    tuneSubline.setFont (juce::FontOptions (16.0f));
    tuneSubline.setJustificationType (juce::Justification::centredTop);
    tuneSubline.setColour (juce::Label::textColourId, glitchkid::ui::textDim);
    tuneSummary.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    tuneSummary.setJustificationType (juce::Justification::centred);
    tuneSummary.setColour (juce::Label::textColourId, lime);
    for (auto* l : { &tuneOnCaption, &styleCaption })
    {
        l->setFont (juce::FontOptions (12.0f, juce::Font::bold));
        l->setColour (juce::Label::textColourId, glitchkid::ui::textDim);
    }
    for (auto* l : { &echoTitle, &dimTitle, &dhTitle, &mutTitle, &stutterTitle, &compTitle })
        l->setJustificationType (juce::Justification::centredLeft);
    tuneReadout.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    tuneReadout.setJustificationType (juce::Justification::centred);

    for (auto* c : std::initializer_list<juce::Component*> { &tuneSubline, &tuneSummary, &tuneOnCaption, &styleCaption })
        content.addAndMakeVisible (c);
}

void GlitchKidEditor::showPage (Page p, bool remember)
{
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
    static const char* worldNames[] = { "ASTRO MODE", "HEARTBREAK", "DIGITAL ALTER EGO" };
    static const char* categoryNames[] = { "PITCH & KEY", "TONE", "DYNAMICS", "SPACE", "CHARACTER" };
    if (page == Page::tune)
        return "SET YOUR KEY";
    if (page == Page::modify)
        return categoryNames[juce::jlimit (0, numCategories - 1, category)];
    const int world = glitchkid::FactoryPresets::indexOfName (glitchProcessor.getPresetName());
    return world >= 0 ? worldNames[world] : "PICK A SOUND";
}

void GlitchKidEditor::startBurst()
{
    if (motionOn)
    {
        glitchFrames = 10;
        glitchSeed = (unsigned) glitchRng.nextInt();
    }
}

void GlitchKidEditor::mouseDown (const juce::MouseEvent&)
{
    startBurst();
}

// One 30 Hz tick of the home screen: real input level into the mascot and scope, the dials read back from the controls, the selected Sound World
// from the loaded preset, the dry/processed switch from the Bypass parameter. Nothing here touches the audio thread.
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
    chaosDial->setEnabledLook (sel.ego >= 0, "CHAOS scales the selected Alter Ego. Pick one first: step 3 > CHARACTER > ALTER EGOS.");
    chaosDial->setValue (sel.chaos);
    chaosDial->tick (dt);

    autoAnalyzeButton.setMotion (motionOn);
    autoAnalyzeButton.tick (dt);
    mascot.tick (dt);

    // TUNE: the key summary and the selected tuning style are read from the real parameters (no detection is claimed)
    {
        glitchkid::SoundState tune;
        for (const char* id : { "tune_on", "tune_strength", "tune_speed" })
            tune.set (id, glitchProcessor.apvts.getRawParameterValue (id)->load());
        const int style = glitchkid::macros::readTuneStyle (tune);
        for (int i = 0; i < glitchkid::macros::numTuneStyles; ++i)
            styleButtons[i].setToggleState (i == style, juce::dontSendNotification);
        juce::String key, scale;
        if (auto* k = dynamic_cast<juce::AudioParameterChoice*> (glitchProcessor.apvts.getParameter (GlitchKidProcessor::tuneKeyId)))
            key = k->getCurrentChoiceName();
        if (auto* s = dynamic_cast<juce::AudioParameterChoice*> (glitchProcessor.apvts.getParameter (GlitchKidProcessor::tuneScaleId)))
            scale = s->getCurrentChoiceName();
        const bool on = tune.get ("tune_on", 0.0f) >= 0.5f;
        const auto summary = on ? "YOUR KEY: " + key + " " + scale + "  |  STYLE: " + (style >= 0 ? juce::String (glitchkid::macros::tuneStyleLabel ((glitchkid::macros::TuneStyle) style)) : juce::String ("CUSTOM"))
                                : juce::String ("TUNING IS OFF: your voice is not being pulled to any key");
        if (summary != tuneSummary.getText())
            tuneSummary.setText (summary, juce::dontSendNotification);
    }

    // the glass screen: the title scrambles into place when it changes; glitch bursts come at random moments (and on clicks, see mouseDown)
    {
        const auto target = currentTitle();
        if (target != titleTarget)
        {
            titleTarget = target;
            scrambleFrames = motionOn ? 8 : 0;
            if (scrambleFrames == 0)
                titleShown = target;
            repaint();
        }
        if (scrambleFrames > 0)
        {
            --scrambleFrames;
            static const juce::String glyphs ("#%&@$01<>/");
            juce::String out;
            for (auto ch : titleTarget)
                out += (ch == ' ' || glitchRng.nextFloat() < (float) (8 - scrambleFrames) / 9.0f) ? ch : glyphs[glitchRng.nextInt (glyphs.length())];
            titleShown = scrambleFrames == 0 ? titleTarget : out;
            repaint();
        }
        if (motionOn)
        {
            if (--nextBurstIn <= 0)
            {
                startBurst();
                nextBurstIn = 55 + glitchRng.nextInt (110);
            }
        }
        else
            glitchFrames = 0;
        if (glitchFrames > 0)
        {
            --glitchFrames;
            ++glitchSeed;
            mascot.setBounds (mascotBase.translated (glitchRng.nextInt (13) - 6, 0));
            repaint();
        }
        else if (mascot.getBounds() != mascotBase)
        {
            mascot.setBounds (mascotBase);
            repaint();
        }
    }

    const bool dry = glitchProcessor.apvts.getRawParameterValue (GlitchKidProcessor::bypassId)->load() >= 0.5f;
    const auto compareText = juce::String (dry ? "D" : "P") + (motionOn ? "m" : "s");
    if (compareText != shownCompareText)
    {
        shownCompareText = compareText;
        dryButton.setToggleState (dry, juce::dontSendNotification);
        processedButton.setToggleState (! dry, juce::dontSendNotification);
        compareCaption.setText (dry ? "HEARING: DRY ORIGINAL" : "HEARING: PROCESSED", juce::dontSendNotification);
        compareCaption.setColour (juce::Label::textColourId, dry ? pink : lime);
        motionButton.setButtonText (motionOn ? "MOTION: ON" : "MOTION: OFF");
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

void GlitchKidEditor::refreshPresetBar()
{
    const auto name = glitchProcessor.getPresetName();
    const auto kind = presets.kindOf (name);
    const auto tag = kind == glitchkid::PresetController::Kind::factory ? "FACTORY:  " : kind == glitchkid::PresetController::Kind::user ? "USER:  " : "PRESET:  ";
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
    menu.addSectionHeader ("FACTORY");
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
    inMeter.pushPeak (inPeak);
    refreshHome (inPeak);
    outMeter.pushPeak (glitchProcessor.consumeOutputPeak());
    grMeter.pushReduction (glitchProcessor.consumeGainReductionDb());

    const bool synced = glitchProcessor.apvts.getRawParameterValue (GlitchKidProcessor::echoSyncId)->load() >= 0.5f;
    echoKnobs[0]->setEnabled (! synced);
    echoDivBox.setEnabled (synced);
    echoDivLabel.setAlpha (synced ? 1.0f : 0.4f);

    // Pitch Tune read-out: the sung note, how far off it is, and the note it is pulled to.
    juce::String tuneText = "OFF";
    if (glitchProcessor.apvts.getRawParameterValue (GlitchKidProcessor::tuneOnId)->load() >= 0.5f)
    {
        const float sung = glitchProcessor.getTuneDetectedMidi(), target = glitchProcessor.getTuneTargetMidi();
        if (sung < 0.0f || target < 0.0f)
            tuneText = "listening...";
        else
        {
            const int cents = (int) std::lround ((sung - std::round (sung)) * 100.0f);
            tuneText = "SUNG " + noteName (sung) + (cents >= 0 ? " +" : " ") + juce::String (cents) + "c\n->  " + noteName (target);
        }
    }
    if (tuneText != shownTuneText)
    {
        shownTuneText = tuneText;
        tuneReadout.setText (tuneText, juce::dontSendNotification);
    }

    // Vocal Mutation read-out: computed from the same mapping the DSP uses, so it always matches what is heard.
    juce::String mutText = "OFF";
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
            mutText = juce::String (kind) + "\n" + (hz < 10.0 ? juce::String (hz, 1) : juce::String (juce::roundToInt (hz))) + " Hz";
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
        harmText = on ? "adds " + msText + " ms delay\n(fixed interval)" : "adds " + msText + " ms\nwhen on";
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
}

void GlitchKidEditor::paint (juce::Graphics& g)
{
    namespace ui = glitchkid::ui;
    constexpr float W = (float) designW, H = (float) designH;
    g.fillAll (ui::obsidian);
    g.addTransform (juce::AffineTransform::scale (scaleFactor));

    // atmosphere behind the hull
    juce::ColourGradient haze (ui::violet.withAlpha (0.26f), 140.0f, 30.0f, ui::violet.withAlpha (0.0f), 800.0f, 420.0f, true);
    g.setGradientFill (haze);
    g.fillRect (0, 0, designW, designH);

    // the hull: a dark body with violet edge lighting, bolts and a light strip along the top
    const juce::Rectangle<float> body (6.0f, 6.0f, W - 12.0f, H - 12.0f);
    juce::ColourGradient bodyFill (ui::panelTop, 0.0f, body.getY(), ui::panelBottom.darker (0.3f), 0.0f, body.getBottom(), false);
    g.setGradientFill (bodyFill);
    g.fillRoundedRectangle (body, 40.0f);
    g.setColour (ui::violet.withAlpha (0.6f));
    g.drawRoundedRectangle (body.reduced (0.5f), 40.0f, 2.0f);
    juce::ColourGradient edge (ui::violet.withAlpha (0.0f), 60.0f, 0.0f, ui::violet.withAlpha (0.0f), W - 60.0f, 0.0f, false);
    edge.addColour (0.3, ui::violet);
    edge.addColour (0.5, ui::pink);
    edge.addColour (0.7, ui::violet);
    g.setGradientFill (edge);
    g.fillRect (60.0f, 9.0f, W - 120.0f, 2.0f);
    for (const auto& c : { juce::Point<float> (26.0f, 26.0f), { W - 26.0f, 26.0f }, { 26.0f, H - 26.0f }, { W - 26.0f, H - 26.0f } })
    {
        juce::ColourGradient bolt (juce::Colour (0xffcfd8ea), c.x - 2.0f, c.y - 2.0f, juce::Colour (0xff4b5468), c.x + 5.0f, c.y + 5.0f, false);
        g.setGradientFill (bolt);
        g.fillEllipse (c.x - 6.0f, c.y - 6.0f, 12.0f, 12.0f);
    }

    // the glass screen: deep violet glass, scanlines, a soft vignette and a bezel
    const auto s = screenRect.toFloat();
    juce::ColourGradient glass (juce::Colour (0xff2a0d3a), s.getCentreX(), s.getY(), juce::Colour (0xff0c0710), s.getCentreX(), s.getBottom(), false);
    g.setGradientFill (glass);
    g.fillRoundedRectangle (s, 28.0f);
    g.setColour (juce::Colours::black.withAlpha (0.22f));
    for (float y = s.getY() + 3.0f; y < s.getBottom() - 3.0f; y += 3.0f)
        g.fillRect (s.getX() + 8.0f, y, s.getWidth() - 16.0f, 1.0f);
    juce::ColourGradient vignette (juce::Colours::transparentBlack, s.getCentreX(), s.getCentreY(), juce::Colours::black.withAlpha (0.55f), s.getX(), s.getY(), true);
    g.setGradientFill (vignette);
    g.fillRoundedRectangle (s, 28.0f);
    g.setColour (juce::Colour (0xff20202c));
    g.drawRoundedRectangle (s.expanded (3.0f), 31.0f, 3.0f);
    g.setColour (juce::Colour (0xff3a3a4c));
    g.drawRoundedRectangle (s, 28.0f, 1.5f);

    // the title, painted so it can split and glitch
    const bool tall = page != Page::modify;
    const juce::Rectangle<float> titleRect (screenRect.getX() + 24.0f, screenRect.getY() + (tall ? 52.0f : 54.0f), 720.0f, tall ? 74.0f : 40.0f);
    const float titleSize = tall ? (titleTarget.length() > 14 ? 46.0f : 64.0f) : 34.0f;
    auto drawTitle = [&] (juce::Rectangle<float> r, juce::Colour c)
    {
        g.setColour (c);
        g.setFont (juce::FontOptions (titleSize, juce::Font::bold));
        g.drawText (titleShown, r, juce::Justification::centredLeft, false);
    };
    drawTitle (titleRect.translated (3.0f, 3.0f), ui::pink.withAlpha (0.55f));
    for (const auto& o : { juce::Point<float> (-1.5f, 0.0f), { 1.5f, 0.0f }, { 0.0f, -1.5f }, { 0.0f, 1.5f } })
        drawTitle (titleRect.translated (o.x, o.y), ui::lime.withAlpha (0.20f));
    drawTitle (titleRect, juce::Colour (0xfff6ffd0));

    if (glitchFrames > 0)
    {
        juce::Random r ((int) glitchSeed);
        for (int i = 0; i < 3; ++i)
        {
            const int y0 = (int) (titleRect.getY() + r.nextFloat() * titleRect.getHeight() * 0.8f), hh = 5 + (int) (r.nextFloat() * 14.0f);
            const float dx = (r.nextFloat() - 0.5f) * 24.0f;
            const juce::Graphics::ScopedSaveState state (g);
            g.reduceClipRegion (juce::Rectangle<int> ((int) titleRect.getX() - 30, y0, (int) titleRect.getWidth() + 60, hh));
            drawTitle (titleRect.translated (dx, 0.0f), ui::pink.withAlpha (0.95f));
            drawTitle (titleRect.translated (-dx * 0.7f, 0.0f), juce::Colour (0xff3d7bff).withAlpha (0.9f));
        }
        for (int i = 0; i < 3; ++i) // tear bars
        {
            const float y = s.getY() + 10.0f + r.nextFloat() * (s.getHeight() - 30.0f), hh = 4.0f + r.nextFloat() * 16.0f, dx = (r.nextFloat() - 0.5f) * 80.0f;
            juce::ColourGradient bandFill (ui::lime.withAlpha (0.0f), s.getX() + dx, y, ui::lime.withAlpha (0.0f), s.getRight() + dx, y, false);
            bandFill.addColour (0.4, ui::lime.withAlpha (0.28f));
            bandFill.addColour (0.7, ui::pink.withAlpha (0.28f));
            g.setGradientFill (bandFill);
            g.fillRect (s.getX() + 8.0f, y, s.getWidth() - 16.0f, hh);
        }
    }

    // the brand plate across the lower edge of the tall screen, flanked by vents
    if (tall)
    {
        const juce::Rectangle<float> plate (W * 0.5f - 120.0f, s.getBottom() - 10.0f, 240.0f, 36.0f);
        juce::ColourGradient rim (ui::violet, plate.getX(), plate.getY(), ui::pink, plate.getRight(), plate.getBottom(), false);
        g.setGradientFill (rim);
        g.fillPath (ui::makePlatePath (plate, false));
        juce::ColourGradient inner (juce::Colour (0xff23232f), 0.0f, plate.getY(), juce::Colour (0xff10101a), 0.0f, plate.getBottom(), false);
        g.setGradientFill (inner);
        g.fillPath (ui::makePlatePath (plate.reduced (2.0f), false));
        juce::ColourGradient letters (ui::lime, plate.getX(), 0.0f, ui::pink, plate.getRight(), 0.0f, false);
        letters.addColour (0.55, ui::chrome);
        g.setGradientFill (letters);
        g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
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
        g.fillRoundedRectangle (f, 18.0f);
        g.setColour (ui::violet.withAlpha (0.45f));
        g.drawRoundedRectangle (f.reduced (0.5f), 18.0f, 1.2f);
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

    // ---- the glass screen (tall on TUNE and AUTO, a short title strip on MODIFY), with the step tabs and the mascot ----
    const bool tall = page != Page::modify;
    screenRect = tall ? Rect (40, 26, 980, 316) : Rect (40, 26, 980, 104);
    for (int i = 0; i < 3; ++i)
        show (navButtons[i], { screenRect.getX() + 18 + i * 150, screenRect.getY() + 14, 144, 38 });
    show (motionButton, { tall ? screenRect.getRight() - 150 : screenRect.getRight() - 290, screenRect.getY() + 16, 120, 30 });
    mascotBase = tall ? Rect (786, 108, 196, 196) : Rect (894, 30, 98, 98);
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
    show (abCopyBA, bar.removeFromRight (54));
    bar.removeFromRight (6);
    show (abCopyAB, bar.removeFromRight (54));
    bar.removeFromRight (10);
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

    // ---- step 1: TUNE (the key controls sit on the glass; NEXT is on the deck) ----
    if (page == Page::tune)
    {
        const int sx = screenRect.getX() + 24, sy = screenRect.getY();
        show (tuneSubline, { sx, sy + 118, 660, 36 });
        show (tuneKeyLabel, { sx, sy + 158, 150, 16 });
        show (tuneKeyBox, { sx, sy + 176, 170, 38 });
        show (tuneScaleLabel, { sx + 190, sy + 158, 160, 16 });
        show (tuneScaleBox, { sx + 190, sy + 176, 250, 38 });
        show (tuneOnCaption, { sx + 460, sy + 158, 100, 16 });
        show (tuneOnButton, { sx + 460, sy + 176, 110, 38 });
        show (styleCaption, { sx, sy + 220, 200, 16 });
        for (int i = 0; i < glitchkid::macros::numTuneStyles; ++i)
            show (styleButtons[i], { sx + i * 160, sy + 238, 150, 38 });
        show (tuneSummary, { sx, sy + 282, 700, 20 });
        show (scope, { 150, 392, 760, 44 });
        show (tuneReadout, { 150, 442, 760, 34 });
        show (nextButton, { 380, 496, 300, 64 });
    }

    // ---- step 2: AUTO (the console: reactor, macro dials, audition plates, sound-world pods) ----
    else if (page == Page::autoMix)
    {
        const int sx = screenRect.getX() + 24, sy = screenRect.getY();
        show (autoStatus, { sx, sy + 126, 660, 64 });
        show (scope, { sx, sy + 198, 560, 40 });
        show (tuneSummary, { sx, sy + 282, 700, 20 });

        show (autoAnalyzeButton, { 54, 380, 134, 134 });
        show (autoStyleLabel, { 36, 516, 206, 14 });
        show (autoIntensitySlider, { 36, 532, 206, 24 });
        show (autoApplyButton, { 36, 554, 206, 30 });
        for (int i = 0; i < macroDials.size(); ++i)
            show (*macroDials[i], { 262 + i * 140, 380, 132, 170 });
        show (*chaosDial, { 262 + 3 * 140, 380, 132, 170 });
        show (dryButton, { 830, 388, 180, 40 });
        show (processedButton, { 830, 432, 180, 40 });
        show (compareCaption, { 830, 476, 180, 20 });
        show (modifyButton, { 830, 504, 180, 52 });
        for (int i = 0; i < worldCards.size(); ++i)
            show (*worldCards[i], { 40 + i * 332, 588, 316, 68 });
    }

    // ---- step 3: MODIFY (one category at a time) ----
    else
    {
        constexpr int oy = 16; // the category layouts below were drawn for a panel starting at y = 164
        const int catW = (designW - 80 - 4 * 8) / numCategories;
        for (int i = 0; i < numCategories; ++i)
        {
            show (categoryButtons[i], { 40 + i * (catW + 8), 138, catW, 38 });
            categoryButtons[i].setToggleState (i == category, juce::dontSendNotification);
        }
        panelRects.push_back ({ 16, 184, designW - 32, 372 });

        // quick tweaks + footer
        show (talkCaption, { 24, 560, 150, 36 });
        for (int i = 0; i < glitchkid::macros::numCommands; ++i)
            show (commandButtons[i], { 172 + i * 156, 560, 148, 36 });
        show (talkNote, { 800, 556, 244, 44 });
        show (backButton, { 16, 606, 200, 44 });
        show (resetButton, { 226, 606, 110, 44 });
        show (savePresetButton, { 346, 606, 160, 44 });
        show (dryButton, { 560, 606, 120, 44 });
        show (processedButton, { 680, 606, 150, 44 });
        show (compareCaption, { 840, 606, 204, 44 });

        switch ((Category) category)
        {
            case Category::pitch:
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
                    showK (*tuneKnobs[i], { 30 + i * 180, 262 + oy, 170, 210 });
                show (tuneSummary, { 580, 282 + oy, 440, 36 });
                show (tuneReadout, { 580, 326 + oy, 440, 50 });
                break;

            case Category::tone:
                for (int b = 0; b < ParametricEq::numBands; ++b)
                {
                    show (bandNames[b], { 30 + b * 200, 176 + oy, 190, 18 });
                    for (int k = 0; k < eqKnobs[b].size(); ++k)
                        showK (*eqKnobs[b][k], { 30 + b * 200 + 10, 198 + k * 112 + oy, 170, 108 });
                }
                showK (*driveKnob[0], { 850, 220 + oy, 180, 220 });
                break;

            case Category::dynamics:
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

            case Category::space:
                show (echoTitle, { 30, 174 + oy, 140, 18 });
                show (echoOnButton, { 30, 196 + oy, 100, 30 });
                show (echoSyncButton, { 30, 234 + oy, 100, 30 });
                show (echoDivLabel, { 150, 196 + oy, 120, 14 });
                show (echoDivBox, { 150, 214 + oy, 130, 32 });
                for (int i = 0; i < echoKnobs.size(); ++i)
                    showK (*echoKnobs[i], { 300 + i * 148, 192 + oy, 140, 98 });
                show (dimTitle, { 30, 292 + oy, 140, 18 });
                show (dimOnButton, { 30, 316 + oy, 100, 30 });
                for (int i = 0; i < dimKnobs.size(); ++i)
                    showK (*dimKnobs[i], { 300 + i * 148, 310 + oy, 140, 96 });
                show (dhTitle, { 30, 408 + oy, 200, 18 });
                show (dblOnButton, { 30, 432 + oy, 150, 30 });
                show (harmOnButton, { 30, 470 + oy, 150, 30 });
                for (int i = 0; i < 4; ++i)
                    showK (*dhKnobs[i], { 190 + i * 128, 428 + oy, 120, 96 });
                show (harmIntervalLabel, { 716, 430 + oy, 150, 14 });
                show (harmIntervalBox, { 716, 448 + oy, 170, 32 });
                show (harmNote, { 716, 484 + oy, 170, 36 });
                showK (*dhKnobs[4], { 896, 428 + oy, 130, 96 });
                break;

            case Category::character:
                show (egoTitle, { 30, 178 + oy, 92, 28 });
                for (int i = 0; i < AlterEgos::count; ++i)
                    show (egoButtons[i], { 126 + i * 128, 178 + oy, 122, 30 });
                show (egoInfo, { 126, 212 + oy, 900, 34 });
                show (chaosLabel, { 30, 250 + oy, 60, 28 });
                show (chaosSlider, { 94, 250 + oy, 300, 28 });
                show (blendLabel, { 420, 250 + oy, 56, 28 });
                show (blendSlider, { 480, 250 + oy, 300, 28 });
                show (surpriseButton, { 810, 249 + oy, 170, 30 });
                show (mutTitle, { 30, 292 + oy, 200, 18 });
                show (mutOnButton, { 30, 316 + oy, 100, 30 });
                show (mutReadout, { 30, 352 + oy, 150, 40 });
                for (int i = 0; i < mutKnobs.size(); ++i)
                    showK (*mutKnobs[i], { 200 + i * 170, 312 + oy, 160, 98 });
                show (stutterTitle, { 30, 418 + oy, 200, 18 });
                show (stutterOnButton, { 30, 442 + oy, 100, 30 });
                show (tempoLabel, { 30, 478 + oy, 180, 40 });
                show (stutterRateLabel, { 230, 440 + oy, 100, 14 });
                show (stutterRateBox, { 230, 458 + oy, 110, 30 });
                showK (*stutterMixKnob[0], { 400, 436 + oy, 170, 98 });
                break;
        }
    }
}
