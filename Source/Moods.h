#pragma once
#include <functional>

// MOODS: built-in, one-tap vocal starting points. A Mood is only data: a list of (parameter id, value) pairs that
// are applied through the normal parameter system (so the controls move, the host sees automation, and all the
// existing smoothing in the DSP applies). It is not a separate effect engine.
//
// A Mood fully defines the effect groups it owns: EQ (all 4 bands), compressor, saturation drive, Liquid Echo and
// Dimension. Applying any Mood therefore gives the same result whatever was set before; nothing leaks from a
// previous Mood. A Mood never touches Output Gain, Bypass, the main Mix or Glitch Stutter.
//
// Moods do not change the output level on purpose, but saturation and compression do change loudness: the
// measured level change of each Mood is documented in docs/TESTING.md. Watch the OUT meter and trim Output Gain.
namespace Moods
{
struct Value { const char* id; float value; }; // value in the parameter's own units (dB, Hz, %, ms, choice index, 0/1)

struct Mood
{
    const char* id;          // stable, never change: used by tests and any future preset/state code
    const char* name;        // shown in the window
    const char* description; // one short line
    const Value* values;
    int numValues;
};

constexpr int count = 5;

const Mood& get (int index);       // 0..count-1
int indexOfId (const char* id);    // -1 if unknown

// Applies every value through `setter(id, value)`, which returns false when that parameter does not exist (the value
// is then skipped, never an error). Returns how many values were applied. Message thread only.
int apply (const Mood&, const std::function<bool (const char* id, float value)>& setter);
}
