#pragma once
#include "SoundState.h"

namespace glitchkid
{
// Code-defined factory presets: lists of parameter overrides applied on top of the Factory Default sound, so every sound parameter is
// always set and a preset sounds the same whatever the plug-in was doing before. They use only existing parameters and effects.
// These are original production-style starting points inspired by a genre/mood, not official presets or recreations of anyone's voice.
// The display names are in one place (the table in FactoryPresets.cpp) so they are trivial to change; saved sound never stores them.
namespace FactoryPresets
{
    constexpr int count = 3;

    struct Info
    {
        const char* name;        // shown in the preset menu and the preset bar
        const char* description; // plain description of the intended sound
    };

    const Info& info (int index);                                       // index 0..count-1
    int indexOfName (const juce::String& name);                         // case-insensitive; -1 if not a factory preset
    SoundState build (int index, const SoundState& factoryDefault);     // defaults + this preset's overrides
}
}
