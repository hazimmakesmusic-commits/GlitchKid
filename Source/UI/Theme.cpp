#include "Theme.h"
#include "PlaygroundWidgets.h"

namespace glitchkid::ui::theme
{
namespace
{
// Each profile: primary (white text on it, AA), secondary (accent text on the background, AA), soft, deep. Contrast is pinned by VisualTests.
const Profile profiles[numProfiles] = {
    { "viridian", "VIRIDIAN", juce::Colour (0xff0d7f5c), juce::Colour (0xff2fe0a0), juce::Colour (0xffa8f2d6), juce::Colour (0xff0a4f57) }, // emerald -> deep teal
    { "magenta",  "MAGENTA",  juce::Colour (0xff8c4cf7), juce::Colour (0xffff4fb8), juce::Colour (0xffc4a6ff), juce::Colour (0xff3b1a7a) }, // hot pink -> ultraviolet
    { "sapphire", "SAPPHIRE", juce::Colour (0xff2563eb), juce::Colour (0xff4fa8ff), juce::Colour (0xffb5d6ff), juce::Colour (0xff0b1f4d) }, // electric -> midnight blue
    { "amber",    "AMBER",    juce::Colour (0xffa8501c), juce::Colour (0xffff9a3d), juce::Colour (0xffffd4a8), juce::Colour (0xff5c2810) }, // warm orange -> burnt copper
    { "void",     "VOID",     juce::Colour (0xff6a6f7a), juce::Colour (0xffc9ced6), juce::Colour (0xffeef0f3), juce::Colour (0xff2a2d33) }, // silver -> graphite
};
int currentIndex = defaultProfile, previousIndex = defaultProfile;
juce::File overrideFile;
juce::ChangeBroadcaster& broadcaster()
{
    static juce::ChangeBroadcaster b;
    return b;
}
}

const Profile& profile (int index) { return profiles[juce::jlimit (0, numProfiles - 1, index)]; }
int current() { return currentIndex; }
const Profile& active() { return profile (currentIndex); }
const Profile& previous() { return profile (previousIndex); }
juce::ChangeBroadcaster& changes() { return broadcaster(); }

void apply (int index)
{
    index = juce::jlimit (0, numProfiles - 1, index);
    previousIndex = currentIndex;
    currentIndex = index;
    const auto& p = profile (index);
    violet = p.primary;
    pink = p.secondary;
    neon = p.soft;
    if (previousIndex != currentIndex)
        broadcaster().sendSynchronousChangeMessage();
}

juce::ColourGradient primaryGradient (juce::Point<float> from, juce::Point<float> to, float alpha)
{
    const auto& p = active();
    juce::ColourGradient g (p.secondary.withMultipliedAlpha (alpha), from, p.primary.withMultipliedAlpha (alpha), to, false);
    g.addColour (0.55, p.secondary.interpolatedWith (p.primary, 0.6f).withMultipliedAlpha (alpha));
    return g;
}

juce::ColourGradient secondaryGradient (juce::Point<float> from, juce::Point<float> to, float alpha)
{
    const auto& p = active();
    return juce::ColourGradient (p.primary.withMultipliedAlpha (alpha), from, p.deep.withMultipliedAlpha (alpha), to, false);
}

juce::File settingsFile()
{
    if (overrideFile != juce::File())
        return overrideFile;
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
#if JUCE_MAC
    base = base.getChildFile ("Application Support"); // JUCE returns ~/Library here (as the preset folder)
#endif
    return base.getChildFile ("GlitchKid").getChildFile ("Settings.xml");
}

void setSettingsFile (const juce::File& f) { overrideFile = f; }

int loadSaved()
{
    if (auto xml = juce::XmlDocument::parse (settingsFile()))
    {
        const auto id = xml->getStringAttribute ("colourProfile");
        for (int i = 0; i < numProfiles; ++i)
            if (id == profiles[i].id)
                return i;
    }
    return defaultProfile; // no file, unreadable or unknown: the default, never an error
}

void save (int index)
{
    const auto f = settingsFile();
    std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (f));
    if (xml == nullptr || ! xml->hasTagName ("GlitchKidSettings"))
        xml = std::make_unique<juce::XmlElement> ("GlitchKidSettings"); // keeps any other setting stored later in the same file
    xml->setAttribute ("colourProfile", profile (index).id);
    f.getParentDirectory().createDirectory();
    xml->writeTo (f);
}

void retint (juce::Component& root, const Profile& from, const Profile& to)
{
    static const int ids[] = { juce::Label::textColourId, juce::Label::outlineColourId, juce::TextButton::buttonOnColourId, juce::TextButton::textColourOnId,
                               juce::TextButton::textColourOffId, juce::TextButton::buttonColourId, juce::Slider::rotarySliderFillColourId, juce::Slider::trackColourId,
                               juce::Slider::thumbColourId, juce::Slider::textBoxTextColourId, juce::ToggleButton::tickColourId, juce::ToggleButton::textColourId,
                               juce::ComboBox::outlineColourId, juce::ComboBox::arrowColourId, juce::ComboBox::textColourId, juce::ComboBox::focusedOutlineColourId };
    const std::pair<juce::Colour, juce::Colour> map[] = { { from.primary, to.primary }, { from.secondary, to.secondary }, { from.soft, to.soft } };
    std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
    {
        for (int id : ids)
            if (c.isColourSpecified (id))
            {
                const auto col = c.findColour (id);
                for (auto& [a, b] : map)
                    if (col.withAlpha (1.0f) == a)
                    {
                        c.setColour (id, b.withAlpha (col.getFloatAlpha()));
                        break;
                    }
            }
        for (auto* ch : c.getChildren())
            walk (*ch);
    };
    walk (root);
}
}
