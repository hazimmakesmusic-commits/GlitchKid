#include "MacroEngine.h"
#include <cmath>

namespace glitchkid::macros
{
namespace
{
float clamp01 (float v) { return std::isfinite (v) ? (v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v)) : 0.0f; }
float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
float lerp (float a, float b, float t) { return a + (b - a) * t; }
}

const char* macroName (Macro m)
{
    switch (m) { case Macro::space: return "SPACE"; case Macro::character: return "CHARACTER"; case Macro::punch: return "PUNCH"; }
    return "";
}

const char* macroDescription (Macro m)
{
    switch (m)
    {
        case Macro::space:     return "More atmosphere: raises the echo, Dimension and Doubler together. (GlitchKid has no reverb: this is delay and width.)";
        case Macro::character: return "More grit: raises saturation and adds a little presence.";
        case Macro::punch:     return "More control: raises the compressor ratio and lowers its threshold, with matching makeup gain.";
    }
    return "";
}

float read (Macro m, const SoundState& s)
{
    switch (m)
    {
        case Macro::space:     return s.get ("echo_on", 0.0f) >= 0.5f ? clamp01 (s.get ("echo_mix", 0.0f) / 60.0f) : 0.0f;
        case Macro::character: return clamp01 (s.get ("sat_drive", 0.0f) / 12.0f);
        case Macro::punch:     return clamp01 ((s.get ("comp_ratio", 1.0f) - 1.0f) / 5.0f);
    }
    return 0.0f;
}

void apply (Macro m, float value, SoundState& s)
{
    const float v = clamp01 (value);
    switch (m)
    {
        case Macro::space:
        {
            const float on = v >= 0.02f ? 1.0f : 0.0f;
            s.set ("echo_on", on);
            s.set ("dim_on", on);
            s.set ("dbl_on", on);
            s.set ("echo_mix", 60.0f * v);
            s.set ("echo_fb", lerp (15.0f, 45.0f, v));
            s.set ("echo_width", lerp (20.0f, 75.0f, v));
            s.set ("dim_mix", 45.0f * v);
            s.set ("dbl_amount", 40.0f * v);
            break;
        }
        case Macro::character:
            s.set ("sat_drive", 12.0f * v);
            s.set ("eq_b2_gain", 2.5f * v);
            break;
        case Macro::punch:
        {
            const float ratio = 1.0f + 5.0f * v;
            s.set ("comp_ratio", ratio);
            s.set ("comp_thresh", lerp (-12.0f, -26.0f, v));
            s.set ("comp_attack", 15.0f);
            s.set ("comp_release", 110.0f);
            s.set ("comp_knee", 6.0f);
            s.set ("comp_makeup", std::fmin (4.0f, 0.8f * (ratio - 1.0f)));
            break;
        }
    }
}

const char* commandLabel (Command c)
{
    switch (c) { case Command::darker: return "DARKER"; case Command::moreSpace: return "MORE SPACE"; case Command::harderTune: return "HARDER TUNE"; case Command::weirder: return "WEIRDER"; }
    return "";
}

const char* commandDescription (Command c)
{
    switch (c)
    {
        case Command::darker:     return "Darker: top shelf -1.5 dB, presence -0.5 dB and a duller echo. Press again for more.";
        case Command::moreSpace:  return "More space: the SPACE macro +15 points (echo, Dimension, Doubler).";
        case Command::harderTune: return "Harder tune: Pitch Tune on, strength +10 %, speed 8 ms faster. Adds about 31 ms of reported latency while Pitch Tune is on.";
        case Command::weirder:    return "Weirder: Vocal Mutation on, +15 % amount and +10 % character.";
    }
    return "";
}

void apply (Command c, SoundState& s)
{
    switch (c)
    {
        case Command::darker:
            s.set ("eq_hi_gain", clampf (s.get ("eq_hi_gain", 0.0f) - 1.5f, -15.0f, 15.0f));
            s.set ("eq_b2_gain", clampf (s.get ("eq_b2_gain", 0.0f) - 0.5f, -15.0f, 15.0f));
            s.set ("echo_tone", clampf (s.get ("echo_tone", 6000.0f) * 0.85f, 400.0f, 20000.0f));
            break;
        case Command::moreSpace:
            apply (Macro::space, read (Macro::space, s) + 0.15f, s);
            break;
        case Command::harderTune:
            s.set ("tune_on", 1.0f);
            s.set ("tune_strength", clampf (s.get ("tune_strength", 100.0f) + 10.0f, 0.0f, 100.0f));
            s.set ("tune_speed", clampf (s.get ("tune_speed", 20.0f) - 8.0f, 0.0f, 400.0f));
            break;
        case Command::weirder:
            s.set ("mut_on", 1.0f);
            s.set ("mut_amount", clampf (s.get ("mut_amount", 40.0f) + 15.0f, 0.0f, 100.0f));
            s.set ("mut_character", clampf (s.get ("mut_character", 50.0f) + 10.0f, 0.0f, 100.0f));
            break;
    }
}
}
