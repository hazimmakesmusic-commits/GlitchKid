#pragma once
#include "SoundState.h"
#include <memory>

namespace glitchkid
{
// User presets: one small, readable, versioned XML file per preset in a per-user folder. Message thread only (it does file I/O):
// nothing here may ever be called from the audio callback.
//
//   <GlitchKidPreset formatVersion="1" name="My Sound" plugin="GlitchKid">
//     <Param id="gain" value="-2.000000"/> ...  <AlterEgo id="celestial" chaos="0.500000" seed="0"/>
//   </GlitchKidPreset>
//
// Rules: a name is reduced to safe characters (no path separators, no leading dots, no reserved Windows names, at most 60
// characters), so it can never point outside the preset folder; names are compared case-insensitively (macOS and Windows file systems are);
// saving never silently overwrites; files are written to a temporary file first and swapped in, so a failed write cannot destroy a
// good preset; unreadable, corrupt or foreign files are reported, never crash. Factory presets are not files (see the processor).
class PresetStore
{
public:
    static constexpr int formatVersion = 1;
    static constexpr int maxNameLength = 60;
    static constexpr const char* fileExtension = ".gkpreset";
    static constexpr juce::int64 maxFileBytes = 1024 * 1024;

    struct Result
    {
        bool ok = false;
        juce::String message; // plain English, suitable for showing to the user
    };

    explicit PresetStore (juce::File userDirectory) : directory (std::move (userDirectory)) {}

    // macOS: ~/Library/Application Support/GlitchKid/Presets/User   (Windows: %APPDATA%\GlitchKid\Presets\User, Linux: ~/.config/GlitchKid/Presets/User)
    static juce::File defaultUserDirectory();

    // "" if nothing usable is left after cleaning.
    static juce::String sanitizeName (const juce::String& wanted);

    const juce::File& getDirectory() const { return directory; }
    juce::StringArray listUserPresets() const;                        // names without extension, sorted; empty if the folder is missing
    bool exists (const juce::String& name) const;                     // case-insensitive
    Result save (const juce::String& name, const SoundState&, bool overwrite) const;
    Result load (const juce::String& name, SoundState&) const;
    Result remove (const juce::String& name) const;

    // The XML conversion, public so tests (and future import/export) can use it without touching the disk.
    static std::unique_ptr<juce::XmlElement> toXml (const juce::String& name, const SoundState&);
    static Result fromXml (const juce::XmlElement&, SoundState&, juce::String* nameOut = nullptr);

private:
    juce::File fileFor (const juce::String& cleanName) const; // an empty File if the name is unusable or would leave the folder
    juce::String existingNameFor (const juce::String& cleanName) const; // the stored spelling of a case-insensitive match, or ""

    juce::File directory;
};
}
