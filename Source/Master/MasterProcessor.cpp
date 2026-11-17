#include "MasterProcessor.h"
#include "MasterEditor.h"

using namespace glitchkid::masterplugin;

MasterProcessor::MasterProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GlitchKidMaster", createLayout())
{
    auto p = [this] (const char* id) { auto* v = apvts.getRawParameterValue (id); jassert (v != nullptr); return v; };
    raw = { p (ids::style), p (ids::intensity), p (ids::input), p (ids::loudness), p (ids::tone), p (ids::width), p (ids::ceiling), p (ids::output), p (ids::bypass) };
    bypassParam = apvts.getParameter (ids::bypass);
    apvts.state.setProperty ("gkm_state_version", stateVersion, nullptr);
}

glitchkid::master::Controls MasterProcessor::readControls() const
{
    glitchkid::master::Controls c;
    c.style = (glitchkid::master::Style) juce::jlimit (0, glitchkid::master::numStyles - 1, (int) std::lround (raw.style->load()));
    c.intensity = raw.intensity->load() * 0.01f;
    c.loudnessDb = raw.loudness->load();
    c.tone = raw.tone->load() * 0.01f;
    c.width = raw.width->load() * 0.01f;
    c.ceilingDbTp = raw.ceiling->load();
    c.outputDb = raw.output->load();
    return c;
}

void MasterProcessor::prepareToPlay (double sampleRate, int)
{
    engine.setControls (readControls());
    engine.setCompareOriginal (compare.load (std::memory_order_relaxed));
    engine.prepare (sampleRate, chunk);
    bypassDelay.prepare (engine.latencySamples());
    for (auto* v : { &inputGain, &bypassMix })
        v->reset (sampleRate, 0.02);
    inputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (raw.input->load()));
    bypassMix.setCurrentAndTargetValue (isBypassed() ? 1.0f : 0.0f);
    setLatencySamples (engine.latencySamples());
}

bool MasterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && in == out;
}

void MasterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int chs = juce::jmin (2, getMainBusNumInputChannels(), getMainBusNumOutputChannels(), buffer.getNumChannels());
    for (int c = chs; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);
    blocks.fetch_add (1, std::memory_order_relaxed);
    if (chs <= 0 || n <= 0)
        return;
    channels.store (chs, std::memory_order_relaxed);

    if (meterReset.exchange (false, std::memory_order_relaxed))
        engine.resetMeters();
    engine.setControls (readControls());
    engine.setCompareOriginal (compare.load (std::memory_order_relaxed));
    inputGain.setTargetValue (juce::Decibels::decibelsToGain (raw.input->load()));
    bypassMix.setTargetValue (isBypassed() ? 1.0f : 0.0f);

    bool sawNonFinite = false;
    for (int pos = 0; pos < n; pos += chunk)
    {
        const int len = juce::jmin (chunk, n - pos);
        juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), chs, pos, len);
        for (int c = 0; c < chs; ++c)
        {
            float* x = view.getWritePointer (c);
            for (int i = 0; i < len; ++i)
                if (! std::isfinite (x[i]))
                {
                    x[i] = 0.0f; // a broken sample upstream must never reach the filters (they would stay broken)
                    sawNonFinite = true;
                }
            dry.copyFrom (c, 0, view, c, 0, len); // the untouched input, for bypass
        }
        juce::AudioBuffer<float> dryView (dry.getArrayOfWritePointers(), chs, len);
        bypassDelay.process (dryView);

        for (int i = 0; i < len; ++i)
        {
            const float g = inputGain.getNextValue();
            for (int c = 0; c < chs; ++c)
                view.getWritePointer (c)[i] *= g;
        }
        for (int c = 0; c < 2; ++c) // the INPUT meters show what enters the chain (after the INPUT trim), like the engine's loudness meter
            raise (c == 0 ? pkInL : pkInR, view.getMagnitude (juce::jmin (c, chs - 1), 0, len));
        engine.process (view);
        raise (pkLim, engine.limiterReductionDb());
        raise (pkGlue, engine.compressorReductionDb());

        if (bypassMix.isSmoothing() || bypassMix.getTargetValue() > 0.0f)
            for (int i = 0; i < len; ++i)
            {
                const float b = bypassMix.getNextValue();
                for (int c = 0; c < chs; ++c)
                {
                    float* y = view.getWritePointer (c);
                    y[i] = y[i] * (1.0f - b) + dryView.getSample (c, i) * b;
                }
            }
        for (int c = 0; c < 2; ++c)
            raise (c == 0 ? pkOutL : pkOutR, view.getMagnitude (juce::jmin (c, chs - 1), 0, len));
    }
    if (sawNonFinite)
        nonFinite.fetch_add (1, std::memory_order_relaxed);
}

MasterProcessor::Peaks MasterProcessor::consumePeaks()
{
    auto take = [] (std::atomic<float>& a) { return a.exchange (0.0f, std::memory_order_relaxed); };
    return { take (pkInL), take (pkInR), take (pkOutL), take (pkOutR), take (pkLim), take (pkGlue) };
}

void MasterProcessor::storeSlot (int slot)
{
    juce::ValueTree t ("AB");
    for (auto* p : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p); r != nullptr && p != bypassParam)
            t.setProperty (juce::Identifier (r->paramID), r->convertFrom0to1 (r->getValue()), nullptr);
    slots[(size_t) (slot & 1)] = t;
    active = slot & 1;
}

bool MasterProcessor::recallSlot (int slot)
{
    const auto& t = slots[(size_t) (slot & 1)];
    if (! t.isValid())
        return false;
    for (auto* p : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p); r != nullptr && p != bypassParam && t.hasProperty (juce::Identifier (r->paramID)))
        {
            const float v = (float) t.getProperty (juce::Identifier (r->paramID));
            if (! std::isfinite (v))
                continue;
            r->beginChangeGesture();
            r->setValueNotifyingHost (r->convertTo0to1 (v));
            r->endChangeGesture();
        }
    active = slot & 1;
    return true;
}

void MasterProcessor::resetToDefaults()
{
    for (auto* p : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            r->beginChangeGesture();
            r->setValueNotifyingHost (r->getDefaultValue());
            r->endChangeGesture();
        }
}

void MasterProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("gkm_state_version", stateVersion, nullptr);
    for (int s = 0; s < 2; ++s)
        if (slots[(size_t) s].isValid())
        {
            auto copy = slots[(size_t) s].createCopy();
            copy.setProperty ("slot", s, nullptr);
            state.appendChild (copy, nullptr);
        }
    state.setProperty ("gkm_ab_active", active, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void MasterProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Safe on anything: unreadable or foreign data changes nothing; a parameter missing from an older state takes its default; unknown values are ignored.
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;
    const auto loaded = juce::ValueTree::fromXml (*xml);
    for (auto* p : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            float value = r->getDefaultValue();
            for (const auto& child : loaded)
                if (child.hasType ("PARAM") && child.getProperty ("id").toString() == r->paramID)
                {
                    const auto v = child.getProperty ("value");
                    if (! v.isVoid() && std::isfinite ((float) v))
                        value = r->convertTo0to1 ((float) v);
                }
            r->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, value));
        }
    slots = {};
    for (const auto& child : loaded)
        if (child.hasType ("AB"))
            slots[(size_t) ((int) child.getProperty ("slot", 0) & 1)] = child.createCopy();
    active = juce::jlimit (-1, 1, (int) loaded.getProperty ("gkm_ab_active", -1));
    if (active >= 0 && ! slots[(size_t) active].isValid())
        active = -1;
    apvts.state.setProperty ("gkm_state_version", stateVersion, nullptr);
}

juce::AudioProcessorEditor* MasterProcessor::createEditor()
{
    return new MasterEditor (*this);
}

// The plug-in entry point for the GlitchKidMaster target (the test executable defines no entry point; it creates MasterProcessor directly).
#if ! defined (GLITCHKID_NO_PLUGIN_ENTRY)
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MasterProcessor();
}
#endif
