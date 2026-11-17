#include "BeatLockParams.h"

namespace glitchkid::beatlock
{
juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using P = juce::AudioParameterFloat;
    using B = juce::AudioParameterBool;
    using Range = juce::NormalisableRange<float>;
    using Attr = juce::AudioParameterFloatAttributes;
    const auto pct = Attr().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; });
    const auto dB = Attr().withLabel ("dB").withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; });
    const auto ms = Attr().withLabel ("ms").withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " ms"; });
    const auto hz = Attr().withLabel ("Hz").withStringFromValueFunction ([] (float v, int) { return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; });
    const auto qa = Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2); });

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto add = [&layout] (auto p) { layout.add (std::move (p)); };
    add (std::make_unique<P> (juce::ParameterID { ids::amount, 1 }, "Amount", Range (0.0f, 100.0f, 0.1f), defaults::amount, pct));
    add (std::make_unique<P> (juce::ParameterID { ids::maxCut, 1 }, "Max Cut", Range (-12.0f, 0.0f, 0.1f), defaults::maxCut, dB));
    add (std::make_unique<P> (juce::ParameterID { ids::sensitivity, 1 }, "Sensitivity", Range (-12.0f, 12.0f, 0.1f), defaults::sensitivity, dB));
    add (std::make_unique<P> (juce::ParameterID { ids::focusLow, 1 }, "Low Focus", Range (0.0f, 100.0f, 0.1f), defaults::focusLow, pct));
    add (std::make_unique<P> (juce::ParameterID { ids::focusMid, 1 }, "Mid Focus", Range (0.0f, 100.0f, 0.1f), defaults::focusMid, pct));
    add (std::make_unique<P> (juce::ParameterID { ids::focusHigh, 1 }, "High Focus", Range (0.0f, 100.0f, 0.1f), defaults::focusHigh, pct));
    Range attackRange (1.0f, 100.0f, 0.1f);
    attackRange.setSkewForCentre (15.0f);
    add (std::make_unique<P> (juce::ParameterID { ids::attack, 1 }, "Attack", attackRange, defaults::attack, ms));
    Range releaseRange (30.0f, 1000.0f, 1.0f);
    releaseRange.setSkewForCentre (180.0f);
    add (std::make_unique<P> (juce::ParameterID { ids::release, 1 }, "Release", releaseRange, defaults::release, ms));
    add (std::make_unique<B> (juce::ParameterID { ids::bypass, 1 }, "Bypass", false));

    for (int b = 0; b < numBands; ++b)
    {
        const auto& id = bandIds()[(size_t) b];
        const juce::String n = "Band " + juce::String (b + 1) + " ";
        add (std::make_unique<B> (juce::ParameterID { id.on, 1 }, n + "On", true));
        Range freqRange (100.0f, 12000.0f, 0.1f);
        freqRange.setSkewForCentre (1100.0f); // about the geometric middle of 100 Hz - 12 kHz
        add (std::make_unique<P> (juce::ParameterID { id.freq, 1 }, n + "Frequency", freqRange, defaultFreqs[(size_t) b], hz));
        Range qRange (0.5f, 4.0f, 0.01f);
        qRange.setSkewForCentre (1.4f);
        add (std::make_unique<P> (juce::ParameterID { id.q, 1 }, n + "Q", qRange, defaults::bandQ, qa));
        add (std::make_unique<P> (juce::ParameterID { id.maxCut, 1 }, n + "Max Cut", Range (-12.0f, 0.0f, 0.1f), defaults::bandMaxCut, dB));
    }
    return layout;
}
}
