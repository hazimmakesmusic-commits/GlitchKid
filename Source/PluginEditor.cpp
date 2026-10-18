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
    labOpen = p.getUiFlag ("uiLabOpen", false);
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
    autoStatus.setFont (juce::FontOptions (11.0f));
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
    for (auto* l : { &worldsCaption, &macrosCaption, &talkCaption })
    {
        l->setFont (juce::FontOptions (13.0f, juce::Font::bold));
        l->setColour (juce::Label::textColourId, glitchkid::ui::textDim);
    }
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
    styleBarButton (labButton, lime, "Show or hide the detailed controls: Alter Ego settings, EQ, compressor, Pitch Tune, Mutation, Doubler, Echo, Dimension.");
    labButton.onClick = [this]
    {
        labOpen = ! labOpen;
        glitchProcessor.setUiFlag ("uiLabOpen", labOpen);
        applyLabState (true);
    };
    for (auto* c : std::initializer_list<juce::Component*> { &mascot, &scope, &statusRing, &worldsCaption, &macrosCaption, &talkCaption, &talkNote, &compareCaption })
        content.addAndMakeVisible (c);

    // Everything lives in `content`, which is laid out at the design size (the LAB part always laid out) and scaled to fit the window.
    layoutContent();
    addAndMakeVisible (content);
    setResizable (true, true);
    refreshAlterEgos();
    applyLabState (false);
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

void GlitchKidEditor::applyLabState (bool resizeWindow)
{
    designH = labOpen ? fullH : homeH;
    labButton.setButtonText (labOpen ? "LAB  ^" : "LAB  v");
    labButton.setToggleState (labOpen, juce::dontSendNotification);
    setResizeLimits (designW / 2, designH / 2, designW * 3 / 2, designH * 3 / 2);
    getConstrainer()->setFixedAspectRatio ((double) designW / (double) designH);
    const int w = resizeWindow && getWidth() > 0 ? getWidth() : designW * (labOpen ? 62 : 78) / 100;
    setSize (w, juce::roundToInt ((double) w * designH / designW));
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
    chaosDial->setEnabledLook (sel.ego >= 0, "CHAOS scales the selected Alter Ego. Pick one first (LAB > ALTER EGOS, or the strip below).");
    chaosDial->setValue (sel.chaos);
    chaosDial->tick (dt);

    statusRing.setMotion (motionOn);
    statusRing.tick (dt);
    mascot.tick (dt);

    const bool dry = glitchProcessor.apvts.getRawParameterValue (GlitchKidProcessor::bypassId)->load() >= 0.5f;
    const auto compareText = juce::String (dry ? "D" : "P") + (motionOn ? "m" : "s");
    if (compareText != shownCompareText)
    {
        shownCompareText = compareText;
        dryButton.setToggleState (dry, juce::dontSendNotification);
        processedButton.setToggleState (! dry, juce::dontSendNotification);
        compareCaption.setText (dry ? "HEARING: DRY ORIGINAL (bypassed)" : "HEARING: PROCESSED", juce::dontSendNotification);
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
    auto ring = glitchkid::ui::StatusRing::State::idle;
    float ringProgress = 0.0f;
    auto mood = glitchkid::ui::Mascot::Mood::idle;
    if (listening)
    {
        ring = glitchkid::ui::StatusRing::State::working;
        ringProgress = an.liveActiveSeconds() / glitchkid::analysis::VocalAnalyzer::maxActiveSeconds;
        mood = glitchkid::ui::Mascot::Mood::listening;
    }
    else if (autoWaiting)
        ring = glitchkid::ui::StatusRing::State::waiting;
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
                ring = glitchkid::ui::StatusRing::State::ready;
                mood = glitchkid::ui::Mascot::Mood::ready;
                text = "Ready: " + juce::String (a.activeSeconds, 1) + " s of vocal, peak " + juce::String (a.peakDb, 1) + " dB, level " + juce::String (a.rmsDb, 1)
                       + " dB" + (a.clippedSamples >= 3 ? ", CLIPPING found" : "") + ". Set INTENSITY, then APPLY.";
                break;
            case S::tooShort: ring = glitchkid::ui::StatusRing::State::error; mood = glitchkid::ui::Mascot::Mood::error;  text = "Too short: only " + juce::String (a.activeSeconds, 1) + " s of vocal heard (needs 2 s). Nothing changed. Try again."; break;
            case S::tooQuiet: ring = glitchkid::ui::StatusRing::State::error; mood = glitchkid::ui::Mascot::Mood::error;  text = "No usable vocal heard (silent or very quiet input). Nothing changed."; break;
            case S::invalid: ring = glitchkid::ui::StatusRing::State::error; mood = glitchkid::ui::Mascot::Mood::error;   text = "The input contained invalid audio. Nothing changed. Try again."; break;
            case S::idle:
            case S::listening: text = "Press ANALYZE, then play the dry vocal for a few seconds."; break;
        }
    }
    statusRing.setState (ring, ringProgress);
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
    g.fillAll (glitchkid::ui::obsidian);
    g.addTransform (juce::AffineTransform::scale (scaleFactor));

    // atmosphere: a violet haze top-left, a pink one bottom-right (cheap radial gradients, painted once per repaint)
    juce::ColourGradient haze (glitchkid::ui::violet.withAlpha (0.22f), 120.0f, 40.0f, glitchkid::ui::violet.withAlpha (0.0f), 760.0f, 420.0f, true);
    g.setGradientFill (haze);
    g.fillRect (0, 0, designW, designH);
    juce::ColourGradient haze2 (glitchkid::ui::pink.withAlpha (0.10f), (float) designW, (float) homeH, glitchkid::ui::pink.withAlpha (0.0f), (float) designW - 520.0f, (float) homeH - 260.0f, true);
    g.setGradientFill (haze2);
    g.fillRect (0, 0, designW, designH);

    // title: liquid-chrome gradient lettering
    juce::ColourGradient letters (glitchkid::ui::lime, 130.0f, 18.0f, glitchkid::ui::pink, 520.0f, 80.0f, false);
    letters.addColour (0.5, glitchkid::ui::chrome);
    g.setGradientFill (letters);
    g.setFont (juce::FontOptions (50.0f, juce::Font::bold));
    g.drawText ("GLITCHKID", 128, 10, 430, 58, juce::Justification::centredLeft);
    g.setColour (glitchkid::ui::textDim);
    g.setFont (juce::FontOptions (15.0f));
    g.drawText ("A playground for your voice.", 132, 66, 420, 22, juce::Justification::centredLeft);

    // thin section rules on the home screen
    g.setColour (juce::Colour (0xff2a2a36));
    g.drawLine (16.0f, 152.0f, (float) designW - 16.0f, 152.0f);
    g.drawLine (16.0f, 358.0f, (float) designW - 16.0f, 358.0f);
    g.drawLine (16.0f, 600.0f, (float) designW - 16.0f, 600.0f);
    g.drawLine ((float) (16 + 4 * 180 + 6), 366.0f, (float) (16 + 4 * 180 + 6), 552.0f);

    if (labOpen)
    {
        g.setColour (juce::Colour (0xff2a2a36));
        g.drawLine (270.0f, (float) labTopY, 270.0f, (float) tuneDividerY - 8.0f);
        g.drawLine ((float) compDividerX, (float) labTopY, (float) compDividerX, (float) tuneDividerY - 8.0f);
        g.drawLine (16.0f, (float) tuneDividerY, (float) designW - 16.0f, (float) tuneDividerY);
        g.drawLine (16.0f, (float) mutDividerY, (float) designW - 16.0f, (float) mutDividerY);
        g.drawLine (16.0f, (float) dhDividerY, (float) designW - 16.0f, (float) dhDividerY);
        g.drawLine (16.0f, (float) echoDividerY, (float) designW - 16.0f, (float) echoDividerY);
        g.drawLine (16.0f, (float) dimDividerY, (float) designW - 16.0f, (float) dimDividerY);
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
    auto r = juce::Rectangle<int> (0, 0, designW, fullH).reduced (16); // the LAB part is always laid out; designH only decides how much is shown

    // Bottom strips: Dimension (lowest), then Liquid Echo above it.
    auto dimArea = r.removeFromBottom (132);
    dimDividerY = dimArea.getY() - 4;
    dimTitle.setBounds (dimArea.removeFromTop (20));
    const int dimCell = dimArea.getWidth() / 7;
    dimOnButton.setBounds (dimArea.removeFromLeft (dimCell).reduced (8, 0).removeFromTop (40).withSizeKeepingCentre (70, 28));
    dimArea.removeFromLeft (dimCell); // keeps the knobs aligned with the Liquid Echo row above
    for (auto* k : dimKnobs)
        k->setBounds (dimArea.removeFromLeft (dimCell).reduced (8, 0));

    auto echoArea = r.removeFromBottom (132);
    echoDividerY = echoArea.getY() - 4;
    auto dhArea = r.removeFromBottom (132); // DOUBLE / HARMONY sits between Vocal Mutation and Liquid Echo
    dhDividerY = dhArea.getY() - 4;
    dhTitle.setBounds (dhArea.removeFromTop (20));
    const int dhCell = dhArea.getWidth() * 2 / 17; // 8.5 cells: switch, 4 knobs, switch, interval (1.5), level knob
    auto dblSwitch = dhArea.removeFromLeft (dhCell).reduced (4, 0);
    dblOnButton.setBounds (dblSwitch.removeFromTop (40).withSizeKeepingCentre (dblSwitch.getWidth(), 28));
    for (int k = 0; k < 4; ++k)
        dhKnobs[k]->setBounds (dhArea.removeFromLeft (dhCell).reduced (8, 0));
    auto harmSwitch = dhArea.removeFromLeft (dhCell).reduced (4, 0);
    harmOnButton.setBounds (harmSwitch.removeFromTop (40).withSizeKeepingCentre (harmSwitch.getWidth(), 28));
    harmNote.setBounds (harmSwitch.removeFromTop (30));
    auto intervalCell = dhArea.removeFromLeft (dhCell + dhCell / 2).reduced (8, 0);
    harmIntervalLabel.setBounds (intervalCell.removeFromTop (14));
    harmIntervalBox.setBounds (intervalCell.removeFromTop (40).withSizeKeepingCentre (intervalCell.getWidth(), 26));
    dhKnobs[4]->setBounds (dhArea.removeFromLeft (dhCell).reduced (8, 0));
    auto mutArea = r.removeFromBottom (132); // Vocal Mutation sits between Pitch Tune / FORMANT and the EQ in the signal path; its row is shown above Liquid Echo
    mutDividerY = mutArea.getY() - 4;
    mutTitle.setBounds (mutArea.removeFromTop (20));
    const int mutCell = mutArea.getWidth() / 7;
    auto mutSwitch = mutArea.removeFromLeft (mutCell).reduced (8, 0);
    mutOnButton.setBounds (mutSwitch.removeFromTop (40).withSizeKeepingCentre (70, 28));
    mutReadout.setBounds (mutSwitch.removeFromTop (40));
    for (auto* k : mutKnobs)
        k->setBounds (mutArea.removeFromLeft (mutCell).reduced (8, 0));
    auto tuneArea = r.removeFromBottom (132); // above Liquid Echo, as it comes first in the signal path
    tuneDividerY = tuneArea.getY() - 4;
    tuneTitle.setBounds (tuneArea.removeFromTop (20));
    const int tuneCell = tuneArea.getWidth() / 7;
    auto tuneSwitch = tuneArea.removeFromLeft (tuneCell).reduced (8, 0);
    tuneOnButton.setBounds (tuneSwitch.removeFromTop (40).withSizeKeepingCentre (70, 28));
    tuneReadout.setBounds (tuneSwitch.removeFromTop (40));
    auto keyCell = tuneArea.removeFromLeft (tuneCell).reduced (8, 0);
    tuneKeyLabel.setBounds (keyCell.removeFromTop (14));
    tuneKeyBox.setBounds (keyCell.removeFromTop (40).withSizeKeepingCentre (76, 26));
    auto scaleCell = tuneArea.removeFromLeft (tuneCell * 3 / 2).reduced (8, 0);
    tuneScaleLabel.setBounds (scaleCell.removeFromTop (14));
    tuneScaleBox.setBounds (scaleCell.removeFromTop (40).withSizeKeepingCentre (150, 26));
    for (auto* k : tuneKnobs)
        k->setBounds (tuneArea.removeFromLeft (tuneCell).reduced (8, 0));
    // (the read-out sits under the ON switch, in the space the FORMANT knob took)
    echoTitle.setBounds (echoArea.removeFromTop (20));
    const int cellW = echoArea.getWidth() / 7;
    auto switches = echoArea.removeFromLeft (cellW).reduced (8, 0);
    echoOnButton.setBounds (switches.removeFromTop (40).withSizeKeepingCentre (70, 28));
    echoSyncButton.setBounds (switches.removeFromTop (40).withSizeKeepingCentre (70, 28));
    auto divCell = echoArea.removeFromLeft (cellW).reduced (8, 0);
    echoDivLabel.setBounds (divCell.removeFromTop (14));
    echoDivBox.setBounds (divCell.removeFromTop (40).withSizeKeepingCentre (110, 26));
    for (auto* k : echoKnobs)
        k->setBounds (echoArea.removeFromLeft (cellW).reduced (8, 0));

    // ---- HOME screen (absolute design coordinates; the LAB starts at y = homeH) ----
    constexpr int x0 = 16, hw = designW - 32;
    mascot.setBounds (x0, 8, 100, 100);
    scope.setBounds (x0 + 560, 26, hw - 560, 64);

    // PRESETS + A/B bar: preset menu, SAVE, DELETE and a status line on the left; A/B controls on the right.
    juce::Rectangle<int> bar (x0, 116, hw, 30);
    presetPrevButton.setBounds (bar.removeFromLeft (28));
    bar.removeFromLeft (4);
    presetMenuButton.setBounds (bar.removeFromLeft (290));
    bar.removeFromLeft (4);
    presetNextButton.setBounds (bar.removeFromLeft (28));
    bar.removeFromLeft (6);
    presetSaveButton.setBounds (bar.removeFromLeft (64));
    bar.removeFromLeft (6);
    presetDeleteButton.setBounds (bar.removeFromLeft (76));
    bar.removeFromLeft (12);
    abCopyBA.setBounds (bar.removeFromRight (54));
    bar.removeFromRight (6);
    abCopyAB.setBounds (bar.removeFromRight (54));
    bar.removeFromRight (10);
    abRecallB.setBounds (bar.removeFromRight (46));
    bar.removeFromRight (6);
    abRecallA.setBounds (bar.removeFromRight (46));
    bar.removeFromRight (10);
    abSetB.setBounds (bar.removeFromRight (62));
    bar.removeFromRight (6);
    abSetA.setBounds (bar.removeFromRight (62));
    bar.removeFromRight (8);
    abLabel.setBounds (bar.removeFromRight (38));
    presetStatus.setBounds (bar);

    // SOUND WORLDS
    worldsCaption.setBounds (x0, 154, 300, 18);
    const int cardW = (hw - 2 * 8) / 3;
    for (int i = 0; i < worldCards.size(); ++i)
        worldCards[i]->setBounds (x0 + i * (cardW + 8), 172, cardW, 112);

    // Main action: status ring, ANALYZE VOCAL, APPLY SOUND, style + intensity, status text
    statusRing.setBounds (x0, 294, 56, 56);
    autoAnalyzeButton.setBounds (x0 + 66, 300, 200, 44);
    autoApplyButton.setBounds (x0 + 276, 300, 180, 44);
    autoStyleLabel.setBounds (x0 + 474, 294, 260, 18);
    autoIntensityLabel.setBounds (x0 + 474, 316, 70, 28);
    autoIntensitySlider.setBounds (x0 + 546, 316, 210, 28);
    autoStatus.setBounds (x0 + 766, 292, hw - 766, 56);

    // MACROS and TALK TO THE KID
    macrosCaption.setBounds (x0, 360, 300, 18);
    for (int i = 0; i < macroDials.size(); ++i)
        macroDials[i]->setBounds (x0 + i * 180, 380, 176, 172);
    chaosDial->setBounds (x0 + macroDials.size() * 180, 380, 176, 172);
    const int talkX = x0 + 4 * 180 + 14;
    talkCaption.setBounds (talkX, 360, 300, 18);
    for (int i = 0; i < glitchkid::macros::numCommands; ++i)
        commandButtons[i].setBounds (talkX + (i % 2) * 150, 388 + (i / 2) * 56, 142, 46);
    talkNote.setBounds (talkX, 502, designW - 16 - talkX, 48);

    // ALTER EGOS strip
    juce::Rectangle<int> egoRow (x0, 560, hw, 34);
    egoTitle.setBounds (egoRow.removeFromLeft (92));
    for (auto& b : egoButtons)
    {
        b.setBounds (egoRow.removeFromLeft (122).withSizeKeepingCentre (122, 28));
        egoRow.removeFromLeft (6);
    }
    egoRow.removeFromLeft (10);
    egoInfo.setBounds (egoRow);

    // Dry / processed + LAB / MOTION
    dryButton.setBounds (x0, 606, 130, 42);
    processedButton.setBounds (x0 + 130, 606, 170, 42);
    compareCaption.setBounds (x0 + 316, 606, 400, 42);
    motionButton.setBounds (designW - 16 - 128 - 8 - 112, 606, 128, 42);
    labButton.setBounds (designW - 16 - 112, 606, 112, 42);

    r.removeFromTop (homeH - r.getY()); // the LAB starts here

    auto engineRow = r.removeFromTop (34);
    engineRow.removeFromLeft (92);
    chaosLabel.setBounds (engineRow.removeFromLeft (50));
    chaosSlider.setBounds (engineRow.removeFromLeft (290).withSizeKeepingCentre (290, 24));
    engineRow.removeFromLeft (24);
    blendLabel.setBounds (engineRow.removeFromLeft (50));
    blendSlider.setBounds (engineRow.removeFromLeft (290).withSizeKeepingCentre (290, 24));
    engineRow.removeFromLeft (24);
    surpriseButton.setBounds (engineRow.removeFromLeft (130).withSizeKeepingCentre (130, 28));
    r.removeFromTop (6);
    labTopY = r.getY();

    // Left: meters either side of the output-gain knob, Bypass underneath.
    auto left = r.removeFromLeft (240);
    r.removeFromLeft (20);
    bypassButton.setBounds (left.removeFromBottom (32));
    auto inCol = left.removeFromLeft (28), outCol = left.removeFromRight (28);
    inLabel.setBounds (inCol.removeFromBottom (16));
    outLabel.setBounds (outCol.removeFromBottom (16));
    inMeter.setBounds (inCol.reduced (6, 0));
    outMeter.setBounds (outCol.reduced (6, 0));
    auto knobs = left.withSizeKeepingCentre (left.getWidth(), 280); // (the main Mix is now the BLEND slider above)
    driveKnob[0]->setBounds (knobs.removeFromTop (140).withTrimmedBottom (6));
    gainKnob[0]->setBounds (knobs.removeFromTop (140).withTrimmedBottom (6));

    // Middle: four EQ columns, each with its own FREQ / GAIN (/ Q) knobs.
    auto eqArea = r.removeFromLeft (400);
    const int colW = eqArea.getWidth() / ParametricEq::numBands;
    for (int b = 0; b < ParametricEq::numBands; ++b)
    {
        auto col = eqArea.removeFromLeft (colW);
        bandNames[b].setBounds (col.removeFromTop (20));
        for (auto* k : eqKnobs[b])
            k->setBounds (col.removeFromTop (108).reduced (8, 0));
    }

    // Right: compressor knobs (2 rows x 3) and the gain-reduction meter.
    r.removeFromLeft (10);
    compDividerX = r.getX();
    r.removeFromLeft (10);
    compTitle.setBounds (r.removeFromTop (20));
    auto grCol = r.removeFromRight (28);
    grLabel.setBounds (grCol.removeFromBottom (16));
    grMeter.setBounds (grCol.reduced (6, 0));
    const int knobW = r.getWidth() / 3;
    for (int row = 0; row < 2; ++row)
    {
        auto line = r.removeFromTop (108);
        for (int col = 0; col < 3; ++col)
            compKnobs[row * 3 + col]->setBounds (line.removeFromLeft (knobW).reduced (8, 0));
    }

    // Glitch Stutter row: ON + tempo source | RATE | MIX
    r.removeFromTop (6);
    stutterTitle.setBounds (r.removeFromTop (20));
    auto stutterRow = r.removeFromTop (108);
    auto onCell = stutterRow.removeFromLeft (knobW).reduced (8, 0);
    stutterOnButton.setBounds (onCell.removeFromTop (36).withSizeKeepingCentre (70, 28));
    tempoLabel.setBounds (onCell.removeFromTop (44));
    auto rateCell = stutterRow.removeFromLeft (knobW).reduced (8, 0);
    stutterRateLabel.setBounds (rateCell.removeFromTop (14));
    stutterRateBox.setBounds (rateCell.removeFromTop (40).withSizeKeepingCentre (76, 26));
    stutterMixKnob[0]->setBounds (stutterRow.reduced (8, 0));
}
