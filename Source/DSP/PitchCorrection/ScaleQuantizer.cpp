#include "ScaleQuantizer.h"
#include <cmath>

namespace glitchkid::pitch
{
namespace
{
// Bit n set = n semitones above the key's root is allowed.
constexpr unsigned masks[] = {
    0b111111111111, // chromatic
    0b101010110101, // major:            0 2 4 5 7 9 11
    0b010110101101, // natural minor:    0 2 3 5 7 8 10
    0b100110101101, // harmonic minor:   0 2 3 5 7 8 11
    0b001010010101, // major pentatonic: 0 2 4 7 9
    0b010010101001, // minor pentatonic: 0 3 5 7 10
};
}

const char* ScaleQuantizer::name (Scale s)
{
    static const char* names[] = { "Chromatic", "Major", "Natural minor", "Harmonic minor", "Major pentatonic", "Minor pentatonic" };
    return names[(int) s];
}

bool ScaleQuantizer::isInScale (int midiNote, int key, Scale scale)
{
    const int degree = ((midiNote - key) % 12 + 12) % 12;
    return (masks[(int) scale] >> degree) & 1u;
}

float ScaleQuantizer::nearestNote (float midi, int key, Scale scale)
{
    if (! std::isfinite (midi))
        return midi;

    const int base = (int) std::floor (midi);
    float best = (float) base;
    float bestDist = 1.0e9f;
    for (int n = base - 3; n <= base + 4; ++n) // the widest gap in any scale here is 3 semitones
        if (isInScale (n, key, scale))
        {
            const float d = std::abs ((float) n - midi);
            if (d < bestDist)
            {
                bestDist = d;
                best = (float) n;
            }
        }
    return best;
}

float ScaleQuantizer::select (float midi, int key, Scale scale, float previousTarget, float hysteresis)
{
    const float nearest = nearestNote (midi, key, scale);
    if (previousTarget >= 0.0f && std::isfinite (midi) && std::abs (previousTarget - nearest) > 0.5f
        && isInScale ((int) std::lround (previousTarget), key, scale)
        && std::abs (midi - previousTarget) <= std::abs (midi - nearest) + hysteresis)
        return previousTarget;
    return nearest;
}
} // namespace glitchkid::pitch
