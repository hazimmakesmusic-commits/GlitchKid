#include "EffectModel.h"
#include <cmath>

namespace glitchkid::fx
{
namespace
{
const Info table[numEffects] = {
    { "PITCH TUNE",  "Pulls your voice to the notes of the key and scale you choose. Adds about 31 ms of latency while on.", "tune_on" },
    { "FORMANT",     "Shifts the vocal character (the body of the voice) without changing the pitch. Adds about 21 ms of latency while away from 0.", nullptr },
    { "HARMONY",     "Adds ONE extra voice at a fixed interval above or below. Not key-aware. Adds about 31 ms of latency while on.", "harm_on" },
    { "MUTATION",    "Bends the voice into robotic, hollow or alien shifts. No added latency.", "mut_on" },
    { "DOUBLER",     "A synthetic double: two drifting, delayed, softened copies for width. No added latency.", "dbl_on" },
    { "EQ",          "Four-band equaliser: low shelf, two bells and a high shelf. Always in the chain, flat by default.", nullptr },
    { "COMPRESSOR",  "Evens out the level. Always in the chain, at 1:1 (no effect) by default.", nullptr },
    { "SATURATION",  "Adds warmth and grit. Always in the chain, at 0 dB drive (no effect) by default.", nullptr },
    { "STUTTER",     "Glitchy, tempo-synced repeats of the vocal.", "stutter_on" },
    { "ECHO",        "Liquid Echo: a tempo-synced or free-time delay with feedback, tone and stereo width.", "echo_on" },
    { "DIMENSION",   "A short chorus-style widener for stereo space.", "dim_on" } };
}

const Info& info (int effect)
{
    return table[effect < 0 ? 0 : (effect >= numEffects ? numEffects - 1 : effect)];
}

bool canToggle (int effect)
{
    return effect >= 0 && effect < numEffects && table[effect].enableId != nullptr;
}

bool isActive (int effect, const SoundState& s)
{
    if (canToggle (effect))
        return s.get (table[effect].enableId, 0.0f) >= 0.5f;
    switch (effect)
    {
        case formant:    return std::fabs (s.get ("formant_shift", 0.0f)) >= 0.05f;
        case eq:         for (const char* id : { "eq_lo_gain", "eq_b1_gain", "eq_b2_gain", "eq_hi_gain" })
                             if (std::fabs (s.get (id, 0.0f)) > 0.05f)
                                 return true;
                         return false;
        case compressor: return s.get ("comp_ratio", 1.0f) > 1.05f;
        case saturation: return s.get ("sat_drive", 0.0f) > 0.05f;
        default:         return false;
    }
}

void setEnabled (int effect, bool on, SoundState& s)
{
    if (canToggle (effect))
        s.set (table[effect].enableId, on ? 1.0f : 0.0f);
}
}
