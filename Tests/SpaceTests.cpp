#include "TestHelpers.h"
#include "../Source/DSP/SpaceReverb.h"
#include "../Source/Presets/SpacePresets.h"
#include <juce_dsp/juce_dsp.h>

// SPACE: measured, not assumed. Decay (RT60 from the Schroeder integral), pre-delay, how the eight algorithms differ (early energy, build-up, tone,
// modulation smear, shimmer), the characters and damping (tail brightness), width, cuts, freeze, tails that ring out, no clicks on an algorithm
// change, no metallic resonances, stability under wild automation, mono, and the cost.
namespace
{
constexpr double twoPi = 6.283185307179586;
using Settings = SpaceReverb::Settings;

Settings wetOnly (int algo, float decay)
{
    Settings s;
    s.on = true;
    s.algorithm = algo;
    s.mix = 1.0f;
    s.decaySeconds = decay;
    s.predelayMs = 0.0f;
    s.lowCutHz = 20.0f;
    s.highCutHz = 20000.0f;
    return s;
}

// the wet impulse response (stereo), `seconds` long
std::vector<float> impulse (const Settings& s, double fs, double seconds, std::vector<float>* right = nullptr)
{
    SpaceReverb r;
    r.prepare (fs);
    r.setSettings (s);
    r.reset();
    const int n = (int) (fs * seconds), block = 256;
    std::vector<float> outL ((size_t) n), outR ((size_t) n);
    for (int pos = 0; pos < n; pos += block)
    {
        const int m = std::min (block, n - pos);
        th::Buf b (2, m);
        b.clear();
        if (pos == 0)
        {
            b.setSample (0, 0, 1.0f);
            b.setSample (1, 0, 1.0f);
        }
        r.process (b);
        for (int i = 0; i < m; ++i)
        {
            outL[(size_t) (pos + i)] = b.getSample (0, i);
            outR[(size_t) (pos + i)] = b.getSample (1, i);
        }
    }
    if (right != nullptr)
        *right = outR;
    for (size_t i = 0; i < outL.size(); ++i)
        outL[i] = 0.5f * (outL[i] + outR[i]);
    return outL;
}

// RT60 from the Schroeder backward integral: the time between -5 and -25 dB, times 3 (T20)
double rt60 (const std::vector<float>& h, double fs)
{
    std::vector<double> edc (h.size());
    double acc = 0.0;
    for (size_t i = h.size(); i-- > 0;)
    {
        acc += (double) h[i] * h[i];
        edc[i] = acc;
    }
    const double total = edc[0];
    int t5 = -1, t25 = -1;
    for (size_t i = 0; i < edc.size(); ++i)
    {
        const double db = 10.0 * std::log10 (edc[i] / total + 1e-30);
        if (t5 < 0 && db <= -5.0) t5 = (int) i;
        if (t25 < 0 && db <= -25.0) { t25 = (int) i; break; }
    }
    return t5 < 0 || t25 < 0 ? -1.0 : 3.0 * (t25 - t5) / fs;
}

double energy (const std::vector<float>& h, int from, int to)
{
    double e = 0.0;
    for (int i = std::max (0, from); i < std::min ((int) h.size(), to); ++i)
        e += (double) h[(size_t) i] * h[(size_t) i];
    return e;
}

// spectral centroid (Hz) of a segment (Welch average of 4096-point Hann frames)
std::vector<double> welch (const std::vector<float>& x, int from, int to, int order = 12)
{
    const int n = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> work ((size_t) (2 * n));
    std::vector<double> p ((size_t) (n / 2), 0.0);
    int frames = 0;
    for (int s = from; s + n <= to; s += n / 2, ++frames)
    {
        for (int i = 0; i < n; ++i)
            work[(size_t) i] = x[(size_t) (s + i)] * (float) (0.5 - 0.5 * std::cos (twoPi * i / (n - 1)));
        std::fill (work.begin() + n, work.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (work.data());
        for (int b = 0; b < n / 2; ++b)
            p[(size_t) b] += (double) work[(size_t) b] * work[(size_t) b];
    }
    for (auto& v : p) v /= std::max (1, frames);
    return p;
}

double centroid (const std::vector<double>& p, double fs)
{
    const double binHz = fs / (2.0 * (double) p.size());
    double num = 0.0, den = 0.0;
    for (size_t b = 1; b < p.size(); ++b)
    {
        num += p[b] * b * binHz;
        den += p[b];
    }
    return den > 0.0 ? num / den : 0.0;
}

// the largest narrow peak (dB over the median of its +-1/6 octave) between 200 Hz and 8 kHz: metallic ringing shows as a high value
double worstResonanceDb (const std::vector<double>& p, double fs)
{
    const double binHz = fs / (2.0 * (double) p.size());
    double worst = 0.0;
    for (size_t b = (size_t) (200.0 / binHz); b < (size_t) (8000.0 / binHz) && b < p.size(); ++b)
    {
        const size_t lo = (size_t) (b / 1.122), hi = std::min (p.size() - 1, (size_t) (b * 1.122));
        std::vector<double> nb (p.begin() + (long) lo, p.begin() + (long) hi + 1);
        std::nth_element (nb.begin(), nb.begin() + (long) nb.size() / 2, nb.end());
        const double med = nb[nb.size() / 2];
        if (med > 0.0)
            worst = std::max (worst, 10.0 * std::log10 (p[b] / med));
    }
    return worst;
}

// steady sine through the reverb (wet only): the share of energy more than 8 Hz away from the tone (modulation smear), and the energy at 2x (shimmer)
std::pair<double, double> sineSmear (int algo, double fs)
{
    SpaceReverb r;
    r.prepare (fs);
    auto s = wetOnly (algo, 3.0f);
    s.modDepth = 0.5f;
    r.setSettings (s);
    r.reset();
    const int n = (int) (fs * 4.0);
    std::vector<float> out ((size_t) n);
    for (int pos = 0; pos < n; pos += 256)
    {
        th::Buf b (2, 256);
        for (int i = 0; i < 256; ++i)
        {
            const float v = 0.3f * (float) std::sin (twoPi * 1000.0 * (pos + i) / fs);
            b.setSample (0, i, v);
            b.setSample (1, i, v);
        }
        r.process (b);
        for (int i = 0; i < 256; ++i)
            out[(size_t) (pos + i)] = b.getSample (0, i);
    }
    const auto p = welch (out, (int) fs * 2, n, 15);
    const double binHz = fs / (2.0 * (double) p.size());
    double near = 0.0, smear = 0.0, octave = 0.0, all = 0.0;
    for (size_t b = 1; b < p.size(); ++b)
    {
        const double hz = b * binHz;
        all += p[b];
        if (std::abs (hz - 1000.0) <= 8.0) near += p[b];
        else if (std::abs (hz - 1000.0) <= 200.0) smear += p[b];
        if (std::abs (hz - 2000.0) <= 30.0) octave += p[b];
    }
    return { smear / (near + smear), octave / all };
}
} // namespace

struct SpaceTests : public juce::UnitTest
{
    SpaceTests() : juce::UnitTest ("SPACE reverb", "GlitchKid") {}

    void runTest() override
    {
        const double fs = 48000.0;

        beginTest ("off is an exact pass-through; on, it does something; no latency");
        {
            SpaceReverb r;
            r.prepare (fs);
            Settings s;
            r.setSettings (s);
            auto in = th::makeNoise (2, 512, 4), b = in;
            r.process (b);
            expectEquals (th::maxAbsDiff (in, b), 0.0f, "off: untouched");
            s.on = true;
            r.setSettings (s);
            for (int k = 0; k < 20; ++k)
            {
                b = th::makeNoise (2, 512, k);
                r.process (b);
            }
            expect (r.isActive(), "on: active");
        }

        beginTest ("decay is the real RT60 (Schroeder T20), at 44.1, 48 and 96 kHz");
        {
            for (double rate : { 44100.0, 48000.0, 96000.0 })
                for (auto [algo, decay] : { std::pair<int, float> { 3, 2.0f }, { 0, 0.8f }, { 2, 2.5f }, { 4, 5.0f } })
                {
                    auto s = wetOnly (algo, decay);
                    s.character = 2;   // modern: low and high decay close to the setting (0.85x / 0.9x)
                    s.damping = 0.0f;
                    s.modDepth = 0.0f;
                    const auto h = impulse (s, rate, decay * 1.6 + 0.5);
                    const double t = rt60 (h, rate);
                    expectWithinAbsoluteError (t, 0.87 * decay, 0.2 * decay, juce::String (SpaceReverb::algorithmName (algo)) + " at " + juce::String (rate) + ": RT60 "
                                               + juce::String (t, 3) + " s for " + juce::String (decay) + " s");
                }
        }

        beginTest ("pre-delay moves the whole reverb later by exactly its time");
        {
            auto onset = [&] (float ms)
            {
                auto s = wetOnly (0, 1.0f);
                s.predelayMs = ms;
                const auto h = impulse (s, fs, 0.5);
                for (size_t i = 0; i < h.size(); ++i)
                    if (std::abs (h[i]) > 1.0e-3f)
                        return (double) i / fs * 1000.0;
                return -1.0;
            };
            expectWithinAbsoluteError (onset (100.0f) - onset (0.0f), 100.0, 0.1, "100 ms of pre-delay");
        }

        beginTest ("the eight algorithms are really different (early energy, build-up, tone, modulation, shimmer)");
        {
            struct M { double early, build, bright, smear, shimmer; };
            std::vector<M> m;
            for (int a = 0; a < SpaceReverb::numAlgorithms; ++a)
            {
                auto s = wetOnly (a, 2.0f);
                const auto h = impulse (s, fs, 2.5);
                const double total = energy (h, 0, (int) h.size());
                // build-up: when the 10 ms envelope peaks
                int peakAt = 0;
                double peak = 0.0;
                for (int t = 0; t + 480 < (int) h.size() / 2; t += 240)
                {
                    const double e = energy (h, t, t + 480);
                    if (e > peak) { peak = e; peakAt = t; }
                }
                const auto smear = sineSmear (a, fs);
                m.push_back ({ energy (h, 0, (int) (0.05 * fs)) / total, peakAt / fs * 1000.0, centroid (welch (h, (int) (0.1 * fs), (int) (0.6 * fs)), fs),
                               smear.first, smear.second });
                logMessage (juce::String (SpaceReverb::algorithmName (a)).paddedRight (' ', 20) + " early (0-50 ms) " + juce::String (m.back().early * 100.0, 1)
                            + " %, build-up peak " + juce::String (m.back().build, 1) + " ms, tail centroid " + juce::String (m.back().bright, 0) + " Hz, smear "
                            + juce::String (m.back().smear * 100.0, 2) + " %, octave " + juce::String (m.back().shimmer * 100.0, 3) + " %");
            }
            for (int a = 0; a < SpaceReverb::numAlgorithms; ++a)
                for (int b = a + 1; b < SpaceReverb::numAlgorithms; ++b)
                {
                    auto rel = [] (double x, double y) { return std::abs (x - y) / std::max ({ std::abs (x), std::abs (y), 1.0e-9 }); };
                    const double d = std::max ({ rel (m[(size_t) a].early, m[(size_t) b].early), rel (m[(size_t) a].build, m[(size_t) b].build),
                                                 rel (m[(size_t) a].bright, m[(size_t) b].bright), rel (m[(size_t) a].smear, m[(size_t) b].smear),
                                                 rel (m[(size_t) a].shimmer, m[(size_t) b].shimmer) });
                    expect (d > 0.25, juce::String (SpaceReverb::algorithmName (a)) + " and " + SpaceReverb::algorithmName (b) + " differ by " + juce::String (d * 100.0, 0) + " % at most");
                }
            expect (m[5].early > m[3].early && m[5].early > m[2].early, "ambience is mostly early reflections");
            expect (m[2].early < m[0].early, "a plate has no early reflections");
            expect (m[4].build > m[0].build, "a cathedral builds up later than a room");
            expect (m[6].smear > 2.0 * m[3].smear, "modulated space swims more than the hall");
            expect (m[7].shimmer > 20.0 * m[3].shimmer, "experimental space adds an octave above");
        }

        beginTest ("characters and damping: WARM is darker than CLASSIC, CLASSIC darker than MODERN; more damping is darker");
        {
            auto bright = [&] (int ch, float damping)
            {
                auto s = wetOnly (3, 2.5f);
                s.character = ch;
                s.damping = damping;
                const auto h = impulse (s, fs, 2.0);
                return centroid (welch (h, (int) (0.3 * fs), (int) (1.5 * fs)), fs);
            };
            const double warm = bright (0, 0.5f), classic = bright (1, 0.5f), modern = bright (2, 0.5f);
            logMessage ("tail centroid: warm " + juce::String (warm, 0) + " Hz, classic " + juce::String (classic, 0) + " Hz, modern " + juce::String (modern, 0) + " Hz");
            expect (warm < 0.85 * classic && classic < 0.85 * modern);
            expect (bright (1, 0.9f) < 0.8 * bright (1, 0.1f), "damping darkens the tail");
        }

        beginTest ("width: 0 is a mono wet signal, more width decorrelates; low and high cut shape the wet signal");
        {
            auto corr = [&] (float width)
            {
                auto s = wetOnly (3, 2.0f);
                s.width = width;
                std::vector<float> r;
                SpaceReverb rv;
                rv.prepare (fs);
                rv.setSettings (s);
                rv.reset();
                double lr = 0.0, ll = 0.0, rr = 0.0;
                for (int k = 0; k < 400; ++k)
                {
                    auto b = th::makeNoise (1, 256, k);
                    th::Buf st (2, 256);
                    st.copyFrom (0, 0, b, 0, 0, 256);
                    st.copyFrom (1, 0, b, 0, 0, 256);
                    rv.process (st);
                    if (k > 100)
                        for (int i = 0; i < 256; ++i)
                        {
                            const double l = st.getSample (0, i), q = st.getSample (1, i);
                            lr += l * q; ll += l * l; rr += q * q;
                        }
                }
                return lr / std::sqrt (ll * rr);
            };
            expectWithinAbsoluteError (corr (0.0f), 1.0, 1.0e-6, "width 0: mono");
            const double c1 = corr (1.0f), c15 = corr (1.5f);
            expect (c1 < 0.8 && c15 < c1, "decorrelated: " + juce::String (c1, 3) + " at 100 %, " + juce::String (c15, 3) + " at 150 %");

            auto s = wetOnly (3, 1.5f);
            const auto open = welch (impulse (s, fs, 1.5), 0, (int) (1.4 * fs));
            s.lowCutHz = 600.0f;
            s.highCutHz = 3000.0f;
            const auto cut = welch (impulse (s, fs, 1.5), 0, (int) (1.4 * fs));
            const double binHz = fs / (2.0 * (double) open.size());
            auto band = [&] (const std::vector<double>& p, double lo, double hi) { double e = 0.0; for (size_t b = (size_t) (lo / binHz); b < (size_t) (hi / binHz); ++b) e += p[b]; return e; };
            expect (band (cut, 80, 200) < 0.2 * band (open, 80, 200), "low cut");
            expect (band (cut, 9000, 14000) < 0.2 * band (open, 9000, 14000), "high cut");
            expect (band (cut, 1000, 2000) > 0.5 * band (open, 1000, 2000), "the middle stays");
        }

        beginTest ("freeze holds the tail; switching SPACE off lets the tail ring out, then it goes idle and passes audio untouched");
        {
            SpaceReverb r;
            r.prepare (fs);
            auto s = wetOnly (3, 1.5f);
            r.setSettings (s);
            r.reset();
            auto rms = [&] (int blocks, bool feed)
            {
                double e = 0.0;
                for (int k = 0; k < blocks; ++k)
                {
                    auto b = feed ? th::makeNoise (2, 480, k) : th::Buf (2, 480);
                    if (! feed) b.clear();
                    r.process (b);
                    e += b.getRMSLevel (0, 0, 480) * b.getRMSLevel (0, 0, 480);
                }
                return 10.0 * std::log10 (e / blocks + 1e-30);
            };
            rms (100, true);
            s.freeze = true;
            r.setSettings (s);
            const double start = rms (10, false), later = rms (200, false); // 2 s of silence in
            expect (later > start - 3.0, "frozen: " + juce::String (start, 1) + " dB, 2 s later " + juce::String (later, 1) + " dB");
            s.freeze = false;
            r.setSettings (s);
            rms (100, true);
            s.on = false;
            r.setSettings (s);
            const double right = rms (5, false), after = rms (20, false);
            expect (after > right - 30.0 && after < right, "the tail rings out after switching off (" + juce::String (right, 1) + " -> " + juce::String (after, 1) + " dB)");
            rms (600, false);
            expect (! r.isActive(), "idle once the tail is gone");
            auto in = th::makeNoise (2, 480, 9), b = in;
            r.process (b);
            expectEquals (th::maxAbsDiff (in, b), 0.0f, "then untouched");
        }

        beginTest ("an algorithm change never clicks; no metallic ringing in any algorithm's tail");
        {
            SpaceReverb r;
            r.prepare (fs);
            auto s = wetOnly (0, 2.0f);
            s.mix = 0.5f;
            r.setSettings (s);
            r.reset();
            float worst = 0.0f, prev = 0.0f;
            int pos = 0;
            for (int a = 0; a < SpaceReverb::numAlgorithms; ++a)
            {
                s.algorithm = a;
                r.setSettings (s);
                for (int k = 0; k < 40; ++k, pos += 256)
                {
                    th::Buf b (2, 256);
                    for (int i = 0; i < 256; ++i)
                    {
                        const float v = 0.3f * (float) std::sin (twoPi * 150.0 * (pos + i) / fs);
                        b.setSample (0, i, v);
                        b.setSample (1, i, v);
                    }
                    r.process (b);
                    for (int i = 0; i < 256; ++i)
                    {
                        worst = std::max (worst, std::abs (b.getSample (0, i) - prev));
                        prev = b.getSample (0, i);
                    }
                }
            }
            expect (worst < 0.06f, "largest step " + juce::String (worst, 4) + " (a 150 Hz sine at 0.3 steps by about 0.006 per sample)");

            for (int a = 0; a < SpaceReverb::numAlgorithms; ++a)
            {
                auto t = wetOnly (a, 2.5f);
                const auto h = impulse (t, fs, 2.0);
                const double res = worstResonanceDb (welch (h, (int) (0.15 * fs), (int) (1.6 * fs)), fs);
                logMessage (juce::String (SpaceReverb::algorithmName (a)).paddedRight (' ', 20) + " largest narrow peak in the tail: " + juce::String (res, 1) + " dB");
                expect (res < 12.0, juce::String (SpaceReverb::algorithmName (a)) + ": a resonance of " + juce::String (res, 1) + " dB");
            }
        }

        beginTest ("stability: wild automation, freeze toggling, extreme settings and non-finite input stay finite and bounded; mono works");
        {
            for (double rate : { 44100.0, 96000.0 })
            {
                SpaceReverb r;
                r.prepare (rate);
                juce::Random rng (5);
                float peak = 0.0f;
                for (int k = 0; k < 600; ++k)
                {
                    Settings s;
                    s.on = rng.nextInt (10) != 0;
                    s.algorithm = rng.nextInt (8);
                    s.character = rng.nextInt (3);
                    s.mix = rng.nextFloat();
                    s.decaySeconds = 0.2f + 19.8f * rng.nextFloat();
                    s.predelayMs = 250.0f * rng.nextFloat();
                    s.size = rng.nextFloat();
                    s.diffusion = rng.nextFloat();
                    s.damping = rng.nextFloat();
                    s.modRateHz = 0.01f + 9.0f * rng.nextFloat();
                    s.modDepth = rng.nextFloat();
                    s.width = 1.5f * rng.nextFloat();
                    s.freeze = rng.nextInt (6) == 0;
                    r.setSettings (s);
                    auto b = th::makeNoise (2, 128, k);
                    if (k % 97 == 0)
                        b.setSample (0, 5, std::numeric_limits<float>::quiet_NaN());
                    r.process (b);
                    peak = std::max ({ peak, b.getMagnitude (0, 0, 128), b.getMagnitude (1, 0, 128) });
                    expect (std::isfinite (peak), "finite at block " + juce::String (k));
                }
                expect (peak < 6.0f, "bounded: peak " + juce::String (peak));
            }
            SpaceReverb mono;
            mono.prepare (fs);
            mono.setSettings (wetOnly (3, 1.0f));
            mono.reset();
            th::Buf b (1, 480);
            b.clear();
            b.setSample (0, 0, 1.0f);
            mono.process (b);
            for (int k = 0; k < 10; ++k) { th::Buf z (1, 480); z.clear(); mono.process (z); expect (z.getMagnitude (0, 0, 480) < 2.0f); }
        }

        beginTest ("the nine vocal presets, through the real processor: each decays as written, stays clean and in level, and the processor reports its tail");
        {
            namespace sp = glitchkid::space;
            for (int i = 0; i < sp::numPresets; ++i)
            {
                const auto& pr = sp::presets[i];
                GlitchKidProcessor p;
                p.prepareToPlay (fs, 480);
                const float v[17] = { 1.0f, (float) pr.algorithm, (float) pr.character, 100.0f, pr.decay, pr.predelay, pr.size, pr.diffusion, pr.damping,
                                      pr.lowCut, pr.highCut, pr.modRate, pr.modDepth, pr.width, pr.early, pr.late, 0.0f };
                for (int k = 0; k < 17; ++k)
                    th::setParam (p, GlitchKidProcessor::spaceIds[k], v[k]);
                expectWithinAbsoluteError (p.getTailLengthSeconds(), pr.predelay * 0.001 + pr.decay * 1.2 + 0.2, 0.01, juce::String (pr.name) + ": the host is told the tail");
                // the impulse response through the whole plug-in (wet only), then a voice-like signal at the preset's own mix
                juce::MidiBuffer midi;
                const int n = (int) (fs * (pr.decay * 1.6 + 0.6));
                std::vector<float> h ((size_t) n);
                for (int pos = 0; pos < n; pos += 480)
                {
                    th::Buf b (2, 480);
                    b.clear();
                    if (pos == 0) { b.setSample (0, 0, 1.0f); b.setSample (1, 0, 1.0f); }
                    p.processBlock (b, midi);
                    for (int k = 0; k < 480 && pos + k < n; ++k)
                        h[(size_t) (pos + k)] = 0.5f * (b.getSample (0, k) + b.getSample (1, k));
                }
                const double t = rt60 (h, fs);
                const auto& c = pr.character == 0 ? 1.0 : pr.character == 1 ? 0.8 : 0.87; // the broadband decay sits between the character's low and high ratios
                expect (t > 0.45 * pr.decay * c && t < 1.6 * pr.decay * c, juce::String (pr.name) + ": RT60 " + juce::String (t, 2) + " s for " + juce::String (pr.decay) + " s");
                const double res = worstResonanceDb (welch (h, (int) (0.1 * fs), juce::jmin (n, (int) (fs * (0.1 + pr.decay))), 12), fs);
                expect (res < 12.0, juce::String (pr.name) + ": a resonance of " + juce::String (res, 1) + " dB");

                th::setParam (p, GlitchKidProcessor::spaceIds[3], pr.mix);
                double inE = 0.0, outE = 0.0;
                float peak = 0.0f;
                for (int k = 0; k < 300; ++k)
                {
                    th::Buf b (2, 480);
                    for (int j = 0; j < 480; ++j)
                    {
                        const double tt = (double) (k * 480 + j) / fs;
                        const float x = 0.3f * (float) (std::sin (twoPi * 196.0 * tt) + 0.5 * std::sin (twoPi * 392.0 * tt) + 0.25 * std::sin (twoPi * 2900.0 * tt))
                                        * (0.5f + 0.5f * (float) std::sin (twoPi * 2.0 * tt));
                        b.setSample (0, j, x);
                        b.setSample (1, j, x);
                        inE += (double) x * x;
                    }
                    p.processBlock (b, midi);
                    for (int j = 0; j < 480; ++j)
                        outE += (double) b.getSample (0, j) * b.getSample (0, j);
                    peak = std::max (peak, b.getMagnitude (0, 0, 480));
                }
                const double levelDb = 10.0 * std::log10 (outE / inE);
                logMessage (juce::String (pr.name).paddedRight (' ', 26) + "RT60 " + juce::String (t, 2) + " s (set " + juce::String (pr.decay) + "), narrow peak "
                            + juce::String (res, 1) + " dB, level at its mix " + juce::String (levelDb, 2) + " dB, peak " + juce::String (peak, 2));
                expect (std::abs (levelDb) < 2.0 && peak < 0.99f, juce::String (pr.name) + ": in level at its own mix");
            }
        }

        beginTest ("CPU (information only): each algorithm, stereo, 48 kHz, 512-sample blocks");
        {
            for (int a = 0; a < SpaceReverb::numAlgorithms; ++a)
            {
                SpaceReverb r;
                r.prepare (fs);
                auto s = wetOnly (a, 2.0f);
                s.mix = 0.3f;
                r.setSettings (s);
                auto x = th::makeNoise (2, 512, 2);
                const int blocks = 48000 * 10 / 512;
                const auto t0 = juce::Time::getHighResolutionTicks();
                for (int k = 0; k < blocks; ++k)
                {
                    th::Buf b (x);
                    r.process (b);
                }
                const double sec = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
                logMessage ("SPACE CPU " + juce::String (SpaceReverb::algorithmName (a)).paddedRight (' ', 20) + juce::String (sec * 10.0, 2) + " % of one core");
                expect (sec < 1.0, "under 10 % of a core");
            }
        }
    }
};

static SpaceTests spaceTests;
