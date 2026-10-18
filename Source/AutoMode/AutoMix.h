#pragma once
#include "../Analysis/VocalAnalyzer.h"
#include "../Presets/SoundState.h"
#include <functional>

namespace glitchkid::automode
{
// The rules engine of AutoMode: a PURE function (no hidden state, no randomness, no I/O). It turns one analysis, a style profile and an
// intensity into a new sound built from the existing controls only. It starts from the current sound and changes only the parameters
// listed in AutoMix.cpp (never Gain, Mix, Bypass, key, scale, FORMANT, Mutation, Harmony, Stutter or the Alter Ego), so the user's other
// work is kept. If the analysis is not usable it changes nothing. It makes no claim about the key, the scale or the "ideal" mix.
enum class Profile { astroMode };

struct Range
{
    float lo = 0.0f, hi = 0.0f;
    std::function<float (float)> snap; // optional: rounds a value to the parameter's own step (so applying twice gives exactly the same sound)
};
using RangeLookup = std::function<bool (const juce::String& id, Range& out)>; // false = unknown id (the value is dropped)

struct Result
{
    bool usable = false;          // false: the analysis was inconclusive (or intensity is 0), nothing changed
    bool usedFallback = false;    // true: the whole proposal comes from ASTRO MODE's typical-vocal defaults, not from this recording (only when allowFallback was set)
    SoundState sound;             // the sound to apply: `current` with the AutoMode parameters replaced (every value inside its range)
    int changedCount = 0;         // how many parameters differ from `current`
    juce::StringArray notes;      // plain-English reasons, e.g. "Left the low shelf flat: the low end already looks strong"
    juce::String message;         // one line for the status display
};

const char* profileName (Profile);
// `allowFallback`: when the analysis is not usable (too short, too quiet, invalid, still listening), false (the default) changes nothing;
// true proposes the curated ASTRO MODE defaults for a typical vocal instead (fixed gentle compressor, no loudness-based decisions) and says so in the notes.
// Individual metrics that are missing or non-finite in an otherwise usable analysis are replaced by neutral typical values either way, and noted.
Result compute (const analysis::VocalAnalysis&, Profile, float intensity, const SoundState& current, const RangeLookup&, bool allowFallback = false);
}
