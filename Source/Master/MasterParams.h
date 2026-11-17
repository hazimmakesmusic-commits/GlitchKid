#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// GLITCHKID MASTER: its own parameters (gkm_*), independent of the GlitchKid vocal plug-in's master_* parameters. The IDs, their order and their ranges are
// part of saved sessions and host automation: never rename or reorder; only append.
namespace glitchkid::masterplugin
{
constexpr int stateVersion = 1;

namespace ids
{
inline constexpr const char* style = "gkm_style";         // Clean & Balanced, Warm & Smooth, Loud & Punchy, Wide & Atmospheric
inline constexpr const char* intensity = "gkm_intensity"; // 0..100 %: how much of the style's EQ, glue, colour and width is used
inline constexpr const char* input = "gkm_input";         // -12..+12 dB: level into the chain
inline constexpr const char* loudness = "gkm_loudness";   // 0..10 dB: drive into the true-peak limiter
inline constexpr const char* tone = "gkm_tone";           // -100 (darker) .. +100 (brighter)
inline constexpr const char* width = "gkm_width";         // 0..200 %: multiplies the style's width (100 = as the style sets it)
inline constexpr const char* ceiling = "gkm_ceiling";     // -6..-0.3 dBTP: the limiter's true-peak ceiling
inline constexpr const char* output = "gkm_output";       // -12..0 dB: after the limiter, never a boost
inline constexpr const char* bypass = "gkm_bypass";       // the host's bypass: the untouched mix, delayed by the plug-in's latency
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
