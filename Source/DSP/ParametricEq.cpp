#include "ParametricEq.h"
#include "Finite.h"

namespace
{
constexpr int chunkSize = 16;            // coefficient refresh interval while a setting moves
constexpr double smoothSeconds = 0.02;
constexpr double corruptState = 1.0e30;  // filter memory this large (~600 dB) cannot come from real audio
constexpr double pi = 3.14159265358979323846;
constexpr double butterQ = 0.70710678118654752;

double clampFreq (double f, double fs) { return juce::jmin (juce::jlimit (10.0, 20000.0, f), 0.45 * fs); }
}

// ---- one section ---------------------------------------------------------------------------------------------------------------------------

void ParametricEq::Section::set (double gIn, double kIn, double m0In, double m1In, double m2In, bool onePoleIn)
{
    g = gIn; k = kIn; m0 = m0In; m1 = m1In; m2 = m2In; onePole = onePoleIn;
    if (onePole)
        a1 = g / (1.0 + g);
    else
    {
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
}

// The digital filter is the bilinear transform of H(s) with s = j tan(pi f / fs) / g, so this is exactly what process() does.
std::complex<double> ParametricEq::Section::response (double tanW) const
{
    const std::complex<double> s (0.0, tanW / g);
    if (onePole)
        return m0 + m2 / (s + 1.0);                        // low-pass 1/(s+1) and the input
    const auto den = s * s + k * s + 1.0;
    return m0 + m1 * s / den + m2 / den;                    // input, band-pass s/D and low-pass 1/D
}

int ParametricEq::slopeIndex (int slopeDb)
{
    int best = 0;
    for (int i = 1; i < numSlopes; ++i)
        if (std::abs (slopes[i] - slopeDb) < std::abs (slopes[best] - slopeDb))
            best = i;
    return best;
}

// ---- design ---------------------------------------------------------------------------------------------------------------------------------

void ParametricEq::design (Design& d, const EqBandSettings& s, double fs)
{
    const double w = std::tan (pi * clampFreq (s.freqHz, fs) / fs);
    const double q = juce::jlimit (0.1, 40.0, (double) s.q);
    const double A = std::pow (10.0, juce::jlimit (-30.0, 30.0, (double) s.gainDb) / 40.0);
    d.count = 1;
    d.scale = 1.0;
    auto& x = d.s[0];
    switch (s.type)
    {
        case EqType::bell:      { const double k = 1.0 / (q * A); x.set (w, k, 1.0, k * (A * A - 1.0), 0.0, false); break; }
        case EqType::lowShelf:  { const double k = 1.0 / q; x.set (w / std::sqrt (A), k, 1.0, k * (A - 1.0), A * A - 1.0, false); break; }
        case EqType::highShelf: { const double k = 1.0 / q; x.set (w * std::sqrt (A), k, A * A, k * (1.0 - A) * A, 1.0 - A * A, false); break; }
        case EqType::tilt:      { const double k = 1.0 / q; x.set (w * std::sqrt (A), k, A * A, k * (1.0 - A) * A, 1.0 - A * A, false); d.scale = 1.0 / A; break; }
        case EqType::notch:     { const double k = 1.0 / q; x.set (w, k, 1.0, -k, 0.0, false); break; }
        case EqType::bandPass:  { const double k = 1.0 / q; x.set (w, k, 0.0, k, 0.0, false); break; }
        case EqType::lowCut:
        case EqType::highCut:
        {
            // Butterworth of order N = slope / 6: an odd order adds one one-pole section; the highest-Q section carries the resonance (Q / 0.707).
            const bool hp = s.type == EqType::lowCut;
            const int order = slopes[slopeIndex (s.slopeDb)] / 6;
            d.count = 0;
            if (order % 2 == 1)
                d.s[d.count++].set (w, 0.0, hp ? 1.0 : 0.0, 0.0, hp ? -1.0 : 1.0, true);
            const int pairs = order / 2;
            for (int i = 0; i < pairs; ++i)
            {
                const double angle = order % 2 == 0 ? (2.0 * i + 1.0) * pi / (2.0 * order) : (i + 1.0) * pi / order;
                double sq = 1.0 / (2.0 * std::cos (angle));
                if (i == pairs - 1)
                    sq *= q / butterQ;
                const double k = 1.0 / sq;
                d.s[d.count++].set (w, k, hp ? 1.0 : 0.0, hp ? -k : 0.0, hp ? -1.0 : 1.0, false);
            }
            break;
        }
    }
}

double ParametricEq::bandResponseDb (const EqBandSettings& s, double hz, double fs)
{
    if (! needsWork (s))
        return 0.0;
    Design d;
    design (d, s, fs);
    const double tanW = std::tan (pi * juce::jlimit (1.0, 0.4999 * fs, hz) / fs);
    std::complex<double> h = d.scale;
    for (int i = 0; i < d.count; ++i)
        h *= d.s[i].response (tanW);
    return 20.0 * std::log10 (juce::jmax (1.0e-12, std::abs (h)));
}

double ParametricEq::responseDb (const EqBandSettings* bands, int count, double hz, double fs)
{
    double total = 0.0;
    for (int b = 0; b < count; ++b)
        total += bandResponseDb (bands[b], hz, fs);
    return total;
}

// ---- state ----------------------------------------------------------------------------------------------------------------------------------

void ParametricEq::Band::clear()
{
    for (auto& r : ic1) for (auto& v : r) v = 0.0;
    for (auto& r : ic2) for (auto& v : r) v = 0.0;
    for (int c = 0; c < maxChannels; ++c)
        detIc1[c] = detIc2[c] = env[c] = 0.0;
    dynDb = designedDyn = 0.0;
    shownDyn.store (0.0f, std::memory_order_relaxed);
    running = false;
}

float ParametricEq::dynamicGainDb (const EqBandSettings& s, float levelDb)
{
    if (! s.dynamic || ! isGainType (s.type) || ! (levelDb > -80.0f)) // silence moves nothing
        return 0.0f;
    const float over = s.below ? s.threshDb - levelDb : levelDb - s.threshDb;
    constexpr float knee = 6.0f;
    const float soft = over <= -0.5f * knee ? 0.0f : over >= 0.5f * knee ? over : (over + 0.5f * knee) * (over + 0.5f * knee) / (2.0f * knee);
    const float amount = juce::jmin (std::abs (s.rangeDb), 0.75f * soft);
    return s.rangeDb < 0.0f ? -amount : amount;
}

void ParametricEq::detect (Band& b, double* const* sig, int numSignals, int n)
{
    const auto& s = b.current;
    const double w = std::tan (pi * clampFreq (b.freq.getCurrentValue(), sampleRate) / sampleRate);
    if (s.type == EqType::lowShelf)
        b.detector.set (w, 1.41421356, 0.0, 0.0, 1.0, false);           // the region a low shelf moves
    else if (s.type == EqType::highShelf)
        b.detector.set (w, 1.41421356, 1.0, -1.41421356, -1.0, false);  // ... and a high shelf
    else
    {
        const double k = 1.0 / juce::jmax (0.5, s.type == EqType::tilt ? 0.5 : (double) b.q.getCurrentValue());
        b.detector.set (w, k, 0.0, k, 0.0, false);                       // a band-pass around a bell (unity peak)
    }
    const double atk = 1.0 - std::exp (-1.0 / (juce::jlimit (0.05, 500.0, (double) b.target.attackMs) * 0.001 * sampleRate));
    const double rel = 1.0 - std::exp (-1.0 / (juce::jlimit (1.0, 5000.0, (double) b.target.releaseMs) * 0.001 * sampleRate));
    double level = 0.0;
    for (int c = 0; c < numSignals; ++c)
    {
        double tmp[chunkSize];
        std::copy (sig[c], sig[c] + n, tmp);
        b.detector.run (tmp, n, b.detIc1[c], b.detIc2[c]);
        double e = b.env[c];
        for (int i = 0; i < n; ++i)
        {
            const double x = std::abs (tmp[i]);
            e += (x > e ? atk : rel) * (x - e);
        }
        if (! (std::abs (b.detIc1[c]) < corruptState && std::abs (b.detIc2[c]) < corruptState && e < corruptState))
            b.detIc1[c] = b.detIc2[c] = e = 0.0;
        b.env[c] = e;
        level = juce::jmax (level, e); // linked: the louder signal decides, so a stereo image never moves
    }
    auto t = b.target;
    t.type = s.type;
    b.dynDb = dynamicGainDb (t, level > 1.0e-9 ? (float) (20.0 * std::log10 (level)) : -200.0f);
    if (std::abs (b.dynDb - b.designedDyn) > 0.005)
        b.dirty = true;
    b.shownDyn.store ((float) b.dynDb, std::memory_order_relaxed);
}

bool ParametricEq::sameStructure (const EqBandSettings& a, const EqBandSettings& b) const
{
    return a.type == b.type && a.placement == b.placement && slopeIndex (a.slopeDb) == slopeIndex (b.slopeDb);
}

void ParametricEq::prepare (double sr)
{
    sampleRate = sr > 1000.0 ? sr : 48000.0;
    for (auto& b : bands)
    {
        b.freq.reset (sampleRate, smoothSeconds);
        b.q.reset (sampleRate, smoothSeconds);
        b.gainDb.reset (sampleRate, smoothSeconds);
        b.level.reset (sampleRate, smoothSeconds);
    }
    soloMix.reset (sampleRate, smoothSeconds);
    reset();
}

void ParametricEq::reset()
{
    for (auto& b : bands)
    {
        b.clear();
        b.current = b.target;
        b.freq.setCurrentAndTargetValue (b.target.freqHz);
        b.q.setCurrentAndTargetValue (b.target.q);
        b.gainDb.setCurrentAndTargetValue (b.target.gainDb);
        b.level.setCurrentAndTargetValue (b.target.on ? 1.0f : 0.0f);
        b.dirty = true;
    }
    soloShown = soloBand;
    soloMix.setCurrentAndTargetValue (soloBand >= 0 ? 1.0f : 0.0f);
    for (auto& r : soloIc1) for (auto& v : r) v = 0.0;
    for (auto& r : soloIc2) for (auto& v : r) v = 0.0;
}

void ParametricEq::setBand (int band, const EqBandSettings& s)
{
    if (band < 0 || band >= numBands)
        return;
    auto& b = bands[band];
    b.target = s;
    b.target.freqHz = juce::jlimit (10.0f, 20000.0f, std::isfinite (s.freqHz) ? s.freqHz : 1000.0f);
    b.target.q = juce::jlimit (0.1f, 40.0f, std::isfinite (s.q) ? s.q : 0.7071f);
    const float g = std::isfinite (s.gainDb) ? juce::jlimit (-30.0f, 30.0f, s.gainDb) : 0.0f;
    b.target.gainDb = std::abs (g) < flatThresholdDb ? 0.0f : g;
    auto finiteOr = [] (float v, float d) { return std::isfinite (v) ? v : d; };
    b.target.threshDb = juce::jlimit (-80.0f, 0.0f, finiteOr (s.threshDb, -30.0f));
    b.target.rangeDb = juce::jlimit (-30.0f, 30.0f, finiteOr (s.rangeDb, 0.0f));
    b.target.attackMs = juce::jlimit (0.05f, 500.0f, finiteOr (s.attackMs, 5.0f));
    b.target.releaseMs = juce::jlimit (1.0f, 5000.0f, finiteOr (s.releaseMs, 120.0f));
    b.target.slopeDb = slopes[slopeIndex (s.slopeDb)];
    b.freq.setTargetValue (b.target.freqHz);
    b.q.setTargetValue (b.target.q);
    b.gainDb.setTargetValue (b.target.gainDb);
}

void ParametricEq::setSolo (int band)
{
    soloBand = band >= 0 && band < numBands ? band : -1;
}

// ---- audio ----------------------------------------------------------------------------------------------------------------------------------

void ParametricEq::Section::run (double* x, int n, double& ic1, double& ic2) const
{
    if (onePole)
    {
        double st = ic1;
        for (int i = 0; i < n; ++i)
        {
            const double v = (x[i] - st) * a1, lp = v + st;
            st = lp + v;
            x[i] = m0 * x[i] + m2 * lp;
        }
        ic1 = st;
        return;
    }
    double s1 = ic1, s2 = ic2;
    for (int i = 0; i < n; ++i)
    {
        const double v0 = x[i], v3 = v0 - s2, v1 = a1 * s1 + a2 * v3, v2 = s2 + a2 * s1 + a3 * v3;
        s1 = 2.0 * v1 - s1;
        s2 = 2.0 * v2 - s2;
        x[i] = m0 * v0 + m1 * v1 + m2 * v2;
    }
    ic1 = s1;
    ic2 = s2;
}

// The band's filter on one signal (a channel, or the mid or side part), in place, with the fade if one is running.
void ParametricEq::runBand (Band& b, double* const* sig, int numSignals, int n, const float* levelRamp, bool fading)
{
    for (int c = 0; c < numSignals; ++c)
    {
        double* x = sig[c];
        double dry[chunkSize];
        if (fading)
            std::copy (x, x + n, dry);
        for (int i = 0; i < b.d.count; ++i)
            b.d.s[i].run (x, n, b.ic1[i][c], b.ic2[i][c]);
        if (! juce::exactlyEqual (b.d.scale, 1.0))
            for (int i = 0; i < n; ++i)
                x[i] *= b.d.scale;
        if (fading)
            for (int i = 0; i < n; ++i)
                x[i] = dry[i] + (double) levelRamp[i] * (x[i] - dry[i]);
        bool ok = true;
        for (int i = 0; i < b.d.count; ++i)
            ok = ok && std::abs (b.ic1[i][c]) < corruptState && std::abs (b.ic2[i][c]) < corruptState; // also false for NaN/Inf
        if (! ok)
            for (int i = 0; i < maxSections; ++i)
                b.ic1[i][c] = b.ic2[i][c] = 0.0; // drop the filter memory, not the plug-in
    }
}

void ParametricEq::process (juce::AudioBuffer<float>& buffer)
{
    const int numChannels = juce::jmin (buffer.getNumChannels(), maxChannels);
    const int numSamples = buffer.getNumSamples();
    if (numChannels <= 0 || numSamples <= 0)
        return;

    // A NaN/Inf in a recursive filter would stay in its memory forever, so never let one in.
    for (int ch = 0; ch < numChannels; ++ch)
        dspguard::zeroNonFinite (buffer.getWritePointer (ch), numSamples);

    // solo: a change of band fades the old one out before the new one comes in
    if (soloShown != soloBand)
    {
        soloMix.setTargetValue (0.0f);
        if (soloMix.getCurrentValue() <= 0.0f && ! soloMix.isSmoothing())
        {
            soloShown = soloBand;
            for (auto& r : soloIc1) for (auto& v : r) v = 0.0;
            for (auto& r : soloIc2) for (auto& v : r) v = 0.0;
        }
    }
    if (soloShown == soloBand)
        soloMix.setTargetValue (soloShown >= 0 && bands[soloShown].target.on ? 1.0f : 0.0f);

    // nothing to do (every band off or flat and settled, no solo): the audio is untouched, at almost no cost
    {
        bool idle = soloShown < 0 && ! soloMix.isSmoothing() && soloMix.getCurrentValue() <= 0.0f;
        for (int k = 0; k < numBands && idle; ++k)
        {
            const auto& b = bands[k];
            const bool settled = ! b.level.isSmoothing() && ! b.freq.isSmoothing() && ! b.q.isSmoothing() && ! b.gainDb.isSmoothing() && sameStructure (b.target, b.current);
            const bool silent = b.level.getCurrentValue() <= 0.0f || ! b.target.on;
            const bool flat = isGainType (b.current.type) && ! b.target.dynamic && std::abs (b.gainDb.getCurrentValue()) < flatThresholdDb;
            idle = settled && ! b.running && (silent || flat);
        }
        if (idle)
            return;
    }

    bool anyActive = false;
    for (int start = 0; start < numSamples; start += chunkSize)
    {
        const int n = juce::jmin (chunkSize, numSamples - start);
        double work[maxChannels][chunkSize], input[maxChannels][chunkSize];
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* src = buffer.getReadPointer (ch) + start;
            for (int i = 0; i < n; ++i)
                work[ch][i] = input[ch][i] = src[i];
        }
        bool touched = false;

        for (auto& b : bands)
        {
            // structure (type, slope, channel) changes and on/off fade over ~20 ms
            const bool sameNow = sameStructure (b.target, b.current);
            b.level.setTargetValue (sameNow && b.target.on ? 1.0f : 0.0f);
            if (! sameNow && b.level.getCurrentValue() <= 0.0f && ! b.level.isSmoothing())
            {
                b.current = b.target;
                b.clear();
                b.dirty = true;
                b.level.setTargetValue (b.target.on ? 1.0f : 0.0f);
            }
            b.current.on = b.target.on;

            const bool moving = b.freq.isSmoothing() || b.q.isSmoothing() || b.gainDb.isSmoothing();
            if (moving)
            {
                b.freq.skip (n); b.q.skip (n); b.gainDb.skip (n);
                b.dirty = true;
            }
            const bool fading = b.level.isSmoothing();
            const bool silentLevel = ! fading && b.level.getCurrentValue() <= 0.0f;
            const bool dynamic = b.target.dynamic && isGainType (b.current.type);
            const bool flat = isGainType (b.current.type) && ! moving && ! dynamic && std::abs (b.gainDb.getCurrentValue()) < flatThresholdDb;
            if (silentLevel || flat)
            {
                if (fading)
                    b.level.skip (n);
                if (b.running)
                    b.clear();
                continue;
            }
            if (! dynamic && std::abs (b.dynDb) > 0.0) // dynamics switched off: back to the static gain
            {
                b.dynDb = 0.0;
                b.dirty = true;
                b.shownDyn.store (0.0f, std::memory_order_relaxed);
            }
            // the filter for the current values (and the dynamic gain change, measured on the band's own input just before it is filtered)
            auto prepareFilter = [&] (double* const* sig, int numSignals)
            {
                if (dynamic)
                    detect (b, sig, numSignals, n);
                if (b.dirty)
                {
                    auto now = b.current;
                    now.freqHz = b.freq.getCurrentValue();
                    now.q = b.q.getCurrentValue();
                    now.gainDb = b.gainDb.getCurrentValue() + (float) b.dynDb;
                    design (b.d, now, sampleRate);
                    b.designedDyn = b.dynDb;
                    b.dirty = false;
                }
            };
            float ramp[chunkSize];
            if (fading)
                for (int i = 0; i < n; ++i)
                    ramp[i] = b.level.getNextValue();
            b.running = true;
            touched = true;

            const auto placement = b.current.placement;
            if (numChannels == 1)
            {
                if (placement != EqPlacement::side) // mono: there is no side; every other placement is the one channel
                {
                    double* sig[1] = { work[0] };
                    prepareFilter (sig, 1);
                    runBand (b, sig, 1, n, ramp, fading);
                }
            }
            else if (placement == EqPlacement::stereo)
            {
                double* sig[2] = { work[0], work[1] };
                prepareFilter (sig, 2);
                runBand (b, sig, 2, n, ramp, fading);
            }
            else if (placement == EqPlacement::left || placement == EqPlacement::right)
            {
                double* sig[1] = { work[placement == EqPlacement::left ? 0 : 1] };
                prepareFilter (sig, 1);
                runBand (b, sig, 1, n, ramp, fading);
            }
            else
            {
                double ms[chunkSize];
                const bool mid = placement == EqPlacement::mid;
                for (int i = 0; i < n; ++i)
                    ms[i] = mid ? 0.5 * (work[0][i] + work[1][i]) : 0.5 * (work[0][i] - work[1][i]);
                double before[chunkSize];
                std::copy (ms, ms + n, before);
                double* sig[1] = { ms };
                prepareFilter (sig, 1);
                runBand (b, sig, 1, n, ramp, fading);
                for (int i = 0; i < n; ++i) // add only the change to the part this band works on: L = M + S, R = M - S
                {
                    const double delta = ms[i] - before[i];
                    work[0][i] += delta;
                    work[1][i] += mid ? delta : -delta;
                }
            }
        }

        // solo: the part of the input the soloed band works on
        if (soloShown >= 0 && (soloMix.getCurrentValue() > 0.0f || soloMix.isSmoothing()))
        {
            const auto& sb = bands[soloShown];
            const auto t = sb.current.type;
            const double w = std::tan (pi * clampFreq (sb.freq.getCurrentValue(), sampleRate) / sampleRate);
            if (t == EqType::lowShelf || t == EqType::lowCut || t == EqType::highShelf || t == EqType::highCut)
            {
                const bool hp = t == EqType::highShelf || t == EqType::highCut; // the region the band moves
                for (int i = 0; i < 2; ++i)
                {
                    const double k = 1.0 / (i == 0 ? 0.5411961 : 1.3065630);
                    soloFilter[i].set (w, k, hp ? 1.0 : 0.0, hp ? -k : 0.0, hp ? -1.0 : 1.0, false);
                }
            }
            else
            {
                const double k = 1.0 / juce::jmax (0.5, t == EqType::tilt ? 0.5 : (double) sb.q.getCurrentValue());
                for (auto& f : soloFilter)
                    f.set (w, k, 0.0, k, 0.0, false);
            }
            double solo[maxChannels][chunkSize];
            const auto p = numChannels == 1 ? EqPlacement::stereo : sb.current.placement;
            const int signals = (p == EqPlacement::stereo) ? numChannels : 1;
            for (int c = 0; c < signals; ++c)
            {
                for (int i = 0; i < n; ++i)
                    solo[c][i] = p == EqPlacement::mid  ? 0.5 * (input[0][i] + input[1][i])
                               : p == EqPlacement::side ? 0.5 * (input[0][i] - input[1][i])
                               : p == EqPlacement::right ? input[1][i]
                               : input[c][i];
                for (int s = 0; s < 2; ++s)
                    soloFilter[s].run (solo[c], n, soloIc1[s][c], soloIc2[s][c]);
            }
            for (int i = 0; i < n; ++i)
            {
                double l = 0.0, r = 0.0;
                switch (p)
                {
                    case EqPlacement::stereo: l = solo[0][i]; r = numChannels > 1 ? solo[1][i] : 0.0; break;
                    case EqPlacement::left:   l = solo[0][i]; break;
                    case EqPlacement::right:  r = solo[0][i]; break;
                    case EqPlacement::mid:    l = r = solo[0][i]; break;
                    case EqPlacement::side:   l = solo[0][i]; r = -solo[0][i]; break;
                }
                const double m = soloMix.getNextValue();
                work[0][i] += m * (l - work[0][i]);
                if (numChannels > 1)
                    work[1][i] += m * (r - work[1][i]);
            }
            for (auto& st : soloIc1) for (auto& v : st) if (! (std::abs (v) < corruptState)) v = 0.0;
            for (auto& st : soloIc2) for (auto& v : st) if (! (std::abs (v) < corruptState)) v = 0.0;
            touched = true;
        }
        else if (soloMix.isSmoothing())
            soloMix.skip (n);

        if (touched)
        {
            anyActive = true;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* dst = buffer.getWritePointer (ch) + start;
                for (int i = 0; i < n; ++i)
                    dst[i] = (float) work[ch][i];
            }
        }
    }

    if (anyActive) // a huge finite input can still overflow the float output; keep it finite
        for (int ch = 0; ch < numChannels; ++ch)
            dspguard::zeroNonFinite (buffer.getWritePointer (ch), numSamples);
}
