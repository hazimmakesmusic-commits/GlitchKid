#pragma once
#include "PresetStore.h"
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
    Outcome loadUser (const juce::String& name);
    Outcome saveAs (const juce::String& name, bool overwrite); // saves the live sound
    Outcome deleteUser (const juce::String& name);
    bool isUserPreset (const juce::String& name) const { return name.isNotEmpty() && store.exists (name); }

private:
    GlitchKidProcessor& processor;
    PresetStore store;
};
}
