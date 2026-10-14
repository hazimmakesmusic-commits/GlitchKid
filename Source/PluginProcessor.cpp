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

    struct BandInfo { const char* name; float fMin, fMax, fDefault, fCentre; };
    const BandInfo info[ParametricEq::numBands] = {
        { "Low Shelf", 20.0f,  500.0f,   100.0f,  100.0f },
        { "Bell 1",    100.0f, 5000.0f,  400.0f,  700.0f },
        { "Bell 2",    500.0f, 16000.0f, 3000.0f, 3000.0f },
        { "High Shelf", 1000.0f, 18000.0f, 8000.0f, 6000.0f } };

    for (int b = 0; b < ParametricEq::numBands; ++b)
    {
        NormalisableRange<float> fr (info[b].fMin, info[b].fMax, 1.0f);
        fr.setSkewForCentre (info[b].fCentre);

        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { eqIds[b].freq, 1 },
            String (info[b].name) + " Freq", fr, info[b].fDefault,
            AudioParameterFloatAttributes().withLabel ("Hz")));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { eqIds[b].gain, 1 },
            String (info[b].name) + " Gain", NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f,
            AudioParameterFloatAttributes().withLabel ("dB")));
        if (eqIds[b].q != nullptr)
        {
            NormalisableRange<float> qr (0.3f, 10.0f, 0.01f);
            qr.setSkewForCentre (1.5f);
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { eqIds[b].q, 1 },
                String (info[b].name) + " Q", qr, 1.0f));
        }
    }

    // Compressor. Ratio defaults to 1:1, so a fresh instance does not change the sound.
    auto skewed = [] (float lo, float hi, float step, float centre)
    {
        NormalisableRange<float> r (lo, hi, step);
        r.setSkewForCentre (centre);
        return r;
    };
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { compThresholdId, 1 }, "Comp Threshold",
        NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -18.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { compRatioId, 1 }, "Comp Ratio",
        skewed (1.0f, 20.0f, 0.1f, 4.0f), 1.0f, AudioParameterFloatAttributes().withLabel (":1")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { compAttackId, 1 }, "Comp Attack",
        skewed (0.1f, 100.0f, 0.1f, 10.0f), 10.0f, AudioParameterFloatAttributes().withLabel ("ms")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { compReleaseId, 1 }, "Comp Release",
        skewed (10.0f, 1000.0f, 1.0f, 100.0f), 100.0f, AudioParameterFloatAttributes().withLabel ("ms")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { compKneeId, 1 }, "Comp Knee",
        NormalisableRange<float> (0.0f, 24.0f, 0.1f), 6.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { compMakeupId, 1 }, "Comp Makeup",
        NormalisableRange<float> (0.0f, 24.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { satDriveId, 1 }, "Saturation Drive",
        NormalisableRange<float> (0.0f, 18.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { mixId, 1 }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, AudioParameterFloatAttributes().withLabel ("%")));

    return layout;
}

GlitchKidProcessor::GlitchKidProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GlitchKidState", createLayout())
{
    setLatencySamples (Saturation::latencySamples);
    gainDb = apvts.getRawParameterValue (gainId);
    bypass = apvts.getRawParameterValue (bypassId);
    for (int b = 0; b < ParametricEq::numBands; ++b)
    {
        eqParams[b].freq = apvts.getRawParameterValue (eqIds[b].freq);
        eqParams[b].gain = apvts.getRawParameterValue (eqIds[b].gain);
        eqParams[b].q    = eqIds[b].q != nullptr ? apvts.getRawParameterValue (eqIds[b].q) : nullptr;
    }
    const char* ids[] = { compThresholdId, compRatioId, compAttackId, compReleaseId, compKneeId, compMakeupId };
    for (int i = 0; i < 6; ++i)
        compParams[i] = apvts.getRawParameterValue (ids[i]);
    satDrive = apvts.getRawParameterValue (satDriveId);
    mixPercent = apvts.getRawParameterValue (mixId);
}

CompressorSettings GlitchKidProcessor::readCompressorSettings() const
{
    return { compParams[0]->load(), compParams[1]->load(), compParams[2]->load(),
             compParams[3]->load(), compParams[4]->load(), compParams[5]->load() };
}

bool GlitchKidProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
           && out == layouts.getMainInputChannelSet();
}

// Target linear gain. The 0.1 dB parameter grid can leave float residue (e.g. 9e-7 dB) at "0 dB" on
// some CPUs; anything within half a step of 0 dB is exactly unity so the default is bit-exact pass-through.
static float targetGain (float db)
{
    return std::abs (db) < 0.05f ? 1.0f : juce::Decibels::decibelsToGain (db);
}

// Share of the processed signal (1 = fully processed, 0 = dry). The 0.1 % parameter grid can leave float
// residue at the ends, so anything within half a step of 0 or 100 % is exactly 0 or 1.
static float wetAmount (bool bypassed, float percent)
{
    if (bypassed || percent < 0.05f)
        return 0.0f;
    return percent >= 99.95f ? 1.0f : percent * 0.01f;
}

void GlitchKidProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    smoothedGain.reset (sampleRate, 0.02); // 20 ms ramps
    smoothedGain.setCurrentAndTargetValue (targetGain (gainDb->load()));
    wetMix.reset (sampleRate, 0.02);
    wetMix.setCurrentAndTargetValue (wetAmount (bypass->load() >= 0.5f, mixPercent->load()));

    for (int b = 0; b < ParametricEq::numBands; ++b) // prepare() starts the smoothers at these values
        eq.setBand (b, { eqParams[b].freq->load(), eqParams[b].gain->load(),
                         eqParams[b].q != nullptr ? eqParams[b].q->load() : 0.7071f });
    eq.prepare (sampleRate);

    compressor.setSettings (readCompressorSettings());
    compressor.prepare (sampleRate);

    saturation.setDriveDb (satDrive->load());
    saturation.prepare (sampleRate, juce::jmax (samplesPerBlock, 512));
    dryDelay.prepare (Saturation::latencySamples);

    dryBuffer.setSize (2, juce::jmax (samplesPerBlock, 512));
}

// Raise a peak value; only the audio thread writes, so a plain load/store is enough.
static void raisePeak (std::atomic<float>& peak, float value)
{
    if (value > peak.load (std::memory_order_relaxed))
        peak.store (value, std::memory_order_relaxed);
}

// Real-time thread: no allocation, locks or I/O here. Splits oversized host blocks so the
// preallocated dry buffer is always big enough.
void GlitchKidProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int cap = dryBuffer.getNumSamples();
    const int total = buffer.getNumSamples();
    for (int start = 0; start < total; start += cap)
    {
        juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), buffer.getNumChannels(),
                                        start, juce::jmin (cap, total - start));
        processChunk (chunk);
    }
}

void GlitchKidProcessor::processChunk (juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Layouts are matched in/out, but never pass unrelated data through extra outputs.
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    const float inMag = buffer.getMagnitude (0, numSamples);
    raisePeak (inputPeak, inMag);

    // Dry copy, delayed by the saturation latency. Kept current on every block so that bypass and the
    // bypass crossfade stay time-aligned with the processed path.
    const int dryChannels = juce::jmin (numChannels, 2);
    for (int ch = 0; ch < dryChannels; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);
    juce::AudioBuffer<float> dry (dryBuffer.getArrayOfWritePointers(), dryChannels, 0, numSamples);
    dryDelay.process (dry);

    const bool bypassed = bypass->load() >= 0.5f;
    const float wetTarget = wetAmount (bypassed, mixPercent->load());
    wetMix.setTargetValue (wetTarget);

    if (wetTarget <= 0.0f && ! wetMix.isSmoothing())
    {
        eq.reset();                 // settled bypass or 0 % mix: exact (delayed) dry signal, no processing cost
        compressor.reset();
        saturation.reset();
        for (int ch = 0; ch < dryChannels; ++ch)
            buffer.copyFrom (ch, 0, dry, ch, 0, numSamples);
        raisePeak (outputPeak, buffer.getMagnitude (0, numSamples));
        return;
    }

    const bool crossfading = wetMix.isSmoothing() || wetTarget < 1.0f; // blending dry and processed

    for (int b = 0; b < ParametricEq::numBands; ++b)
        eq.setBand (b, { eqParams[b].freq->load(), eqParams[b].gain->load(),
                         eqParams[b].q != nullptr ? eqParams[b].q->load() : 0.7071f });
    eq.process (buffer);

    compressor.setSettings (readCompressorSettings());
    compressor.process (buffer);
    raisePeak (gainReductionPeak, compressor.getGainReductionDb());

    saturation.setDriveDb (satDrive->load());
    saturation.process (buffer);

    smoothedGain.setTargetValue (targetGain (gainDb->load()));
    if (! smoothedGain.isSmoothing())
    {
        buffer.applyGain (smoothedGain.getTargetValue());
    }
    else
    {
        // Same ramp on every channel: step the smoother once per sample, not once per channel.
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = smoothedGain.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.getWritePointer (ch)[i] *= g;
        }
    }

    if (crossfading)
        for (int i = 0; i < numSamples; ++i)
        {
            const float m = wetMix.getNextValue();
            for (int ch = 0; ch < dryChannels; ++ch)
            {
                float* out = buffer.getWritePointer (ch);
                out[i] = dry.getSample (ch, i) + (out[i] - dry.getSample (ch, i)) * m;
            }
        }

    raisePeak (outputPeak, buffer.getMagnitude (0, numSamples));
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
