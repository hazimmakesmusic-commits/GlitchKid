#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

// GLITCHKID BEAT LOCK: parameter IDs, ranges and defaults. Independent of the GlitchKid vocal plug-in (its own `bl_` IDs, its own state).
// IDs are permanent once released: never rename or reorder them (hosts store automation by ID).
namespace glitchkid::beatlock
{
constexpr int numBands = 8;
constexpr int stateVersion = 1;

namespace ids
{
inline constexpr const char* amount = "bl_amount";             // 0..100 %: how much of the computed cut is applied
inline constexpr const char* maxCut = "bl_maxcut";             // -12..0 dB: the deepest cut any band may make
inline constexpr const char* sensitivity = "bl_sensitivity";   // -12..+12 dB: higher = cuts start sooner (less conflict needed)
inline constexpr const char* focusLow = "bl_focus_low";        // 0..100 %: weight below ~500 Hz
inline constexpr const char* focusMid = "bl_focus_mid";        // 0..100 %: weight ~500 Hz - 2.5 kHz
inline constexpr const char* focusHigh = "bl_focus_high";      // 0..100 %: weight above ~2.5 kHz
inline constexpr const char* attack = "bl_attack";             // 1..100 ms: a cut deepening
inline constexpr const char* release = "bl_release";           // 30..1000 ms: a cut recovering
inline constexpr const char* bypass = "bl_bypass";             // the host's bypass (exact pass-through after a 20 ms fade)
}

struct BandIds
{
    juce::String on, freq, q, maxCut;
};

inline const std::array<BandIds, numBands>& bandIds()
{
    static const std::array<BandIds, numBands> table = [] {
        std::array<BandIds, numBands> t;
        for (int b = 0; b < numBands; ++b)
        {
            const juce::String p = "bl_b" + juce::String (b + 1) + "_";
            t[(size_t) b] = { p + "on", p + "freq", p + "q", p + "maxcut" };
        }
        return t;
    }();
    return table;
}

inline constexpr std::array<float, numBands> defaultFreqs { 180.0f, 300.0f, 500.0f, 850.0f, 1400.0f, 2400.0f, 4000.0f, 7000.0f };

namespace defaults
{
inline constexpr float amount = 50.0f, maxCut = -4.0f, sensitivity = 0.0f, focusLow = 50.0f, focusMid = 100.0f, focusHigh = 75.0f;
inline constexpr float attack = 15.0f, release = 180.0f, bandQ = 1.4f, bandMaxCut = -12.0f;
}

// The layout, in a fixed order (globals, then band 1..8: on, freq, q, max cut). All automatable.
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
