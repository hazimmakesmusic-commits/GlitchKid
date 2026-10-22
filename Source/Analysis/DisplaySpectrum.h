#pragma once
#include <array>
#include <juce_dsp/juce_dsp.h>
#include "DisplayTap.h"

namespace glitchkid::analysis
{
// The interface's spectrum: a windowed FFT of the newest samples in a DisplayTap, folded into log-spaced columns. Message thread only (never the audio thread).
// Levels are real: 0 = -90 dBFS or lower, 1 = -6 dBFS or higher, for a sine of amplitude A (A = 1 reads 0 dBFS before the clamp). They rise at once and fall
// at a fixed rate so the display is readable at 30 Hz. This is a display, not a measurement tool: nothing in the audio path uses it.
class DisplaySpectrum
{
public:
    static constexpr int columns = 96;
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;

    DisplaySpectrum();
    void update (const DisplayTap&, double sampleRate);
    const std::array<float, columns>& levels() const { return shown; }
    bool hasSignal() const { return signal; }          // the newest window held more than inaudible noise
    static float columnHz (int column, double sampleRate);
    static float levelToDb (float level01) { return -90.0f + level01 * 84.0f; }

private:
    juce::dsp::FFT fft;
    std::array<float, fftSize> window {};
    std::array<float, fftSize> samples {};
    std::array<float, 2 * fftSize> work {};
    std::array<float, columns> shown {};
    bool signal = false;
};

// The newest samples reduced to min/max pairs per column, for a waveform strip.
struct WaveColumns
{
    static constexpr int columns = 240;
    std::array<float, columns> lo {}, hi {};
    bool hasSignal = false;
    void update (const DisplayTap&, int windowSamples);
};
}
