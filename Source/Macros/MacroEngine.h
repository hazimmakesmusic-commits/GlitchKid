#pragma once
#include "../Presets/SoundState.h"

namespace glitchkid::macros
{
// The big home-screen controls and the quick commands, as PURE functions on a SoundState (no hidden state, no randomness, no I/O).
// They only ever write existing parameters, always inside the parameter's real range, and they never touch latency-bearing effects
// (Pitch Tune is only switched by HARDER TUNE, which says so), Gain, Mix, Bypass, key, scale or the Alter Ego.
//
// A macro is a *gesture*: while it is dragged it writes the parameters listed below. Its position is read BACK from one primary parameter, so the
// dial always agrees with the detailed controls (change the knobs by hand and the dial follows).
enum class Macro { space, character, punch };
constexpr int numMacros = 3;

const char* macroName (Macro);        // "SPACE", "CHARACTER", "PUNCH"
const char* macroDescription (Macro); // plain English: what it moves

// SPACE      echo_on, dim_on, dbl_on (on above 2 %), echo_mix 0..60 %, echo_fb 15..45 %, echo_width 20..75 %, dim_mix 0..45 %, dbl_amount 0..40 %.
//            Primary (read back): echo_mix / 60 while Echo is on. There is no reverb in GlitchKid: this is delay + width.
// CHARACTER  sat_drive 0..12 dB, eq_b2_gain 0..+2.5 dB (a little presence). Primary: sat_drive / 12.
// PUNCH      comp_ratio 1..6, comp_thresh -12..-26 dB, comp_attack 15 ms, comp_release 110 ms, comp_knee 6 dB, comp_makeup min(4, 0.8 (ratio-1)) dB.
//            Primary: (comp_ratio - 1) / 5.
float read (Macro, const SoundState&);               // 0..1
void apply (Macro, float value01, SoundState&);      // value is clamped to 0..1; non-finite is treated as 0

// Quick commands: fixed, bounded nudges. They are NOT language understanding: each is one button with one documented effect.
enum class Command { darker, moreSpace, harderTune, weirder };
constexpr int numCommands = 4;

const char* commandLabel (Command);       // "DARKER", "MORE SPACE", "HARDER TUNE", "WEIRDER"
const char* commandDescription (Command); // plain English, shown as the tooltip
void apply (Command, SoundState&);
// DARKER      eq_hi_gain -1.5 dB, eq_b2_gain -0.5 dB, echo_tone x0.85 (limits: -15 dB, 400 Hz)
// MORE SPACE  SPACE macro +15 points
// HARDER TUNE tune_on, tune_strength +10 % (max 100), tune_speed -8 ms (min 0). Switching Pitch Tune on adds ~31 ms of reported latency.
// WEIRDER     mut_on, mut_amount +15 %, mut_character +10 % (max 100): more Vocal Mutation
}
