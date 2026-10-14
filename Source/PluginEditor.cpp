#include "PluginEditor.h"

namespace
{
const juce::Colour lime (0xffb6ff00), pink (0xffff2d95), violet (0xff8a2be2);
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
      dimOnAttachment (p.apvts, GlitchKidProcessor::dimOnId, dimOnButton)
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
    stutterMixKnob[0]->addTo (*this);
    for (auto* c : std::initializer_list<juce::Component*> { &stutterTitle, &stutterRateLabel, &tempoLabel,
                                                              &stutterOnButton, &stutterRateBox })
        addAndMakeVisible (c);

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
        k->addTo (*this);
    for (auto* c : std::initializer_list<juce::Component*> { &echoTitle, &echoDivLabel, &echoOnButton, &echoSyncButton, &echoDivBox })
        addAndMakeVisible (c);

    // Dimension: stereo thickening. WIDTH 0 = centred thickening, 100 = stereo spread; DETUNE = pitch swing in cents.
    dimTitle.setJustificationType (juce::Justification::centred);
    dimTitle.setColour (juce::Label::textColourId, violet);
    dimOnButton.setColour (juce::ToggleButton::tickColourId, lime);
    dimKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::dimWidthId, "WIDTH", " %", lime));
    dimKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::dimDetuneId, "DETUNE", " cents", pink));
    dimKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::dimDepthId, "DEPTH", " %", violet));
    dimKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::dimMixId, "DIMENSION MIX", " %", pink));
    for (auto* k : dimKnobs)
        k->addTo (*this);
    addAndMakeVisible (dimTitle);
    addAndMakeVisible (dimOnButton);

    gainKnob.add (new ParamKnob (p.apvts, GlitchKidProcessor::gainId, "OUTPUT GAIN", " dB", pink));
    gainKnob[0]->addTo (*this);
    driveKnob.add (new ParamKnob (p.apvts, GlitchKidProcessor::satDriveId, "SATURATION DRIVE", " dB", lime));
    driveKnob[0]->addTo (*this);
    mixKnob.add (new ParamKnob (p.apvts, GlitchKidProcessor::mixId, "MIX (DRY / PROCESSED)", " %", violet));
    mixKnob[0]->addTo (*this);

    const char* names[] = { "LOW SHELF", "BELL 1", "BELL 2", "HIGH SHELF" };
    for (int b = 0; b < ParametricEq::numBands; ++b)
    {
        bandNames[b].setText (names[b], juce::dontSendNotification);
        bandNames[b].setJustificationType (juce::Justification::centred);
        bandNames[b].setColour (juce::Label::textColourId, violet);
        addAndMakeVisible (bandNames[b]);

        const auto& ids = GlitchKidProcessor::eqIds[b];
        eqKnobs[b].add (new ParamKnob (p.apvts, ids.freq, "FREQ", " Hz", lime));
        eqKnobs[b].add (new ParamKnob (p.apvts, ids.gain, "GAIN", " dB", pink));
        if (ids.q != nullptr)
            eqKnobs[b].add (new ParamKnob (p.apvts, ids.q, "Q", "", violet));
        for (auto* k : eqKnobs[b])
            k->addTo (*this);
    }

    // Compressor: threshold, ratio, makeup on the first row; attack, release, knee on the second.
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compThresholdId, "THRESHOLD", " dB", lime));
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compRatioId, "RATIO", ":1", pink));
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compMakeupId, "MAKEUP", " dB", violet));
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compAttackId, "ATTACK", " ms", lime));
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compReleaseId, "RELEASE", " ms", pink));
    compKnobs.add (new ParamKnob (p.apvts, GlitchKidProcessor::compKneeId, "KNEE", " dB", violet));
    for (auto* k : compKnobs)
        k->addTo (*this);

    compTitle.setJustificationType (juce::Justification::centred);
    compTitle.setColour (juce::Label::textColourId, violet);
    grLabel.setJustificationType (juce::Justification::centred);
    grLabel.setFont (juce::FontOptions (11.0f));

    for (auto* c : std::initializer_list<juce::Component*> { &title, &tagline, &bypassButton, &inMeter, &outMeter,
                                                              &inLabel, &outLabel, &grMeter, &compTitle, &grLabel })
        addAndMakeVisible (c);

    setSize (1060, 780);
    timerCallback(); // fill the tempo line immediately
    startTimerHz (30);
}

void GlitchKidEditor::timerCallback()
{
    inMeter.pushPeak (glitchProcessor.consumeInputPeak());
    outMeter.pushPeak (glitchProcessor.consumeOutputPeak());
    grMeter.pushReduction (glitchProcessor.consumeGainReductionDb());

    const bool synced = glitchProcessor.apvts.getRawParameterValue (GlitchKidProcessor::echoSyncId)->load() >= 0.5f;
    echoKnobs[0]->setEnabled (! synced);
    echoDivBox.setEnabled (synced);
    echoDivLabel.setAlpha (synced ? 1.0f : 0.4f);

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
    g.setColour (juce::Colour (0xff2a2a30));
    g.drawLine (270.0f, 90.0f, 270.0f, (float) echoDividerY - 8.0f);
    g.drawLine ((float) compDividerX, 90.0f, (float) compDividerX, (float) echoDividerY - 8.0f);
    g.drawLine (16.0f, (float) echoDividerY, (float) getWidth() - 16.0f, (float) echoDividerY);
    g.drawLine (16.0f, (float) dimDividerY, (float) getWidth() - 16.0f, (float) dimDividerY);
}

void GlitchKidEditor::resized()
{
    auto r = getLocalBounds().reduced (16);

    // Bottom strips: Dimension (lowest), then Liquid Echo above it.
    auto dimArea = r.removeFromBottom (140);
    dimDividerY = dimArea.getY() - 4;
    dimTitle.setBounds (dimArea.removeFromTop (20));
    const int dimCell = dimArea.getWidth() / 7;
    dimOnButton.setBounds (dimArea.removeFromLeft (dimCell).reduced (8, 0).removeFromTop (40).withSizeKeepingCentre (70, 28));
    dimArea.removeFromLeft (dimCell); // keeps the knobs aligned with the Liquid Echo row above
    for (auto* k : dimKnobs)
        k->setBounds (dimArea.removeFromLeft (dimCell).reduced (8, 0));

    auto echoArea = r.removeFromBottom (140);
    echoDividerY = echoArea.getY() - 4;
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
    r.removeFromTop (12);

    // Left: meters either side of the output-gain knob, Bypass underneath.
    auto left = r.removeFromLeft (240);
    r.removeFromLeft (20);
    bypassButton.setBounds (left.removeFromBottom (32));
    auto inCol = left.removeFromLeft (28), outCol = left.removeFromRight (28);
    inLabel.setBounds (inCol.removeFromBottom (16));
    outLabel.setBounds (outCol.removeFromBottom (16));
    inMeter.setBounds (inCol.reduced (6, 0));
    outMeter.setBounds (outCol.reduced (6, 0));
    auto knobs = left.withSizeKeepingCentre (left.getWidth(), 336);
    driveKnob[0]->setBounds (knobs.removeFromTop (112).withTrimmedBottom (6));
    gainKnob[0]->setBounds (knobs.removeFromTop (112).withTrimmedBottom (6));
    mixKnob[0]->setBounds (knobs.withTrimmedBottom (6));

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
