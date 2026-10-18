#include "TestHelpers.h"
#include "../Source/Analysis/VocalAnalyzer.h"
#include "../Source/AutoMode/AutoMix.h"
#include "../Source/Presets/PresetController.h"

// AutoMode Stage 1: the vocal analyzer and the rules engine. These tests check measurements on synthetic signals, safety and determinism.
// They do NOT show that the resulting sound is good; that needs listening.
extern std::atomic<long> g_allocations; // counted in FormantShiftTests.cpp

namespace
{
using glitchkid::analysis::VocalAnalysis;
using glitchkid::analysis::VocalAnalyzer;
using glitchkid::automode::Profile;
using S = VocalAnalysis::Status;
constexpr float pi = 3.14159265f;

// Vocal-like test signal: 220 Hz fundamental with 16 harmonics (1/h), syllables of 0.3 s (alternating loud and soft) with 0.1 s gaps.
std::vector<float> vocal (double fs, double seconds, float loud = 0.3f, float soft = 0.1f, double f0 = 220.0)
{
    std::vector<float> x ((size_t) (fs * seconds));
    for (size_t n = 0; n < x.size(); ++n)
    {
        const double t = (double) n / fs;
        const int syl = (int) (t / 0.4);
        const double ph = t - syl * 0.4;
        if (ph > 0.3) { x[n] = 0.0f; continue; }
        const double env = std::sin (pi * ph / 0.3) * ((syl % 2) ? soft : loud);
        double s = 0.0;
        for (int h = 1; h <= 16; ++h)
            s += std::sin (2.0 * pi * f0 * h * t) / h;
        x[n] = (float) (env * s * 0.6);
    }
    return x;
}

std::vector<float> tone (double fs, double seconds, double f, float amp)
{
    std::vector<float> x ((size_t) (fs * seconds));
    for (size_t n = 0; n < x.size(); ++n)
        x[n] = amp * (float) std::sin (2.0 * pi * f * (double) n / fs);
    return x;
}

// Feeds `x` (mono, or the same on two channels) in blocks of `block` to a fresh analyzer after start(); stop() if it has not finished.
VocalAnalysis analyse (const std::vector<float>& x, double fs, int block = 480, int channels = 1, bool finishWithStop = true)
{
    VocalAnalyzer an;
    an.prepare (fs);
    an.start();
    for (size_t pos = 0; pos < x.size(); pos += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, x.size() - pos);
        const float* ch[2] = { x.data() + pos, x.data() + pos };
        an.process (ch, channels, n);
    }
    if (finishWithStop && an.isListening())
    {
        an.stop();
        const float* ch[2] = { x.data(), x.data() };
        an.process (ch, channels, 0);
    }
    return an.fetch();
}

bool finite (const VocalAnalysis& a)
{
    bool ok = true;
    for (float v : { a.totalSeconds, a.activeSeconds, a.peakDb, a.rmsDb, a.dynP10Db, a.dynP50Db, a.dynP90Db, a.dynamicRangeDb, a.sibilanceIndex })
        ok &= std::isfinite (v);
    for (float v : a.bandFraction)
        ok &= std::isfinite (v);
    return ok;
}

// A hand-made analysis for the rules tests.
VocalAnalysis ready()
{
    VocalAnalysis a;
    a.status = S::ready;
    a.activeSeconds = 6.0f; a.totalSeconds = 8.0f;
    a.peakDb = -6.0f; a.rmsDb = -20.0f;
    a.dynP10Db = -32.0f; a.dynP50Db = -22.0f; a.dynP90Db = -14.0f; a.dynamicRangeDb = 18.0f;
    a.bandFraction = { 0.10f, 0.45f, 0.25f, 0.12f, 0.08f };
    a.sibilanceIndex = 0.1f;
    return a;
}

glitchkid::automode::RangeLookup rangesOf (juce::AudioProcessor& p)
{
    return [&p] (const juce::String& id, glitchkid::automode::Range& out)
    {
        for (auto* q : p.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (q); r && r->paramID == id)
            {
                const auto range = r->getNormalisableRange();
                out = { range.start, range.end, [range] (float v) { return range.snapToLegalValue (v); } };
                return true;
            }
        return false;
    };
}

struct AutoModeTests : public juce::UnitTest
{
    AutoModeTests() : juce::UnitTest ("AutoMode", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("analyzer: silence and very low-level input are reported as unusable, with finite numbers");
        {
            for (double level : { 0.0, 1.0e-5 })
            {
                std::vector<float> x ((size_t) (48000.0 * 4.0));
                juce::Random rng (3);
                for (auto& v : x) v = (float) level * (rng.nextFloat() - 0.5f);
                const auto a = analyse (x, 48000.0);
                expect (a.status == S::tooQuiet, "level " + juce::String (level));
                expect (finite (a));
                expect (a.clippedSamples == 0 && a.nonFiniteSamples == 0);
            }
        }

        beginTest ("analyzer: a normal vocal-like signal is ready, with plausible peak, level, band shares and dynamics");
        {
            const auto a = analyse (vocal (48000.0, 8.0), 48000.0);
            expect (a.status == S::ready);
            expect (finite (a));
            expect (a.activeSeconds > 4.0f && a.activeSeconds <= a.totalSeconds + 0.2f);
            expect (a.peakDb < 0.0f && a.peakDb > -20.0f, "peak " + juce::String (a.peakDb));
            expect (a.rmsDb < a.peakDb && a.rmsDb > -50.0f, "rms " + juce::String (a.rmsDb));
            float sum = 0.0f;
            for (float f : a.bandFraction) { sum += f; expect (f >= 0.0f && f <= 1.0f); }
            expectWithinAbsoluteError (sum, 1.0f, 1.0e-3f);
            expect (a.bandFraction[1] + a.bandFraction[2] > 0.6f, "a 220 Hz voice puts most energy at 120 Hz - 2 kHz");
            expect (a.dynP10Db <= a.dynP50Db && a.dynP50Db <= a.dynP90Db);
            expect (a.dynamicRangeDb > 5.0f, "loud and soft syllables differ by ~9.5 dB: " + juce::String (a.dynamicRangeDb));
            expect (a.clippedSamples == 0);
        }

        beginTest ("analyzer: the bands separate low, presence and high content");
        {
            const auto low = analyse (tone (48000.0, 4.0, 80.0, 0.3f), 48000.0);
            const auto mid = analyse (tone (48000.0, 4.0, 1000.0, 0.3f), 48000.0);
            const auto pres = analyse (tone (48000.0, 4.0, 3000.0, 0.3f), 48000.0);
            const auto high = analyse (tone (48000.0, 4.0, 9000.0, 0.3f), 48000.0);
            // The band split is coarse (second-order filters leak into the neighbours): each tone must make ITS band the largest.
            auto largest = [] (const VocalAnalysis& a) { return (int) (std::max_element (a.bandFraction.begin(), a.bandFraction.end()) - a.bandFraction.begin()); };
            expectEquals (largest (low), 0);
            expect (low.bandFraction[0] > 0.5f, "80 Hz -> low band: " + juce::String (low.bandFraction[0]));
            expectEquals (largest (mid), 2);
            expectEquals (largest (pres), 3);
            expect (pres.bandFraction[3] > 0.4f, "3 kHz -> 2-5k band: " + juce::String (pres.bandFraction[3]));
            expectEquals (largest (high), 4);
            expect (high.bandFraction[4] > 0.8f, "9 kHz -> top band: " + juce::String (high.bandFraction[4]));
            expect (high.sibilanceIndex > 0.9f && low.sibilanceIndex < 0.1f);
        }

        beginTest ("analyzer: same answer for any block size, 44.1 / 48 / 96 kHz, mono or two channels");
        {
            const auto ref = analyse (vocal (48000.0, 8.0), 48000.0, 480);
            for (int block : { 1, 64, 4096 })
            {
                const auto a = analyse (vocal (48000.0, 8.0), 48000.0, block);
                expect (a.status == S::ready);
                expectWithinAbsoluteError (a.rmsDb, ref.rmsDb, 0.05f);
                expectWithinAbsoluteError (a.peakDb, ref.peakDb, 0.01f);
                for (int b = 0; b < VocalAnalysis::numBands; ++b)
                    expectWithinAbsoluteError (a.bandFraction[(size_t) b], ref.bandFraction[(size_t) b], 0.005f);
            }
            for (double fs : { 44100.0, 96000.0 })
            {
                const auto a = analyse (vocal (fs, 8.0), fs, 480, 2);
                expect (a.status == S::ready && finite (a));
                expectWithinAbsoluteError (a.rmsDb, ref.rmsDb, 1.0f);
                for (int b = 0; b < VocalAnalysis::numBands; ++b)
                    expectWithinAbsoluteError (a.bandFraction[(size_t) b], ref.bandFraction[(size_t) b], 0.03f);
            }
        }

        beginTest ("analyzer: clipping is counted and a short recording is reported as too short");
        {
            auto x = vocal (48000.0, 8.0, 0.9f, 0.5f);
            for (auto& v : x) v = juce::jlimit (-1.0f, 1.0f, v * 6.0f);
            const auto a = analyse (x, 48000.0);
            expect (a.status == S::ready);
            expect (a.clippedSamples > 100, "clipped " + juce::String (a.clippedSamples));
            expect (a.peakDb > -0.1f);

            const auto s = analyse (vocal (48000.0, 1.0), 48000.0);
            expect (s.status == S::tooShort, "1 s of vocal");
        }

        beginTest ("analyzer: NaN/Inf input is counted and ignored; a mostly invalid input is rejected");
        {
            auto x = vocal (48000.0, 8.0);
            for (size_t n = 100; n < x.size(); n += 997)
                x[n] = (n % 2) ? std::numeric_limits<float>::quiet_NaN() : std::numeric_limits<float>::infinity();
            const auto a = analyse (x, 48000.0);
            expect (a.status == S::ready && finite (a));
            expect (a.nonFiniteSamples > 100);

            auto bad = vocal (48000.0, 8.0);
            for (size_t n = 0; n < bad.size(); n += 10)
                bad[n] = std::numeric_limits<float>::quiet_NaN();
            const auto b = analyse (bad, 48000.0);
            expect (b.status == S::invalid && finite (b));
        }

        beginTest ("analyzer: does nothing until started, finishes by itself, and a new start discards the old result");
        {
            VocalAnalyzer an;
            an.prepare (48000.0);
            const auto x = vocal (48000.0, 20.0);
            const float* ch[1] = { x.data() };
            an.process (ch, 1, 4800);
            expect (an.fetch().status == S::idle && ! an.isListening());
            an.start();
            for (size_t pos = 0; pos + 480 <= x.size() && an.fetch().status != S::ready; pos += 480)
            {
                const float* c[1] = { x.data() + pos };
                an.process (c, 1, 480);
            }
            const auto a = an.fetch();
            expect (a.status == S::ready && ! an.isListening());
            expect (a.activeSeconds >= 8.0f && a.activeSeconds < 8.3f, "ends at 8 s of vocal: " + juce::String (a.activeSeconds));
            an.start();
            const float* c[1] = { x.data() };
            an.process (c, 1, 480);
            expect (an.isListening());
        }

        beginTest ("rules engine: deterministic, every value inside the parameter's range, only owned parameters touched");
        {
            GlitchKidProcessor p;
            const auto current = p.captureSoundState();
            const auto a = ready();
            const auto r1 = glitchkid::automode::compute (a, Profile::astroMode, 0.6f, current, rangesOf (p));
            const auto r2 = glitchkid::automode::compute (a, Profile::astroMode, 0.6f, current, rangesOf (p));
            expect (r1.usable && r1.changedCount > 10);
            expectEquals ((int) r1.sound.values.size(), (int) current.values.size());
            for (size_t i = 0; i < r1.sound.values.size(); ++i)
                expect (r1.sound.values[i].id == r2.sound.values[i].id && ! (r1.sound.values[i].value < r2.sound.values[i].value) && ! (r1.sound.values[i].value > r2.sound.values[i].value));

            glitchkid::automode::Range rg;
            for (const auto& e : r1.sound.values)
            {
                expect (rangesOf (p) (e.id, rg));
                expect (std::isfinite (e.value) && e.value >= rg.lo && e.value <= rg.hi, e.id);
            }
            for (const char* id : { "gain", "mix", "tune_key", "tune_scale", "formant_shift", "mut_on", "mut_amount", "mut_character", "mut_blend",
                                    "harm_on", "harm_interval", "harm_level", "stutter_on", "stutter_rate", "stutter_mix", "echo_time" })
                expectEquals (r1.sound.get (id, -1.0f), current.get (id, -2.0f), id);
            expect (r1.sound.egoId == current.egoId);
        }

        beginTest ("rules engine: inconclusive analysis, zero intensity or invalid numbers change nothing");
        {
            GlitchKidProcessor p;
            const auto current = p.captureSoundState();
            for (S st : { S::idle, S::listening, S::tooShort, S::tooQuiet, S::invalid })
            {
                auto a = ready();
                a.status = st;
                const auto r = glitchkid::automode::compute (a, Profile::astroMode, 0.8f, current, rangesOf (p));
                expect (! r.usable && r.changedCount == 0);
                for (size_t i = 0; i < current.values.size(); ++i)
                    expectEquals (r.sound.values[i].value, current.values[i].value);
            }
            expect (! glitchkid::automode::compute (ready(), Profile::astroMode, 0.0f, current, rangesOf (p)).usable);
            // A single missing metric no longer aborts: a typical value is assumed for it (and the compressor falls back to fixed gentle settings).
            auto nan = ready();
            nan.dynP50Db = std::numeric_limits<float>::quiet_NaN();
            const auto rn = glitchkid::automode::compute (nan, Profile::astroMode, 0.8f, current, rangesOf (p));
            expect (rn.usable && ! rn.usedFallback && rn.notes.size() > 0);
            expectEquals (rn.sound.get ("comp_thresh", 0.0f), -18.0f);
            expectEquals (rn.sound.get ("comp_makeup", 9.0f), 0.0f);
            auto nanBand = ready();
            nanBand.bandFraction[2] = std::numeric_limits<float>::infinity();
            const auto rb = glitchkid::automode::compute (nanBand, Profile::astroMode, 0.8f, current, rangesOf (p));
            const auto rtyp = glitchkid::automode::compute (ready(), Profile::astroMode, 0.8f, current, rangesOf (p));
            expect (rb.usable && rb.notes.size() > 0);
            expectWithinAbsoluteError (rb.sound.get ("eq_b2_gain", 0.0f), 0.5f * 0.8f, 0.01f); // typical balance: no tonal decision from the broken measurement
            (void) rtyp;
            const auto weird = glitchkid::automode::compute (ready(), Profile::astroMode, std::numeric_limits<float>::quiet_NaN(), current, rangesOf (p));
            expect (! weird.usable);
            const auto big = glitchkid::automode::compute (ready(), Profile::astroMode, 50.0f, current, rangesOf (p));
            expect (big.usable);
            expectEquals (big.sound.get ("tune_strength", 0.0f), 85.0f);
        }

        beginTest ("rules engine: each documented rule reacts the way it says");
        {
            GlitchKidProcessor p;
            const auto cur = p.captureSoundState();
            auto run = [&] (VocalAnalysis a, float i = 1.0f) { return glitchkid::automode::compute (a, Profile::astroMode, i, cur, rangesOf (p)).sound; };
            const auto base = run (ready());
            expect (base.get ("eq_lo_gain", 0.0f) > 1.0f && base.get ("eq_hi_gain", 0.0f) < -2.0f && base.get ("sat_drive", 0.0f) > 3.0f);

            auto boomy = ready(); boomy.bandFraction = { 0.40f, 0.30f, 0.15f, 0.10f, 0.05f };
            expectEquals (run (boomy).get ("eq_lo_gain", 9.0f), 0.0f);
            expect (run (boomy).get ("eq_b1_gain", 0.0f) < base.get ("eq_b1_gain", 0.0f));

            auto dull = ready(); dull.bandFraction = { 0.10f, 0.45f, 0.35f, 0.03f, 0.07f };
            expect (run (dull).get ("eq_b2_gain", 0.0f) > base.get ("eq_b2_gain", 0.0f));
            expect (run (dull).get ("echo_mix", 0.0f) < base.get ("echo_mix", 0.0f) && run (dull).get ("dbl_amount", 0.0f) < base.get ("dbl_amount", 0.0f));

            auto forward = ready(); forward.bandFraction = { 0.05f, 0.30f, 0.30f, 0.28f, 0.07f };
            expect (run (forward).get ("eq_b2_gain", 9.0f) <= 0.0f, "no boost where the 2-5 kHz share is already high");

            auto dark = ready(); dark.bandFraction = { 0.10f, 0.50f, 0.30f, 0.095f, 0.005f };
            expectEquals (run (dark).get ("eq_hi_gain", 9.0f), 0.0f);

            auto sib = ready(); sib.sibilanceIndex = 0.6f;
            expect (run (sib).get ("eq_hi_gain", 0.0f) < base.get ("eq_hi_gain", 0.0f));

            auto bright = ready(); bright.bandFraction = { 0.05f, 0.35f, 0.25f, 0.20f, 0.15f };
            expect (run (bright).get ("sat_drive", 9.0f) < base.get ("sat_drive", 0.0f));

            auto clipped = ready(); clipped.clippedSamples = 50;
            expectEquals (run (clipped).get ("sat_drive", 9.0f), 0.0f);
            auto hot = ready(); hot.peakDb = -0.1f;
            expectEquals (run (hot).get ("sat_drive", 9.0f), 0.0f);

            auto flat = ready(); flat.dynP10Db = -24.0f; flat.dynP90Db = -20.0f; flat.dynamicRangeDb = 4.0f;
            auto wide = ready(); wide.dynP10Db = -50.0f; wide.dynP90Db = -12.0f; wide.dynamicRangeDb = 38.0f;
            expect (run (flat).get ("comp_ratio", 0.0f) < run (wide).get ("comp_ratio", 0.0f));
            expect (run (wide).get ("comp_makeup", 0.0f) <= 6.0f);

            // intensity scales the amounts monotonically
            expect (run (ready(), 0.3f).get ("echo_mix", 0.0f) < run (ready(), 0.9f).get ("echo_mix", 0.0f));
            expect (run (ready(), 0.3f).get ("eq_hi_gain", 0.0f) > run (ready(), 0.9f).get ("eq_hi_gain", 0.0f));
            expectEquals (run (ready(), 0.1f).get ("tune_on", 1.0f), 0.0f); // very low intensity does not switch the creative effects on
        }


        beginTest ("analyzer: empty, null and silent input are handled (offline path too); results repeat exactly");
        {
            const float none[1] = { 0.0f };
            const auto e1 = VocalAnalyzer::analyzeOffline (nullptr, 0, 48000.0);
            const auto e2 = VocalAnalyzer::analyzeOffline (none, 0, 48000.0);
            const auto e3 = VocalAnalyzer::analyzeOffline (none, -5, 48000.0);
            const std::vector<float> zeros (48000 * 3, 0.0f);
            const auto e4 = VocalAnalyzer::analyzeOffline (zeros.data(), (int) zeros.size(), 48000.0);
            for (const auto& a : { e1, e2, e3, e4 })
                expect (a.status == S::tooQuiet && finite (a), "empty/silent input is 'too quiet', never an error value");
            const auto x = vocal (48000.0, 8.0);
            const auto a1 = VocalAnalyzer::analyzeOffline (x.data(), (int) x.size(), 48000.0);
            const auto a2 = VocalAnalyzer::analyzeOffline (x.data(), (int) x.size(), 48000.0);
            expect (a1.status == S::ready);
            expect (std::memcmp (&a1, &a2, sizeof (VocalAnalysis)) == 0, "identical input -> bit-identical analysis");
            const auto live = analyse (x, 48000.0, 480);
            expectWithinAbsoluteError (live.rmsDb, a1.rmsDb, 0.05f);
            expectWithinAbsoluteError (live.crestDb, a1.crestDb, 0.05f);
        }

        beginTest ("analyzer: a steady tone has a ~3 dB crest factor and almost no level variation");
        {
            const auto a = analyse (tone (48000.0, 6.0, 1000.0, 0.3f), 48000.0);
            expect (a.status == S::ready);
            expectWithinAbsoluteError (a.crestDb, 3.0f, 0.4f);
            expect (a.dynamicRangeDb <= 2.0f, "steady tone: " + juce::String (a.dynamicRangeDb));
            expect (a.nearClippedSamples == 0 && a.clippedSamples == 0);
            const auto v = analyse (vocal (48000.0, 8.0), 48000.0);
            expect (v.crestDb > a.crestDb + 3.0f, "a vocal-like signal is far peakier than a sine: " + juce::String (v.crestDb));
        }

        beginTest ("analyzer: quiet, near-clipping, bright and widely varying signals are told apart");
        {
            const auto base = vocal (48000.0, 8.0);
            const auto ref = analyse (base, 48000.0);

            auto quietSig = base;
            const float qs = std::pow (10.0f, (-45.0f - ref.rmsDb) / 20.0f);
            for (auto& v : quietSig) v *= qs;
            const auto q = analyse (quietSig, 48000.0);
            expect (q.status == S::ready && q.rmsDb < -40.0f && q.rmsDb > -50.0f, "quiet vocal level " + juce::String (q.rmsDb));

            auto hotSig = base;
            float pk = 0.0f;
            for (float v : hotSig) pk = std::max (pk, std::fabs (v));
            for (auto& v : hotSig) v = juce::jlimit (-0.985f, 0.985f, v * (1.5f / pk)); // pressed against 0.985, below the 0.999 clip level
            const auto h = analyse (hotSig, 48000.0);
            expect (h.nearClippedSamples > 50 && h.clippedSamples == 0, "near " + juce::String (h.nearClippedSamples) + " hard " + juce::String (h.clippedSamples));
            expect (h.peakDb > -0.3f);

            auto bright = base;
            const auto hf = tone (48000.0, 8.0, 7000.0, 0.08f);
            for (size_t n = 0; n < bright.size(); ++n) bright[n] += hf[n];
            const auto b = analyse (bright, 48000.0);
            expect (b.bandFraction[4] > ref.bandFraction[4] + 0.1f, "top share " + juce::String (ref.bandFraction[4]) + " -> " + juce::String (b.bandFraction[4]));

            const auto steady = analyse (vocal (48000.0, 8.0, 0.3f, 0.27f), 48000.0);
            const auto varied = analyse (vocal (48000.0, 8.0, 0.3f, 0.02f), 48000.0);
            expect (varied.dynamicRangeDb > steady.dynamicRangeDb + 6.0f, "dynamic range " + juce::String (steady.dynamicRangeDb) + " vs " + juce::String (varied.dynamicRangeDb));

            // and the engine reacts the right way to each real measurement
            GlitchKidProcessor p;
            const auto cur = p.captureSoundState();
            auto prop = [&] (const VocalAnalysis& a) { return glitchkid::automode::compute (a, Profile::astroMode, 1.0f, cur, rangesOf (p)).sound; };
            expect (prop (h).get ("sat_drive", 9.0f) < 0.05f, "near-clipping take gets no saturation");
            expect (prop (ref).get ("sat_drive", 0.0f) > 3.0f);
            expect (prop (b).get ("sat_drive", 9.0f) <= prop (ref).get ("sat_drive", 0.0f));
            expect (prop (q).get ("comp_ratio", 9.0f) <= 2.0f + 1.0e-3f && prop (q).get ("comp_makeup", 9.0f) < 0.05f, "quiet take: gentle compression");
            expect (prop (varied).get ("comp_ratio", 0.0f) >= prop (steady).get ("comp_ratio", 9.0f), "more level variation: not less compression");
        }

        beginTest ("rules engine: very quiet and already-compressed takes are never squashed further");
        {
            GlitchKidProcessor p;
            const auto cur = p.captureSoundState();
            auto quiet = ready(); quiet.rmsDb = -48.0f; quiet.dynP50Db = -50.0f; quiet.dynP90Db = -44.0f; quiet.dynP10Db = -60.0f; quiet.dynamicRangeDb = 16.0f;
            const auto rq = glitchkid::automode::compute (quiet, Profile::astroMode, 1.0f, cur, rangesOf (p));
            expect (rq.sound.get ("comp_ratio", 9.0f) <= 2.001f);
            expectEquals (rq.sound.get ("comp_makeup", 9.0f), 0.0f);
            expect (rq.sound.get ("comp_thresh", 0.0f) >= -40.0f, "threshold stays inside the safe window");

            auto squashed = ready(); squashed.dynamicRangeDb = 4.0f; squashed.crestDb = 7.0f; squashed.dynP10Db = -22.0f; squashed.dynP90Db = -18.0f;
            const auto rs = glitchkid::automode::compute (squashed, Profile::astroMode, 1.0f, cur, rangesOf (p));
            expectWithinAbsoluteError (rs.sound.get ("comp_ratio", 9.0f), 1.5f, 0.11f);
            expectEquals (rs.sound.get ("comp_makeup", 9.0f), 0.0f);
            bool said = false;
            for (const auto& n : rs.notes) said |= n.contains ("already looks compressed");
            expect (said, "the reason is reported");

            auto lowDrOnly = ready(); lowDrOnly.dynamicRangeDb = 4.0f; lowDrOnly.crestDb = 16.0f; // small range but peaky: not "already compressed"
            expect (glitchkid::automode::compute (lowDrOnly, Profile::astroMode, 1.0f, cur, rangesOf (p)).sound.get ("comp_ratio", 0.0f) > 1.9f);

            auto nearClip = ready(); nearClip.nearClippedSamples = 40;
            expectEquals (glitchkid::automode::compute (nearClip, Profile::astroMode, 1.0f, cur, rangesOf (p)).sound.get ("sat_drive", 9.0f), 0.0f);
        }

        beginTest ("fallback: with allowFallback an unusable analysis gives valid ASTRO MODE defaults and says so; without it nothing changes");
        {
            GlitchKidProcessor p;
            const auto cur = p.captureSoundState();
            for (S st : { S::idle, S::listening, S::tooShort, S::tooQuiet, S::invalid })
            {
                auto a = ready();
                a.status = st;
                const auto off = glitchkid::automode::compute (a, Profile::astroMode, 0.7f, cur, rangesOf (p));
                expect (! off.usable && off.changedCount == 0);
                const auto on = glitchkid::automode::compute (a, Profile::astroMode, 0.7f, cur, rangesOf (p), true);
                expect (on.usable && on.usedFallback && on.changedCount > 5 && on.notes.size() > 0);
                expect (on.notes[0].contains ("typical vocal"));
                expectEquals (on.sound.get ("comp_thresh", 0.0f), -18.0f);
                expectEquals (on.sound.get ("comp_makeup", 9.0f), 0.0f);
                glitchkid::automode::Range rg;
                for (const auto& e : on.sound.values)
                    if (rangesOf (p) (e.id, rg))
                        expect (std::isfinite (e.value) && e.value >= rg.lo && e.value <= rg.hi, e.id);
            }
            const auto zero = glitchkid::automode::compute (ready(), Profile::astroMode, 0.0f, cur, rangesOf (p), true);
            expect (! zero.usable, "intensity 0 still changes nothing, even with fallback");
        }

        beginTest ("ASTRO MODE gives a valid, in-range proposal for every supported input condition (real analyses of synthetic signals)");
        {
            GlitchKidProcessor p;
            const auto cur = p.captureSoundState();
            std::vector<std::pair<juce::String, std::vector<float>>> cases;
            cases.push_back ({ "silence", std::vector<float> (48000 * 4, 0.0f) });
            cases.push_back ({ "tone", tone (48000.0, 6.0, 1000.0, 0.3f) });
            cases.push_back ({ "vocal", vocal (48000.0, 8.0) });
            cases.push_back ({ "too short", vocal (48000.0, 1.0) });
            auto loud = vocal (48000.0, 8.0, 0.9f, 0.6f);
            for (auto& v : loud) v = juce::jlimit (-1.0f, 1.0f, v * 8.0f);
            cases.push_back ({ "clipped", loud });
            auto dirty = vocal (48000.0, 8.0);
            for (size_t n = 50; n < dirty.size(); n += 311) dirty[n] = std::numeric_limits<float>::infinity();
            cases.push_back ({ "non-finite", dirty });
            auto noise = std::vector<float> (48000 * 6);
            juce::Random rng (5);
            for (auto& v : noise) v = 0.4f * (rng.nextFloat() - 0.5f);
            cases.push_back ({ "noise", noise });
            for (const auto& c : cases)
                for (float intensity : { 0.1f, 0.6f, 1.0f })
                    for (bool fb : { false, true })
                    {
                        const auto a = VocalAnalyzer::analyzeOffline (c.second.data(), (int) c.second.size(), 48000.0);
                        expect (finite (a), c.first);
                        const auto r = glitchkid::automode::compute (a, Profile::astroMode, intensity, cur, rangesOf (p), fb);
                        expect (r.usable == (a.status == S::ready || fb), c.first);
                        glitchkid::automode::Range rg;
                        for (const auto& e : r.sound.values)
                        {
                            expect (rangesOf (p) (e.id, rg), e.id);
                            expect (std::isfinite (e.value) && e.value >= rg.lo && e.value <= rg.hi, c.first + " " + e.id);
                        }
                    }
        }

        beginTest ("rules engine: any analysis at all (fuzz) gives finite, in-range values");
        {
            GlitchKidProcessor p;
            const auto cur = p.captureSoundState();
            juce::Random rng (11);
            glitchkid::automode::Range rg;
            for (int n = 0; n < 400; ++n)
            {
                auto a = ready();
                a.peakDb = rng.nextFloat() * 20.0f - 20.0f;
                a.rmsDb = rng.nextFloat() * 60.0f - 70.0f;
                a.dynP10Db = rng.nextFloat() * 60.0f - 80.0f;
                a.dynP50Db = a.dynP10Db + rng.nextFloat() * 20.0f;
                a.dynP90Db = a.dynP50Db + rng.nextFloat() * 30.0f;
                a.dynamicRangeDb = a.dynP90Db - a.dynP10Db;
                a.clippedSamples = rng.nextInt (3) == 0 ? rng.nextInt (500) : 0;
                a.sibilanceIndex = rng.nextFloat();
                float sum = 0.0f;
                for (auto& f : a.bandFraction) { f = rng.nextFloat(); sum += f; }
                for (auto& f : a.bandFraction) f /= sum;
                const auto r = glitchkid::automode::compute (a, Profile::astroMode, rng.nextFloat(), cur, rangesOf (p));
                for (const auto& e : r.sound.values)
                    if (rangesOf (p) (e.id, rg))
                        expect (std::isfinite (e.value) && e.value >= rg.lo && e.value <= rg.hi, e.id);
            }
        }

        beginTest ("plug-in: listening changes no control; APPLY sets the sound once, repeatably; without a usable analysis it changes nothing");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 480);
            juce::MidiBuffer midi;
            const auto before = p.captureSoundState();
            const auto none = p.applyAutoMode (0.8f);
            expect (! none.usable && none.changedCount == 0);
            expect (p.matchesSoundState (before) && p.getPresetName().isEmpty());

            p.getAnalyzer().start();
            const auto x = vocal (48000.0, 12.0);
            for (size_t pos = 0; pos + 480 <= x.size(); pos += 480)
            {
                th::Buf b (2, 480);
                for (int i = 0; i < 480; ++i) { b.setSample (0, i, x[pos + (size_t) i]); b.setSample (1, i, x[pos + (size_t) i]); }
                p.processBlock (b, midi);
                if (pos == 4800)
                    expect (p.getAnalyzer().isListening());
            }
            expect (! p.getAnalyzer().isListening() && p.getAnalyzer().fetch().status == S::ready);
            expect (p.matchesSoundState (before), "listening and finishing never move a control");

            const auto preview = p.previewAutoMode (0.8f);
            expect (preview.usable && p.matchesSoundState (before), "preview changes nothing");
            const auto done = p.applyAutoMode (0.8f);
            expect (done.usable && done.changedCount > 10);
            expect (p.matchesSoundState (done.sound));
            expectEquals (p.getPresetName(), juce::String (GlitchKidProcessor::autoModePresetName));
            expect (! p.isPresetModified());
            expectEquals (th::getParam (p, "tune_on"), 1.0f);
            const auto after = p.captureSoundState();

            const auto again = p.applyAutoMode (0.8f);
            expect (again.usable && again.changedCount == 0, "same analysis + same intensity = same sound");
            expect (p.matchesSoundState (after));

            th::setParam (p, "echo_mix", 77.0f); // the user can still edit
            expect (p.isPresetModified());
        }

        beginTest ("plug-in: the audio stays finite with AutoMode applied and with NaN/Inf input while listening (mono and stereo)");
        {
            for (int channels = 1; channels <= 2; ++channels)
            {
                GlitchKidProcessor p;
                if (channels == 1) th::setLayout (p, juce::AudioChannelSet::mono());
                p.prepareToPlay (48000.0, 480);
                juce::MidiBuffer midi;
                p.getAnalyzer().start();
                const auto x = vocal (48000.0, 12.0);
                bool ok = true;
                for (size_t pos = 0, blk = 0; pos + 480 <= x.size(); pos += 480, ++blk)
                {
                    th::Buf b (channels, 480);
                    for (int ch = 0; ch < channels; ++ch)
                        for (int i = 0; i < 480; ++i)
                            b.setSample (ch, i, x[pos + (size_t) i]);
                    if (blk % 7 == 3) b.setSample (0, 10, std::numeric_limits<float>::quiet_NaN());
                    if (blk == 500) p.applyAutoMode (1.0f);
                    p.processBlock (b, midi);
                    for (int ch = 0; ch < channels; ++ch)
                        for (int i = 0; i < 480; ++i)
                            ok &= std::isfinite (b.getSample (ch, i));
                }
                expect (ok, "channels " + juce::String (channels));
                expect (p.getAnalyzer().fetch().status == S::ready);
            }
        }

        beginTest ("state: intensity is saved with the session, an older session without it loads with 0.6, presets and A/B are unaffected");
        {
            GlitchKidProcessor a;
            expectWithinAbsoluteError (a.getAutoIntensity(), 0.6f, 1.0e-6f);
            a.setAutoIntensity (0.35f);
            a.captureAbSlot (0);
            juce::MemoryBlock mb;
            a.getStateInformation (mb);
            GlitchKidProcessor b;
            b.setStateInformation (mb.getData(), (int) mb.getSize());
            expectWithinAbsoluteError (b.getAutoIntensity(), 0.35f, 1.0e-6f);
            expect (b.hasAbSlot (0));
            a.setAutoIntensity (std::numeric_limits<float>::quiet_NaN());
            expectWithinAbsoluteError (a.getAutoIntensity(), 0.6f, 1.0e-6f);
            a.setAutoIntensity (7.0f);
            expectEquals (a.getAutoIntensity(), 1.0f);

            // The intensity is not a sound parameter: presets neither store nor change it.
            expectEquals ((int) a.captureSoundState().values.size(), a.getParameters().size() - 1);
            auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("glitchkid-am-" + juce::String::toHexString (juce::Random().nextInt64()));
            glitchkid::PresetController c (a, glitchkid::PresetStore (dir));
            a.setAutoIntensity (0.25f);
            c.loadFactory (0);
            expectWithinAbsoluteError (a.getAutoIntensity(), 0.25f, 1.0e-6f);
            dir.deleteRecursively();
        }

        beginTest ("real-time safety: the analyzer allocates nothing while listening, idle, or finishing");
        {
            VocalAnalyzer an;
            an.prepare (48000.0);
            const auto x = vocal (48000.0, 12.0);
            an.start();
            const long start = g_allocations.load();
            for (size_t pos = 0; pos + 480 <= x.size(); pos += 480)
            {
                const float* c[2] = { x.data() + pos, x.data() + pos };
                an.process (c, 2, 480);
            }
            const float* c[2] = { x.data(), x.data() };
            an.process (c, 2, 480); // idle again after finishing
            expectEquals (g_allocations.load() - start, 0L);
        }
    }
};
AutoModeTests autoModeTests;
}
