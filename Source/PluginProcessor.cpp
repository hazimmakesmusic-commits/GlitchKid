#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout GlitchKidProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { gainId, 1 }, "Gain",
        NormalisableRange<float> (-60.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { bypassId, 1 }, "Bypass", false));

    return layout;
}

GlitchKidProcessor::GlitchKidProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GlitchKidState", createLayout())
{
    gainDb = apvts.getRawParameterValue (gainId);
    bypass = apvts.getRawParameterValue (bypassId);
}

bool GlitchKidProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
           && out == layouts.getMainInputChannelSet();
}

void GlitchKidProcessor::prepareToPlay (double sampleRate, int)
{
    smoothedGain.reset (sampleRate, 0.02); // 20 ms ramp
    smoothedGain.setCurrentAndTargetValue (bypass->load() >= 0.5f ? 1.0f
                                           : juce::Decibels::decibelsToGain (gainDb->load()));
}

// Real-time thread: no allocation, locks or I/O here.
void GlitchKidProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    smoothedGain.setTargetValue (bypass->load() >= 0.5f ? 1.0f
                                 : juce::Decibels::decibelsToGain (gainDb->load()));

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (! smoothedGain.isSmoothing())
    {
        buffer.applyGain (smoothedGain.getTargetValue());
        return;
    }

    // Same ramp on every channel: step the smoother once per sample, not once per channel.
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = smoothedGain.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer (ch)[i] *= g;
    }
}

juce::AudioProcessorEditor* GlitchKidProcessor::createEditor()
{
    return new GlitchKidEditor (*this);
}

void GlitchKidProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void GlitchKidProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Ignore anything that is not our own state (corrupt or foreign data).
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GlitchKidProcessor();
}
