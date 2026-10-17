#pragma once
#include <juce_core/juce_core.h>
#include <vector>

namespace glitchkid
{
// A complete "sound": the value of every sound parameter (in the parameter's own units: dB, Hz, %, ms, choice index, 0/1) plus the
// Alter Ego selection. It is plain data, captured from and applied to the plug-in's AudioProcessorValueTreeState (which stays the only
// source of truth for the live sound). Used for user presets (written to disk) and for the A/B slots (kept in the plug-in state).
//
// Bypass is not part of a sound (it is a monitoring switch): it is never captured, stored or changed by presets or A/B.
struct SoundState
{
    struct Entry { juce::String id; float value = 0.0f; };

    std::vector<Entry> values;
    juce::String egoId; // Alter Ego id ("" = none); see AlterEgos.h
    float egoChaos = 0.0f;
    int egoSeed = 0;

    static bool isSoundParameter (const juce::String& id) { return id != "bypass"; }

    bool has (const juce::String& id) const;
    float get (const juce::String& id, float fallback) const; // the stored value, or `fallback` if the id is not in the state
    void set (const juce::String& id, float value);           // replaces an existing entry or appends one

    // XML form: <tag><Param id=".." value=".."/> ... <AlterEgo id=".." chaos=".." seed=".."/></tag>. Reading never throws: it keeps every
    // well-formed Param (finite value, non-empty id, first occurrence wins) and ignores everything else (unknown elements are future fields).
    void writeTo (juce::XmlElement& parent) const;
    bool readFrom (const juce::XmlElement& parent); // false if nothing usable was found
};
}
