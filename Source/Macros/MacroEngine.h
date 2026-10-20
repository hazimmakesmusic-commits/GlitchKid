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
enum class Macro { space, punch, tone };
constexpr int numMacros = 3;

const char* macroName (Macro);        // "SPACE", "PUNCH", "TONE"
const char* macroDescription (Macro); // plain English: what it moves

// SPACE      echo_on, dim_on, dbl_on (on above 2 %), echo_mix 0..60 %, echo_fb 15..45 %, echo_width 20..75 %, dim_mix 0..45 %, dbl_amount 0..40 %.
//            Primary (read back): echo_mix / 60 while Echo is on. There is no reverb in GlitchKid: this is delay + width.
// PUNCH      comp_ratio 1..6, comp_thresh -12..-26 dB, comp_attack 15 ms, comp_release 110 ms, comp_knee 6 dB, comp_makeup min(4, 0.8 (ratio-1)) dB.
//            Primary: (comp_ratio - 1) / 5.
// TONE       eq_hi_gain -6..+6 dB (AIR), eq_b2_gain -1.5..+1.5 dB (PRESENCE); 0.5 = flat. Darker to brighter. Primary: (eq_hi_gain + 6) / 12.
float read (Macro, const SoundState&);               // 0..1
void apply (Macro, float value01, SoundState&);      // value is clamped to 0..1; non-finite is treated as 0

// Quick commands: fixed, bounded nudges. They are NOT language understanding: each is one button with one documented effect.
enum class Command { darker, moreSpace, harderTune };
constexpr int numCommands = 3;

const char* commandLabel (Command);       // "DARKER", "MORE SPACE", "HARDER TUNE"
const char* commandDescription (Command); // plain English, shown as the tooltip
void apply (Command, SoundState&);
// DARKER      eq_hi_gain -1.5 dB, eq_b2_gain -0.5 dB, echo_tone x0.85 (limits: -15 dB, 400 Hz)
// MORE SPACE  SPACE macro +15 points
// HARDER TUNE tune_on, tune_strength +10 % (max 100), tune_speed -8 ms (min 0). Switching Pitch Tune on adds ~31 ms of reported latency.

// Tuning styles for the TUNE page: three fixed settings of Pitch Tune's own STRENGTH and SPEED. They never touch key or scale (those are the user's).
//   NATURAL  tune_strength 50 %, tune_speed 80 ms   (gentle, slow glide)
//   LIGHT    tune_strength 75 %, tune_speed 35 ms   (firm, still smooth)
//   HARD     tune_strength 100 %, tune_speed 0 ms   (instant snap, the "hard tune" sound)
// Applying one switches Pitch Tune on (which adds ~31 ms of reported latency). The style is READ BACK from the two values (+-3 % and +-5 ms), so
// moving the knobs by hand correctly shows no style selected (-1).
enum class TuneStyle { natural, light, hard };
constexpr int numTuneStyles = 3;
const char* tuneStyleLabel (TuneStyle);
const char* tuneStyleDescription (TuneStyle);
void apply (TuneStyle, SoundState&);
int readTuneStyle (const SoundState&); // 0..2, or -1 when Pitch Tune is off or the values match no style
}
