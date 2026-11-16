#pragma once

// SPACE's vocal presets (this product's own settings, written for vocals). Values are in the parameters' own units (choice index, %, s, ms, Hz).
// Applying one writes the space_* parameters; nothing else changes. Build My Mix starts from one of these per profile.
namespace glitchkid::space
{
struct Preset
{
    const char* name;
    int algorithm, character;
    float mix, decay, predelay, size, diffusion, damping, lowCut, highCut, modRate, modDepth, width, early, late;
    const char* about;
};

inline constexpr Preset presets[] = {
    { "Tight Vocal Room",         0, 1, 14, 0.6f, 8,  35, 75, 55, 220, 8000,  0.5f,  15, 80,  60, 50, "A small, close room: the voice in a real space without a tail." },
    { "Intimate Vocal",           1, 0, 16, 1.1f, 15, 40, 80, 60, 200, 7000,  0.4f,  25, 85,  45, 70, "A warm, short chamber that keeps the voice up close." },
    { "Smooth Plate",             2, 0, 20, 1.8f, 25, 55, 85, 55, 180, 8000,  0.6f,  30, 100, 0,  85, "A dense, dark plate: the classic polished vocal." },
    { "Bright Plate",             2, 2, 18, 1.6f, 20, 50, 80, 30, 250, 14000, 0.8f,  20, 110, 0,  85, "A bright, clear plate for pop and present vocals." },
    { "Wide Vocal Hall",          3, 1, 22, 2.6f, 35, 65, 75, 50, 200, 9000,  0.5f,  35, 130, 40, 85, "A wide hall with a pre-delay that keeps the words clear." },
    { "Deep Melodic Vocal",       3, 0, 26, 3.4f, 45, 75, 80, 60, 180, 7500,  0.45f, 45, 120, 35, 90, "A long, warm hall for melodic lines." },
    { "Atmospheric Vocal",        6, 0, 30, 5.0f, 60, 85, 85, 55, 250, 8000,  0.35f, 60, 140, 25, 95, "A slow, swimming space around the voice." },
    { "Long Ambient Tail",        4, 1, 32, 9.0f, 80, 90, 80, 50, 300, 9000,  0.3f,  40, 140, 30, 95, "A cathedral-length tail for sparse, open moments." },
    { "Experimental Vocal Space", 7, 2, 28, 6.0f, 40, 80, 90, 35, 300, 12000, 0.6f,  50, 140, 20, 90, "A shimmering space with an octave above the tail." } };
inline constexpr int numPresets = (int) (sizeof (presets) / sizeof (presets[0]));
}
