#include "OutputLimiter.h"
#include <cmath>

void OutputLimiter::prepare (double sampleRate)
{
    sr = sampleRate > 8000.0 ? sampleRate : 48000.0;
    lookahead = juce::jlimit (16, maxLookahead, (int) std::lround (0.0015 * sr));
    blend.reset (sr, 0.02);
    reset();
    setSettings (s);
    blend.setCurrentAndTargetValue (s.on ? 1.0f : 0.0f);
}

void OutputLimiter::reset()
{
    for (auto& a : audio)
        a.fill (0.0f);
    reqRing.fill (1.0f);
    minRing.fill (1.0f);
    pos = 0;
    gain = 1.0f;
    grDb.store (0.0f, std::memory_order_relaxed);
}

void OutputLimiter::setSettings (const Settings& n)
{
    s = n;
    s.ceilingDb = juce::jlimit (-12.0f, -0.1f, std::isfinite (s.ceilingDb) ? s.ceilingDb : -1.0f);
    s.driveDb = juce::jlimit (0.0f, 24.0f, std::isfinite (s.driveDb) ? s.driveDb : 0.0f);
    s.releaseMs = juce::jlimit (10.0f, 500.0f, std::isfinite (s.releaseMs) ? s.releaseMs : 80.0f);
    ceilingLin = juce::Decibels::decibelsToGain (s.ceilingDb);
    driveLin = juce::Decibels::decibelsToGain (s.driveDb);
    relCoef = 1.0f - std::exp (-1.0f / (s.releaseMs * 0.001f * (float) sr));
    blend.setTargetValue (s.on ? 1.0f : 0.0f);
}

void OutputLimiter::process (float* const* ch, int numChannels, int numSamples) noexcept
{
    const int nch = numChannels >= 2 ? 2 : 1;
    const std::uint32_t mask = ring - 1;
    float minGain = 1.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        const float b = blend.getNextValue();
        float in[2];
        for (int c = 0; c < nch; ++c)
        {
            const float v = ch[c][i];
            in[c] = std::isfinite (v) ? v : 0.0f;
        }
        if (nch == 1)
            in[1] = in[0];

        const bool active = s.on || b > 0.0f;
        float d0 = in[0] * driveLin, d1 = in[1] * driveLin;
        audio[0][(size_t) (pos & mask)] = d0;
        audio[1][(size_t) (pos & mask)] = d1;
        float req = 1.0f;
        if (active)
        {
            const float peak = std::max (std::abs (d0), std::abs (d1));
            if (peak > ceilingLin)
                req = ceilingLin / peak;
        }
        reqRing[(size_t) (pos & mask)] = req;

        float g = 1.0f;
        if (active)
        {
            float m = 1.0f;
            for (int k = 0; k <= lookahead; ++k)
                m = std::min (m, reqRing[(size_t) ((pos - (std::uint32_t) k) & mask)]);
            minRing[(size_t) (pos & mask)] = m;
            float sum = 0.0f;
            for (int k = 0; k <= lookahead; ++k)
                sum += minRing[(size_t) ((pos - (std::uint32_t) k) & mask)];
            g = sum / (float) (lookahead + 1);
            gain = std::min (g, gain + (1.0f - gain) * relCoef);
        }
        else
        {
            minRing[(size_t) (pos & mask)] = 1.0f;
            gain = 1.0f;
        }
        const std::uint32_t rd = (pos - (std::uint32_t) lookahead) & mask;
        const float y0 = juce::jlimit (-ceilingLin, ceilingLin, audio[0][(size_t) rd] * gain);
        const float y1 = juce::jlimit (-ceilingLin, ceilingLin, audio[1][(size_t) rd] * gain);
        if (active)
            minGain = std::min (minGain, gain);
        ++pos;

        // 0 = the untouched, undelayed signal; 1 = the limited, delayed one
        const bool full = b >= 1.0f, off = b <= 0.0f; // exact at both ends (no rounding from the blend)
        ch[0][i] = full ? y0 : off ? in[0] : in[0] + (y0 - in[0]) * b;
        if (nch == 2)
            ch[1][i] = full ? y1 : off ? in[1] : in[1] + (y1 - in[1]) * b;
    }
    if (minGain < 1.0f && s.on)
    {
        const float db = -20.0f * std::log10 (std::max (minGain, 1.0e-4f));
        if (db > grDb.load (std::memory_order_relaxed))
            grDb.store (db, std::memory_order_relaxed);
    }
}
