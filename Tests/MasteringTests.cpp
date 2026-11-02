#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include "../Source/Mastering/MasteringEngine.h"

// The mastering chain, module by module and as a whole: loudness to BS.1770 (EBU Tech 3341-style cases), true peak, the limiter's ceiling, width that
// cannot hurt mono, and the chain on quiet, loud, transient, bass-heavy, wide and mono material at 44.1, 48 and 96 kHz.
namespace
{
using namespace glitchkid::master;
using Buf = juce::AudioBuffer<float>;
constexpr double twoPi = juce::MathConstants<double>::twoPi;

Buf sine (double fs, double seconds, double hz, float dbfs, double phase = 0.0, int channels = 2)
{
    Buf b (channels, (int) (fs * seconds));
    const float a = juce::Decibels::decibelsToGain (dbfs);
    for (int i = 0; i < b.getNumSamples(); ++i)
        for (int ch = 0; ch < channels; ++ch)
            b.setSample (ch, i, a * (float) std::sin (twoPi * hz * i / fs + phase));
    return b;
}

void append (Buf& a, const Buf& b)
{
    Buf c (a.getNumChannels(), a.getNumSamples() + b.getNumSamples());
    for (int ch = 0; ch < c.getNumChannels(); ++ch)
    {
        c.copyFrom (ch, 0, a, ch, 0, a.getNumSamples());
        c.copyFrom (ch, a.getNumSamples(), b, ch, 0, b.getNumSamples());
    }
    a = c;
}

// A mix-like test signal: pink-ish noise (summed octave-spaced filtered noise), with options for transients, heavy bass and stereo width.
Buf mix (double fs, double seconds, float levelDb, bool transients, bool bass, float stereo, juce::int64 seed)
{
    juce::Random rng (seed);
    Buf b (2, (int) (fs * seconds));
    double lp1L = 0, lp2L = 0, lp1R = 0, lp2R = 0;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double c = rng.nextFloat() * 2.0 - 1.0, uL = rng.nextFloat() * 2.0 - 1.0, uR = rng.nextFloat() * 2.0 - 1.0;
        const double nl = (1.0 - stereo) * c + stereo * uL, nr = (1.0 - stereo) * c + stereo * uR;
        lp1L += 0.05 * (nl - lp1L); lp2L += 0.005 * (nl - lp2L);
        lp1R += 0.05 * (nr - lp1R); lp2R += 0.005 * (nr - lp2R);
        double l = 0.3 * nl + 1.5 * lp1L + 4.0 * lp2L, r = 0.3 * nr + 1.5 * lp1R + 4.0 * lp2R; // more energy low, like music
        if (bass) { const double s = 0.9 * std::sin (twoPi * 50.0 * i / fs); l += s; r += s; }
        if (transients && i % (int) (fs * 0.25) < (int) (fs * 0.002)) { const double k = (i % 2 ? 1.0 : -1.0) * 3.0; l += k; r += k; } // a 2 ms click every 250 ms
        b.setSample (0, i, (float) l);
        b.setSample (1, i, (float) r);
    }
    // scale to the requested sample peak
    const float pk = juce::jmax (b.getMagnitude (0, 0, b.getNumSamples()), b.getMagnitude (1, 0, b.getNumSamples()));
    b.applyGain (juce::Decibels::decibelsToGain (levelDb) / juce::jmax (1.0e-9f, pk));
    return b;
}

struct Reading // a copy of what the meter published
{
    float integ, st, mom, spk, tpk, corr; int clips;
    float integratedLufs() const { return integ; } float shortTermLufs() const { return st; } float momentaryLufs() const { return mom; }
    float samplePeakDb() const { return spk; } float truePeakDb() const { return tpk; } float correlation() const { return corr; } int clippedSamples() const { return clips; }
};

Reading measure (const Buf& b, double fs, int skip = 0)
{
    LoudnessMeter m;
    m.prepare (fs);
    m.process (b.getReadPointer (0) + skip, b.getNumChannels() > 1 ? b.getReadPointer (1) + skip : nullptr, b.getNumSamples() - skip);
    return { m.integratedLufs(), m.shortTermLufs(), m.momentaryLufs(), m.samplePeakDb(), m.truePeakDb(), m.correlation(), m.clippedSamples() };
}

Buf runEngine (MasteringEngine& e, const Buf& in, int block = 512)
{
    Buf out (in);
    for (int s = 0; s < out.getNumSamples(); s += block)
    {
        const int n = juce::jmin (block, out.getNumSamples() - s);
        Buf v (out.getArrayOfWritePointers(), out.getNumChannels(), s, n);
        e.process (v);
    }
    return out;
}

struct MasteringTests : public juce::UnitTest
{
    MasteringTests() : juce::UnitTest ("Mastering", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("loudness (BS.1770-4): 1 kHz sines at -23 and -33 dBFS read -23 and -33 LUFS at 44.1, 48 and 96 kHz");
        for (double fs : { 44100.0, 48000.0, 96000.0 })
            for (float level : { -23.0f, -33.0f })
            {
                const auto m = measure (sine (fs, 20.0, 1000.0, level), fs);
                expectWithinAbsoluteError (m.integratedLufs(), level, 0.1f, "integrated at " + juce::String (fs));
                expectWithinAbsoluteError (m.shortTermLufs(), level, 0.1f, "short-term at " + juce::String (fs));
                expectWithinAbsoluteError (m.momentaryLufs(), level, 0.1f, "momentary at " + juce::String (fs));
            }

        beginTest ("loudness gating: quiet and silent parts are gated out (EBU Tech 3341 cases 3 and 4)");
        {
            const double fs = 48000.0;
            Buf a = sine (fs, 10.0, 1000.0, -36.0f);
            append (a, sine (fs, 60.0, 1000.0, -23.0f));
            append (a, sine (fs, 10.0, 1000.0, -36.0f));
            expectWithinAbsoluteError (measure (a, fs).integratedLufs(), -23.0f, 0.1f, "case 3");
            Buf b = sine (fs, 10.0, 1000.0, -72.0f);
            append (b, sine (fs, 10.0, 1000.0, -36.0f));
            append (b, sine (fs, 60.0, 1000.0, -23.0f));
            append (b, sine (fs, 10.0, 1000.0, -36.0f));
            append (b, sine (fs, 10.0, 1000.0, -72.0f));
            expectWithinAbsoluteError (measure (b, fs).integratedLufs(), -23.0f, 0.1f, "case 4");
            Buf silence (2, 48000 * 5);
            silence.clear(); // a new AudioBuffer is not cleared
            expect (measure (silence, fs).integratedLufs() <= LoudnessMeter::silence, "silence has no loudness");
        }

        beginTest ("true peak: a quarter-sample-rate sine at 45 degrees reads its real peak, 3 dB above its samples; correlation and clipping are measured");
        {
            const double fs = 48000.0;
            const auto m = measure (sine (fs, 2.0, fs / 4.0, -6.0f, juce::MathConstants<double>::pi / 4.0), fs);
            expectWithinAbsoluteError (m.samplePeakDb(), -9.01f, 0.05f, "samples sit 3 dB below the waveform's peak");
            expectWithinAbsoluteError (m.truePeakDb(), -6.0f, 0.5f, "true peak " + juce::String (m.truePeakDb()));
            expectWithinAbsoluteError (measure (sine (fs, 2.0, 997.0, -6.0f), fs).truePeakDb(), -6.0f, 0.1f, "a low sine: true peak = sample peak");
            auto inv = sine (fs, 2.0, 500.0, -12.0f);
            inv.applyGain (1, 0, inv.getNumSamples(), -1.0f);
            expect (measure (sine (fs, 2.0, 500.0, -12.0f), fs).correlation() > 0.99f && measure (inv, fs).correlation() < -0.99f);
            expect (std::abs (measure (mix (fs, 3.0, -6.0f, false, false, 1.0f, 3), fs).correlation()) < 0.15f, "independent channels: about 0");
            expect (measure (sine (fs, 1.0, 100.0, 0.0f), fs).clippedSamples() > 100, "full-scale samples are counted");
        }

        beginTest ("limiter: below the ceiling it is an exact delay; driven hard it never exceeds the ceiling, in samples or between them");
        for (double fs : { 44100.0, 48000.0, 96000.0 })
        {
            MasterLimiter lim;
            lim.prepare (fs);
            lim.setSettings (0.0f, -1.0f, 100.0f);
            Buf quiet = sine (fs, 0.5, 440.0, -12.0f);
            Buf ref (quiet);
            lim.process (quiet.getWritePointer (0), quiet.getWritePointer (1), quiet.getNumSamples());
            const int d = lim.latencySamples();
            float err = 0.0f;
            for (int i = d; i < quiet.getNumSamples(); ++i)
                err = juce::jmax (err, std::abs (quiet.getSample (0, i) - ref.getSample (0, i - d)));
            expect (err < 1.0e-7f, "transparent below the ceiling, delayed by " + juce::String (d) + " samples");

            MasterLimiter hard;
            hard.prepare (fs);
            hard.setSettings (10.0f, -1.0f, 80.0f);
            Buf loud = mix (fs, 4.0, 0.0f, true, true, 0.3f, 7);
            append (loud, sine (fs, 1.0, fs * 0.24, -0.5f, 0.7)); // energy near a quarter of the rate: the hardest case for inter-sample peaks
            for (int s = 0; s < loud.getNumSamples(); s += 480)
            {
                const int n = juce::jmin (480, loud.getNumSamples() - s);
                hard.process (loud.getWritePointer (0) + s, loud.getWritePointer (1) + s, n);
            }
            const auto m = measure (loud, fs);
            expect (m.samplePeakDb() <= -1.0f + 0.001f, "sample peak " + juce::String (m.samplePeakDb()) + " at " + juce::String (fs));
            expect (m.truePeakDb() <= -1.0f + 0.2f, "true peak " + juce::String (m.truePeakDb()) + " dBTP at " + juce::String (fs));
            expect (m.clippedSamples() == 0);
        }

        beginTest ("width: unity is transparent, the mono sum never changes, mono stays mono, the low end is never widened, and it cannot push a mix out of phase");
        {
            const double fs = 48000.0;
            for (float w : { 0.0f, 0.5f, 1.0f, 1.5f, 2.0f })
            {
                StereoWidth sw;
                sw.prepare (fs);
                sw.setWidth (w);
                sw.reset();
                Buf b = mix (fs, 2.0, -6.0f, false, true, 0.6f, 11), ref (b);
                sw.process (b.getWritePointer (0), b.getWritePointer (1), b.getNumSamples());
                float sumErr = 0.0f, idErr = 0.0f;
                for (int i = 0; i < b.getNumSamples(); ++i)
                {
                    sumErr = juce::jmax (sumErr, std::abs ((b.getSample (0, i) + b.getSample (1, i)) - (ref.getSample (0, i) + ref.getSample (1, i))));
                    idErr = juce::jmax (idErr, std::abs (b.getSample (0, i) - ref.getSample (0, i)));
                }
                expect (sumErr < 1.0e-5f, "mono sum unchanged at width " + juce::String (w));
                if (std::abs (w - 1.0f) < 1e-6f) expect (idErr < 1.0e-5f, "unity is transparent");
                Buf mono = mix (fs, 1.0, -6.0f, false, false, 0.0f, 12);
                sw.reset();
                sw.process (mono.getWritePointer (0), mono.getWritePointer (1), mono.getNumSamples());
                float diff = 0.0f;
                for (int i = 0; i < mono.getNumSamples(); ++i) diff = juce::jmax (diff, std::abs (mono.getSample (0, i) - mono.getSample (1, i)));
                expect (diff < 1.0e-6f, "mono stays mono at width " + juce::String (w));
            }
            StereoWidth sw;
            sw.prepare (fs);
            sw.setWidth (2.0f);
            sw.reset();
            Buf wide = mix (fs, 4.0, -6.0f, false, false, 1.0f, 13); // independent channels: side as strong as mid
            sw.process (wide.getWritePointer (0), wide.getWritePointer (1), wide.getNumSamples());
            expect (measure (wide, fs, (int) fs).correlation() > -0.05f, "never pushed out of phase: " + juce::String (measure (wide, fs, (int) fs).correlation()));
            Buf bassSide (2, (int) fs * 2);
            for (int i = 0; i < bassSide.getNumSamples(); ++i) { const float v = 0.3f * (float) std::sin (twoPi * 40.0 * i / fs); bassSide.setSample (0, i, v + 0.3f); bassSide.setSample (1, i, -v + 0.3f); }
            Buf bs (bassSide);
            StereoWidth sb;
            sb.prepare (fs);
            sb.setWidth (2.0f);
            sb.reset();
            sb.process (bs.getWritePointer (0), bs.getWritePointer (1), bs.getNumSamples());
            expect (bs.getRMSLevel (0, (int) fs, (int) fs) <= bassSide.getRMSLevel (0, (int) fs, (int) fs) * 1.02f, "40 Hz side content is not widened");
        }

        beginTest ("the styles are real processing: they differ in tone, dynamics and level, and intensity 0 is close to a plain limiter");
        {
            Controls c;
            c.intensity = 1.0f;
            std::array<Chain, numStyles> ch;
            for (int s = 0; s < numStyles; ++s) { c.style = (Style) s; ch[(size_t) s] = resolve (c); }
            expect (ch[1].eq[0].gainDb > ch[0].eq[0].gainDb && ch[1].eq[3].gainDb < ch[0].eq[3].gainDb && ch[1].satDriveDb > 0.0f, "warm: more low, less top, saturation");
            expect (ch[2].comp.ratio > ch[0].comp.ratio && ch[2].driveDb > ch[0].driveDb, "loud: firmer glue, more drive");
            expect (ch[3].width > 1.0f && std::abs (ch[0].width - 1.0f) < 1e-6f, "wide: wider side");
            c.style = Style::cleanBalanced;
            c.intensity = 0.0f;
            const auto zero = resolve (c);
            for (auto& b : zero.eq) expect (std::abs (b.gainDb) < 1.0e-6f);
            expect (std::abs (zero.comp.ratio - 1.0f) < 1e-6f && zero.satDriveDb < 1e-6f && std::abs (zero.width - 1.0f) < 1e-6f, "intensity 0: only the limiter (and Loudness) remain");
            c.tone = 1.0f; c.intensity = 1.0f; c.style = Style::wideAtmospheric;
            expect (resolve (c).eq[3].gainDb <= 4.0f, "tone and style together stay within +4 dB on the top shelf");
            c.loudnessDb = 50.0f;
            expect (resolve (c).driveDb <= 12.0f && std::abs (resolve (c).ceilingDbTp + 1.0f) < 1e-6f && resolve (c).outputDb <= 0.0f, "unsafe values are clamped");
        }

        beginTest ("the chain on quiet, loud, transient, bass-heavy, wide and mono mixes, every style, 44.1 / 48 / 96 kHz: never over -1 dBTP, never broken");
        for (double fs : { 44100.0, 48000.0, 96000.0 })
            for (int s = 0; s < numStyles; ++s)
            {
                struct Case { const char* name; float level; bool tr, bass; float stereo; };
                for (auto [label, level, tr, bass, stereo] : { Case { "quiet", -24.0f, false, false, 0.4f }, Case { "loud", 0.0f, false, false, 0.4f }, Case { "transients", -3.0f, true, false, 0.3f },
                                                               Case { "bass", -2.0f, false, true, 0.2f }, Case { "wide", -6.0f, false, false, 1.0f }, Case { "mono", -6.0f, false, false, 0.0f } })
                {
                    MasteringEngine e;
                    e.prepare (fs, 512);
                    Controls c;
                    c.style = (Style) s;
                    c.intensity = 0.8f;
                    c.loudnessDb = 6.0f;
                    c.width = 1.5f;
                    e.setControls (c);
                    const Buf in = mix (fs, 6.0, level, tr, bass, stereo, 21 + s);
                    const Buf out = runEngine (e, in);
                    const auto m = measure (out, fs);
                    const juce::String what = juce::String (label) + ", " + styleName ((Style) s) + ", " + juce::String (fs);
                    bool finite = true;
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < out.getNumSamples(); ++i) finite = finite && std::isfinite (out.getSample (ch, i));
                    expect (finite, what);
                    expect (m.samplePeakDb() <= -1.0f + 0.001f && m.truePeakDb() <= -1.0f + 0.25f, what + ": peak " + juce::String (m.samplePeakDb()) + ", true peak " + juce::String (m.truePeakDb()));
                    expect (m.clippedSamples() == 0, what);
                    const auto mi = measure (in, fs);
                    if (stereo < 1e-6f) expect (m.correlation() > 0.999f, what + ": mono stays mono");
                    else expect (m.correlation() > juce::jmin (0.0f, mi.correlation()) - 0.05f, what + ": correlation " + juce::String (m.correlation()));
                    if (level <= -20.0f) expect (m.integratedLufs() > mi.integratedLufs() + 3.0f, what + ": a quiet mix comes up");
                    expect (e.limiterReductionDb() < 15.0f, what + ": limiting stays bounded");
                }
            }

        beginTest ("ORIGINAL: the untouched mix, time-aligned, at the master's loudness and never clipped");
        {
            const double fs = 48000.0;
            MasteringEngine e;
            e.prepare (fs, 512);
            Controls c;
            c.style = Style::loudPunchy;
            c.loudnessDb = 6.0f;
            e.setControls (c);
            const Buf in = mix (fs, 8.0, -12.0f, true, false, 0.4f, 31);
            const Buf master = runEngine (e, in);
            e.setCompareOriginal (true);
            const Buf orig = runEngine (e, in);
            const auto mm = measure (master, fs, (int) fs * 4), mo = measure (orig, fs, (int) fs * 4);
            expectWithinAbsoluteError (mo.shortTermLufs(), mm.shortTermLufs(), 1.0f, "loudness-matched: " + juce::String (mo.shortTermLufs()) + " vs " + juce::String (mm.shortTermLufs()));
            expect (mo.samplePeakDb() <= -1.0f + 0.001f, "the raised original is kept below clipping");
            // time-aligned: the original's waveform, delayed by the latency (scaled by the match gain)
            const int d = e.latencySamples();
            double num = 0, den1 = 0, den2 = 0;
            for (int i = (int) fs * 5; i < (int) fs * 6; ++i) { const double a = orig.getSample (0, i), b = in.getSample (0, i - d); num += a * b; den1 += a * a; den2 += b * b; }
            expect (num / std::sqrt (den1 * den2) > 0.99, "aligned with the input");
        }

        beginTest ("CPU: the whole chain at 48 and 96 kHz stays a small fraction of real time");
        for (double fs : { 48000.0, 96000.0 })
        {
            MasteringEngine e;
            e.prepare (fs, 512);
            Controls c;
            c.style = Style::warmSmooth; // saturation on, so the oversampler runs
            c.intensity = 1.0f;
            c.loudnessDb = 6.0f;
            e.setControls (c);
            const Buf in = mix (fs, 10.0, -6.0f, true, true, 0.4f, 51);
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            runEngine (e, in);
            const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
            logMessage ("  mastering chain at " + juce::String (fs / 1000.0, 1) + " kHz: " + juce::String (ms / 100.0, 2) + " % of one core (10 s of stereo in " + juce::String (ms, 0) + " ms)");
            expect (ms < 10000.0 * 0.25, "under 25 % of real time on this machine");
        }

        beginTest ("Loudness raises the master in steps, intensity 0 leaves the tone alone, and output trim only lowers");
        {
            const double fs = 48000.0;
            const Buf in = mix (fs, 6.0, -10.0f, false, false, 0.4f, 41);
            float last = -100.0f;
            for (float l : { 0.0f, 3.0f, 6.0f, 9.0f })
            {
                MasteringEngine e;
                e.prepare (fs, 512);
                Controls c;
                c.loudnessDb = l;
                e.setControls (c);
                const float lufs = measure (runEngine (e, in), fs).integratedLufs();
                expect (lufs > last + 0.5f, "loudness " + juce::String (l) + ": " + juce::String (lufs) + " LUFS");
                last = lufs;
            }
            MasteringEngine e;
            e.prepare (fs, 512);
            Controls c;
            c.intensity = 0.0f;
            c.loudnessDb = 0.0f;
            c.outputDb = -6.0f;
            e.setControls (c);
            const Buf quiet = mix (fs, 2.0, -20.0f, false, false, 0.4f, 42);
            const Buf out = runEngine (e, quiet);
            const int d = e.latencySamples();
            float err = 0.0f;
            for (int i = (int) fs; i < out.getNumSamples(); ++i)
                err = juce::jmax (err, std::abs (out.getSample (0, i) - 0.5012f * quiet.getSample (0, i - d)));
            expect (err < 1.0e-4f, "intensity 0, no drive, -6 dB trim: just the input, delayed and 6 dB down (" + juce::String (err) + ")");
        }
    }
};
static MasteringTests masteringTests;
}
