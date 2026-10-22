#pragma once
#include "../Analysis/VocalAnalyzer.h"
#include <string>
#include <vector>

namespace glitchkid::automode
{
// How the lead vocal sits against the instrumental, from two real analyses (the lead = this track's input, the instrumental = the optional sidechain input).
// These are ESTIMATES from five coarse energy bands (not calibrated spectra, not LUFS). They say where the beat probably competes with the voice;
// they do not judge the mix, and they say nothing about emotion, genre or quality. Pure and deterministic: no audio, no allocation guarantees (message thread).
struct Relationship
{
    bool valid = false;             // both analyses were ready; otherwise nothing below means anything
    float confidence = 0.0f;        // 0..1: how much active audio both analyses heard (1 = the full analysis length)
    float vocalMinusBeatDb = 0.0f;  // lead RMS minus instrumental RMS (digital level, active frames)
    std::array<float, analysis::VocalAnalysis::numBands> maskRisk {}; // 0..1 per band: how likely the beat covers the voice there (band 0 and 4 are not rated: 0)
    int worstBand = -1;             // band with the highest risk, or -1 if no band is at risk
    bool beatLowEndHeavy = false;   // more than half of the beat's energy is below 120 Hz
    bool beatVeryLoud = false;      // the beat peaks within 1 dB of full scale
    std::vector<std::string> reasons; // plain-English findings, in order of importance
    std::string unavailable;        // when !valid: why, in plain words
};

// What the relationship suggests changing, as plain moves (shared by BUILD MY VOCAL MIX and BEAT LOCK, so the two never disagree): an EQ gain delta, or a
// factor for an ambience amount. category: PRESENCE, CLARITY or SPACE. Pure and deterministic; the caller clamps to real ranges.
struct BeatMove
{
    std::string id, category, reason;
    float delta = 0.0f;      // added to the current value (EQ gains, dB)
    bool scale = false;      // true: multiply the current value by `factor` instead (echo and width amounts)
    float factor = 1.0f;
};

const char* bandName (int band);
std::vector<BeatMove> beatMoves (const Relationship&);
Relationship compare (const analysis::VocalAnalysis& lead, const analysis::VocalAnalysis& beat);
}
