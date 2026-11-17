#include "BeatLockProcessor.h"
#include "BeatLockEditor.h"

using namespace glitchkid::beatlock;

BeatLockProcessor::BeatLockProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Vocal Sidechain", juce::AudioChannelSet::stereo(), true)), // listened to only; never reaches the output
      apvts (*this, nullptr, "GlitchKidBeatLock", createLayout())
{
    auto p = [this] (const juce::String& id) { auto* v = apvts.getRawParameterValue (id); jassert (v != nullptr); return v; };
    raw.amount = p (ids::amount);
    raw.maxCut = p (ids::maxCut);
    raw.sensitivity = p (ids::sensitivity);
    raw.focus[0] = p (ids::focusLow);
    raw.focus[1] = p (ids::focusMid);
    raw.focus[2] = p (ids::focusHigh);
    raw.attack = p (ids::attack);
    raw.release = p (ids::release);
    raw.bypass = p (ids::bypass);
    for (int b = 0; b < numBands; ++b)
    {
        const auto& id = bandIds()[(size_t) b];
        raw.on[b] = p (id.on);
        raw.freq[b] = p (id.freq);
        raw.q[b] = p (id.q);
        raw.maxCutBand[b] = p (id.maxCut);
    }
    bypassParam = apvts.getParameter (ids::bypass);
    apvts.state.setProperty ("bl_state_version", stateVersion, nullptr);
}

Settings BeatLockProcessor::readSettings() const
{
    Settings s;
    s.amount = raw.amount->load() * 0.01f;
    s.maxCutDb = raw.maxCut->load();
    s.sensitivityDb = raw.sensitivity->load();
    s.focusLow = raw.focus[0]->load() * 0.01f;
    s.focusMid = raw.focus[1]->load() * 0.01f;
    s.focusHigh = raw.focus[2]->load() * 0.01f;
    s.attackMs = raw.attack->load();
    s.releaseMs = raw.release->load();
    s.bypass = raw.bypass->load() >= 0.5f;
    for (int b = 0; b < numBands; ++b)
        s.bands[(size_t) b] = { raw.on[b]->load() >= 0.5f, raw.freq[b]->load(), raw.q[b]->load(), raw.maxCutBand[b]->load() };
    return s;
}

void BeatLockProcessor::prepareToPlay (double sampleRate, int)
{
    rate.store (sampleRate, std::memory_order_relaxed);
    engine.setSettings (readSettings());
    engine.prepare (sampleRate);
    silentSeconds = 0.0;
    setLatencySamples (0);
}

bool BeatLockProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    const bool mainOk = (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && in == out;
    if (layouts.inputBuses.size() < 2)
        return mainOk;
    const auto& sc = layouts.inputBuses[1];
    return mainOk && (sc.isDisabled() || sc == juce::AudioChannelSet::mono() || sc == juce::AudioChannelSet::stereo());
}

void BeatLockProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    // The instrumental: the main bus (input channels 0.., written in place = the output). The vocal: the sidechain bus, whose input channels follow the main
    // INPUT channels in the host buffer. Only read. Channel counts are clamped to what the host buffer really holds.
    const int mainIn = getMainBusNumInputChannels(), mainOut = juce::jmin (getMainBusNumOutputChannels(), buffer.getNumChannels());
    const int mainCh = juce::jmin (mainIn, mainOut);
    int scCh = getBusCount (true) > 1 && getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled() ? getChannelCountOfBus (true, 1) : 0;
    if (buffer.getNumChannels() < mainIn + scCh)
        scCh = 0;
    for (int c = mainCh; c < mainOut; ++c) // an output channel with no matching input (not expected with in == out) is silence, never sidechain audio
        buffer.clear (c, 0, n);
    if (mainCh <= 0 || n <= 0)
        return;

    engine.setSettings (readSettings());
    float* mainPtr[Engine::maxChannels] = { buffer.getWritePointer (0), mainCh > 1 ? buffer.getWritePointer (1) : nullptr };
    const float* scPtr[Engine::maxChannels] = { scCh > 0 ? buffer.getReadPointer (mainIn) : nullptr, scCh > 1 ? buffer.getReadPointer (mainIn + 1) : nullptr };
    float* vocalMono = scratch.getWritePointer (0);
    float* gateFlag = scratch.getWritePointer (1);
    float* instMono = scratch.getWritePointer (2);

    float peak = 0.0f;
    for (int pos = 0; pos < n; pos += chunk)
    {
        const int len = juce::jmin (chunk, n - pos);
        float* m[Engine::maxChannels] = { mainPtr[0] + pos, mainCh > 1 ? mainPtr[1] + pos : nullptr };
        const float* s[Engine::maxChannels] = { scPtr[0] != nullptr ? scPtr[0] + pos : nullptr, scPtr[1] != nullptr ? scPtr[1] + pos : nullptr };
        instInTap.push (m[0], m[1], len); // before processing: the real instrumental input (non-finite samples are stored as 0)
        for (int i = 0; i < len; ++i)
        {
            const float l = m[0][i], r = mainCh > 1 ? m[1][i] : l;
            instMono[i] = std::isfinite (l) && std::isfinite (r) ? 0.5f * (l + r) : 0.0f;
        }
        engine.process (m, mainCh, s, scCh, len, vocalMono, gateFlag);
        instOutTap.push (m[0], m[1], len);
        if (scCh > 0)
        {
            vocalTap.push (vocalMono, nullptr, len);
            for (int i = 0; i < len; ++i)
                peak = juce::jmax (peak, std::abs (vocalMono[i]));
        }
        fifo.write (vocalMono, instMono, gateFlag, len); // only while LEARN / ANALYZE is listening
    }

    // sidechain status: not connected / connected but silent (for more than half a second) / live
    if (scCh == 0)
    {
        status.store ((int) SidechainStatus::notConnected, std::memory_order_relaxed);
        silentSeconds = 0.0;
    }
    else
    {
        silentSeconds = peak > 1.0e-4f ? 0.0 : silentSeconds + n / juce::jmax (1.0, rate.load (std::memory_order_relaxed));
        status.store ((int) (silentSeconds > 0.5 ? SidechainStatus::silent : SidechainStatus::live), std::memory_order_relaxed);
    }
    float prev = scPeak.load (std::memory_order_relaxed);
    while (peak > prev && ! scPeak.compare_exchange_weak (prev, peak, std::memory_order_relaxed)) {}
}

void BeatLockProcessor::resetToDefaults()
{
    for (auto* p : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            r->beginChangeGesture();
            r->setValueNotifyingHost (r->getDefaultValue());
            r->endChangeGesture();
        }
}

void BeatLockProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("bl_state_version", stateVersion, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void BeatLockProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Safe on anything: unreadable or foreign data changes nothing; a parameter missing from an older state takes its default; unknown extra values are ignored.
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
    apvts.state.setProperty ("bl_state_version", stateVersion, nullptr);
}

juce::AudioProcessorEditor* BeatLockProcessor::createEditor()
{
    return new BeatLockEditor (*this);
}

// The plug-in entry point for the GlitchKidBeatLock target (the test executable defines no entry point; it creates BeatLockProcessor directly).
#if ! defined (GLITCHKID_NO_PLUGIN_ENTRY)
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BeatLockProcessor();
}
#endif
