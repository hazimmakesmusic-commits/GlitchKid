#include "MasteringEngine.h"

namespace glitchkid::master
{
const char* styleName (Style s)
{
    switch (s)
    {
        case Style::cleanBalanced:   return "Clean & Balanced";
        case Style::warmSmooth:      return "Warm & Smooth";
        case Style::loudPunchy:      return "Loud & Punchy";
        case Style::wideAtmospheric: return "Wide & Atmospheric";
    }
    return "";
}

const char* styleDescription (Style s)
{
    switch (s)
    {
        case Style::cleanBalanced:   return "Transparent: a little low-mid tidy-up, a touch of air, gentle 1.5:1 glue, no colour.";
        case Style::warmSmooth:      return "Fuller lows, softer highs, slow 2:1 glue and light saturation for warmth.";
        case Style::loudPunchy:      return "Tighter low-mids, more bite, 2.5:1 glue with a slower attack so drums still hit, extra drive into the limiter.";
        case Style::wideAtmospheric: return "Open air on top and a wider side (never below 120 Hz), with soft glue.";
    }
    return "";
}

namespace
{
// A style at full intensity: EQ (low shelf, bell, bell, high shelf), compressor (ratio, attack, release, threshold in LU above the mix's integrated loudness),
// saturation, width and extra limiter drive. Moderate on purpose: mastering moves are small.
struct Recipe { float lowHz, lowDb, b1Hz, b1Db, b1Q, b2Hz, b2Db, b2Q, highHz, highDb; float ratio, attackMs, releaseMs, aboveLu; float satDb, width, extraDriveDb, limRelMs; };
constexpr Recipe recipes[numStyles] = {
    {  90.0f, 0.5f, 300.0f, -1.0f, 0.7f, 3200.0f,  0.0f, 0.7f, 10000.0f,  1.0f, 1.5f, 30.0f, 150.0f, 5.0f, 0.0f, 1.0f, 0.0f, 120.0f },
    { 120.0f, 1.5f, 400.0f,  0.0f, 0.7f, 3500.0f, -1.0f, 0.8f,  9000.0f, -0.5f, 2.0f, 40.0f, 250.0f, 4.0f, 2.5f, 1.0f, 0.0f, 200.0f },
    {  70.0f, 1.0f, 300.0f, -1.5f, 0.9f, 2500.0f,  1.0f, 0.7f, 10000.0f,  1.5f, 2.5f, 20.0f, 100.0f, 3.0f, 1.5f, 1.0f, 2.0f,  60.0f },
    { 100.0f, 0.5f, 350.0f, -1.0f, 0.7f, 5000.0f,  0.5f, 0.7f, 12000.0f,  2.0f, 1.6f, 40.0f, 200.0f, 5.0f, 0.5f, 1.3f, 0.0f, 150.0f } };
}

Chain resolve (const Controls& c)
{
    const auto& r = recipes[juce::jlimit (0, numStyles - 1, (int) c.style)];
    const float i = juce::jlimit (0.0f, 1.0f, c.intensity), tone = juce::jlimit (-1.0f, 1.0f, c.tone);
    Chain ch;
    // tone tilts around ~1 kHz: up to +-2.5 dB on the high shelf and -+1.5 dB on the low shelf; each shelf stays within +-4 dB in total
    ch.eq[0] = { r.lowHz, juce::jlimit (-4.0f, 4.0f, r.lowDb * i - 1.5f * tone), 0.7071f };
    ch.eq[1] = { r.b1Hz, r.b1Db * i, r.b1Q };
    ch.eq[2] = { r.b2Hz, r.b2Db * i, r.b2Q };
    ch.eq[3] = { r.highHz, juce::jlimit (-4.0f, 4.0f, r.highDb * i + 2.5f * tone), 0.7071f };
    ch.comp.ratio = 1.0f + (r.ratio - 1.0f) * i;
    ch.comp.attackMs = r.attackMs;
    ch.comp.releaseMs = r.releaseMs;
    ch.comp.kneeDb = 6.0f;
    ch.comp.makeupDb = 0.0f;
    ch.comp.thresholdDb = r.aboveLu;
    ch.satDriveDb = r.satDb * i;
    ch.width = juce::jlimit (0.0f, 2.0f, (1.0f + (r.width - 1.0f) * i) * juce::jlimit (0.0f, 2.0f, c.width));
    ch.driveDb = juce::jlimit (0.0f, 12.0f, juce::jlimit (0.0f, 10.0f, c.loudnessDb) + r.extraDriveDb * i);
    ch.ceilingDbTp = -1.0f; // fixed: leaves room for lossy encoding; never a setting that can be pushed to 0
    ch.releaseMs = r.limRelMs;
    ch.outputDb = juce::jlimit (-12.0f, 0.0f, c.outputDb);
    return ch;
}

void MasteringEngine::prepare (double sampleRate, int maxBlockSize)
{
    fs = sampleRate > 0.0 ? sampleRate : 48000.0;
    eq.prepare (fs);
    comp.prepare (fs);
    sat.prepare (fs, maxBlockSize);
    width.prepare (fs);
    limiter.prepare (fs);
    for (auto* m : { &inMeter, &outMeter, &wetMeter })
        m->prepare (fs);
    dryDelay.prepare (latencySamples());
    dry.setSize (2, juce::jmax (1, maxBlockSize), false, false, true);
    for (auto* v : { &wet, &trim, &match })
        v->reset (fs, 0.02);
    setControls (controls);
    reset();
}

void MasteringEngine::reset()
{
    eq.reset();
    comp.reset();
    sat.reset();
    width.reset();
    limiter.reset();
    dryDelay.reset();
    resetMeters();
    wet.setCurrentAndTargetValue (compareOriginal ? 0.0f : 1.0f);
    trim.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (chain.outputDb));
    match.setCurrentAndTargetValue (1.0f);
}

void MasteringEngine::setControls (const Controls& c)
{
    controls = c;
    chain = resolve (c);
    for (int b = 0; b < 4; ++b)
        eq.setBand (b, chain.eq[(size_t) b]);
    sat.setDriveDb (chain.satDriveDb);
    width.setWidth (chain.width);
    limiter.setSettings (chain.driveDb, chain.ceilingDbTp, chain.releaseMs);
    trim.setTargetValue (juce::Decibels::decibelsToGain (chain.outputDb));
}

void MasteringEngine::process (juce::AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples(), chs = juce::jmin (2, buffer.getNumChannels());
    if (n <= 0 || chs <= 0)
        return;
    float* L = buffer.getWritePointer (0);
    float* R = chs > 1 ? buffer.getWritePointer (1) : nullptr;
    inMeter.process (L, R, n);
    float blockPeak = 0.0f;
    for (int ch = 0; ch < chs; ++ch)
        blockPeak = juce::jmax (blockPeak, buffer.getMagnitude (ch, 0, n));
    inputPeakHold = juce::jmax (blockPeak, inputPeakHold * (float) std::exp (-(double) n / fs)); // about a 1 s hold

    // ORIGINAL: the untouched input, delayed by exactly the chain's latency
    for (int ch = 0; ch < chs; ++ch)
        dry.copyFrom (ch, 0, buffer, ch, 0, n);
    juce::AudioBuffer<float> dryView (dry.getArrayOfWritePointers(), chs, n);
    dryDelay.process (dryView);

    // the glue compressor's threshold follows the mix's own loudness (before the first 400 ms: an assumed -18 LUFS)
    const float lufs = inMeter.integratedLufs();
    auto cs = chain.comp;
    cs.thresholdDb = juce::jlimit (-40.0f, -3.0f, (lufs > -70.0f ? lufs : -18.0f) + chain.comp.thresholdDb);
    comp.setSettings (cs);

    juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), chs, n);
    eq.process (view);
    comp.process (view);
    compGr.store (comp.getGainReductionDb(), std::memory_order_relaxed);
    sat.process (view);
    if (R != nullptr)
        width.process (L, R, n);
    limiter.process (L, R, n);
    for (int i = 0; i < n; ++i)
    {
        const float g = trim.getNextValue();
        L[i] *= g;
        if (R != nullptr) R[i] *= g;
    }
    wetMeter.process (L, R, n);

    // the loudness match for ORIGINAL: master short-term loudness minus input short-term loudness (both K-weighted, BS.1770)
    const float wetSt = wetMeter.shortTermLufs(), inSt = inMeter.shortTermLufs();
    if (wetSt > -70.0f && inSt > -70.0f)
        matchDb.store (juce::jlimit (-24.0f, 24.0f, wetSt - inSt), std::memory_order_relaxed);
    const float ceilingLin = juce::Decibels::decibelsToGain (chain.ceilingDbTp);
    match.setTargetValue (juce::jmin (juce::Decibels::decibelsToGain (matchDb.load (std::memory_order_relaxed)), ceilingLin / juce::jmax (1.0e-6f, inputPeakHold)));
    wet.setTargetValue (compareOriginal ? 0.0f : 1.0f);
    if (wet.isSmoothing() || wet.getTargetValue() < 1.0f)
        for (int i = 0; i < n; ++i)
        {
            const float w = wet.getNextValue(), m = match.getNextValue() * (1.0f - w);
            L[i] = L[i] * w + juce::jlimit (-ceilingLin, ceilingLin, dry.getSample (0, i) * m);
            if (R != nullptr) R[i] = R[i] * w + juce::jlimit (-ceilingLin, ceilingLin, dry.getSample (1, i) * m);
        }
    else
        match.skip (n);
    outMeter.process (L, R, n);
}
}
