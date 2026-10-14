#include "Moods.h"
#include <cstring>

namespace
{
using Moods::Value;

// Common layout of every Mood table: EQ, compressor, saturation, Liquid Echo, Dimension.
// comp_makeup is tuned per Mood so the level on speech-like noise stays within about 0.2 dB of the input (measured, docs/TESTING.md);
// real voices differ, so watch the OUT meter.
// Echo division indices: 0 1/2, 1 1/4, 2 1/4 dotted, 3 1/4 triplet, 4 1/8, 5 1/8 dotted, 6 1/8 triplet, 7 1/16, 8 1/16 dotted, 9 1/16 triplet, 10 1/32.

// DREAM: smooth, spacious, atmospheric. Clean the mud, lift the air, gentle saturation, dotted-quarter echo and a soft stereo bloom.
constexpr Value dream[] = {
    { "eq_lo_freq", 120.0f },  { "eq_lo_gain", -2.0f },
    { "eq_b1_freq", 300.0f },  { "eq_b1_gain", -1.5f }, { "eq_b1_q", 0.9f },
    { "eq_b2_freq", 5000.0f }, { "eq_b2_gain", 1.5f },  { "eq_b2_q", 0.8f },
    { "eq_hi_freq", 10000.0f }, { "eq_hi_gain", 2.5f },
    { "comp_thresh", -20.0f }, { "comp_ratio", 2.0f }, { "comp_attack", 15.0f }, { "comp_release", 150.0f }, { "comp_knee", 10.0f }, { "comp_makeup", 3.0f },
    { "sat_drive", 3.0f },
    { "echo_on", 1.0f }, { "echo_sync", 1.0f }, { "echo_time", 750.0f }, { "echo_div", 2.0f }, { "echo_fb", 38.0f }, { "echo_tone", 3500.0f }, { "echo_width", 60.0f }, { "echo_mix", 22.0f },
    { "dim_on", 1.0f }, { "dim_width", 70.0f }, { "dim_detune", 7.0f }, { "dim_depth", 45.0f }, { "dim_mix", 70.0f } };

// DEMON: dark, heavy, assertive. Weight below, darker top, firm compression, strong saturation, short tight echo, little width.
constexpr Value demon[] = {
    { "eq_lo_freq", 100.0f },  { "eq_lo_gain", 4.0f },
    { "eq_b1_freq", 250.0f },  { "eq_b1_gain", -2.0f }, { "eq_b1_q", 1.2f },
    { "eq_b2_freq", 2500.0f }, { "eq_b2_gain", 2.0f },  { "eq_b2_q", 1.0f },
    { "eq_hi_freq", 8000.0f }, { "eq_hi_gain", -3.0f },
    { "comp_thresh", -24.0f }, { "comp_ratio", 4.0f }, { "comp_attack", 8.0f }, { "comp_release", 120.0f }, { "comp_knee", 6.0f }, { "comp_makeup", 2.0f },
    { "sat_drive", 9.0f },
    { "echo_on", 1.0f }, { "echo_sync", 1.0f }, { "echo_time", 250.0f }, { "echo_div", 4.0f }, { "echo_fb", 25.0f }, { "echo_tone", 1800.0f }, { "echo_width", 20.0f }, { "echo_mix", 12.0f },
    { "dim_on", 1.0f }, { "dim_width", 30.0f }, { "dim_detune", 4.0f }, { "dim_depth", 35.0f }, { "dim_mix", 60.0f } };

// ALIEN: strange and futuristic. Nasal mid colour, bright top, wide detuned Dimension, fast triplet echo with long feedback.
constexpr Value alien[] = {
    { "eq_lo_freq", 150.0f },  { "eq_lo_gain", -3.0f },
    { "eq_b1_freq", 1500.0f }, { "eq_b1_gain", 3.0f },  { "eq_b1_q", 3.0f },
    { "eq_b2_freq", 6000.0f }, { "eq_b2_gain", -2.0f }, { "eq_b2_q", 2.5f },
    { "eq_hi_freq", 12000.0f }, { "eq_hi_gain", 3.0f },
    { "comp_thresh", -22.0f }, { "comp_ratio", 2.5f }, { "comp_attack", 10.0f }, { "comp_release", 100.0f }, { "comp_knee", 8.0f }, { "comp_makeup", 4.0f },
    { "sat_drive", 5.0f },
    { "echo_on", 1.0f }, { "echo_sync", 1.0f }, { "echo_time", 167.0f }, { "echo_div", 9.0f }, { "echo_fb", 55.0f }, { "echo_tone", 5000.0f }, { "echo_width", 100.0f }, { "echo_mix", 28.0f },
    { "dim_on", 1.0f }, { "dim_width", 100.0f }, { "dim_detune", 14.0f }, { "dim_depth", 80.0f }, { "dim_mix", 100.0f } };

// RAGE: forward, punchy, high energy. Fast firm compression, strong saturation, presence boost, dry (no echo, no Dimension) for clarity.
constexpr Value rage[] = {
    { "eq_lo_freq", 120.0f },  { "eq_lo_gain", 2.0f },
    { "eq_b1_freq", 400.0f },  { "eq_b1_gain", -2.5f }, { "eq_b1_q", 1.0f },
    { "eq_b2_freq", 3500.0f }, { "eq_b2_gain", 3.5f },  { "eq_b2_q", 1.0f },
    { "eq_hi_freq", 7000.0f }, { "eq_hi_gain", 1.0f },
    { "comp_thresh", -26.0f }, { "comp_ratio", 6.0f }, { "comp_attack", 4.0f }, { "comp_release", 90.0f }, { "comp_knee", 4.0f }, { "comp_makeup", 2.0f },
    { "sat_drive", 10.0f },
    { "echo_on", 0.0f }, { "echo_sync", 1.0f }, { "echo_time", 350.0f }, { "echo_div", 4.0f }, { "echo_fb", 35.0f }, { "echo_tone", 6000.0f }, { "echo_width", 0.0f }, { "echo_mix", 30.0f },
    { "dim_on", 0.0f }, { "dim_width", 60.0f }, { "dim_detune", 6.0f }, { "dim_depth", 50.0f }, { "dim_mix", 100.0f } };

// GHOST: airy, distant, ethereal. Thin the low end, lift the air, no saturation, long dark quarter-note echo, restrained width.
constexpr Value ghost[] = {
    { "eq_lo_freq", 200.0f },  { "eq_lo_gain", -5.0f },
    { "eq_b1_freq", 500.0f },  { "eq_b1_gain", -2.0f }, { "eq_b1_q", 0.7f },
    { "eq_b2_freq", 8000.0f }, { "eq_b2_gain", 2.0f },  { "eq_b2_q", 0.7f },
    { "eq_hi_freq", 12000.0f }, { "eq_hi_gain", 4.0f },
    { "comp_thresh", -18.0f }, { "comp_ratio", 1.8f }, { "comp_attack", 20.0f }, { "comp_release", 200.0f }, { "comp_knee", 12.0f }, { "comp_makeup", 3.5f },
    { "sat_drive", 0.0f },
    { "echo_on", 1.0f }, { "echo_sync", 1.0f }, { "echo_time", 500.0f }, { "echo_div", 1.0f }, { "echo_fb", 50.0f }, { "echo_tone", 2500.0f }, { "echo_width", 70.0f }, { "echo_mix", 32.0f },
    { "dim_on", 1.0f }, { "dim_width", 35.0f }, { "dim_detune", 5.0f }, { "dim_depth", 40.0f }, { "dim_mix", 50.0f } };

template <size_t N> constexpr int n (const Value (&)[N]) { return (int) N; }

const Moods::Mood moods[Moods::count] = {
    { "dream", "DREAM", "Smooth and spacious: soft air, gentle saturation, dotted echo, light stereo bloom.", dream, n (dream) },
    { "demon", "DEMON", "Dark and heavy: low weight, darker top, firm compression, strong saturation.", demon, n (demon) },
    { "alien", "ALIEN", "Strange and futuristic: nasal colour, wide detuned Dimension, fast triplet echo.", alien, n (alien) },
    { "rage",  "RAGE",  "Forward and punchy: fast firm compression, strong saturation, presence boost, no echo.", rage, n (rage) },
    { "ghost", "GHOST", "Airy and distant: thin lows, bright air, long dark echo, restrained width.", ghost, n (ghost) } };
} // namespace

namespace Moods
{
const Mood& get (int index)
{
    return moods[index < 0 ? 0 : (index >= count ? count - 1 : index)];
}

int indexOfId (const char* id)
{
    for (int i = 0; i < count; ++i)
        if (std::strcmp (moods[i].id, id) == 0)
            return i;
    return -1;
}

int apply (const Mood& mood, const std::function<bool (const char*, float)>& setter)
{
    int applied = 0;
    for (int i = 0; i < mood.numValues; ++i)
        if (setter (mood.values[i].id, mood.values[i].value))
            ++applied;
    return applied;
}
}
