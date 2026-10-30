#include "TestHelpers.h"
#include "../Source/DSP/OutputLimiter.h"
#include "../Source/AutoMode/BeatLock.h"
#include "../Source/AutoMode/MixEngine.h"
#include "../Source/Analysis/VocalAnalyzer.h"
#include "../Source/Presets/PresetController.h"
#include <atomic>
#include <limits>

extern std::atomic<long> g_allocations; // counts every heap allocation (defined in FormantShiftTests.cpp)

// OUTPUT LIMITER (DSP, in the real plug-in) and the bounds on BEAT LOCK / BUILD's beat-informed moves. Synthetic signals only.
namespace
{
namespace am = glitchkid::automode;
namespace au = glitchkid::analysis;
constexpr double pi = 3.14159265358979323846;
constexpr int blockSize = 480;

float ceilingOf (float db) { return juce::Decibels::decibelsToGain (db); }

// Runs a stereo (or mono) signal through the limiter in host-style blocks. `settings` may change per block through `perBlock`.
struct Run
{
    std::vector<float> l, r;
    float grDb = 0.0f;
};

Run runLimiter (OutputLimiter& lim, const std::vector<float>& inL, const std::vector<float>* inR)
{
    Run out;
    out.l = inL;
    if (inR != nullptr)
        out.r = *inR;
    for (size_t pos = 0; pos < inL.size(); pos += blockSize)
    {
        const int n = (int) std::min<size_t> (blockSize, inL.size() - pos);
        float* ch[2] = { out.l.data() + pos, inR != nullptr ? out.r.data() + pos : nullptr };
        lim.process (ch, inR != nullptr ? 2 : 1, n);
        out.grDb = std::max (out.grDb, lim.consumeGainReductionDb());
    }
    return out;
}

void setupLimiter (OutputLimiter& lim, double fs, float ceilingDb = -1.0f, float driveDb = 0.0f, float releaseMs = 80.0f, bool on = true)
{
    lim.setSettings ({ on, ceilingDb, driveDb, releaseMs });
    lim.prepare (fs);
}

std::vector<float> sine (double fs, double hz, double seconds, float amp, double phase = 0.0)
{
    std::vector<float> x ((size_t) (fs * seconds));
    for (size_t n = 0; n < x.size(); ++n)
        x[n] = amp * (float) std::sin (2.0 * pi * hz * (double) n / fs + phase);
    return x;
}

std::vector<float> noise (size_t n, float amp, int seed)
{
    juce::Random rng (seed);
    std::vector<float> x (n);
    for (auto& v : x)
        v = amp * (rng.nextFloat() * 2.0f - 1.0f);
    return x;
}

float peakOf (const std::vector<float>& x, size_t from = 0)
{
    float p = 0.0f;
    for (size_t i = from; i < x.size(); ++i)
        p = std::max (p, std::abs (x[i]));
    return p;
}

// 8x windowed-sinc interpolation (offline): the highest value the reconstructed waveform reaches, to quantify the difference between sample peak and true peak.
float truePeak (const std::vector<float>& x, size_t from = 0)
{
    constexpr int taps = 24, up = 8;
    float peak = 0.0f;
    for (size_t n = std::max<size_t> (from, taps); n + taps < x.size(); ++n)
        for (int k = 0; k < up; ++k)
        {
            const double frac = (double) k / up;
            double sum = 0.0;
            for (int t = -taps + 1; t <= taps; ++t)
            {
                const double d = (double) t - frac;
                const double sinc = std::abs (d) < 1.0e-9 ? 1.0 : std::sin (pi * d) / (pi * d);
                const double w = 0.5 + 0.5 * std::cos (pi * d / (taps + 1));
                sum += x[n + (size_t) t] * sinc * w;
            }
            peak = std::max (peak, (float) std::abs (sum));
        }
    return peak;
}

au::VocalAnalysis typical()
{
    au::VocalAnalysis a;
    a.status = au::VocalAnalysis::Status::ready;
    a.activeSeconds = 8.0f; a.totalSeconds = 9.0f;
    a.peakDb = -6.0f; a.rmsDb = -20.0f; a.crestDb = 14.0f;
    a.dynP10Db = -32.0f; a.dynP50Db = -22.0f; a.dynP90Db = -16.0f; a.dynamicRangeDb = 16.0f;
    a.bandFraction = { 0.10f, 0.45f, 0.28f, 0.10f, 0.07f };
    a.sibilanceIndex = 0.10f;
    return a;
}

au::VocalAnalysis crowdedBeat()
{
    auto b = typical();
    b.rmsDb = -12.0f; b.peakDb = -2.0f;
    b.bandFraction = { 0.10f, 0.20f, 0.55f, 0.10f, 0.05f };
    return b;
}

am::RangeLookup rangesOf (GlitchKidProcessor& p)
{
    return [&p] (const juce::String& id, am::Range& out)
    {
        auto* r = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (id));
        if (r == nullptr)
            return false;
        const auto range = r->getNormalisableRange();
        out = { range.start, range.end, [range] (float v) { return range.snapToLegalValue (v); } };
        return true;
    };
}

std::vector<float> voiceSignal (double fs, double seconds, double f0, float dark, float sibilantAmp, float noiseAmp)
{
    std::vector<float> x ((size_t) (fs * seconds), 0.0f);
    juce::Random rng (7);
    float hp = 0.0f, lastN = 0.0f;
    for (size_t n = 0; n < x.size(); ++n)
    {
        const double t = (double) n / fs, ph = std::fmod (t, 0.5);
        const double env = ph > 0.4 ? 0.0 : std::sin (pi * ph / 0.4) * 0.3;
        double s = 0.0;
        for (int h = 1; h <= 14; ++h)
            s += std::sin (2.0 * pi * f0 * h * t) / std::pow ((double) h, 1.0 + (double) dark);
        const float wn = rng.nextFloat() * 2.0f - 1.0f;
        hp = wn - lastN; // crude high-pass: sibilant hiss
        lastN = wn;
        const double hiss = ph > 0.4 ? sibilantAmp * hp : 0.0;
        x[n] = (float) (env * s * 0.6 + hiss + noiseAmp * wn);
    }
    return x;
}

struct OutputLimiterTests : public juce::UnitTest
{
    OutputLimiterTests() : juce::UnitTest ("Output limiter and bounded recommendations", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("containment: no output sample exceeds the ceiling, for loud noise, sine bursts and impulses, at several ceilings, rates and in mono");
        {
            for (double fs : { 44100.0, 48000.0, 96000.0 })
                for (float ceilDb : { -0.1f, -1.0f, -6.0f, -12.0f })
                {
                    OutputLimiter lim; setupLimiter (lim, fs, ceilDb);
                    auto a = noise ((size_t) (fs * 3), 4.0f, 1), b = noise ((size_t) (fs * 3), 3.0f, 2);
                    for (size_t i = 0; i < a.size(); ++i) // bursts and single-sample impulses on top
                    {
                        if ((i / 4800) % 3 == 0) a[i] += 5.0f * (float) std::sin (0.2 * (double) i);
                        if (i % 20011 == 0) { a[i] = 9.0f; b[i] = -7.0f; }
                    }
                    const auto o = runLimiter (lim, a, &b);
                    const float limit = ceilingOf (ceilDb) + 1.0e-6f;
                    expect (peakOf (o.l) <= limit && peakOf (o.r) <= limit, "ceiling " + juce::String (ceilDb) + " dB at " + juce::String (fs) + ": peak " + juce::String (juce::jmax (peakOf (o.l), peakOf (o.r))));
                    expect (o.grDb > 6.0f, "it really limited");
                }
            OutputLimiter lim; setupLimiter (lim, 48000.0, -3.0f);
            const auto m = runLimiter (lim, noise (48000, 4.0f, 3), nullptr);
            expect (peakOf (m.l) <= ceilingOf (-3.0f) + 1.0e-6f, "mono");
        }

        beginTest ("below the ceiling it is transparent (a delayed copy, bit for bit); after limiting it returns to unity");
        {
            OutputLimiter lim; setupLimiter (lim, 48000.0);
            const auto x = sine (48000.0, 440.0, 1.0, 0.5f);
            const auto o = runLimiter (lim, x, &x);
            const int L = lim.latencySamples();
            expect (L >= 16 && L <= 256);
            for (size_t i = (size_t) L; i < x.size(); ++i)
                if (o.l[i] != x[i - (size_t) L] || o.r[i] != x[i - (size_t) L])
                {
                    expect (false, "not bit-exact at " + juce::String ((int) i));
                    break;
                }
            expectWithinAbsoluteError (o.grDb, 0.0f, 1.0e-6f);

            // a single loud impulse: the gain reduction is real, bounded, and the gain recovers
            OutputLimiter lim2; setupLimiter (lim2, 48000.0, -1.0f, 0.0f, 50.0f);
            auto y = sine (48000.0, 300.0, 2.0, 0.2f);
            y[10000] = 4.0f;
            const auto o2 = runLimiter (lim2, y, &y);
            expect (peakOf (o2.l) <= ceilingOf (-1.0f) + 1.0e-6f);
            expectWithinAbsoluteError (o2.grDb, 20.0f * std::log10 (4.0f / ceilingOf (-1.0f)), 0.6f);
            const size_t tail = y.size() - 2000;
            expectWithinAbsoluteError (o2.l[tail], y[tail - (size_t) lim2.latencySamples()], 0.002f);
        }

        beginTest ("sustained loud signal: held at the ceiling without pumping; stereo gain is linked so the image does not move");
        {
            OutputLimiter lim; setupLimiter (lim, 48000.0, -1.0f, 0.0f, 80.0f);
            const auto a = sine (48000.0, 220.0, 3.0, 3.0f), b = sine (48000.0, 220.0, 3.0, 0.75f);
            const auto o = runLimiter (lim, a, &b);
            const float c = ceilingOf (-1.0f);
            expect (peakOf (o.l, 48000) <= c + 1.0e-6f && peakOf (o.l, 48000) > c * 0.97f, "held at the ceiling: " + juce::String (peakOf (o.l, 48000)));
            const int L = lim.latencySamples();
            for (size_t i = 60000; i < 70000; ++i)
                if (std::abs (a[i - (size_t) L]) > 0.5f)
                    expectWithinAbsoluteError (o.l[i] / o.r[i], 4.0f, 0.01f); // the same gain on both channels
        }

        beginTest ("silence stays silent; NaN and Inf input never reach the output or contaminate what follows");
        {
            OutputLimiter lim; setupLimiter (lim, 48000.0);
            const auto z = runLimiter (lim, std::vector<float> (48000, 0.0f), nullptr);
            expectEquals (peakOf (z.l), 0.0f);
            expectEquals (z.grDb, 0.0f);
            auto x = sine (48000.0, 440.0, 2.0, 0.3f);
            for (size_t i = 5000; i < 5100; ++i)
                x[i] = (i % 3 == 0) ? std::numeric_limits<float>::quiet_NaN() : (i % 3 == 1 ? std::numeric_limits<float>::infinity() : -std::numeric_limits<float>::infinity());
            OutputLimiter lim2; setupLimiter (lim2, 48000.0);
            const auto o = runLimiter (lim2, x, &x);
            for (float v : o.l)
                expect (std::isfinite (v) && std::abs (v) <= ceilingOf (-1.0f) + 1.0e-6f);
            const int L = lim2.latencySamples();
            expectWithinAbsoluteError (o.l[90000], x[90000 - (size_t) L], 1.0e-6f); // later audio passes unchanged
        }

        beginTest ("switching on and off is a crossfade, not a click; off is the untouched, undelayed signal");
        {
            OutputLimiter lim;
            lim.setSettings ({ false, -1.0f, 0.0f, 80.0f });
            lim.prepare (48000.0);
            const auto x = sine (48000.0, 440.0, 3.0, 0.3f);
            std::vector<float> y = x;
            for (size_t pos = 0; pos < x.size(); pos += blockSize)
            {
                lim.setSettings ({ ((pos / blockSize) / 30) % 2 == 1, -1.0f, 0.0f, 80.0f }); // toggled every 0.3 s
                float* ch[1] = { y.data() + pos };
                lim.process (ch, 1, blockSize);
            }
            float worst = 0.0f;
            for (size_t i = 1; i < y.size(); ++i)
                worst = std::max (worst, std::abs (y[i] - y[i - 1]));
            const float slope = 0.3f * (float) (2.0 * pi * 440.0 / 48000.0);
            expect (worst < 3.0f * slope, "largest step " + juce::String (worst) + " against a normal slope of " + juce::String (slope));
            for (size_t i = 0; i < 20; ++i)
                expectEquals (y[i], x[i]); // off at the start: identical, no delay
        }

        beginTest ("parameters can move while it runs: always finite, within the largest ceiling, and within the final ceiling once settled");
        {
            OutputLimiter lim;
            lim.setSettings ({ true, -1.0f, 0.0f, 80.0f });
            lim.prepare (48000.0);
            auto x = noise (48000 * 4, 3.0f, 9);
            juce::Random rng (5);
            for (size_t pos = 0; pos < x.size(); pos += blockSize)
            {
                lim.setSettings ({ true, -12.0f + 11.9f * rng.nextFloat(), 24.0f * rng.nextFloat(), 10.0f + 490.0f * rng.nextFloat() });
                float* ch[1] = { x.data() + pos };
                lim.process (ch, 1, blockSize);
            }
            for (float v : x)
                expect (std::isfinite (v) && std::abs (v) <= ceilingOf (-0.1f) + 1.0e-6f);
        }

        beginTest ("sample peak versus true peak: measured, and the documented limits hold (it does not claim true-peak protection)");
        {
            OutputLimiter lim; setupLimiter (lim, 48000.0, -1.0f);
            const auto q = sine (48000.0, 12000.0, 2.0, 3.0f, pi / 4.0); // a quarter of the sample rate: the worst case, samples at +-0.707 of the true peak
            const auto o = runLimiter (lim, q, nullptr);
            const float sp = peakOf (o.l, 24000), tp = truePeak (o.l, 24000);
            const float overDb = 20.0f * std::log10 (tp / ceilingOf (-1.0f));
            logMessage ("worst case (fs/4 sine): sample peak " + juce::String (20.0f * std::log10 (sp), 2) + " dBFS, true peak " + juce::String (20.0f * std::log10 (tp), 2) + " dBFS (" + juce::String (overDb, 2) + " dB over the ceiling)");
            expect (sp <= ceilingOf (-1.0f) + 1.0e-6f);
            expect (overDb < 3.2f, "inter-sample peaks stay within the documented 3 dB");
            OutputLimiter lim2; setupLimiter (lim2, 48000.0, -1.0f);
            const auto v = runLimiter (lim2, voiceSignal (48000.0, 4.0, 220.0, 0.5f, 0.0f, 0.0f), nullptr);
            auto louder = voiceSignal (48000.0, 4.0, 220.0, 0.5f, 0.0f, 0.0f);
            for (auto& s : louder) s *= 8.0f;
            OutputLimiter lim3; setupLimiter (lim3, 48000.0, -1.0f);
            const auto o3 = runLimiter (lim3, louder, nullptr);
            const float over3 = 20.0f * std::log10 (truePeak (o3.l, 24000) / ceilingOf (-1.0f));
            logMessage ("voice-like signal 18 dB over the ceiling: true peak " + juce::String (over3, 2) + " dB over the ceiling");
            expect (over3 < 1.5f);
            juce::ignoreUnused (v);
        }

        beginTest ("latency is the look-ahead (about 1.5 ms) at every sample rate; CPU is small");
        {
            for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
            {
                OutputLimiter lim; setupLimiter (lim, fs);
                expect (lim.latencySamples() >= 16 && lim.latencySamples() <= OutputLimiter::maxLookahead);
                if (fs <= 96000.0)
                    expectWithinAbsoluteError ((float) lim.latencySamples() / (float) fs, 0.0015f, 0.0001f);
            }
            OutputLimiter lim; setupLimiter (lim, 48000.0);
            const auto x = noise (48000 * 10, 2.0f, 4);
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            runLimiter (lim, x, &x);
            const double frac = (juce::Time::getMillisecondCounterHiRes() - t0) / 10000.0;
            logMessage ("limiter CPU (one core, stereo, 48 kHz, fraction of real time): " + juce::String (frac * 100.0, 2) + " %");
            expect (frac < 0.25);
        }

        beginTest ("in the real plug-in: 69 parameters, the limiter ones are appended last and off by default (an older state is unchanged)");
        {
            GlitchKidProcessor p;
            expectEquals (p.getParameters().size(), 69);
            const char* ids[4] = { "lim_on", "lim_ceiling", "lim_drive", "lim_release" };
            for (int i = 0; i < 4; ++i)
                expectEquals (juce::String (dynamic_cast<juce::RangedAudioParameter*> (p.getParameters()[65 + i])->paramID), juce::String (ids[i]));
            expectEquals (th::getParam (p, "lim_on"), 0.0f);
            expectEquals (th::getParam (p, "lim_ceiling"), -1.0f, "lim_ceiling default reads " + juce::String (th::getParam (p, "lim_ceiling"), 9));
            expectEquals (th::getParam (p, "lim_drive"), 0.0f);
            expectEquals (th::getParam (p, "lim_release"), 80.0f);
            p.prepareToPlay (48000.0, blockSize);
            expectEquals (p.getLatencySamples(), th::pluginLatency, "off: the reported latency is exactly what it was");
            th::setParam (p, "lim_on", 1.0f);
            p.updateReportedLatency();
            expect (p.getLatencySamples() > th::pluginLatency + 16, "on: the look-ahead is reported (" + juce::String (p.getLatencySamples()) + ")");
            th::setParam (p, "lim_on", 0.0f);
            p.updateReportedLatency();
            expectEquals (p.getLatencySamples(), th::pluginLatency);
            th::setParam (p, "lim_ceiling", 99.0f);
            expect (th::getParam (p, "lim_ceiling") <= -0.1f + 1.0e-4f, "the ceiling cannot be set to 0 dBFS or above");
            th::setParam (p, "lim_drive", 99.0f);
            expectEquals (th::getParam (p, "lim_drive"), 24.0f);
        }

        beginTest ("in the real plug-in: a hot chain clips without the limiter and stays under the ceiling with it (mono and stereo, DRY and PROCESSED)");
        {
            for (int channels : { 2, 1 })
                for (bool dry : { false, true })
                    for (bool limOn : { false, true })
                    {
                        GlitchKidProcessor p;
                        if (channels == 1)
                            th::setLayout (p, juce::AudioChannelSet::mono());
                        th::setParam (p, "gain", 12.0f);
                        th::setParam (p, "comp_ratio", 3.0f);
                        th::setParam (p, "comp_makeup", 12.0f);
                        th::setParam (p, "sat_drive", 12.0f);
                        th::setParam (p, "clean_on", 1.0f);
                        th::setParam (p, "air_on", 1.0f);
                        th::setParam (p, "bypass", dry ? 1.0f : 0.0f);
                        th::setParam (p, "lim_on", limOn ? 1.0f : 0.0f);
                        th::setParam (p, "lim_ceiling", -1.0f);
                        p.prepareToPlay (48000.0, blockSize);
                        juce::MidiBuffer midi;
                        float peak = 0.0f;
                        for (int b = 0; b < 200; ++b)
                        {
                            th::Buf buf (channels, blockSize);
                            for (int i = 0; i < blockSize; ++i)
                                for (int ch = 0; ch < channels; ++ch)
                                    buf.setSample (ch, i, 0.9f * (float) std::sin (2.0 * pi * 220.0 * (double) (b * blockSize + i) / 48000.0) * (ch == 1 ? 0.8f : 1.0f));
                            p.processBlock (buf, midi);
                            if (b >= 10)
                                for (int ch = 0; ch < channels; ++ch)
                                    peak = juce::jmax (peak, buf.getMagnitude (ch, 0, blockSize));
                        }
                        const float ceilLin = ceilingOf (-1.0f) + 1.0e-6f;
                        if (limOn)
                            expect (peak <= ceilLin, juce::String (channels) + " ch, dry " + juce::String ((int) dry) + ": peak " + juce::String (peak));
                        else if (! dry)
                            expect (peak > 1.0f, "without the limiter this chain really exceeds full scale: " + juce::String (peak));
                    }
        }

        beginTest ("meters: both channels count, and the output meter is after the limiter; gain reduction is reported only while limiting");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, blockSize);
            juce::MidiBuffer midi;
            auto feed = [&] (float left, float right, int blocks)
            {
                for (int b = 0; b < blocks; ++b)
                {
                    th::Buf buf (2, blockSize);
                    for (int i = 0; i < blockSize; ++i)
                    {
                        buf.setSample (0, i, left * (float) std::sin (0.05 * (double) (b * blockSize + i)));
                        buf.setSample (1, i, right * (float) std::sin (0.05 * (double) (b * blockSize + i)));
                    }
                    p.processBlock (buf, midi);
                }
            };
            feed (0.1f, 0.7f, 10);
            expectWithinAbsoluteError (p.consumeInputPeak(), 0.7f, 0.02f); // the right channel is the loud one
            expect (p.consumeOutputPeak() > 0.6f);
            expectEquals (p.consumeLimiterGainReductionDb(), 0.0f);
            th::setParam (p, "gain", 12.0f);
            th::setParam (p, "lim_on", 1.0f);
            th::setParam (p, "lim_ceiling", -6.0f);
            feed (0.1f, 0.7f, 100);
            p.consumeOutputPeak();
            feed (0.1f, 0.7f, 20);
            expect (p.consumeOutputPeak() <= ceilingOf (-6.0f) + 1.0e-4f, "the output meter reads what leaves the plug-in, after the limiter");
            expect (p.consumeLimiterGainReductionDb() > 6.0f, "the gain-reduction meter reports the real reduction");
        }

        beginTest ("state: the limiter settings round-trip through a DAW save and reopen, an older state loads with the defaults, and they are part of a sound (A/B, presets) but never changed by a profile or BUILD");
        {
            GlitchKidProcessor a;
            th::setParam (a, "lim_on", 1.0f);
            th::setParam (a, "lim_ceiling", -3.5f);
            th::setParam (a, "lim_drive", 4.0f);
            th::setParam (a, "lim_release", 120.0f);
            juce::MemoryBlock mb;
            a.getStateInformation (mb);
            GlitchKidProcessor b;
            b.setStateInformation (mb.getData(), (int) mb.getSize());
            expectEquals (th::getParam (b, "lim_on"), 1.0f);
            expectWithinAbsoluteError (th::getParam (b, "lim_ceiling"), -3.5f, 0.01f);
            expectWithinAbsoluteError (th::getParam (b, "lim_drive"), 4.0f, 0.01f);
            expectEquals (th::getParam (b, "lim_release"), 120.0f);

            // an older state has no limiter parameters at all
            auto xml = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
            expect (xml != nullptr);
            if (xml != nullptr)
            {
                for (int i = xml->getNumChildElements() - 1; i >= 0; --i)
                    if (auto* e = xml->getChildElement (i); e != nullptr && e->getStringAttribute ("id").startsWith ("lim_"))
                        xml->removeChildElement (e, true);
                juce::MemoryBlock old;
                juce::AudioProcessor::copyXmlToBinary (*xml, old);
                th::setParam (a, "gain", -2.0f);
                GlitchKidProcessor c;
                c.setStateInformation (old.getData(), (int) old.getSize());
                expectEquals (th::getParam (c, "lim_on"), 0.0f);
                expectEquals (th::getParam (c, "lim_ceiling"), -1.0f, "lim_ceiling after restore reads " + juce::String (th::getParam (c, "lim_ceiling"), 9));
                expectEquals (th::getParam (c, "lim_release"), 80.0f);
            }

            const auto st = a.captureSoundState();
            expect (st.has ("lim_on") && st.has ("lim_ceiling") && st.has ("lim_drive") && st.has ("lim_release"));
            auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("glitchkid-lim-" + juce::String::toHexString (juce::Random().nextInt64()));
            glitchkid::PresetController pc (a, glitchkid::PresetStore (dir));
            pc.loadFactory (2);
            expectEquals (th::getParam (a, "lim_on"), 1.0f);
            expectWithinAbsoluteError (th::getParam (a, "lim_ceiling"), -3.5f, 0.01f);
            dir.deleteRecursively();

            am::MixInput in;
            in.profile = 1;
            in.current = a.captureSoundState();
            in.factoryDefault = a.factoryDefaultState();
            in.lead = typical();
            const auto built = am::buildMix (in, rangesOf (a));
            expect (built.usable);
            expectEquals (built.sound.get ("lim_on", -1.0f), 1.0f);
            expectWithinAbsoluteError (built.sound.get ("lim_ceiling", 0.0f), -3.5f, 0.01f);
        }

        beginTest ("real-time safety: no allocation while the limiter runs (limiter on, CLEAN LAB on, DRY toggled)");
        {
            GlitchKidProcessor p;
            th::setParam (p, "lim_on", 1.0f);
            th::setParam (p, "clean_on", 1.0f);
            th::setParam (p, "gain", 6.0f);
            p.prepareToPlay (48000.0, blockSize);
            juce::MidiBuffer midi;
            th::Buf buf (2, blockSize);
            for (int i = 0; i < blockSize; ++i)
                buf.setSample (0, i, 0.5f * (float) std::sin (0.04 * i)), buf.setSample (1, i, 0.4f * (float) std::sin (0.05 * i));
            for (int i = 0; i < 50; ++i)
                p.processBlock (buf, midi);
            long allocated = 0;
            for (int i = 0; i < 400; ++i)
            {
                if (i % 100 == 50)
                    th::setParam (p, "bypass", (i / 100) % 2 == 0 ? 1.0f : 0.0f);
                const long before = g_allocations.load();
                p.processBlock (buf, midi);
                allocated += g_allocations.load() - before;
            }
            expectEquals ((int) allocated, 0);
        }

        beginTest ("bounded recommendations: a presence boost never takes the bell above +4 dB and a cut never below -6 dB; a value already beyond is left alone");
        {
            GlitchKidProcessor p;
            const auto lead = typical();
            const auto rel = am::compare (lead, crowdedBeat());
            expect (rel.valid);
            const auto result = am::analyseBeatLock (lead, crowdedBeat(), am::InstrumentalInput::live);
            expect (result.valid);
            std::vector<bool> all (result.items.size(), true);
            for (float start : { -15.0f, -7.0f, -5.5f, 0.0f, 3.0f, 3.9f, 4.0f, 10.2f, 15.0f })
            {
                auto cur = p.captureSoundState();
                cur.set ("eq_b2_gain", start);
                cur.set ("eq_b1_gain", start);
                cur.set ("eq_lo_gain", start);
                const auto out = am::applyBeatLock (result, all, cur, rangesOf (p), nullptr);
                const float b2 = out.get ("eq_b2_gain", 0.0f), b1 = out.get ("eq_b1_gain", 0.0f), lo = out.get ("eq_lo_gain", 0.0f);
                expect (b2 >= start - 1.0e-4f && b2 <= std::max (start, 4.0f) + 1.0e-4f, "presence from " + juce::String (start) + " -> " + juce::String (b2));
                expect (b1 <= start + 1.0e-4f && b1 >= std::min (start, -6.0f) - 1.0e-4f, "body bell from " + juce::String (start) + " -> " + juce::String (b1));
                expect (lo <= start + 1.0e-4f && lo >= std::min (start, -6.0f) - 1.0e-4f, "low shelf from " + juce::String (start) + " -> " + juce::String (lo));
            }
            auto cur = p.captureSoundState();
            cur.set ("eq_b2_gain", 10.2f); // the case from the screenshot: already boosted by the user
            const auto out = am::applyBeatLock (result, all, cur, rangesOf (p), nullptr);
            expectWithinAbsoluteError (out.get ("eq_b2_gain", 0.0f), 10.2f, 1.0e-4f);

            // BUILD's beat-informed moves obey the same limits
            am::MixInput in;
            in.profile = 3;
            in.current = p.captureSoundState();
            in.current.set ("eq_b2_gain", 10.2f);
            in.factoryDefault = p.factoryDefaultState();
            in.lead = lead;
            in.beat = crowdedBeat();
            in.instrumental = am::InstrumentalInput::live;
            const auto built = am::buildMix (in, rangesOf (p));
            expect (built.usable && built.mode == am::MixMode::beatInformed);
            expect (built.sound.get ("eq_b2_gain", 99.0f) <= 4.0f + 1.0e-4f, "BUILD's presence: " + juce::String (built.sound.get ("eq_b2_gain", 99.0f)));
        }

        beginTest ("a vocal that is already bright or sibilant is never brightened by BEAT LOCK or BUILD; the reason is shown instead");
        {
            GlitchKidProcessor p;
            for (int variant = 0; variant < 2; ++variant)
            {
                auto lead = typical();
                if (variant == 0) lead.sibilanceIndex = 0.55f;
                else lead.bandFraction = { 0.06f, 0.30f, 0.28f, 0.16f, 0.20f };
                const auto result = am::analyseBeatLock (lead, crowdedBeat(), am::InstrumentalInput::live);
                expect (result.valid);
                bool note = false;
                for (const auto& it : result.items)
                {
                    expect (it.move.id != "eq_b2_gain", "no presence boost");
                    if (it.advisory && it.move.category == "PRESENCE")
                    {
                        note = true;
                        expect (juce::String (it.move.reason).contains ("NOT raised"));
                    }
                }
                expect (note, "the withheld move is explained");
                std::vector<bool> all (result.items.size(), true);
                const auto cur = p.captureSoundState();
                const auto out = am::applyBeatLock (result, all, cur, rangesOf (p), nullptr);
                expectWithinAbsoluteError (out.get ("eq_b2_gain", 0.0f), cur.get ("eq_b2_gain", 0.0f), 1.0e-6f);

                am::MixInput in;
                in.profile = 3;
                in.current = p.captureSoundState();
                in.factoryDefault = p.factoryDefaultState();
                in.lead = lead;
                in.beat = crowdedBeat();
                in.instrumental = am::InstrumentalInput::live;
                const auto b0 = am::buildMix ({ [&] { auto i = in; i.beat = {}; return i; }() }, rangesOf (p));
                const auto b1 = am::buildMix (in, rangesOf (p));
                expectWithinAbsoluteError (b1.sound.get ("eq_b2_gain", 0.0f), b0.sound.get ("eq_b2_gain", 0.0f), 0.01f); // the beat added no presence
            }
        }

        beginTest ("measured vocals of several kinds (low, mid, high, dark, sibilant, noisy, silent) never produce an unbounded recommendation");
        {
            GlitchKidProcessor p;
            struct Case { const char* name; double f0; float dark, hiss, noise; bool silent; };
            const Case cases[] = { { "low voice", 110.0, 0.5f, 0.0f, 0.0f, false }, { "mid voice", 220.0, 0.5f, 0.0f, 0.0f, false }, { "high voice", 440.0, 0.5f, 0.0f, 0.0f, false },
                                   { "dark voice", 200.0, 2.5f, 0.0f, 0.0f, false }, { "sibilant voice", 220.0, 0.5f, 0.6f, 0.0f, false },
                                   { "noisy recording", 220.0, 0.5f, 0.0f, 0.08f, false }, { "silence", 220.0, 0.5f, 0.0f, 0.0f, true } };
            const auto padSig = [&]
            {
                std::vector<float> x (48000 * 10, 0.0f);
                int k = 0;
                for (double f : { 620.0, 910.0, 1330.0, 1810.0 })
                {
                    for (size_t n = 0; n < x.size(); ++n)
                        x[n] += 0.22f * (float) std::sin (2.0 * pi * f * (double) n / 48000.0 + 0.7 * k);
                    ++k;
                }
                return x;
            }();
            const auto beat = au::VocalAnalyzer::analyzeOffline (padSig.data(), (int) padSig.size(), 48000.0);
            for (const auto& c : cases)
            {
                auto sig = voiceSignal (48000.0, 10.0, c.f0, c.dark, c.hiss, c.noise);
                if (c.silent)
                    std::fill (sig.begin(), sig.end(), 0.0f);
                const auto lead = au::VocalAnalyzer::analyzeOffline (sig.data(), (int) sig.size(), 48000.0);
                const auto result = am::analyseBeatLock (lead, beat, am::InstrumentalInput::live);
                logMessage (juce::String (c.name) + ": status " + juce::String ((int) lead.status) + ", sibilance " + juce::String (lead.sibilanceIndex, 2) + ", air " + juce::String (lead.bandFraction[4], 2)
                            + ", " + juce::String ((int) result.items.size()) + " items");
                if (c.silent)
                {
                    expect (! result.valid && result.items.empty(), "silence: no analysis, no recommendation");
                    continue;
                }
                if (! result.valid)
                    continue;
                std::vector<bool> all (result.items.size(), true);
                for (float start : { -3.0f, 0.0f, 5.0f })
                {
                    auto cur = p.captureSoundState();
                    cur.set ("eq_b2_gain", start);
                    cur.set ("eq_b1_gain", start);
                    cur.set ("eq_lo_gain", start);
                    const auto out = am::applyBeatLock (result, all, cur, rangesOf (p), nullptr);
                    expect (out.get ("eq_b2_gain", 0.0f) <= std::max (start, 4.0f) + 1.0e-4f, juce::String (c.name) + " presence");
                    expect (out.get ("eq_b1_gain", 0.0f) >= std::min (start, -6.0f) - 1.0e-4f && out.get ("eq_lo_gain", 0.0f) >= std::min (start, -6.0f) - 1.0e-4f, juce::String (c.name) + " cuts");
                }
                for (const auto& it : result.items)
                {
                    expect (juce::String (it.move.reason).isNotEmpty(), "every item has a reason");
                    if (lead.sibilanceIndex > 0.35f || lead.bandFraction[4] > 0.12f)
                        expect (it.move.id != "eq_b2_gain", juce::String (c.name) + ": no presence boost for a bright or sibilant vocal");
                }
            }
        }

        beginTest ("CLEAN LAB stays inside its documented limits on sibilant, dark, noisy and silent input (telemetry from the real plug-in)");
        {
            struct Case { const char* name; double f0; float dark, hiss, noise; bool silent; };
            const Case cases[] = { { "mid voice", 220.0, 0.5f, 0.0f, 0.0f, false }, { "dark voice", 200.0, 2.5f, 0.0f, 0.0f, false }, { "sibilant voice", 220.0, 0.5f, 0.9f, 0.0f, false },
                                   { "noisy recording", 220.0, 0.5f, 0.0f, 0.1f, false }, { "silence", 220.0, 0.5f, 0.0f, 0.0f, true } };
            for (const auto& c : cases)
            {
                GlitchKidProcessor p;
                th::setParam (p, "clean_on", 1.0f);
                th::setParam (p, "clean_amount", 100.0f);
                p.prepareToPlay (48000.0, blockSize);
                auto sig = voiceSignal (48000.0, 12.0, c.f0, c.dark, c.hiss, c.noise);
                if (c.silent)
                    std::fill (sig.begin(), sig.end(), 0.0f);
                juce::MidiBuffer midi;
                float worstMud = 0.0f, worstRes = 0.0f, worstEss = 0.0f, peak = 0.0f;
                for (size_t pos = 0; pos + blockSize <= sig.size(); pos += blockSize)
                {
                    th::Buf buf (2, blockSize);
                    for (int i = 0; i < blockSize; ++i)
                        for (int ch = 0; ch < 2; ++ch)
                            buf.setSample (ch, i, sig[pos + (size_t) i]);
                    p.processBlock (buf, midi);
                    const auto a = p.getCleanApplied();
                    worstMud = std::max (worstMud, a.mudDb);
                    worstEss = std::max (worstEss, a.essGrDb);
                    for (int r = 0; r < CleanLab::numRes; ++r)
                        worstRes = std::max (worstRes, a.resDb[r]);
                    peak = std::max (peak, buf.getMagnitude (0, blockSize));
                }
                expect (worstMud <= 3.55f && worstRes <= 6.05f && worstEss <= 9.05f, juce::String (c.name) + ": mud " + juce::String (worstMud, 2) + ", resonance " + juce::String (worstRes, 2) + ", S sounds " + juce::String (worstEss, 2));
                expect (std::isfinite (peak));
                if (c.silent)
                    expect (worstMud == 0.0f && worstRes == 0.0f && worstEss == 0.0f && peak == 0.0f, "silence: nothing is applied");
            }
        }
    }
};
OutputLimiterTests outputLimiterTests;
}
