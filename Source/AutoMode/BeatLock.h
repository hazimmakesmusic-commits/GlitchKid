#pragma once
#include "MixEngine.h"

namespace glitchkid::automode
{
// BEAT LOCK: how the lead vocal sits against the instrumental, and a few conservative, optional adjustments to the vocal's OWN controls.
// A PURE function of two real analyses (the lead = this track's input, the instrumental = the plug-in's optional second input bus) and the live sound.
//
// What it is NOT: it does not hear a beat that was not routed to the instrumental input (a stereo input that already contains vocal and beat mixed together is
// measured as "the vocal"), it does not process the instrumental, it does not move your mixer faders (BALANCE is advice only), and it has no reverb to recommend
// (SPACE moves only the existing echo and width amounts). The numbers are digital levels from five coarse bands, not LUFS and not calibrated spectra.
// It shares its rules (automode::beatMoves) with BUILD MY VOCAL MIX, so the two never disagree.
struct BeatLockItem
{
    BeatMove move;           // the plain move (id empty for advice that changes nothing)
    juce::String title;      // e.g. "PRESENCE BELL (3 kHz)"
    bool advisory = false;   // true: advice about your mixer, no parameter to apply
};

struct BeatLockResult
{
    enum class Status { noBeatInput, beatSilent, notAnalysed, beatNotAnalysed, ready };
    Status status = Status::notAnalysed;
    bool valid = false;                 // true only when both analyses are ready: nothing below means anything otherwise
    juce::String message;               // one honest line for the status display
    float confidence = 0.0f;            // 0..1: how much active audio both analyses heard
    float vocalMinusBeatDb = 0.0f;
    float vocalRmsDb = 0.0f, beatRmsDb = 0.0f, vocalPeakDb = 0.0f, beatPeakDb = 0.0f; // digital levels (not LUFS)
    std::array<float, analysis::VocalAnalysis::numBands> vocalBands {}, beatBands {}, maskRisk {}; // energy shares (0..1) and the voice-covered estimate per band
    int worstBand = -1;
    std::vector<BeatLockItem> items;    // BALANCE first (advice), then the moves
    juce::StringArray facts;            // the measurements, in words
    juce::StringArray unavailable;      // what is not known or not available (stated, not guessed)
};

BeatLockResult analyseBeatLock (const analysis::VocalAnalysis& lead, const analysis::VocalAnalysis& beat, InstrumentalInput input);

// The live sound with the selected items applied (selected[i] belongs to result.items[i]; advisory items do nothing). Every value stays inside its range;
// `changes` (optional) lists what really moved, with the reasons. Nothing outside the items' parameters is touched.
SoundState applyBeatLock (const BeatLockResult&, const std::vector<bool>& selected, const SoundState& current, const RangeLookup&, std::vector<MixChange>* changes = nullptr);
}
