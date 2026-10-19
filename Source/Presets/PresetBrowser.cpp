#include "PresetBrowser.h"
#include "FactoryPresets.h"
#include <algorithm>

namespace glitchkid::browser
{
namespace
{
juce::String squash (const juce::String& s)
{
    juce::String out;
    for (auto c : s.toLowerCase())
        if (c == '$')
            out << 's'; // KR$NA is typed "krsna"
        else if (juce::CharacterFunctions::isLetterOrDigit (c))
            out << c;
    return out;
}
}

const char* filterName (int filter)
{
    static const char* names[numFilters] = { "All 50", "International", "Indian DHH", "Melodic Rap", "Hardcore / Lyrical", "Trap", "Drill",
                                             "Rage / Distorted", "R&B / Pop-Rap", "Experimental", "Punjabi", "Hindi / Urdu" };
    return names[juce::jlimit (0, numFilters - 1, filter)];
}

const char* sortName (int sort)
{
    static const char* names[numSorts] = { "A-Z", "ORIGIN", "STYLE" };
    return names[juce::jlimit (0, numSorts - 1, sort)];
}

bool matchesFilter (int preset, int filter)
{
    if (preset < 0 || preset >= FactoryPresets::count)
        return false;
    const auto& i = FactoryPresets::info (preset);
    if (filter <= 0)
        return true;
    if (filter == 1)
        return i.region == FactoryPresets::Region::international;
    if (filter == 2)
        return i.region == FactoryPresets::Region::dhh;
    return (i.tags >> (filter - 3)) & 1u;
}

std::vector<int> run (const Query& q, std::uint64_t favourites)
{
    const auto needle = squash (q.text);
    std::vector<int> out;
    for (int i = 0; i < FactoryPresets::count; ++i)
    {
        if (! matchesFilter (i, q.filter))
            continue;
        if (q.favouritesOnly && ! ((favourites >> i) & 1u))
            continue;
        if (needle.isNotEmpty() && ! squash (FactoryPresets::info (i).artist).contains (needle) && ! squash (FactoryPresets::info (i).name).contains (needle))
            continue;
        out.push_back (i);
    }
    auto artist = [] (int i) { return juce::String (FactoryPresets::info (i).artist).toLowerCase(); };
    auto firstTag = [] (int i) { unsigned t = FactoryPresets::info (i).tags; int b = 0; while (b < 31 && ! ((t >> b) & 1u)) ++b; return b; };
    std::stable_sort (out.begin(), out.end(), [&] (int a, int b)
    {
        if (q.sort == origin && FactoryPresets::info (a).region != FactoryPresets::info (b).region)
            return FactoryPresets::info (a).region < FactoryPresets::info (b).region;
        if (q.sort == style && firstTag (a) != firstTag (b))
            return firstTag (a) < firstTag (b);
        return artist (a) < artist (b);
    });
    return out;
}

std::uint64_t parseFavourites (const juce::String& hex)
{
    if (! hex.containsOnly ("0123456789abcdefABCDEF"))
        return 0;
    const auto mask = hex.getHexValue64();
    return (std::uint64_t) mask & ((FactoryPresets::count >= 64) ? ~0ull : ((1ull << FactoryPresets::count) - 1ull));
}

juce::String formatFavourites (std::uint64_t mask) { return juce::String::toHexString ((juce::int64) mask); }
}
