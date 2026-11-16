#pragma once
#include <memory>
#include <vector>
#include <juce_dsp/juce_dsp.h>
#include "DisplaySpectrum.h"

namespace glitchkid::analysis
{
// EQ LAB's spectrum analyser: a Hann-windowed FFT of the newest audio in a DisplayTap (the EQ's real input or output), averaged over time in the power
// domain. Resolution (FFT size 2048 / 4096 / 8192), averaging time and freeze are the user's. Levels are dBFS for a sine (a sine of amplitude A reads about
// 20 log10 A). Message thread only; nothing in the audio path reads it. Memory is allocated only when the resolution changes.
class EqAnalyzer
{
public:
    EqAnalyzer() { setResolution (12); }
    void setResolution (int fftOrder);             // 11, 12 or 13
    int getResolution() const { return order; }
    void setAveragingSeconds (float s) { averaging = juce::jlimit (0.01f, 5.0f, s); }
    float getAveragingSeconds() const { return averaging; }
    void setFrozen (bool f) { frozen = f; }
    bool isFrozen() const { return frozen; }

    // Returns true when the levels changed. A tap that stopped being written decays to silence (unless frozen).
    bool update (const DisplayTap& tap, double sampleRate, float dtSeconds);
    bool hasSignal() const { return signal; }
    float levelDb (float loHz, float hiHz) const;  // the loudest averaged level between two frequencies (one display column); -120 when silent
    // The loudest peak between loHz and hiHz (parabolic interpolation between bins): false when there is no signal there above -80 dBFS.
    bool peak (float loHz, float hiHz, float& hz, float& db) const;
    static juce::String noteName (float hz);        // "A4 +12 c"

private:
    int order = 12, size = 4096;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window, work, power;        // power: averaged, per bin (size / 2 + 1)
    double rate = 48000.0;
    float averaging = 0.35f;
    bool frozen = false, signal = false;
    TapFreshness freshness;
};
}
