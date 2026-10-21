#include "AirLab.h"
#include "Finite.h"

namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr float smoothSeconds = 0.02f;
constexpr float presenceMax = 0.9f;   // added band-pass gain at 100 %: about +5.6 dB at the centre
constexpr float airLinearMax = 0.45f; // added high-pass gain at 100 %: about +3.2 dB far above 9 kHz
constexpr float airHarmMax = 0.9f;    // harmonic path gain at 100 %
constexpr float knee = 0.9f, kneeRange = 0.1f; // the module's output knee: linear to 0.9, then a smooth approach to 1.0
float envCoef (double fs, double seconds) { return 1.0f - (float) std::exp (-1.0 / (seconds * fs)); }
}

void AirLab::Biquad::lowpass (double fs, double f, double q)
{
    const double w0 = 2.0 * pi * juce::jmin (f, 0.45 * fs) / fs, c = std::cos (w0), s = std::sin (w0), alpha = s / (2.0 * q), a0 = 1.0 + alpha;
    b0 = (1.0 - c) * 0.5 / a0;  b1 = (1.0 - c) / a0;  b2 = b0;
    a1 = -2.0 * c / a0;         a2 = (1.0 - alpha) / a0;
}

void AirLab::Biquad::highpass (double fs, double f, double q)
{
    const double w0 = 2.0 * pi * juce::jmin (f, 0.45 * fs) / fs, c = std::cos (w0), s = std::sin (w0), alpha = s / (2.0 * q), a0 = 1.0 + alpha;
    b0 = (1.0 + c) * 0.5 / a0;  b1 = -(1.0 + c) / a0;  b2 = b0;
    a1 = -2.0 * c / a0;         a2 = (1.0 - alpha) / a0;
}

void AirLab::Biquad::bandpass (double fs, double f, double q)
{
    const double w0 = 2.0 * pi * juce::jmin (f, 0.45 * fs) / fs, c = std::cos (w0), s = std::sin (w0), alpha = s / (2.0 * q), a0 = 1.0 + alpha;
    b0 = alpha / a0;  b1 = 0.0;  b2 = -alpha / a0;
    a1 = -2.0 * c / a0;  a2 = (1.0 - alpha) / a0;
}

void AirLab::prepare (double sr)
{
    sampleRate = sr > 8000.0 ? sr : 48000.0;
    for (auto& c : channels)
    {
        c.presenceBp.bandpass (sampleRate, 3400.0, 0.8);
        c.airHp.highpass (sampleRate, 9000.0, 0.7071);
        c.exHp.highpass (sampleRate, 2500.0, 0.7071);
        c.exLp1.lowpass (sampleRate, 5500.0, 0.5412); // two Butterworth sections: a 4th-order low-pass (24 dB/oct) so content above the band cannot create harmonics near Nyquist
        c.exLp2.lowpass (sampleRate, 5500.0, 1.3065);
        c.harmHp1.highpass (sampleRate, 6000.0, 0.5412);
        c.harmHp2.highpass (sampleRate, 6000.0, 1.3065);
        c.harmLp.lowpass (sampleRate, 16000.0, 0.7071);
    }
    sibBp.bandpass (sampleRate, 7500.0, 1.0);
    bodyBp.bandpass (sampleRate, 1000.0, 0.7);
    envAttack = envCoef (sampleRate, 0.001);
    envRelease = envCoef (sampleRate, 0.03);
    exAttack = envCoef (sampleRate, 0.005);
    exRelease = envCoef (sampleRate, 0.08);
    duckAttack = envCoef (sampleRate, 0.002);
    duckRelease = envCoef (sampleRate, 0.06);
    for (auto* s : { &presenceGain, &airLinear, &airHarm, &trimGain, &onGain })
        s->reset (sampleRate, smoothSeconds);
    reset();
}

void AirLab::setSettings (const Settings& s)
{
    target = s;
    target.presence = juce::jlimit (0.0f, 1.0f, std::isfinite (s.presence) ? s.presence : 0.0f);
    target.air = juce::jlimit (0.0f, 1.0f, std::isfinite (s.air) ? s.air : 0.0f);
    target.mix = juce::jlimit (0.0f, 1.0f, std::isfinite (s.mix) ? s.mix : 0.0f);
    target.trimDb = juce::jlimit (-12.0f, 12.0f, std::isfinite (s.trimDb) ? s.trimDb : 0.0f);
    presenceGain.setTargetValue (target.presence * presenceMax * target.mix);
    airLinear.setTargetValue (target.air * airLinearMax * target.mix);
    airHarm.setTargetValue (target.air * airHarmMax * target.mix);
    trimGain.setTargetValue (juce::Decibels::decibelsToGain (target.trimDb));
    onGain.setTargetValue (target.on ? 1.0f : 0.0f);
}

void AirLab::reset()
{
    for (auto& c : channels)
    {
        c.presenceBp.clear(); c.airHp.clear(); c.exHp.clear(); c.exLp1.clear(); c.exLp2.clear();
        c.harmHp1.clear(); c.harmHp2.clear(); c.harmLp.clear();
        c.exEnv = 0.0f;
    }
    sibBp.clear();
    bodyBp.clear();
    sibEnv = bodyEnv = 0.0f;
    duck = 1.0f;
    presenceGain.setCurrentAndTargetValue (presenceGain.getTargetValue());
    airLinear.setCurrentAndTargetValue (airLinear.getTargetValue());
    airHarm.setCurrentAndTargetValue (airHarm.getTargetValue());
    trimGain.setCurrentAndTargetValue (trimGain.getTargetValue());
    onGain.setCurrentAndTargetValue (onGain.getTargetValue());
}

void AirLab::process (juce::AudioBuffer<float>& buffer)
{
    const int numChannels = juce::jmin (buffer.getNumChannels(), maxChannels), n = buffer.getNumSamples();
    if (numChannels <= 0 || n <= 0)
        return;

    if (! target.on && ! onGain.isSmoothing() && onGain.getCurrentValue() <= 0.0f)
    {
        // settled off: an exact pass-through (nothing is touched); the states are cleared so switching on starts clean
        for (auto& c : channels)
        {
            c.presenceBp.clear(); c.airHp.clear(); c.exHp.clear(); c.exLp1.clear(); c.exLp2.clear();
            c.harmHp1.clear(); c.harmHp2.clear(); c.harmLp.clear();
            c.exEnv = 0.0f;
        }
        sibBp.clear(); bodyBp.clear();
        sibEnv = bodyEnv = 0.0f;
        duck = 1.0f;
        return;
    }

    float* data[maxChannels] = { buffer.getWritePointer (0), numChannels > 1 ? buffer.getWritePointer (1) : nullptr };
    for (int ch = 0; ch < numChannels; ++ch)
        dspguard::zeroNonFinite (data[ch], n);

    for (int i = 0; i < n; ++i)
    {
        const float pg = presenceGain.getNextValue(), al = airLinear.getNextValue(), ah = airHarm.getNextValue(), tg = trimGain.getNextValue(), on = onGain.getNextValue();

        // sibilance detector on the mono sum
        float mono = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            mono += data[ch][i];
        mono /= (float) numChannels;
        const float s = std::abs (sibBp.process (mono)), b = std::abs (bodyBp.process (mono));
        sibEnv += (s > sibEnv ? envAttack : envRelease) * (s - sibEnv);
        bodyEnv += (b > bodyEnv ? envAttack : envRelease) * (b - bodyEnv);
        const float ratio = sibEnv / (bodyEnv + 1.0e-4f);
        const float wanted = sibEnv < 1.0e-4f ? 1.0f : 1.0f / (1.0f + 2.5f * juce::jmax (0.0f, ratio - 0.6f)); // quiet highs are never ducked
        duck += (wanted < duck ? duckAttack : duckRelease) * (wanted - duck);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& c = channels[ch];
            const float dry = data[ch][i];

            const float presence = c.presenceBp.process (dry);
            const float sparkle = c.airHp.process (dry);

            // harmonics from the 2.5-5.5 kHz band
            const float band = c.exLp2.process (c.exLp1.process (c.exHp.process (dry)));
            const float mag = std::abs (band);
            c.exEnv += (mag > c.exEnv ? exAttack : exRelease) * (mag - c.exEnv);
            const float nrm = juce::jlimit (-2.0f, 2.0f, band / (c.exEnv + 1.0e-3f));
            const float shaped = (0.6f * nrm * nrm + 0.4f * nrm * nrm * nrm) * c.exEnv;
            const float harm = c.harmLp.process (c.harmHp2.process (c.harmHp1.process (shaped)));

            const float added = (pg * presence + al * sparkle + ah * harm) * duck * on;
            const float trim = 1.0f + (tg - 1.0f) * on;
            float y = (dry + added) * trim;

            // keep the module from pushing a signal that was below full scale past it (never reduces a signal that is already louder)
            const float a = std::abs (y);
            if (a > knee && a > std::abs (dry))
            {
                const float limited = knee + kneeRange * std::tanh ((a - knee) / kneeRange);
                y = std::copysign (juce::jmax (limited, std::abs (dry)), y);
            }
            data[ch][i] = y;
        }
    }
}
