#include "CleanLab.h"
#include "Finite.h"
#include <algorithm>

namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr float smoothSeconds = 0.02f;
float coef (double fs, double seconds) { return 1.0f - (float) std::exp (-1.0 / (seconds * fs)); }
float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
constexpr double essHz = 7500.0, essQ = 0.9; // the de-esser lowers a wide bell centred on the S region (about 4.5-12.5 kHz)
constexpr float rumbleHz = 70.0f, mudQ = 0.9f, resQ = 6.0f;
constexpr float mudStartDb = 6.5f; // excess over the vocal's own tilt before any low-mid cut starts
constexpr float maxMudCutDb = 3.5f, maxResCutDb = 6.0f, maxEssDb = 9.0f;
constexpr float resFloorDb = 4.0f, resKeepDb = 2.5f, resProminenceDb = 3.0f; // an excess above the surroundings that creates / keeps a resonance slot
constexpr int minVoicedForRes = 150; // frames of 512 samples: about 1.6 s of voiced material
constexpr float minMoveEvidence = 60.0f; // accumulated 'the voice is moving' weight (frames) before any corrective or resonance decision is made
}

void CleanLab::Biquad::highpass (double fs, double f, double q)
{
    const double w0 = 2.0 * pi * juce::jmin (f, 0.45 * fs) / fs, c = std::cos (w0), s = std::sin (w0), alpha = s / (2.0 * q), a0 = 1.0 + alpha;
    b0 = (1.0 + c) * 0.5 / a0;  b1 = -(1.0 + c) / a0;  b2 = b0;  a1 = -2.0 * c / a0;  a2 = (1.0 - alpha) / a0;
}

void CleanLab::Biquad::lowpass (double fs, double f, double q)
{
    const double w0 = 2.0 * pi * juce::jmin (f, 0.45 * fs) / fs, c = std::cos (w0), s = std::sin (w0), alpha = s / (2.0 * q), a0 = 1.0 + alpha;
    b0 = (1.0 - c) * 0.5 / a0;  b1 = (1.0 - c) / a0;  b2 = b0;  a1 = -2.0 * c / a0;  a2 = (1.0 - alpha) / a0;
}

void CleanLab::Biquad::bandpass (double fs, double f, double q) // constant 0 dB peak gain
{
    const double w0 = 2.0 * pi * juce::jmin (f, 0.45 * fs) / fs, c = std::cos (w0), s = std::sin (w0), alpha = s / (2.0 * q), a0 = 1.0 + alpha;
    b0 = alpha / a0;  b1 = 0.0;  b2 = -alpha / a0;  a1 = -2.0 * c / a0;  a2 = (1.0 - alpha) / a0;
}

void CleanLab::Biquad::peaking (double fs, double f, double q, double gainDb)
{
    const double A = std::pow (10.0, gainDb / 40.0), w0 = 2.0 * pi * juce::jmin (f, 0.45 * fs) / fs, c = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
    const double a0 = 1.0 + alpha / A;
    b0 = (1.0 + alpha * A) / a0;  b1 = -2.0 * c / a0;  b2 = (1.0 - alpha * A) / a0;  a1 = b1;  a2 = (1.0 - alpha / A) / a0;
}

void CleanLab::Bell::update (double fs)
{
    for (auto& biquad : f)
        biquad.peaking (fs, freq.getCurrentValue(), q, gainDb.getCurrentValue());
}

CleanLab::CleanLab()
{
    for (int i = 0; i < numRes; ++i)
    {
        tResHz[i].store (0.0f);
        tResDb[i].store (0.0f);
    }
}

void CleanLab::prepare (double sr)
{
    sampleRate = sr > 8000.0 ? sr : 48000.0;
    for (auto* s : { &onGain, &rumbleMix, &essDepth })
        s->reset (sampleRate, smoothSeconds);
    mud.gainDb.reset (sampleRate, 0.25);
    mud.freq.reset (sampleRate, 0.5);
    mud.q = mudQ;
    for (auto& r : res)
    {
        r.gainDb.reset (sampleRate, 0.25);
        r.freq.reset (sampleRate, 0.5);
        r.q = resQ;
    }
    for (auto& f : rumble)
        f.highpass (sampleRate, rumbleHz, 0.7071);
    sibHp.highpass (sampleRate, 4500.0, 0.7071);
    bodyBp.bandpass (sampleRate, 1000.0, 0.5);
    {
        const double w0 = 2.0 * pi * essHz / sampleRate; // the de-esser's bell: fixed centre and width, only its gain follows the sibilance
        essCos = std::cos (w0);
        essAlpha = std::sin (w0) / (2.0 * essQ);
    }
    envAtk = coef (sampleRate, 0.0003);
    envRel = coef (sampleRate, 0.03);
    grAtk = coef (sampleRate, 0.0005);
    grRel = coef (sampleRate, 0.04);
    alphaRes = 1.0f - (float) std::exp (-(double) hop / (1.5 * sampleRate));
    betaLong = 1.0f - (float) std::exp (-(double) hop / (3.0 * sampleRate));
    leakRes = 1.0f - (float) std::exp (-(double) hop / (20.0 * sampleRate));

    ring.assign ((size_t) fftSize, 0.0f);
    window.resize ((size_t) fftSize);
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * (float) pi * (float) i / (float) fftSize);
    fftBuf.assign ((size_t) fftSize * 2, 0.0f);
    for (auto* v : { &S, &smoothDb, &env, &excess, &avgExcess, &oldExcess, &longTerm, &longNum, &line })
        v->assign ((size_t) numBins + 2, 0.0f);
    pre.assign ((size_t) numBins + 3, 0.0f);
    reset();
}

void CleanLab::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    for (auto* v : { &S, &smoothDb, &env, &excess, &avgExcess, &oldExcess, &longTerm, &longNum, &line })
        std::fill (v->begin(), v->end(), 0.0f);
    ringPos = sinceHop = voicedFrames = framesSinceSnapshot = framesSinceTargets = 0;
    haveOld = haveLong = false;
    moveEvidence = longDen = 0.0f;
    std::fill (longNum.begin(), longNum.end(), 0.0f);
    rumbleNeed = mudWantDb = 0.0f;
    mudWantHz = 250.0f;
    for (int i = 0; i < numRes; ++i)
        resSlotHz[i] = resSlotWant[i] = 0.0f;
    park();
    onGain.setCurrentAndTargetValue (target.on ? 1.0f : 0.0f);
}

void CleanLab::setLearningFrozen (bool frozen)
{
    learningFrozen = frozen;
}

void CleanLab::park()
{
    idleReset();
}

void CleanLab::idleReset()
{
    for (auto& f : rumble) f.clear();
    for (auto& f : essBell) f.clear();
    for (auto& f : mud.f) f.clear();
    for (auto& r : res)
        for (auto& f : r.f)
            f.clear();
    sibHp.clear();
    bodyBp.clear();
    sibEnv = bodyEnv = grDb = 0.0f;
    mud.gainDb.setCurrentAndTargetValue (0.0f);
    for (auto& r : res)
        r.gainDb.setCurrentAndTargetValue (0.0f);
    rumbleMix.setCurrentAndTargetValue (0.0f);
    essDepth.setCurrentAndTargetValue (target.amount * target.ess);
    levelCorr = levelEss = 0.0f;
    tRumbleAmount.store (0.0f);
    tMudDb.store (0.0f);
    tEss.store (0.0f);
    tLevel.store (0.0f);
    for (int i = 0; i < numRes; ++i)
        tResDb[i].store (0.0f);
}

void CleanLab::setSettings (const Settings& s)
{
    auto fix = [] (float v) { return clampf (std::isfinite (v) ? v : 0.0f, 0.0f, 1.0f); };
    Settings n { s.on, fix (s.amount), fix (s.tone), fix (s.res), fix (s.ess) };
    const bool changed = n.on != target.on || ! juce::exactlyEqual (n.amount, target.amount) || ! juce::exactlyEqual (n.tone, target.tone) || ! juce::exactlyEqual (n.res, target.res) || ! juce::exactlyEqual (n.ess, target.ess);
    target = n;
    onGain.setTargetValue (target.on ? 1.0f : 0.0f);
    essDepth.setTargetValue (target.amount * target.ess);
    if (changed && target.on)
        chooseTargets(); // push what has already been learned through the new depths
}

CleanLab::Applied CleanLab::applied() const
{
    Applied a;
    a.rumbleAmount = tRumbleAmount.load (std::memory_order_relaxed);
    a.mudHz = tMudHz.load (std::memory_order_relaxed);
    a.mudDb = tMudDb.load (std::memory_order_relaxed);
    for (int i = 0; i < numRes; ++i)
    {
        a.resHz[i] = tResHz[i].load (std::memory_order_relaxed);
        a.resDb[i] = tResDb[i].load (std::memory_order_relaxed);
    }
    a.essGrDb = tEss.load (std::memory_order_relaxed);
    a.levelChangeDb = tLevel.load (std::memory_order_relaxed);
    a.learnedSeconds = tLearned.load (std::memory_order_relaxed);
    return a;
}

// ---- analysis -------------------------------------------------------------------------------------------------------------------------

void CleanLab::pushForAnalysis (float x)
{
    if (learningFrozen)
        return;
    ring[(size_t) ringPos] = x;
    ringPos = (ringPos + 1) & (fftSize - 1);
    if (++sinceHop >= hop)
    {
        sinceHop = 0;
        analyseFrame();
    }
}

void CleanLab::analyseFrame()
{
    double ms = 0.0;
    for (int i = 0; i < fftSize; ++i)
    {
        const float x = ring[(size_t) ((ringPos + i) & (fftSize - 1))];
        ms += (double) x * (double) x;
        fftBuf[(size_t) i] = x * window[(size_t) i];
    }
    ms /= fftSize;
    if (ms < 1.0e-5) // about -50 dBFS: silence and breath are never learned from
        return;
    std::fill (fftBuf.begin() + fftSize, fftBuf.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (fftBuf.data());
    for (int k = 0; k < numBins; ++k)
        S[(size_t) k] = 20.0f * std::log10 (fftBuf[(size_t) k] + 1.0e-9f);
    ++voicedFrames;
    tLearned.store (juce::jmin (3.0f, (float) voicedFrames * (float) hop / (float) sampleRate), std::memory_order_relaxed);

    // Resonance evidence. The power spectrum is first smoothed over +-1/12 octave (wider than the spacing of a voice's harmonics at 1 kHz and above, so the comb of
    // harmonics is averaged out), then each bin's excess over its +-1/3 octave surroundings (in dB) is taken. Below 1 kHz harmonics are far apart in bins, so that
    // region is not searched for resonances.
    pre[0] = 0.0f;
    for (int k = 0; k < numBins; ++k)
        pre[(size_t) k + 1] = pre[(size_t) k] + std::pow (10.0f, S[(size_t) k] * 0.1f);
    const int kLo = binOf (1000.0f), kHi = binOf (juce::jmin (8000.0f, 0.4f * (float) sampleRate)), kCorr = binOf (6000.0f);
    constexpr float oct12 = 1.0595f, oct3 = 1.2599f;
    const int kFrom = juce::jmax (1, (int) ((float) kLo / oct3) - 2), kTo = juce::jmin (numBins - 1, (int) std::ceil ((float) kHi * oct3) + 2);
    for (int k = kFrom; k <= kTo; ++k)
    {
        const int lo = juce::jmax (1, (int) ((float) k / oct12)), hi = juce::jmin (numBins - 1, (int) std::ceil ((float) k * oct12));
        smoothDb[(size_t) k] = 10.0f * std::log10 ((pre[(size_t) hi + 1] - pre[(size_t) lo]) / (float) (hi - lo + 1) + 1.0e-12f);
    }
    pre[0] = 0.0f;
    for (int k = 0; k < numBins; ++k)
        pre[(size_t) k + 1] = pre[(size_t) k] + smoothDb[(size_t) k];
    for (int k = kLo; k <= kHi; ++k)
    {
        const int lo = juce::jmax (1, (int) ((float) k / oct3)), hi = juce::jmin (numBins - 1, (int) std::ceil ((float) k * oct3));
        env[(size_t) k] = (pre[(size_t) hi + 1] - pre[(size_t) lo]) / (float) (hi - lo + 1);
        excess[(size_t) k] = smoothDb[(size_t) k] - env[(size_t) k];
    }

    // how much the spectrum is moving (pitch glide, vowel change): the shape of the excess now against about 0.34 s ago
    float weight = 0.0f;
    if (haveOld)
    {
        double mA = 0.0, mB = 0.0;
        const int count = kCorr - kLo + 1;
        for (int k = kLo; k <= kCorr; ++k)
        {
            mA += excess[(size_t) k];
            mB += oldExcess[(size_t) k];
        }
        mA /= count;
        mB /= count;
        double cov = 0.0, vA = 0.0, vB = 0.0;
        for (int k = kLo; k <= kCorr; ++k)
        {
            const double a = excess[(size_t) k] - mA, b = oldExcess[(size_t) k] - mB;
            cov += a * b;
            vA += a * a;
            vB += b * b;
        }
        const float c = (float) (cov / std::sqrt (vA * vB + 1.0e-9));
        weight = clampf ((0.95f - c) / 0.25f, 0.0f, 1.0f); // a held note (same shape) teaches nothing; a moving voice teaches the full amount
    }
    if (++framesSinceSnapshot >= 32)
    {
        framesSinceSnapshot = 0;
        std::copy (excess.begin() + kLo, excess.begin() + kHi + 1, oldExcess.begin() + kLo);
        haveOld = true;
    }
    // Long-term level spectrum for rumble and low-mid build-up: a weighted mean of the dB spectrum, where a frame counts only as much as the voice is MOVING.
    // A held note shows a comb of harmonics at low frequencies that says nothing about the voice's own balance, so it is never learned from.
    moveEvidence = moveEvidence * (1.0f - leakRes) + weight;
    if (weight > 0.0f)
    {
        const float b = betaLong * weight;
        longDen = longDen * (1.0f - b) + b;
        for (int k = 0; k < numBins; ++k)
            longNum[(size_t) k] = longNum[(size_t) k] * (1.0f - b) + b * S[(size_t) k];
        for (int k = 0; k < numBins; ++k)
            longTerm[(size_t) k] = longNum[(size_t) k] / juce::jmax (longDen, 1.0e-6f);
    }
    for (int k = kLo; k <= kHi; ++k)
    {
        avgExcess[(size_t) k] += alphaRes * weight * (excess[(size_t) k] - avgExcess[(size_t) k]);
        avgExcess[(size_t) k] -= leakRes * avgExcess[(size_t) k];
    }

    if (++framesSinceTargets >= 4)
    {
        framesSinceTargets = 0;
        chooseTargets();
    }
}

void CleanLab::chooseTargets()
{
    const float depthTone = target.on ? target.amount * target.tone : 0.0f, depthRes = target.on ? target.amount * target.res : 0.0f;
    const float bin = binHz();

    // ---- rumble and low-mid build-up, from the long-term spectrum ----
    float rumbleWant = 0.0f;
    mudWantDb = 0.0f;
    if (moveEvidence >= minMoveEvidence && depthTone > 0.0f)
    {
        auto meanPower = [this] (float f1, float f2)
        {
            const int a = f1 <= 1.0f ? 1 : binOf (f1), b = juce::jmax (a, f2 <= 1.0f ? 1 : binOf (f2));
            double p = 0.0;
            for (int k = a; k <= b; ++k)
                p += std::pow (10.0, (double) longTerm[(size_t) k] / 10.0);
            return p / (double) (b - a + 1);
        };
        const double ratioDb = 10.0 * std::log10 (meanPower (1.0f, 1.0f) / (meanPower (150.0f, 500.0f) + 1.0e-30) + 1.0e-30); // only the lowest bin (about 23 Hz): the window's leakage from a low voice's fundamental does not reach it
        rumbleWant = clampf ((float) (ratioDb + 22.0) / 10.0f, 0.0f, 1.0f) * depthTone;

        // the vocal's own tilt: a straight line through its 700 Hz - 5 kHz region (log frequency), extended downward
        const int k1 = binOf (700.0f), k2 = binOf (juce::jmin (5000.0f, 0.4f * (float) sampleRate));
        double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
        const int n = k2 - k1 + 1;
        for (int k = k1; k <= k2; ++k)
        {
            const double x = std::log2 ((double) k * bin), y = longTerm[(size_t) k];
            sx += x;
            sy += y;
            sxx += x * x;
            sxy += x * y;
        }
        const double den = (double) n * sxx - sx * sx;
        const double slope = std::abs (den) > 1.0e-9 ? ((double) n * sxy - sx * sy) / den : 0.0, icpt = (sy - slope * sx) / (double) n;
        for (int k = binOf (110.0f); k <= binOf (800.0f); ++k)
            line[(size_t) k] = longTerm[(size_t) k] - (float) (icpt + slope * std::log2 ((double) k * bin));
        float best = 0.0f;
        int bestK = binOf (250.0f);
        for (int k = binOf (150.0f); k <= binOf (400.0f); ++k)
        {
            const int lo = juce::jmax (binOf (110.0f), (int) ((float) k / 1.19f)), hi = juce::jmin (binOf (800.0f), (int) std::ceil ((float) k * 1.19f));
            float m = 0.0f;
            for (int j = lo; j <= hi; ++j)
                m += line[(size_t) j];
            m /= (float) (hi - lo + 1);
            if (m > best)
            {
                best = m;
                bestK = k;
            }
        }
        mudWantDb = clampf ((best - mudStartDb) * 0.7f, 0.0f, maxMudCutDb) * depthTone;
        mudWantHz = (float) bestK * bin;
    }
    rumbleNeed = rumbleWant;
    rumbleMix.setTargetValue (rumbleWant);
    if (mud.gainDb.getCurrentValue() > -0.05f && mudWantDb > 0.0f)
        mud.freq.setCurrentAndTargetValue (mudWantHz);
    else if (mudWantDb > 0.0f)
        mud.freq.setTargetValue (mudWantHz);
    mud.gainDb.setTargetValue (-mudWantDb);

    // ---- resonances, from the movement-weighted excess ----
    const int kLo = binOf (1000.0f), kHi = binOf (juce::jmin (8000.0f, 0.4f * (float) sampleRate));
    struct Cand { float v, hz; };
    Cand cands[numRes] {};
    int numCands = 0;
    if (moveEvidence >= minMoveEvidence && voicedFrames >= minVoicedForRes && depthRes > 0.0f)
    {
        Cand found[32]; // fixed storage: nothing is allocated on the audio thread
        int numFound = 0;
        for (int k = kLo + 2; k <= kHi - 2; ++k)
        {
            const float v = avgExcess[(size_t) k];
            const int kn = juce::jmax (3, (int) std::lround ((float) k * 0.19f)); // +-1/4 octave
            if (k - kn >= kLo && k + kn <= kHi && v >= resFloorDb && v >= avgExcess[(size_t) k - 1] && v > avgExcess[(size_t) k + 1] && v >= avgExcess[(size_t) k - 2] && v > avgExcess[(size_t) k + 2]
                && v - 0.5f * (avgExcess[(size_t) (k - kn)] + avgExcess[(size_t) (k + kn)]) >= resProminenceDb)
            {
                const float a = avgExcess[(size_t) k - 1], b = avgExcess[(size_t) k + 1], d = a - 2.0f * v + b;
                const float delta = std::abs (d) > 1.0e-6f ? clampf (0.5f * (a - b) / d, -0.5f, 0.5f) : 0.0f;
                if (numFound < 32)
                    found[numFound++] = { v, ((float) k + delta) * bin };
            }
        }
        std::sort (found, found + numFound, [] (const Cand& x, const Cand& y) { return x.v > y.v; });
        for (int fi = 0; fi < numFound; ++fi)
        {
            const auto& c = found[fi];
            bool spaced = true;
            for (int i = 0; i < numCands; ++i)
                if (c.hz / cands[i].hz < 1.26f && cands[i].hz / c.hz < 1.26f)
                    spaced = false;
            if (spaced && numCands < numRes)
                cands[numCands++] = c;
        }
    }
    auto cutFor = [depthRes] (float v) { return clampf (v - 2.5f, 0.0f, maxResCutDb) * depthRes; };
    bool slotTaken[numRes] {};
    float want[numRes] {}, hzFor[numRes] {};
    for (int i = 0; i < numRes; ++i) // existing slots stay on their resonance while it is still there
    {
        if (resSlotWant[i] <= 0.0f || resSlotHz[i] <= 0.0f)
            continue;
        const int kc = binOf (resSlotHz[i]);
        float v = 0.0f;
        for (int k = juce::jmax (kLo, kc - 2); k <= juce::jmin (kHi, kc + 2); ++k)
            v = juce::jmax (v, avgExcess[(size_t) k]);
        if (v >= resKeepDb && depthRes > 0.0f)
        {
            slotTaken[i] = true;
            want[i] = cutFor (v);
            hzFor[i] = resSlotHz[i];
        }
    }
    for (int c = 0; c < numCands; ++c)
    {
        bool known = false;
        for (int i = 0; i < numRes; ++i)
            if (slotTaken[i] && cands[c].hz / hzFor[i] < 1.19f && hzFor[i] / cands[c].hz < 1.19f)
                known = true;
        if (known)
            continue;
        for (int i = 0; i < numRes; ++i)
            if (! slotTaken[i] && res[i].gainDb.getCurrentValue() > -0.05f)
            {
                slotTaken[i] = true;
                want[i] = cutFor (cands[c].v);
                hzFor[i] = cands[c].hz;
                res[i].freq.setCurrentAndTargetValue (cands[c].hz); // a new slot starts silent, so it can jump to its frequency
                break;
            }
    }
    for (int i = 0; i < numRes; ++i)
    {
        resSlotWant[i] = slotTaken[i] ? juce::jmax (want[i], 0.001f) : 0.0f;
        if (slotTaken[i])
        {
            resSlotHz[i] = hzFor[i];
            res[i].freq.setTargetValue (hzFor[i]);
        }
        res[i].gainDb.setTargetValue (slotTaken[i] ? -want[i] : 0.0f);
    }
}

// ---- processing -----------------------------------------------------------------------------------------------------------------------

void CleanLab::processCorrective (juce::AudioBuffer<float>& buffer)
{
    const int numChannels = juce::jmin (buffer.getNumChannels(), maxChannels), n = buffer.getNumSamples();
    if (numChannels <= 0 || n <= 0)
        return;
    if (! target.on && ! onGain.isSmoothing() && onGain.getCurrentValue() <= 0.0f)
    {
        idleReset(); // settled off: an exact pass-through; what was learned is kept
        return;
    }
    float* d[maxChannels] = { buffer.getWritePointer (0), numChannels > 1 ? buffer.getWritePointer (1) : nullptr };
    for (int ch = 0; ch < numChannels; ++ch)
        dspguard::zeroNonFinite (d[ch], n);

    double inE = 0.0, outE = 0.0;
    for (int start = 0; start < n; start += 32)
    {
        const int len = juce::jmin (32, n - start);
        mud.gainDb.skip (len);
        mud.freq.skip (len);
        if (mud.active())
            mud.update (sampleRate);
        for (auto& r : res)
        {
            r.gainDb.skip (len);
            r.freq.skip (len);
            if (r.active())
                r.update (sampleRate);
        }
        const bool mudOn = mud.active();
        bool resOn[numRes];
        for (int i = 0; i < numRes; ++i)
            resOn[i] = res[i].active();

        for (int i = start; i < start + len; ++i)
        {
            const float on = onGain.getNextValue(), rm = rumbleMix.getNextValue();
            float mono = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                mono += d[ch][i];
            mono /= (float) numChannels;
            if (target.on)
                pushForAnalysis (mono);
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float x = d[ch][i];
                float y = x + rm * (rumble[ch].process (x) - x);
                if (mudOn)
                    y = mud.f[ch].process (y);
                for (int r = 0; r < numRes; ++r)
                    if (resOn[r])
                        y = res[r].f[ch].process (y);
                const float out = x + on * (y - x);
                inE += (double) x * (double) x;
                outE += (double) out * (double) out;
                d[ch][i] = out;
            }
        }
    }
    if (inE > 1.0e-9)
        levelCorr += 0.1f * ((float) (10.0 * std::log10 ((outE + 1.0e-12) / (inE + 1.0e-12))) - levelCorr);
    else
        levelCorr *= 0.98f;
    tLevel.store (levelCorr + levelEss, std::memory_order_relaxed);
    tRumbleAmount.store (rumbleMix.getCurrentValue(), std::memory_order_relaxed);
    tMudHz.store (mud.freq.getCurrentValue(), std::memory_order_relaxed);
    tMudDb.store (juce::jmax (0.0f, -mud.gainDb.getCurrentValue()) * onGain.getCurrentValue(), std::memory_order_relaxed);
    for (int i = 0; i < numRes; ++i)
    {
        tResHz[i].store (res[i].freq.getCurrentValue(), std::memory_order_relaxed);
        tResDb[i].store (juce::jmax (0.0f, -res[i].gainDb.getCurrentValue()) * onGain.getCurrentValue(), std::memory_order_relaxed);
    }
}

void CleanLab::processDeEss (juce::AudioBuffer<float>& buffer)
{
    const int numChannels = juce::jmin (buffer.getNumChannels(), maxChannels), n = buffer.getNumSamples();
    if (numChannels <= 0 || n <= 0)
        return;
    if (! target.on && ! onGain.isSmoothing() && onGain.getCurrentValue() <= 0.0f)
        return; // the corrective stage's idle reset already cleared this stage's state
    float* d[maxChannels] = { buffer.getWritePointer (0), numChannels > 1 ? buffer.getWritePointer (1) : nullptr };
    for (int ch = 0; ch < numChannels; ++ch)
        dspguard::zeroNonFinite (d[ch], n);

    double inE = 0.0, outE = 0.0;
    essBlockMax = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float depth = essDepth.getNextValue(), on = onGain.getCurrentValue();
        float mono = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            mono += d[ch][i];
        mono /= (float) numChannels;
        const float s = std::abs (sibHp.process (mono)), b = std::abs (bodyBp.process (mono));
        sibEnv += (s > sibEnv ? envAtk : envRel) * (s - sibEnv);
        bodyEnv += (b > bodyEnv ? envAtk : envRel) * (b - bodyEnv);

        float wanted = 0.0f;
        if (depth > 0.001f && sibEnv > 3.0e-4f) // quiet highs are never reduced
        {
            const float r = sibEnv / (bodyEnv + 1.0e-4f), threshold = essThreshold - 0.9f * depth;
            const float x = 20.0f * std::log10 (r / threshold + 1.0e-9f); // dB above the threshold
            const float over = x <= -1.5f ? 0.0f : (x < 1.5f ? (x + 1.5f) * (x + 1.5f) / 6.0f : x); // 3 dB soft knee
            wanted = juce::jmin (0.75f * over, maxEssDb * depth); // ratio 4
        }
        grDb += (wanted > grDb ? grAtk : grRel) * (wanted - grDb);
        essBlockMax = juce::jmax (essBlockMax, grDb * on);
        const bool reducing = grDb > 0.01f;
        if (reducing && (++essTick & 3) == 1) // the bell's coefficients follow the gain every 4 samples (one pow, no trig)
        {
            const double A = std::pow (10.0, -(double) grDb / 40.0), a0 = 1.0 + essAlpha / A;
            for (auto& f : essBell)
            {
                f.b0 = (1.0 + essAlpha * A) / a0;  f.b1 = -2.0 * essCos / a0;  f.b2 = (1.0 - essAlpha * A) / a0;
                f.a1 = f.b1;  f.a2 = (1.0 - essAlpha / A) / a0;
            }
        }
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float x = d[ch][i];
            const float y = reducing ? essBell[ch].process (x) : x; // no reduction: an exact pass-through
            const float out = x + on * (y - x);
            inE += (double) x * (double) x;
            outE += (double) out * (double) out;
            d[ch][i] = out;
        }
    }
    if (inE > 1.0e-9)
        levelEss += 0.1f * ((float) (10.0 * std::log10 ((outE + 1.0e-12) / (inE + 1.0e-12))) - levelEss);
    else
        levelEss *= 0.98f;
    tLevel.store (levelCorr + levelEss, std::memory_order_relaxed);
    tEss.store (essBlockMax, std::memory_order_relaxed);
}
