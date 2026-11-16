#include "AirLab.h"
#include "Finite.h"

namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr float smoothSeconds = 0.02f;
constexpr float presenceMax = 0.9f;   // MID AIR band lift at 100 % and full adaptive depth: about +5.6 dB at its centre
constexpr float midHarmMax = 0.35f;   // MID AIR density (harmonics) at 100 %
constexpr float airLinearMax = 0.45f; // HIGH AIR lift at 100 %: about +3.2 dB far above 8.5 kHz
constexpr float airHarmMax = 0.9f;    // HIGH AIR harmonic path at 100 %
constexpr float knee = 0.9f, kneeRange = 0.1f; // the module's output knee: linear to 0.9, then a smooth approach to 1.0
constexpr int controlBlock = 16;      // the adaptive depths and the gate are recomputed every 16 samples
float envCoef (double fs, double seconds) { return 1.0f - (float) std::exp (-1.0 / (seconds * fs)); }
float db (float x) { return 20.0f * std::log10 (x + 1.0e-9f); }
// 2nd and 3rd harmonics only (Chebyshev T2 and T3 of the envelope-normalised band): no copy of the band itself, which the high-pass after it could not
// fully remove and whose phase would dip the source band. Same harmonic levels as 0.6 x^2 + 0.4 x^3 at full amplitude.
float harmonics (float x) { return 0.3f * (2.0f * x * x - 1.0f) + 0.1f * (4.0f * x * x * x - 3.0f * x); }
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

void AirLab::Channel::clear()
{
    for (auto* b : { &midBp, &midExHp, &midExLp, &midHarmHp, &midHarmLp, &airHp, &exHp, &exLp1, &exLp2, &harmHp1, &harmHp2, &harmLp })
        b->clear();
    exEnv = midExEnv = 0.0f;
}

void AirLab::prepare (double sr)
{
    sampleRate = sr > 8000.0 ? sr : 48000.0;
    for (auto& c : channels)
    {
        c.midBp.bandpass (sampleRate, 3200.0, 0.8);
        c.midExHp.highpass (sampleRate, 1200.0, 0.7071);
        c.midExLp.lowpass (sampleRate, 2400.0, 0.7071);
        c.midHarmHp.highpass (sampleRate, 2400.0, 0.7071);
        c.midHarmLp.lowpass (sampleRate, 6000.0, 0.7071);
        c.airHp.highpass (sampleRate, 8500.0, 0.7071);
        c.exHp.highpass (sampleRate, 2500.0, 0.7071);
        c.exLp1.lowpass (sampleRate, 5500.0, 0.5412); // two Butterworth sections: a 4th-order low-pass (24 dB/oct) so content above the band cannot create harmonics near Nyquist
        c.exLp2.lowpass (sampleRate, 5500.0, 1.3065);
        c.harmHp1.highpass (sampleRate, 7500.0, 0.5412);
        c.harmHp2.highpass (sampleRate, 7500.0, 1.3065); // 7.5 kHz: the intermodulation of the voice's own harmonics stays out of its presence band
        c.harmLp.lowpass (sampleRate, 16000.0, 0.7071);
    }
    sibBp.bandpass (sampleRate, 7500.0, 1.0);
    bodyBp.bandpass (sampleRate, 1000.0, 0.7);
    presDet.bandpass (sampleRate, 3200.0, 0.8);
    airDet.highpass (sampleRate, 9000.0, 0.7071);
    envAttack = envCoef (sampleRate, 0.001);
    envRelease = envCoef (sampleRate, 0.03);
    exAttack = envCoef (sampleRate, 0.005);
    exRelease = envCoef (sampleRate, 0.08);
    duckAttack = envCoef (sampleRate, 0.002);
    duckRelease = envCoef (sampleRate, 0.06);
    fullRelease = envCoef (sampleRate, 0.1);
    peakRelease = envCoef (sampleRate, 2.0);
    const double blockFs = sampleRate / controlBlock;
    dynUp = envCoef (blockFs, 0.03);
    dynDown = envCoef (blockFs, 0.15);
    gateOpen = envCoef (blockFs, 0.005);
    gateClose = envCoef (blockFs, 0.08);
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
    presenceGain.setTargetValue (target.presence * target.mix);
    airLinear.setTargetValue (target.air * airLinearMax * target.mix);
    airHarm.setTargetValue (target.air * airHarmMax * target.mix);
    trimGain.setTargetValue (juce::Decibels::decibelsToGain (target.trimDb));
    onGain.setTargetValue (target.on ? 1.0f : 0.0f);
}

void AirLab::clearState()
{
    for (auto& c : channels)
        c.clear();
    for (auto* b : { &sibBp, &bodyBp, &presDet, &airDet })
        b->clear();
    sibEnv = bodyEnv = presEnv = airEnv = fullEnv = peakEnv = 0.0f;
    duck = 1.0f;
    midDyn = highDyn = 0.6f;
    gate = 0.0f;
    blockLeft = 0;
    midNowDb.store (0.0f, std::memory_order_relaxed);
    highNowDb.store (0.0f, std::memory_order_relaxed);
}

void AirLab::reset()
{
    clearState();
    for (auto* s : { &presenceGain, &airLinear, &airHarm, &trimGain, &onGain })
        s->setCurrentAndTargetValue (s->getTargetValue());
}

void AirLab::process (juce::AudioBuffer<float>& buffer)
{
    const int numChannels = juce::jmin (buffer.getNumChannels(), maxChannels), n = buffer.getNumSamples();
    if (numChannels <= 0 || n <= 0)
        return;

    if (! target.on && ! onGain.isSmoothing() && onGain.getCurrentValue() <= 0.0f)
    {
        clearState(); // settled off: an exact pass-through (nothing is touched); the states are cleared so switching on starts clean
        return;
    }

    float* data[maxChannels] = { buffer.getWritePointer (0), numChannels > 1 ? buffer.getWritePointer (1) : nullptr };
    for (int ch = 0; ch < numChannels; ++ch)
        dspguard::zeroNonFinite (data[ch], n);

    for (int i = 0; i < n; ++i)
    {
        const float pg = presenceGain.getNextValue(), al = airLinear.getNextValue(), ah = airHarm.getNextValue(), tg = trimGain.getNextValue(), on = onGain.getNextValue();

        // detectors on the mono sum: the body, the presence and air bands, S sounds, the level and its recent peak
        float mono = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            mono += data[ch][i];
        mono /= (float) numChannels;
        auto follow = [this] (float& env, float x) { env += (x > env ? envAttack : envRelease) * (x - env); };
        follow (sibEnv, std::abs (sibBp.process (mono)));
        follow (bodyEnv, std::abs (bodyBp.process (mono)));
        follow (presEnv, std::abs (presDet.process (mono)));
        follow (airEnv, std::abs (airDet.process (mono)));
        const float a = std::abs (mono);
        fullEnv += (a > fullEnv ? envAttack : fullRelease) * (a - fullEnv);
        peakEnv = a > peakEnv ? a : peakEnv + peakRelease * (a - peakEnv);
        const float ratio = sibEnv / (bodyEnv + 1.0e-4f);
        const float wanted = sibEnv < 1.0e-4f ? 1.0f : 1.0f / (1.0f + 2.5f * juce::jmax (0.0f, ratio - 0.6f)); // quiet highs are never ducked
        duck += (wanted < duck ? duckAttack : duckRelease) * (wanted - duck);

        // every 16 samples: how much each lift is wanted now (adaptive depth) and whether the voice is there at all (gate)
        if (--blockLeft <= 0)
        {
            blockLeft = controlBlock;
            const float dp = db (presEnv) - db (bodyEnv), da = db (airEnv) - db (bodyEnv);
            const float midT = juce::jlimit (0.25f, 1.0f, 0.6f + (-6.0f - dp) / 12.0f);  // a weak presence band is lifted more, a harsh one less
            const float highT = juce::jlimit (0.25f, 1.0f, 0.6f + (-28.0f - da) / 16.0f); // a dark top more, an already bright one less
            midDyn += (midT > midDyn ? dynUp : dynDown) * (midT - midDyn);
            highDyn += (highT > highDyn ? dynUp : dynDown) * (highT - highDyn);
            const float floorDb = juce::jmax (db (peakEnv) - 30.0f, -60.0f);
            const float gateT = juce::jlimit (0.0f, 1.0f, (db (fullEnv) - floorDb + 6.0f) / 12.0f);
            gate += (gateT > gate ? gateOpen : gateClose) * (gateT - gate);
            const float duckMid = 1.0f - 0.5f * (1.0f - duck);
            midNowDb.store (db (1.0f + pg * presenceMax * midDyn * duckMid * gate * on), std::memory_order_relaxed);
            highNowDb.store (db (1.0f + al * highDyn * duck * gate * on), std::memory_order_relaxed);
        }
        const float duckMid = 1.0f - 0.5f * (1.0f - duck);
        const float midAmt = pg * gate * duckMid * on, highAmt = gate * duck * on;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& c = channels[ch];
            const float dry = data[ch][i];

            // MID AIR: the adaptive presence lift and a little density from the 1.2-2.4 kHz band
            const float presence = c.midBp.process (dry);
            const float midBand = c.midExLp.process (c.midExHp.process (dry));
            const float mmag = std::abs (midBand);
            c.midExEnv += (mmag > c.midExEnv ? exAttack : exRelease) * (mmag - c.midExEnv);
            const float mn = juce::jlimit (-2.0f, 2.0f, midBand / (c.midExEnv + 1.0e-3f));
            const float midHarm = c.midHarmLp.process (c.midHarmHp.process (harmonics (mn) * c.midExEnv));

            // HIGH AIR: the adaptive air lift and harmonics from the 2.5-5.5 kHz band
            const float sparkle = c.airHp.process (dry);
            const float band = c.exLp2.process (c.exLp1.process (c.exHp.process (dry)));
            const float mag = std::abs (band);
            c.exEnv += (mag > c.exEnv ? exAttack : exRelease) * (mag - c.exEnv);
            const float nrm = juce::jlimit (-2.0f, 2.0f, band / (c.exEnv + 1.0e-3f));
            const float harm = c.harmLp.process (c.harmHp2.process (c.harmHp1.process (harmonics (nrm) * c.exEnv)));

            const float added = midAmt * (presenceMax * midDyn * presence + midHarmMax * midHarm) + highAmt * (al * highDyn * sparkle + ah * harm);
            const float trim = 1.0f + (tg - 1.0f) * on;
            float y = (dry + added) * trim;

            // keep the module from pushing a signal that was below full scale past it (never reduces a signal that is already louder)
            const float ay = std::abs (y);
            if (ay > knee && ay > std::abs (dry))
            {
                const float limited = knee + kneeRange * std::tanh ((ay - knee) / kneeRange);
                y = std::copysign (juce::jmax (limited, std::abs (dry)), y);
            }
            data[ch][i] = y;
        }
    }
}
