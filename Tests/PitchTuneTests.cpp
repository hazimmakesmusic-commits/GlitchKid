#include "TestHelpers.h"
#include "../Source/DSP/PitchCorrection/PitchCorrector.h"

// Stage 4.2: Pitch Tune inside the plug-in (parameters, latency, dry-path alignment, switching, robustness).
// The correction algorithm itself is tested in PitchCorrectionTests.cpp. Synthetic signals only.
namespace
{
using Vec = std::vector<float>;
constexpr double pi = 3.14159265358979323846;
constexpr float qNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float posInf = std::numeric_limits<float>::infinity();

// Vowel-like test voice: harmonics with 1/h amplitude, phase accumulated in double.
Vec voice (double sr, int n, double f0, float peak = 0.4f)
{
    Vec v ((size_t) n);
    double phase = 0.0;
    for (int i = 0; i < n; ++i)
    {
        phase += f0 / sr;
        phase -= std::floor (phase);
        double s = 0.0, norm = 0.0;
        for (int h = 1; h * f0 < 0.45 * sr && h <= 12; ++h)
        {
            s += std::sin (2.0 * pi * h * phase) / h;
            norm += 1.0 / h;
        }
        v[(size_t) i] = (float) (peak * s / norm * 1.6);
    }
    return v;
}

double cents (double hz, double ref) { return 1200.0 * std::log2 (hz / ref); }

// Normalised autocorrelation + parabolic peak: an estimate of a steady tone that does not use the plug-in's detector.
double autocorrFreq (const float* x, int n, double sr, double nearHz)
{
    const int lo = (int) (sr / (nearHz * 1.3)), hi = (int) (sr / (nearHz / 1.3));
    const int w = n - hi - 1;
    std::vector<double> r ((size_t) hi + 2, 0.0);
    for (int lag = lo - 1; lag <= hi + 1; ++lag)
    {
        double xy = 0, xx = 0, yy = 0;
        for (int i = 0; i < w; ++i)
        {
            xy += (double) x[i] * x[i + lag];
            xx += (double) x[i] * x[i];
            yy += (double) x[i + lag] * x[i + lag];
        }
        r[(size_t) lag] = xy / std::sqrt (xx * yy + 1.0e-30);
    }
    int best = lo;
    for (int lag = lo; lag <= hi; ++lag)
        if (r[(size_t) lag] > r[(size_t) best])
            best = lag;
    const double a = r[(size_t) best - 1], b = r[(size_t) best], c = r[(size_t) best + 1];
    const double den = a - 2 * b + c;
    return sr / (best + (den < 0 ? 0.5 * (a - c) / den : 0.0));
}

double rms (const float* x, int n)
{
    double s = 0;
    for (int i = 0; i < n; ++i)
        s += (double) x[i] * x[i];
    return std::sqrt (s / std::max (n, 1));
}

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

// Prepared processor with the given layout; parameters are set first so the state at prepare time is what the test wants.
struct Rig
{
    GlitchKidProcessor p;
    explicit Rig (double sr, int channels = 2, int block = 512)
    {
        if (channels == 1)
            setLayout (p, juce::AudioChannelSet::mono());
        sampleRate = sr;
        blockHint = block;
    }
    void set (const char* id, float v) { setParam (p, id, v); }
    void prepare() { p.prepareToPlay (sampleRate, blockHint); }
    double sampleRate;
    int blockHint;
};

void tuneOn (Rig& r, float strength = 100.0f, float speedMs = 0.0f, int key = 0, int scale = 0)
{
    r.set ("tune_on", 1.0f);
    r.set ("tune_strength", strength);
    r.set ("tune_speed", speedMs);
    r.set ("tune_key", (float) key);
    r.set ("tune_scale", (float) scale);
}

// The output equals `in` delayed by `delay` samples, bit for bit, from sample `from` on (channel 0).
bool exactlyDelayed (const Buf& in, const Buf& out, int delay, int from)
{
    for (int ch = 0; ch < in.getNumChannels(); ++ch)
        for (int i = from; i < in.getNumSamples(); ++i)
            if (! juce::exactlyEqual (out.getSample (ch, i), in.getSample (ch, i - delay)))
                return false;
    return true;
}

double outputFreq (const Buf& out, int ch, double sr, double nearHz, double fromSec, double lenSec)
{
    return autocorrFreq (out.getReadPointer (ch) + (int) (fromSec * sr), (int) (lenSec * sr), sr, nearHz);
}
} // namespace

struct PitchTuneTests : public juce::UnitTest
{
    PitchTuneTests() : juce::UnitTest ("Pitch Tune (plug-in)", "GlitchKid") {}

    void runTest() override
    {
        using glitchkid::pitch::PitchCorrector;

        beginTest ("Parameters exist, default to off/neutral, and the plug-in still reports 6 samples");
        {
            GlitchKidProcessor p;
            expectEquals (getParam (p, "tune_on"), 0.0f);
            expectEquals (getParam (p, "tune_key"), 0.0f);
            expectEquals (getParam (p, "tune_scale"), 0.0f);
            expectWithinAbsoluteError (getParam (p, "tune_strength"), 100.0f, 0.1f);
            expectWithinAbsoluteError (getParam (p, "tune_speed"), 20.0f, 0.5f);
            p.prepareToPlay (48000.0, 512);
            p.updateReportedLatency();
            expectEquals (p.getLatencySamples(), pluginLatency);
            expectEquals (p.getWantedLatencySamples(), pluginLatency);
            // Pitch Tune added five parameters (Formant Shift a sixth, tested in FormantTuneTests.cpp); none of the 36 earlier ones may have moved or gone.
            const char* oldIds[] = { "gain", "bypass", "mix", "sat_drive", "stutter_on", "echo_on", "echo_mix", "dim_on", "dim_mix",
                                     "eq_lo_freq", "eq_b1_q", "comp_thresh", "comp_makeup" };
            for (auto* id : oldIds)
                expect (getParam (p, id) > -998.0f, id);
            expectEquals (p.getParameters().size(), 46);
        }

        beginTest ("Off: the output is the input delayed by exactly 6 samples (no Pitch Tune cost, bit-exact)");
        for (double sr : { 44100.0, 48000.0 })
            for (int block : { 64, 480, 1000, 4096 })
            {
                Rig r (sr, 2, 512);
                r.prepare();
                const auto v = voice (sr, (int) sr, 233.0);
                const auto in = makeInput (v, &v);
                expect (exactlyDelayed (in, stream (r.p, in, block), pluginLatency, 64), "sr " + juce::String (sr) + " block " + juce::String (block));
            }

        beginTest ("On: the reported latency is 6 plus the Pitch Tune delay, and goes back to 6 when switched off");
        for (double sr : { 44100.0, 48000.0, 96000.0 })
        {
            Rig r (sr);
            r.prepare();
            const int pitchDelay = PitchCorrector::latencyFor (sr);
            expectEquals (r.p.getWantedLatencySamples(), pluginLatency);
            r.set ("tune_on", 1.0f);
            expectEquals (r.p.getWantedLatencySamples(), pluginLatency + pitchDelay);
            r.p.updateReportedLatency();
            expectEquals (r.p.getLatencySamples(), pluginLatency + pitchDelay);
            r.set ("tune_on", 0.0f);
            r.p.updateReportedLatency();
            expectEquals (r.p.getLatencySamples(), pluginLatency);
            logMessage ("  " + juce::String (sr) + " Hz: Pitch Tune adds " + juce::String (pitchDelay) + " samples = " + juce::String (pitchDelay * 1000.0 / sr, 2) + " ms");
        }

        beginTest ("On: a sharp note is pulled onto the note, in stereo and mono, at several block sizes and rates");
        for (double sr : { 44100.0, 48000.0 })
            for (int channels : { 2, 1 })
                for (int block : { 64, 480, 1000, 4096 })
                {
                    const double target = 220.0, sung = target * std::pow (2.0, 35.0 / 1200.0);
                    Rig r (sr, channels);
                    tuneOn (r);
                    r.prepare();
                    const auto v = voice (sr, (int) (1.3 * sr), sung);
                    const auto out = stream (r.p, makeInput (v, channels == 2 ? &v : nullptr), block);
                    expect (allFinite (out));
                    for (int ch = 0; ch < channels; ++ch)
                    {
                        const double err = cents (outputFreq (out, ch, sr, target, 0.9, 0.3), target);
                        expectLessThan (std::abs (err), 10.0);
                    }
                }

        beginTest ("On, strength 0: same pitch and level as the input, timing matches the reported latency");
        {
            // The shifter moves in whole pitch periods; it re-anchors at the start of each voiced stretch, so a steady
            // note keeps the reported latency. (A later change of period can still leave up to half a period of offset,
            // which for a held note is only a phase shift.)
            const double sr = 48000.0, hz = 223.8;
            Rig r (sr);
            tuneOn (r, 0.0f, 0.0f);
            r.prepare();
            const int total = r.p.getWantedLatencySamples();
            const auto v = voice (sr, (int) (1.2 * sr), hz);
            const auto in = makeInput (v, &v);
            const auto out = stream (r.p, in, 480);
            const int from = (int) (0.6 * sr), len = (int) (0.3 * sr);
            expectLessThan (std::abs (cents (outputFreq (out, 0, sr, hz, 0.7, 0.3), hz)), 3.0);
            const double level = rms (out.getReadPointer (0) + from, len) / rms (in.getReadPointer (0) + from, len);
            expect (level > 0.9 && level < 1.1, "level " + juce::String (level, 3));
            int bestLag = 0;
            double bestCorr = -1.0e30;
            const int half = (int) std::lround (sr / hz / 2.0);
            for (int lag = -2 * half; lag <= 2 * half; ++lag)
            {
                double c = 0.0;
                for (int i = from; i < from + len; ++i)
                    c += (double) out.getSample (0, i) * in.getSample (0, i - total + lag);
                if (c > bestCorr) { bestCorr = c; bestLag = lag; }
            }
            logMessage ("  strength 0: output timing is " + juce::String (bestLag) + " samples from the reported latency (half a period = " + juce::String (half) + ")");
            expect (std::abs (bestLag) <= 8, "timing within 8 samples (0.17 ms) of the reported latency");
        }

        beginTest ("On: Mix 0 and Bypass give the dry input delayed by the TOTAL latency, bit-exact");
        for (int mode = 0; mode < 2; ++mode)
        {
            const double sr = 48000.0;
            Rig r (sr);
            tuneOn (r);
            if (mode == 0) r.set ("mix", 0.0f); else r.set ("bypass", 1.0f);
            r.prepare();
            const int total = r.p.getWantedLatencySamples();
            const auto v = voice (sr, (int) (0.8 * sr), 250.0);
            const auto in = makeInput (v, &v);
            expect (exactlyDelayed (in, stream (r.p, in, 333), total, (int) (0.3 * sr)), mode == 0 ? "mix 0" : "bypass");
        }

        beginTest ("Switching on and off during playback: finite, bounded, no gap, and off is exact again");
        {
            const double sr = 48000.0;
            Rig r (sr);
            r.prepare();
            const auto v = voice (sr, (int) (3.0 * sr), 233.0);
            const auto in = makeInput (v, &v);
            Buf out;
            out.makeCopyOf (in);
            juce::MidiBuffer midi;
            const int block = 256;
            const int onAt = (int) (0.5 * sr), offAt = (int) (1.8 * sr);
            for (int start = 0; start < out.getNumSamples(); start += block)
            {
                if (start >= onAt && start - block < onAt) tuneOn (r, 100.0f, 20.0f);
                if (start >= offAt && start - block < offAt) r.set ("tune_on", 0.0f);
                Buf view (out.getArrayOfWritePointers(), 2, start, juce::jmin (block, out.getNumSamples() - start));
                r.p.processBlock (view, midi);
            }
            expect (allFinite (out));
            float peak = 0.0f;
            for (int i = 0; i < out.getNumSamples(); ++i)
                peak = std::max (peak, std::abs (out.getSample (0, i)));
            expectLessThan (peak, 0.4f * 1.6f * 1.5f);

            // No silent gap when it engages: shortly after the 20 ms crossfade, before the delay has elapsed, there is signal.
            const int total = glitchkid::pitch::PitchCorrector::latencyFor (sr);
            const int a = onAt + (int) (0.022 * sr), b = onAt + (int) (0.9 * total);
            const double level = rms (out.getReadPointer (0) + a, b - a), ref = rms (in.getReadPointer (0) + a, b - a);
            expect (level > 0.5 * ref, "level right after engaging " + juce::String (level / ref, 2) + " of the input");

            // Back off: exact 6-sample delay again.
            expect (exactlyDelayed (in, out, pluginLatency, offAt + (int) (0.1 * sr)), "off is bit-exact again");
        }

        beginTest ("Switching with Mix 0 (dry only): no click, no gap");
        {
            const double sr = 48000.0;
            Rig r (sr);
            r.set ("mix", 0.0f);
            r.prepare();
            const auto v = voice (sr, (int) (2.0 * sr), 150.0, 0.3f);
            const auto in = makeInput (v, &v);
            Buf out;
            out.makeCopyOf (in);
            juce::MidiBuffer midi;
            for (int start = 0; start < out.getNumSamples(); start += 512)
            {
                if (start == (int) (0.5 * sr) / 512 * 512) r.set ("tune_on", 1.0f);
                Buf view (out.getArrayOfWritePointers(), 2, start, juce::jmin (512, out.getNumSamples() - start));
                r.p.processBlock (view, midi);
            }
            expect (allFinite (out));
            float step = 0.0f, ownStep = 0.0f;
            for (int i = 1; i < in.getNumSamples(); ++i)
                ownStep = std::max (ownStep, std::abs (in.getSample (0, i) - in.getSample (0, i - 1)));
            for (int i = (int) (0.45 * sr); i < (int) (0.7 * sr); ++i)
                step = std::max (step, std::abs (out.getSample (0, i) - out.getSample (0, i - 1)));
            logMessage ("  dry-only switch: largest sample step " + juce::String (step, 4) + " vs the signal's own " + juce::String (ownStep, 4));
            expectLessThan (step, ownStep * 4.0f);
        }

        beginTest ("On: NaN, Inf and huge samples are treated as zeros (bit-exact) and the output stays finite");
        {
            const double sr = 48000.0;
            auto cleanV = voice (sr, (int) (1.0 * sr), 225.0);
            auto dirtyV = cleanV;
            for (int i = 20000; i < 20100; ++i)
            {
                dirtyV[(size_t) i] = (i % 3 == 0) ? qNaN : (i % 3 == 1 ? posInf : -posInf);
                cleanV[(size_t) i] = 0.0f;
            }
            dirtyV[30000] = 1.0e30f; cleanV[30000] = 0.0f;
            Rig a (sr), b (sr);
            tuneOn (a, 100.0f, 20.0f);
            tuneOn (b, 100.0f, 20.0f);
            a.prepare(); b.prepare();
            const auto oa = stream (a.p, makeInput (cleanV, &cleanV), 500), ob = stream (b.p, makeInput (dirtyV, &dirtyV), 500);
            expect (allFinite (ob));
            expectEquals (th::maxAbsDiff (oa, ob), 0.0f);
        }

        beginTest ("Switching on while NaN/Inf arrive during the crossfade: finite, and identical to the same audio with zeros");
        {
            const double sr = 48000.0;
            auto clean = voice (sr, (int) (1.0 * sr), 225.0);
            auto dirty = clean;
            const int onAt = 188 * 128; // a block boundary, so the switch really happens here
            for (int i = onAt + 100; i < onAt + 200; ++i)
            {
                dirty[(size_t) i] = (i % 2 == 0) ? qNaN : posInf;
                clean[(size_t) i] = 0.0f;
            }
            auto run = [&] (const Vec& v)
            {
                Rig r (sr);
                r.prepare();
                Buf out = makeInput (v, &v);
                juce::MidiBuffer midi;
                for (int start = 0; start < out.getNumSamples(); start += 128)
                {
                    if (start == onAt) tuneOn (r, 100.0f, 20.0f);
                    Buf view (out.getArrayOfWritePointers(), 2, start, juce::jmin (128, out.getNumSamples() - start));
                    r.p.processBlock (view, midi);
                }
                return out;
            };
            const auto a = run (clean), b = run (dirty);
            expect (allFinite (b));
            expectEquals (th::maxAbsDiff (a, b), 0.0f);
        }

        beginTest ("Stereo: identical channels give identical outputs; a very different second channel is not disturbed");
        {
            const double sr = 48000.0;
            Rig r (sr);
            tuneOn (r, 100.0f, 10.0f);
            r.prepare();
            const auto v = voice (sr, (int) (1.0 * sr), 227.0);
            const auto out = stream (r.p, makeInput (v, &v), 512);
            bool same = true;
            for (int i = 0; i < out.getNumSamples(); ++i)
                same &= juce::exactlyEqual (out.getSample (0, i), out.getSample (1, i));
            expect (same, "L == R in, L == R out");
        }

        beginTest ("The read-out follows the voice: sung note and target note");
        {
            const double sr = 48000.0;
            Rig r (sr);
            tuneOn (r, 100.0f, 0.0f);
            r.prepare();
            const auto v = voice (sr, (int) (0.8 * sr), 220.0 * std::pow (2.0, 35.0 / 1200.0));
            stream (r.p, makeInput (v, &v), 512);
            expectWithinAbsoluteError (r.p.getTuneDetectedMidi(), 57.35f, 0.08f);
            expectEquals (r.p.getTuneTargetMidi(), 57.0f);
            Rig off (sr);
            off.prepare();
            stream (off.p, makeInput (v, &v), 512);
            expect (off.p.getTuneDetectedMidi() < 0.0f && off.p.getTuneTargetMidi() < 0.0f, "no read-out while off");
        }

        beginTest ("Key and scale are honoured: a note between two scale notes goes to the nearer one");
        {
            const double sr = 48000.0;
            // A voice at MIDI 58.3 (a little sharp of A#3), tuned with different keys and scales.
            const double sungHz = 440.0 * std::pow (2.0, (58.3 - 69.0) / 12.0); // MIDI 58.3
            const struct { int key, scale; double expected; } cases[] = {
                { 0, 0, 58.0 },  // C chromatic
                { 0, 1, 59.0 },  // C major: A# is not in the scale; 58.3 is 1.3 from A(57) and 0.7 from B(59)
                { 9, 2, 59.0 },  // A natural minor: A B C D E F G -> B(59)
                { 3, 4, 58.0 } } ; // D# major pentatonic: D# F G A# C -> A#(58) is in the scale
            for (const auto& c : cases)
            {
                Rig r (sr);
                tuneOn (r, 100.0f, 0.0f, c.key, c.scale);
                r.prepare();
                const auto v = voice (sr, (int) (0.8 * sr), sungHz);
                stream (r.p, makeInput (v, &v), 512);
                expectEquals (r.p.getTuneTargetMidi(), (float) c.expected, "key " + juce::String (c.key) + " scale " + juce::String (c.scale));
            }
        }

        beginTest ("Alter Egos never touch the Pitch Tune controls, and the state round-trips (older state loads with Tune off)");
        {
            GlitchKidProcessor p;
            th::setParam (p, "tune_on", 1.0f);
            th::setParam (p, "tune_key", 5.0f);
            th::setParam (p, "tune_scale", 2.0f);
            th::setParam (p, "tune_strength", 63.0f);
            th::setParam (p, "tune_speed", 77.0f);
            for (int ego = 0; ego < AlterEgos::count; ++ego)
            {
                p.applyAlterEgo (ego, 1.0f, (std::uint32_t) ego + 1u);
                expectEquals (getParam (p, "tune_on"), 1.0f);
                expectEquals (getParam (p, "tune_key"), 5.0f);
                expectEquals (getParam (p, "tune_scale"), 2.0f);
                expectWithinAbsoluteError (getParam (p, "tune_strength"), 63.0f, 0.1f);
                expectWithinAbsoluteError (getParam (p, "tune_speed"), 77.0f, 0.5f);
            }
            juce::MemoryBlock state;
            p.getStateInformation (state);
            GlitchKidProcessor q;
            q.setStateInformation (state.getData(), (int) state.getSize());
            expectEquals (getParam (q, "tune_on"), 1.0f);
            expectEquals (getParam (q, "tune_key"), 5.0f);
            expectEquals (getParam (q, "tune_scale"), 2.0f);
            expectWithinAbsoluteError (getParam (q, "tune_strength"), 63.0f, 0.1f);
            expectWithinAbsoluteError (getParam (q, "tune_speed"), 77.0f, 0.5f);

            // A session saved before Pitch Tune existed has none of these parameters.
            auto xml = juce::parseXML (juce::String ("<GlitchKidState><PARAM id=\"gain\" value=\"-3\"/></GlitchKidState>"));
            juce::MemoryBlock old;
            juce::AudioProcessor::copyXmlToBinary (*xml, old);
            GlitchKidProcessor o;
            o.setStateInformation (old.getData(), (int) old.getSize());
            expectEquals (getParam (o, "tune_on"), 0.0f);
            expectWithinAbsoluteError (getParam (o, "gain"), -3.0f, 0.1f);
        }
    }
};

static PitchTuneTests pitchTuneTests;
