#include "MasterParams.h"
#include "../Mastering/MasteringEngine.h"

namespace glitchkid::masterplugin
{
juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    auto db = [] (int decimals) { return AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([decimals] (float v, int) { return String (v, decimals) + " dB"; }); };
    auto pct = AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; });
    StringArray styles;
    for (int s = 0; s < master::numStyles; ++s)
        styles.add (master::styleName ((master::Style) s));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::style, 1 }, "Style", styles, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::intensity, 1 }, "Intensity", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 60.0f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::input, 1 }, "Input", NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f, db (1)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::loudness, 1 }, "Loudness", NormalisableRange<float> (0.0f, 10.0f, 0.01f), 3.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int) { return "+" + String (v, 1) + " dB drive"; })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::tone, 1 }, "Tone", NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                       {
                                                           const int a = roundToInt (std::abs (v));
                                                           return a < 2 ? String ("Neutral") : (v > 0.0f ? "Brighter " : "Darker ") + String (a);
                                                       })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::width, 1 }, "Width", NormalisableRange<float> (0.0f, 200.0f, 0.1f), 100.0f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::ceiling, 1 }, "Ceiling", NormalisableRange<float> (-6.0f, -0.3f, 0.01f), -1.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dBTP").withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dBTP"; })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::output, 1 }, "Output", NormalisableRange<float> (-12.0f, 0.0f, 0.01f), 0.0f, db (1)));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::bypass, 1 }, "Bypass", false));
    return layout;
}
}
