#pragma once
#include <juce_core/juce_core.h>
#include <cstdint>
#include <vector>

namespace glitchkid::browser
{
// The logic behind the artist-preset gallery: which presets match a filter and a search text, in which order. Pure functions, no UI.
constexpr int numFilters = 12; // All 50, International, Indian DHH, then the nine style tags (FactoryPresets::Tag order)
const char* filterName (int filter);
bool matchesFilter (int preset, int filter);

enum Sort { alphabetical = 0, origin = 1, style = 2 };
constexpr int numSorts = 3;
const char* sortName (int sort);

struct Query
{
    int filter = 0;
    juce::String text;           // matches the artist or preset name, ignoring case and punctuation ("krsna" finds KR$NA)
    bool favouritesOnly = false;
    int sort = alphabetical;
};

// Preset indexes that match, ordered by the sort. `favourites` is a bit mask over preset indexes.
std::vector<int> run (const Query&, std::uint64_t favourites);
std::uint64_t parseFavourites (const juce::String& hex);
juce::String formatFavourites (std::uint64_t mask);
}
