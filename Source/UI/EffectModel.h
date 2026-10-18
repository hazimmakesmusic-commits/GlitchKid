#pragma once
#include "../Presets/SoundState.h"

namespace glitchkid::fx
{
// The effects of GlitchKid, in their ACTUAL processing order (see the signal-path comment in PluginProcessor.h). The order is fixed by the engine: nothing here
// reorders, adds or removes an effect, and the interface must not pretend otherwise. Most effects have a real on/off parameter; four are always in the chain and
// are simply neutral at their defaults (Formant at 0 st, EQ with flat bands, Compressor at 1:1, Saturation at 0 dB drive).
enum Effect { pitchTune = 0, formant, harmony, mutation, doubler, eq, compressor, saturation, stutter, echo, dimension, numEffects };

struct Info
{
    const char* name;        // shown on tiles and in the chain
    const char* description; // one plain sentence (tooltip / panel header)
    const char* enableId;    // the real on/off parameter, or nullptr when the effect is always in the chain
};

const Info& info (int effect);
bool canToggle (int effect);                           // true if the effect has a real on/off parameter
bool isActive (int effect, const SoundState&);         // on (toggle effects), or away from its neutral setting (always-in-chain effects)
void setEnabled (int effect, bool on, SoundState&);    // only for effects where canToggle() is true; otherwise does nothing
}
