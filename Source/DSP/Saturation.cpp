#include "Saturation.h"
#include "Finite.h"

namespace
{
constexpr float offThresholdDb = 0.05f; // half of the 0.1 dB parameter step
constexpr double tinyK = 1.0e-6;        // below this the curve is the identity to double precision
constexpr int factor = 4;
}

void Saturation::prepare (double sampleRate, int maxBlockSize)
{
    maxBlock = juce::jmax (1, maxBlockSize);
    oversampler.initProcessing ((size_t) maxBlock);
    jassert (std::lround (oversampler.getLatencyInSamples()) == latencySamples);

    exactDelay.prepare (latencySamples);
    exact.setSize (2, maxBlock);
    driveDb.reset (sampleRate, 0.02);
    wet.reset (sampleRate, 0.02);
    // Start settled on whatever was set before prepare() (e.g. a restored session), without a fade-in.
    driveDb.setCurrentAndTargetValue (targetIsOff ? 0.0f : driveDb.getTargetValue());
    wet.setCurrentAndTargetValue (targetIsOff ? 0.0f : 1.0f);
    reset();
}

void Saturation::reset()
{
    oversampler.reset();
    oversamplerDirty = false;
    exactDelay.reset();
}

void Saturation::setDriveDb (float db)
{
    targetIsOff = std::abs (db) < offThresholdDb;
    driveDb.setTargetValue (targetIsOff ? 0.0f : juce::jlimit (0.0f, 18.0f, db));
}

void Saturation::process (juce::AudioBuffer<float>& buffer)
{
    for (int start = 0; start < buffer.getNumSamples(); start += maxBlock)
    {
        juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start,
                                        juce::jmin (maxBlock, buffer.getNumSamples() - start));
        processChunk (chunk);
    }
}

void Saturation::processChunk (juce::AudioBuffer<float>& buffer)
{
    const int numChannels = juce::jmin (buffer.getNumChannels(), 2);
    const int n = buffer.getNumSamples();

    // The oversampler is a recursive filter: one NaN/Inf would stay in its memory forever.
    for (int ch = 0; ch < numChannels; ++ch)
        dspguard::zeroNonFinite (buffer.getWritePointer (ch), n);

    // The exact delayed copy is always kept up to date: it is the output when idle and the
    // "dry" side of the fade when drive is switched on or off.
    for (int ch = 0; ch < numChannels; ++ch)
        exact.copyFrom (ch, 0, buffer, ch, 0, n);
    juce::AudioBuffer<float> exactView (exact.getArrayOfWritePointers(), numChannels, 0, n);
    exactDelay.process (exactView);

    wet.setTargetValue (targetIsOff ? 0.0f : 1.0f);

    if (targetIsOff && ! wet.isSmoothing() && ! driveDb.isSmoothing())
    {
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.copyFrom (ch, 0, exact, ch, 0, n);
        if (oversamplerDirty)
        {
            oversampler.reset();
            oversamplerDirty = false;
        }
        return;
    }
    oversamplerDirty = true;

    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numChannels, (size_t) n);
    auto up = oversampler.processSamplesUp (block);

    for (int i = 0; i < n; ++i)
    {
        const double k = std::pow (10.0, (double) driveDb.getNextValue() / 20.0) - 1.0;
        if (k < tinyK)
            continue; // identity

        const double invTanhK = 1.0 / std::tanh (k);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* p = up.getChannelPointer ((size_t) ch) + (size_t) (i * factor);
            for (int j = 0; j < factor; ++j)
                p[j] = (float) (std::tanh (k * (double) p[j]) * invTanhK);
        }
    }

    oversampler.processSamplesDown (block);

    for (int i = 0; i < n; ++i)
    {
        const float w = wet.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* out = buffer.getWritePointer (ch) + i;
            const float e = exact.getSample (ch, i);
            *out = e + (*out - e) * w;
        }
    }

    // Overflow from a huge finite input: reset the oversampler's memory and keep the output finite.
    bool bad = false;
    for (int ch = 0; ch < numChannels; ++ch)
        bad |= dspguard::zeroNonFinite (buffer.getWritePointer (ch), n);
    if (bad)
        oversampler.reset();
}
