#include "PresetStore.h"

namespace glitchkid
{
namespace
{
bool isReservedWindowsName (const juce::String& name)
{
    static const char* reserved[] = { "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
                                      "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9" };
    const auto upper = name.toUpperCase();
    for (auto* r : reserved)
        if (upper == r)
            return true;
    return false;
}
}

juce::File PresetStore::defaultUserDirectory()
{
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
#if JUCE_MAC
    base = base.getChildFile ("Application Support"); // JUCE returns ~/Library here
#endif
    return base.getChildFile ("GlitchKid").getChildFile ("Presets").getChildFile ("User");
}

juce::String PresetStore::sanitizeName (const juce::String& wanted)
{
    juce::String out;
    for (auto c : wanted.trim())
    {
        // Letters and digits of any script are kept (tested by code point, not by the C library's locale-dependent isalpha), but not control,
        // line-separator, zero-width or direction-mark characters, which could make a name invisible or confusing.
        const bool printableNonAscii = c >= 0xA0 && c != 0x2028 && c != 0x2029 && ! (c >= 0x200B && c <= 0x200F) && ! (c >= 0x202A && c <= 0x202E)
                                       && c != 0xFEFF && c != 0xFFFD && c != 0xFF0F && c != 0xFF3C && c != 0x2215 && c != 0x2044;
        const bool ok = (c < 0x80 && juce::CharacterFunctions::isLetterOrDigit (c)) || printableNonAscii || c == ' ' || c == '-' || c == '_'
                        || c == '.' || c == '(' || c == ')' || c == '\'' || c == '+' || c == '#' || c == '&';
        out += ok ? juce::String::charToString (c) : juce::String ("_");
    }
    out = out.trim();
    while (out.startsWithChar ('.') || out.startsWithChar (' '))
        out = out.substring (1); // no hidden files, no leading spaces
    while (out.endsWithChar ('.') || out.endsWithChar (' '))
        out = out.dropLastCharacters (1); // Windows silently strips these
    if (out.length() > maxNameLength)
        out = out.substring (0, maxNameLength).trim();
    if (out.isEmpty() || out.containsOnly ("_-.() "))
        return {};
    if (isReservedWindowsName (out))
        out += "_";
    return out;
}

juce::File PresetStore::fileFor (const juce::String& cleanName) const
{
    if (cleanName.isEmpty() || cleanName != sanitizeName (cleanName))
        return {};
    const auto f = directory.getChildFile (cleanName + fileExtension);
    return f.getParentDirectory() == directory ? f : juce::File();
}

juce::StringArray PresetStore::listUserPresets() const
{
    juce::StringArray names;
    if (! directory.isDirectory())
        return names;
    for (const auto& entry : juce::RangedDirectoryIterator (directory, false, juce::String ("*") + fileExtension, juce::File::findFiles))
        names.add (entry.getFile().getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

juce::String PresetStore::existingNameFor (const juce::String& cleanName) const
{
    for (const auto& n : listUserPresets())
        if (n.equalsIgnoreCase (cleanName))
            return n;
    return {};
}

bool PresetStore::exists (const juce::String& name) const
{
    const auto clean = sanitizeName (name);
    return clean.isNotEmpty() && existingNameFor (clean).isNotEmpty();
}

std::unique_ptr<juce::XmlElement> PresetStore::toXml (const juce::String& name, const SoundState& state)
{
    auto root = std::make_unique<juce::XmlElement> ("GlitchKidPreset");
    root->setAttribute ("formatVersion", formatVersion);
    root->setAttribute ("name", name);
    root->setAttribute ("plugin", "GlitchKid");
    state.writeTo (*root);
    return root;
}

PresetStore::Result PresetStore::fromXml (const juce::XmlElement& xml, SoundState& out, juce::String* nameOut)
{
    if (! xml.hasTagName ("GlitchKidPreset"))
        return { false, "This file is not a GlitchKid preset." };
    const int version = xml.getIntAttribute ("formatVersion", 0);
    if (version < 1)
        return { false, "This preset has no valid format version, so it was not loaded." };
    SoundState s;
    if (! s.readFrom (xml))
        return { false, "This preset contains no settings." };
    out = std::move (s);
    if (nameOut != nullptr)
        *nameOut = xml.getStringAttribute ("name");
    if (version > formatVersion)
        return { true, "This preset was made by a newer GlitchKid. Settings this version does not know were ignored." };
    return { true, {} };
}

PresetStore::Result PresetStore::save (const juce::String& name, const SoundState& state, bool overwrite) const
{
    const auto clean = sanitizeName (name);
    if (clean.isEmpty())
        return { false, "Please type a name with letters or numbers." };

    const auto existing = existingNameFor (clean);
    if (existing.isNotEmpty() && ! overwrite)
        return { false, "A preset called \"" + existing + "\" already exists." };

    const auto target = fileFor (existing.isNotEmpty() ? existing : clean); // keeps the stored spelling when overwriting
    if (target == juce::File())
        return { false, "That name cannot be used." };

    if (! directory.exists() && ! directory.createDirectory().wasOk())
        return { false, "Could not create the preset folder: " + directory.getFullPathName() };

    const auto xml = toXml (existing.isNotEmpty() ? existing : clean, state);
    juce::TemporaryFile temp (target);
    if (! xml->writeTo (temp.getFile()) || ! temp.overwriteTargetFileWithTemporary())
        return { false, "Could not write the preset file. Nothing was changed." };
    return { true, {} };
}

PresetStore::Result PresetStore::load (const juce::String& name, SoundState& out) const
{
    const auto clean = sanitizeName (name);
    const auto stored = clean.isEmpty() ? juce::String() : existingNameFor (clean);
    if (stored.isEmpty())
        return { false, "That preset does not exist." };
    const auto file = fileFor (stored);
    if (file == juce::File() || ! file.existsAsFile())
        return { false, "That preset does not exist." };
    if (file.getSize() > maxFileBytes)
        return { false, "That preset file is too large to be a GlitchKid preset." };

    const auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr)
        return { false, "That preset file is damaged and could not be read." };
    return fromXml (*xml, out);
}

PresetStore::Result PresetStore::remove (const juce::String& name) const
{
    const auto clean = sanitizeName (name);
    const auto stored = clean.isEmpty() ? juce::String() : existingNameFor (clean);
    if (stored.isEmpty())
        return { false, "That preset does not exist." };
    const auto file = fileFor (stored);
    if (file == juce::File() || ! file.deleteFile())
        return { false, "Could not delete the preset file." };
    return { true, {} };
}
}
