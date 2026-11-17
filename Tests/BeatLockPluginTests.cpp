#include "TestHelpers.h"
#include "../Source/BeatLock/BeatLockProcessor.h"
#include "../Source/BeatLock/BeatLockEditor.h"
#include <set>

long threadAllocations(); // FormantShiftTests.cpp replaces the global operator new and counts per thread

// GLITCHKID BEAT LOCK: the separate instrumental plug-in. The INSTRUMENTAL is processed; the VOCAL on the sidechain is only listened to.
namespace
{
namespace bl = glitchkid::beatlock;
using Vec = std::vector<float>;
constexpr double twoPi = 6.283185307179586;

// a dense "instrumental": pink-ish noise (Paul Kellet's filter), about -18 dBFS RMS
Vec pink (int n, juce::int64 seed, float gain = 0.12f)
{
    juce::Random r (seed);
    Vec x ((size_t) n);
    double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    for (auto& v : x)
    {
        const double w = r.nextFloat() * 2.0 - 1.0;
        b0 = 0.99886 * b0 + w * 0.0555179; b1 = 0.99332 * b1 + w * 0.0750759; b2 = 0.96900 * b2 + w * 0.1538520;
        b3 = 0.86650 * b3 + w * 0.3104856; b4 = 0.55000 * b4 - w * 0.5329522; b5 = -0.7616 * b5 - w * 0.0168980;
        v = (float) (gain * (b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362) * 0.25);
        b6 = w * 0.115926;
    }
    return x;
}

// a sung-like key: partials around `centre` with a syllable envelope and short gaps (voiced: its energy is below 1 kHz unless centre is high)
Vec vocal (int n, double fs, double centre, float amp = 0.25f, double startSec = 0.0)
{
    Vec x ((size_t) n);
    double ph[3] = { 0.0, 0.0, 0.0 };
    const double f[3] = { centre * 0.94, centre, centre * 1.06 };
    for (int i = 0; i < n; ++i)
    {
        const double t = i / fs - startSec;
        if (t < 0) continue;
        const double bar = std::fmod (t, 1.2);
        const double env = bar < 1.0 ? std::pow (std::sin (juce::MathConstants<double>::pi * bar), 0.5) : 0.0;
        const double vib = 1.0 + 0.004 * std::sin (twoPi * 5.0 * t); // a 0.4 % vibrato, as a phase increment (not a growing phase error)
        double s = 0.0;
        for (int k = 0; k < 3; ++k)
        {
            ph[k] += twoPi * f[k] * vib / fs;
            s += std::sin (ph[k]);
        }
        x[(size_t) i] = (float) (amp * env * s / 3.0);
    }
    return x;
}

Vec constTone (int n, double fs, double hz, float amp)
{
    Vec x ((size_t) n);
    for (int i = 0; i < n; ++i) x[(size_t) i] = (float) (amp * std::sin (twoPi * hz * i / fs));
    return x;
}

double rmsDb (const Vec& x, size_t from = 0)
{
    double s = 0;
    for (size_t i = from; i < x.size(); ++i) s += (double) x[i] * x[i];
    return 10.0 * std::log10 (s / (double) std::max<size_t> (1, x.size() - from) + 1e-30);
}

// power in [lo, hi) Hz, averaged over FFT frames of the second half
double bandDb (const Vec& x, double fs, double lo, double hi)
{
    constexpr int order = 13, N = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> w ((size_t) N * 2);
    double total = 0; int frames = 0;
    for (size_t pos = x.size() / 2; pos + N <= x.size(); pos += N / 2)
    {
        for (int i = 0; i < N; ++i) w[(size_t) i] = x[pos + (size_t) i] * (0.5f - 0.5f * std::cos ((float) twoPi * i / N));
        std::fill (w.begin() + N, w.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (w.data());
        for (int k = 0; k < N / 2; ++k) { const double f = k * fs / N; if (f >= lo && f < hi) total += (double) w[(size_t) k] * w[(size_t) k]; }
        ++frames;
    }
    return 10.0 * std::log10 (total / std::max (1, frames) + 1e-30);
}

// run the processor: instrumental L/R (or mono), sidechain channels as given (0, 1 or 2). Sets the bus layout to match first.
struct Run { Vec l, r; };
Run run (BeatLockProcessor& p, const Vec& instL, const Vec* instR, const std::vector<const Vec*>& sc, double fs, int block, bool prepare = true)
{
    const int mainCh = instR != nullptr ? 2 : 1, scCh = (int) sc.size();
    if (prepare)
    {
        auto layout = p.getBusesLayout();
        layout.inputBuses.getReference (0) = mainCh == 2 ? juce::AudioChannelSet::stereo() : juce::AudioChannelSet::mono();
        layout.outputBuses.getReference (0) = layout.inputBuses[0];
        layout.inputBuses.getReference (1) = scCh == 0 ? juce::AudioChannelSet::disabled() : scCh == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
        const bool ok = p.setBusesLayout (layout);
        jassert (ok); juce::ignoreUnused (ok);
        p.prepareToPlay (fs, block);
    }
    const int n = (int) instL.size();
    Run out { Vec ((size_t) n), instR != nullptr ? Vec ((size_t) n) : Vec() };
    juce::MidiBuffer midi;
    for (int pos = 0; pos < n; pos += block)
    {
        const int len = std::min (block, n - pos);
        juce::AudioBuffer<float> b (mainCh + scCh, len);
        for (int i = 0; i < len; ++i)
        {
            b.setSample (0, i, instL[(size_t) (pos + i)]);
            if (mainCh == 2) b.setSample (1, i, (*instR)[(size_t) (pos + i)]);
            for (int c = 0; c < scCh; ++c) b.setSample (mainCh + c, i, (*sc[(size_t) c])[(size_t) (pos + i)]);
        }
        p.processBlock (b, midi);
        for (int i = 0; i < len; ++i)
        {
            out.l[(size_t) (pos + i)] = b.getSample (0, i);
            if (mainCh == 2) out.r[(size_t) (pos + i)] = b.getSample (1, i);
        }
    }
    return out;
}

std::vector<float> values (BeatLockProcessor& p)
{
    std::vector<float> v;
    for (auto* prm : p.getParameters()) v.push_back (prm->getValue());
    return v;
}

void set (BeatLockProcessor& p, const juce::String& id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

struct BeatLockPluginTests : public juce::UnitTest
{
    BeatLockPluginTests() : juce::UnitTest ("Beat Lock plug-in", "GlitchKid") {}

    void runTest() override
    {
        const double fs = 48000.0;

        beginTest ("identity: 41 automatable bl_ parameters with unique IDs, the host's bypass, zero latency, the bus layouts");
        {
            BeatLockProcessor p;
            expectEquals (p.getParameters().size(), 41);
            std::set<juce::String> idsSeen;
            for (auto* prm : p.getParameters())
            {
                auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm);
                expect (r != nullptr && r->paramID.startsWith ("bl_") && r->isAutomatable(), r != nullptr ? r->paramID : "?");
                idsSeen.insert (r->paramID);
            }
            expectEquals ((int) idsSeen.size(), 41, "IDs are unique");
            expect (p.getBypassParameter() == p.apvts.getParameter (bl::ids::bypass), "the host's bypass is bl_bypass");
            expectEquals (p.getLatencySamples(), 0);
            for (int b = 0; b < bl::numBands; ++b)
                expectWithinAbsoluteError (p.apvts.getRawParameterValue (bl::bandIds()[(size_t) b].freq)->load(), bl::defaultFreqs[(size_t) b], 0.5f);
            expectEquals (p.getBusCount (true), 2);
            expect (p.getBus (true, 1)->getName() == "Vocal Sidechain" && p.getBus (true, 1)->isEnabledByDefault());
            auto ok = [&p] (juce::AudioChannelSet in, juce::AudioChannelSet out, juce::AudioChannelSet sc) {
                auto l = p.getBusesLayout();
                l.inputBuses.getReference (0) = in; l.outputBuses.getReference (0) = out; l.inputBuses.getReference (1) = sc;
                return p.checkBusesLayoutSupported (l);
            };
            using CS = juce::AudioChannelSet;
            expect (ok (CS::stereo(), CS::stereo(), CS::stereo()) && ok (CS::stereo(), CS::stereo(), CS::mono()) && ok (CS::stereo(), CS::stereo(), CS::disabled()));
            expect (ok (CS::mono(), CS::mono(), CS::stereo()) && ok (CS::mono(), CS::mono(), CS::disabled()));
            expect (! ok (CS::mono(), CS::stereo(), CS::stereo()), "main in must equal out");
            expect (! ok (CS::create5point1(), CS::create5point1(), CS::stereo()) && ! ok (CS::stereo(), CS::stereo(), CS::create5point1()), "only mono or stereo");
        }

        beginTest ("the filter: the bell's response is exactly its gain at the centre, unity at 0 dB, and the running filter matches the formula");
        {
            for (double g : { -12.0, -6.0, -3.0, -1.0, 0.0 })
                expectWithinAbsoluteError (bl::Svf::bellResponseDb (1000.0, 1000.0, 1.4, g, fs), g, 1.0e-9);
            expectWithinAbsoluteError (bl::Svf::bellResponseDb (300.0, 1000.0, 1.4, 0.0, fs), 0.0, 1.0e-12);
            for (double hz : { 250.0, 700.0, 1000.0, 1500.0, 4000.0 }) // a cut never boosts anywhere
                expect (bl::Svf::bellResponseDb (hz, 1000.0, 1.4, -6.0, fs) <= 1.0e-9);
            // the time-domain filter (as the engine runs it) against the formula
            for (double hz : { 250.0, 1000.0, 3000.0 })
            {
                bl::Svf f;
                f.set (fs, 1000.0, 1.4);
                const double A = std::pow (10.0, -6.0 / 40.0);
                f.k = (1.0 / 1.4) / A;
                f.coeffs();
                const double m1 = f.k * (A * A - 1.0);
                bl::Svf::State st;
                Vec y ((size_t) fs);
                for (int i = 0; i < (int) fs; ++i)
                {
                    double v1, v2;
                    const double x = std::sin (twoPi * hz * i / fs);
                    f.tick (st, x, v1, v2);
                    y[(size_t) i] = (float) (x + m1 * v1);
                }
                const double measured = 20.0 * std::log10 (std::sqrt (2.0) * std::sqrt (std::pow (10.0, rmsDb (y, (size_t) fs / 2) / 10.0)));
                expectWithinAbsoluteError (measured, bl::Svf::bellResponseDb (hz, 1000.0, 1.4, -6.0, fs), 0.05, "at " + juce::String (hz) + " Hz");
            }
        }

        beginTest ("pass-through: no sidechain, a silent sidechain, AMOUNT 0 and bypass leave the instrumental bit-identical");
        {
            const int n = (int) fs * 2;
            const auto l = pink (n, 1), r = pink (n, 2), v = vocal (n, fs, 850.0), silence = Vec ((size_t) n, 0.0f);
            {
                BeatLockProcessor p;
                const auto o = run (p, l, &r, {}, fs, 480);
                expect (o.l == l && o.r == r, "no sidechain: exact");
                BeatLockProcessor m;
                expect (run (m, l, nullptr, {}, fs, 480).l == l, "mono, no sidechain: exact");
            }
            {
                BeatLockProcessor p;
                const auto o = run (p, l, &r, { &silence, &silence }, fs, 480);
                expect (o.l == l && o.r == r, "silent sidechain: exact");
                expect (p.getSidechainStatus() == bl::SidechainStatus::silent, "status: silent");
            }
            {
                BeatLockProcessor p;
                set (p, bl::ids::amount, 0.0f);
                const auto o = run (p, l, &r, { &v, &v }, fs, 480);
                expect (o.l == l && o.r == r, "AMOUNT 0 with an active vocal: exact");
                expect (p.getSidechainStatus() == bl::SidechainStatus::live);
            }
            {
                BeatLockProcessor p;
                set (p, bl::ids::bypass, 1.0f);
                const auto o = run (p, l, &r, { &v, &v }, fs, 480);
                expect (o.l == l && o.r == r, "bypassed from the start: exact");
            }
        }

        beginTest ("isolation: the vocal never reaches the output");
        {
            const int n = (int) fs * 3;
            const auto v = vocal (n, fs, 850.0, 0.5f);
            const Vec zero ((size_t) n, 0.0f);
            BeatLockProcessor p;
            set (p, bl::ids::amount, 100.0f);
            const auto o = run (p, zero, &zero, { &v, &v }, fs, 512);
            bool allZero = true;
            for (size_t i = 0; i < o.l.size(); ++i) allZero = allZero && o.l[i] == 0.0f && o.r[i] == 0.0f;
            expect (allZero, "silent instrumental + loud vocal = exactly silent output");
            // with a real instrumental (a 3 kHz tone only), the vocal's 850 Hz never appears in the output
            const auto inst = constTone (n, fs, 3000.0, 0.3f);
            BeatLockProcessor q;
            set (q, bl::ids::amount, 100.0f);
            const auto o2 = run (q, inst, &inst, { &v, &v }, fs, 512);
            const double leak = bandDb (o2.l, fs, 750, 950), tone = bandDb (o2.l, fs, 2900, 3100);
            logMessage ("output around 850 Hz: " + juce::String (leak - tone, 1) + " dB relative to the instrumental's 3 kHz tone");
            expect (leak < tone - 80.0, "no vocal content in the output (only the instrumental, filtered)");
        }

        beginTest ("conflict: cuts where the vocal sits and the instrumental competes; regions without vocal energy are left alone");
        {
            const int n = (int) fs * 4;
            const auto l = pink (n, 3, 0.4f), r = pink (n, 4, 0.4f), v = vocal (n, fs, 850.0, 0.15f);
            BeatLockProcessor p;
            set (p, bl::ids::amount, 100.0f);
            set (p, bl::ids::maxCut, -6.0f);
            const auto o = run (p, l, &r, { &v, &v }, fs, 512);
            const auto& e = p.getEngine();
            juce::String cuts;
            for (int b = 0; b < bl::numBands; ++b) cuts << juce::String (e.cutDb (b), 2) << " ";
            logMessage ("band cuts at the end (dB): " + cuts);
            juce::String vi;
            for (int b = 0; b < bl::numBands; ++b) vi << "[" << juce::String (e.vocalBandDb (b), 1) << " / " << juce::String (e.instrumentalBandDb (b), 1) << "] ";
            logMessage ("vocal / instrumental band levels (dB): " + vi + " gate " + juce::String (e.gate(), 2) + " voiced " + juce::String (e.voiced(), 2));
            const double d850 = bandDb (o.l, fs, 700, 1000) - bandDb (l, fs, 700, 1000);
            const double d7k = bandDb (o.l, fs, 6000, 8000) - bandDb (l, fs, 6000, 8000);
            const double d150 = bandDb (o.l, fs, 120, 200) - bandDb (l, fs, 120, 200);
            logMessage ("measured change: 700-1000 Hz " + juce::String (d850, 2) + " dB, 6-8 kHz " + juce::String (d7k, 2) + " dB, 120-200 Hz " + juce::String (d150, 2) + " dB");
            expect (d850 < -1.0, "the vocal's region is cut on the instrumental");
            expect (std::abs (d7k) < 0.3 && std::abs (d150) < 0.5, "unrelated regions are left alone");
            // a vocal at 3 kHz moves the cut up there instead
            BeatLockProcessor q;
            set (q, bl::ids::amount, 100.0f);
            const auto vHi = vocal (n, fs, 2400.0, 0.08f);
            const auto o2 = run (q, l, &r, { &vHi, &vHi }, fs, 512);
            const double hi = bandDb (o2.l, fs, 2000, 2900) - bandDb (l, fs, 2000, 2900), lo = bandDb (o2.l, fs, 700, 1000) - bandDb (l, fs, 700, 1000);
            logMessage ("vocal at 2.4 kHz: 2-2.9 kHz " + juce::String (hi, 2) + " dB, 700-1000 Hz " + juce::String (lo, 2) + " dB");
            expect (hi < -0.8 && std::abs (lo) < 0.3, "the cut follows the vocal");
            // no instrumental energy where the vocal is: no cut there (nothing to make room in)
            BeatLockProcessor w;
            set (w, bl::ids::amount, 100.0f);
            const auto lowInst = constTone (n, fs, 120.0, 0.3f);
            run (w, lowInst, &lowInst, { &v, &v }, fs, 512);
            expectGreaterOrEqual (w.getEngine().cutDb (3), -0.05f, "the instrumental has nothing at 850 Hz: band 4 stays at 0 dB");
        }

        beginTest ("maximum cut: the global limit and each band's own limit are respected (the milder wins)");
        {
            const int n = (int) fs * 3;
            const auto l = pink (n, 5), v = vocal (n, fs, 850.0, 0.4f);
            BeatLockProcessor p;
            set (p, bl::ids::amount, 100.0f);
            set (p, bl::ids::sensitivity, 12.0f);
            set (p, bl::ids::maxCut, -3.0f);
            set (p, bl::bandIds()[3].maxCut, -1.5f);
            float worst = 0.0f, worstB4 = 0.0f;
            for (int k = 0; k < 6; ++k)
            {
                const auto chunk = Vec (l.begin() + k * n / 6, l.begin() + (k + 1) * n / 6), vc = Vec (v.begin() + k * n / 6, v.begin() + (k + 1) * n / 6);
                run (p, chunk, &chunk, { &vc, &vc }, fs, 512, k == 0);
                for (int b = 0; b < bl::numBands; ++b) worst = std::min (worst, p.getEngine().cutDb (b));
                worstB4 = std::min (worstB4, p.getEngine().cutDb (3));
            }
            logMessage ("deepest cut seen: " + juce::String (worst, 2) + " dB, band 4: " + juce::String (worstB4, 2) + " dB");
            expect (worst >= -3.001f && worstB4 >= -1.501f);
            expect (worst < -1.0f, "with maximum sensitivity the cut really reaches toward the limit");
        }

        beginTest ("attack and release: a cut follows the vocal in, recovers after it stops, and returns to exact pass-through");
        {
            const int n = (int) fs * 4;
            const auto l = pink (n, 6, 0.4f);
            Vec v ((size_t) n, 0.0f);
            for (int i = (int) fs; i < (int) (fs * 2.0); ++i) v[(size_t) i] = (float) (0.08 * std::sin (twoPi * 850.0 * i / fs)); // on from 1 s to 2 s
            bl::Engine e;
            bl::Settings s;
            s.amount = 1.0f; s.maxCutDb = -6.0f; s.attackMs = 15.0f; s.releaseMs = 180.0f;
            e.setSettings (s);
            e.prepare (fs);
            Vec out (l), cutTrace;
            const float* scp[1] = { v.data() };
            for (int pos = 0; pos < n; pos += 16)
            {
                float* m[1] = { out.data() + pos };
                const float* sp[1] = { scp[0] + pos };
                e.process (m, 1, sp, 1, 16);
                cutTrace.push_back (e.cutDb (3));
            }
            const float steady = cutTrace[(size_t) (1.9 * fs / 16)];
            int t63 = -1;
            for (int k = (int) (fs / 16); k < (int) cutTrace.size(); ++k) if (cutTrace[(size_t) k] <= 0.63f * steady) { t63 = k; break; }
            const double attackMs = (t63 - fs / 16) * 16.0 / fs * 1000.0;
            int r90 = -1;
            for (int k = (int) (2.0 * fs / 16); k < (int) cutTrace.size(); ++k) if (cutTrace[(size_t) k] >= 0.1f * steady) { r90 = k; break; }
            const double releaseMs = (r90 - 2.0 * fs / 16) * 16.0 / fs * 1000.0;
            logMessage ("steady cut " + juce::String (steady, 2) + " dB; 63 % reached " + juce::String (attackMs, 1) + " ms after the vocal started; 90 % recovered "
                        + juce::String (releaseMs, 1) + " ms after it stopped");
            expect (steady < -1.0f);
            expect (attackMs > 5.0 && attackMs < 80.0, "attack: the detector plus the 15 ms attack");
            expect (releaseMs > 150.0 && releaseMs < 1200.0, "release: hold, gate and the 180 ms release");
            expect (cutTrace.back() == 0.0f && ! e.anyBandActive(), "back to exactly 0 dB, no band running");
            bool exactTail = true;
            for (int i = (int) (3.5 * fs); i < n; ++i) exactTail = exactTail && out[(size_t) i] == l[(size_t) i];
            expect (exactTail, "after the release the instrumental is bit-identical again");
        }

        beginTest ("stereo-linked: both channels get the same filter (the image does not move)");
        {
            const int n = (int) fs * 3;
            const auto l = pink (n, 7, 0.4f);
            Vec r (l);
            for (auto& x : r) x *= 0.5f;
            const auto v = vocal (n, fs, 850.0, 0.15f);
            BeatLockProcessor p;
            set (p, bl::ids::amount, 100.0f);
            const auto o = run (p, l, &r, { &v, &v }, fs, 480);
            double worst = 0.0;
            for (size_t i = 0; i < o.l.size(); ++i) worst = std::max (worst, (double) std::abs (o.r[i] - 0.5f * o.l[i]));
            expect (worst < 1.0e-6, "R = L/2 in, R = L/2 out: identical processing (" + juce::String (worst) + ")");
            expect (rmsDb (o.l) < rmsDb (l) - 0.05, "and it really processed");
        }

        beginTest ("vocal activity: constant hiss, silence and low noise do not trigger; a vocal does; unvoiced S sounds drive only the top bands");
        {
            const int n = (int) fs * 3;
            const auto inst = pink (n, 8, 0.4f);
            auto gateFor = [&] (const Vec& sc, float& maxGate, float& maxLowCut, float& maxHighCut) {
                bl::Engine e;
                bl::Settings s;
                s.amount = 1.0f;
                e.setSettings (s);
                e.prepare (fs);
                Vec out (inst);
                maxGate = 0; maxLowCut = 0; maxHighCut = 0;
                for (int pos = 0; pos < n; pos += 64)
                {
                    float* m[1] = { out.data() + pos };
                    const float* sp[1] = { sc.data() + pos };
                    e.process (m, 1, sp, 1, 64);
                    maxGate = std::max (maxGate, e.gate());
                    for (int b = 0; b < 5; ++b) maxLowCut = std::min (maxLowCut, e.cutDb (b));
                    for (int b = 6; b < 8; ++b) maxHighCut = std::min (maxHighCut, e.cutDb (b));
                }
            };
            float g, lo, hi;
            gateFor (Vec ((size_t) n, 0.0f), g, lo, hi);
            expect (g < 0.01f && lo == 0.0f, "silence");
            juce::Random rnd (9);
            Vec hiss ((size_t) n);
            for (auto& x : hiss) x = 0.01f * (rnd.nextFloat() * 2 - 1); // about -45 dBFS, constant from the start
            gateFor (hiss, g, lo, hi);
            expect (g < 0.01f, "constant hiss is the noise floor, not a vocal (" + juce::String (g) + ")");
            Vec low ((size_t) n);
            for (auto& x : low) x = 0.0005f * (rnd.nextFloat() * 2 - 1); // about -71 dBFS
            gateFor (low, g, lo, hi);
            expect (g < 0.01f, "low noise");
            const auto v = vocal (n, fs, 850.0, 0.15f, 0.5);
            Vec withNoise (v);
            for (size_t i = 0; i < withNoise.size(); ++i) withNoise[i] += low[i];
            gateFor (withNoise, g, lo, hi);
            expect (g > 0.9f && lo < -0.5f, "a vocal over the noise opens the gate and cuts");
            // S sounds: bursts of noise above 4 kHz
            Vec s ((size_t) n, 0.0f);
            juce::Random rr (10);
            double z1 = 0, z2 = 0;
            for (int i = 0; i < n; ++i)
            {
                const double t = i / fs - 0.5;
                const double env = t > 0 && std::fmod (t, 0.6) < 0.2 ? 1.0 : 0.0;
                const double w = (rr.nextFloat() * 2 - 1) * env * 0.3;
                const double hpOut = w - z1; z1 = w; // crude high emphasis
                z2 = 0.5 * z2 + 0.5 * hpOut;
                s[(size_t) i] = (float) (hpOut - z2);
            }
            gateFor (s, g, lo, hi);
            logMessage ("S bursts: gate " + juce::String (g, 2) + ", deepest low/mid cut " + juce::String (lo, 2) + " dB, deepest top cut " + juce::String (hi, 2) + " dB");
            expect (lo > -0.3f, "S sounds do not drive the low and mid bands");
        }

        beginTest ("robustness: 44.1-192 kHz, block sizes 1-4096 give the same result, non-finite input, automation sweeps, disconnect and reconnect");
        {
            for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
            {
                const int n = (int) rate * 2;
                const auto l = pink (n, 11, 0.4f), v = vocal (n, rate, 850.0, 0.15f);
                BeatLockProcessor p;
                set (p, bl::ids::amount, 100.0f);
                const auto o = run (p, l, &l, { &v }, rate, 512);
                bool finite = true;
                for (const float x : o.l) finite = finite && std::isfinite (x);
                const double d = bandDb (o.l, rate, 700, 1000) - bandDb (l, rate, 700, 1000);
                expect (finite && d < -0.5, juce::String (rate) + " Hz: finite and cutting (" + juce::String (d, 2) + " dB)");
            }
            const int n = (int) fs;
            const auto l = pink (n, 12), v = vocal (n, fs, 850.0, 0.3f);
            Vec ref;
            for (int block : { 4096, 1, 7, 64, 480, 1024 })
            {
                BeatLockProcessor p;
                set (p, bl::ids::amount, 100.0f);
                const auto o = run (p, l, &l, { &v, &v }, fs, block);
                if (ref.empty()) ref = o.l;
                else expect (o.l == ref, "block " + juce::String (block) + " = block 4096, bit for bit");
            }
            // non-finite samples on either input
            {
                BeatLockProcessor p;
                set (p, bl::ids::amount, 100.0f);
                Vec bad (l), badV (v);
                for (int i = 1000; i < (int) bad.size(); i += 997) { bad[(size_t) i] = std::numeric_limits<float>::quiet_NaN(); badV[(size_t) i + 3] = std::numeric_limits<float>::infinity(); }
                const auto o = run (p, bad, &bad, { &badV, &badV }, fs, 256);
                bool finite = true;
                for (const float x : o.l) finite = finite && std::isfinite (x);
                expect (finite, "NaN / infinity never reach the output");
            }
            // automation: every control moved every block
            {
                BeatLockProcessor p;
                const auto lt = pink ((int) fs * 3, 13), vt = vocal ((int) fs * 3, fs, 1000.0, 0.3f);
                juce::Random rnd (14);
                float peakOut = 0.0f, peakIn = 0.0f, worstStep = 0.0f, worstStepIn = 0.0f;
                float prevOut = 0.0f, prevIn = 0.0f;
                bool finite = true;
                for (int k = 0; k < (int) (lt.size() / 512); ++k)
                {
                    set (p, bl::ids::amount, rnd.nextFloat() * 100.0f);
                    set (p, bl::ids::maxCut, -12.0f * rnd.nextFloat());
                    set (p, bl::ids::sensitivity, rnd.nextFloat() * 24.0f - 12.0f);
                    set (p, bl::ids::attack, 1.0f + 99.0f * rnd.nextFloat());
                    set (p, bl::ids::release, 30.0f + 970.0f * rnd.nextFloat());
                    for (int b = 0; b < bl::numBands; ++b)
                    {
                        set (p, bl::bandIds()[(size_t) b].freq, 100.0f * std::pow (120.0f, rnd.nextFloat()));
                        set (p, bl::bandIds()[(size_t) b].q, 0.5f + 3.5f * rnd.nextFloat());
                        set (p, bl::bandIds()[(size_t) b].on, rnd.nextFloat() > 0.2f ? 1.0f : 0.0f);
                    }
                    const Vec sc (vt.begin() + k * 512, vt.begin() + (k + 1) * 512);
                    const Vec in (lt.begin() + k * 512, lt.begin() + (k + 1) * 512);
                    const auto o = run (p, in, nullptr, { &sc }, fs, 512, k == 0);
                    for (int i = 0; i < 512; ++i)
                    {
                        finite = finite && std::isfinite (o.l[(size_t) i]);
                        peakOut = std::max (peakOut, std::abs (o.l[(size_t) i]));
                        peakIn = std::max (peakIn, std::abs (in[(size_t) i]));
                        worstStep = std::max (worstStep, std::abs (o.l[(size_t) i] - prevOut));
                        worstStepIn = std::max (worstStepIn, std::abs (in[(size_t) i] - prevIn));
                        prevOut = o.l[(size_t) i]; prevIn = in[(size_t) i];
                    }
                }
                logMessage ("automation sweep: peak in " + juce::String (peakIn, 3) + ", out " + juce::String (peakOut, 3) + "; largest sample step in "
                            + juce::String (worstStepIn, 3) + ", out " + juce::String (worstStep, 3));
                expect (finite && peakOut <= peakIn * 1.12f, "no instability, no gain blow-up");
                expect (worstStep <= worstStepIn * 1.5f + 0.01f, "no clicks from the moving coefficients");
            }
            // disconnect and reconnect the sidechain mid-stream
            {
                BeatLockProcessor p;
                set (p, bl::ids::amount, 100.0f);
                const auto lt = pink ((int) fs * 2, 15, 0.4f), vt = vocal ((int) fs * 2, fs, 850.0, 0.15f);
                run (p, lt, &lt, { &vt, &vt }, fs, 512);
                const float during = p.getEngine().cutDb (3);
                const auto o = run (p, lt, &lt, {}, fs, 512, false); // the host stops sending: the processor sees no sidechain channels
                bool exactEnd = true;
                for (int i = (int) fs; i < (int) lt.size(); ++i) exactEnd = exactEnd && o.l[(size_t) i] == lt[(size_t) i];
                expect (during < -0.5f && exactEnd, "after a disconnect the cut releases and the instrumental is exact again");
                run (p, lt, &lt, { &vt, &vt }, fs, 512, false);
                expect (p.getEngine().cutDb (3) < -0.5f, "reconnected: cutting again");
            }
        }

        beginTest ("state: a round trip restores every value; empty, foreign and older states are safe");
        {
            BeatLockProcessor a;
            set (a, bl::ids::amount, 73.0f);
            set (a, bl::ids::maxCut, -7.5f);
            set (a, bl::ids::focusLow, 20.0f);
            set (a, bl::bandIds()[2].freq, 640.0f);
            set (a, bl::bandIds()[5].on, 0.0f);
            set (a, bl::ids::bypass, 1.0f);
            juce::MemoryBlock mb;
            a.getStateInformation (mb);
            BeatLockProcessor b;
            b.setStateInformation (mb.getData(), (int) mb.getSize());
            for (auto* prm : a.getParameters())
            {
                auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm);
                expectWithinAbsoluteError (b.apvts.getParameter (r->paramID)->getValue(), r->getValue(), 1.0e-6f, r->paramID);
            }
            BeatLockProcessor c;
            set (c, bl::ids::amount, 10.0f);
            c.setStateInformation (nullptr, 0);
            const char junk[] = "not a state at all";
            c.setStateInformation (junk, (int) sizeof (junk));
            expectWithinAbsoluteError (c.apvts.getRawParameterValue (bl::ids::amount)->load(), 10.0f, 0.01f, "unreadable data changes nothing");
            // an older state with only one parameter: the rest take their defaults
            juce::ValueTree t ("GlitchKidBeatLock");
            juce::ValueTree prm ("PARAM");
            prm.setProperty ("id", bl::ids::amount, nullptr).setProperty ("value", 30.0f, nullptr);
            t.appendChild (prm, nullptr);
            juce::MemoryBlock old;
            juce::AudioProcessor::copyXmlToBinary (*t.createXml(), old);
            c.setStateInformation (old.getData(), (int) old.getSize());
            expectWithinAbsoluteError (c.apvts.getRawParameterValue (bl::ids::amount)->load(), 30.0f, 0.01f);
            expectWithinAbsoluteError (c.apvts.getRawParameterValue (bl::ids::maxCut)->load(), bl::defaults::maxCut, 0.01f, "missing = default");
        }

        beginTest ("LEARN / ANALYZE: the FIFO never blocks and counts overflow; no data gives an honest answer; real data finds the vocal's conflict region");
        {
            bl::AnalysisFifo f;
            Vec a (1000, 0.1f), g (1000, 1.0f);
            f.write (a.data(), a.data(), g.data(), 1000);
            expectEquals (f.dropped(), 0, "disarmed: nothing is written, nothing dropped");
            f.setArmed (true);
            const long before = threadAllocations();
            for (int k = 0; k < 200; ++k) f.write (a.data(), a.data(), g.data(), 1000); // 200 000 samples into 131 072
            const long writerAllocations = threadAllocations() - before; // read before the message string is built (it allocates)
            expectEquals (writerAllocations, 0L, "the writer never allocates");
            expect (f.dropped() > 0, "overflow is counted");
            Vec rv (200000), ri (200000), rg (200000);
            expectEquals (f.read (rv.data(), ri.data(), rg.data(), 200000), bl::AnalysisFifo::capacity - 1);

            bl::ConflictAnalysis an;
            an.start (bl::ConflictAnalysis::Mode::analyze, fs, 4.0f, 6.0f);
            Vec quiet ((size_t) fs * 7, 0.0f), gateOff ((size_t) fs * 7, 0.0f);
            an.feed (quiet.data(), quiet.data(), gateOff.data(), (int) quiet.size());
            expect (an.finished(), "the timeout ends a capture");
            const auto none = an.result();
            expect (none.status == bl::AnalysisResult::Status::insufficientVocal && none.message.contains ("Not enough vocal"), none.message);
            expect (! bl::ConflictAnalysis::propose (none, bl::defaultFreqs).valid, "no proposal without data");

            const int n = (int) fs * 6;
            const auto v = vocal (n, fs, 1250.0, 0.15f), inst = pink (n, 16, 0.5f);
            Vec gate ((size_t) n, 1.0f);
            an.start (bl::ConflictAnalysis::Mode::learn, fs, 4.0f, 30.0f);
            an.feed (v.data(), inst.data(), gate.data(), n);
            const auto r = an.result();
            expect (r.status == bl::AnalysisResult::Status::ready, r.message);
            expect (! r.strongest.empty(), "conflicts were found");
            if (! r.strongest.empty())
            {
                const float hz = r.regions[(size_t) r.strongest[0]].centreHz;
                logMessage ("strongest conflict region: " + juce::String (hz, 0) + " Hz (" + juce::String (r.regions[(size_t) r.strongest[0]].conflictPct, 0) + " %), median gap "
                            + juce::String (r.medianInstMinusVocalDb, 1) + " dB");
                expect (hz > 900.0f && hz < 1700.0f, "around the vocal's 1.25 kHz");
            }
            const auto prop = bl::ConflictAnalysis::propose (r, bl::defaultFreqs);
            expect (prop.valid);
            bool sorted = true, inRange = true, near = false;
            for (int b = 0; b < bl::numBands; ++b)
            {
                inRange = inRange && prop.freqHz[(size_t) b] >= 100.0f && prop.freqHz[(size_t) b] <= 12000.0f;
                if (b > 0) sorted = sorted && prop.freqHz[(size_t) b] > prop.freqHz[(size_t) b - 1];
                near = near || std::abs (std::log2 (prop.freqHz[(size_t) b] / 1250.0f)) < 0.34f;
            }
            juce::String pf;
            for (const float f : prop.freqHz) pf << juce::String (f, 0) << " ";
            logMessage ("proposed bands (Hz): " + pf + "; sensitivity " + juce::String (prop.sensitivityDb, 1) + " dB");
            expect (sorted && inRange && near, "eight ordered bands in range, one at the vocal's region");
            expect (prop.sensitivityDb >= -12.0f && prop.sensitivityDb <= 12.0f);
        }

        beginTest ("the editor: honest status, LEARN only changes settings on APPLY, no sidechain = no capture; snapshots");
        {
            BeatLockProcessor p;
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            auto* e = dynamic_cast<BeatLockEditor*> (ed.get());
            expect (e != nullptr && e->getWidth() == 1120 && e->getHeight() == 680);
            const auto dir = juce::SystemStats::getEnvironmentVariable ("GLITCHKID_SNAPSHOT_DIR", {});
            auto shot = [&] (const juce::String& shotName) {
                e->tickForTest();
                if (dir.isEmpty()) return;
                auto img = e->createComponentSnapshot (e->getLocalBounds(), true, 1.0f);
                juce::File f = juce::File (dir).getChildFile (shotName + ".png");
                f.deleteFile();
                juce::FileOutputStream os (f);
                if (os.openedOk()) juce::PNGImageFormat().writeImageToStream (img, os);
            };
            // no sidechain
            const int n = (int) fs * 2;
            const auto l = pink (n, 17);
            run (p, l, &l, {}, fs, 480);
            e->tickForTest();
            expect (e->statusTextForTest().startsWith ("NO VOCAL SIDECHAIN"), e->statusTextForTest());
            e->startAnalysisForTest (true);
            expect (! e->analysisRunningForTest() && e->panelShownForTest(), "no sidechain: nothing is captured, the panel says why");
            shot ("beatlock_no_sidechain_learn");
            e->closePanelForTest();
            shot ("beatlock_no_sidechain");
            // live: a vocal competing with the instrumental
            set (p, bl::ids::amount, 100.0f);
            const auto lt = pink ((int) fs * 12, 18, 0.5f), vt = vocal ((int) fs * 12, fs, 1250.0, 0.15f);
            // play 2 s in 100 ms pieces with the interface refreshing in between (as in a host), ending mid-phrase
            for (int pos = 0; pos + 4800 <= (int) (fs * 2.0); pos += 4800)
            {
                const Vec li (lt.begin() + pos, lt.begin() + pos + 4800), vi (vt.begin() + pos, vt.begin() + pos + 4800);
                run (p, li, nullptr, { &vi }, fs, 480, pos == 0);
                for (int k = 0; k < 3; ++k) e->tickForTest();
            }
            expect (e->statusTextForTest() == "PROCESSING THE INSTRUMENTAL", e->statusTextForTest());
            shot ("beatlock_live");
            e->setAdvancedForTest (true);
            shot ("beatlock_advanced");
            e->setAdvancedForTest (false);
            // LEARN: capture through the processor's FIFO, drained by the editor's timer
            const auto before = values (p);
            e->startAnalysisForTest (true);
            expect (e->analysisRunningForTest());
            for (int pos = (int) (fs * 2.0); pos + 4800 <= (int) lt.size() && e->analysisRunningForTest(); pos += 4800)
            {
                const Vec li (lt.begin() + pos, lt.begin() + pos + 4800);
                const Vec vi (vt.begin() + pos, vt.begin() + pos + 4800);
                run (p, li, nullptr, { &vi }, fs, 480, false);
                e->tickForTest();
            }
            expect (! e->analysisRunningForTest(), "LEARN finished");
            expect (e->proposalForTest().valid, "a proposal was made");
            expect (values (p) == before, "nothing changed before APPLY");
            shot ("beatlock_learn");
            const auto proposal = e->proposalForTest();
            e->applyLearnForTest();
            for (int b = 0; b < bl::numBands; ++b)
                expectWithinAbsoluteError (p.apvts.getRawParameterValue (bl::bandIds()[(size_t) b].freq)->load(), proposal.freqHz[(size_t) b], 0.6f);
            expectWithinAbsoluteError (p.apvts.getRawParameterValue (bl::ids::sensitivity)->load(), proposal.sensitivityDb, 0.06f);
            expectWithinAbsoluteError (p.apvts.getRawParameterValue (bl::ids::amount)->load(), 100.0f, 0.01f, "APPLY changes only the bands and sensitivity");
            // ANALYZE: a report only; it changes nothing
            const auto beforeAnalyze = values (p);
            e->startAnalysisForTest (false);
            for (int pos = 0; pos + 4800 <= (int) lt.size() && e->analysisRunningForTest(); pos += 4800)
            {
                const Vec li (lt.begin() + pos, lt.begin() + pos + 4800), vi (vt.begin() + pos, vt.begin() + pos + 4800);
                run (p, li, nullptr, { &vi }, fs, 480, false);
                e->tickForTest();
            }
            expect (! e->analysisRunningForTest() && e->panelShownForTest(), "ANALYZE finished and shows its report");
            expect (values (p) == beforeAnalyze, "ANALYZE changed nothing");
            shot ("beatlock_analyze");
            e->closePanelForTest();
            e->setRoutingForTest (true);
            shot ("beatlock_routing");
        }

        beginTest ("performance: CPU (information only) and no allocation on the audio thread");
        {
            const int n = (int) fs * 10;
            const auto l = pink (n, 19, 0.4f), r = pink (n, 20, 0.4f), v = vocal (n, fs, 850.0, 0.15f);
            BeatLockProcessor p;
            set (p, bl::ids::amount, 100.0f);
            set (p, bl::ids::sensitivity, 12.0f);
            run (p, Vec (l.begin(), l.begin() + 48000), &r, { &v, &v }, fs, 512); // warm up (and prepare)
            juce::AudioBuffer<float> b (4, 512);
            juce::MidiBuffer midi;
            const long before = threadAllocations();
            const auto t0 = juce::Time::getHighResolutionTicks();
            for (int pos = 0; pos + 512 <= n; pos += 512)
            {
                b.copyFrom (0, 0, l.data() + pos, 512); b.copyFrom (1, 0, r.data() + pos, 512);
                b.copyFrom (2, 0, v.data() + pos, 512); b.copyFrom (3, 0, v.data() + pos, 512);
                p.processBlock (b, midi);
            }
            const long blockAllocations = threadAllocations() - before; // read before the message string is built (it allocates)
            const double sec = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
            expectEquals (blockAllocations, 0L, "processBlock allocates nothing");
            logMessage ("BEAT LOCK CPU: " + juce::String (sec / 10.0 * 100.0, 2) + " % of one core (stereo instrumental + stereo sidechain, 48 kHz, 512-sample blocks, every band cutting)");
        }
    }
};
static BeatLockPluginTests beatLockPluginTests;
}
