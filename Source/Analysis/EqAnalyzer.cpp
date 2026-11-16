#include "EqAnalyzer.h"

namespace glitchkid::analysis
{
void EqAnalyzer::setResolution (int fftOrder)
{
    fftOrder = juce::jlimit (11, 13, fftOrder);
    if (fft != nullptr && fftOrder == order)
        return;
    order = fftOrder;
    size = 1 << order;
    fft = std::make_unique<juce::dsp::FFT> (order);
    window.resize ((size_t) size);
    for (int i = 0; i < size; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (size - 1));
    work.assign ((size_t) (2 * size), 0.0f);
    power.assign ((size_t) (size / 2 + 1), 0.0f);
}

bool EqAnalyzer::update (const DisplayTap& tap, double sr, float dt)
{
    if (frozen || sr < 8000.0)
        return false;
    rate = sr;
    const float a = 1.0f - std::exp (-juce::jlimit (0.001f, 0.5f, dt) / averaging);
    if (! freshness.update (tap, dt)) // nothing is being written: decay toward silence instead of freezing on the last frame
    {
        bool any = false;
        for (auto& p : power)
        {
            p *= 1.0f - a;
            any = any || p > 1.0e-12f;
        }
        signal = false;
        return any;
    }
    tap.copyLatest (work.data(), juce::jmin (size, (int) DisplayTap::size));
    float peakSample = 0.0f;
    for (int i = 0; i < size; ++i)
    {
        const float v = std::isfinite (work[(size_t) i]) ? work[(size_t) i] : 0.0f;
        peakSample = juce::jmax (peakSample, std::abs (v));
        work[(size_t) i] = v * window[(size_t) i];
    }
    std::fill (work.begin() + size, work.end(), 0.0f);
    fft->performFrequencyOnlyForwardTransform (work.data());
    signal = peakSample > 3.0e-5f; // about -90 dBFS
    const float scale = 4.0f / (float) size; // a sine of amplitude A peaks at A in its bin with a Hann window
    for (size_t b = 0; b < power.size(); ++b)
    {
        const float m = work[b] * scale;
        power[b] += a * (m * m - power[b]);
    }
    return true;
}

float EqAnalyzer::levelDb (float loHz, float hiHz) const
{
    const float binHz = (float) rate / (float) size;
    const int last = (int) power.size() - 1;
    int b0 = juce::jlimit (1, last, (int) std::floor (loHz / binHz)), b1 = juce::jlimit (b0, last, (int) std::ceil (hiHz / binHz));
    float m = 0.0f;
    for (int b = b0; b <= b1; ++b)
        m = juce::jmax (m, power[(size_t) b]);
    return m > 1.0e-12f ? 10.0f * std::log10 (m) : -120.0f;
}

bool EqAnalyzer::peak (float loHz, float hiHz, float& hz, float& db) const
{
    const float binHz = (float) rate / (float) size;
    const int last = (int) power.size() - 2;
    const int b0 = juce::jlimit (2, last, (int) std::floor (loHz / binHz)), b1 = juce::jlimit (b0, last, (int) std::ceil (hiHz / binHz));
    int best = -1;
    float bestP = 1.0e-8f; // -80 dBFS
    for (int b = b0; b <= b1; ++b)
        if (power[(size_t) b] > bestP && power[(size_t) b] >= power[(size_t) b - 1] && power[(size_t) b] >= power[(size_t) b + 1])
        {
            bestP = power[(size_t) b];
            best = b;
        }
    if (best < 0)
        return false;
    const auto dbAt = [this] (int b) { return 10.0f * std::log10 (juce::jmax (1.0e-12f, power[(size_t) b])); };
    const float l = dbAt (best - 1), c = dbAt (best), r = dbAt (best + 1), den = l - 2.0f * c + r;
    const float shift = std::abs (den) > 1.0e-6f ? juce::jlimit (-0.5f, 0.5f, 0.5f * (l - r) / den) : 0.0f;
    hz = ((float) best + shift) * binHz;
    db = c - 0.25f * (l - r) * shift;
    return true;
}

juce::String EqAnalyzer::noteName (float hz)
{
    if (! (hz > 0.0f))
        return {};
    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const float midi = 69.0f + 12.0f * std::log2 (hz / 440.0f);
    const int n = juce::roundToInt (midi);
    const int cents = juce::roundToInt ((midi - (float) n) * 100.0f);
    return juce::String (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1) + (cents == 0 ? juce::String() : (cents > 0 ? " +" : " ") + juce::String (cents) + " c");
}
}
