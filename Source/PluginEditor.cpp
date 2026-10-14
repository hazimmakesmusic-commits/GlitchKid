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

void ParamKnob::setBounds (juce::Rectangle<int> r)
{
    label.setBounds (r.removeFromTop (14));
    slider.setBounds (r);
}

GlitchKidEditor::GlitchKidEditor (GlitchKidProcessor& p)
    : AudioProcessorEditor (&p),
      glitchProcessor (p),
      bypassAttachment (p.apvts, GlitchKidProcessor::bypassId, bypassButton)
{
    title.setFont (juce::FontOptions (32.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, lime);
    title.setJustificationType (juce::Justification::centred);

    tagline.setFont (juce::FontOptions (13.0f));
    tagline.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    tagline.setJustificationType (juce::Justification::centred);

    for (auto* l : { &inLabel, &outLabel, &gainLabel })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setFont (juce::FontOptions (11.0f));
    }

    bypassButton.setColour (juce::ToggleButton::tickColourId, violet);

    gainKnob.add (new ParamKnob (p.apvts, GlitchKidProcessor::gainId, "GAIN", " dB", pink));
    gainKnob[0]->addTo (*this);

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

    for (auto* c : std::initializer_list<juce::Component*> { &title, &tagline, &bypassButton, &inMeter, &outMeter,
                                                              &inLabel, &outLabel, &gainLabel })
        addAndMakeVisible (c);

    setSize (720, 440);
    startTimerHz (30);
}

void GlitchKidEditor::timerCallback()
{
    inMeter.pushPeak (glitchProcessor.consumeInputPeak());
    outMeter.pushPeak (glitchProcessor.consumeOutputPeak());
}

void GlitchKidEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff121214)); // obsidian
    g.setColour (juce::Colour (0xff2a2a30));
    g.drawLine (270.0f, 90.0f, 270.0f, (float) getHeight() - 20.0f);
}

void GlitchKidEditor::resized()
{
    auto r = getLocalBounds().reduced (16);
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
    gainLabel.setBounds (left.removeFromTop (16));
    gainKnob[0]->setBounds (left.withSizeKeepingCentre (left.getWidth(), 110));

    // Right: four EQ columns, each with its own FREQ / GAIN (/ Q) knobs.
    const int colW = r.getWidth() / ParametricEq::numBands;
    for (int b = 0; b < ParametricEq::numBands; ++b)
    {
        auto col = r.removeFromLeft (colW);
        bandNames[b].setBounds (col.removeFromTop (20));
        for (auto* k : eqKnobs[b])
            k->setBounds (col.removeFromTop (100).reduced (8, 0));
    }
}
