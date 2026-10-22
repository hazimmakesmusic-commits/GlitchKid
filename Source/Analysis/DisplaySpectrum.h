#pragma once
#include <array>
#include <juce_dsp/juce_dsp.h>
#include "DisplayTap.h"

namespace glitchkid::analysis
{
// Tells the interface whether a tap is still being written to. When the host stops calling the plug-in (transport stopped, the track muted at the host, ...)
// the tap keeps its last samples; without this the display would freeze on the last frame as if it were live. After `staleSeconds` without new samples
// the display treats the tap as silent.
class TapFreshness
{
public:
    static constexpr float staleSeconds = 0.25f;
    bool update (const DisplayTap& tap, float dtSeconds)
    {
        const auto w = tap.writeCount();
        if (w != last)
        {
            last = w;
            idle = 0.0f;
        }
        else
            idle += dtSeconds;
        return idle < staleSeconds;
    }

private:
    std::uint32_t last = 0;
    float idle = 1000.0f; // nothing has arrived yet
};

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
    void update (const DisplayTap&, double sampleRate, float dtSeconds = 1.0f / 30.0f); // dtSeconds: time since the last call (the fall rate is per second)
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
    TapFreshness freshness;
};

// The newest samples reduced to min/max pairs per column, for a waveform strip.
struct WaveColumns
{
    static constexpr int columns = 240;
    std::array<float, columns> lo {}, hi {};
    bool hasSignal = false;
    void update (const DisplayTap&, int windowSamples, float dtSeconds = 1.0f / 30.0f);
    TapFreshness freshness;
};
}
