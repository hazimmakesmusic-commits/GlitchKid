#include "PitchCorrectorPrototype.h"
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

void PitchCorrectorPrototype::prepare (double sampleRate, float lowestF0Hz, float highestF0Hz)
{
    sr = sampleRate;
    detector.prepare (sampleRate, lowestF0Hz, highestF0Hz);
    shifter.prepare (sampleRate, lowestF0Hz);
    dryDelay.assign ((size_t) shifter.getLatencySamples(), 0.0f); // read-then-write: delay = size
    reset();
}

void PitchCorrectorPrototype::reset()
{
    detector.reset();
    shifter.reset();
    std::fill (dryDelay.begin(), dryDelay.end(), 0.0f);
    dryPos = 0;
    numPeriods = 0;
    periodNow = 0.0f;
    targetNote = -1.0f;
    targetCorrSemis = corrSemis = 0.0f;
    samplesSinceVoiced = 1 << 30;
    mixNow = s.bypass ? 0.0f : s.mix;
    tel = Telemetry();
}

void PitchCorrectorPrototype::setSettings (const Settings& n)
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

void PitchCorrectorPrototype::onNewEstimate()
{
    const auto& r = detector.result();
    tel.detectedHz = r.frequencyHz;
    tel.confidence = r.confidence;
    tel.voiced = r.voiced && r.confidence >= s.minConfidence;

    if (tel.voiced)
    {
        samplesSinceVoiced = 0;
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
        targetNote = ScaleQuantizer::select (midi, s.key, s.scale, targetNote);
        targetCorrSemis = (targetNote - midi) * s.strength;
    }
    else if (samplesSinceVoiced > (long) (s.holdMs * 0.001 * sr))
    {
        targetCorrSemis = 0.0f; // voice gone: let the correction relax to nothing
        targetNote = -1.0f;
        numPeriods = 0;
    }
    tel.targetMidi = targetNote;
}

float PitchCorrectorPrototype::process (float x)
{
    if (! (std::abs (x) < insane))
        x = 0.0f;

    if (samplesSinceVoiced < (1 << 30))
        ++samplesSinceVoiced;
    if (detector.push (x))
        onNewEstimate();

    // Retune speed: one-pole glide of the correction (in semitones) towards its target.
    const float coef = s.retuneSpeedMs <= 0.0f ? 1.0f
                                               : 1.0f - std::exp (-1.0f / (s.retuneSpeedMs * 0.001f * (float) sr));
    corrSemis += (targetCorrSemis - corrSemis) * coef;
    tel.correctionCents = corrSemis * 100.0f;

    if (periodNow > 0.0f)
        shifter.setPeriod (periodNow);
    shifter.setRatio (std::exp2 (corrSemis / 12.0f));
    const float wet = shifter.process (x);

    const float dry = dryDelay[dryPos];
    dryDelay[dryPos] = x;
    if (++dryPos == dryDelay.size())
        dryPos = 0;

    const float mixTarget = s.bypass ? 0.0f : s.mix;
    mixNow += (mixTarget - mixNow) * 0.002f; // about 10 ms at 48 kHz
    if (std::abs (mixTarget - mixNow) < 1.0e-6f)
        mixNow = mixTarget;

    const float y = mixNow <= 0.0f ? dry : dry + (wet - dry) * mixNow;
    return std::abs (y) < insane ? y : 0.0f;
}

void PitchCorrectorPrototype::processBlock (const float* in, float* out, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
        out[i] = process (in[i]);
}
} // namespace glitchkid::pitch
