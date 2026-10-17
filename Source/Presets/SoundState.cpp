#include "SoundState.h"
#include <cmath>

namespace glitchkid
{
namespace
{
constexpr int maxEntries = 1000; // a sanity limit for files from outside
}

bool SoundState::has (const juce::String& id) const
{
    for (const auto& e : values)
        if (e.id == id)
            return true;
    return false;
}

float SoundState::get (const juce::String& id, float fallback) const
{
    for (const auto& e : values)
        if (e.id == id)
            return e.value;
    return fallback;
}

void SoundState::set (const juce::String& id, float value)
{
    for (auto& e : values)
        if (e.id == id)
        {
            e.value = value;
            return;
        }
    values.push_back ({ id, value });
}

void SoundState::writeTo (juce::XmlElement& parent) const
{
    for (const auto& e : values)
    {
        auto* p = parent.createNewChildElement ("Param");
        p->setAttribute ("id", e.id);
        p->setAttribute ("value", juce::String (e.value, 6));
    }
    auto* ego = parent.createNewChildElement ("AlterEgo");
    ego->setAttribute ("id", egoId);
    ego->setAttribute ("chaos", juce::String (egoChaos, 6));
    ego->setAttribute ("seed", egoSeed);
}

bool SoundState::readFrom (const juce::XmlElement& parent)
{
    values.clear();
    egoId = {};
    egoChaos = 0.0f;
    egoSeed = 0;

    bool any = false;
    for (auto* child : parent.getChildIterator())
    {
        if (child->hasTagName ("Param") && (int) values.size() < maxEntries)
        {
            const auto id = child->getStringAttribute ("id");
            const auto text = child->getStringAttribute ("value");
            if (id.isEmpty() || text.isEmpty() || has (id))
                continue;
            const double v = text.getDoubleValue();
            if (! std::isfinite (v) || std::abs (v) > 1.0e9)
                continue;
            values.push_back ({ id, (float) v });
            any = true;
        }
        else if (child->hasTagName ("AlterEgo"))
        {
            egoId = child->getStringAttribute ("id");
            const double c = child->getDoubleAttribute ("chaos", 0.0);
            egoChaos = std::isfinite (c) ? (float) juce::jlimit (0.0, 1.0, c) : 0.0f;
            egoSeed = child->getIntAttribute ("seed", 0);
        }
    }
    return any;
}
}
