#include "AlterEgos.h"
#include <cstring>

namespace
{
using namespace AlterEgos;

// One row per owned parameter: its value at Chaos 0 (a) and at Chaos 1 (b). Fixed parameters have a == b.
struct Row { const char* id; float a; float b; };
constexpr Row K (const char* id, float v) { return { id, v, v }; }         // does not move with Chaos
constexpr Row M (const char* id, float a, float b) { return { id, a, b }; } // moves linearly with Chaos

// Every table lists the same 30 parameters in the same order: EQ, compressor, saturation, Liquid Echo, Dimension.
// Echo division indices: 0 1/2, 1 1/4, 2 1/4 dotted, 3 1/4 triplet, 4 1/8, 5 1/8 dotted, 6 1/8 triplet, 7 1/16, 8 1/16 dotted, 9 1/16 triplet, 10 1/32.
// Safe limits used by every table (checked by the tests): drive <= 16 dB, echo feedback <= 75 % (the DSP itself caps at 90 %),
// detune <= 20 cents, switches never move. comp_makeup is tuned per table from measurements so the level on speech-like noise stays
// within about 0.5 dB at Chaos 0 and Chaos 1 (see docs/TESTING.md); real voices differ, so watch the OUT meter.

// CELESTIAL: polished, glossy, airy. Clean lows, lifted air, light saturation, dotted-quarter echo, soft thickening.
constexpr Row celestial[numOwned] = {
    K ("eq_lo_freq", 110.0f), M ("eq_lo_gain", -2.0f, -3.0f),
    K ("eq_b1_freq", 300.0f), M ("eq_b1_gain", -1.5f, -2.0f), K ("eq_b1_q", 0.9f),
    K ("eq_b2_freq", 5500.0f), M ("eq_b2_gain", 1.5f, 3.0f), K ("eq_b2_q", 0.8f),
    K ("eq_hi_freq", 11000.0f), M ("eq_hi_gain", 3.0f, 5.0f),
    K ("comp_thresh", -20.0f), M ("comp_ratio", 2.0f, 2.5f), K ("comp_attack", 15.0f), K ("comp_release", 150.0f), K ("comp_knee", 10.0f), M ("comp_makeup", 2.6f, 1.7f),
    M ("sat_drive", 3.0f, 6.0f),
    K ("echo_on", 1.0f), K ("echo_sync", 1.0f), K ("echo_time", 750.0f), K ("echo_div", 2.0f), M ("echo_fb", 35.0f, 50.0f), M ("echo_tone", 4500.0f, 6500.0f), M ("echo_width", 60.0f, 90.0f), M ("echo_mix", 16.0f, 30.0f),
    K ("dim_on", 1.0f), M ("dim_width", 60.0f, 85.0f), M ("dim_detune", 6.0f, 10.0f), M ("dim_depth", 40.0f, 65.0f), M ("dim_mix", 70.0f, 85.0f) };

// THE CLONE: fuller, thicker, layered. Dimension is the defining effect (two modulated voices, centred); a short slap echo adds depth.
constexpr Row theClone[numOwned] = {
    K ("eq_lo_freq", 120.0f), K ("eq_lo_gain", -1.0f),
    K ("eq_b1_freq", 250.0f), K ("eq_b1_gain", -1.5f), K ("eq_b1_q", 1.0f),
    K ("eq_b2_freq", 3500.0f), M ("eq_b2_gain", 1.0f, 2.0f), K ("eq_b2_q", 0.9f),
    K ("eq_hi_freq", 9000.0f), M ("eq_hi_gain", 1.5f, 2.5f),
    K ("comp_thresh", -22.0f), M ("comp_ratio", 2.5f, 3.5f), K ("comp_attack", 12.0f), K ("comp_release", 130.0f), K ("comp_knee", 8.0f), M ("comp_makeup", 3.6f, 3.4f),
    M ("sat_drive", 2.0f, 5.0f),
    K ("echo_on", 1.0f), K ("echo_sync", 1.0f), K ("echo_time", 250.0f), K ("echo_div", 4.0f), M ("echo_fb", 20.0f, 35.0f), K ("echo_tone", 3500.0f), M ("echo_width", 40.0f, 70.0f), M ("echo_mix", 6.0f, 14.0f),
    K ("dim_on", 1.0f), M ("dim_width", 55.0f, 85.0f), M ("dim_detune", 8.0f, 16.0f), M ("dim_depth", 70.0f, 100.0f), K ("dim_mix", 100.0f) };

// VOIDWALKER: dark, spacious, distant, cinematic. Weight below, darker top, long dark quarter-note echo, slow wide movement.
constexpr Row voidwalker[numOwned] = {
    K ("eq_lo_freq", 90.0f), M ("eq_lo_gain", 2.0f, 3.5f),
    K ("eq_b1_freq", 300.0f), K ("eq_b1_gain", -1.0f), K ("eq_b1_q", 0.9f),
    K ("eq_b2_freq", 4000.0f), M ("eq_b2_gain", -2.0f, -4.0f), K ("eq_b2_q", 0.8f),
    K ("eq_hi_freq", 5000.0f), M ("eq_hi_gain", -4.0f, -8.0f),
    K ("comp_thresh", -20.0f), M ("comp_ratio", 2.2f, 3.0f), K ("comp_attack", 18.0f), K ("comp_release", 180.0f), K ("comp_knee", 10.0f), M ("comp_makeup", 5.4f, 6.1f),
    M ("sat_drive", 3.0f, 6.0f),
    K ("echo_on", 1.0f), K ("echo_sync", 1.0f), K ("echo_time", 500.0f), K ("echo_div", 1.0f), M ("echo_fb", 48.0f, 65.0f), M ("echo_tone", 1600.0f, 900.0f), M ("echo_width", 70.0f, 100.0f), M ("echo_mix", 24.0f, 42.0f),
    K ("dim_on", 1.0f), M ("dim_width", 40.0f, 80.0f), M ("dim_detune", 5.0f, 9.0f), M ("dim_depth", 40.0f, 70.0f), M ("dim_mix", 60.0f, 80.0f) };

// THE PARASITE: distorted, textured, unnerving. Saturation and EQ do the work (nasal mid push, thin lows, hard compression);
// a very short, brightish echo adds a metallic flutter; Dimension adds a restless, detuned width. Glitch Stutter is not used.
constexpr Row theParasite[numOwned] = {
    K ("eq_lo_freq", 140.0f), M ("eq_lo_gain", -3.0f, -4.0f),
    K ("eq_b1_freq", 1200.0f), M ("eq_b1_gain", 3.0f, 6.0f), M ("eq_b1_q", 2.5f, 4.0f),
    K ("eq_b2_freq", 2800.0f), M ("eq_b2_gain", 2.0f, 3.5f), K ("eq_b2_q", 1.5f),
    K ("eq_hi_freq", 8000.0f), M ("eq_hi_gain", -2.0f, -4.0f),
    M ("comp_thresh", -26.0f, -38.0f), M ("comp_ratio", 4.0f, 8.0f), M ("comp_attack", 6.0f, 3.0f), M ("comp_release", 110.0f, 70.0f), M ("comp_knee", 6.0f, 3.0f), M ("comp_makeup", 3.0f, 7.1f),
    M ("sat_drive", 9.0f, 16.0f),
    K ("echo_on", 1.0f), K ("echo_sync", 1.0f), K ("echo_time", 125.0f), K ("echo_div", 10.0f), M ("echo_fb", 30.0f, 60.0f), M ("echo_tone", 3000.0f, 4500.0f), M ("echo_width", 30.0f, 80.0f), M ("echo_mix", 8.0f, 22.0f),
    K ("dim_on", 1.0f), M ("dim_width", 60.0f, 100.0f), M ("dim_detune", 10.0f, 20.0f), M ("dim_depth", 50.0f, 90.0f), M ("dim_mix", 70.0f, 100.0f) };

// ALTERED STATE: unusual, experimental, psychedelic. A swirling triplet echo with long feedback and wide detuned Dimension are the
// defining sound; the EQ adds a vowel-like mid resonance; saturation glues it. Lower Chaos for something tamer.
constexpr Row alteredState[numOwned] = {
    K ("eq_lo_freq", 160.0f), K ("eq_lo_gain", -3.0f),
    M ("eq_b1_freq", 1500.0f, 2400.0f), M ("eq_b1_gain", 3.0f, 5.0f), M ("eq_b1_q", 3.0f, 6.0f),
    K ("eq_b2_freq", 6000.0f), M ("eq_b2_gain", -2.0f, -3.0f), K ("eq_b2_q", 2.5f),
    K ("eq_hi_freq", 12000.0f), M ("eq_hi_gain", 3.0f, 5.0f),
    K ("comp_thresh", -22.0f), M ("comp_ratio", 2.5f, 4.0f), K ("comp_attack", 10.0f), K ("comp_release", 100.0f), K ("comp_knee", 8.0f), M ("comp_makeup", 3.9f, 0.9f),
    M ("sat_drive", 5.0f, 9.0f),
    K ("echo_on", 1.0f), K ("echo_sync", 1.0f), K ("echo_time", 167.0f), K ("echo_div", 6.0f), M ("echo_fb", 55.0f, 75.0f), M ("echo_tone", 4500.0f, 7500.0f), K ("echo_width", 100.0f), M ("echo_mix", 25.0f, 42.0f),
    K ("dim_on", 1.0f), K ("dim_width", 100.0f), M ("dim_detune", 14.0f, 20.0f), M ("dim_depth", 80.0f, 100.0f), K ("dim_mix", 100.0f) };

const Info infos[count] = {
    { "celestial",  "CELESTIAL",     "Polished, glossy and airy: lifted top, soft saturation, dotted echo, gentle thickening.",
      eq | comp | sat | echo, dim },
    { "the_clone",  "THE CLONE",     "Fuller and layered: two detuned voices spread around a steady centre, plus a short slap.",
      eq | comp | sat | dim, echo },
    { "voidwalker", "VOIDWALKER",    "Dark, distant and cinematic: weight below, closed top, long dark echo, slow wide movement.",
      eq | comp | sat | echo, dim },
    { "the_parasite", "THE PARASITE", "Distorted and unnerving: hard drive, nasal push, metallic flutter, restless detuned width.",
      eq | comp | sat, echo | dim },
    { "altered_state", "ALTERED STATE", "Psychedelic and strange: swirling triplet echo, wide detuned spread, vowel-like resonance.",
      eq | comp | sat | echo | dim, 0 } };

const Row* const tables[count] = { celestial, theClone, voidwalker, theParasite, alteredState };

// Surprise: small, bounded, seeded variation. lo/hi are hard limits for the result (every table value sits inside them).
// Not varied on purpose: switches, echo division/time (musical timing is part of the identity), compressor, saturation, makeup (level).
constexpr SurpriseRule rules[] = {
    { "eq_b1_freq",  250.0f,  200.0f, 3500.0f, eq },
    { "eq_b1_gain",    1.5f,   -6.0f,    7.0f, eq },
    { "eq_b2_freq",  600.0f, 2000.0f, 9000.0f, eq },
    { "eq_b2_gain",    1.5f,   -5.0f,    5.0f, eq },
    { "eq_hi_gain",    1.5f,   -8.0f,    6.0f, eq },
    { "echo_fb",       8.0f,   15.0f,   70.0f, echo },
    { "echo_tone",   800.0f,  800.0f, 8000.0f, echo },
    { "echo_width",   20.0f,    0.0f,  100.0f, echo },
    { "echo_mix",      5.0f,    4.0f,   45.0f, echo },
    { "dim_width",    20.0f,   20.0f,  100.0f, dim },
    { "dim_detune",    3.0f,    2.0f,   20.0f, dim },
    { "dim_depth",    15.0f,   20.0f,  100.0f, dim },
    { "dim_mix",      10.0f,   40.0f,  100.0f, dim } };

// xorshift32: tiny, integer-only, identical on every platform.
std::uint32_t step (std::uint32_t x)
{
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return x == 0 ? 0x9e3779b9u : x;
}

float unit (std::uint32_t& state) // 0..1
{
    state = step (state);
    return (float) (state >> 8) / 16777216.0f;
}

float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

int find (const Value* v, int n, const char* id)
{
    for (int i = 0; i < n; ++i)
        if (std::strcmp (v[i].id, id) == 0)
            return i;
    return -1;
}
} // namespace

namespace AlterEgos
{
const Info& info (int index) { return infos[index < 0 ? 0 : (index >= count ? count - 1 : index)]; }

int indexOfId (const char* id)
{
    for (int i = 0; i < count; ++i)
        if (std::strcmp (infos[i].id, id) == 0)
            return i;
    return -1;
}

bool isAvailable (int index, unsigned implemented)
{
    return index >= 0 && index < count && (infos[index].required & ~implemented) == 0;
}

int compute (int index, float chaos, std::uint32_t seed, Value* out)
{
    index = index < 0 ? 0 : (index >= count ? count - 1 : index);
    const float c = clampf (chaos, 0.0f, 1.0f);
    for (int i = 0; i < numOwned; ++i)
        out[i] = { tables[index][i].id, tables[index][i].a + c * (tables[index][i].b - tables[index][i].a) };

    applySurprise (out, numOwned, seed, (std::uint32_t) index);
    return numOwned;
}

void applySurprise (Value* out, int n, std::uint32_t seed, std::uint32_t salt)
{
    if (seed == 0)
        return;
    std::uint32_t state = seed * 2654435761u + salt * 40503u + 1u;
    for (const auto& rule : rules)
    {
        const float r = unit (state) * 2.0f - 1.0f; // always drawn, so one rule being skipped never shifts the others
        const int echoOn = find (out, n, "echo_on"), dimOn = find (out, n, "dim_on");
        const bool active = rule.needsEffect == eq
                            || (rule.needsEffect == echo && echoOn >= 0 && out[echoOn].value > 0.5f)
                            || (rule.needsEffect == dim && dimOn >= 0 && out[dimOn].value > 0.5f);
        const int i = find (out, n, rule.id);
        if (active && i >= 0)
            out[i].value = clampf (out[i].value + r * rule.jitter, rule.lo, rule.hi);
    }
}

const SurpriseRule* surpriseRules (int& n)
{
    n = (int) (sizeof (rules) / sizeof (rules[0]));
    return rules;
}

std::uint32_t nextSeed (std::uint32_t previous) { return step (previous == 0 ? 0x1234567u : previous); }

int applyValues (const Value* values, int n, const std::function<bool (const char*, float)>& setter)
{
    int applied = 0;
    for (int i = 0; i < n; ++i)
        if (setter (values[i].id, values[i].value))
            ++applied;
    return applied;
}
}
