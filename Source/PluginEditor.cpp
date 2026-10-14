#include "PluginEditor.h"

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
    g.setColour (shownDb > 0.0f ? juce::Colour (0xffff3030) : juce::Colour (0xffb6ff00));
    g.fillRoundedRectangle (r.withTop (r.getBottom() - h), 3.0f);
}

GlitchKidEditor::GlitchKidEditor (GlitchKidProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      gainAttachment (p.apvts, GlitchKidProcessor::gainId, gainSlider),
      bypassAttachment (p.apvts, GlitchKidProcessor::bypassId, bypassButton)
{
    title.setFont (juce::FontOptions (32.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, juce::Colour (0xffb6ff00)); // acid lime
    title.setJustificationType (juce::Justification::centred);

    tagline.setFont (juce::FontOptions (13.0f));
    tagline.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    tagline.setJustificationType (juce::Justification::centred);

    gainLabel.setJustificationType (juce::Justification::centred);
    gainSlider.setTextValueSuffix (" dB");
    gainSlider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffff2d95)); // hot pink
    bypassButton.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff8a2be2));      // violet

    for (auto* l : { &inLabel, &outLabel })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setFont (juce::FontOptions (11.0f));
    }

    for (auto* c : std::initializer_list<juce::Component*> { &title, &tagline, &gainSlider, &gainLabel, &bypassButton,
                                                              &inMeter, &outMeter, &inLabel, &outLabel })
        addAndMakeVisible (c);

    setSize (360, 340);
    startTimerHz (30);
}

void GlitchKidEditor::timerCallback()
{
    inMeter.pushPeak (processor.consumeInputPeak());
    outMeter.pushPeak (processor.consumeOutputPeak());
}

void GlitchKidEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff121214)); // obsidian
}

void GlitchKidEditor::resized()
{
    auto r = getLocalBounds().reduced (16);
    title.setBounds (r.removeFromTop (44));
    tagline.setBounds (r.removeFromTop (20));
    r.removeFromTop (12);
    gainLabel.setBounds (r.removeFromTop (20));
    bypassButton.setBounds (r.removeFromBottom (32));

    auto inCol = r.removeFromLeft (28), outCol = r.removeFromRight (28);
    inLabel.setBounds (inCol.removeFromBottom (16));
    outLabel.setBounds (outCol.removeFromBottom (16));
    inMeter.setBounds (inCol.reduced (6, 0));
    outMeter.setBounds (outCol.reduced (6, 0));
    gainSlider.setBounds (r.reduced (8));
}
