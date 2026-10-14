#pragma once

// Pitch-correction module (Stage 4.1 prototype, productised in Stage 4.2).
// Picks the note a sung pitch should be pulled to, for a chosen key and scale.
// Pure functions on MIDI note numbers (A4 = 69); no state, safe on any thread.
namespace glitchkid::pitch
{
class ScaleQuantizer
{
public:
    enum class Scale { chromatic, major, naturalMinor, harmonicMinor, majorPentatonic, minorPentatonic };
    static constexpr int numScales = 6;

    static const char* name (Scale s);

    // key: 0 = C ... 11 = B.
    static bool isInScale (int midiNote, int key, Scale scale);

    // Nearest allowed note (as a MIDI number) to a fractional MIDI pitch. Non-finite input is returned unchanged.
    static float nearestNote (float midi, int key, Scale scale);

    // Like nearestNote, but keeps `previousTarget` (pass a negative value for "none") until the sung pitch
    // is clearly closer to another note. Stops the target flickering when a singer sits between two notes.
    static float select (float midi, int key, Scale scale, float previousTarget, float hysteresisSemitones = 0.15f);
};
} // namespace glitchkid::pitch
