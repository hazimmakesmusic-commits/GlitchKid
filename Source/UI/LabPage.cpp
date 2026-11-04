#include "LabPage.h"
#include "PlaygroundWidgets.h"

namespace glitchkid::ui
{
namespace
{
const char* const cardTitle[4] = { "FORMANT  #2", "HARMONY  #3", "MUTATION  #4", "STUTTER  #12" };
const char* const cardNote[4] = {
    "The size of the voice, not its pitch. Down: bigger. Up: smaller. 0 is off. Adds about 21 ms latency while not 0.",
    "A second voice at a fixed interval from yours. Adds Pitch Tune's latency while on.",
    "Bends the voice into something new. MORE: the kind (down, robot, up) and how much you hear.",
    "Tempo-locked slices of the finished voice." };
const std::vector<const char*> cardParams[4] = {
    { "formant_shift" },
    { "harm_on", "harm_interval", "harm_level" },
    { "mut_on", "mut_amount", "mut_character", "mut_blend" },
    { "stutter_on", "stutter_rate", "stutter_mix" } };
}

LabPage::LabPage (juce::AudioProcessorValueTreeState& s) : state (s)
{
    bind (formant, "formant_shift", "SHIFT", "Formant shift in semitones (formant_shift). 0 = off.");
    bind (harmOn, "harm_on", "ON", "Harmony on/off (harm_on).");
    bind (harmInterval, "harm_interval", "INTERVAL", "The harmony's interval from your voice (harm_interval).");
    bind (harmLevel, "harm_level", "LEVEL", "Harmony level (harm_level).");
    bind (mutOn, "mut_on", "ON", "Mutation on/off (mut_on).");
    bind (mutAmount, "mut_amount", "AMOUNT", "Mutation amount (mut_amount).");
    bind (mutCharacter, "mut_character", "CHARACTER", "Mutation character: down, robot, up (mut_character).");
    bind (mutBlend, "mut_blend", "BLEND", "How much mutated voice you hear (mut_blend).");
    bind (stutterOn, "stutter_on", "ON", "Stutter on/off (stutter_on).");
    bind (stutterRate, "stutter_rate", "SLICE", "Slice length (stutter_rate).");
    bind (stutterMix, "stutter_mix", "MIX", "Sliced loop (100) vs live voice (0) (stutter_mix).");
    for (int i = 0; i < 4; ++i)
    {
        notes[i].setText (cardNote[i], juce::dontSendNotification);
        notes[i].setFont (fontOf (12.0f));
        notes[i].setColour (juce::Label::textColourId, textDim);
        notes[i].setJustificationType (juce::Justification::topLeft);
        notes[i].getProperties().set ("rawText", true);
        addAndMakeVisible (notes[i]);
    }
    for (int i = 0; i < 4; ++i)
    {
        auto& b = resets[(size_t) i];
        b.setButtonText ("Reset");
        b.setTooltip ("Put this card's controls back to their defaults (the effect turns off). Other cards are not touched.");
        b.onClick = [this, i]
        {
            for (auto* id : cardParams[i])
                if (auto* prm = state.getParameter (id))
                {
                    prm->beginChangeGesture();
                    prm->setValueNotifyingHost (prm->getDefaultValue());
                    prm->endChangeGesture();
                }
        };
        addAndMakeVisible (b);
    }
    more.setClickingTogglesState (true);
    more.setTooltip ("Mutation's finer controls: Character (down, robot, up) and Blend (how much of it you hear).");
    more.onClick = [this] { showMore = more.getToggleState(); resized(); };
    addAndMakeVisible (more);
}

bool LabPage::isOn (int card) const
{
    auto v = [this] (const char* id) { auto* p = state.getRawParameterValue (id); return p != nullptr ? p->load() : 0.0f; };
    switch (card)
    {
        case 0: return std::abs (v ("formant_shift")) > 0.05f;
        case 1: return v ("harm_on") >= 0.5f;
        case 2: return v ("mut_on") >= 0.5f;
        default: return v ("stutter_on") >= 0.5f;
    }
}

void LabPage::timerCallback()
{
    for (int i = 0; i < 4; ++i)
        if (isOn (i) != shownOn[(size_t) i])
        {
            shownOn[(size_t) i] = isOn (i);
            repaint (cards[(size_t) i].expanded (2));
        }
}

void LabPage::bind (Knob& k, const char* id, const juce::String& name, const juce::String& tip)
{
    k.label.setText (name, juce::dontSendNotification);
    k.label.setFont (fontOf (11.5f, true));
    k.label.setColour (juce::Label::textColourId, textDim);
    k.label.setJustificationType (juce::Justification::centred);
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, pink);
    k.slider.setTooltip (tip);
    k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, id, k.slider);
    addAndMakeVisible (k.label);
    addAndMakeVisible (k.slider);
}

void LabPage::bind (Switch& s, const char* id, const juce::String& name, const juce::String& tip)
{
    s.button.setButtonText (name);
    s.button.setTooltip (tip);
    s.att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, id, s.button);
    addAndMakeVisible (s.button);
}

void LabPage::bind (Choice& c, const char* id, const juce::String& name, const juce::String& tip)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (id)))
        c.box.addItemList (p->choices, 1);
    c.box.setTooltip (tip);
    c.label.setText (name, juce::dontSendNotification);
    c.label.setFont (fontOf (11.5f, true));
    c.label.setColour (juce::Label::textColourId, textDim);
    c.att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, id, c.box);
    addAndMakeVisible (c.label);
    addAndMakeVisible (c.box);
}

void LabPage::paint (juce::Graphics& g)
{
    for (int i = 0; i < 4; ++i)
    {
        const auto r = cards[(size_t) i].toFloat();
        const bool on = isOn (i);
        g.setColour (on ? juce::Colour (0xff1a1019) : panelBottom); // quiet until the effect is really on
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (on ? pink.withAlpha (0.55f) : border);
        g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);
        g.setColour (textBright);
        g.setFont (fontOf (15.0f, true));
        g.drawText (cardTitle[i], r.reduced (14.0f, 10.0f).removeFromTop (20.0f), juce::Justification::centredLeft, false);
        g.setFont (fontOf (11.5f, true));
        g.setColour (on ? pink : textDim);
        g.drawText (on ? "ON" : "OFF", r.reduced (14.0f, 10.0f).removeFromTop (20.0f), juce::Justification::centredRight, false);
    }
}

void LabPage::resized()
{
    auto area = getLocalBounds().reduced (12, 8);
    const int gap = 12, w = (area.getWidth() - gap) / 2, h = (area.getHeight() - gap) / 2;
    cards = { juce::Rectangle<int> (area.getX(), area.getY(), w, h), { area.getX() + w + gap, area.getY(), w, h },
              { area.getX(), area.getY() + h + gap, w, h }, { area.getX() + w + gap, area.getY() + h + gap, w, h } };
    auto knob = [] (Knob& k, juce::Rectangle<int> r) { k.label.setBounds (r.removeFromTop (16)); k.slider.setBounds (r); };
    auto inner = [] (juce::Rectangle<int> c) { return c.reduced (14, 10).withTrimmedTop (26); };
    for (int i = 0; i < 4; ++i)
    {
        notes[i].setBounds (inner (cards[(size_t) i]).removeFromBottom (48));
        resets[(size_t) i].setBounds (cards[(size_t) i].reduced (14, 10).removeFromTop (20).withTrimmedRight (44).removeFromRight (56)); // left of the painted ON / OFF
    }

    auto f = inner (cards[0]).withTrimmedBottom (52);
    knob (formant, f.withSizeKeepingCentre (110, f.getHeight()));

    auto hcard = inner (cards[1]).withTrimmedBottom (52);
    harmOn.button.setBounds (hcard.removeFromTop (26).removeFromLeft (80));
    auto hrow = hcard.removeFromTop (24);
    harmInterval.label.setBounds (hrow.removeFromLeft (80));
    harmInterval.box.setBounds (hrow.removeFromLeft (200));
    knob (harmLevel, hcard.withTrimmedTop (4).withSizeKeepingCentre (100, juce::jmax (60, hcard.getHeight() - 4)));

    auto m = inner (cards[2]).withTrimmedBottom (52);
    auto mtop = m.removeFromTop (26);
    mutOn.button.setBounds (mtop.removeFromLeft (80));
    more.setBounds (mtop.removeFromRight (72));
    mutCharacter.slider.setVisible (showMore);
    mutCharacter.label.setVisible (showMore);
    mutBlend.slider.setVisible (showMore);
    mutBlend.label.setVisible (showMore);
    if (showMore)
    {
        const int kw = m.getWidth() / 3;
        knob (mutAmount, m.removeFromLeft (kw));
        knob (mutCharacter, m.removeFromLeft (kw));
        knob (mutBlend, m);
    }
    else
        knob (mutAmount, m.withSizeKeepingCentre (110, m.getHeight()));

    auto s = inner (cards[3]).withTrimmedBottom (52);
    stutterOn.button.setBounds (s.removeFromTop (26).removeFromLeft (80));
    auto srow = s.removeFromTop (24);
    stutterRate.label.setBounds (srow.removeFromLeft (80));
    stutterRate.box.setBounds (srow.removeFromLeft (120));
    knob (stutterMix, s.withTrimmedTop (4).withSizeKeepingCentre (100, juce::jmax (60, s.getHeight() - 4)));
}
}
