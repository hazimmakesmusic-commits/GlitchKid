#include "PitchCorrector.h"
#include <algorithm>
#include <cmath>

namespace glitchkid::pitch
{
namespace
{
constexpr float insane = 1.0e6f;

float median3 (float a, float b, float c)
{
    return std::max (std::min (a, b), std::min (std::max (a, b), c));
}
}

void PitchCorrector::prepare (double sampleRate, float lowestF0Hz, float highestF0Hz)
{
    sr = sampleRate;
    detector.prepare (sampleRate, lowestF0Hz, highestF0Hz);
    for (auto& sh : shifters)
        sh.prepare (sampleRate, lowestF0Hz);
    for (auto& d : dryDelay)
        d.assign ((size_t) shifters[0].getLatencySamples(), 0.0f); // read-then-write: delay = size
    reset();
}

void PitchCorrector::resetDecisionState()
{
    numPeriods = 0;
    periodNow = 0.0f;
    targetNote = -1.0f;
    targetCorrSemis = corrSemis = 0.0f;
    samplesSinceVoiced = 1 << 30;
    tel = Telemetry();
}

void PitchCorrector::reset()
{
    detector.reset();
    detector.setAnalysisEnabled (true);
    for (auto& sh : shifters)
        sh.reset();
    for (auto& d : dryDelay)
        std::fill (d.begin(), d.end(), 0.0f);
    dryPos = 0;
    idle = false;
    mixNow = s.bypass ? 0.0f : s.mix;
    resetDecisionState();
}

void PitchCorrector::setSettings (const Settings& n)
{
    s = n;
    s.key = ((s.key % 12) + 12) % 12;
    s.strength = std::isfinite (s.strength) ? std::clamp (s.strength, 0.0f, 1.0f) : 0.0f;
    s.mix = std::isfinite (s.mix) ? std::clamp (s.mix, 0.0f, 1.0f) : 0.0f;
    s.retuneSpeedMs = std::isfinite (s.retuneSpeedMs) ? std::clamp (s.retuneSpeedMs, 0.0f, 2000.0f) : 20.0f;
    s.holdMs = std::isfinite (s.holdMs) ? std::clamp (s.holdMs, 0.0f, 1000.0f) : 60.0f;
    s.minConfidence = std::isfinite (s.minConfidence) ? std::clamp (s.minConfidence, 0.0f, 1.0f) : 0.85f;
    if (! (s.refA4Hz > 300.0f && s.refA4Hz < 600.0f))
        s.refA4Hz = 440.0f;
}

void PitchCorrector::onNewEstimate()
{
    const auto& r = detector.result();
    tel.detectedHz = r.frequencyHz;
    tel.confidence = r.confidence;
    tel.voiced = r.voiced && r.confidence >= s.minConfidence;

    if (tel.voiced)
    {
        samplesSinceVoiced = 0;
        if (numPeriods == 0) // a new voiced stretch: line the grains up with the real period from here
            for (auto& sh : shifters)
            {
                sh.setPeriod (r.periodSamples);
                sh.reanchor();
            }
        if (numPeriods < 3)
            periods[numPeriods++] = r.periodSamples;
        else
        {
            periods[0] = periods[1];
            periods[1] = periods[2];
            periods[2] = r.periodSamples;
        }
        periodNow = numPeriods == 3 ? median3 (periods[0], periods[1], periods[2]) : r.periodSamples;

        const float hz = (float) (sr / periodNow);
        const float midi = 69.0f + 12.0f * std::log2 (hz / s.refA4Hz);
        tel.detectedMidi = midi;
        targetNote = ScaleQuantizer::select (midi, s.key, s.scale, targetNote);
        targetCorrSemis = (targetNote - midi) * s.strength;
    }
    else if (samplesSinceVoiced > (long) (s.holdMs * 0.001 * sr))
    {
        targetCorrSemis = 0.0f; // voice gone: let the correction relax to nothing
        targetNote = -1.0f;
        numPeriods = 0;
        tel.detectedMidi = -1.0f;
    }
    tel.targetMidi = targetNote;
}

void PitchCorrector::process (float* const* channels, int numChannels, int numSamples)
{
    const int nch = std::clamp (numChannels, 1, maxChannels);
    if (idle) // coming out of idle: forget any decision made before the pause
    {
        resetDecisionState();
        detector.setAnalysisEnabled (true);
        idle = false;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        float x[maxChannels] = { 0.0f, 0.0f };
        float sum = 0.0f;
        for (int c = 0; c < nch; ++c)
        {
            float v = channels[c][i];
            if (! (std::abs (v) < insane)) // false for NaN and +-Inf too
                v = 0.0f;
            x[c] = v;
            sum += v;
        }

        if (samplesSinceVoiced < (1 << 30))
            ++samplesSinceVoiced;
        if (detector.push (sum / (float) nch))
            onNewEstimate();

        // Retune speed: one-pole glide of the correction (in semitones) towards its target.
        const float coef = s.retuneSpeedMs <= 0.0f ? 1.0f
                                                   : 1.0f - std::exp (-1.0f / (s.retuneSpeedMs * 0.001f * (float) sr));
        corrSemis += (targetCorrSemis - corrSemis) * coef;
        tel.correctionCents = corrSemis * 100.0f;

        const float ratio = std::exp2 (corrSemis / 12.0f);
        const float mixTarget = s.bypass ? 0.0f : s.mix;
        mixNow += (mixTarget - mixNow) * 0.002f; // about 10 ms at 48 kHz
        if (std::abs (mixTarget - mixNow) < 1.0e-6f)
            mixNow = mixTarget;

        for (int c = 0; c < nch; ++c)
        {
            auto& sh = shifters[c];
            if (periodNow > 0.0f)
                sh.setPeriod (periodNow);
            sh.setRatio (ratio);
            const float wet = sh.process (x[c]);

            const float dry = dryDelay[c][dryPos];
            dryDelay[c][dryPos] = x[c];

            const float y = mixNow <= 0.0f ? dry : dry + (wet - dry) * mixNow;
            channels[c][i] = std::abs (y) < insane ? y : 0.0f;
        }
        if (++dryPos == dryDelay[0].size())
            dryPos = 0;
    }
}

void PitchCorrector::processBlock (const float* in, float* out, int numSamples)
{
    if (in != out)
        std::copy (in, in + numSamples, out);
    float* ch[1] = { out };
    process (ch, 1, numSamples);
}

void PitchCorrector::feed (const float* const* channels, int numChannels, int numSamples)
{
    const int nch = std::clamp (numChannels, 1, maxChannels);
    if (! idle)
    {
        for (int c = 0; c < nch; ++c)
            shifters[c].flush();
        detector.setAnalysisEnabled (false);
        idle = true;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        float x[maxChannels] = { 0.0f, 0.0f };
        float sum = 0.0f;
        for (int c = 0; c < nch; ++c)
        {
            float v = channels[c][i];
            if (! (std::abs (v) < insane))
                v = 0.0f;
            x[c] = v;
            sum += v;
        }
        detector.push (sum / (float) nch);
        for (int c = 0; c < nch; ++c)
        {
            shifters[c].feed (x[c]);
            dryDelay[c][dryPos] = x[c];
        }
        if (++dryPos == dryDelay[0].size())
            dryPos = 0;
    }
}
} // namespace glitchkid::pitch
