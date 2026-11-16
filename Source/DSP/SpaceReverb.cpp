#include "SpaceReverb.h"
#include "Finite.h"

namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr int chunk = 32;
constexpr float maxPredelayMs = 250.0f, maxSize = 1.5f;

struct Algo
{
    const char* name;
    float lineMs[SpaceReverb::numLines]; // FDN delay lengths at size 1x (tuned apart so no two share a common period)
    struct { float ms, gain; } er[12];   // the early-reflection pattern (alternating left / right)
    int erCount;
    int diffStages;                      // input diffusion
    bool tankDiffusion;                  // an allpass inside every line: plate-like density
    float modScale, early, late, shimmer;
};

// Delay lengths and reflection patterns are this product's own choices: rooms short and dense, halls long and sparse, a plate with no early
// reflections and a very dense tank, ambience as reflections with almost no tail.
const Algo algos[SpaceReverb::numAlgorithms] = {
    { "Room",      { 13.7f, 17.3f, 19.9f, 23.3f, 27.1f, 29.9f, 33.7f, 37.1f },
      { { 2.3f, .9f }, { 4.1f, .8f }, { 5.9f, .75f }, { 7.7f, .7f }, { 9.8f, .62f }, { 12.1f, .55f }, { 14.6f, .5f }, { 17.3f, .44f }, { 20.2f, .38f },
        { 23.4f, .32f }, { 26.9f, .27f }, { 30.7f, .22f } }, 12, 4, false, 0.6f, 0.8f, 0.7f, 0.0f },
    { "Chamber",   { 21.1f, 25.7f, 29.3f, 34.1f, 38.9f, 43.7f, 47.3f, 53.1f },
      { { 3.1f, .8f }, { 6.7f, .7f }, { 9.9f, .62f }, { 13.6f, .55f }, { 17.8f, .48f }, { 22.3f, .41f }, { 27.4f, .35f }, { 33.1f, .29f }, { 39.5f, .24f },
        { 46.1f, .2f } }, 10, 4, true, 0.8f, 0.6f, 0.85f, 0.0f },
    { "Plate",     { 17.9f, 22.3f, 26.9f, 31.1f, 35.3f, 40.7f, 44.9f, 49.1f },
      {}, 0, 6, true, 1.0f, 0.0f, 1.0f, 0.0f },
    { "Hall",      { 39.7f, 45.1f, 51.3f, 57.9f, 64.1f, 71.3f, 79.9f, 87.7f },
      { { 8.3f, .7f }, { 14.9f, .6f }, { 21.7f, .55f }, { 29.3f, .48f }, { 37.9f, .42f }, { 47.1f, .36f }, { 57.3f, .3f }, { 68.9f, .25f }, { 81.1f, .2f } },
      9, 4, true, 1.0f, 0.5f, 0.9f, 0.0f },
    { "Cathedral", { 61.3f, 70.9f, 81.7f, 93.1f, 104.9f, 117.1f, 131.3f, 146.9f },
      { { 14.1f, .6f }, { 27.3f, .52f }, { 41.9f, .46f }, { 57.7f, .4f }, { 74.3f, .34f }, { 93.1f, .28f }, { 113.9f, .23f }, { 136.7f, .18f } },
      8, 4, true, 1.2f, 0.45f, 0.95f, 0.0f },
    { "Ambience",  { 7.1f, 8.9f, 10.7f, 12.7f, 14.3f, 16.1f, 18.7f, 21.1f },
      { { 1.1f, .9f }, { 2.3f, .85f }, { 3.4f, .8f }, { 4.7f, .74f }, { 6.1f, .68f }, { 7.6f, .62f }, { 9.3f, .56f }, { 11.2f, .5f }, { 13.3f, .44f },
        { 15.6f, .38f }, { 18.1f, .32f }, { 20.9f, .27f } }, 12, 2, false, 0.4f, 1.0f, 0.35f, 0.0f },
    { "Modulated Space", { 35.3f, 42.1f, 49.7f, 56.3f, 63.7f, 70.1f, 77.9f, 85.1f },
      { { 9.7f, .5f }, { 18.3f, .42f }, { 28.9f, .35f }, { 41.3f, .28f }, { 55.1f, .22f } }, 5, 4, true, 3.0f, 0.35f, 0.95f, 0.0f },
    { "Experimental Space", { 29.9f, 37.3f, 45.7f, 53.9f, 61.1f, 69.7f, 79.1f, 89.3f },
      { { 19.7f, .5f }, { 41.3f, .4f }, { 67.1f, .3f } }, 3, 6, true, 2.0f, 0.3f, 0.9f, 0.32f } };

struct Character
{
    const char* name;
    float crossoverHz, lowRatio, highRatio, modRate, modDepth, earlyToneHz;
};
// WARM: dark, smooth, slowly swimming; CLASSIC: balanced and dense; MODERN: bright, clear, steady, a tighter low end.
const Character chars[SpaceReverb::numCharacters] = {
    { "Warm",    2500.0f, 1.25f, 0.45f, 0.7f, 1.4f, 5500.0f },
    { "Classic", 4500.0f, 1.00f, 0.65f, 1.0f, 1.0f, 9000.0f },
    { "Modern",  7500.0f, 0.85f, 0.90f, 1.3f, 0.5f, 15000.0f } };

const float diffMs[6] = { 4.771f, 3.595f, 12.73f, 9.307f, 7.12f, 5.61f };

int nearestPrime (int n)
{
    auto isPrime = [] (int v) { if (v < 2) return false; for (int d = 2; d * d <= v; ++d) if (v % d == 0) return false; return true; };
    for (int k = 0; k < 1000; ++k)
    {
        if (isPrime (n + k)) return n + k;
        if (isPrime (n - k)) return n - k;
    }
    return n;
}

float onePoleA (double hz, double fs) { const double g = std::tan (pi * juce::jlimit (10.0, 0.45 * fs, hz) / fs); return (float) (g / (1.0 + g)); }
}

const char* SpaceReverb::algorithmName (int i) { return algos[juce::jlimit (0, numAlgorithms - 1, i)].name; }
const char* SpaceReverb::characterName (int i) { return chars[juce::jlimit (0, numCharacters - 1, i)].name; }

// ---- buffers ------------------------------------------------------------------------------------------------------------------------------------

void SpaceReverb::Delay::allocate (int minSize)
{
    int n = 1;
    while (n < minSize + 4)
        n <<= 1;
    buf.assign ((size_t) n, 0.0f);
    mask = n - 1;
}

float SpaceReverb::Delay::linear (int wp, float d) const
{
    const float pos = (float) wp - d;
    const int i0 = (int) std::floor (pos);
    const float t = pos - (float) i0;
    const float a = buf[(size_t) (i0 & mask)], b = buf[(size_t) ((i0 + 1) & mask)];
    return a + t * (b - a);
}

float SpaceReverb::Delay::cubic (int wp, float d) const // Catmull-Rom; d >= 2
{
    const float pos = (float) wp - d;
    const int i0 = (int) std::floor (pos);
    const float t = pos - (float) i0;
    const float ym = buf[(size_t) ((i0 - 1) & mask)], y0 = buf[(size_t) (i0 & mask)], y1 = buf[(size_t) ((i0 + 1) & mask)], y2 = buf[(size_t) ((i0 + 2) & mask)];
    const float c1 = 0.5f * (y1 - ym), c2 = ym - 2.5f * y0 + 2.0f * y1 - 0.5f * y2, c3 = 0.5f * (y2 - ym) + 1.5f * (y0 - y1);
    return ((c3 * t + c2) * t + c1) * t + y0;
}

float SpaceReverb::Allpass::process (int wp, float x, float g)
{
    const float out = d.at (wp, length);
    const float in = x + g * out;
    d.buf[(size_t) (wp & d.mask)] = in;
    return out - g * in;
}

// ---- set-up -------------------------------------------------------------------------------------------------------------------------------------

void SpaceReverb::prepare (double sampleRate)
{
    fs = sampleRate > 8000.0 ? sampleRate : 48000.0;
    const auto samples = [this] (double ms) { return (int) std::ceil (ms * 0.001 * fs); };
    for (auto& p : pre)
        p.allocate (samples (maxPredelayMs + 137.0 * maxSize + 10.0));
    for (auto& l : lines)
        l.allocate (samples (147.0 * maxSize + 12.0));
    for (auto& side : inDiff)
        for (auto& a : side)
            a.d.allocate (samples (14.0));
    for (auto& a : tankAp)
        a.d.allocate (samples (147.0 * maxSize * 0.15 + 2.0));
    shimmerBuf.allocate (samples (60.0));
    mixS.reset (fs, 0.03);
    inGain.reset (fs, 0.03);
    xfade.reset (fs, 0.03);
    freezeS.reset (fs, 0.1);
    sizeS.reset (fs, 0.3);
    predelayS.reset (fs, 0.2);
    reset();
}

void SpaceReverb::reset()
{
    for (auto& p : pre) p.clear();
    for (auto& l : lines) l.clear();
    for (auto& side : inDiff) for (auto& a : side) a.d.clear();
    for (auto& a : tankAp) a.d.clear();
    shimmerBuf.clear();
    std::fill (std::begin (dampState), std::end (dampState), 0.0f);
    erState[0] = erState[1] = 0.0f;
    for (auto& f : cutHpS) f[0] = f[1] = 0.0f;
    for (auto& f : cutLpS) f[0] = f[1] = 0.0f;
    shimmerPhase = 0.0f;
    structureAlgo = -1;
    active = target.on;
    idleBlocks = 0;
    mixS.setCurrentAndTargetValue (target.mix);
    inGain.setCurrentAndTargetValue (target.on && ! target.freeze ? 1.0f : 0.0f);
    freezeS.setCurrentAndTargetValue (target.on && target.freeze ? 1.0f : 0.0f);
    sizeS.setCurrentAndTargetValue (0.5f + target.size);
    predelayS.setCurrentAndTargetValue ((float) (target.predelayMs * 0.001 * fs));
    xfade.setCurrentAndTargetValue (1.0f);
    chunkLeft = 0;
}

void SpaceReverb::setSettings (const Settings& s)
{
    auto fin = [] (float v, float d) { return std::isfinite (v) ? v : d; };
    Settings t = s;
    t.algorithm = juce::jlimit (0, numAlgorithms - 1, s.algorithm);
    t.character = juce::jlimit (0, numCharacters - 1, s.character);
    t.mix = juce::jlimit (0.0f, 1.0f, fin (s.mix, 0.0f));
    t.decaySeconds = juce::jlimit (0.2f, 20.0f, fin (s.decaySeconds, 1.8f));
    t.predelayMs = juce::jlimit (0.0f, maxPredelayMs, fin (s.predelayMs, 0.0f));
    t.size = juce::jlimit (0.0f, 1.0f, fin (s.size, 0.5f));
    t.diffusion = juce::jlimit (0.0f, 1.0f, fin (s.diffusion, 0.7f));
    t.damping = juce::jlimit (0.0f, 1.0f, fin (s.damping, 0.5f));
    t.lowCutHz = juce::jlimit (20.0f, 2000.0f, fin (s.lowCutHz, 150.0f));
    t.highCutHz = juce::jlimit (1000.0f, 20000.0f, fin (s.highCutHz, 9000.0f));
    t.modRateHz = juce::jlimit (0.01f, 10.0f, fin (s.modRateHz, 0.6f));
    t.modDepth = juce::jlimit (0.0f, 1.0f, fin (s.modDepth, 0.3f));
    t.width = juce::jlimit (0.0f, 1.5f, fin (s.width, 1.0f));
    t.early = juce::jlimit (0.0f, 1.0f, fin (s.early, 0.5f));
    t.late = juce::jlimit (0.0f, 1.0f, fin (s.late, 0.8f));
    const bool wasIdle = ! active;
    target = t;
    mixS.setTargetValue (t.mix);
    inGain.setTargetValue (t.on && ! t.freeze ? 1.0f : 0.0f);
    freezeS.setTargetValue (t.on && t.freeze ? 1.0f : 0.0f);
    sizeS.setTargetValue (0.5f + t.size);
    predelayS.setTargetValue ((float) (t.predelayMs * 0.001 * fs));
    if (t.on && wasIdle) // waking up: start from the settings, the input fades in
    {
        active = true;
        idleBlocks = 0;
        mixS.setCurrentAndTargetValue (t.mix);
        sizeS.setCurrentAndTargetValue (0.5f + t.size);
        predelayS.setCurrentAndTargetValue ((float) (t.predelayMs * 0.001 * fs));
        structureAlgo = -1;
    }
}

double SpaceReverb::tailSeconds() const
{
    if (! target.on && ! active)
        return 0.0;
    if (target.freeze)
        return 60.0;
    return target.predelayMs * 0.001 + target.decaySeconds * 1.2 + 0.2;
}

void SpaceReverb::configure()
{
    structureAlgo = target.algorithm;
    const auto& a = algos[structureAlgo];
    const float size = sizeS.getCurrentValue();
    numDiff = a.diffStages;
    tankDiffusion = a.tankDiffusion;
    for (int s = 0; s < 6; ++s)
    {
        inDiff[0][s].length = juce::jmax (1, nearestPrime ((int) (diffMs[s] * 0.001 * fs)));
        inDiff[1][s].length = juce::jmax (1, nearestPrime ((int) (diffMs[s] * 1.07 * 0.001 * fs)));
    }
    for (int j = 0; j < numLines; ++j)
    {
        lineBase[j] = (float) nearestPrime ((int) (a.lineMs[j] * 0.001 * fs));
        lineLen[j] = lineBase[j] * size;
        tankAp[j].length = juce::jlimit (1, tankAp[j].d.mask - 2, nearestPrime ((int) (a.lineMs[j] * 0.13 * size * 0.001 * fs)));
        lfoPhase[j] = 0.618 * j * 2.0 * pi;
        lines[j].clear();
        tankAp[j].d.clear();
        dampState[j] = 0.0f;
    }
    for (auto& side : inDiff) for (auto& d : side) d.d.clear();
    numTaps = a.erCount;
    for (int k = 0; k < numTaps; ++k)
        taps[(size_t) k] = { 0, k % 2, a.er[k].gain };
    shimmerBuf.clear();
    shimmer = a.shimmer;
    chunkLeft = 0;
}

void SpaceReverb::updateCoefficients()
{
    const auto& a = algos[structureAlgo];
    const auto& c = chars[target.character];
    const float size = sizeS.getCurrentValue(), freeze = freezeS.getCurrentValue();
    const float decay = target.decaySeconds;
    const float hfRatio = juce::jlimit (0.08f, 1.2f, c.highRatio * (1.0f - 0.8f * target.damping)), lfRatio = c.lowRatio;
    modDepthSamples = target.modDepth * a.modScale * c.modDepth * 0.0008f * (float) fs;
    for (int j = 0; j < numLines; ++j)
    {
        lineLen[j] = juce::jmax (modDepthSamples + 4.0f, lineBase[j] * size);
        const double len = lineLen[j];
        const double gl = std::pow (10.0, -3.0 * len / (decay * lfRatio * fs)), gh = std::pow (10.0, -3.0 * len / (decay * hfRatio * fs));
        const double gf = std::pow (10.0, -3.0 * len / (600.0 * fs)); // frozen: an RT60 of ten minutes (0.1 dB a second), no damping
        gLow[j] = (float) (gl + (gf - gl) * freeze);
        gHigh[j] = (float) (gh + (gf - gh) * freeze);
        lfoStep[j] = 2.0 * pi * target.modRateHz * c.modRate * (0.75 + 0.11 * j) / fs;
    }
    dampA = onePoleA (c.crossoverHz, fs);
    erA = onePoleA (c.earlyToneHz, fs);
    for (int k = 0; k < numTaps; ++k)
        taps[(size_t) k].delay = (int) (a.er[k].ms * size * 0.001 * fs);
    const double hg = std::tan (pi * juce::jmin ((double) target.lowCutHz, 0.45 * fs) / fs), lg = std::tan (pi * juce::jmin ((double) target.highCutHz, 0.45 * fs) / fs);
    hpG = (float) hg;
    lpG = (float) lg;
    predelaySamples = predelayS.getCurrentValue();
}

// one 12 dB/oct high-pass (low cut) then low-pass (high cut), TPT state-variable filters
float SpaceReverb::applyCuts (int ch, float x)
{
    auto svf = [] (float in, float g, float k, float* s, bool high)
    {
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float v3 = in - s[1], v1 = a1 * s[0] + a2 * v3, v2 = s[1] + a2 * s[0] + a3 * v3;
        s[0] = 2.0f * v1 - s[0];
        s[1] = 2.0f * v2 - s[1];
        return high ? in - k * v1 - v2 : v2;
    };
    return svf (svf (x, hpG, hpK, cutHpS[ch], true), lpG, lpK, cutLpS[ch], false);
}

// ---- audio ------------------------------------------------------------------------------------------------------------------------------------

void SpaceReverb::process (juce::AudioBuffer<float>& buffer)
{
    const int numChannels = juce::jmin (2, buffer.getNumChannels()), n = buffer.getNumSamples();
    if (! active || numChannels <= 0 || n <= 0)
        return; // idle: the audio is untouched

    // an algorithm change: fade the wet signal out, rebuild, fade back in
    if (structureAlgo < 0)
        configure();
    else if (structureAlgo != target.algorithm)
    {
        xfade.setTargetValue (0.0f);
        if (xfade.getCurrentValue() <= 0.0f && ! xfade.isSmoothing())
        {
            configure();
            xfade.setTargetValue (1.0f);
        }
    }

    float* data[2] = { buffer.getWritePointer (0), numChannels > 1 ? buffer.getWritePointer (1) : nullptr };
    for (int ch = 0; ch < numChannels; ++ch)
        dspguard::zeroNonFinite (data[ch], n);
    const auto& a = algos[structureAlgo];
    double wetEnergy = 0.0;
    const float diffG = 0.75f * target.diffusion, tankG = (structureAlgo == 2 ? 0.6f : 0.35f) * target.diffusion; // a plate is the densest
    const float shimLen = (float) (0.04 * fs);

    for (int i = 0; i < n; ++i)
    {
        if (chunkLeft <= 0)
        {
            for (auto* sm : { &freezeS, &sizeS, &predelayS })
                sm->skip (chunk);
            updateCoefficients();
            chunkLeft = chunk;
        }
        --chunkLeft;
        const float mix = mixS.getNextValue(), ing = inGain.getNextValue(), xf = xfade.getNextValue();
        const float xL = data[0][i], xR = data[1] != nullptr ? data[1][i] : xL;

        // pre-delay (written with the input fade: off and freeze stop feeding the space)
        pre[0].buf[(size_t) (w & pre[0].mask)] = xL * ing;
        pre[1].buf[(size_t) (w & pre[1].mask)] = xR * ing;
        const float pdL = pre[0].linear (w, predelaySamples), pdR = pre[1].linear (w, predelaySamples);

        // early reflections: the algorithm's pattern after the pre-delay, toned by the character
        float erL = 0.0f, erR = 0.0f;
        const int pd = (int) predelaySamples;
        for (int k = 0; k < numTaps; ++k)
        {
            const auto& t = taps[(size_t) k];
            const float v = t.gain * pre[t.channel].at (w, pd + t.delay);
            (t.channel == 0 ? erL : erR) += v;
        }
        erState[0] += erA * (erL - erState[0]);
        erState[1] += erA * (erR - erState[1]);

        // input diffusion
        float dL = pdL, dR = pdR;
        for (int s = 0; s < numDiff; ++s)
        {
            dL = inDiff[0][s].process (w, dL, diffG);
            dR = inDiff[1][s].process (w, dR, diffG);
        }

        // the tank: modulated reads, Householder mixing, two-band decay, injection, optional density and shimmer
        float o[numLines];
        float sum = 0.0f;
        for (int j = 0; j < numLines; ++j)
        {
            lfoPhase[j] += lfoStep[j];
            if (lfoPhase[j] > 2.0 * pi)
                lfoPhase[j] -= 2.0 * pi;
            o[j] = lines[j].cubic (w, lineLen[j] + modDepthSamples * (float) std::sin (lfoPhase[j]));
            sum += o[j];
        }
        const float lateL = 0.5f * (o[0] - o[2] + o[4] - o[6]), lateR = 0.5f * (o[1] - o[3] + o[5] - o[7]);
        float shifted = 0.0f;
        if (shimmer > 0.0f) // an octave up: two read heads sweeping the last 40 ms at double speed, crossfaded
        {
            shimmerBuf.buf[(size_t) (w & shimmerBuf.mask)] = 0.5f * (lateL + lateR);
            shimmerPhase += 1.0f / shimLen;
            if (shimmerPhase >= 1.0f)
                shimmerPhase -= 1.0f;
            const float p2 = shimmerPhase + 0.5f >= 1.0f ? shimmerPhase - 0.5f : shimmerPhase + 0.5f;
            const float s1 = shimmerBuf.linear (w, 2.0f + shimLen * (1.0f - shimmerPhase)), s2 = shimmerBuf.linear (w, 2.0f + shimLen * (1.0f - p2));
            shifted = s1 * (1.0f - std::abs (2.0f * shimmerPhase - 1.0f)) + s2 * (1.0f - std::abs (2.0f * p2 - 1.0f));
        }
        const float house = 0.25f * sum; // 2 / N
        for (int j = 0; j < numLines; ++j)
        {
            const float y = o[j] - house;
            const float v = (y - dampState[j]) * dampA, lp = v + dampState[j];
            dampState[j] = lp + v;
            float f = gHigh[j] * y + (gLow[j] - gHigh[j]) * lp;
            f += ((j & 1) == 0 ? dL : dR) * ((j & 2) == 0 ? 0.5f : -0.5f) + shimmer * shifted * 0.5f;
            if (tankDiffusion)
                f = tankAp[j].process (w, f, tankG);
            if (! (std::abs (f) < 4.0f)) // overflow guard (and NaN): never let the tank run away
                f = std::isfinite (f) ? 4.0f * std::tanh (f * 0.25f) : 0.0f;
            lines[j].buf[(size_t) (w & lines[j].mask)] = f;
        }

        // the wet signal: early + late, width, cuts
        float wL = target.early * a.early * 0.6f * erState[0] + target.late * a.late * 0.7f * lateL;
        float wR = target.early * a.early * 0.6f * erState[1] + target.late * a.late * 0.7f * lateR;
        const float m = 0.5f * (wL + wR), sd = 0.5f * (wL - wR) * target.width;
        wL = applyCuts (0, m + sd) * xf;
        wR = applyCuts (1, m - sd) * xf;
        wetEnergy += (double) wL * wL + (double) wR * wR;

        const float dryG = std::cos (mix * (float) (0.5 * pi)), wetG = std::sin (mix * (float) (0.5 * pi));
        if (data[1] != nullptr)
        {
            data[0][i] = xL * dryG + wL * wetG;
            data[1][i] = xR * dryG + wR * wetG;
        }
        else
            data[0][i] = xL * dryG + 0.5f * (wL + wR) * wetG;
        w = (w + 1) & 0x3fffffff;
    }

    // switched off: the tail rings out; idle (pass-through, no CPU) once it is under -100 dBFS
    if (! target.on && ! inGain.isSmoothing() && wetEnergy / (2.0 * n) < 1.0e-10)
    {
        if (++idleBlocks > 4)
        {
            const auto keep = target;
            reset();
            target = keep;
            active = false;
        }
    }
    else
        idleBlocks = 0;
}
