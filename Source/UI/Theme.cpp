#include "Theme.h"
#include "PlaygroundWidgets.h"
#include "NeonKit.h"

namespace glitchkid::ui::theme
{
namespace
{
// The GLITCHKID brand tokens (VIRIDIAN, the default identity). The website uses the same values (website/src/styles.css).
const juce::Colour viridianC (0xff00dda5), emeraldC (0xff00b894), deepTealC (0xff08776f);
const juce::Colour voidC (0xff080d0d), surfaceC (0xff111a18), elevatedC (0xff1b2925), borderC (0xff293a34), textC (0xfff1f7f4), textDimC (0xff91a69c);

// VIRIDIAN's teal-black foundation; hint text is a derived shade (secondary text sunk 45 % into the void), not a new colour.
const Foundation tealBlack { voidC, voidC, surfaceC, elevatedC, borderC, borderC, voidC, textC, textDimC, textDimC.interpolatedWith (voidC, 0.45f) };
// The original near-black and graphite, kept for the other profiles.
const Foundation graphite { juce::Colour (0xff101014), juce::Colour (0xff060509), juce::Colour (0xff1b1921), juce::Colour (0xff25222d), juce::Colour (0xff3a3644),
                            juce::Colour (0xff25222d), juce::Colour (0xff0d0d11), juce::Colour (0xfff0ecf5), juce::Colour (0xffa7a0b2), juce::Colour (0xff5d5868) };
juce::Colour between (juce::Colour a, juce::Colour b) { return a.interpolatedWith (b, 0.6f); }

// Each profile: primary (white text on it, AA), secondary (accent text on the background, AA), soft, deep, the gradient's centre and the foundation.
// Contrast is pinned by VisualTests. VIRIDIAN: deep teal carries white text (5.4:1), viridian is the bright accent, emerald the quiet one (hover, focus).
const Profile profiles[numProfiles] = {
    { "viridian", "VIRIDIAN", deepTealC, viridianC, emeraldC, deepTealC, emeraldC, tealBlack }, // viridian -> emerald -> deep teal
    { "magenta",  "MAGENTA",  juce::Colour (0xff8c4cf7), juce::Colour (0xffff4fb8), juce::Colour (0xffc4a6ff), juce::Colour (0xff3b1a7a),
      between (juce::Colour (0xffff4fb8), juce::Colour (0xff8c4cf7)), graphite }, // hot pink -> ultraviolet
    { "sapphire", "SAPPHIRE", juce::Colour (0xff2563eb), juce::Colour (0xff4fa8ff), juce::Colour (0xffb5d6ff), juce::Colour (0xff0b1f4d),
      between (juce::Colour (0xff4fa8ff), juce::Colour (0xff2563eb)), graphite }, // electric -> midnight blue
    { "amber",    "AMBER",    juce::Colour (0xffa8501c), juce::Colour (0xffff9a3d), juce::Colour (0xffffd4a8), juce::Colour (0xff5c2810),
      between (juce::Colour (0xffff9a3d), juce::Colour (0xffa8501c)), graphite }, // warm orange -> burnt copper
    { "void",     "VOID",     juce::Colour (0xff6a6f7a), juce::Colour (0xffc9ced6), juce::Colour (0xffeef0f3), juce::Colour (0xff2a2d33),
      between (juce::Colour (0xffc9ced6), juce::Colour (0xff6a6f7a)), graphite }, // silver -> graphite
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
    const auto& b = p.base; // the foundation: both workspaces' names
    obsidian = b.window;
    panelBottom = b.panel;
    panelTop = b.raised;
    border = b.border;
    well = b.well;
    textBright = b.text;
    textDim = b.textDim;
    disabled = b.faint;
    nk::bgTop = b.window;
    nk::bgBottom = b.windowLow;
    nk::surface = b.panel;
    nk::surfaceHi = b.raised;
    nk::hairline = b.hairline;
    nk::dim = b.textDim;
    nk::faint = b.faint;
    if (previousIndex != currentIndex)
        broadcaster().sendSynchronousChangeMessage();
}

juce::ColourGradient primaryGradient (juce::Point<float> from, juce::Point<float> to, float alpha)
{
    const auto& p = active();
    juce::ColourGradient g (p.secondary.withMultipliedAlpha (alpha), from, p.primary.withMultipliedAlpha (alpha), to, false);
    g.addColour (0.48, p.mid.withMultipliedAlpha (alpha));
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
                               juce::ComboBox::outlineColourId, juce::ComboBox::arrowColourId, juce::ComboBox::textColourId, juce::ComboBox::focusedOutlineColourId,
                               juce::ComboBox::backgroundColourId, juce::TextEditor::backgroundColourId, juce::TextEditor::textColourId,
                               juce::TextEditor::outlineColourId, juce::TextEditor::focusedOutlineColourId, juce::ListBox::backgroundColourId };
    const auto &f = from.base, &t = to.base; // accents first, then the foundation (text before surfaces: a colour equal to both maps as text)
    const std::pair<juce::Colour, juce::Colour> map[] = { { from.primary, to.primary }, { from.secondary, to.secondary }, { from.soft, to.soft },
                                                          { f.text, t.text },         { f.textDim, t.textDim },          { f.faint, t.faint },
                                                          { f.window, t.window },     { f.panel, t.panel },              { f.raised, t.raised },
                                                          { f.border, t.border },     { f.well, t.well },                { f.windowLow, t.windowLow } };
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
