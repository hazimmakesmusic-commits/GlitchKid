#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

// Delays 1-2 channels by a fixed whole number of samples, in place. Allocates only in prepare().
class FixedDelay
{
public:
    void prepare (int delaySamples)
    {
        length = juce::jmax (0, delaySamples);
        for (auto& r : ring)
            r.assign ((size_t) juce::jmax (1, length), 0.0f);
        pos = 0;
    }

    void reset()
    {
        for (auto& r : ring)
            std::fill (r.begin(), r.end(), 0.0f);
        pos = 0;
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (length == 0)
            return;

        const int numChannels = juce::jmin (buffer.getNumChannels(), 2);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float& slot = ring[(size_t) ch][(size_t) pos];
                float* x = buffer.getWritePointer (ch) + i;
                std::swap (slot, *x); // output the oldest sample, store the newest
            }
            pos = (pos + 1) % length;
        }
    }

private:
    std::vector<float> ring[2];
    int length = 0, pos = 0;
};
