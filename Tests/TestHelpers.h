#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"

// Shared helpers for the unit tests.
namespace th
{
using Buf = juce::AudioBuffer<float>;

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

// Run `blocks` blocks of `blockSize`; returns the last processed block and its input.
inline void run (GlitchKidProcessor& p, int channels, int blockSize, int blocks, Buf& lastIn, Buf& lastOut)
{
    juce::MidiBuffer midi;
    for (int b = 0; b < blocks; ++b)
    {
        lastIn = makeNoise (channels, blockSize, b + 1);
        lastOut.makeCopyOf (lastIn);
        p.processBlock (lastOut, midi);
    }
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
