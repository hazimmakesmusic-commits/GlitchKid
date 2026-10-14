#include "Compressor.h"

namespace
{
constexpr float activeRatio = 1.005f;     // below this the static curve is the identity
constexpr float idleReductionDb = 0.01f;  // below this the release tail is considered finished
constexpr float flatMakeupDb = 0.05f;     // half of the 0.1 dB parameter step
}

float Compressor::staticReductionDb (float x, const CompressorSettings& s)
{
    const float over = x - s.thresholdDb;
    const float w = s.kneeDb;
    const float slope = 1.0f - 1.0f / juce::jmax (1.0f, s.ratio);

    if (2.0f * over < -w)
        return 0.0f;
    if (w > 0.0f && 2.0f * std::abs (over) <= w)
    {
        const float t = over + 0.5f * w;
        return slope * t * t / (2.0f * w);
    }
    return slope * over;
}

void Compressor::prepare (double sr)
{
    sampleRate = sr;
    makeupGain.reset (sr, 0.02);
    makeupGain.setCurrentAndTargetValue (makeupIsUnity ? 1.0f : juce::Decibels::decibelsToGain (settings.makeupDb));
    updateCoefficients();
    reset();
}

void Compressor::reset()
{
    reductionDb = 0.0f;
    blockMaxReductionDb = 0.0f;
}

void Compressor::updateCoefficients()
{
    auto coef = [this] (float ms) { return (float) std::exp (-1.0 / (juce::jmax (0.01, (double) ms) * 0.001 * sampleRate)); };
    attackCoef = coef (settings.attackMs);
    releaseCoef = coef (settings.releaseMs);
}

void Compressor::setSettings (const CompressorSettings& s)
{
    settings = s;
    makeupIsUnity = std::abs (s.makeupDb) < flatMakeupDb;
    makeupGain.setTargetValue (makeupIsUnity ? 1.0f : juce::Decibels::decibelsToGain (s.makeupDb));
    updateCoefficients();
}

void Compressor::process (juce::AudioBuffer<float>& buffer)
{
    blockMaxReductionDb = 0.0f;
    const bool ratioActive = settings.ratio > activeRatio;

    if (! ratioActive && reductionDb < idleReductionDb && makeupIsUnity && ! makeupGain.isSmoothing())
    {
        reductionDb = 0.0f; // idle: leave the audio untouched
        return;
    }

    const int numChannels = juce::jmin (buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();
    float* data[2] = { buffer.getWritePointer (0), numChannels > 1 ? buffer.getWritePointer (1) : nullptr };

    for (int i = 0; i < numSamples; ++i)
    {
        float peak = std::abs (data[0][i]);
        if (data[1] != nullptr)
            peak = juce::jmax (peak, std::abs (data[1][i]));

        const float levelDb = peak > 1.0e-6f ? 20.0f * std::log10 (peak) : -120.0f;
        const float target = ratioActive ? staticReductionDb (levelDb, settings) : 0.0f;

        const float c = target > reductionDb ? attackCoef : releaseCoef;
        reductionDb = c * reductionDb + (1.0f - c) * target;
        blockMaxReductionDb = juce::jmax (blockMaxReductionDb, reductionDb);

        const float gain = std::pow (10.0f, -reductionDb * 0.05f) * makeupGain.getNextValue();
        data[0][i] *= gain;
        if (data[1] != nullptr)
            data[1][i] *= gain;
    }
}
