#pragma once
#include "PresetStore.h"
#include "FactoryPresets.h"
#include "../PluginProcessor.h"

namespace glitchkid
{
// The glue between the preset files (PresetStore) and the live plug-in (GlitchKidProcessor): what the Save, Load, Delete and Factory
// Default actions of the window do. No UI here, so it can be tested. Message thread only.
class PresetController
{
public:
    struct Outcome
    {
        bool ok = false;
        bool needsOverwriteConfirmation = false; // a preset of that name exists: ask the user, then call saveAs again with overwrite = true
        juce::String name;                       // the final name (cleaned, with the stored spelling)
        juce::String message;                    // plain English for the status line
    };

    explicit PresetController (GlitchKidProcessor& p, PresetStore s = PresetStore (PresetStore::defaultUserDirectory()))
        : processor (p), store (std::move (s)) {}

    static constexpr const char* factoryDefaultName = "Factory Default";

    const PresetStore& getStore() const { return store; }
    Outcome loadFactoryDefault();
    Outcome loadFactory (int index);                            // a built-in preset (see FactoryPresets.h). Validated first; keeps YOUR tune key and scale
    bool validateFactory (int index, juce::String* problem = nullptr) const; // every id exists, every value finite and in range, nothing reserved is touched
    static bool isFactoryName (const juce::String& name);       // "Factory Default" or a built-in preset name: reserved, cannot be saved over
    Outcome loadUser (const juce::String& name);
    Outcome saveAs (const juce::String& name, bool overwrite); // saves the live sound
    Outcome deleteUser (const juce::String& name);
    enum class Kind { none, factory, user };
    Kind kindOf (const juce::String& name) const { return name.isEmpty() ? Kind::none : isFactoryName (name) ? Kind::factory : store.exists (name) ? Kind::user : Kind::none; }
    juce::StringArray allPresetNames() const;                  // Factory Default, the built-in presets, then user presets (A-Z): the order of the menu and of the step buttons
    Outcome loadByName (const juce::String& name);             // any name from allPresetNames()
    Outcome step (int direction);                              // +1 next / -1 previous, wrapping; from "no preset" next = first, previous = last
    static juce::String variationName (const juce::String& current); // suggested name for saving your own version of `current`
    bool isUserPreset (const juce::String& name) const { return name.isNotEmpty() && store.exists (name); }

private:
    GlitchKidProcessor& processor;
    PresetStore store;
};
}
