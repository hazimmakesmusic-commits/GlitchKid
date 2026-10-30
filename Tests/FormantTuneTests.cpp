#include "TestHelpers.h"
#include "VoiceSignals.h"
#include "../Source/DSP/FormantShifting/FormantShifter.h"
#include "../Source/DSP/PitchCorrection/PitchCorrector.h"
#include <atomic>

// Stage 4.4: Formant inside the plug-in (parameter, latency, dry-path alignment, switching, robustness, together with Pitch Tune).
// The algorithm itself is tested in FormantShiftTests.cpp. Synthetic signals only.
extern std::atomic<long> g_allocations; // counted in FormantShiftTests.cpp (it replaces the global operator new)

namespace
{
using vs::Vec;
constexpr float qNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float posInf = std::numeric_limits<float>::infinity();

Buf makeInput (const Vec& left, const Vec* right = nullptr)
{
    Buf b (right != nullptr ? 2 : 1, (int) left.size());
    b.copyFrom (0, 0, left.data(), (int) left.size());
    if (right != nullptr)
        b.copyFrom (1, 0, right->data(), (int) right->size());
    return b;
}

// Streams `in` through the processor in blocks of `block` samples (the last one may be shorter).
Buf stream (GlitchKidProcessor& p, const Buf& in, int block)
{
    Buf out;
    out.makeCopyOf (in);
    juce::MidiBuffer midi;
    for (int start = 0; start < out.getNumSamples(); start += block)
    {
        Buf view (out.getArrayOfWritePointers(), out.getNumChannels(), start, juce::jmin (block, out.getNumSamples() - start));
        p.processBlock (view, midi);
    }
    return out;
}

bool allFinite (const Buf& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (ch, i)))
                return false;
    return true;
}

// A prepared processor; parameters are set first so the state at prepare time is what the test wants.
struct Rig
{
    GlitchKidProcessor p;
    double sampleRate;
    explicit Rig (double sr, int channels = 2) : sampleRate (sr)
    {
        if (channels == 1)
            setLayout (p, juce::AudioChannelSet::mono());
    }
    void set (const char* id, float v) { setParam (p, id, v); }
    void prepare() { p.prepareToPlay (sampleRate, 512); }
};

void tuneOn (Rig& r, float strength = 100.0f, float speedMs = 0.0f)
{
    r.set ("tune_on", 1.0f);
    r.set ("tune_strength", strength);
    r.set ("tune_speed", speedMs);
}

// The output equals `in` delayed by `delay` samples, bit for bit, from sample `from` on.
bool exactlyDelayed (const Buf& in, const Buf& out, int delay, int from)
{
    for (int ch = 0; ch < in.getNumChannels(); ++ch)
        for (int i = from; i < in.getNumSamples(); ++i)
            if (! juce::exactlyEqual (out.getSample (ch, i), in.getSample (ch, i - delay)))
                return false;
    return true;
}

// The standalone shifter's output for the same input (per-sample, so block size does not matter).
Buf moduleOutput (const Buf& in, double sr, float semitones)
{
    glitchkid::formant::FormantShifter fs;
    fs.prepare (sr);
    fs.reset();                       // like the plug-in: starts neutral and glides to the value (30 ms), not already there
    fs.setShiftSemitones (semitones);
    Buf out;
    out.makeCopyOf (in);
    fs.process (out.getArrayOfWritePointers(), out.getNumChannels(), out.getNumSamples());
    return out;
}

double freqAt (const Buf& out, double sr, double nearHz, double fromSec, double lenSec, int ch = 0)
{
    return vs::autocorrFreq (out.getReadPointer (ch) + (int) (fromSec * sr), (int) (lenSec * sr), sr, nearHz);
}
} // namespace

struct FormantTuneTests : public juce::UnitTest
{
    FormantTuneTests() : juce::UnitTest ("Formant (plug-in)", "GlitchKid") {}

    void runTest() override
    {
        using glitchkid::formant::FormantShifter;
        using glitchkid::pitch::PitchCorrector;
        const auto* vowels = vs::vowels();

        beginTest ("Parameter: -12..+12 semitones, default 0 (off), the plug-in still reports 6 samples");
        {
            GlitchKidProcessor p;
            expectEquals (getParam (p, "formant_shift"), 0.0f);
            auto* prm = dynamic_cast<juce::RangedAudioParameter*> (p.getParameters()[0]);
            for (auto* q : p.getParameters())
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (q); r && r->paramID == "formant_shift")
                    prm = r;
            expectEquals (prm->convertFrom0to1 (0.0f), -12.0f);
            expectEquals (prm->convertFrom0to1 (1.0f), 12.0f);
            expectEquals (prm->getNormalisableRange().interval, 0.1f);
            p.prepareToPlay (48000.0, 512);
            p.updateReportedLatency();
            expectEquals (p.getLatencySamples(), pluginLatency);
        }

        beginTest ("Off: with Formant at 0 the output is the input delayed by exactly 6 samples (bit-exact), also after it was used");
        for (double sr : { 44100.0, 48000.0, 96000.0 })
            for (int block : { 64, 480, 4096 })
            {
                Rig r (sr);
                r.prepare();
                const auto v = vs::vowel (vowels[0], sr, (int) (1.2 * sr), 140.0);
                const auto in = makeInput (v, &v);
                expect (exactlyDelayed (in, stream (r.p, in, block), pluginLatency, 64), "fresh, sr " + juce::String (sr) + " block " + juce::String (block));
                r.set ("formant_shift", 4.0f);
                stream (r.p, in, block);
                r.set ("formant_shift", 0.0f);
                stream (r.p, in, block); // lets the 20 ms crossfade and the shifter's own smoothing finish
                expect (exactlyDelayed (in, stream (r.p, in, block), pluginLatency, 64), "after use, sr " + juce::String (sr));
            }

        beginTest ("Latency: 6 + the Formant delay while away from 0, + the Pitch Tune delay if that is on too");
        for (double sr : { 44100.0, 48000.0, 96000.0 })
        {
            Rig r (sr);
            r.prepare();
            const int fLat = FormantShifter::latencyFor (sr), pLat = PitchCorrector::latencyFor (sr);
            expectEquals (r.p.getWantedLatencySamples(), pluginLatency);
            r.set ("formant_shift", -3.0f);
            expectEquals (r.p.getWantedLatencySamples(), pluginLatency + fLat);
            r.p.updateReportedLatency();
            expectEquals (r.p.getLatencySamples(), pluginLatency + fLat);
            r.set ("tune_on", 1.0f);
            expectEquals (r.p.getWantedLatencySamples(), pluginLatency + fLat + pLat);
            r.set ("formant_shift", 0.0f);
            expectEquals (r.p.getWantedLatencySamples(), pluginLatency + pLat);
            r.set ("tune_on", 0.0f);
            r.p.updateReportedLatency();
            expectEquals (r.p.getLatencySamples(), pluginLatency);
            logMessage ("  " + juce::String (sr) + " Hz: Formant adds " + juce::String (fLat) + " samples = " + juce::String (fLat * 1000.0 / sr, 2)
                        + " ms; with Pitch Tune as well " + juce::String ((fLat + pLat) * 1000.0 / sr, 1) + " ms");
        }

        beginTest ("Integration is exact: the plug-in output equals the standalone shifter's output delayed by 6 samples");
        for (double sr : { 44100.0, 48000.0 })
            for (int channels : { 2, 1 })
                for (float shift : { -8.0f, 5.0f })
                    for (int block : { 64, 480, 4096 })
                    {
                        const auto v = vs::vowel (vowels[2], sr, (int) (1.0 * sr), 150.0);
                        Vec w = v;
                        for (auto& s : w)
                            s *= 0.6f;
                        const auto in = makeInput (v, channels == 2 ? &w : nullptr);
                        Rig r (sr, channels);
                        r.set ("formant_shift", shift);
                        r.prepare();
                        const auto got = stream (r.p, in, block);
                        const auto want = moduleOutput (in, sr, shift);
                        bool same = true;
                        double maxDifference = 0.0;
                        int firstDifferent = -1;
                        for (int ch = 0; ch < channels; ++ch)
                            for (int i = 0; i < in.getNumSamples(); ++i)
                            {
                                const float expected = i >= pluginLatency ? want.getSample (ch, i - pluginLatency) : 0.0f;
                                const bool equal = juce::exactlyEqual (got.getSample (ch, i), expected);
                                same &= equal;
                                if (! equal)
                                {
                                    maxDifference = std::max (maxDifference, std::abs ((double) got.getSample (ch, i) - (double) expected));
                                    firstDifferent = firstDifferent < 0 ? i : firstDifferent;
                                }
                            }
                        expect (same, juce::String (channels) + " ch, " + juce::String (shift) + " st, " + juce::String (sr) + " Hz, block " + juce::String (block)
                                          + (same ? juce::String() : " (largest difference " + juce::String (maxDifference, 9) + ", first at sample " + juce::String (firstDifferent) + ")"));
                    }

        beginTest ("In the plug-in the notes stay and the formants move: vowels vs the same vowel with moved formants");
        for (float shift : { -4.0f, 4.0f })
            for (int vi = 0; vi < 3; ++vi)
            {
                const double sr = 48000.0, f0 = 130.0, ratio = std::pow (2.0, shift / 12.0);
                const int n = (int) sr;
                const auto in = vs::vowel (vowels[vi], sr, n, f0), ref = vs::vowel (vowels[vi], sr, n, f0, ratio);
                Rig r (sr);
                r.set ("formant_shift", shift);
                r.prepare();
                const auto out = stream (r.p, makeInput (in, &in), 480);
                expectLessThan (std::abs (vs::cents (freqAt (out, sr, f0, 0.6, 0.3), f0)), 3.0);
                const int total = r.p.getWantedLatencySamples();
                Vec mono (out.getReadPointer (0), out.getReadPointer (0) + n);
                const auto hOut = vs::harmonicDb (mono, (size_t) (24000 + total), f0, sr, 40);
                const auto hRef = vs::harmonicDb (ref, 24000, f0, sr, 40), hIn = vs::harmonicDb (in, 24000, f0, sr, 40);
                auto shapeError = [&] (const std::vector<double>& a)
                {
                    double top = -1e9, mean = 0, e = 0;
                    int m = 0;
                    for (double d : hRef) top = std::max (top, d);
                    for (size_t i = 0; i < a.size(); ++i)
                        if (hRef[i] > top - 40.0) { mean += a[i] - hRef[i]; ++m; }
                    mean /= std::max (m, 1);
                    for (size_t i = 0; i < a.size(); ++i)
                        if (hRef[i] > top - 40.0) e += std::abs (a[i] - hRef[i] - mean);
                    return e / std::max (m, 1);
                };
                const double eOut = shapeError (hOut), eIn = shapeError (hIn);
                expectLessThan (eOut, 3.5);
                expectLessThan (eOut, eIn);
            }

        beginTest ("Mix 0 and Bypass give the dry input delayed by the TOTAL latency (Formant alone, and with Pitch Tune), bit-exact");
        for (int withTune = 0; withTune < 2; ++withTune)
            for (int mode = 0; mode < 2; ++mode)
            {
                const double sr = 48000.0;
                Rig r (sr);
                r.set ("formant_shift", 3.0f);
                if (withTune != 0)
                    tuneOn (r);
                if (mode == 0) r.set ("mix", 0.0f); else r.set ("bypass", 1.0f);
                r.prepare();
                const int total = r.p.getWantedLatencySamples();
                expectEquals (total, pluginLatency + FormantShifter::latencyFor (sr) + (withTune != 0 ? PitchCorrector::latencyFor (sr) : 0));
                const auto v = vs::vowel (vowels[0], sr, (int) (0.9 * sr), 180.0);
                const auto in = makeInput (v, &v);
                expect (exactlyDelayed (in, stream (r.p, in, 333), total, (int) (0.4 * sr)),
                        juce::String (withTune != 0 ? "tune + formant, " : "formant, ") + (mode == 0 ? "mix 0" : "bypass"));
            }

        beginTest ("Moving Formant off and on during playback: finite, bounded, no dip on engage, and 0 is exact again");
        {
            const double sr = 48000.0;
            Rig r (sr);
            r.prepare();
            const auto v = vs::vowel (vowels[0], sr, (int) (3.0 * sr), 140.0);
            const auto in = makeInput (v, &v);
            Buf out;
            out.makeCopyOf (in);
            juce::MidiBuffer midi;
            const int block = 256, onAt = (int) (0.5 * sr) / block * block, offAt = (int) (1.8 * sr) / block * block;
            for (int start = 0; start < out.getNumSamples(); start += block)
            {
                if (start == onAt) r.set ("formant_shift", 5.0f);
                if (start == offAt) r.set ("formant_shift", 0.0f);
                Buf view (out.getArrayOfWritePointers(), 2, start, juce::jmin (block, out.getNumSamples() - start));
                r.p.processBlock (view, midi);
            }
            expect (allFinite (out));
            float peak = 0.0f;
            for (int i = 0; i < out.getNumSamples(); ++i)
                peak = std::max (peak, std::abs (out.getSample (0, i)));
            expectLessThan (peak, 0.3f * 1.6f);

            // no silent gap while it engages: every 5 ms stretch in the 80 ms after the switch carries real signal
            const double ref = vs::rms (in.getReadPointer (0) + 20000, 9600);
            double lowest = 1e9;
            for (int s = onAt; s < onAt + (int) (0.08 * sr); s += 240)
                lowest = std::min (lowest, vs::rms (out.getReadPointer (0) + s, 240) / ref);
            expect (lowest > 0.35, "lowest 5 ms level after engaging: " + juce::String (lowest, 2) + " of the input");

            expect (exactlyDelayed (in, out, pluginLatency, offAt + (int) (0.15 * sr)), "0 is bit-exact again");
        }

        beginTest ("NaN, Inf and huge samples are treated as zeros (bit-exact), always on and while the switch crossfades");
        {
            const double sr = 48000.0;
            auto clean = vs::vowel (vowels[1], sr, (int) (1.0 * sr), 170.0);
            auto dirty = clean;
            for (int i = 20000; i < 20100; ++i)
            {
                dirty[(size_t) i] = (i % 3 == 0) ? qNaN : (i % 3 == 1 ? posInf : -posInf);
                clean[(size_t) i] = 0.0f;
            }
            dirty[30000] = 1.0e30f; clean[30000] = 0.0f;
            auto run = [&] (const Vec& v, bool switchedOn)
            {
                Rig r (sr);
                if (! switchedOn)
                    r.set ("formant_shift", -5.0f);
                r.prepare();
                Buf out = makeInput (v, &v);
                juce::MidiBuffer midi;
                const int switchAt = 188 * 128; // a block boundary
                for (int start = 0; start < out.getNumSamples(); start += 128)
                {
                    if (switchedOn && start == switchAt) r.set ("formant_shift", -5.0f);
                    Buf view (out.getArrayOfWritePointers(), 2, start, juce::jmin (128, out.getNumSamples() - start));
                    r.p.processBlock (view, midi);
                }
                return out;
            };
            for (bool switched : { false, true })
            {
                // for the crossfade case the bad samples must land inside the 20 ms ramp after the switch at sample 24064
                auto c = clean, d = dirty;
                if (switched)
                {
                    for (int i = 20000; i < 20100; ++i) { d[(size_t) i] = clean[(size_t) i]; }
                    for (int i = 24064 + 100; i < 24064 + 200; ++i)
                    {
                        d[(size_t) i] = (i % 2 == 0) ? qNaN : posInf;
                        c[(size_t) i] = 0.0f;
                    }
                }
                const auto a = run (c, switched), b = run (d, switched);
                expect (allFinite (b), switched ? "finite (switching)" : "finite (always on)");
                expectEquals (maxAbsDiff (a, b), 0.0f);
            }
        }

        beginTest ("Formant and Pitch Tune together: the sharp note is pulled onto the note AND the formants move");
        {
            const double sr = 44100.0, target = 220.0, sung = target * std::pow (2.0, 35.0 / 1200.0);
            const int n = (int) (1.8 * sr);
            const auto in = vs::vowel (vowels[0], sr, n, sung);
            auto run = [&] (float shift)
            {
                Rig r (sr);
                tuneOn (r);
                r.set ("formant_shift", shift);
                r.prepare();
                return stream (r.p, makeInput (in, &in), 512);
            };
            const auto both = run (3.0f), tuneOnly = run (0.0f);
            expectLessThan (std::abs (vs::cents (freqAt (both, sr, target, 1.0, 0.3), target)), 10.0);
            expectLessThan (std::abs (vs::cents (freqAt (tuneOnly, sr, target, 1.0, 0.3), target)), 10.0);
            auto levels = [&] (const Buf& b, int total)
            {
                Vec mono (b.getReadPointer (0), b.getReadPointer (0) + n);
                return vs::harmonicDb (mono, (size_t) (40000 + total), target, sr, 20);
            };
            const int tF = FormantShifter::latencyFor (sr), tP = PitchCorrector::latencyFor (sr);
            const double cBoth = vs::centroid (levels (both, pluginLatency + tF + tP), target), cTune = vs::centroid (levels (tuneOnly, pluginLatency + tP), target);
            expect (cBoth > cTune, "the spectral centroid moves up with Formant +3 on top of Pitch Tune: " + juce::String (cTune, 0) + " -> " + juce::String (cBoth, 0) + " Hz");
        }

        beginTest ("Alter Egos never touch Formant; the state round-trips; an older state loads with Formant at 0");
        {
            GlitchKidProcessor p;
            th::setParam (p, "formant_shift", 4.5f);
            for (int ego = 0; ego < AlterEgos::count; ++ego)
            {
                p.applyAlterEgo (ego, 1.0f, (std::uint32_t) ego + 1u);
                expectWithinAbsoluteError (getParam (p, "formant_shift"), 4.5f, 0.05f);
            }
            juce::MemoryBlock state;
            p.getStateInformation (state);
            GlitchKidProcessor q;
            q.setStateInformation (state.getData(), (int) state.getSize());
            expectWithinAbsoluteError (getParam (q, "formant_shift"), 4.5f, 0.05f);
            auto xml = juce::parseXML (juce::String ("<GlitchKidState><PARAM id=\"gain\" value=\"-3\"/></GlitchKidState>"));
            juce::MemoryBlock old;
            juce::AudioProcessor::copyXmlToBinary (*xml, old);
            GlitchKidProcessor o;
            o.setStateInformation (old.getData(), (int) old.getSize());
            expectEquals (getParam (o, "formant_shift"), 0.0f);
        }

        beginTest ("Real-time safety: the whole plug-in allocates nothing per block with Formant and Pitch Tune on");
        {
            const double sr = 48000.0;
            Rig r (sr);
            tuneOn (r);
            r.set ("formant_shift", -4.0f);
            r.prepare();
            const auto v = vs::vowel (vowels[0], sr, (int) (1.5 * sr), 160.0);
            const auto in = makeInput (v, &v);
            std::vector<Buf> blocks;
            for (int start = 0; start + 480 <= in.getNumSamples(); start += 480)
            {
                Buf b (2, 480);
                for (int ch = 0; ch < 2; ++ch)
                    b.copyFrom (ch, 0, in, ch, start, 480);
                blocks.push_back (std::move (b));
            }
            juce::MidiBuffer midi;
            for (size_t i = 0; i < 20; ++i)
                r.p.processBlock (blocks[i], midi); // warm-up
            const long before = g_allocations.load();
            for (size_t i = 20; i < blocks.size(); ++i)
                r.p.processBlock (blocks[i], midi);
            expectEquals (g_allocations.load() - before, 0L);
        }
    }
};

static FormantTuneTests formantTuneTests;
