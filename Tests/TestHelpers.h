#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include "../Source/DSP/Saturation.h"

// Shared helpers for the unit tests.
namespace th
{
using Buf = juce::AudioBuffer<float>;

// Constant delay the plugin adds (the saturation oversampler); every path, including bypass, has it.
constexpr int pluginLatency = Saturation::latencySamples;

// Deterministic test signal in [-0.5, 0.5].
inline Buf makeNoise (int channels, int samples, juce::int64 seed = 1)
{
    juce::Random rng (seed);
    Buf b (channels, samples);
    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < samples; ++i)
            b.setSample (ch, i, rng.nextFloat() - 0.5f);
    return b;
}

inline void setParam (juce::AudioProcessor& p, const char* id, float plainValue)
{
    auto* prm = dynamic_cast<juce::RangedAudioParameter*> (p.getParameters()[0]);
    for (auto* q : p.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (q); r && r->paramID == id)
            prm = r;
    prm->setValueNotifyingHost (prm->convertTo0to1 (plainValue));
}

inline float getParam (juce::AudioProcessor& p, const char* id)
{
    for (auto* q : p.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (q); r && r->paramID == id)
            return r->convertFrom0to1 (r->getValue());
    return -999.0f;
}

inline void setLayout (juce::AudioProcessor& p, const juce::AudioChannelSet& set)
{
    juce::AudioProcessor::BusesLayout l;
    l.inputBuses.add (set);
    l.outputBuses.add (set);
    p.setBusesLayout (l);
}

// Run `blocks` blocks of `blockSize`; returns the last processed block, and the matching input:
// `lastIn` is the input delayed by the plugin latency, i.e. what the output should be compared with.
inline void run (GlitchKidProcessor& p, int channels, int blockSize, int blocks, Buf& lastIn, Buf& lastOut)
{
    juce::MidiBuffer midi;
    Buf all (channels, blockSize * blocks);
    for (int b = 0; b < blocks; ++b)
    {
        Buf in = makeNoise (channels, blockSize, b + 1);
        for (int ch = 0; ch < channels; ++ch)
            all.copyFrom (ch, b * blockSize, in, ch, 0, blockSize);
        lastOut.makeCopyOf (in);
        p.processBlock (lastOut, midi);
    }
    lastIn.setSize (channels, blockSize);
    lastIn.clear();
    const int start = (blocks - 1) * blockSize - pluginLatency;
    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < blockSize; ++i)
            if (start + i >= 0)
                lastIn.setSample (ch, i, all.getSample (ch, start + i));
}

// True if `out` is exactly `in` delayed by `delay` samples, when the same block `in` was fed repeatedly
// (so the delayed samples at the start of `out` come from the end of `in`).
inline bool rotatedExactly (const Buf& in, const Buf& out, int delay)
{
    const int n = in.getNumSamples();
    for (int ch = 0; ch < in.getNumChannels(); ++ch)
        for (int i = 0; i < n; ++i)
            if (! juce::exactlyEqual (out.getSample (ch, i), in.getSample (ch, (i - delay + n) % n)))
                return false;
    return true;
}

inline float maxAbsDiff (const Buf& a, const Buf& b, float gain = 1.0f)
{
    float m = 0.0f;
    for (int ch = 0; ch < a.getNumChannels(); ++ch)
        for (int i = 0; i < a.getNumSamples(); ++i)
            m = juce::jmax (m, std::abs (a.getSample (ch, i) * gain - b.getSample (ch, i)));
    return m;
}
}
using namespace th;
