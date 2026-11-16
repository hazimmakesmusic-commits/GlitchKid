#pragma once
#include "ParametricEq.h"

// EQ LAB parameter IDs. They are stored in saved sessions, presets and host automation: never rename or reuse them.
// Bands 1-4 keep their original IDs and ranges (eq_lo / eq_b1 / eq_b2 / eq_hi: Build My Mix, Beat Lock, macros, KID, presets and saved automation
// use them); their _q (shelves), _on, _type, _slope and _stereo, and all of bands 5-16, were appended with EQ LAB.
namespace glitchkid
{
struct EqIds { const char* freq; const char* gain; const char* q; const char* on; const char* type; const char* slope; const char* stereo;
               const char* dyn; const char* thresh; const char* range; const char* attack; const char* release; const char* dynMode; }; // dynamics: appended later
inline constexpr int legacyEqBands = 4;
inline constexpr EqIds eqIds[ParametricEq::numBands] = {
    { "eq_lo_freq", "eq_lo_gain", "eq_lo_q", "eq_lo_on", "eq_lo_type", "eq_lo_slope", "eq_lo_stereo",
      "eq_lo_dyn", "eq_lo_thresh", "eq_lo_range", "eq_lo_attack", "eq_lo_release", "eq_lo_dynmode" },
    { "eq_b1_freq", "eq_b1_gain", "eq_b1_q", "eq_b1_on", "eq_b1_type", "eq_b1_slope", "eq_b1_stereo",
      "eq_b1_dyn", "eq_b1_thresh", "eq_b1_range", "eq_b1_attack", "eq_b1_release", "eq_b1_dynmode" },
    { "eq_b2_freq", "eq_b2_gain", "eq_b2_q", "eq_b2_on", "eq_b2_type", "eq_b2_slope", "eq_b2_stereo",
      "eq_b2_dyn", "eq_b2_thresh", "eq_b2_range", "eq_b2_attack", "eq_b2_release", "eq_b2_dynmode" },
    { "eq_hi_freq", "eq_hi_gain", "eq_hi_q", "eq_hi_on", "eq_hi_type", "eq_hi_slope", "eq_hi_stereo",
      "eq_hi_dyn", "eq_hi_thresh", "eq_hi_range", "eq_hi_attack", "eq_hi_release", "eq_hi_dynmode" },
    { "eq_band5_freq", "eq_band5_gain", "eq_band5_q", "eq_band5_on", "eq_band5_type", "eq_band5_slope", "eq_band5_stereo",
      "eq_band5_dyn", "eq_band5_thresh", "eq_band5_range", "eq_band5_attack", "eq_band5_release", "eq_band5_dynmode" },
    { "eq_band6_freq", "eq_band6_gain", "eq_band6_q", "eq_band6_on", "eq_band6_type", "eq_band6_slope", "eq_band6_stereo",
      "eq_band6_dyn", "eq_band6_thresh", "eq_band6_range", "eq_band6_attack", "eq_band6_release", "eq_band6_dynmode" },
    { "eq_band7_freq", "eq_band7_gain", "eq_band7_q", "eq_band7_on", "eq_band7_type", "eq_band7_slope", "eq_band7_stereo",
      "eq_band7_dyn", "eq_band7_thresh", "eq_band7_range", "eq_band7_attack", "eq_band7_release", "eq_band7_dynmode" },
    { "eq_band8_freq", "eq_band8_gain", "eq_band8_q", "eq_band8_on", "eq_band8_type", "eq_band8_slope", "eq_band8_stereo",
      "eq_band8_dyn", "eq_band8_thresh", "eq_band8_range", "eq_band8_attack", "eq_band8_release", "eq_band8_dynmode" },
    { "eq_band9_freq", "eq_band9_gain", "eq_band9_q", "eq_band9_on", "eq_band9_type", "eq_band9_slope", "eq_band9_stereo",
      "eq_band9_dyn", "eq_band9_thresh", "eq_band9_range", "eq_band9_attack", "eq_band9_release", "eq_band9_dynmode" },
    { "eq_band10_freq", "eq_band10_gain", "eq_band10_q", "eq_band10_on", "eq_band10_type", "eq_band10_slope", "eq_band10_stereo",
      "eq_band10_dyn", "eq_band10_thresh", "eq_band10_range", "eq_band10_attack", "eq_band10_release", "eq_band10_dynmode" },
    { "eq_band11_freq", "eq_band11_gain", "eq_band11_q", "eq_band11_on", "eq_band11_type", "eq_band11_slope", "eq_band11_stereo",
      "eq_band11_dyn", "eq_band11_thresh", "eq_band11_range", "eq_band11_attack", "eq_band11_release", "eq_band11_dynmode" },
    { "eq_band12_freq", "eq_band12_gain", "eq_band12_q", "eq_band12_on", "eq_band12_type", "eq_band12_slope", "eq_band12_stereo",
      "eq_band12_dyn", "eq_band12_thresh", "eq_band12_range", "eq_band12_attack", "eq_band12_release", "eq_band12_dynmode" },
    { "eq_band13_freq", "eq_band13_gain", "eq_band13_q", "eq_band13_on", "eq_band13_type", "eq_band13_slope", "eq_band13_stereo",
      "eq_band13_dyn", "eq_band13_thresh", "eq_band13_range", "eq_band13_attack", "eq_band13_release", "eq_band13_dynmode" },
    { "eq_band14_freq", "eq_band14_gain", "eq_band14_q", "eq_band14_on", "eq_band14_type", "eq_band14_slope", "eq_band14_stereo",
      "eq_band14_dyn", "eq_band14_thresh", "eq_band14_range", "eq_band14_attack", "eq_band14_release", "eq_band14_dynmode" },
    { "eq_band15_freq", "eq_band15_gain", "eq_band15_q", "eq_band15_on", "eq_band15_type", "eq_band15_slope", "eq_band15_stereo",
      "eq_band15_dyn", "eq_band15_thresh", "eq_band15_range", "eq_band15_attack", "eq_band15_release", "eq_band15_dynmode" },
    { "eq_band16_freq", "eq_band16_gain", "eq_band16_q", "eq_band16_on", "eq_band16_type", "eq_band16_slope", "eq_band16_stereo",
      "eq_band16_dyn", "eq_band16_thresh", "eq_band16_range", "eq_band16_attack", "eq_band16_release", "eq_band16_dynmode" } };

// A band's settings from any store of parameter values (`get (id, fallback)`), with the parameters' own defaults for values that are missing
// (an older session or preset): the original four bands stay low shelf / bell / bell / high shelf, all on; bands 5-16 stay off.
template <typename Get>
EqBandSettings eqBandFrom (int b, Get&& get)
{
    static constexpr float freqs[ParametricEq::numBands] = { 100, 400, 3000, 8000, 60, 120, 200, 300, 500, 800, 1200, 2000, 3000, 5000, 8000, 12000 };
    const bool legacy = b < legacyEqBands;
    const auto& id = eqIds[b];
    EqBandSettings s;
    s.freqHz = get (id.freq, freqs[b]);
    s.gainDb = get (id.gain, 0.0f);
    s.q = get (id.q, b == 0 || b == 3 ? 0.7071f : 1.0f);
    s.on = get (id.on, legacy ? 1.0f : 0.0f) >= 0.5f;
    const int legacyType = b == 0 ? (int) EqType::lowShelf : b == 3 ? (int) EqType::highShelf : (int) EqType::bell;
    s.type = (EqType) juce::jlimit (0, 7, (int) std::lround (get (id.type, (float) legacyType)));
    s.slopeDb = ParametricEq::slopes[juce::jlimit (0, ParametricEq::numSlopes - 1, (int) std::lround (get (id.slope, 1.0f)))];
    s.placement = (EqPlacement) juce::jlimit (0, 4, (int) std::lround (get (id.stereo, 0.0f)));
    s.dynamic = get (id.dyn, 0.0f) >= 0.5f;
    s.threshDb = get (id.thresh, -30.0f);
    s.rangeDb = get (id.range, -6.0f);
    s.attackMs = get (id.attack, 5.0f);
    s.releaseMs = get (id.release, 120.0f);
    s.below = get (id.dynMode, 0.0f) >= 0.5f;
    return s;
}
}
