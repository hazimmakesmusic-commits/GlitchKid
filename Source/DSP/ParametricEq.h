#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

struct EqBandSettings
{
    float freqHz = 1000.0f;
    float gainDb = 0.0f;
    float q = 0.7071f; // ignored for shelves (fixed at 0.7071, a Butterworth-like shelf slope)
};

// 4-band parametric EQ for up to 2 channels: low shelf, bell 1, bell 2, high shelf.
// Each band is an RBJ "Audio EQ Cookbook" biquad run in double precision.
// Real-time safe: no allocation after prepare(). Settings are smoothed over ~20 ms and the
// coefficients are refreshed every 32 samples while a setting is moving.
// A band whose gain is within 0.05 dB of 0 is skipped entirely, so a flat EQ is bit-exact.
class ParametricEq
{
public:
    static constexpr int numBands = 4;
    static constexpr int maxChannels = 2;

    void prepare (double sampleRate);
    void reset();                                   // clear filter memory
    void setBand (int band, const EqBandSettings&); // set the target (audio thread safe)
    void process (juce::AudioBuffer<float>&);       // 1 or 2 channels

private:
    enum class Type { lowShelf, bell, highShelf };

    struct Band
    {
        Type type = Type::bell;
        EqBandSettings target;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> freq { 1000.0f }, q { 0.7071f };
        juce::SmoothedValue<float> gainDb { 0.0f };
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1[maxChannels] {}, z2[maxChannels] {};
        bool active = false, dirty = true;
    };

    void updateCoefficients (Band&) const;

    Band bands[numBands];
    double sampleRate = 48000.0;
};
