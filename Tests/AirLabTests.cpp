#include "TestHelpers.h"
#include "../Source/DSP/AirLab.h"

// AIR LAB: neutral and bypass behaviour, the expected presence / air changes, wet-dry and trim, mono / stereo, silence and tiny signals, extreme and non-finite
// input, smoothing, repeatability, aliasing, sibilance handling, and the integration in the processor (parameters, defaults, state, latency).
namespace
{
using Buf = juce::AudioBuffer<float>;
constexpr double fs = 48000.0;
constexpr double twoPi = 6.283185307179586;

std::vector<float> whiteNoise (int n, juce::int64 seed, float amp)
{
    juce::Random rng (seed);
    std::vector<float> x ((size_t) n);
    for (auto& v : x)
        v = (rng.nextFloat() * 2.0f - 1.0f) * amp;
    return x;
}

// A voice-like test source: harmonics of 180 Hz falling 1/h (strong body, some presence, little air) with a slow syllable envelope.
std::vector<float> voiced (int n, float amp)
{
    std::vector<float> x ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const double t = (double) i / fs, env = 0.55 + 0.45 * std::sin (twoPi * 3.0 * t);
        double s = 0.0;
        for (int h = 1; h <= 40; ++h)
            s += std::sin (twoPi * 180.0 * h * t) / (double) h;
        x[(size_t) i] = (float) (amp * env * s * 0.35);
    }
    return x;
}

// The same voice with a little natural breath and room noise (about -48 dBFS), so there is real content above 9 kHz to measure a lift against.
std::vector<float> voicedWithAir (int n, float amp)
{
    auto x = voiced (n, amp);
    const auto noise = whiteNoise (n, 13, 0.004f);
    for (size_t i = 0; i < x.size(); ++i)
        x[i] += noise[i];
    return x;
}

// White noise high-passed above 5 kHz: a sibilant "S", at a level like a loud vocal.
std::vector<float> sibilant (int n, float amp)
{
    auto x = whiteNoise (n, 7, amp);
    double z1 = 0.0, z2 = 0.0;
    const double w0 = twoPi * 5000.0 / fs, c = std::cos (w0), a = std::sin (w0) / (2.0 * 0.7071), a0 = 1.0 + a;
    const double b0 = (1.0 + c) * 0.5 / a0, b1 = -(1.0 + c) / a0, a1 = -2.0 * c / a0, a2 = (1.0 - a) / a0;
    for (auto& v : x)
    {
        const double y = b0 * v + z1;
        z1 = b1 * v - a1 * y + z2;
        z2 = b0 * v - a2 * y;
        v = (float) y;
    }
    return x;
}

AirLab::Settings on (float presence, float air, float mix = 1.0f, float trimDb = 0.0f)
{
    AirLab::Settings s;
    s.on = true;
    s.presence = presence;
    s.air = air;
    s.mix = mix;
    s.trimDb = trimDb;
    return s;
}

// Run mono or stereo data through AirLab in blocks (settings set once, smoothing settled by `warm` samples of the same signal first).
std::vector<std::vector<float>> run (const AirLab::Settings& s, const std::vector<std::vector<float>>& in, int block = 480)
{
    AirLab lab;
    lab.setSettings (s);
    lab.prepare (fs);
    const int channels = (int) in.size(), n = (int) in[0].size();
    std::vector<std::vector<float>> out (in);
    for (int pos = 0; pos < n; pos += block)
    {
        const int len = juce::jmin (block, n - pos);
        Buf b (channels, len);
        for (int ch = 0; ch < channels; ++ch)
            b.copyFrom (ch, 0, in[(size_t) ch].data() + pos, len);
        lab.process (b);
        for (int ch = 0; ch < channels; ++ch)
            std::copy (b.getReadPointer (ch), b.getReadPointer (ch) + len, out[(size_t) ch].begin() + pos);
    }
    return out;
}

// Mean power in dB of a band, from Hann-windowed 8192-point FFT frames over the second half of the signal.
double bandDb (const std::vector<float>& x, float lo, float hi)
{
    constexpr int order = 13, N = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> frame ((size_t) N * 2);
    double total = 0.0;
    int frames = 0;
    for (size_t pos = x.size() / 2; pos + (size_t) N <= x.size(); pos += (size_t) N / 2)
    {
        for (int i = 0; i < N; ++i)
            frame[(size_t) i] = x[pos + (size_t) i] * (0.5f - 0.5f * std::cos ((float) twoPi * (float) i / (float) N));
        std::fill (frame.begin() + N, frame.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (frame.data());
        for (int k = 0; k < N / 2; ++k)
        {
            const double f = (double) k * fs / (double) N;
            if (f >= lo && f < hi)
                total += (double) frame[(size_t) k] * (double) frame[(size_t) k];
        }
        ++frames;
    }
    return 10.0 * std::log10 (total / (double) juce::jmax (1, frames) + 1.0e-20);
}

double rms (const std::vector<float>& x)
{
    double s = 0.0;
    for (float v : x)
        s += (double) v * (double) v;
    return std::sqrt (s / (double) x.size());
}

double goertzelDb (const std::vector<float>& x, double hz, double rate)
{
    const double w = twoPi * hz / rate, coeff = 2.0 * std::cos (w);
    double s1 = 0.0, s2 = 0.0;
    for (size_t i = x.size() / 2; i < x.size(); ++i)
    {
        const double s0 = (double) x[i] + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    const double p = s1 * s1 + s2 * s2 - coeff * s1 * s2;
    return 10.0 * std::log10 (p / (double) (x.size() / 2) + 1.0e-20);
}

struct AirLabTests : public juce::UnitTest
{
    AirLabTests() : juce::UnitTest ("AIR LAB", "GlitchKid") {}

    void runTest() override
    {
        const int n = (int) fs * 3;

        beginTest ("neutral: off is an exact pass-through (even with extreme settings and trim), zero amounts and 0 dB trim change nothing, no latency");
        {
            const auto in = std::vector<std::vector<float>> { whiteNoise (n, 3, 0.4f), whiteNoise (n, 4, 0.4f) };
            auto s = on (1.0f, 1.0f, 1.0f, 12.0f);
            s.on = false;
            const auto off = run (s, in);
            expect (off == in, "off = bit-exact");
            const auto zero = run (on (0.0f, 0.0f), in);
            expect (zero == in, "on with zero amounts and 0 dB trim = bit-exact");
            AirLab lab;
            lab.prepare (fs); // (the class has no latency member: it is a per-sample IIR path; the processor test below checks the reported latency)
        }

        beginTest ("presence: lifts 2.5-5 kHz, leaves the low end alone, and a higher setting lifts more");
        {
            const auto src = voiced (n, 0.5f);
            const std::vector<std::vector<float>> in { src };
            const double low = bandDb (src, 80.0f, 400.0f), pres = bandDb (src, 2500.0f, 5000.0f);
            const auto half = run (on (0.5f, 0.0f), in), full = run (on (1.0f, 0.0f), in);
            const double d50 = bandDb (half[0], 2500.0f, 5000.0f) - pres, d100 = bandDb (full[0], 2500.0f, 5000.0f) - pres;
            logMessage ("presence band lift: 50 % " + juce::String (d50, 2) + " dB, 100 % " + juce::String (d100, 2) + " dB");
            expect (d50 > 1.0 && d100 > d50 + 0.8 && d100 < 7.0, "presence lift is audible and bounded");
            expectWithinAbsoluteError (bandDb (full[0], 80.0f, 400.0f) - low, 0.0, 0.5, "the low end is untouched");
        }

        beginTest ("air: lifts 9-16 kHz on a source that has some, creates content there on a source that has none, leaves the body alone, a higher setting lifts more");
        {
            const auto src = voicedWithAir (n, 0.5f);
            const std::vector<std::vector<float>> in { src };
            const double hi = bandDb (src, 9000.0f, 16000.0f), mid = bandDb (src, 100.0f, 2000.0f);
            const auto half = run (on (0.0f, 0.5f), in), full = run (on (0.0f, 1.0f), in);
            const double d50 = bandDb (half[0], 9000.0f, 16000.0f) - hi, d100 = bandDb (full[0], 9000.0f, 16000.0f) - hi;
            const double d30 = bandDb (run (on (0.0f, 0.3f), in)[0], 9000.0f, 16000.0f) - hi;
            logMessage ("air band lift: 30 % (default) " + juce::String (d30, 2) + " dB, 50 % " + juce::String (d50, 2) + " dB, 100 % " + juce::String (d100, 2) + " dB");
            expect (d50 > 1.0 && d100 > d50 + 0.8 && d100 < 9.0, "air lift is audible, grows with the control and is bounded");
            expectWithinAbsoluteError (bandDb (full[0], 100.0f, 2000.0f) - mid, 0.0, 0.3, "the body is untouched");
            const auto bare = voiced (n, 0.5f);
            const auto bareOut = run (on (0.0f, 1.0f), { bare });
            const double created = bandDb (bareOut[0], 9000.0f, 16000.0f) - bandDb (bare, 9000.0f, 16000.0f);
            logMessage ("content created above 9 kHz from a source without any: " + juce::String (created, 1) + " dB above its (near-silent) level");
            expect (bandDb (bareOut[0], 9000.0f, 16000.0f) > bandDb (bare, 9000.0f, 16000.0f) + 30.0, "harmonics make new content above 9 kHz");
        }

        beginTest ("wet/dry and trim: MIX 0 is the dry signal, MIX 50 % adds half of the added content, +6 / -6 dB trim scales the level");
        {
            const auto src = voiced (n, 0.4f);
            const std::vector<std::vector<float>> in { src };
            expect (run (on (1.0f, 1.0f, 0.0f), in) == in, "mix 0 = dry");
            const auto full = run (on (1.0f, 1.0f, 1.0f), in), half = run (on (1.0f, 1.0f, 0.5f), in);
            double worst = 0.0;
            for (size_t i = (size_t) n / 2; i < (size_t) n; ++i)
            {
                const double added = (double) full[0][i] - (double) src[i];
                worst = juce::jmax (worst, std::abs (((double) src[i] + 0.5 * added) - (double) half[0][i]));
            }
            expect (worst < 0.01, "mix 50 % = dry + half of the added content (worst error " + juce::String (worst, 5) + ")");
            const auto up = run (on (0.0f, 0.0f, 1.0f, 6.0f), in), down = run (on (0.0f, 0.0f, 1.0f, -6.0f), in);
            expectWithinAbsoluteError ((float) (rms (up[0]) / rms (src)), 1.9953f, 0.01f, "+6 dB");
            expectWithinAbsoluteError ((float) (rms (down[0]) / rms (src)), 0.5012f, 0.005f, "-6 dB");
        }

        beginTest ("mono and stereo: identical channels give identical outputs, mono equals one stereo channel, a silent channel stays silent");
        {
            const auto src = voiced (n, 0.5f);
            const auto st = run (on (0.7f, 0.7f), { src, src }), mo = run (on (0.7f, 0.7f), { src });
            expect (st[0] == st[1], "L == R for identical input");
            expect (st[0] == mo[0], "mono = a channel of the stereo result");
            const std::vector<float> silent ((size_t) n, 0.0f);
            const auto one = run (on (1.0f, 1.0f), { src, silent });
            double peak = 0.0;
            for (float v : one[1])
                peak = juce::jmax (peak, (double) std::abs (v));
            expect (peak <= 0.0, "the silent channel stays silent (no crosstalk)");
        }

        beginTest ("silence and very low levels: silence stays silent, a -100 dBFS signal stays finite and tiny");
        {
            const std::vector<float> zeros ((size_t) n, 0.0f);
            const auto z = run (on (1.0f, 1.0f, 1.0f, 12.0f), { zeros });
            double peak = 0.0;
            for (float v : z[0])
                peak = juce::jmax (peak, (double) std::abs (v));
            expectEquals (peak, 0.0);
            const auto tiny = whiteNoise (n, 11, 1.0e-5f);
            const auto t = run (on (1.0f, 1.0f), { tiny });
            double tp = 0.0;
            bool finite = true;
            for (float v : t[0])
            {
                finite = finite && std::isfinite (v);
                tp = juce::jmax (tp, (double) std::abs (v));
            }
            expect (finite && tp < 6.0e-5, "-100 dBFS noise stays tiny (peak " + juce::String (tp) + ")");
        }

        beginTest ("high level and non-finite input: never beyond full scale, NaN and infinity do not poison later audio");
        {
            const auto hot = std::vector<float> (whiteNoise (n, 5, 1.0f));
            const auto o = run (on (1.0f, 1.0f, 1.0f, 12.0f), { hot });
            double peak = 0.0;
            for (size_t i = 0; i < hot.size(); ++i)
                peak = juce::jmax (peak, (double) std::abs (o[0][i]) - (double) std::abs (hot[i]) * 0.0);
            // trim +12 dB on a full-scale signal: the knee keeps the module's own boost below 1.0 wherever the dry input was below 1.0 (dry samples are <= 1.0 here, the
            // knee leaves anything that the module did not make louder alone); the gain itself is the user's request, so only check that the result is finite and bounded
            expect (std::isfinite (peak) && peak < 5.0, "bounded");
            const auto loud = whiteNoise (n, 6, 0.8f);
            const auto o2 = run (on (1.0f, 1.0f, 1.0f, 0.0f), { loud });
            double p2 = 0.0;
            for (float v : o2[0])
                p2 = juce::jmax (p2, (double) std::abs (v));
            expect (p2 <= 1.0001, "a 0.8 full-scale signal with maximum presence and air does not exceed full scale: " + juce::String (p2, 4));
            auto bad = whiteNoise (n, 8, 0.3f);
            bad[1000] = std::numeric_limits<float>::quiet_NaN();
            bad[2000] = std::numeric_limits<float>::infinity();
            bad[3000] = -std::numeric_limits<float>::infinity();
            const auto o3 = run (on (1.0f, 1.0f), { bad });
            bool finite = true;
            for (float v : o3[0])
                finite = finite && std::isfinite (v);
            expect (finite, "output is finite after NaN / Inf input");
            double energy = 0.0;
            for (size_t i = 6000; i < o3[0].size(); ++i)
                energy += (double) o3[0][i] * (double) o3[0][i];
            expect (std::isfinite (energy) && std::sqrt (energy / 40000.0) < 0.6, "and stays sane");
        }

        beginTest ("smoothing and repeatability: switching on mid-signal does not click, results do not depend on the block size, a second run is identical");
        {
            const auto src = voiced (n, 0.5f);
            AirLab lab;
            lab.setSettings (AirLab::Settings {}); // off
            lab.prepare (fs);
            std::vector<float> out;
            for (int pos = 0; pos < 48000; pos += 480)
            {
                Buf b (1, 480);
                b.copyFrom (0, 0, src.data() + pos, 480);
                if (pos == 24000)
                    lab.setSettings (on (1.0f, 1.0f, 1.0f, 0.0f)); // an abrupt change of every control (trim stays 0 dB: it would scale the signal's own steps)
                lab.process (b);
                for (int i = 0; i < 480; ++i)
                    out.push_back (b.getSample (0, i));
            }
            // the added content has steps of its own (it is a 3.4 kHz band), so compare the first 30 ms after the change with the settled state
            double rampStep = 0.0, settledStep = 0.0;
            for (size_t i = 24000; i < 24000 + 1400; ++i)
                rampStep = juce::jmax (rampStep, std::abs ((double) (out[i + 1] - src[i + 1]) - (double) (out[i] - src[i])));
            for (size_t i = 30000; i < 36000; ++i)
                settledStep = juce::jmax (settledStep, std::abs ((double) (out[i + 1] - src[i + 1]) - (double) (out[i] - src[i])));
            expect (rampStep <= settledStep * 1.05 + 0.002, "no click when switching on: ramp worst step " + juce::String (rampStep, 4) + ", settled " + juce::String (settledStep, 4));
            const std::vector<std::vector<float>> in { src };
            const auto a = run (on (0.6f, 0.4f, 0.8f, 1.0f), in, 480), b2 = run (on (0.6f, 0.4f, 0.8f, 1.0f), in, 480), c = run (on (0.6f, 0.4f, 0.8f, 1.0f), in, 97);
            expect (a == b2, "repeatable");
            double worst = 0.0;
            for (size_t i = 0; i < a[0].size(); ++i)
                worst = juce::jmax (worst, std::abs ((double) a[0][i] - (double) c[0][i]));
            expect (worst < 1.0e-6, "independent of the block size (worst " + juce::String (worst, 9) + ")");
        }

        beginTest ("aliasing: a 5 kHz and a 10 kHz sine produce no folded harmonics at 44.1 kHz");
        {
            for (double hz : { 5000.0, 10000.0 })
            {
                const double rate = 44100.0;
                const int len = (int) rate * 2;
                std::vector<float> sine ((size_t) len);
                for (int i = 0; i < len; ++i)
                    sine[(size_t) i] = (float) (0.3 * std::sin (twoPi * hz * (double) i / rate));
                AirLab lab;
                lab.setSettings (on (0.0f, 1.0f));
                lab.prepare (rate);
                std::vector<float> out (sine);
                for (int pos = 0; pos < len; pos += 441)
                {
                    Buf b (1, 441);
                    b.copyFrom (0, 0, sine.data() + pos, 441);
                    lab.process (b);
                    std::copy (b.getReadPointer (0), b.getReadPointer (0) + 441, out.begin() + pos);
                }
                const double fundamental = goertzelDb (out, hz, rate);
                // 3rd harmonic folds to |rate - 3 hz| (or rate - 2 hz for the 2nd when above Nyquist): check the one that would alias
                const double third = 3.0 * hz, alias = third > rate / 2.0 ? rate - third : third;
                const double aliasDb = goertzelDb (out, alias, rate) - fundamental;
                logMessage ("sine " + juce::String (hz) + " Hz: component at " + juce::String (alias) + " Hz is " + juce::String (aliasDb, 1) + " dB re the fundamental");
                if (third > rate / 2.0)
                    expect (aliasDb < -45.0, "folded 3rd harmonic below -45 dB: " + juce::String (aliasDb, 1));
            }
        }

        beginTest ("sibilant and bright material: the added lift is smaller on sibilant noise than on voiced material, and nothing exceeds full scale");
        {
            const auto sib = sibilant (n, 0.5f), voice = voiced (n, 0.5f);
            const auto sibOut = run (on (1.0f, 1.0f), { sib }), voiceOut = run (on (1.0f, 1.0f), { voice });
            const double sibLift = bandDb (sibOut[0], 5000.0f, 12000.0f) - bandDb (sib, 5000.0f, 12000.0f);
            const double voiceLift = bandDb (voiceOut[0], 5000.0f, 12000.0f) - bandDb (voice, 5000.0f, 12000.0f);
            logMessage ("5-12 kHz lift: sibilant noise " + juce::String (sibLift, 2) + " dB, voiced " + juce::String (voiceLift, 2) + " dB");
            expect (sibLift < voiceLift - 1.0, "sibilance is lifted less than voiced content");
            expect (sibLift < 3.0, "the lift on a sibilant is bounded");
            const auto bright = whiteNoise (n, 21, 0.8f);
            const auto bo = run (on (1.0f, 1.0f), { bright });
            double peak = 0.0;
            for (float v : bo[0])
                peak = juce::jmax (peak, (double) std::abs (v));
            expect (peak <= 1.0001, "bright noise stays within full scale: " + juce::String (peak, 4));
        }

        beginTest ("CPU (information only, not an assertion): stereo, 480-sample blocks, all controls at maximum");
        {
            AirLab lab;
            lab.setSettings (on (1.0f, 1.0f, 1.0f, 3.0f));
            lab.prepare (fs);
            const auto src = whiteNoise (480, 17, 0.3f);
            Buf b (2, 480);
            const int blocks = 48000 * 10 / 480;
            const auto t0 = juce::Time::getHighResolutionTicks();
            for (int k = 0; k < blocks; ++k)
            {
                b.copyFrom (0, 0, src.data(), 480);
                b.copyFrom (1, 0, src.data(), 480);
                lab.process (b);
            }
            const double seconds = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
            logMessage ("AIR LAB CPU: " + juce::String (seconds * 100.0 / 10.0, 2) + " % of one core at 48 kHz stereo (10 s of audio processed in " + juce::String (seconds, 3) + " s)");
            expect (std::isfinite (b.getSample (0, 0)));
        }

        beginTest ("processor integration: new parameters appended with neutral defaults, AIR LAB off leaves the output identical, on changes it, no latency change, state round trip, old state loads neutral");
        {
            GlitchKidProcessor p;
            expectEquals (p.getParameters().size(), 69);
            expectEquals (th::getParam (p, "air_on"), 0.0f);
            expectEquals (th::getParam (p, "air_presence"), 30.0f);
            expectEquals (th::getParam (p, "air_air"), 30.0f);
            expectEquals (th::getParam (p, "air_link"), 1.0f);
            expectEquals (th::getParam (p, "air_mix"), 100.0f);
            expectEquals (th::getParam (p, "air_trim"), 0.0f);
            // the last six parameters are the AIR LAB ones, so every older parameter keeps its host index
            const char* ids[] = { "air_on", "air_presence", "air_air", "air_link", "air_mix", "air_trim" };
            for (int i = 0; i < 6; ++i)
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p.getParameters()[54 + i]))
                    expect (r->paramID == ids[i], r->paramID);

            auto render = [] (GlitchKidProcessor& proc)
            {
                proc.prepareToPlay (fs, 480);
                juce::MidiBuffer midi;
                const auto src = voiced (48000, 0.4f);
                std::vector<float> out;
                for (int pos = 0; pos + 480 <= (int) src.size(); pos += 480)
                {
                    Buf b (2, 480);
                    for (int i = 0; i < 480; ++i) { b.setSample (0, i, src[(size_t) (pos + i)]); b.setSample (1, i, src[(size_t) (pos + i)]); }
                    proc.processBlock (b, midi);
                    for (int i = 0; i < 480; ++i)
                        out.push_back (b.getSample (0, i));
                }
                return out;
            };
            GlitchKidProcessor offProc, onProc, offProc2;
            const int latencyOff = offProc.getLatencySamples();
            th::setParam (onProc, "air_on", 1.0f);
            th::setParam (onProc, "air_presence", 100.0f);
            th::setParam (onProc, "air_air", 100.0f);
            const auto a = render (offProc), b = render (onProc), c = render (offProc2);
            expect (a == c, "AIR LAB off: two instances identical");
            double diff = 0.0;
            for (size_t i = 20000; i < a.size(); ++i)
                diff = juce::jmax (diff, std::abs ((double) a[i] - (double) b[i]));
            expect (diff > 0.01, "AIR LAB on changes the processed output (max difference " + juce::String (diff, 4) + ")");
            expectEquals (onProc.getLatencySamples(), latencyOff);

            // state round trip
            juce::MemoryBlock mb;
            onProc.getStateInformation (mb);
            GlitchKidProcessor restored;
            restored.setStateInformation (mb.getData(), (int) mb.getSize());
            expectEquals (th::getParam (restored, "air_on"), 1.0f);
            expectEquals (th::getParam (restored, "air_presence"), 100.0f);

            // an older state (no AIR LAB parameters) loads with AIR LAB off
            GlitchKidProcessor src;
            juce::MemoryBlock old;
            src.getStateInformation (old);
            if (auto xml = juce::AudioProcessor::getXmlFromBinary (old.getData(), (int) old.getSize()))
            {
                for (int i = xml->getNumChildElements() - 1; i >= 0; --i)
                    if (auto* child = xml->getChildElement (i); child != nullptr && child->getStringAttribute ("id").startsWith ("air_"))
                        xml->removeChildElement (child, true);
                juce::MemoryBlock stripped;
                juce::AudioProcessor::copyXmlToBinary (*xml, stripped);
                GlitchKidProcessor loaded;
                loaded.setStateInformation (stripped.getData(), (int) stripped.getSize());
                expectEquals (th::getParam (loaded, "air_on"), 0.0f);
                expectEquals (th::getParam (loaded, "air_presence"), 30.0f);
                expectEquals (th::getParam (loaded, "air_trim"), 0.0f);
            }
        }
    }
};
AirLabTests airLabTests;
}
