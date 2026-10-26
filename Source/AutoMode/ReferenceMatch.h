#pragma once
#include "MixEngine.h"
#include "../Analysis/ReferenceAnalyzer.h"
#include <array>
#include <functional>

namespace glitchkid::automode
{
// REFERENCE MATCH ("Reference-Inspired EQ"): a PURE function (no hidden state, no randomness, no I/O) that compares the tonal shape of a reference with the
// tonal shape of the user's own vocal AS IT SOUNDS WITH THE CURRENT EQ, and proposes small, bounded changes to the gains of the existing EQ bands. Nothing else.
//
// What it does not do: it does not isolate or recover a vocal from a finished mix (the reference tone includes instruments and mastering), it does not match
// dynamics, stereo width, ambience or loudness, and it never touches Gain, Mix, Pitch Tune or any parameter other than eq_b1_gain, eq_b2_gain and eq_hi_gain.
// The low shelf is left alone because the analysis starts at 200 Hz (a master's bass and drums dominate below that).
//
// Bounds: a single move is at most 3 dB; a band's total gain stays within +-6 dB (the presence bell at most +4 dB, as in BEAT LOCK); a band already beyond a
// limit is left alone rather than pulled back; differences under 0.5 dB are ignored; and only half of the measured difference is ever requested (a full mix is not a
// vocal), scaled by the intensity. The confidence is never "high" for a full-mix reference.
enum class Confidence { low, medium, high };

struct MatchInput
{
    analysis::SpectralProfile reference;
    analysis::SpectralProfile vocal;       // the user's dry vocal, measured the same way (mono sum)
    SoundState current;                    // the live sound (its eq_* values are the starting point)
    float intensity = 0.6f;                // 0..1
    std::function<float (float hz)> currentEqDb; // the real response of the CURRENT EQ settings at a frequency (null = flat); lets the engine compare against the vocal as it sounds now
};

struct MatchProposal
{
    enum class Status { ok, referenceMissing, vocalMissing, incompatible, nothingToChange };
    Status status = Status::referenceMissing;
    Confidence confidence = Confidence::low;
    juce::String message;                  // one line for the status display
    SoundState sound;                      // the whole sound to apply (== current unless status is ok)
    std::vector<MixChange> changes;        // only the parameters that differ, each with a plain-English reason
    juce::StringArray confidenceReasons;   // why the confidence is not higher
    juce::StringArray evidence;            // the measurements that were used
    juce::StringArray limitations;         // what this does NOT do (always stated)
    std::array<float, analysis::SpectralProfile::numBands> differenceDb {}; // reference minus the vocal as it sounds now, per band (real numbers, for display)
    int bandsCompared = 0;
};

MatchProposal proposeReferenceInspiredEq (const MatchInput&, const RangeLookup&);

// Applies the ticked changes only (by parameter id) to `current`, inside the ranges, and returns the new sound; the editor then uses the normal applyParameterChanges.
SoundState applyMatchChanges (const MatchProposal&, const std::vector<bool>& ticked, const SoundState& current, const RangeLookup&, std::vector<MixChange>* applied = nullptr);
}
