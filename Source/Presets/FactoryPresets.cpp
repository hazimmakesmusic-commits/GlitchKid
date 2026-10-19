#include "FactoryPresets.h"

namespace glitchkid::FactoryPresets
{
namespace
{
struct Entry { Info info; const Override* overrides; size_t numOverrides; };

// Liquid Echo division choice indexes: 0 = 1/2, 1 = 1/4, 2 = 1/4 dotted, ..., 10 = 1/32. Tune key and scale are never part of a preset: the
// preset controller keeps the user's own (see PresetController::loadFactory).
#include "ArtistPresets.inc"
}

const char* tagName (int bitIndex)
{
    static const char* names[numTags] = { "Melodic Rap", "Hardcore / Lyrical", "Trap", "Drill", "Rage / Distorted", "R&B / Pop-Rap", "Experimental", "Punjabi", "Hindi / Urdu" };
    return names[juce::jlimit (0, numTags - 1, bitIndex)];
}

juce::String tagList (unsigned tags)
{
    juce::StringArray out;
    for (int i = 0; i < numTags; ++i)
        if (tags & (1u << i))
            out.add (tagName (i));
    return out.joinIntoString (", ");
}

const Info& info (int index) { return entries[juce::jlimit (0, count - 1, index)].info; }
int numOverrides (int index) { return (int) entries[juce::jlimit (0, count - 1, index)].numOverrides; }
const Override* overrides (int index) { return entries[juce::jlimit (0, count - 1, index)].overrides; }

int indexOfName (const juce::String& name)
{
    for (int i = 0; i < count; ++i)
        if (name.equalsIgnoreCase (entries[i].info.name))
            return i;
    return -1;
}

SoundState build (int index, const SoundState& factoryDefault)
{
    SoundState s = factoryDefault;
    const auto& e = entries[juce::jlimit (0, count - 1, index)];
    for (size_t i = 0; i < e.numOverrides; ++i)
        s.set (e.overrides[i].id, e.overrides[i].value);
    s.egoId = {};
    return s;
}
}
