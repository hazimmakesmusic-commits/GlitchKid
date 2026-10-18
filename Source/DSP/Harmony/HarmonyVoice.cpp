#include "HarmonyVoice.h"
#include <algorithm>
#include <cmath>

namespace glitchkid::harmony
{
namespace
{
constexpr float insane = 1.0e6f;
float clean (float v) { return std::abs (v) < insane ? v : 0.0f; } // false for NaN and +-Inf too
}

void HarmonyVoice::prepare (double sampleRate)
{
    sr = sampleRate;
    voice.prepare (sampleRate);
    latency = voice.getLatencySamples();
    for (auto& r : leadRing)
        r.assign ((size_t) latency, 0.0f);
    gateRing.assign ((size_t) latency, 0.0f);

    applyVoiceSettings();
    reset();
}

void HarmonyVoice::applyVoiceSettings()
{
    pitch::PitchCorrector::Settings p;
    p.fixedShift = true;
    p.fixedShiftSemitones = s.semitones;
    p.mix = 1.0f;           // the voice only; the lead is added here
    p.retuneSpeedMs = 8.0f; // a new interval glides in over a few ms instead of jumping
    p.holdMs = 80.0f;       // brief unvoiced dips (a consonant inside a word) keep the interval
    voice.setSettings (p);
}

void HarmonyVoice::reset()
{
    voice.reset();
    for (auto& r : leadRing)
        std::fill (r.begin(), r.end(), 0.0f);
    std::fill (gateRing.begin(), gateRing.end(), 0.0f);
    pos = 0;
    counter = 0;
    gateTarget = gateNow = 0.0f;
    sinceVoiced = 1 << 30;
    gainNow = gainTarget;
}

void HarmonyVoice::setSettings (const Settings& n)
{
    s.semitones = std::isfinite (n.semitones) ? std::clamp (n.semitones, minSemitones, maxSemitones) : 0.0f;
    s.levelDb = std::isfinite (n.levelDb) ? std::clamp (n.levelDb, -60.0f, 12.0f) : -60.0f;
    gainTarget = std::pow (10.0f, s.levelDb / 20.0f);
    applyVoiceSettings();
}

void HarmonyVoice::process (float* const* channels, int numChannels, int numSamples)
{
    const int nch = std::clamp (numChannels, 1, maxChannels);
    const size_t len = gateRing.size();
    if (len == 0)
        return;

    int done = 0;
    while (done < numSamples)
    {
        // Chunks end on multiples of `chunk` of the absolute sample count, whatever the block size.
        const int n = (int) std::min<long> (numSamples - done, chunk - (counter % chunk));

        for (int c = 0; c < nch; ++c)
            for (int i = 0; i < n; ++i)
                scratch[c][i] = clean (channels[c][done + i]);

        // The delayed lead (and store the new input in its place).
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < nch; ++c)
            {
                leadOut[c][i] = leadRing[c][(pos + (size_t) i) % len];
                leadRing[c][(pos + (size_t) i) % len] = scratch[c][i];
            }
        }

        float* ptrs[maxChannels] = { scratch[0], scratch[1] };
        voice.process (ptrs, nch, n); // scratch now holds the shifted voice, delayed by `latency`


        const float attack = 1.0f - std::exp (-1.0f / (0.003f * (float) sr));
        const float release = 1.0f - std::exp (-1.0f / (0.025f * (float) sr));
        for (int i = 0; i < n; ++i)
        {
            if (sinceVoiced < (1 << 30))
                ++sinceVoiced;
            gateNow += (gateTarget - gateNow) * (gateTarget > gateNow ? attack : release);
            const size_t at = (pos + (size_t) i) % len;
            const float gateOut = gateRing[at]; // the gate from one latency ago
            gateRing[at] = gateNow;
            gainNow += (gainTarget - gainNow) * 0.002f; // ~10 ms at 48 kHz
            const float g = gainNow * gateOut;
            for (int c = 0; c < nch; ++c)
            {
                const float y = leadOut[c][i] + g * clean (scratch[c][i]);
                channels[c][done + i] = clean (y);
            }
        }
        pos = (pos + (size_t) n) % len;
        counter += n;
        done += n;

        // The detector's verdict is read only when a whole chunk of the fixed grid is complete (never mid-chunk), and applies from the NEXT
        // chunk on, so the result is the same for every host block size. One latency later it reaches the matching part of the output.
        if (counter % chunk == 0)
        {
            if (voice.telemetry().voiced)
                sinceVoiced = 0;
            gateTarget = sinceVoiced < (long) (0.04 * sr) ? 1.0f : 0.0f; // 40 ms of hold against flicker
        }
    }
}

void HarmonyVoice::feed (const float* const* channels, int numChannels, int numSamples)
{
    const int nch = std::clamp (numChannels, 1, maxChannels);
    const size_t len = gateRing.size();
    if (len == 0)
        return;
    voice.feed (channels, nch, numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        for (int c = 0; c < nch; ++c)
            leadRing[c][pos] = clean (channels[c][i]);
        gateRing[pos] = 0.0f;
        if (++pos == len)
            pos = 0;
    }
    counter += numSamples;
    gateTarget = gateNow = 0.0f;
    sinceVoiced = 1 << 30;
}
} // namespace glitchkid::harmony
