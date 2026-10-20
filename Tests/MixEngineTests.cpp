#include "TestHelpers.h"
#include "../Source/AutoMode/MixEngine.h"
#include "../Source/Presets/PresetController.h"
#include <set>

// BUILD MY VOCAL MIX: the pure engine (what it proposes from which evidence), and the proposal applied to the real plug-in (parameters, undo, state, audio).
namespace
{
namespace au = glitchkid::analysis;
namespace am = glitchkid::automode;
namespace fp = glitchkid::FactoryPresets;
using glitchkid::SoundState;

au::VocalAnalysis typicalLead()
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

// A beat that is loud and sits right on the voice's mids.
au::VocalAnalysis crowdedBeat()
{
    auto b = typicalLead();
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

am::MixInput inputFor (GlitchKidProcessor& p, int profile)
{
    am::MixInput in;
    in.profile = profile;
    in.current = p.captureSoundState();
    in.factoryDefault = p.factoryDefaultState();
    in.lead = typicalLead();
    return in;
}

int differing (const SoundState& a, const SoundState& b)
{
    int n = 0;
    for (const auto& e : a.values)
        n += std::abs (e.value - b.get (e.id, e.value)) > 1.0e-4f ? 1 : 0;
    return n;
}

std::vector<float> voice (double fs, double seconds)
{
    std::vector<float> x ((size_t) (fs * seconds));
    juce::Random rng (11);
    double phase = 0.0;
    for (size_t n = 0; n < x.size(); ++n)
    {
        const double t = (double) n / fs;
        phase += 2.0 * juce::MathConstants<double>::pi * 210.0 * (1.0 + 0.02 * std::sin (2.0 * juce::MathConstants<double>::pi * 5.0 * t)) / fs;
        double s = 0.0;
        for (int h = 1; h <= 14; ++h)
            s += std::sin (phase * h) / h;
        x[n] = (float) (0.22 * s + 0.004 * (rng.nextFloat() - 0.5f));
    }
    return x;
}

struct MixEngineTests : public juce::UnitTest
{
    MixEngineTests() : juce::UnitTest ("Build my vocal mix", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("a profile and a vocal analysis produce real parameter changes, each with a reason, evidence and a plain summary");
        {
            GlitchKidProcessor p;
            auto in = inputFor (p, 0);
            const auto r = am::buildMix (in, rangesOf (p));
            expect (r.usable && r.mode == am::MixMode::vocalOnly);
            expect (r.changes.size() > 12, "many controls move for a profile: " + juce::String ((int) r.changes.size()));
            for (const auto& c : r.changes)
            {
                expect (c.reason.isNotEmpty(), c.id);
                expect (std::abs (c.to - c.from) > 1.0e-4f);
                expectWithinAbsoluteError (r.sound.get (c.id, -999.0f), c.to, 1.0e-6f);
            }
            expect (! r.evidence.isEmpty() && ! r.unavailable.isEmpty());
            expect (r.message.contains ("VOCAL ONLY") && r.message.contains ("TRAVIS SCOTT STYLE"), r.message);
            expectEquals (differing (r.sound, in.current), (int) r.changes.size());
        }

        beginTest ("vocal-only works with no instrumental at all; the wording says what is missing and claims nothing about the beat");
        {
            GlitchKidProcessor p;
            for (auto state : { am::InstrumentalInput::notConnected, am::InstrumentalInput::connectedSilent, am::InstrumentalInput::live })
            {
                auto in = inputFor (p, 5);
                in.instrumental = state;
                const auto r = am::buildMix (in, rangesOf (p)); // beat analysis stays idle
                expect (r.usable && r.mode == am::MixMode::vocalOnly);
                expect (r.unavailable[0].contains ("VOCAL ONLY"));
                for (const auto& e : r.evidence)
                    expect (! e.startsWith ("BEAT"), "no beat finding without a beat analysis");
            }
            auto in = inputFor (p, 5);
            in.instrumental = am::InstrumentalInput::notConnected;
            in.beat = crowdedBeat(); // a stale analysis from a connection that is gone must not be used
            expect (am::buildMix (in, rangesOf (p)).mode == am::MixMode::vocalOnly);
        }

        beginTest ("beat-informed: used only for a valid, ready beat analysis; findings are named; the presence moves where the beat competes");
        {
            GlitchKidProcessor p;
            auto vocalOnly = inputFor (p, 2);
            const auto a = am::buildMix (vocalOnly, rangesOf (p));
            auto informed = vocalOnly;
            informed.beat = crowdedBeat();
            informed.instrumental = am::InstrumentalInput::live;
            const auto b = am::buildMix (informed, rangesOf (p));
            expect (b.mode == am::MixMode::beatInformed);
            bool beatFinding = false;
            for (const auto& e : b.evidence)
                beatFinding = beatFinding || e.startsWith ("BEAT:");
            expect (beatFinding);
            expect (b.sound.get ("eq_b2_gain", 0.0f) > a.sound.get ("eq_b2_gain", 0.0f) + 0.5f, "presence raised against a mid-range beat");
            expect (b.sound.get ("echo_mix", 0.0f) < a.sound.get ("echo_mix", 0.0f), "ambience reduced when the beat is crowded");
            expect (b.message.contains ("BEAT INFORMED"));
            // a beat that did not analyse (too quiet) leaves a vocal-only mix, identical to not having one
            informed.beat.status = au::VocalAnalysis::Status::tooQuiet;
            const auto c = am::buildMix (informed, rangesOf (p));
            expect (c.mode == am::MixMode::vocalOnly && differing (c.sound, a.sound) == 0);
        }

        beginTest ("missing or invalid analysis proposes nothing and fabricates nothing");
        {
            GlitchKidProcessor p;
            for (auto st : { au::VocalAnalysis::Status::idle, au::VocalAnalysis::Status::listening, au::VocalAnalysis::Status::tooShort,
                             au::VocalAnalysis::Status::tooQuiet, au::VocalAnalysis::Status::invalid })
            {
                auto in = inputFor (p, 7);
                in.lead.status = st;
                const auto r = am::buildMix (in, rangesOf (p));
                expect (! r.usable && r.changes.empty() && r.evidence.isEmpty() && r.message.isNotEmpty());
                expectEquals (differing (r.sound, in.current), 0);
            }
            auto in = inputFor (p, 7);
            in.lead.rmsDb = std::numeric_limits<float>::quiet_NaN();
            expect (! am::buildMix (in, rangesOf (p)).usable);
        }

        beginTest ("fuzz: every value stays inside its range and finite, for all profiles, intensities and odd analyses (NaN, Inf, extremes)");
        {
            GlitchKidProcessor p;
            const auto ranges = rangesOf (p);
            juce::Random rng (5);
            auto rnd = [&] (float lo, float hi) { return lo + (hi - lo) * rng.nextFloat(); };
            for (int i = 0; i < 600; ++i)
            {
                auto in = inputFor (p, i % (fp::count + 1) - 1);
                in.intensity = i % 7 == 0 ? std::numeric_limits<float>::quiet_NaN() : rnd (-1.0f, 2.0f);
                auto& l = in.lead;
                l.rmsDb = rnd (-70.0f, 0.0f); l.peakDb = rnd (-60.0f, 3.0f); l.crestDb = rnd (0.0f, 30.0f); l.dynamicRangeDb = rnd (0.0f, 40.0f);
                l.clippedSamples = rng.nextInt (50); l.sibilanceIndex = rnd (0.0f, 1.0f);
                float sum = 0.0f;
                for (auto& b : l.bandFraction) { b = rnd (0.0f, 1.0f); sum += b; }
                for (auto& b : l.bandFraction) b /= sum;
                if (i % 5 == 0) { in.beat = l; in.beat.rmsDb = rnd (-50.0f, 0.0f); in.beat.peakDb = rnd (-30.0f, 0.0f); in.instrumental = am::InstrumentalInput::live; }
                if (i % 11 == 0) l.peakDb = std::numeric_limits<float>::infinity();
                const auto r = am::buildMix (in, ranges);
                for (const auto& e : r.sound.values)
                {
                    am::Range range;
                    expect (ranges (e.id, range) && std::isfinite (e.value) && e.value >= range.lo - 1.0e-4f && e.value <= range.hi + 1.0e-4f, "legal value: " + e.id);
                }
                for (const char* id : { "eq_lo_gain", "eq_b1_gain", "eq_b2_gain", "eq_hi_gain" })
                    if (r.usable)
                        expect (std::abs (r.sound.get (id, 0.0f)) <= 6.0f + 1.0e-3f, juce::String ("EQ never beyond 6 dB: ") + id);
            }
        }

        beginTest ("never touched: Gain, Mix, Bypass, your key and scale; the Alter Ego is cleared");
        {
            GlitchKidProcessor p;
            th::setParam (p, "gain", -3.0f);
            th::setParam (p, "mix", 70.0f);
            th::setParam (p, "tune_key", 9.0f);
            th::setParam (p, "tune_scale", 2.0f);
            for (int i = -1; i < fp::count; i += 7)
            {
                auto in = inputFor (p, i);
                in.lead.clippedSamples = 40; // even a clipping take only changes the effect settings, never the Gain
                const auto r = am::buildMix (in, rangesOf (p));
                for (const char* id : { "gain", "mix", "tune_key", "tune_scale" })
                    expectWithinAbsoluteError (r.sound.get (id, -999.0f), in.current.get (id, -998.0f), 1.0e-4f);
                expect (! r.sound.has ("bypass"));
                for (const auto& c : r.changes)
                    expect (c.id != "gain" && c.id != "mix" && c.id != "tune_key" && c.id != "tune_scale" && c.id != "bypass");
            }
        }

        beginTest ("profiles stay distinct after the analysis adjusts them; the intensity really scales the effect amounts");
        {
            GlitchKidProcessor p;
            std::vector<SoundState> states;
            for (int i = 0; i < fp::count; ++i)
                states.push_back (am::buildMix (inputFor (p, i), rangesOf (p)).sound);
            int minDiff = 1000;
            for (int a = 0; a < fp::count; ++a)
                for (int b = a + 1; b < fp::count; ++b)
                    minDiff = juce::jmin (minDiff, differing (states[(size_t) a], states[(size_t) b]));
            logMessage ("closest pair of built mixes differs in " + juce::String (minDiff) + " controls");
            expect (minDiff >= 8);
            auto low = inputFor (p, 0), high = inputFor (p, 0);
            low.intensity = 0.0f; high.intensity = 1.0f;
            const auto l = am::buildMix (low, rangesOf (p)), h = am::buildMix (high, rangesOf (p));
            expect (l.sound.get ("echo_mix", 0.0f) < h.sound.get ("echo_mix", 0.0f) && l.sound.get ("dbl_amount", 0.0f) < h.sound.get ("dbl_amount", 0.0f));
            expectWithinAbsoluteError (h.sound.get ("echo_mix", 0.0f), 28.0f, 0.15f); // intensity 1 = the profile as saved (Travis Scott echo mix 28)
        }

        beginTest ("repeating a build does not drift: applying the proposal and building again changes nothing");
        {
            GlitchKidProcessor p;
            auto in = inputFor (p, 11);
            in.beat = crowdedBeat();
            in.instrumental = am::InstrumentalInput::live;
            auto first = am::buildMix (in, rangesOf (p));
            SoundState now = first.sound;
            for (int i = 0; i < 6; ++i)
            {
                in.current = now;
                const auto again = am::buildMix (in, rangesOf (p));
                expectEquals ((int) again.changes.size(), 0, "build #" + juce::String (i + 2) + " changes nothing");
                now = again.sound;
            }
            expectEquals (differing (now, first.sound), 0);
        }

        beginTest ("analysis adjusts restrainedly: a quiet, dull, boomy or hot take moves the settings in the documented direction, bounded");
        {
            GlitchKidProcessor p;
            auto base = inputFor (p, 3);
            const auto ref = am::buildMix (base, rangesOf (p)).sound;
            auto quiet = base; quiet.lead.rmsDb = -32.0f;
            expect (am::buildMix (quiet, rangesOf (p)).sound.get ("comp_thresh", 0.0f) < ref.get ("comp_thresh", 0.0f) - 4.0f, "a quiet take gets a lower threshold");
            auto loud = base; loud.lead.rmsDb = -8.0f;
            expect (am::buildMix (loud, rangesOf (p)).sound.get ("comp_thresh", 0.0f) > ref.get ("comp_thresh", 0.0f) + 4.0f);
            auto veryQuiet = base; veryQuiet.lead.rmsDb = -60.0f;
            expectWithinAbsoluteError (am::buildMix (veryQuiet, rangesOf (p)).sound.get ("comp_thresh", 0.0f), ref.get ("comp_thresh", 0.0f) - 8.0f, 0.2f); // bounded shift
            auto dull = base; dull.lead.bandFraction = { 0.10f, 0.55f, 0.28f, 0.03f, 0.04f };
            expect (am::buildMix (dull, rangesOf (p)).sound.get ("eq_b2_gain", 0.0f) > ref.get ("eq_b2_gain", 0.0f) + 0.5f);
            auto boomy = base; boomy.lead.bandFraction = { 0.40f, 0.35f, 0.15f, 0.07f, 0.03f };
            expect (am::buildMix (boomy, rangesOf (p)).sound.get ("eq_lo_gain", 0.0f) < ref.get ("eq_lo_gain", 0.0f) - 1.0f);
            auto peaky = base; peaky.lead.peakDb = -1.0f;
            expect (am::buildMix (peaky, rangesOf (p)).sound.get ("comp_makeup", 0.0f) < ref.get ("comp_makeup", 0.0f) - 2.0f, "louder peaks get less makeup gain");
            auto hot = base; hot.lead.clippedSamples = 30;
            const auto h = am::buildMix (hot, rangesOf (p));
            expect (h.sound.get ("sat_drive", 0.0f) < ref.get ("sat_drive", 0.0f) && h.evidence.joinIntoString (" ").contains ("clips"));
        }

        beginTest ("applied to the real plug-in: the parameters move, the host sees normal changes, UNDO restores everything, state survives save/restore");
        {
            GlitchKidProcessor p;
            th::setParam (p, "echo_mix", 61.0f);
            th::setParam (p, "gain", -2.0f);
            const auto before = p.captureSoundState();
            auto in = inputFor (p, 20);
            const auto r = am::buildMix (in, rangesOf (p));
            p.applyPreset ("Built: " + juce::String (fp::info (20).name), r.sound);
            for (const auto& c : r.changes)
            {
                auto* prm = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (c.id));
                const float step = prm->getNormalisableRange().interval;
                expect (std::abs (th::getParam (p, c.id.toRawUTF8()) - c.to) <= (step > 0.0f ? 0.5f * step + 1.0e-3f : 2.0e-3f), "applied: " + c.id);
            }
            expectWithinAbsoluteError (th::getParam (p, "gain"), -2.0f, 0.06f);
            juce::MemoryBlock mb;
            p.getStateInformation (mb);
            GlitchKidProcessor q;
            q.setStateInformation (mb.getData(), (int) mb.getSize());
            for (const auto& e : p.captureSoundState().values)
                expectWithinAbsoluteError (q.captureSoundState().get (e.id, -999.0f), e.value, 1.0e-3f);

            p.applyPreset ("", before); // UNDO: the sound captured before the build
            for (const auto& e : before.values)
                expectWithinAbsoluteError (th::getParam (p, e.id.toRawUTF8()), e.value, 0.06f);
        }

        beginTest ("end to end audio: analyse a real signal offline, build every profile, render: finite, near the input level, never clipping");
        {
            const auto in = voice (48000.0, 5.0);
            const auto analysis = au::VocalAnalyzer::analyzeOffline (in.data(), (int) in.size(), 48000.0);
            expect (analysis.status == au::VocalAnalysis::Status::ready, "the test voice analyses as usable");
            double inE = 0.0;
            for (size_t i = 72000; i < in.size(); ++i) inE += (double) in[i] * in[i];
            const double inDb = 10.0 * std::log10 (inE / (double) (in.size() - 72000));
            double worst = 0.0, worstPeak = 0.0;
            for (int prof = -1; prof < fp::count; ++prof)
            {
                GlitchKidProcessor p;
                auto mi = inputFor (p, prof);
                mi.lead = analysis;
                const auto r = am::buildMix (mi, rangesOf (p));
                expect (r.usable);
                p.applyPreset ("built", r.sound);
                p.prepareToPlay (48000.0, 480);
                juce::MidiBuffer midi;
                double e = 0.0, peak = 0.0;
                bool finite = true;
                for (size_t pos = 0; pos + 480 <= in.size(); pos += 480)
                {
                    th::Buf b (2, 480);
                    for (int i = 0; i < 480; ++i) { b.setSample (0, i, in[pos + (size_t) i]); b.setSample (1, i, in[pos + (size_t) i]); }
                    p.processBlock (b, midi);
                    for (int i = 0; i < 480; ++i)
                    {
                        const float v = b.getSample (0, i);
                        finite = finite && std::isfinite (v);
                        peak = juce::jmax (peak, (double) std::abs (v));
                        if (pos >= 72000) e += (double) v * v;
                    }
                }
                const double d = 10.0 * std::log10 (e / (double) (in.size() - 72000) + 1.0e-20) - inDb;
                expect (finite, "finite");
                expect (std::abs (d) < 4.0 && peak < 0.99, juce::String (r.profileName) + " level " + juce::String (d, 1) + " dB peak " + juce::String (peak, 2));
                worst = juce::jmax (worst, std::abs (d));
                worstPeak = juce::jmax (worstPeak, peak);
            }
            logMessage ("end to end over 51 builds: worst level change " + juce::String (worst, 2) + " dB, worst peak " + juce::String (worstPeak, 2));
        }
    }
};
MixEngineTests mixEngineTests;
}
