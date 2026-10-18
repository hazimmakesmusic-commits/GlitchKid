#include "FactoryPresets.h"

namespace glitchkid::FactoryPresets
{
namespace
{
struct Override { const char* id; float value; };

// Liquid Echo division choice indexes: 2 = 1/4 dotted, 5 = 1/8 dotted, 9 = 1/16 triplet. Tune scale 0 = Chromatic (key does not matter).
// Latency: any preset with Tune on reports Pitch Tune latency (~31 ms at 48 kHz); the third also reports FORMANT latency (~21 ms).
const Override astroMode[] = {
    { "tune_on", 1 }, { "tune_key", 0 }, { "tune_scale", 0 }, { "tune_strength", 85 }, { "tune_speed", 12 },
    { "eq_lo_gain", 1.5f }, { "eq_b1_freq", 400 }, { "eq_b1_gain", -2.0f }, { "eq_hi_freq", 8000 }, { "eq_hi_gain", -3.0f },
    { "comp_thresh", -20 }, { "comp_ratio", 3 }, { "comp_attack", 12 }, { "comp_release", 120 }, { "comp_knee", 8 }, { "comp_makeup", 3 },
    { "sat_drive", 4 },
    { "echo_on", 1 }, { "echo_sync", 1 }, { "echo_div", 2 }, { "echo_fb", 40 }, { "echo_tone", 2500 }, { "echo_width", 70 }, { "echo_mix", 28 },
    { "dim_on", 1 }, { "dim_width", 60 }, { "dim_detune", 6 }, { "dim_depth", 50 }, { "dim_mix", 35 },
    { "dbl_on", 1 }, { "dbl_amount", 35 }, { "dbl_width", 80 }, { "dbl_detune", 8 }, { "dbl_timing", 18 } };

const Override heartbreak[] = {
    { "tune_on", 1 }, { "tune_key", 0 }, { "tune_scale", 0 }, { "tune_strength", 70 }, { "tune_speed", 35 },
    { "eq_lo_gain", 1.0f }, { "eq_b1_freq", 350 }, { "eq_b1_gain", -1.5f }, { "eq_b2_freq", 3500 }, { "eq_b2_gain", 1.5f },
    { "eq_hi_freq", 9000 }, { "eq_hi_gain", 1.0f },
    { "comp_thresh", -18 }, { "comp_ratio", 2.5f }, { "comp_attack", 15 }, { "comp_release", 150 }, { "comp_knee", 10 }, { "comp_makeup", 2.5f },
    { "sat_drive", 1.5f },
    { "echo_on", 1 }, { "echo_sync", 1 }, { "echo_div", 5 }, { "echo_fb", 25 }, { "echo_tone", 3500 }, { "echo_width", 20 }, { "echo_mix", 15 } };

const Override digitalAlterEgo[] = {
    { "tune_on", 1 }, { "tune_key", 0 }, { "tune_scale", 0 }, { "tune_strength", 100 }, { "tune_speed", 0 }, { "formant_shift", 3.0f },
    { "eq_lo_gain", -2.0f }, { "eq_b2_freq", 3000 }, { "eq_b2_gain", 1.5f }, { "eq_hi_freq", 8000 }, { "eq_hi_gain", 3.0f },
    { "comp_thresh", -20 }, { "comp_ratio", 3 }, { "comp_attack", 10 }, { "comp_release", 100 }, { "comp_knee", 6 }, { "comp_makeup", 3 },
    { "sat_drive", 6 },
    { "echo_on", 1 }, { "echo_sync", 1 }, { "echo_div", 9 }, { "echo_fb", 35 }, { "echo_tone", 5000 }, { "echo_width", 80 }, { "echo_mix", 25 },
    { "dim_on", 1 }, { "dim_width", 70 }, { "dim_detune", 10 }, { "dim_depth", 60 }, { "dim_mix", 40 },
    { "mut_on", 1 }, { "mut_amount", 20 }, { "mut_character", 50 }, { "mut_blend", 60 } };

struct Entry { Info info; const Override* overrides; size_t numOverrides; };

#define GK_ENTRY(arr) arr, sizeof (arr) / sizeof (arr[0])
const Entry entries[count] = {
    { { "TRAVIS SCOTT STYLE", "ASTRO MODE: dark, spacious, psychedelic melodic rap" },  GK_ENTRY (astroMode) },
    { { "JUICE WRLD STYLE",   "HEARTBREAK: intimate, emotional melodic rap" },           GK_ENTRY (heartbreak) },
    { { "CHEROMANI+ STYLE",   "DIGITAL ALTER EGO: playful, futuristic, vocal-forward" }, GK_ENTRY (digitalAlterEgo) } };
#undef GK_ENTRY
}

const Info& info (int index) { return entries[juce::jlimit (0, count - 1, index)].info; }

int indexOfName (const juce::String& name)
{
    for (int i = 0; i < count; ++i)
        if (name.equalsIgnoreCase (entries[i].info.name))
            return i;
    return -1;
}

SoundState build (int index, const SoundState& factoryDefault)
{
    SoundState s = factoryDefault;
    const auto& e = entries[juce::jlimit (0, count - 1, index)];
    for (size_t i = 0; i < e.numOverrides; ++i)
        s.set (e.overrides[i].id, e.overrides[i].value);
    s.egoId = {};
    return s;
}
}
