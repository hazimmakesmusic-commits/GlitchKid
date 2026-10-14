#include "Dimension.h"
#include "Finite.h"

namespace
{
constexpr double twoPi = juce::MathConstants<double>::twoPi;
constexpr float corrupt = 1.0e30f;
constexpr double initialPhase[2] = { 0.0, 2.1 };
constexpr float rampSeconds = 0.02f;
constexpr float excursionSeconds = 0.05f;

// Peak delay excursion (samples) that gives a peak pitch swing of `cents` at LFO rate `hz`:
// delay = d0 + A sin(w t)  ->  pitch ratio = 1 - A w cos(w t), so the swing is A w = 2^(cents/1200) - 1.
double excursionSamples (float cents, double hz, double sr)
{
    return (std::pow (2.0, (double) cents / 1200.0) - 1.0) / (twoPi * hz) * sr;
}
} // namespace

void Dimension::prepare (double sr)
{
    sampleRate = sr;
    ringSize = (int) std::ceil (0.03 * sr) + 8; // 30 ms: the longest delay is 13 ms + 4 ms excursion
    ring.assign ((size_t) ringSize, 0.0f);
    hpR = std::exp (-twoPi * 200.0 / sr);
    wet.reset (sr, rampSeconds);
    widthAngle.reset (sr, rampSeconds);
    excursion.reset (sr, excursionSeconds);
    reset();
    // Adopt whatever was set before prepare() (e.g. a restored session) without a fade.
    const auto s = target;
    wet.setCurrentAndTargetValue (0.0f);
    widthAngle.setCurrentAndTargetValue (0.0f);
    excursion.setCurrentAndTargetValue (0.0f);
    setSettings (s);
    wet.setCurrentAndTargetValue (wet.getTargetValue());
    widthAngle.setCurrentAndTargetValue (widthAngle.getTargetValue());
    excursion.setCurrentAndTargetValue (excursion.getTargetValue());
}

void Dimension::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    writePos = 0;
    hpX = hpY = 0.0;
    phase[0] = initialPhase[0];
    phase[1] = initialPhase[1];
    active = false;
}

void Dimension::setSettings (const DimensionSettings& s)
{
    target = s;
    const float mix = s.mixPct < 0.05f ? 0.0f : (s.mixPct >= 99.95f ? 1.0f : s.mixPct * 0.01f);
    const float depth = juce::jlimit (0.0f, 100.0f, s.depthPct) * 0.01f;
    wet.setTargetValue (s.on ? maxWet * depth * mix : 0.0f);
    widthAngle.setTargetValue (juce::jlimit (0.0f, 100.0f, s.widthPct) * 0.01f * juce::MathConstants<float>::halfPi);
    excursion.setTargetValue (juce::jlimit (0.0f, 20.0f, s.detuneCents));
}

float Dimension::readVoice (double d) const
{
    const double pos = (double) writePos - d;
    const double fl = std::floor (pos);
    const float f = (float) (pos - fl);
    const int i0 = (int) fl;
    auto at = [&] (int i) { return ring[(size_t) (((i % ringSize) + ringSize) % ringSize)]; };
    const float ym1 = at (i0 - 1), y0 = at (i0), y1 = at (i0 + 1), y2 = at (i0 + 2);
    return y0 + 0.5f * f * (y1 - ym1 + f * (2.0f * ym1 - 5.0f * y0 + 4.0f * y1 - y2 + f * (3.0f * (y0 - y1) + y2 - ym1)));
}

void Dimension::process (juce::AudioBuffer<float>& buffer)
{
    const int numChannels = juce::jmin (buffer.getNumChannels(), 2);
    const int n = buffer.getNumSamples();

    if (! target.on && ! wet.isSmoothing() && wet.getTargetValue() <= 0.0f)
    {
        if (active)
            reset(); // settled off: forget old audio so a later switch-on starts clean
        return;
    }
    active = true;

    for (int ch = 0; ch < numChannels; ++ch)
        dspguard::zeroNonFinite (buffer.getWritePointer (ch), n);

    const double inc[2] = { twoPi * lfoHz[0] / sampleRate, twoPi * lfoHz[1] / sampleRate };
    // Excursion (samples) and the Width angle's cos/sin only change while their smoothers move.
    double amp[2];
    float cosA = 1.0f, sinA = 0.0f;
    auto refresh = [&]
    {
        for (int k = 0; k < 2; ++k)
            amp[k] = excursionSamples (excursion.getCurrentValue(), lfoHz[k], sampleRate);
        cosA = std::cos (widthAngle.getCurrentValue());
        sinA = std::sin (widthAngle.getCurrentValue());
    };
    refresh();

    float* l = buffer.getWritePointer (0);
    float* r = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < n; ++i)
    {
        const float g = wet.getNextValue();
        const bool moving = widthAngle.isSmoothing() || excursion.isSmoothing();
        widthAngle.skip (1);
        excursion.skip (1);
        if (moving)
            refresh();

        // Mid, high-passed (one pole), written into the delay line.
        const double mid = r != nullptr ? 0.5 * ((double) l[i] + (double) r[i]) : (double) l[i];
        hpY = hpR * (hpY + mid - hpX);
        hpX = mid;
        float w = (float) hpY;
        if (! (std::abs (w) < corrupt))
        {
            w = 0.0f;
            hpX = hpY = 0.0;
        }
        ring[(size_t) writePos] = w;

        double v[2];
        for (int k = 0; k < 2; ++k)
        {
            const double d = baseDelayMs[k] * 0.001 * sampleRate + amp[k] * std::sin (phase[k]);
            v[k] = (double) readVoice (d);
            phase[k] += inc[k];
            if (phase[k] >= twoPi) phase[k] -= twoPi;
        }
        writePos = (writePos + 1) % ringSize;

        const float m = (float) (0.5 * (v[0] + v[1]));
        const float s = (float) (0.5 * (v[0] - v[1]));
        if (r != nullptr)
        {
            const float mPart = g * cosA * m, sPart = g * sinA * s;
            l[i] += mPart + sPart;
            r[i] += mPart - sPart;
        }
        else
            l[i] += g * m;
    }
}
