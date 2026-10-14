#include "GlitchStutter.h"

namespace
{
constexpr double maxSliceSeconds = 1.5; // 1/4 note at 40 BPM
constexpr double crossfadeSeconds = 0.004;
constexpr float flatMixPercent = 0.05f; // half of the 0.1 % parameter step
}

double GlitchStutter::rateInBeats (int rateIndex)
{
    static constexpr double beats[numRates] = { 1.0, 0.5, 0.25, 0.125 };
    return beats[juce::jlimit (0, numRates - 1, rateIndex)];
}

float GlitchStutter::resolveTempo (bool hostHasTempo, double hostBpm, bool& usedHost)
{
    usedHost = hostHasTempo && std::isfinite (hostBpm) && hostBpm >= 20.0 && hostBpm <= 500.0;
    return usedHost ? juce::jlimit (minBpm, maxBpm, (float) hostBpm) : fallbackBpm;
}

void GlitchStutter::prepare (double sr)
{
    sampleRate = sr;
    maxLength = (int) std::ceil (maxSliceSeconds * sr) + 1;
    maxCrossfade = (int) std::ceil (crossfadeSeconds * sr) + 1;
    ringSize = maxLength + 2 * maxCrossfade + 8;
    for (int ch = 0; ch < 2; ++ch)
    {
        ring[ch].assign ((size_t) ringSize, 0.0f);
        frozen[ch].assign ((size_t) (maxLength + maxCrossfade), 0.0f);
    }
    ramp.reset (sr, 0.003);
    mixAmount.reset (sr, 0.02);
    reset();
}

void GlitchStutter::reset()
{
    for (int ch = 0; ch < 2; ++ch)
        std::fill (ring[ch].begin(), ring[ch].end(), 0.0f);
    writePos = 0;
    state = State::idle;
    ramp.setCurrentAndTargetValue (0.0f);
    loopLength = playPos = 0;
}

void GlitchStutter::setSettings (const StutterSettings& s)
{
    const bool mixOff = s.mixPercent < flatMixPercent;
    wantOn = s.on && ! mixOff;
    wantedRate = juce::jlimit (0, numRates - 1, s.rateIndex);
    wantedBpm = std::isfinite (s.bpm) ? juce::jlimit (minBpm, maxBpm, s.bpm) : fallbackBpm;
    mixAmount.setTargetValue (mixOff ? 0.0f : (s.mixPercent >= 100.0f - flatMixPercent ? 1.0f : s.mixPercent * 0.01f));
}

int GlitchStutter::lengthFor (int rateIndex, float bpm) const
{
    const double seconds = rateInBeats (rateIndex) * 60.0 / (double) bpm;
    return juce::jlimit (64, maxLength, (int) std::lround (seconds * sampleRate));
}

// Copy the last `loopLength` samples of history (plus the crossfade lead-in before them) into `frozen`.
void GlitchStutter::capture (int rateIndex, float bpm)
{
    loopLength = lengthFor (rateIndex, bpm);
    crossfade = juce::jmin ((int) std::lround (crossfadeSeconds * sampleRate), loopLength / 4);
    loopRate = rateIndex;
    loopBpm = bpm;
    playPos = 0;

    const int total = loopLength + crossfade;
    for (int ch = 0; ch < 2; ++ch)
        for (int k = 0; k < total; ++k)
        {
            int idx = writePos - total + k;
            if (idx < 0)
                idx += ringSize;
            frozen[ch][(size_t) k] = ring[ch][(size_t) idx];
        }
}

// frozen = [lead-in (crossfade samples)][slice (loopLength samples)]. Near the end of the slice we fade
// toward the lead-in, which is the audio that preceded the slice, so the wrap back to the start is smooth.
float GlitchStutter::loopSample (int ch) const
{
    const auto& f = frozen[ch];
    const int p = playPos;
    const float slice = f[(size_t) (crossfade + p)];
    const int tail = loopLength - crossfade;
    if (p < tail)
        return slice;
    const float t = (float) (p - tail) / (float) crossfade;
    return (1.0f - t) * slice + t * f[(size_t) (p - tail)];
}

void GlitchStutter::process (juce::AudioBuffer<float>& buffer)
{
    const int numChannels = juce::jmin (buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();
    float* data[2] = { buffer.getWritePointer (0), numChannels > 1 ? buffer.getWritePointer (1) : nullptr };

    for (int i = 0; i < numSamples; ++i)
    {
        switch (state)
        {
            case State::idle:
                if (wantOn)
                {
                    capture (wantedRate, wantedBpm);
                    ramp.setTargetValue (1.0f);
                    state = State::active;
                }
                break;

            case State::active:
                if (! wantOn || wantedRate != loopRate || std::abs (wantedBpm - loopBpm) > 0.02f * loopBpm)
                {
                    ramp.setTargetValue (0.0f);
                    state = State::releasing;
                }
                break;

            case State::releasing:
                if (! ramp.isSmoothing())
                    state = State::idle; // a pending retrigger starts on the next sample
                break;
        }

        const float weight = ramp.getNextValue() * mixAmount.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float live = data[ch][i];
            if (state != State::idle)
                data[ch][i] = live + (loopSample (ch) - live) * weight;
            ring[ch][(size_t) writePos] = live; // the history records the stage input, never its output
        }

        if (state != State::idle && ++playPos >= loopLength)
            playPos = 0;
        if (++writePos >= ringSize)
            writePos = 0;
    }
}
