#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <complex>

// EQ LAB: a 16-band minimum-phase parametric equaliser for up to 2 channels.
//
//  - Every filter is a trapezoidal (TPT) state-variable filter (Andrew Simper's "SvfLinearTrapOptimised2" formulation, public), run in double precision.
//    It is the bilinear transform of the textbook analogue prototype with the cutoff prewarped, so a bell or shelf has exactly the response of the
//    RBJ "Audio EQ Cookbook" filter it replaces, and it stays stable and click-free while its settings move every few samples (needed for dynamic EQ).
//  - Types: bell, low shelf, high shelf, low cut, high cut (6-96 dB/oct Butterworth cascades; Q above 0.707 adds resonance at the cutoff), notch,
//    band pass (unity peak), tilt (a shelf pivoting around its frequency: -gain/2 below, +gain/2 above).
//  - Each band works on Stereo (both channels), Left, Right, Mid or Side. Mid/Side bands encode M = (L+R)/2, S = (L-R)/2 around themselves.
//  - responseDb() computes |H| from the same coefficients the audio uses, so a graph drawn with it is the real filter, not a picture of one.
//  - Real-time safe: no allocation after prepare(). Frequency, gain and Q are smoothed over ~20 ms and the coefficients are refreshed every 16
//    samples while a setting moves. A change of type, slope, channel or on/off fades the band out and back in over ~20 ms instead of clicking.
//  - A gain-type band (bell, shelves, tilt) within 0.05 dB of flat is skipped entirely, so a flat EQ is bit-exact.
//  - Dynamic EQ (bell, shelves, tilt): a band can move its own gain with the level of its own region. The detector is a band-pass (bell, tilt), low-pass
//    (low shelf) or high-pass (high shelf) at the band's frequency, run on the signal entering the band (on its channel: stereo bands are linked, so the
//    image never moves) and followed by a peak envelope (attack / release). ABOVE: the gain moves by up to `range` as the band rises above the threshold
//    (negative range = a cut that only happens when the band gets loud: de-essing, taming harshness; positive = a lift when loud). BELOW: the gain moves
//    as the band falls under the threshold (positive range = lifting quiet detail; negative = expanding downward). The slope is 3:4 (full range 4/3 x range
//    dB past the threshold) with a 6 dB soft knee; silence (under -80 dBFS) moves nothing. The current change is published for the interface.
//  - Solo: setSolo(b) replaces the output with the part of the input that band b works on (a band-pass around a bell, notch or band pass; the region a
//    shelf, cut or tilt moves), crossfaded over ~20 ms. It is monitoring only: never stored, never automated.
enum class EqType { bell = 0, lowShelf, highShelf, lowCut, highCut, notch, bandPass, tilt };
enum class EqPlacement { stereo = 0, left, right, mid, side };

struct EqBandSettings
{
    float freqHz = 1000.0f;
    float gainDb = 0.0f;   // bell, shelves, tilt; ignored by cuts, notch and band pass
    float q = 0.7071f;     // bandwidth (bell, notch, band pass), shelf/tilt slope, resonance of a cut
    EqType type = EqType::bell;
    bool on = true;
    int slopeDb = 12;      // cuts only: 6, 12, 18, 24, 36, 48, 72 or 96 dB/oct
    EqPlacement placement = EqPlacement::stereo;
    bool dynamic = false;  // gain types only
    float threshDb = -30.0f, rangeDb = -6.0f, attackMs = 5.0f, releaseMs = 120.0f;
    bool below = false;    // false: acts above the threshold; true: below it
};

class ParametricEq
{
public:
    static constexpr int numBands = 16;
    static constexpr int maxChannels = 2;
    static constexpr int numSlopes = 8;
    static constexpr int slopes[numSlopes] = { 6, 12, 18, 24, 36, 48, 72, 96 };
    static constexpr int maxSections = 8; // 96 dB/oct = 16th order = 8 two-pole sections

    void prepare (double sampleRate);
    void reset();                                   // clear filter memory, jump to the targets
    void setBand (int band, const EqBandSettings&); // set the target (audio thread safe)
    void setSolo (int band);                        // -1 = no solo
    void process (juce::AudioBuffer<float>&);       // 1 or 2 channels

    // |H| in dB of one band (0 when it is off or flat) and of a set of bands, at `hz`, for sample rate `fs`. Same coefficients as the audio.
    static double bandResponseDb (const EqBandSettings&, double hz, double fs);
    static double responseDb (const EqBandSettings* bands, int count, double hz, double fs);
    static bool isGainType (EqType t) { return t == EqType::bell || t == EqType::lowShelf || t == EqType::highShelf || t == EqType::tilt; }
    static int slopeIndex (int slopeDb);            // nearest supported slope
    static constexpr float flatThresholdDb = 0.05f; // half of the 0.1 dB parameter step
    static bool needsWork (const EqBandSettings& s) { return s.on && (! isGainType (s.type) || std::abs (s.gainDb) >= flatThresholdDb || s.dynamic); } // can change the sound
    // The dynamic gain change of a band right now, in dB (0 when it is not dynamic). Audio thread writes, any thread reads.
    float getDynamicDb (int band) const { return band >= 0 && band < numBands ? bands[band].shownDyn.load (std::memory_order_relaxed) : 0.0f; }
    // The static curve the dynamic gain change follows (exposed for tests and the interface): the change in dB for a band level in dBFS.
    static float dynamicGainDb (const EqBandSettings&, float levelDb);

private:
    struct Section // one TPT SVF (or one-pole when onePole)
    {
        double g = 0, k = 1, m0 = 1, m1 = 0, m2 = 0, a1 = 1, a2 = 0, a3 = 0;
        bool onePole = false;
        void set (double gIn, double kIn, double m0In, double m1In, double m2In, bool onePoleIn);
        std::complex<double> response (double tanW) const; // tanW = tan (pi f / fs)
        void run (double* x, int n, double& ic1, double& ic2) const; // in place, one channel's state
    };
    struct Design { Section s[maxSections]; int count = 0; double scale = 1.0; };
    static void design (Design&, const EqBandSettings&, double fs); // the filter for these (current) values

    struct Band
    {
        EqBandSettings target, current;               // current: the structure (type/slope/placement/on) in use now
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> freq { 1000.0f }, q { 0.7071f };
        juce::SmoothedValue<float> gainDb { 0.0f }, level { 0.0f }; // level: 1 = filtered, 0 = passed through (fades structural changes)
        Design d;
        double ic1[maxSections][maxChannels] {}, ic2[maxSections][maxChannels] {};
        bool dirty = true, running = false;
        // dynamic EQ: the detector filter, its state per signal, the envelope per signal, the gain change in use and the one shown
        Section detector;
        double detIc1[maxChannels] {}, detIc2[maxChannels] {};
        double env[maxChannels] {}, dynDb = 0.0, designedDyn = 0.0;
        struct SharedFloat // an atomic float that can be copied with the band (the engine stays a value type)
        {
            std::atomic<float> v { 0.0f };
            SharedFloat() = default;
            SharedFloat (const SharedFloat& o) : v (o.v.load (std::memory_order_relaxed)) {}
            SharedFloat& operator= (const SharedFloat& o) { v.store (o.v.load (std::memory_order_relaxed), std::memory_order_relaxed); return *this; }
            void store (float x, std::memory_order m) { v.store (x, m); }
            float load (std::memory_order m) const { return v.load (m); }
        } shownDyn;
        void clear();
    };
    void detect (Band&, double* const* sig, int numSignals, int n); // updates the band's dynamic gain change from its own input
    bool sameStructure (const EqBandSettings& a, const EqBandSettings& b) const;
    void runBand (Band&, double* const* ch, int numChannels, int n, const float* levelRamp, bool fading);

    Band bands[numBands];
    int soloBand = -1, soloShown = -1;
    juce::SmoothedValue<float> soloMix { 0.0f };
    Section soloFilter[2];          // two sections (a 4th-order band-pass, low-pass or high-pass)
    double soloIc1[2][maxChannels] {}, soloIc2[2][maxChannels] {};
    double sampleRate = 48000.0;
};
