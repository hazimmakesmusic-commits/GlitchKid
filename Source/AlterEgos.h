#pragma once
#include <cstdint>
#include <functional>

// ALTER EGOS + MOOD ENGINE (data and maths only; no DSP lives here).
//
// An Alter Ego is a curated vocal identity: a table of parameter values for the effects that already exist
// (EQ, compressor, saturation, Liquid Echo, Dimension). Applying one sets those parameters through the normal parameter
// system (message thread), so the knobs move, the host sees automation, and each effect's own smoothing applies.
//
// CHAOS (0..1) intensifies the Alter Ego: every continuous parameter moves linearly from its value at Chaos 0 to its own
// value at Chaos 1 (both stored in the table, each inside safe limits). Switches (echo_on, echo_sync, echo_div, dim_on) never
// move, so Chaos cannot make an effect appear or an echo time jump.
//
// SURPRISE (a seed, 0 = none) adds a small, seeded, bounded variation to a short allowlist of safe parameters on top of the
// Chaos result. The same (Alter Ego, Chaos, seed) always gives the same values on every platform.
//
// BLEND is the plug-in's existing main Mix parameter ("mix"); Alter Egos never touch it.
// Alter Egos never touch Output Gain, Bypass, Mix or Glitch Stutter.
namespace AlterEgos
{
enum Effect : unsigned { eq = 1, comp = 2, sat = 4, echo = 8, dim = 16, stutter = 32 };

// The DSP that exists in this build. An Alter Ego is only available when every effect it requires is in this set.
constexpr unsigned implementedEffects = eq | comp | sat | echo | dim | stutter;

struct Value { const char* id; float value; }; // value in the parameter's own units (dB, Hz, %, ms, choice index, 0/1)

struct Info
{
    const char* id;          // stable, never change
    const char* name;        // shown in the window
    const char* description; // one short line
    unsigned required;       // effects whose absence makes this Alter Ego unavailable
    unsigned optional;       // effects that add to it; when absent a reduced version is used (those parameters are skipped)
};

constexpr int count = 5;
constexpr int numOwned = 30; // every Alter Ego defines exactly these parameters

const Info& info (int index);                // clamped to 0..count-1
int indexOfId (const char* id);              // -1 if unknown
bool isAvailable (int index, unsigned implemented = implementedEffects);

// Fills out[0..numOwned-1] with the parameter values for this Alter Ego at `chaos` (clamped to 0..1) with variation `seed`
// (0 = no variation). Returns numOwned.
int compute (int index, float chaos, std::uint32_t seed, Value* out);

// The Surprise allowlist: v' = clamp (v + random(-jitter, +jitter), lo, hi), applied only when the Alter Ego has that effect on.
struct SurpriseRule { const char* id; float jitter; float lo; float hi; unsigned needsEffect; };
const SurpriseRule* surpriseRules (int& count);
// Applies the Surprise variation to `values[0..n-1]` in place (seed 0 does nothing). `salt` only decorrelates the Alter Egos.
void applySurprise (Value* values, int n, std::uint32_t seed, std::uint32_t salt);
std::uint32_t nextSeed (std::uint32_t previous); // deterministic non-zero sequence, for the "another variation" button

// Applies every value through `setter(id, value)`, which returns false when that parameter does not exist (the value is then
// skipped, never an error). Returns how many values were applied.
int applyValues (const Value* values, int n, const std::function<bool (const char* id, float value)>& setter);
}
