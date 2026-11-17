#include "BeatLockEngine.h"
#include <algorithm>
#include <complex>
#include <juce_core/juce_core.h>

namespace glitchkid::beatlock
{
namespace
{
constexpr double pi = 3.14159265358979323846;
inline double toDb (double power) { return 10.0 * std::log10 (power + 1.0e-20); }
inline float finiteOr0 (float x) { return std::isfinite (x) ? x : 0.0f; }
inline double smoothstep (double e0, double e1, double x)
{
    const double t = std::clamp ((x - e0) / (e1 - e0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// Vocal activity detector constants (initial values, checked by the tests; see docs/BEAT_LOCK.md)
constexpr double openAboveFloorDb = 9.0, closeAboveFloorDb = 5.0;    // hysteresis
constexpr double openAbsoluteDb = -55.0, closeAbsoluteDb = -58.0;    // nothing quieter than this is a vocal
constexpr double openBelowPeakDb = 30.0, closeBelowPeakDb = 36.0;    // breaths and room tone well under the recent phrases do not count
constexpr double holdSeconds = 0.08, acquireSeconds = 0.1;
constexpr double floorFallSeconds = 0.05, floorRiseSeconds = 1.5, floorRiseCapDbPerSecond = 10.0;
constexpr double peakFallDbPerSecond = 6.0;
constexpr double gateOpenSeconds = 0.005, gateCloseSeconds = 0.04, voicedSeconds = 0.02; // the gate only switches; RELEASE shapes the recovery
constexpr double silentDb = -70.0;
// conflict
constexpr double conflictOffsetDb = 6.0;   // a band counts as conflicting from "instrumental 6 dB under the vocal" (plus sensitivity)
constexpr double kneeDb = 6.0, slope = 0.5; // soft knee, then 0.5 dB of cut per dB of excess
constexpr double vocalRelevanceRangeDb = 20.0, instrumentalPresenceRangeDb = 30.0, absoluteFloorDb = -60.0;
constexpr double neighbourSpill = 0.35;    // a band where the vocal also lives takes at least 35 % of the sum of its neighbours' cuts: broad, not notchy
constexpr double snapDb = 0.05;            // a cut this close to 0 dB with nothing asking for more finishes at exactly 0 (ramped: inaudible)
constexpr double voicedOnlyBelowHz = 3000.0; // under this, only voiced sound (vowels) may drive a cut; S sounds and breaths drive the top bands only
}

Settings::Settings()
{
    for (int b = 0; b < numBands; ++b)
        bands[(size_t) b] = { true, defaultFreqs[(size_t) b], defaults::bandQ, defaults::bandMaxCut };
}

void Svf::setG (double fs, double hz)
{
    g = std::tan (pi * std::clamp (hz, 10.0, 0.45 * fs) / fs);
}

void Svf::set (double fs, double hz, double q)
{
    setG (fs, hz);
    k = 1.0 / std::max (0.05, q);
    coeffs();
}

double Svf::bellResponseDb (double hz, double centreHz, double q, double gainDb, double fs)
{
    const double g = std::tan (pi * std::clamp (centreHz, 10.0, 0.45 * fs) / fs);
    const double w = std::tan (pi * std::clamp (hz, 1.0, 0.4999 * fs) / fs) / g;
    const double A = std::pow (10.0, gainDb / 40.0), k = 1.0 / (std::max (0.05, q) * A);
    const std::complex<double> s (0.0, w);
    const auto h = (s * s + k * A * A * s + 1.0) / (s * s + k * s + 1.0);
    return 20.0 * std::log10 (std::abs (h) + 1.0e-30);
}

float Engine::focusWeight (float hz, float low, float mid, float high)
{
    const double x = std::log2 (std::max (1.0f, hz));
    const double t1 = smoothstep (std::log2 (500.0) - 0.5, std::log2 (500.0) + 0.5, x);
    const double t2 = smoothstep (std::log2 (2500.0) - 0.5, std::log2 (2500.0) + 0.5, x);
    return (float) (low * (1.0 - t1) + mid * (t1 - t2) + high * t2);
}

void Engine::prepare (double sampleRate)
{
    fs = sampleRate > 1000.0 ? sampleRate : 48000.0;
    controlRate = fs / controlStep;
    scHp.set (fs, 100.0, 0.7071);
    scLp.set (fs, 10000.0, 0.7071);
    scVoicedLp.set (fs, 1000.0, 0.7071);
    scVoicedHp.set (fs, 4000.0, 0.7071);
    aLevelAtt = coef (0.005, fs);
    aLevelRel = coef (0.025, fs); // fast enough that the gate closes soon after a phrase ends (about 170 dB/s)
    aVoiceEnv = coef (0.01, fs);
    aDetAtt = coef (0.01, fs);
    aDetRel = coef (0.1, fs);
    reset();
}

void Engine::reset()
{
    for (auto* s : { &scHpS, &scLpS, &scVoicedLpS, &scVoicedHpS })
        s->clear();
    levelEnv = lowEnv = highEnv = 0.0;
    floorDb = -100.0;
    peakDb = -120.0;
    gateSm = voicedSm = 0.0;
    floorInit = gateOpen = false;
    holdLeft = acquireLeft = 0;
    anyBandActiveCache = false;
    for (int b = 0; b < numBands; ++b)
    {
        auto& d = det[(size_t) b];
        d.vS.clear();
        d.vS2.clear();
        for (auto& s : d.iS) s.clear();
        for (auto& s : d.iS2) s.clear();
        d.vEnv = 0.0;
        for (auto& e : d.iEnv) e = 0.0;
        d.detFreq = d.detQ = -1.0f;
        auto& e = bell[(size_t) b];
        for (auto& s : e.s) s.clear();
        e.cutDb = 0.0;
        e.ampNow = e.ampTarget = 1.0;
        e.ampStep = 0.0;
        e.active = false;
        e.freqSm = settings.bands[(size_t) b].freqHz;
        e.invQ = 1.0 / std::max (0.05f, settings.bands[(size_t) b].q);
        e.f.setG (fs, e.freqSm);
        shownCut[(size_t) b].store (0.0f, std::memory_order_relaxed);
        shownV[(size_t) b].store (-120.0f, std::memory_order_relaxed);
        shownI[(size_t) b].store (-120.0f, std::memory_order_relaxed);
    }
    wet = settings.bypass ? 0.0 : 1.0;
    wetStep = 0.0;
    stepLeft = 0;
    shownGate.store (0.0f, std::memory_order_relaxed);
    shownVoiced.store (0.0f, std::memory_order_relaxed);
}

bool Engine::anyBandActive() const
{
    for (const auto& e : bell)
        if (e.active)
            return true;
    return false;
}

void Engine::process (float* const* main, int numMain, const float* const* sc, int numSc, int n, float* scMonoOut, float* gateOut)
{
    numMain = std::clamp (numMain, 0, maxChannels);
    numSc = sc == nullptr ? 0 : std::clamp (numSc, 0, maxChannels);
    if (numMain == 0 || main == nullptr || n <= 0)
        return;
    const bool connected = numSc > 0;
    if (numMain != lastNumMain)
    {
        lastNumMain = numMain;
        for (auto& d : det) for (auto& e : d.iEnv) e = 0.0;
    }
    const double wetTarget = settings.bypass ? 0.0 : 1.0, wetDelta = 1.0 / (0.02 * fs);

    for (int i = 0; i < n; ++i)
    {
        // ---- the vocal: listened to, never output ----
        float s = 0.0f;
        for (int c = 0; c < numSc; ++c)
            s += finiteOr0 (sc[c][i]);
        if (numSc > 1)
            s *= 0.5f;
        double v1, v2;
        scHp.tick (scHpS, s, v1, v2);
        const double hp = s - scHp.k * v1 - v2;
        scLp.tick (scLpS, hp, v1, v2);
        const double sp = v2; // the speech band
        const double e = sp * sp;
        levelEnv += (e > levelEnv ? aLevelAtt : aLevelRel) * (e - levelEnv);
        scVoicedLp.tick (scVoicedLpS, sp, v1, v2);
        lowEnv += aVoiceEnv * (v2 * v2 - lowEnv);
        scVoicedHp.tick (scVoicedHpS, sp, v1, v2);
        const double hi = sp - scVoicedHp.k * v1 - v2;
        highEnv += aVoiceEnv * (hi * hi - highEnv);

        // ---- the instrumental (sanitised in place; the dry path for the bypass fade is this sanitised input) ----
        double x[maxChannels] {};
        for (int c = 0; c < numMain; ++c)
        {
            const float in = main[c][i];
            x[c] = std::isfinite (in) ? in : 0.0;
            if (! std::isfinite (in))
                main[c][i] = 0.0f;
        }

        for (int b = 0; b < numBands; ++b)
        {
            auto& d = det[(size_t) b];
            d.vf.tick (d.vS, sp, v1, v2);
            d.vf.tick (d.vS2, d.vf.k * v1, v1, v2);
            const double yv = d.vf.k * v1;
            d.vEnv += (yv * yv > d.vEnv ? aDetAtt : aDetRel) * (yv * yv - d.vEnv);
            for (int c = 0; c < numMain; ++c)
            {
                d.vf.tick (d.iS[c], x[c], v1, v2);
                d.vf.tick (d.iS2[c], d.vf.k * v1, v1, v2);
                const double yi = d.vf.k * v1;
                d.iEnv[c] += (yi * yi > d.iEnv[c] ? aDetAtt : aDetRel) * (yi * yi - d.iEnv[c]);
            }
        }

        if (--stepLeft <= 0)
        {
            stepLeft = controlStep;
            control (connected);
        }

        // ---- the dynamic bells (only bands with a cut run at all) ----
        double y[maxChannels] = { x[0], x[1] };
        for (auto& bl : bell)
        {
            if (! bl.active)
                continue;
            bl.ampNow += bl.ampStep;
            const double A = bl.ampNow, k = bl.invQ / A;
            bl.f.k = k;
            bl.f.coeffs();
            const double m1 = k * (A * A - 1.0);
            for (int c = 0; c < numMain; ++c)
            {
                bl.f.tick (bl.s[c], y[c], v1, v2);
                y[c] += m1 * v1;
            }
        }

        // ---- bypass crossfade (exact at both ends) ----
        if (wet != wetTarget)
            wet = wetTarget > wet ? std::min (wetTarget, wet + wetDelta) : std::max (wetTarget, wet - wetDelta);
        if (wet >= 1.0)
        {
            if (anyBandActiveCache)
                for (int c = 0; c < numMain; ++c)
                    main[c][i] = (float) y[c];
        }
        else if (wet > 0.0)
            for (int c = 0; c < numMain; ++c)
                main[c][i] = (float) (x[c] + (y[c] - x[c]) * wet);
        // wet == 0: the input stays untouched

        if (scMonoOut != nullptr) scMonoOut[i] = s;
        if (gateOut != nullptr) gateOut[i] = gateOpen ? 1.0f : 0.0f;
    }
}

void Engine::control (bool connected)
{
    const double L = toDb (levelEnv);
    // ---- vocal activity: adaptive noise floor, hysteresis, hold ----
    if (! connected)
    {
        floorInit = gateOpen = false;
        holdLeft = 0;
    }
    else
    {
        if (! floorInit)
        {
            floorDb = std::max (L, -100.0);
            floorInit = true;
            acquireLeft = std::max (1, (int) std::lround (acquireSeconds * controlRate));
        }
        if (acquireLeft > 0)
        {
            // learning the floor: it follows the level both ways, so a constant noise present from the start (hiss, room tone) becomes the floor
            --acquireLeft;
            floorDb += coef (0.02, controlRate) * (std::max (L, -100.0) - floorDb);
        }
        else if (L < floorDb)
            floorDb += coef (floorFallSeconds, controlRate) * (L - floorDb);
        else
            floorDb += std::min (coef (floorRiseSeconds, controlRate) * (L - floorDb), floorRiseCapDbPerSecond / controlRate);
        floorDb = std::max (floorDb, -100.0);
        peakDb = L > peakDb ? L : std::max (L, peakDb - peakFallDbPerSecond / controlRate);
        const double openThr = std::max ({ floorDb + openAboveFloorDb, openAbsoluteDb, peakDb - openBelowPeakDb });
        const double closeThr = std::max ({ floorDb + closeAboveFloorDb, closeAbsoluteDb, peakDb - closeBelowPeakDb });
        const int holdSteps = std::max (1, (int) std::lround (holdSeconds * controlRate));
        if (! gateOpen)
        {
            if (acquireLeft == 0 && L > openThr)
            {
                gateOpen = true;
                holdLeft = holdSteps;
            }
        }
        else if (L > closeThr)
            holdLeft = holdSteps;
        else if (--holdLeft <= 0)
            gateOpen = false;
    }
    gateSm += coef (gateOpen ? gateOpenSeconds : gateCloseSeconds, controlRate) * ((gateOpen ? 1.0 : 0.0) - gateSm);
    if (! gateOpen && gateSm < 1.0e-3)
        gateSm = 0.0; // closed means closed: the cuts can then return to exactly 0 dB
    const double voicedNow = std::clamp ((toDb (lowEnv) - toDb (highEnv) + 3.0) / 6.0, 0.0, 1.0);
    voicedSm += coef (voicedSeconds, controlRate) * (voicedNow - voicedSm);

    // ---- per band: follow the settings, measure, decide ----
    const auto& S = settings;
    const double aFreq = coef (0.02, controlRate);
    std::array<double, numBands> V {}, I {}, raw {}, limit {}, relevanceOf {};
    double vMax = -200.0, iMax = -200.0;
    for (int b = 0; b < numBands; ++b)
    {
        auto& bl = bell[(size_t) b];
        auto& d = det[(size_t) b];
        const auto& bs = S.bands[(size_t) b];
        const double wantFreq = std::clamp ((double) bs.freqHz, 20.0, 0.45 * fs);
        bl.freqSm = std::exp (std::log (bl.freqSm) + aFreq * (std::log (wantFreq) - std::log (bl.freqSm)));
        bl.invQ += aFreq * (1.0 / std::max (0.05f, bs.q) - bl.invQ);
        bl.f.setG (fs, bl.freqSm);
        if (std::abs (d.detFreq - bl.freqSm) > 0.001 * bl.freqSm || std::abs (d.detQ - 1.0 / bl.invQ) > 0.001)
        {
            d.vf.set (fs, bl.freqSm, 1.0 / bl.invQ);
            d.detFreq = (float) bl.freqSm;
            d.detQ = (float) (1.0 / bl.invQ);
        }
        V[(size_t) b] = toDb (d.vEnv);
        double ip = 0.0;
        for (int c = 0; c < lastNumMain; ++c) ip += d.iEnv[c];
        I[(size_t) b] = toDb (ip / lastNumMain); // stereo-linked: the power mean of the channels
        if (bs.on)
        {
            vMax = std::max (vMax, V[(size_t) b]);
            iMax = std::max (iMax, I[(size_t) b]);
        }
        limit[(size_t) b] = std::min (std::abs ((double) S.maxCutDb), std::abs ((double) bs.maxCutDb)); // the milder of the two limits
    }
    for (int b = 0; b < numBands; ++b)
    {
        const auto& bs = S.bands[(size_t) b];
        if (! connected || ! bs.on || S.amount <= 0.0f || limit[(size_t) b] <= 0.0)
            continue;
        const double v = V[(size_t) b], in = I[(size_t) b];
        const double relevance = std::clamp ((v - std::max (vMax - vocalRelevanceRangeDb, absoluteFloorDb)) / 10.0, 0.0, 1.0);
        relevanceOf[(size_t) b] = relevance;
        const double x = in - v + conflictOffsetDb + S.sensitivityDb;
        const double y = x <= -kneeDb * 0.5 ? 0.0 : x >= kneeDb * 0.5 ? x : (x + kneeDb * 0.5) * (x + kneeDb * 0.5) / (2.0 * kneeDb);
        const double focus = focusWeight ((float) bell[(size_t) b].freqSm, S.focusLow, S.focusMid, S.focusHigh);
        const double voicing = bell[(size_t) b].freqSm >= voicedOnlyBelowHz ? 1.0 : voicedSm;
        raw[(size_t) b] = -std::min (limit[(size_t) b], S.amount * focus * slope * y) * relevance * gateSm * voicing;
    }
    anyBandActiveCache = false;
    const double aAtt = coef (std::max (0.001, S.attackMs * 0.001), controlRate), aRel = coef (std::max (0.01, S.releaseMs * 0.001), controlRate);
    for (int b = 0; b < numBands; ++b)
    {
        const auto& bs = S.bands[(size_t) b];
        auto& bl = bell[(size_t) b];
        double target = 0.0;
        if (connected && bs.on)
        {
            const double left = b > 0 && S.bands[(size_t) b - 1].on ? raw[(size_t) b - 1] : 0.0;
            const double right = b + 1 < numBands && S.bands[(size_t) b + 1].on ? raw[(size_t) b + 1] : 0.0;
            const double spread = relevanceOf[(size_t) b] > 0.0 ? std::min (raw[(size_t) b], neighbourSpill * (left + right)) : raw[(size_t) b];
            const double presence = std::clamp ((I[(size_t) b] - std::max (absoluteFloorDb, iMax - instrumentalPresenceRangeDb)) / 12.0, 0.0, 1.0);
            target = std::max (spread * presence, -limit[(size_t) b]);
        }
        bl.cutDb += (target < bl.cutDb ? aAtt : aRel) * (target - bl.cutDb);
        if (target == 0.0 && bl.cutDb > -snapDb)
            bl.cutDb = 0.0;
        if (bl.cutDb < 0.0 || target < 0.0)
        {
            if (! bl.active)
            {
                bl.active = true;
                for (auto& s : bl.s) s.clear();
                bl.ampNow = 1.0;
            }
            bl.ampTarget = std::pow (10.0, bl.cutDb / 40.0);
            bl.ampStep = (bl.ampTarget - bl.ampNow) / controlStep;
        }
        else if (bl.active)
        {
            if (std::abs (bl.ampNow - 1.0) < 1.0e-9) // the ramp back to 0 dB has finished: stop processing this band (exact pass-through)
            {
                bl.active = false;
                bl.ampNow = bl.ampTarget = 1.0;
                bl.ampStep = 0.0;
                for (auto& s : bl.s) s.clear();
            }
            else
            {
                bl.ampTarget = 1.0;
                bl.ampStep = (1.0 - bl.ampNow) / controlStep;
            }
        }
        anyBandActiveCache = anyBandActiveCache || bl.active;
        shownCut[(size_t) b].store ((float) bl.cutDb, std::memory_order_relaxed);
        shownV[(size_t) b].store ((float) V[(size_t) b], std::memory_order_relaxed);
        shownI[(size_t) b].store ((float) I[(size_t) b], std::memory_order_relaxed);
    }
    shownGate.store ((float) gateSm, std::memory_order_relaxed);
    shownVoiced.store ((float) voicedSm, std::memory_order_relaxed);
    shownLevel.store ((float) L, std::memory_order_relaxed);
    shownFloor.store ((float) floorDb, std::memory_order_relaxed);
}
}
