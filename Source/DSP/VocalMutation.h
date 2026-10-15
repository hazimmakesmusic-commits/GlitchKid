#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

struct MutationSettings
{
    bool on = false;
    float mutationPct = 40.0f;  // 0..100: how far the voice is moved (sets the carrier frequency, 0 Hz .. maxCarrierHz, squared curve)
    float characterPct = 50.0f; // 0..100: 0 = DOWN (alien, content moves down), 50 = ROBOT (ring modulation), 100 = UP (alien, content moves up)
    float blendPct = 100.0f;    // 0..100: dry (0) to fully mutated (100)
};

// Vocal Mutation: a single-sideband frequency shifter / ring modulator for voices.
//
// This is NOT a waveshaper. The saturator (tanh) makes harmonics that are exact multiples of the notes being sung. Mutation
// moves every partial of the voice by the same number of HERTZ (not by a ratio), so harmonic relationships are broken:
// a voice at 220 Hz (partials 220, 440, 660 ...) shifted up by 100 Hz becomes 320, 540, 760 ... which is inharmonic and
// sounds metallic, bell-like or alien. A pitch shifter moves partials by a RATIO and keeps them harmonic; this does not.
//
//  - Analytic signal: two parallel chains of four second-order all-pass sections (computed offline: phase difference between
//    the chains is 90 degrees within 1.06 degrees from 100 Hz to 19.2 kHz at 48 kHz, 2.4 degrees at the extremes; the
//    unwanted image sideband is at least 40 dB down over 50 Hz..18 kHz, measured) give in-phase I and quadrature Q copies of the input with
//    unity magnitude. Zero reported latency (it is an IIR all-pass pair); the group delay is a few samples, and the dry
//    path of the Blend control is NOT delayed (see the note on Blend).
//  - Carrier: one sine/cosine pair at frequency fc, shared by both channels so the stereo image cannot move.
//  - out = g * ( I cos(wt) - s Q sin(wt) ).  s = +1: upper sideband only (every partial moves UP by fc); s = -1: lower sideband
//    only (DOWN by fc); s = 0: both sidebands = classic ring modulation (ROBOT). Character sweeps s from -1 to +1 and
//    morphs continuously between the three. g = sqrt(2 / (1 + s^2)) keeps the long-term level equal across Character
//    (ring modulation alone would be 3 dB quieter).
//  - Mutation sets fc = maxCarrierHz * m^2: fine control at the low end (a few Hz = a slow phasing/tremolo-like movement,
//    tens of Hz = rough growl, hundreds of Hz = robotic/metallic). The wet signal fades in over the first 3 % of
//    Mutation, so Mutation 0 is an exact copy of the input (skipped, bit-exact).
//  - Blend: out = in + b (mutated - in). The two are not time-aligned beyond the all-pass delay of a few samples, which is
//    harmless for a frequency-shifted wet signal (the wet is a different spectrum anyway); at small Mutation a partial blend
//    gives the classic beating/phasing "detuned double" sound.
//  - Off, Blend 0 or Mutation 0 (and settled) = skipped, bit-exact, memory cleared once. Switching on/off, Mutation, Character and Blend are all
//    smoothed (no zipper noise). Frequency changes glide the carrier, so a Mutation sweep is a smooth pitch-like sweep.
//  - NaN/Inf input is replaced by 0 before it reaches the recursive filters; the filters clear themselves if they ever
//    overflow. No allocation, locking or I/O after prepare().
//  - Known limits: partials moved above Nyquist fold back (aliasing) when shifting UP: with fc <= 600 Hz this affects only
//    content within 600 Hz of Nyquist, which is nearly silent in a voice. Below about 30 Hz the all-pass pair loses its
//    90-degree accuracy, so very low content is not cleanly single-sideband. Not a pitch-tracking effect: it does not
//    follow the sung note, so the result depends on the voice.
class VocalMutation
{
public:
    static constexpr double maxCarrierHz = 600.0;
    static constexpr int numSections = 4;

    void prepare (double sampleRate);
    void reset();
    void setSettings (const MutationSettings&); // once per block; audio thread safe
    void process (juce::AudioBuffer<float>&);   // 1 or 2 channels

    bool isActive() const { return active; }

    // Exposed for tests: the mapping from the Mutation percentage to the carrier frequency in Hz.
    static double carrierHzFor (float mutationPct);
    static double sidebandFor (float characterPct); // s in [-1, +1]

private:
    struct Section { double x1 = 0, x2 = 0, y1 = 0, y2 = 0; };
    struct Channel
    {
        Section a[numSections], b[numSections];
        double qDelay = 0.0; // the quadrature chain is delayed by one sample
    };

    static double run (Section* chain, const double* coef, double x, bool& overflow);
    void clearChannel (Channel&);

    double sampleRate = 48000.0;
    double phase = 0.0;
    MutationSettings target;
    bool active = false;
    Channel ch[2];
    juce::SmoothedValue<float> wet { 0.0f }, mutation { 0.0f }, sideband { 0.0f };
};
