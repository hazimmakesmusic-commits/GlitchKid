#include "VocalDoubler.h"
#include "../Finite.h"

namespace
{
constexpr double pi = juce::MathConstants<double>::pi;
constexpr float corrupt = 1.0e30f;
constexpr float wetRampSeconds = 0.02f;
constexpr float widthRampSeconds = 0.05f;
constexpr float shapeRampSeconds = 0.2f; // detune and timing: a slow glide so moving the knob only bends the pitch gently
constexpr double minAverageFactor = 0.75; // the shortest segment is 0.75 x the average one (see Wander::next)
}

// ---- the random wander ----------------------------------------------------------------------------------------------------------------

double VocalDoubler::Wander::unit()
{
    state ^= state << 13; state ^= state >> 17; state ^= state << 5;
    if (state == 0) state = 0x9e3779b9u;
    return (double) (state >> 8) / 16777216.0;
}

void VocalDoubler::Wander::next (double sr)
{
    from = to;
    to = unit() * 2.0 - 1.0;
    length = juce::jmax (1.0, averageSeconds * (minAverageFactor + 0.5 * unit()) * sr);
    pos = 0.0;
}

void VocalDoubler::Wander::reseed (std::uint32_t seed, double avgSeconds, double sr)
{
    state = seed == 0 ? 1u : seed;
    averageSeconds = avgSeconds;
    to = unit() * 2.0 - 1.0;
    next (sr);
}

double VocalDoubler::Wander::value (double sr)
{
    if (pos >= length)
        next (sr);
    const double t = pos / length;
    pos += 1.0;
    return from + (to - from) * 0.5 * (1.0 - std::cos (pi * t)); // eases in and out, so the pitch wobble has no jumps
}

// ---- the doubler -----------------------------------------------------------------------------------------------------------------------

void VocalDoubler::prepare (double sr)
{
    sampleRate = sr;
    ringSize = (int) std::ceil (0.1 * sr) + 8; // 100 ms: the longest delay is 45 ms x 1.25 plus the wander and the timing glide
    ring.assign ((size_t) ringSize, 0.0f);
    lowCoef = 1.0 - std::exp (-2.0 * pi * (double) lowpassHz / sr);
    wet.reset (sr, wetRampSeconds);
    widthAngle.reset (sr, widthRampSeconds);
    excursion.reset (sr, shapeRampSeconds);
    timing.reset (sr, shapeRampSeconds);
    reset();
    const auto s = target; // adopt whatever was set before prepare() (a restored session) without a fade
    wet.setCurrentAndTargetValue (0.0f);
    setSettings (s);
    wet.setCurrentAndTargetValue (wet.getTargetValue());
    widthAngle.setCurrentAndTargetValue (widthAngle.getTargetValue());
    excursion.setCurrentAndTargetValue (excursion.getTargetValue());
    timing.setCurrentAndTargetValue (timing.getTargetValue());
}

void VocalDoubler::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    writePos = 0;
    lowState[0] = lowState[1] = 0.0;
    wander[0].reseed (0x1badc0deu, driftSeconds[0], sampleRate);
    wander[1].reseed (0x0ddba11u, driftSeconds[1], sampleRate);
    lastDelay[0] = lastDelay[1] = 0.0;
    active = false;
}

void VocalDoubler::setSettings (const DoublerSettings& s)
{
    target = s;
    auto clampPct = [] (float v, float fallback) { return std::isfinite (v) ? juce::jlimit (0.0f, 100.0f, v) : fallback; };
    const float amount = clampPct (s.amountPct, 0.0f) * 0.01f;
    wet.setTargetValue (s.on ? maxWet * amount : 0.0f);
    widthAngle.setTargetValue (clampPct (s.widthPct, 70.0f) * 0.01f * juce::MathConstants<float>::halfPi);
    excursion.setTargetValue (std::isfinite (s.detuneCents) ? juce::jlimit (0.0f, maxDetuneCents, s.detuneCents) : 0.0f);
    timing.setTargetValue (std::isfinite (s.timingMs) ? juce::jlimit (minTimingMs, maxTimingMs, s.timingMs) : 18.0f);
}

float VocalDoubler::readVoice (double d) const
{
    const double pos = (double) writePos - d;
    const double fl = std::floor (pos);
    const float f = (float) (pos - fl);
    const int i0 = (int) fl;
    auto at = [&] (int i) { return ring[(size_t) (((i % ringSize) + ringSize) % ringSize)]; };
    const float ym1 = at (i0 - 1), y0 = at (i0), y1 = at (i0 + 1), y2 = at (i0 + 2);
    return y0 + 0.5f * f * (y1 - ym1 + f * (2.0f * ym1 - 5.0f * y0 + 4.0f * y1 - y2 + f * (3.0f * (y0 - y1) + y2 - ym1)));
}

void VocalDoubler::process (juce::AudioBuffer<float>& buffer)
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

    float* l = buffer.getWritePointer (0);
    float* r = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    // The wander amplitude (samples) that makes the largest pitch wobble equal DETUNE: a path of +-1 swung over its shortest
    // segment moves the delay by at most A x pi / L_min samples per sample, which is the pitch deviation.
    double amp[2];
    float cosA = 1.0f, sinA = 0.0f;
    auto refresh = [&]
    {
        const double dev = std::pow (2.0, (double) excursion.getCurrentValue() / 1200.0) - 1.0;
        for (int k = 0; k < 2; ++k)
            amp[k] = dev * minAverageFactor * driftSeconds[k] * sampleRate / pi;
        cosA = std::cos (widthAngle.getCurrentValue());
        sinA = std::sin (widthAngle.getCurrentValue());
    };
    refresh();

    for (int i = 0; i < n; ++i)
    {
        const float g = wet.getNextValue();
        const bool moving = widthAngle.isSmoothing() || excursion.isSmoothing();
        widthAngle.skip (1);
        excursion.skip (1);
        if (moving)
            refresh();
        const double timingSamples = (double) timing.getNextValue() * 0.001 * sampleRate;

        const double mid = r != nullptr ? 0.5 * ((double) l[i] + (double) r[i]) : (double) l[i];
        ring[(size_t) writePos] = (float) mid;

        double v[2];
        for (int k = 0; k < 2; ++k)
        {
            const double d = juce::jmax (2.0, timingSamples * delayScale[k] + amp[k] * wander[k].value (sampleRate));
            lastDelay[k] = d;
            const double raw = (double) readVoice (d);
            lowState[k] += lowCoef * (raw - lowState[k]); // softened: the double is a little darker than the lead
            if (! (std::abs (lowState[k]) < (double) corrupt))
                lowState[k] = 0.0;
            v[k] = lowState[k];
        }
        writePos = (writePos + 1) % ringSize;

        const float m = (float) (0.5 * (v[0] + v[1]));
        const float s = (float) (0.5 * (v[0] - v[1]));
        if (r != nullptr)
        {
            l[i] += g * (cosA * m + sinA * s);
            r[i] += g * (cosA * m - sinA * s);
        }
        else
            l[i] += g * m;
    }
}
