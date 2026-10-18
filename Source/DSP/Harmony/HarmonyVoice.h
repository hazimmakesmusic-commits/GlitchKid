#pragma once
#include <vector>
#include "../PitchCorrection/PitchCorrector.h"

// Harmony voice (Stage 4.8): ONE extra voice that sings the lead's melody at a fixed interval, for mono or stereo.
//
// What it is: the Pitch Tune machinery (YIN pitch detector + PSOLA pitch shifters, `PitchCorrector`) run in its new fixed-interval mode:
// the voice is moved by exactly `semitones` whenever it is voiced. It is therefore a FIXED-INTERVAL harmony: it does not know the
// song's key or scale, so a third is always a major or always a minor third, whatever the melody does (two notes of a scale need different
// thirds). It follows ONE voice at a time (a solo vocal), not chords. PSOLA is a time-domain method: it sounds most natural up to a few
// semitones and gets "small" or "chipmunk" at an octave, and it adds a little grain/roughness. It is a creative effect, not studio-grade harmony.
//
// Output = the lead, delayed by getLatencySamples(), plus the harmony voice (delayed by the same amount) at `levelDb`. Delaying the lead
// by the shifter's latency keeps lead and harmony in time with each other; the plug-in delays the dry path the same way and reports the latency
// to the host (as for Pitch Tune), only while the harmony is on.
//
// Voiced gate: the harmony voice is faded out while the singer is not voiced (consonants, breaths, silence): PSOLA of noise would only buzz and a
// doubled "s" is lisping. The gate is delayed by the latency too, so it lines up with the voice. The harmony never feeds into its own input
// (the detector and shifters hear only the lead), so it cannot build up. Non-finite input counts as silence. No allocation after prepare().
namespace glitchkid::harmony
{
// The fixed intervals offered in the window. Direction is part of the name, so UP and DOWN are never ambiguous. "Third" and "fifth" are
// fixed numbers of semitones (a major or a minor third), not scale-aware intervals.
struct IntervalInfo { const char* name; float semitones; };
constexpr int numIntervals = 9;
constexpr int defaultInterval = 7; // fifth up: consonant over both major and minor melodies
inline const IntervalInfo& intervalInfo (int index)
{
    static const IntervalInfo table[numIntervals] = {
        { "Octave DOWN (-12)", -12.0f }, { "Fifth DOWN (-7)", -7.0f },     { "Major 3rd DOWN (-4)", -4.0f },
        { "Minor 3rd DOWN (-3)", -3.0f }, { "Minor 3rd UP (+3)", 3.0f },    { "Major 3rd UP (+4)", 4.0f },
        { "Fourth UP (+5)", 5.0f },      { "Fifth UP (+7)", 7.0f },        { "Octave UP (+12)", 12.0f } };
    return table[index < 0 ? 0 : (index >= numIntervals ? numIntervals - 1 : index)];
}

class HarmonyVoice
{
public:
    static constexpr int maxChannels = 2;
    static constexpr float minSemitones = -12.0f, maxSemitones = 12.0f;

    struct Settings
    {
        float semitones = 7.0f; // interval above (+) or below (-) the lead
        float levelDb = -9.0f;  // level of the harmony voice relative to the lead
    };

    // The delay the voice adds (same as Pitch Tune), so the host-facing owner can report it without a prepared instance.
    static int latencyFor (double sampleRate) { return pitch::PitchCorrector::latencyFor (sampleRate); }

    void prepare (double sampleRate); // allocates: message thread only
    void reset();
    void setSettings (const Settings&); // once per block; audio-thread safe

    // In place: output = delayed lead + harmony voice. 1 or 2 channels.
    void process (float* const* channels, int numChannels, int numSamples);
    // While the harmony is off: keep the lead history and the pitch detector current (cheap), so process() can start straight away.
    void feed (const float* const* channels, int numChannels, int numSamples);

    int getLatencySamples() const { return latency; }
    bool isVoiced() const { return gateTarget > 0.5f; }

private:
    static constexpr int chunk = 64; // the voiced gate is refreshed on this fixed grid (so the output never depends on the host's block size)

    void applyVoiceSettings(); // hands the interval to the pitch-shifting voice (the one place that does it)

    double sr = 48000.0;
    int latency = 0;
    pitch::PitchCorrector voice;
    std::vector<float> leadRing[maxChannels];
    std::vector<float> gateRing;
    size_t pos = 0;
    float scratch[maxChannels][chunk] {};
    float leadOut[maxChannels][chunk] {};
    long counter = 0; // absolute sample count: chunk boundaries are multiples of `chunk`
    float gateTarget = 0.0f, gateNow = 0.0f;
    long sinceVoiced = 1 << 30;
    float gainTarget = 0.0f, gainNow = 0.0f;
    Settings s;
};
} // namespace glitchkid::harmony
