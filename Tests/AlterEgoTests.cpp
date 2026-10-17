#include "TestHelpers.h"
#include "../Source/AlterEgos.h"
#include <map>
#include <set>

namespace
{
using AlterEgos::Value;
using Snapshot = std::map<juce::String, float>;

Snapshot snapshot (juce::AudioProcessor& p)
{
    Snapshot s;
    for (auto* q : p.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (q))
            s[r->paramID] = r->convertFrom0to1 (r->getValue());
    return s;
}

bool allFinite (const Buf& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (ch, i)))
                return false;
    return true;
}

// Noise with a speech-like spectrum (low-passed around 3 kHz) at -20 dBFS RMS.
Buf speechLike (int channels, int n, juce::int64 seed)
{
    Buf b (channels, n);
    juce::Random rng (seed);
    for (int ch = 0; ch < channels; ++ch)
    {
        double lp = 0.0;
        for (int i = 0; i < n; ++i)
        {
            lp += 0.35 * ((rng.nextDouble() * 2.0 - 1.0) - lp);
            b.setSample (ch, i, (float) lp);
        }
        b.applyGain (ch, 0, n, 0.1f / b.getRMSLevel (ch, 0, n));
    }
    return b;
}

Buf sine (int n, double sr, float freq, float amp)
{
    Buf b (2, n);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < n; ++i)
            b.setSample (ch, i, (float) (amp * std::sin (juce::MathConstants<double>::twoPi * freq * (double) i / sr)));
    return b;
}

void process (GlitchKidProcessor& p, Buf& b, int blockSize = 512)
{
    juce::MidiBuffer midi;
    for (int start = 0; start < b.getNumSamples(); start += blockSize)
    {
        Buf view (b.getArrayOfWritePointers(), b.getNumChannels(), start, juce::jmin (blockSize, b.getNumSamples() - start));
        p.processBlock (view, midi);
    }
}

float maxStep (const Buf& b, int from, int to)
{
    float m = 0.0f;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = juce::jmax (1, from); i < to; ++i)
            m = juce::jmax (m, std::abs (b.getSample (ch, i) - b.getSample (ch, i - 1)));
    return m;
}

double levelChangeDb (int ego, float chaos, std::uint32_t seed)
{
    GlitchKidProcessor p;
    p.prepareToPlay (48000.0, 512);
    p.applyAlterEgo (ego, chaos, seed);
    Buf in = speechLike (2, 48000 * 4, 60 + ego), out;
    out.makeCopyOf (in);
    process (p, out);
    return juce::Decibels::gainToDecibels (out.getRMSLevel (0, 48000, 48000 * 3) / in.getRMSLevel (0, 48000, 48000 * 3));
}

float valueOf (const Value* v, const char* id)
{
    for (int i = 0; i < AlterEgos::numOwned; ++i)
        if (juce::String (v[i].id) == id)
            return v[i].value;
    return -9999.0f;
}

const char* ownedIds[] = { "eq_lo_freq", "eq_lo_gain", "eq_b1_freq", "eq_b1_gain", "eq_b1_q", "eq_b2_freq", "eq_b2_gain", "eq_b2_q",
                           "eq_hi_freq", "eq_hi_gain", "comp_thresh", "comp_ratio", "comp_attack", "comp_release", "comp_knee",
                           "comp_makeup", "sat_drive", "echo_on", "echo_sync", "echo_time", "echo_div", "echo_fb", "echo_tone",
                           "echo_width", "echo_mix", "dim_on", "dim_width", "dim_detune", "dim_depth", "dim_mix",
                           "mut_on", "mut_amount", "mut_character", "mut_blend" };
const char* switches[] = { "echo_on", "echo_sync", "echo_time", "echo_div", "dim_on", "mut_on" };
const char* untouched[] = { "gain", "bypass", "mix", "stutter_on", "stutter_rate", "stutter_mix" };

// Every parameter ID that existed before Alter Egos. None may be renamed or removed.
const char* allOriginalIds[] = { "gain", "bypass", "eq_lo_freq", "eq_lo_gain", "eq_b1_freq", "eq_b1_gain", "eq_b1_q", "eq_b2_freq",
    "eq_b2_gain", "eq_b2_q", "eq_hi_freq", "eq_hi_gain", "comp_thresh", "comp_ratio", "comp_attack", "comp_release", "comp_knee",
    "comp_makeup", "sat_drive", "stutter_on", "stutter_rate", "stutter_mix", "mix", "echo_on", "echo_sync", "echo_time", "echo_div",
    "echo_fb", "echo_tone", "echo_width", "echo_mix", "dim_on", "dim_width", "dim_detune", "dim_depth", "dim_mix" };

struct CountingListener : juce::AudioProcessorParameter::Listener
{
    int changes = 0, begins = 0, ends = 0;
    void parameterValueChanged (int, float) override { ++changes; }
    void parameterGestureChanged (int, bool starting) override { (starting ? begins : ends)++; }
};
} // namespace

struct AlterEgoTests : public juce::UnitTest
{
    AlterEgoTests() : juce::UnitTest ("Alter Egos", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("five Alter Egos with unique, stable IDs, names, descriptions and documented required effects");
        {
            const char* ids[] = { "celestial", "the_clone", "voidwalker", "the_parasite", "altered_state" };
            const char* names[] = { "CELESTIAL", "THE CLONE", "VOIDWALKER", "THE PARASITE", "ALTERED STATE" };
            expectEquals (AlterEgos::count, 5);
            std::set<juce::String> seen;
            for (int i = 0; i < 5; ++i)
            {
                expectEquals (juce::String (AlterEgos::info (i).id), juce::String (ids[i]));
                expectEquals (juce::String (AlterEgos::info (i).name), juce::String (names[i]));
                expect (seen.insert (ids[i]).second);
                expect (juce::String (AlterEgos::info (i).description).length() > 30, "description " + juce::String (i));
                expect ((AlterEgos::info (i).required & AlterEgos::eq) != 0 && AlterEgos::info (i).required != 0, "required effects documented");
                expectEquals (AlterEgos::indexOfId (ids[i]), i);
                expect (AlterEgos::isAvailable (i), juce::String (ids[i]) + " is available: all its effects exist in this build");
            }
            expectEquals (AlterEgos::indexOfId ("nope"), -1);
            expectEquals (AlterEgos::info (-3).id, AlterEgos::info (0).id); // out-of-range indices are clamped, never invalid memory
            expectEquals (AlterEgos::info (99).id, AlterEgos::info (4).id);
        }

        beginTest ("unavailable effects are never treated as implemented: availability follows each Alter Ego's required effects");
        {
            using namespace AlterEgos;
            const unsigned all = implementedEffects;
            expect (isAvailable (1, all) && ! isAvailable (1, all & ~dim), "The Clone needs Dimension");
            expect (isAvailable (0, all & ~dim), "Celestial only lists Dimension as optional");
            expect (! isAvailable (0, all & ~echo) && ! isAvailable (2, all & ~echo), "Celestial and Voidwalker need Liquid Echo");
            expect (isAvailable (3, all & ~echo & ~dim), "The Parasite works from EQ, compressor and saturation alone");
            expect (! isAvailable (4, all & ~dim) && ! isAvailable (4, all & ~echo), "Altered State needs both echo and Dimension");
            for (int i = 0; i < count; ++i)
                expect (! isAvailable (i, 0) && ! isAvailable (i, stutter), "nothing is available without its effects");
            expect (! isAvailable (-1) && ! isAvailable (count));
        }

        beginTest ("every Alter Ego references only real parameters with legal values at every Chaos and every Surprise seed");
        {
            GlitchKidProcessor p;
            const std::set<juce::String> owned (std::begin (ownedIds), std::end (ownedIds));
            for (int e = 0; e < AlterEgos::count; ++e)
                for (float chaos : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
                    for (std::uint32_t seed = 0; seed < 25; ++seed)
                    {
                        Value v[AlterEgos::numOwned];
                        expectEquals (AlterEgos::compute (e, chaos, seed, v), AlterEgos::numOwned);
                        std::set<juce::String> seen;
                        for (int i = 0; i < AlterEgos::numOwned; ++i)
                        {
                            const juce::String id (v[i].id);
                            auto* r = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (id));
                            if (r == nullptr) { expect (false, "unknown parameter " + id); continue; }
                            expect (seen.insert (id).second, "duplicate " + id);
                            const auto range = r->getNormalisableRange();
                            expect (std::isfinite (v[i].value) && v[i].value >= range.start - 1.0e-4f && v[i].value <= range.end + 1.0e-4f,
                                    juce::String (AlterEgos::info (e).id) + ": " + id + " = " + juce::String (v[i].value) + " out of range");
                            if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (r))
                                expect (juce::approximatelyEqual (v[i].value, std::round (v[i].value)) && (int) v[i].value < c->choices.size(), id + " choice");
                        }
                        expect (seen == owned, "exactly the 34 owned parameters (30 effect parameters + the 4 Vocal Mutation parameters)");
                        for (auto* u : untouched)
                            expect (seen.count (u) == 0, juce::String (u) + " is never set by an Alter Ego");
                        expect (valueOf (v, "echo_fb") <= 75.0f && valueOf (v, "sat_drive") <= 16.0f && valueOf (v, "dim_detune") <= 20.0f
                                    && valueOf (v, "echo_tone") <= 8000.0f, "safe limits: feedback <= 75 %, drive <= 16 dB, detune <= 20 cents");
                    }
        }

        beginTest ("documented base (Chaos 0) and full (Chaos 1) values of the defining parameters");
        {
            struct Anchor { int ego; const char* id; float at0, at1; };
            const Anchor anchors[] = {
                { 0, "sat_drive", 3.0f, 6.0f }, { 0, "echo_fb", 35.0f, 50.0f }, { 0, "dim_detune", 6.0f, 10.0f }, { 0, "eq_hi_gain", 3.0f, 5.0f },
                { 1, "sat_drive", 2.0f, 5.0f }, { 1, "echo_fb", 20.0f, 35.0f }, { 1, "dim_detune", 8.0f, 16.0f }, { 1, "dim_depth", 70.0f, 100.0f },
                { 2, "sat_drive", 3.0f, 6.0f }, { 2, "echo_fb", 48.0f, 65.0f }, { 2, "dim_detune", 5.0f, 9.0f }, { 2, "eq_hi_gain", -4.0f, -8.0f },
                { 3, "sat_drive", 9.0f, 16.0f }, { 3, "echo_fb", 30.0f, 60.0f }, { 3, "dim_detune", 10.0f, 20.0f }, { 3, "comp_ratio", 4.0f, 8.0f },
                { 4, "sat_drive", 5.0f, 9.0f }, { 4, "echo_fb", 55.0f, 75.0f }, { 4, "dim_detune", 14.0f, 20.0f }, { 4, "eq_b1_q", 3.0f, 6.0f } };
            for (auto& a : anchors)
            {
                Value v0[AlterEgos::numOwned], v1[AlterEgos::numOwned];
                AlterEgos::compute (a.ego, 0.0f, 0, v0);
                AlterEgos::compute (a.ego, 1.0f, 0, v1);
                expectWithinAbsoluteError (valueOf (v0, a.id), a.at0, 1.0e-4f, juce::String (AlterEgos::info (a.ego).id) + " " + a.id + " at Chaos 0");
                expectWithinAbsoluteError (valueOf (v1, a.id), a.at1, 1.0e-4f, juce::String (AlterEgos::info (a.ego).id) + " " + a.id + " at Chaos 1");
            }
        }

        beginTest ("Chaos is a deterministic, bounded, linear move of continuous parameters; switches never move");
        for (int e = 0; e < AlterEgos::count; ++e)
        {
            Value v0[AlterEgos::numOwned], v1[AlterEgos::numOwned], vm[AlterEgos::numOwned], again[AlterEgos::numOwned];
            AlterEgos::compute (e, 0.0f, 0, v0);
            AlterEgos::compute (e, 1.0f, 0, v1);
            AlterEgos::compute (e, 0.5f, 0, vm);
            AlterEgos::compute (e, 0.5f, 0, again);
            int moving = 0;
            for (int i = 0; i < AlterEgos::numOwned; ++i)
            {
                expectEquals (vm[i].value, again[i].value);                                    // deterministic
                expectWithinAbsoluteError (vm[i].value, 0.5f * (v0[i].value + v1[i].value), 1.0e-3f); // linear
                for (float c : { 0.1f, 0.3f, 0.6f, 0.9f })
                {
                    Value vc[AlterEgos::numOwned];
                    AlterEgos::compute (e, c, 0, vc);
                    expect (vc[i].value >= juce::jmin (v0[i].value, v1[i].value) - 1.0e-4f
                                && vc[i].value <= juce::jmax (v0[i].value, v1[i].value) + 1.0e-4f, "stays between its Chaos 0 and Chaos 1 values");
                }
                if (std::abs (v1[i].value - v0[i].value) > 1.0e-4f) ++moving;
                for (auto* s : switches)
                    if (juce::String (v0[i].id) == s)
                        expectEquals (v1[i].value, v0[i].value, juce::String (s) + " must not move with Chaos");
            }
            expect (moving >= 10, juce::String (AlterEgos::info (e).id) + ": Chaos moves " + juce::String (moving) + " parameters (EQ, dynamics, saturation, echo, Dimension)");
            Value lo[AlterEgos::numOwned], hi[AlterEgos::numOwned];
            AlterEgos::compute (e, -3.0f, 0, lo);
            AlterEgos::compute (e, 7.0f, 0, hi);
            for (int i = 0; i < AlterEgos::numOwned; ++i)
                expect (juce::exactlyEqual (lo[i].value, v0[i].value) && juce::exactlyEqual (hi[i].value, v1[i].value), "Chaos is clamped to 0..1");
            expect (valueOf (v1, "sat_drive") > valueOf (v0, "sat_drive") && valueOf (v1, "dim_detune") > valueOf (v0, "dim_detune")
                        && valueOf (v1, "echo_fb") > valueOf (v0, "echo_fb"), "Chaos adds saturation, modulation and echo feedback");
        }

        beginTest ("applying Chaos through the processor sets the intended parameters, leaves everything else alone and stores the selection");
        for (int e = 0; e < AlterEgos::count; ++e)
        {
            GlitchKidProcessor p;
            setParam (p, "gain", -4.0f); setParam (p, "mix", 70.0f); setParam (p, "stutter_rate", 2.0f);
            setParam (p, "stutter_mix", 55.0f); setParam (p, "stutter_on", 1.0f); setParam (p, "bypass", 1.0f);
            const auto before = snapshot (p);
            expectEquals (p.getAlterEgoSelection().ego, -1);
            expectEquals (p.applyAlterEgo (e, 0.0f, 0), AlterEgos::numActive (e)); // inactive entries (a parameter this Alter Ego leaves alone) are not applied
            const auto atZero = snapshot (p);
            Value v0[AlterEgos::numOwned];
            AlterEgos::compute (e, 0.0f, 0, v0);
            std::set<juce::String> mine;
            for (int i = 0; i < AlterEgos::numOwned; ++i)
            {
                if (! v0[i].active)
                    continue; // left unaffected by this Alter Ego
                mine.insert (v0[i].id);
                auto* r = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (v0[i].id));
                const float step = r->getNormalisableRange().interval;
                expectWithinAbsoluteError (atZero.at (v0[i].id), v0[i].value, step > 0.0f ? 0.5f * step + 1.0e-4f : 1.0e-3f, "Chaos 0 = base: " + juce::String (v0[i].id));
            }
            expect (p.isAlterEgoActive());
            p.setChaos (1.0f);
            const auto atFull = snapshot (p);
            int changed = 0;
            for (auto& kv : atFull)
            {
                if (mine.count (kv.first) == 0)
                    expectEquals (kv.second, before.at (kv.first), juce::String (AlterEgos::info (e).id) + " changed unrelated parameter " + kv.first);
                else if (std::abs (kv.second - atZero.at (kv.first)) > 1.0e-4f)
                    ++changed;
            }
            expect (changed >= 10, "Chaos 1 changed " + juce::String (changed) + " of the Alter Ego's parameters");
            const auto sel = p.getAlterEgoSelection();
            expectEquals (sel.ego, e);
            expectEquals (sel.chaos, 1.0f);
            expect (p.isAlterEgoActive());
        }

        beginTest ("Blend is the main Mix: 0 % is the exact latency-aligned dry signal, 100 % the processed sound, 50 % their average");
        for (int e = 0; e < AlterEgos::count; ++e)
        {
            Buf in = speechLike (2, 48000, 11);
            auto render = [&] (float blend)
            {
                GlitchKidProcessor p;
                p.prepareToPlay (48000.0, 512);
                p.applyAlterEgo (e, 0.5f, 0);
                setParam (p, "mix", blend);
                p.prepareToPlay (48000.0, 512); // settle the Blend at its target from the first sample
                Buf out;
                out.makeCopyOf (in);
                process (p, out);
                return out;
            };
            const int latency = Saturation::latencySamples;
            Buf dry = render (0.0f), wet = render (100.0f), half = render (50.0f);
            bool exactDry = true;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = latency; i < in.getNumSamples(); ++i)
                    exactDry &= juce::exactlyEqual (dry.getSample (ch, i), in.getSample (ch, i - latency));
            expect (exactDry, juce::String (AlterEgos::info (e).id) + ": Blend 0 is the input delayed by exactly the reported latency");
            expect (maxAbsDiff (dry, wet) > 0.02f, "Blend 100 is clearly the processed sound");
            float err = 0.0f;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < in.getNumSamples(); ++i)
                    err = juce::jmax (err, std::abs (half.getSample (ch, i) - 0.5f * (dry.getSample (ch, i) + wet.getSample (ch, i))));
            expect (err < 1.0e-4f, "Blend 50 is the average of the two endpoints (error " + juce::String (err) + ")");
        }
        {
            GlitchKidProcessor p;
            expectEquals (p.getLatencySamples(), Saturation::latencySamples); // unchanged by this feature
        }

        beginTest ("Surprise Me changes only allowlisted parameters, only by bounded steps, repeatably for a seed");
        {
            int nRules = 0;
            const auto* rules = AlterEgos::surpriseRules (nRules);
            std::set<juce::String> allow;
            for (int i = 0; i < nRules; ++i) allow.insert (rules[i].id);
            expect (nRules >= 8);
            for (auto* u : untouched) expect (allow.count (u) == 0);
            for (auto* s : { "comp_makeup", "sat_drive", "echo_on", "echo_div", "dim_on", "bypass", "gain" })
                expect (allow.count (s) == 0, juce::String (s) + " must not be randomised");

            GlitchKidProcessor p;
            for (int e = 0; e < AlterEgos::count; ++e)
                for (float chaos : { 0.0f, 0.5f, 1.0f })
                {
                    Value base[AlterEgos::numOwned];
                    AlterEgos::compute (e, chaos, 0, base);
                    for (std::uint32_t seed = 1; seed <= 60; ++seed)
                    {
                        Value v[AlterEgos::numOwned], again[AlterEgos::numOwned];
                        AlterEgos::compute (e, chaos, seed, v);
                        AlterEgos::compute (e, chaos, seed, again);
                        for (int i = 0; i < AlterEgos::numOwned; ++i)
                        {
                            const juce::String id (v[i].id);
                            expectEquals (v[i].value, again[i].value);                         // same seed, same result
                            auto* r = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (id));
                            const auto range = r->getNormalisableRange();
                            expect (v[i].value >= range.start && v[i].value <= range.end, "valid range: " + id);
                            if (allow.count (id) == 0)
                                expectEquals (v[i].value, base[i].value, "non-allowlisted " + id + " must not change");
                            else
                                for (int k = 0; k < nRules; ++k)
                                    if (juce::String (rules[k].id) == id)
                                    {
                                        expect (std::abs (v[i].value - base[i].value) <= rules[k].jitter + 1.0e-3f, id + " moves by at most its jitter");
                                        expect (v[i].value >= rules[k].lo - 1.0e-4f && v[i].value <= rules[k].hi + 1.0e-4f, id + " stays inside its hard limits");
                                    }
                        }
                        expect (valueOf (v, "echo_fb") <= 75.0f, "no unsafe feedback");
                    }
                }
            Value a[AlterEgos::numOwned], b[AlterEgos::numOwned];
            AlterEgos::compute (3, 0.5f, 5, a);
            AlterEgos::compute (3, 0.5f, 6, b);
            bool differs = false;
            for (int i = 0; i < AlterEgos::numOwned; ++i) differs |= std::abs (a[i].value - b[i].value) > 1.0e-4f;
            expect (differs, "different seeds give different variations");
            expect (AlterEgos::nextSeed (0) != 0 && AlterEgos::nextSeed (12345) != 12345 && AlterEgos::nextSeed (12345) == AlterEgos::nextSeed (12345),
                    "the 'another variation' sequence is deterministic and never 0");
        }

        beginTest ("Surprise skips an effect that is off, and leaves Chaos-0 identity values alone");
        {
            Value v[AlterEgos::numOwned];
            AlterEgos::compute (0, 0.5f, 0, v);
            for (int i = 0; i < AlterEgos::numOwned; ++i)
                if (juce::String (v[i].id) == "echo_on") v[i].value = 0.0f; // pretend the echo is off
            Value before[AlterEgos::numOwned];
            std::copy (v, v + AlterEgos::numOwned, before);
            AlterEgos::applySurprise (v, AlterEgos::numOwned, 99, 0);
            for (int i = 0; i < AlterEgos::numOwned; ++i)
                if (juce::String (v[i].id).startsWith ("echo_"))
                    expectEquals (v[i].value, before[i].value, juce::String (v[i].id) + " untouched while echo is off");
            expect (std::abs (valueOf (v, "dim_detune") - valueOf (before, "dim_detune")) > 1.0e-4f, "Dimension (on) did vary");
            Value copy[AlterEgos::numOwned];
            std::copy (before, before + AlterEgos::numOwned, copy);
            AlterEgos::applySurprise (copy, AlterEgos::numOwned, 0, 0); // seed 0 = no variation
            for (int i = 0; i < AlterEgos::numOwned; ++i) expectEquals (copy[i].value, before[i].value);
        }

        beginTest ("Surprise through the processor keeps the Alter Ego identifiable, touches nothing unrelated and stays level-safe");
        for (int e = 0; e < AlterEgos::count; ++e)
        {
            GlitchKidProcessor p;
            setParam (p, "gain", -2.0f); setParam (p, "mix", 80.0f); setParam (p, "stutter_rate", 3.0f);
            p.applyAlterEgo (e, 0.4f, 0);
            const auto base = snapshot (p);
            expectEquals (p.surprise (4242), AlterEgos::numActive (e));
            const auto varied = snapshot (p);
            expect (p.isAlterEgoActive());
            expectEquals (p.getAlterEgoSelection().ego, e);
            expect (p.getAlterEgoSelection().seed == 4242u);
            for (auto* u : untouched) expectEquals (varied.at (u), base.at (u));
            for (auto* keep : { "sat_drive", "echo_div", "echo_on", "dim_on", "comp_makeup", "comp_ratio" })
                expectEquals (varied.at (keep), base.at (keep), juce::String (keep) + " keeps the Alter Ego's identity");
            expect (varied != base, "the variation is audible");
            for (std::uint32_t seed : { 1u, 2u, 3u, 77u, 1000u, 99999u })
                for (float chaos : { 0.0f, 1.0f })
                {
                    const double db = levelChangeDb (e, chaos, seed);
                    expect (std::abs (db) <= 2.5, juce::String (AlterEgos::info (e).id) + " chaos " + juce::String (chaos) + " seed " + juce::String ((int) seed) + ": " + juce::String (db, 2) + " dB");
                }
        }

        beginTest ("every Alter Ego, at any Chaos and Surprise seed, gives finite, bounded audio (mono and stereo, 44.1/48/96 kHz)");
        for (int e = 0; e < AlterEgos::count; ++e)
            for (float chaos : { 0.0f, 0.5f, 1.0f })
                for (std::uint32_t seed : { 0u, 7u })
                    for (double sr : { 44100.0, 48000.0, 96000.0 })
                        for (int ch : { 1, 2 })
                        {
                            GlitchKidProcessor p;
                            if (ch == 1) setLayout (p, juce::AudioChannelSet::mono());
                            p.prepareToPlay (sr, 512);
                            p.applyAlterEgo (e, chaos, seed);
                            Buf in = speechLike (ch, (int) (sr * 2), 40 + e);
                            process (p, in);
                            expect (allFinite (in) && in.getMagnitude (0, in.getNumSamples()) < 4.0f,
                                    juce::String (AlterEgos::info (e).id) + " chaos " + juce::String (chaos) + " seed " + juce::String ((int) seed)
                                        + " sr " + juce::String (sr) + " ch " + juce::String (ch));
                        }

        beginTest ("level stays within +-1 dB of the input at every Chaos (speech-like noise); Alter Egos do not hide level changes");
        {
            double worst = 0.0;
            for (int e = 0; e < AlterEgos::count; ++e)
                for (float chaos : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
                {
                    const double db = levelChangeDb (e, chaos, 0);
                    worst = juce::jmax (worst, std::abs (db));
                    expect (std::abs (db) <= 1.0, juce::String (AlterEgos::info (e).id) + " chaos " + juce::String (chaos) + ": " + juce::String (db, 2) + " dB");
                }
            logMessage ("  worst level change over all Alter Egos and Chaos settings: " + juce::String (worst, 2) + " dB");
        }

        beginTest ("switching Alter Egos, and dragging Chaos or pressing Surprise during playback, is smooth (30 Hz tone: no jump)");
        {
            float worstRatio = 0.0f;
            auto check = [&] (const juce::String& what, GlitchKidProcessor& p, const std::function<void()>& change)
            {
                Buf warm = sine (48000, 48000.0, 30.0f, 0.4f);
                process (p, warm, 256);
                Buf before = sine (24000, 48000.0, 30.0f, 0.4f);
                process (p, before, 256);
                change();
                Buf during = sine (48000, 48000.0, 30.0f, 0.4f);
                process (p, during, 256);
                expect (allFinite (during), "finite: " + what);
                const float own = juce::jmax (maxStep (before, 1, before.getNumSamples()), maxStep (during, 36000, 48000));
                float worst = maxStep (during, 1, 36000);
                for (int ch = 0; ch < 2; ++ch)
                    worst = juce::jmax (worst, std::abs (during.getSample (ch, 0) - before.getSample (ch, before.getNumSamples() - 1)));
                worstRatio = juce::jmax (worstRatio, worst / own);
                expect (worst <= 8.0f * own, what + ": step " + juce::String (worst) + " vs the tone's own " + juce::String (own));
            };
            for (int from = 0; from < AlterEgos::count; ++from)
                for (int to = 0; to < AlterEgos::count; ++to)
                    if (from != to)
                    {
                        GlitchKidProcessor p;
                        p.prepareToPlay (48000.0, 256);
                        p.applyAlterEgo (from, 0.3f, 0);
                        check ("switch " + juce::String (from) + " -> " + juce::String (to), p, [&] { p.applyAlterEgo (to, 0.3f, 0); });
                    }
            for (int e = 0; e < AlterEgos::count; ++e)
            {
                GlitchKidProcessor p;
                p.prepareToPlay (48000.0, 256);
                p.applyAlterEgo (e, 0.0f, 0);
                check ("Chaos jump 0 -> 1 on " + juce::String (e), p, [&] { p.setChaos (1.0f); });
                GlitchKidProcessor q;
                q.prepareToPlay (48000.0, 256);
                q.applyAlterEgo (e, 0.5f, 0);
                check ("Surprise on " + juce::String (e), q, [&] { q.surprise (31337); });
            }
            logMessage ("  worst Alter-Ego/Chaos/Surprise change step vs the tone's own step: " + juce::String (worstRatio, 2) + "x (limit 8x)");
        }

        beginTest ("saved state: the selection and every parameter survive save/restore; restoring does not re-apply or overwrite edits");
        for (int e = 0; e < AlterEgos::count; ++e)
        {
            GlitchKidProcessor a;
            a.applyAlterEgo (e, 0.6f, 1234);
            setParam (a, "gain", -2.5f);
            juce::MemoryBlock state;
            a.getStateInformation (state);
            GlitchKidProcessor b;
            b.setStateInformation (state.getData(), (int) state.getSize());
            expect (snapshot (a) == snapshot (b), "parameters round-trip, Alter Ego " + juce::String (e));
            const auto sel = b.getAlterEgoSelection();
            expectEquals (sel.ego, e);
            expectWithinAbsoluteError (sel.chaos, 0.6f, 1.0e-6f);
            expect (sel.seed == 1234u);
            expect (b.isAlterEgoActive(), "the restored controls still match the selection");

            // A user edit (or automation) after selecting persists through save/restore: restoring never re-applies the Alter Ego.
            setParam (a, "echo_fb", 12.0f);
            juce::MemoryBlock edited;
            a.getStateInformation (edited);
            GlitchKidProcessor c;
            c.setStateInformation (edited.getData(), (int) edited.getSize());
            expectWithinAbsoluteError (getParam (c, "echo_fb"), 12.0f, 0.1f);
            expect (! c.isAlterEgoActive() && c.getAlterEgoSelection().ego == e, "edited: reported as custom, selection remembered");
        }
        {
            GlitchKidProcessor a, b;
            juce::MemoryBlock state;
            a.getStateInformation (state);
            b.applyAlterEgo (2, 0.5f, 9);
            b.setStateInformation (state.getData(), (int) state.getSize());
            expectEquals (b.getAlterEgoSelection().ego, -1); // a state saved before Alter Egos has no selection
            expect (! b.isAlterEgoActive());
            expect (snapshot (a) == snapshot (b), "a default state restores the defaults");

            // An older state without the selection notes (and without Dimension parameters) still loads.
            GlitchKidProcessor src;
            src.applyAlterEgo (1, 0.5f, 0);
            juce::MemoryBlock full;
            src.getStateInformation (full);
            if (auto xml = juce::AudioProcessor::getXmlFromBinary (full.getData(), (int) full.getSize()))
            {
                xml->removeAttribute ("alterEgo"); xml->removeAttribute ("alterEgoChaos"); xml->removeAttribute ("alterEgoSeed");
                for (int i = xml->getNumChildElements() - 1; i >= 0; --i)
                    if (xml->getChildElement (i)->getStringAttribute ("id").startsWith ("dim_"))
                        xml->removeChildElement (xml->getChildElement (i), true);
                juce::MemoryBlock old;
                juce::AudioProcessor::copyXmlToBinary (*xml, old);
                GlitchKidProcessor d;
                d.setStateInformation (old.getData(), (int) old.getSize());
                expectEquals (d.getAlterEgoSelection().ego, -1);
                expectEquals (getParam (d, "dim_on"), 0.0f);
                expectWithinAbsoluteError (getParam (d, "echo_fb"), 27.5f, 0.1f); // The Clone at Chaos 0.5: the rest of the old state is intact
            }
            else
                expect (false, "could not read saved state");
        }

        beginTest ("existing parameter IDs and automation are intact; applying an Alter Ego reports normal host gestures");
        {
            GlitchKidProcessor p;
            std::set<juce::String> ids;
            for (auto* q : p.getParameters())
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (q)) ids.insert (r->paramID);
            for (auto* id : allOriginalIds)
                expect (ids.count (id) == 1, juce::String ("parameter ID still present: ") + id);
            // No original parameter was renamed or removed. The only additions are the five Pitch Tune parameters (Stage 4.2) and
            // Formant Shift (Stage 4.4), which no Alter Ego may touch (checked in PitchTuneTests.cpp and FormantTuneTests.cpp), and the four
            // Vocal Mutation parameters (Stage 4.5), which Alter Egos do not touch either (checked in MutationTests.cpp).
            expectEquals ((int) ids.size(), (int) std::size (allOriginalIds) + 10);
            for (auto* id : { "tune_on", "tune_key", "tune_scale", "tune_strength", "tune_speed", "formant_shift",
                              "mut_on", "mut_amount", "mut_character", "mut_blend" })
                expect (ids.count (id) == 1, juce::String ("Pitch Tune parameter present: ") + id);

            CountingListener l;
            auto* sat = p.apvts.getParameter ("sat_drive");
            sat->addListener (&l);
            p.applyAlterEgo (3, 1.0f, 0);
            expect (l.changes >= 1 && l.begins == 1 && l.ends == 1, "gesture reported to the host");
            setParam (p, "echo_fb", 44.0f);
            expectWithinAbsoluteError (getParam (p, "echo_fb"), 44.0f, 0.1f);
            expect (! p.isAlterEgoActive(), "editing a control makes the state custom");
            p.setChaos (1.0f);
            expect (p.isAlterEgoActive(), "moving Chaos re-applies the selection");
            sat->removeListener (&l);
            expectEquals (p.applyAlterEgo (-1, 0.5f, 0), 0);
            expectEquals (p.applyAlterEgo (17, 0.5f, 0), 0);
            GlitchKidProcessor none;
            expectEquals (none.setChaos (0.7f), 0);   // no Alter Ego selected: Chaos is only remembered
            expectEquals (none.surprise (5), 0);
            expectWithinAbsoluteError (none.getAlterEgoSelection().chaos, 0.7f, 1.0e-6f);
        }

        beginTest ("missing optional effects or parameters are skipped safely (no crash, no invalid access)");
        {
            Value v[AlterEgos::numOwned];
            AlterEgos::compute (0, 0.5f, 3, v);
            std::set<juce::String> known { "sat_drive", "echo_fb", "eq_hi_gain", "not_owned" };
            std::map<juce::String, float> got;
            expectEquals (AlterEgos::applyValues (v, AlterEgos::numOwned, [&] (const char* id, float x) { if (! known.count (id)) return false; got[id] = x; return true; }), 3);
            expectEquals (AlterEgos::applyValues (v, AlterEgos::numOwned, [] (const char*, float) { return false; }), 0);
            for (int e = 0; e < AlterEgos::count; ++e)
            {
                Value w[AlterEgos::numOwned];
                AlterEgos::compute (e, 1.0f, 11, w);
                int skippedDim = 0;
                AlterEgos::applyValues (w, AlterEgos::numOwned, [&] (const char* id, float) { if (juce::String (id).startsWith ("dim_")) { ++skippedDim; return false; } return true; });
                expectEquals (skippedDim, 5);
            }
        }
    }
};

static AlterEgoTests alterEgoTests;
