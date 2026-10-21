#include "PresetController.h"

namespace glitchkid
{
PresetController::Outcome PresetController::loadFactoryDefault()
{
    processor.applyPreset (factoryDefaultName, processor.factoryDefaultState());
    return { true, false, factoryDefaultName, "Factory default sound restored." };
}

PresetController::Outcome PresetController::loadFactory (int index)
{
    if (index < 0 || index >= FactoryPresets::count)
        return { false, false, {}, "That factory preset does not exist." };
    const juce::String name = FactoryPresets::info (index).name;
    juce::String problem;
    if (! validateFactory (index, &problem))
        return { false, false, name, "\"" + name + "\" could not be loaded (" + problem + "). Nothing was changed." };
    auto state = FactoryPresets::build (index, processor.factoryDefaultState());
    const auto live = processor.captureSoundState(); // tuning (on, style, key, scale) and your Gain / Mix are yours, not the profile's
    for (const char* id : { "tune_on", "tune_strength", "tune_speed", "tune_key", "tune_scale", "gain", "mix", "air_on", "air_presence", "air_air", "air_link", "air_mix", "air_trim", "clean_on", "clean_amount", "clean_tone", "clean_res", "clean_ess" })
        state.set (id, live.get (id, state.get (id, 0.0f)));
    processor.applyPreset (name, state);
    return { true, false, name, "Loaded \"" + name + "\"." };
}

bool PresetController::validateFactory (int index, juce::String* problem) const
{
    auto fail = [problem] (const juce::String& why) { if (problem != nullptr) *problem = why; return false; };
    if (index < 0 || index >= FactoryPresets::count)
        return fail ("no such preset");
    juce::StringArray seen;
    const auto* o = FactoryPresets::overrides (index);
    for (int i = 0; i < FactoryPresets::numOverrides (index); ++i)
    {
        const juce::String id (o[i].id);
        for (const char* reserved : { "tune_on", "tune_strength", "tune_speed", "tune_key", "tune_scale", "gain", "mix", "bypass", "air_on", "air_presence", "air_air", "air_link", "air_mix", "air_trim", "clean_on", "clean_amount", "clean_tone", "clean_res", "clean_ess" })
            if (id == reserved)
                return fail (id + " is reserved");
        if (seen.contains (id))
            return fail (id + " is set twice");
        seen.add (id);
        auto* r = dynamic_cast<juce::RangedAudioParameter*> (processor.apvts.getParameter (id));
        if (r == nullptr)
            return fail ("unknown control " + id);
        const auto range = r->getNormalisableRange();
        if (! std::isfinite (o[i].value) || o[i].value < range.start - 1.0e-4f || o[i].value > range.end + 1.0e-4f)
            return fail (id + " is out of range");
    }
    return true;
}

bool PresetController::isFactoryName (const juce::String& name)
{
    const auto n = name.trim();
    return n.equalsIgnoreCase (factoryDefaultName) || FactoryPresets::indexOfName (n) >= 0;
}

juce::StringArray PresetController::allPresetNames() const
{
    juce::StringArray names { factoryDefaultName };
    for (int i = 0; i < FactoryPresets::count; ++i)
        names.add (FactoryPresets::info (i).name);
    names.addArray (store.listUserPresets());
    return names;
}

PresetController::Outcome PresetController::loadByName (const juce::String& name)
{
    if (name.equalsIgnoreCase (factoryDefaultName))
        return loadFactoryDefault();
    const int f = FactoryPresets::indexOfName (name);
    return f >= 0 ? loadFactory (f) : loadUser (name);
}

PresetController::Outcome PresetController::step (int direction)
{
    const auto names = allPresetNames();
    const auto current = processor.getPresetName();
    int index = -1;
    for (int i = 0; i < names.size(); ++i)
        if (names[i].equalsIgnoreCase (current))
            index = i;
    const int n = names.size();
    const int dir = direction > 0 ? 1 : -1;
    Outcome last { false, false, {}, "No presets found." };
    for (int tries = 0; tries < n; ++tries) // a damaged user preset is skipped, so the step buttons never get stuck on it
    {
        index = index < 0 ? (dir > 0 ? 0 : n - 1) : ((index + dir) % n + n) % n;
        last = loadByName (names[index]);
        if (last.ok)
            return last;
    }
    return last;
}

juce::String PresetController::variationName (const juce::String& current)
{
    return current.isEmpty() || current.equalsIgnoreCase (factoryDefaultName) ? juce::String() : (isFactoryName (current) ? current + " My Version" : current);
}

PresetController::Outcome PresetController::loadUser (const juce::String& name)
{
    SoundState s;
    const auto r = store.load (name, s);
    if (! r.ok)
        return { false, false, name, r.message }; // nothing was changed
    processor.applyPreset (name, s);
    return { true, false, name, r.message.isNotEmpty() ? r.message : "Loaded \"" + name + "\"." };
}

PresetController::Outcome PresetController::saveAs (const juce::String& name, bool overwrite)
{
    const auto clean = PresetStore::sanitizeName (name);
    if (clean.isEmpty())
        return { false, false, {}, "Please type a name with letters or numbers." };
    if (isFactoryName (clean))
        return { false, false, clean, "\"" + clean + "\" is a built-in preset name. Please choose another name." };
    if (store.exists (clean) && ! overwrite)
        return { false, true, clean, "A preset called \"" + clean + "\" already exists." };

    const auto state = processor.captureSoundState();
    const auto r = store.save (clean, state, overwrite);
    if (! r.ok)
        return { false, false, clean, r.message };

    juce::String stored = clean;
    for (const auto& n : store.listUserPresets())
        if (n.equalsIgnoreCase (clean))
            stored = n;
    processor.notePresetSaved (stored, state);
    return { true, false, stored, (overwrite ? "Replaced \"" : "Saved \"") + stored + "\"." };
}

PresetController::Outcome PresetController::deleteUser (const juce::String& name)
{
    const auto r = store.remove (name);
    if (! r.ok)
        return { false, false, name, r.message };
    if (processor.getPresetName().equalsIgnoreCase (name))
        processor.clearPresetName();
    return { true, false, name, "Deleted \"" + name + "\"." };
}
}
