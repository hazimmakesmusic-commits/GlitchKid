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
      tuneKeyAttachment (p.apvts, GlitchKidProcessor::tuneKeyId, tuneKeyBox),
      tuneScaleAttachment (p.apvts, GlitchKidProcessor::tuneScaleId, tuneScaleBox),
      blendAttachment (p.apvts, GlitchKidProcessor::mixId, blendSlider)
{
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
    tuneReadout.setJustificationType (juce::Justification::centred);
    tuneReadout.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    tuneReadout.setColour (juce::Label::textColourId, lime);
    tuneOnButton.setColour (juce::ToggleButton::tickColourId, lime);
    tuneOnButton.setTooltip ("Vocal pitch correction for a single voice (adds about 31 ms of latency while on). Sing or play a dry vocal.");
    tuneKeyBox.setTooltip ("The key (root note) of the song.");
    tuneScaleBox.setTooltip ("Which notes the voice may be pulled to. Chromatic = any semitone.");
    tuneKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::tuneStrengthId, "STRENGTH", " %", lime));
    tuneKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::tuneSpeedId, "SPEED", " ms", pink));
    for (auto* k : tuneKnobs)
        k->addTo (content);
    for (auto* c : std::initializer_list<juce::Component*> { &tuneTitle, &tuneReadout, &tuneOnButton, &tuneKeyBox, &tuneScaleBox })
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

    // Everything lives in `content`, which is laid out at the design size and scaled to fit the window.
    layoutContent();
    addAndMakeVisible (content);
    setResizable (true, true);
    setResizeLimits (designW / 2, designH / 2, designW * 3 / 2, designH * 3 / 2);
    getConstrainer()->setFixedAspectRatio ((double) designW / (double) designH);
    refreshAlterEgos();
    setSize (designW * 74 / 100, designH * 74 / 100);
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
        text = "CUSTOM. An Alter Ego sets the EQ, compressor, saturation, echo and Dimension; Output Gain, Blend and Stutter are never touched.";
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

void GlitchKidEditor::timerCallback()
{
    refreshAlterEgos();
    inMeter.pushPeak (glitchProcessor.consumeInputPeak());
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
            tuneText = "SUNG " + noteName (sung) + (cents >= 0 ? " +" : " ") + juce::String (cents) + "c   ->   " + noteName (target);
        }
    }
    if (tuneText != shownTuneText)
    {
        shownTuneText = tuneText;
        tuneReadout.setText (tuneText, juce::dontSendNotification);
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
    g.fillAll (juce::Colour (0xff121214)); // obsidian
    g.addTransform (juce::AffineTransform::scale (scaleFactor));
    g.setColour (juce::Colour (0xff2a2a30));
    g.drawLine (270.0f, 90.0f, 270.0f, (float) tuneDividerY - 8.0f);
    g.drawLine ((float) compDividerX, 90.0f, (float) compDividerX, (float) tuneDividerY - 8.0f);
    g.drawLine (16.0f, (float) tuneDividerY, (float) designW - 16.0f, (float) tuneDividerY);
    g.drawLine (16.0f, (float) echoDividerY, (float) designW - 16.0f, (float) echoDividerY);
    g.drawLine (16.0f, (float) dimDividerY, (float) designW - 16.0f, (float) dimDividerY);
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
    auto r = juce::Rectangle<int> (0, 0, designW, designH).reduced (16);

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
    auto tuneArea = r.removeFromBottom (132); // above Liquid Echo, as it comes first in the signal path
    tuneDividerY = tuneArea.getY() - 4;
    tuneTitle.setBounds (tuneArea.removeFromTop (20));
    const int tuneCell = tuneArea.getWidth() / 7;
    auto tuneSwitch = tuneArea.removeFromLeft (tuneCell).reduced (8, 0);
    tuneOnButton.setBounds (tuneSwitch.removeFromTop (40).withSizeKeepingCentre (70, 28));
    auto keyCell = tuneArea.removeFromLeft (tuneCell).reduced (8, 0);
    tuneKeyLabel.setBounds (keyCell.removeFromTop (14));
    tuneKeyBox.setBounds (keyCell.removeFromTop (40).withSizeKeepingCentre (76, 26));
    auto scaleCell = tuneArea.removeFromLeft (tuneCell * 3 / 2).reduced (8, 0);
    tuneScaleLabel.setBounds (scaleCell.removeFromTop (14));
    tuneScaleBox.setBounds (scaleCell.removeFromTop (40).withSizeKeepingCentre (150, 26));
    for (auto* k : tuneKnobs)
        k->setBounds (tuneArea.removeFromLeft (tuneCell).reduced (8, 0));
    tuneReadout.setBounds (tuneArea.reduced (4, 0).withTrimmedBottom (40));
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

    title.setBounds (r.removeFromTop (44));
    tagline.setBounds (r.removeFromTop (20));
    r.removeFromTop (8);

    // ALTER EGOS strip: title, five buttons, a one-line description; below it the Mood Engine: Chaos, Blend, Surprise Me.
    auto egoRow = r.removeFromTop (34);
    egoTitle.setBounds (egoRow.removeFromLeft (92));
    for (auto& b : egoButtons)
    {
        b.setBounds (egoRow.removeFromLeft (122).withSizeKeepingCentre (122, 28));
        egoRow.removeFromLeft (6);
    }
    egoRow.removeFromLeft (10);
    egoInfo.setBounds (egoRow);

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
