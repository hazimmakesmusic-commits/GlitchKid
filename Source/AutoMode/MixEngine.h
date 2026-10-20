#pragma once
#include "AutoMix.h"
#include "SourceRelationship.h"
#include "../Presets/FactoryPresets.h"

namespace glitchkid::automode
{
// BUILD MY VOCAL MIX: a PURE function (no hidden state, no randomness, no I/O) that turns the selected artist-inspired profile, the lead vocal's analysis and,
// when it was really measured, the instrumental's analysis into a complete sound built only from existing controls.
//
// Foundation: the profile's own saved values (or the clean Factory Default when no profile is selected). The measurements then make small, documented,
// bounded adjustments to it. Because the foundation never depends on the current sound (only the key, scale, Gain and Mix are carried over), building twice
// gives the same result: repeated builds cannot push anything further and further.
// Never touched: Bypass, Mix, Gain, Tune key and scale (yours), and anything the profile does not set. No key, BPM, genre or "quality" is ever claimed.
enum class MixMode { vocalOnly, beatInformed };
enum class InstrumentalInput { notConnected, connectedSilent, live }; // what the plug-in's instrumental input is doing (see GlitchKidProcessor::SourceStatus)

struct MixInput
{
    int profile = -1;                       // FactoryPresets index (0..count-1), or -1 for the clean default
    float intensity = 0.6f;                 // 0..1: how much of the profile's effect amounts is used (0.5 + 0.5 * intensity of each, 1 = the profile as saved)
    SoundState current;                     // the live sound (its key, scale, Gain and Mix are kept)
    SoundState factoryDefault;
    analysis::VocalAnalysis lead;
    analysis::VocalAnalysis beat;           // only used when status is ready
    InstrumentalInput instrumental = InstrumentalInput::notConnected;
};

struct MixChange
{
    juce::String id;     // parameter id
    float from = 0.0f, to = 0.0f;
    juce::String reason; // plain English
};

struct MixProposal
{
    bool usable = false;               // false: nothing was proposed (see message); sound == current
    MixMode mode = MixMode::vocalOnly;
    juce::String profileName;          // e.g. "TRAVIS SCOTT STYLE", or "CLEAN DEFAULT"
    SoundState sound;                  // the whole sound to apply (every value inside its range)
    std::vector<MixChange> changes;    // only the parameters that differ from the current sound
    juce::StringArray evidence;        // the measurements and findings that were used
    juce::StringArray unavailable;     // what was NOT available or NOT known (stated, not guessed)
    juce::String message;              // one line for the status display
};

MixProposal buildMix (const MixInput&, const RangeLookup&);
}
