#include "VocalMutation.h"
#include "Finite.h"

namespace
{
constexpr double twoPi = juce::MathConstants<double>::twoPi;
constexpr double stateLimit = 1.0e9; // legitimate audio is within about +-100; anything past this clears the filters
constexpr float wetRampSeconds = 0.02f;
constexpr float shapeRampSeconds = 0.04f;
constexpr double floatCeiling = 3.0e38; // keeps the float result finite for inputs near the float limit
constexpr float fadeInPct = 3.0f; // Mutation percentage over which the wet signal fades in from 0

// All-pass coefficients a_k of (a_k - z^-2) / (1 - a_k z^-2), found by least-squares design so that the quadrature chain
// (delayed by one sample) is 90 degrees behind the in-phase chain across the audio band (see the header).
constexpr double coefA[VocalMutation::numSections] = { 0.15298956, 0.72839545, 0.94898832, 0.99222501 };
constexpr double coefB[VocalMutation::numSections] = { 0.46649754, 0.87826725, 0.9794451, 0.99801862 };
} // namespace

double VocalMutation::carrierHzFor (float mutationPct)
{
    const double m = juce::jlimit (0.0, 1.0, (double) mutationPct * 0.01);
    return maxCarrierHz * m * m;
}

double VocalMutation::sidebandFor (float characterPct)
{
    return juce::jlimit (-1.0, 1.0, ((double) characterPct * 0.01 - 0.5) * 2.0);
}

double VocalMutation::run (Section* chain, const double* coef, double x, bool& overflow)
{
    for (int k = 0; k < numSections; ++k)
    {
        Section& s = chain[k];
        const double y = coef[k] * (x + s.y2) - s.x2;
        if (! (std::abs (y) < stateLimit))
            overflow = true;
        s.x2 = s.x1;
        s.x1 = x;
        s.y2 = s.y1;
        s.y1 = y;
        x = y;
    }
    return x;
}

void VocalMutation::clearChannel (Channel& c)
{
    c = Channel();
}

void VocalMutation::prepare (double sr)
{
    sampleRate = sr;
    wet.reset (sr, wetRampSeconds);
    mutation.reset (sr, shapeRampSeconds);
    sideband.reset (sr, shapeRampSeconds);
    reset();
    // Adopt whatever was set before prepare() (e.g. a restored session) without a fade.
    const auto s = target;
    setSettings (s);
    wet.setCurrentAndTargetValue (wet.getTargetValue());
    mutation.setCurrentAndTargetValue (mutation.getTargetValue());
    sideband.setCurrentAndTargetValue (sideband.getTargetValue());
}

void VocalMutation::reset()
{
    clearChannel (ch[0]);
    clearChannel (ch[1]);
    phase = 0.0;
    active = false;
}

void VocalMutation::setSettings (const MutationSettings& s)
{
    target = s;
    const float blend = s.blendPct < 0.05f ? 0.0f : (s.blendPct >= 99.95f ? 1.0f : s.blendPct * 0.01f);
    wet.setTargetValue (s.on ? blend : 0.0f);
    mutation.setTargetValue (juce::jlimit (0.0f, 100.0f, s.mutationPct));
    sideband.setTargetValue ((float) sidebandFor (s.characterPct));
}

void VocalMutation::process (juce::AudioBuffer<float>& buffer)
{
    const int numChannels = juce::jmin (buffer.getNumChannels(), 2);
    const int n = buffer.getNumSamples();

    // Off, Blend at 0 or Mutation at 0, and settled: nothing to do, bit-exact.
    const bool idle = (wet.getTargetValue() <= 0.0f || mutation.getTargetValue() <= 0.0f)
                      && ! wet.isSmoothing() && ! mutation.isSmoothing();
    if (idle)
    {
        if (active)
            reset(); // forget old audio so a later switch-on starts clean
        return;
    }
    active = true;

    for (int c = 0; c < numChannels; ++c)
        dspguard::zeroNonFinite (buffer.getWritePointer (c), n);

    float* data[2] = { buffer.getWritePointer (0), numChannels > 1 ? buffer.getWritePointer (1) : nullptr };

    for (int i = 0; i < n; ++i)
    {
        const float m = mutation.getNextValue();
        // The wet signal fades in over the first few percent of Mutation, so Mutation 0 is an exact copy of the input
        // (a carrier of 0 Hz alone would still apply the all-pass phase shift and, in ROBOT, a level change).
        const double g = (double) wet.getNextValue() * (double) juce::jmin (1.0f, m * (1.0f / fadeInPct));
        const double fc = carrierHzFor (m);
        const double s = (double) sideband.getNextValue();
        phase += twoPi * fc / sampleRate;
        if (phase >= twoPi) phase -= twoPi;
        const double cosP = std::cos (phase), sinP = std::sin (phase);
        const double gain = std::sqrt (2.0 / (1.0 + s * s)); // equal long-term level for every Character

        for (int c = 0; c < numChannels; ++c)
        {
            Channel& st = ch[c];
            const double x = (double) data[c][i];
            bool overflow = false;
            const double in = run (st.a, coefA, x, overflow);
            const double q1 = run (st.b, coefB, x, overflow);
            const double quad = st.qDelay;
            st.qDelay = q1;
            double wetSample = gain * (in * cosP - s * quad * sinP);
            if (overflow)
            {
                clearChannel (st); // absurdly large input: forget the filter memory at once, the dry signal carries on
                wetSample = x;
            }
            data[c][i] = (float) juce::jlimit (-floatCeiling, floatCeiling, x + g * (wetSample - x));
        }
    }
}
