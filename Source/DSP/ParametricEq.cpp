#include "ParametricEq.h"
#include "Finite.h"

namespace
{
constexpr int chunkSize = 32;           // coefficient refresh interval while a setting is moving
constexpr double smoothSeconds = 0.02;
constexpr float flatThresholdDb = 0.05f; // half of the 0.1 dB parameter step
constexpr double corruptState = 1.0e30;  // filter memory this large (~600 dB) cannot come from real audio
}

void ParametricEq::prepare (double sr)
{
    sampleRate = sr;
    bands[0].type = Type::lowShelf;
    bands[1].type = Type::bell;
    bands[2].type = Type::bell;
    bands[3].type = Type::highShelf;

    for (auto& b : bands)
    {
        b.freq.reset (sr, smoothSeconds);
        b.q.reset (sr, smoothSeconds);
        b.gainDb.reset (sr, smoothSeconds);
        b.freq.setCurrentAndTargetValue (b.target.freqHz);
        b.q.setCurrentAndTargetValue (b.target.q);
        b.gainDb.setCurrentAndTargetValue (b.target.gainDb);
        b.dirty = true;
    }
    reset();
}

void ParametricEq::reset()
{
    for (auto& b : bands)
        for (int ch = 0; ch < maxChannels; ++ch)
            b.z1[ch] = b.z2[ch] = 0.0;
}

void ParametricEq::setBand (int band, const EqBandSettings& s)
{
    auto& b = bands[band];
    b.target = s;
    b.target.gainDb = std::abs (s.gainDb) < flatThresholdDb ? 0.0f : s.gainDb;
    b.freq.setTargetValue (juce::jlimit (10.0f, 20000.0f, s.freqHz));
    b.q.setTargetValue (juce::jlimit (0.1f, 20.0f, s.q));
    b.gainDb.setTargetValue (b.target.gainDb);
}

// RBJ Audio EQ Cookbook (Robert Bristow-Johnson), normalised so a0 = 1.
void ParametricEq::updateCoefficients (Band& b) const
{
    const double f = juce::jmin ((double) b.freq.getCurrentValue(), 0.45 * sampleRate);
    const double A = std::pow (10.0, (double) b.gainDb.getCurrentValue() / 40.0);
    const double w0 = juce::MathConstants<double>::twoPi * f / sampleRate;
    const double cosw = std::cos (w0), sinw = std::sin (w0);
    double nb0, nb1, nb2, na0, na1, na2;

    if (b.type == Type::bell)
    {
        const double alpha = sinw / (2.0 * (double) b.q.getCurrentValue());
        nb0 = 1 + alpha * A;  nb1 = -2 * cosw;  nb2 = 1 - alpha * A;
        na0 = 1 + alpha / A;  na1 = -2 * cosw;  na2 = 1 - alpha / A;
    }
    else
    {
        const double alpha = sinw / (2.0 * 0.7071);
        const double k = 2.0 * std::sqrt (A) * alpha;
        if (b.type == Type::lowShelf)
        {
            nb0 = A * ((A + 1) - (A - 1) * cosw + k);
            nb1 = 2 * A * ((A - 1) - (A + 1) * cosw);
            nb2 = A * ((A + 1) - (A - 1) * cosw - k);
            na0 = (A + 1) + (A - 1) * cosw + k;
            na1 = -2 * ((A - 1) + (A + 1) * cosw);
            na2 = (A + 1) + (A - 1) * cosw - k;
        }
        else
        {
            nb0 = A * ((A + 1) + (A - 1) * cosw + k);
            nb1 = -2 * A * ((A - 1) + (A + 1) * cosw);
            nb2 = A * ((A + 1) + (A - 1) * cosw - k);
            na0 = (A + 1) - (A - 1) * cosw + k;
            na1 = 2 * ((A - 1) - (A + 1) * cosw);
            na2 = (A + 1) - (A - 1) * cosw - k;
        }
    }
    b.b0 = nb0 / na0; b.b1 = nb1 / na0; b.b2 = nb2 / na0;
    b.a1 = na1 / na0; b.a2 = na2 / na0;
}

void ParametricEq::process (juce::AudioBuffer<float>& buffer)
{
    const int numChannels = juce::jmin (buffer.getNumChannels(), maxChannels);
    const int numSamples = buffer.getNumSamples();

    // A NaN/Inf in a recursive filter would stay in its memory forever, so never let one in.
    for (int ch = 0; ch < numChannels; ++ch)
        dspguard::zeroNonFinite (buffer.getWritePointer (ch), numSamples);
    bool anyActive = false;

    for (int start = 0; start < numSamples; start += chunkSize)
    {
        const int n = juce::jmin (chunkSize, numSamples - start);

        for (auto& b : bands)
        {
            const bool moving = b.freq.isSmoothing() || b.q.isSmoothing() || b.gainDb.isSmoothing();
            if (moving)
            {
                b.freq.skip (n); b.q.skip (n); b.gainDb.skip (n);
                b.dirty = true;
            }

            const bool wasActive = b.active;
            b.active = moving || std::abs (b.target.gainDb) >= flatThresholdDb;
            if (! b.active)
            {
                if (wasActive)
                    for (int ch = 0; ch < maxChannels; ++ch)
                        b.z1[ch] = b.z2[ch] = 0.0;
                continue;
            }

            if (b.dirty)
            {
                updateCoefficients (b);
                b.dirty = false;
            }

            // Transposed direct form II.
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* x = buffer.getWritePointer (ch) + start;
                double z1 = b.z1[ch], z2 = b.z2[ch];
                for (int i = 0; i < n; ++i)
                {
                    const double in = x[i];
                    const double out = b.b0 * in + z1;
                    z1 = b.b1 * in - b.a1 * out + z2;
                    z2 = b.b2 * in - b.a2 * out;
                    x[i] = (float) out;
                }
                if (! (std::abs (z1) < corruptState && std::abs (z2) < corruptState)) // also true for NaN/Inf
                    z1 = z2 = 0.0; // drop the filter memory, not the plugin
                b.z1[ch] = z1;
                b.z2[ch] = z2;
            }
            anyActive = true;
        }
    }

    if (anyActive) // a huge finite input can still overflow the float output; keep it finite
        for (int ch = 0; ch < numChannels; ++ch)
            dspguard::zeroNonFinite (buffer.getWritePointer (ch), numSamples);
}
