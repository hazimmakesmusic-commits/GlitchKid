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
    processor.applyPreset (name, FactoryPresets::build (index, processor.factoryDefaultState()));
    return { true, false, name, "Loaded \"" + name + "\"." };
}

bool PresetController::isFactoryName (const juce::String& name)
{
    const auto n = name.trim();
    return n.equalsIgnoreCase (factoryDefaultName) || FactoryPresets::indexOfName (n) >= 0;
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
