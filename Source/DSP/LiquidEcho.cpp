#include "LiquidEcho.h"
#include "Finite.h"

namespace
{
constexpr double widthRatio = 0.5;     // right delay = left x (1 + widthRatio x width)
constexpr double minDelay = 4.0;       // samples; the 4-point read needs at least 3
constexpr double glideSeconds = 0.08;
constexpr double rampSeconds = 0.02;
constexpr float corrupt = 1.0e30f;     // a repeat this large cannot come from real audio

constexpr const char* names[LiquidEcho::numDivisions] = {
    "1/2", "1/4", "1/4 dotted", "1/4 triplet", "1/8", "1/8 dotted", "1/8 triplet", "1/16", "1/16 dotted", "1/16 triplet", "1/32" };
constexpr double beats[LiquidEcho::numDivisions] = {
    2.0, 1.0, 1.5, 2.0 / 3.0, 0.5, 0.75, 1.0 / 3.0, 0.25, 0.375, 1.0 / 6.0, 0.125 };

double widthFactor (float widthPct) { return 1.0 + widthRatio * juce::jlimit (0.0f, 100.0f, widthPct) * 0.01; }
} // namespace

const char* LiquidEcho::divisionName (int index) { return names[juce::jlimit (0, numDivisions - 1, index)]; }
double LiquidEcho::divisionBeats (int index) { return beats[juce::jlimit (0, numDivisions - 1, index)]; }

double LiquidEcho::delaySamplesFor (const EchoSettings& s, double sr)
{
    const double seconds = s.sync ? divisionBeats (s.division) * 60.0 / (double) juce::jlimit (1.0f, 1000.0f, s.bpm)
                                  : (double) juce::jlimit (1.0f, 2000.0f, s.timeMs) * 0.001;
    return juce::jlimit (minDelay, maxDelaySeconds * sr, seconds * sr);
}

void LiquidEcho::prepare (double sr)
{
    sampleRate = sr;
    ringSize = (int) std::ceil (maxDelaySeconds * (1.0 + widthRatio) * sr) + 8;
    for (auto& l : line)
        l.assign ((size_t) ringSize, 0.0f);
    dcR = 1.0 - juce::MathConstants<double>::twoPi * 10.0 / sr;
    for (auto& d : delay)
        d.reset (sr, glideSeconds);
    feedback.reset (sr, rampSeconds);
    toneCoef.reset (sr, rampSeconds);
    wet.reset (sr, rampSeconds);
    reset();
    snap = true;
    setSettings (target); // adopt whatever was set before prepare() (e.g. a restored session) without a glide
}

void LiquidEcho::reset()
{
    for (auto& l : line)
        std::fill (l.begin(), l.end(), 0.0f);
    for (int ch = 0; ch < 2; ++ch)
        lp[ch] = dcX[ch] = dcY[ch] = 0.0;
    writePos = 0;
    active = false;
    snap = true;
}

void LiquidEcho::setSettings (const EchoSettings& s)
{
    target = s;
    const double left = delaySamplesFor (s, sampleRate);
    const double right = juce::jlimit (minDelay, maxDelaySeconds * (1.0 + widthRatio) * sampleRate,
                                       left * widthFactor (s.widthPct));
    const float a = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi
                                              * juce::jlimit (400.0, 0.45 * sampleRate, (double) s.toneHz) / sampleRate));
    const float fb = juce::jlimit (0.0f, maxFeedbackPct, s.feedbackPct) * 0.01f;
    const float mix = s.mixPct < 0.05f ? 0.0f : (s.mixPct >= 99.95f ? 1.0f : s.mixPct * 0.01f);

    if (snap || ! active)
    {
        delay[0].setCurrentAndTargetValue (left);
        delay[1].setCurrentAndTargetValue (right);
        feedback.setCurrentAndTargetValue (fb);
        toneCoef.setCurrentAndTargetValue (a);
        snap = false;
    }
    else
    {
        delay[0].setTargetValue (left);
        delay[1].setTargetValue (right);
        feedback.setTargetValue (fb);
        toneCoef.setTargetValue (a);
    }
    wet.setTargetValue (s.on ? mix : 0.0f);
}

float LiquidEcho::readLine (int ch, double d) const
{
    const double pos = (double) writePos - d;
    const double fl = std::floor (pos);
    const float f = (float) (pos - fl);
    const int i0 = (int) fl;
    auto at = [&] (int i) { return line[ch][(size_t) (((i % ringSize) + ringSize) % ringSize)]; };
    const float ym1 = at (i0 - 1), y0 = at (i0), y1 = at (i0 + 1), y2 = at (i0 + 2);
    return y0 + 0.5f * f * (y1 - ym1 + f * (2.0f * ym1 - 5.0f * y0 + 4.0f * y1 - y2 + f * (3.0f * (y0 - y1) + y2 - ym1)));
}

void LiquidEcho::process (juce::AudioBuffer<float>& buffer)
{
    const int numChannels = juce::jmin (buffer.getNumChannels(), 2);
    const int n = buffer.getNumSamples();

    if (! target.on && ! wet.isSmoothing() && wet.getTargetValue() <= 0.0f)
    {
        if (active)
            reset(); // settled off: forget the old audio so a later switch-on starts clean
        return;
    }
    active = true;

    for (int ch = 0; ch < numChannels; ++ch)
        dspguard::zeroNonFinite (buffer.getWritePointer (ch), n);

    for (int i = 0; i < n; ++i)
    {
        const float m = wet.getNextValue();
        const float fb = feedback.getNextValue();
        const double a = (double) toneCoef.getNextValue();
        const double dl[2] = { delay[0].getNextValue(), delay[1].getNextValue() };

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* x = buffer.getWritePointer (ch) + i;
            const float d = readLine (ch, dl[ch]);

            double s = (double) *x + (double) fb * (double) d;
            lp[ch] += a * (s - lp[ch]);
            const double y = lp[ch] - dcX[ch] + dcR * dcY[ch];
            dcX[ch] = lp[ch];
            dcY[ch] = y;

            float w = (float) y;
            if (! (std::abs (w) < corrupt)) // also true for NaN/Inf: drop the filter memory, keep the line finite
            {
                w = 0.0f;
                lp[ch] = dcX[ch] = dcY[ch] = 0.0;
            }
            line[ch][(size_t) writePos] = w;

            *x = *x * (1.0f - m) + d * m;
        }
        writePos = (writePos + 1) % ringSize;
    }
}
