#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>

// CLEAN LAB: conservative, bounded vocal cleanup. Three parts, all zero-latency IIR processing; the FFT is only a *side-chain detector* (the audio is never resynthesised).
//
//   corrective EQ (before the compressor, after the user's EQ, so it sees what the user already shaped):
//       RUMBLE      a 2nd-order 70 Hz high-pass blended in, only as much as the measured energy below 60 Hz (against 150-500 Hz) calls for;
//       LOW-MID     one wide bell (Q 0.9, up to -3.5 dB) at the frequency where the long-term spectrum most exceeds the vocal's own spectral tilt
//                   (a straight line fitted to its 700 Hz - 5 kHz region and extended downward), only if the excess is above 7 dB.
//   RESONANCE  (same position): up to 3 narrow bells (Q 6, up to -6 dB). A resonance is a narrow band that stays above its surroundings (+-1/3 octave) while the
//       voice MOVES: the evidence is accumulated only while the spectrum is changing (pitch glides, vowel changes) and is frozen while a note is held, so the harmonics
//       of a sustained note are not mistaken for a resonance. Detection is slow (seconds), slots keep their frequency, and everything is smoothed.
//   DE-ESSER   (after the compressor, before the saturation): sibilance = energy above 4.5 kHz relative to the body of the voice (about 1 kHz), measured on the mono
//       sum so both channels get the same gain. When it exceeds a threshold a wide bell (7.5 kHz, Q 0.9, about 4.5-12.5 kHz) is lowered by up to 9 dB (ratio 4, 3 dB knee, 0.5 ms attack,
//       40 ms release) while the sibilance lasts; everything else is untouched, and with no reduction the audio passes through bit for bit. No look-ahead, so a very fast consonant can pass its first fraction of a millisecond.
//
//   CLEAN (amount) 0..1 sets how much of each is allowed; the three section trims (tone, res, ess) multiply it for that section only. Amount 0 = the module does nothing.
//   Off = an exact pass-through (bit for bit) at no CPU cost; on/off is a 20 ms crossfade. While on, CLEAN LAB LEARNS the voice: the corrective measures need about
//   a second of voiced material and resonances a few seconds of moving voice before anything is applied.
//
// Real-time safe after prepare(); 1 or 2 channels; no latency. Telemetry is published through atomics for the editor.
class CleanLab
{
public:
    struct Settings
    {
        bool on = false;
        float amount = 0.5f; // 0..1
        float tone = 1.0f;   // section trims, 0..1
        float res = 1.0f;
        float ess = 1.0f;
    };

    static constexpr int maxChannels = 2, numRes = 3;

    // What is applied right now (real, smoothed values from the filters that are running).
    struct Applied
    {
        float rumbleAmount = 0.0f, rumbleHz = 70.0f;
        float mudHz = 250.0f, mudDb = 0.0f, mudQ = 0.9f;
        float resHz[numRes] {}, resDb[numRes] {};
        float resQ = 6.0f;
        float essGrDb = 0.0f;       // de-esser gain reduction (positive dB) at the last block
        float levelChangeDb = 0.0f; // net level change of the module (smoothed)
        float learnedSeconds = 0.0f; // voiced material heard while on, capped at 3 s
    };

    CleanLab();
    void prepare (double sampleRate);
    void reset();                          // full reset, including what has been learned
    void setLearningFrozen (bool frozen); // keeps what has been learned and stops learning more (the corrections stay applied); used by the tests to measure a steady response
    void park();                           // clear the filters and gains but keep what has been learned (bypass, switched off)
    void setSettings (const Settings&);
    void processCorrective (juce::AudioBuffer<float>&); // rumble, low-mid, resonances
    void processDeEss (juce::AudioBuffer<float>&);
    Applied applied() const;

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void highpass (double fs, double f, double q);
        void lowpass (double fs, double f, double q);
        void bandpass (double fs, double f, double q);
        void peaking (double fs, double f, double q, double gainDb);
        void clear() { z1 = z2 = 0.0; }
        float process (float x)
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            if (std::abs (z1) < 1.0e-25) z1 = 0.0;
            if (std::abs (z2) < 1.0e-25) z2 = 0.0;
            return (float) y;
        }
    };

    struct Bell
    {
        Biquad f[maxChannels];
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> freq { 1000.0f };
        juce::SmoothedValue<float> gainDb { 0.0f };
        float q = 1.0f;
        bool active() const { return std::abs (gainDb.getCurrentValue()) > 0.02f || gainDb.isSmoothing() || std::abs (gainDb.getTargetValue()) > 0.02f; }
        void update (double fs);
    };

    static constexpr int fftOrder = 11, fftSize = 1 << fftOrder, hop = 512, numBins = fftSize / 2;

    void idleReset();
    void pushForAnalysis (float monoSample);
    void analyseFrame();
    void chooseTargets();
    float binHz() const { return (float) (sampleRate / fftSize); }
    int binOf (float hz) const { return juce::jlimit (1, numBins - 2, (int) std::lround (hz / binHz())); }

    Settings target;
    double sampleRate = 48000.0;
    juce::SmoothedValue<float> onGain { 0.0f }, rumbleMix { 0.0f };
    Biquad rumble[maxChannels];
    Bell mud, res[numRes];

    // de-esser
    Biquad sibHp, bodyBp, essBell[maxChannels];
    double essCos = 1.0, essAlpha = 0.0;
    int essTick = 0;
    float sibEnv = 0.0f, bodyEnv = 0.0f, grDb = 0.0f;
    float envAtk = 0.0f, envRel = 0.0f, grAtk = 0.0f, grRel = 0.0f;
    juce::SmoothedValue<float> essDepth { 0.0f };
    float essBlockMax = 0.0f;
    static constexpr float essThreshold = 1.9f; // sibilance-to-body ratio that starts de-essing at depth 0 (lowered by 0.9 at full depth)

    // analysis (message-free; runs inside processCorrective on the audio thread, nothing is allocated after prepare)
    juce::dsp::FFT fft { fftOrder };
    std::vector<float> ring, window, fftBuf, S, smoothDb, pre, longNum, env, excess, avgExcess, oldExcess, longTerm, line;
    int ringPos = 0, sinceHop = 0, voicedFrames = 0, framesSinceSnapshot = 0, framesSinceTargets = 0;
    bool haveOld = false, haveLong = false, learningFrozen = false;
    float moveEvidence = 0.0f, longDen = 0.0f;
    float rumbleNeed = 0.0f, mudWantDb = 0.0f, mudWantHz = 250.0f;
    float resSlotHz[numRes] {}, resSlotWant[numRes] {};
    float alphaRes = 0.0f, betaLong = 0.0f, leakRes = 0.0f;

    // telemetry
    std::atomic<float> tRumbleAmount { 0.0f }, tMudHz { 250.0f }, tMudDb { 0.0f }, tEss { 0.0f }, tLevel { 0.0f }, tLearned { 0.0f };
    std::atomic<float> tResHz[numRes], tResDb[numRes];
    float levelCorr = 0.0f, levelEss = 0.0f;
};
