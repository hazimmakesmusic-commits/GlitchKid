#include "TestHelpers.h"
#include "../Source/Moods.h"
#include <map>
#include <set>

namespace
{
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
        const float rms = b.getRMSLevel (ch, 0, n);
        b.applyGain (ch, 0, n, 0.1f / rms);
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

struct CountingListener : juce::AudioProcessorParameter::Listener
{
    int changes = 0, begins = 0, ends = 0;
    void parameterValueChanged (int, float) override { ++changes; }
    void parameterGestureChanged (int, bool starting) override { (starting ? begins : ends)++; }
};

const char* ownedGroup[] = { "eq_lo_freq", "eq_lo_gain", "eq_b1_freq", "eq_b1_gain", "eq_b1_q", "eq_b2_freq", "eq_b2_gain", "eq_b2_q",
                             "eq_hi_freq", "eq_hi_gain", "comp_thresh", "comp_ratio", "comp_attack", "comp_release", "comp_knee",
                             "comp_makeup", "sat_drive", "echo_on", "echo_sync", "echo_time", "echo_div", "echo_fb", "echo_tone",
                             "echo_width", "echo_mix", "dim_on", "dim_width", "dim_detune", "dim_depth", "dim_mix" };
const char* untouched[] = { "gain", "bypass", "mix", "stutter_on", "stutter_rate", "stutter_mix" };
} // namespace

struct MoodTests : public juce::UnitTest
{
    MoodTests() : juce::UnitTest ("Moods", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("five Moods exist with stable IDs, names and descriptions");
        {
            const char* ids[] = { "dream", "demon", "alien", "rage", "ghost" };
            const char* names[] = { "DREAM", "DEMON", "ALIEN", "RAGE", "GHOST" };
            expectEquals (Moods::count, 5);
            for (int i = 0; i < 5; ++i)
            {
                expectEquals (juce::String (Moods::get (i).id), juce::String (ids[i]));
                expectEquals (juce::String (Moods::get (i).name), juce::String (names[i]));
                expect (juce::String (Moods::get (i).description).length() > 20, "description " + juce::String (i));
                expectEquals (Moods::indexOfId (ids[i]), i);
            }
            expectEquals (Moods::indexOfId ("nope"), -1);
            expectEquals (Moods::get (-3).id, Moods::get (0).id); // out-of-range indices are clamped, never invalid memory
            expectEquals (Moods::get (99).id, Moods::get (4).id);
        }

        beginTest ("every Mood references only real parameters, with legal values, no duplicates, and owns the same full set");
        {
            GlitchKidProcessor p;
            std::set<juce::String> ownedSet (std::begin (ownedGroup), std::end (ownedGroup));
            for (int m = 0; m < Moods::count; ++m)
            {
                const auto& mood = Moods::get (m);
                std::set<juce::String> seen;
                for (int i = 0; i < mood.numValues; ++i)
                {
                    const juce::String id (mood.values[i].id);
                    auto* r = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (id));
                    expect (r != nullptr, juce::String (mood.id) + ": unknown parameter " + id);
                    if (r == nullptr) continue;
                    expect (seen.insert (id).second, juce::String (mood.id) + ": duplicate " + id);
                    const auto range = r->getNormalisableRange();
                    expect (mood.values[i].value >= range.start && mood.values[i].value <= range.end,
                            juce::String (mood.id) + ": " + id + " = " + juce::String (mood.values[i].value) + " out of range");
                    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (r))
                        expect (juce::approximatelyEqual (mood.values[i].value, std::round (mood.values[i].value)) && (int) mood.values[i].value < c->choices.size());
                    if (id == "echo_fb")
                        expect (mood.values[i].value <= 60.0f, "feedback kept well below the 90 % limit");
                }
                expect (seen == ownedSet, juce::String (mood.id) + " defines exactly the EQ, compressor, saturation, echo and Dimension parameters");
                for (auto* u : untouched)
                    expect (seen.count (u) == 0, juce::String (mood.id) + " must not touch " + u);
            }
        }

        beginTest ("applying a Mood sets its parameters, leaves everything else exactly alone, and is deterministic");
        for (int m = 0; m < Moods::count; ++m)
        {
            const auto& mood = Moods::get (m);
            GlitchKidProcessor p;
            setParam (p, "gain", -4.0f); setParam (p, "mix", 70.0f); setParam (p, "stutter_rate", 2.0f);
            setParam (p, "stutter_mix", 55.0f); setParam (p, "bypass", 0.0f);
            const auto before = snapshot (p);
            expectEquals (p.applyMood (m), mood.numValues);
            const auto after = snapshot (p);

            std::set<juce::String> mine;
            for (int i = 0; i < mood.numValues; ++i)
            {
                mine.insert (mood.values[i].id);
                auto* r = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (mood.values[i].id));
                const float step = r->getNormalisableRange().interval;
                expectWithinAbsoluteError (after.at (mood.values[i].id), mood.values[i].value, step > 0.0f ? 0.5f * step + 1.0e-4f : 1.0e-3f);
            }
            for (auto& kv : before)
                if (mine.count (kv.first) == 0)
                    expectEquals (after.at (kv.first), kv.second, juce::String (mood.id) + " changed unrelated parameter " + kv.first);
            expectEquals (p.getActiveMood(), m);

            p.applyMood (m); // again
            expect (snapshot (p) == after, juce::String (mood.id) + " is deterministic when re-applied");
        }

        beginTest ("no leakage: a Mood gives identical settings whatever Mood or edits came before it");
        {
            GlitchKidProcessor fresh;
            for (int target = 0; target < Moods::count; ++target)
            {
                fresh.applyMood (target);
                const auto reference = snapshot (fresh);
                for (int previous = 0; previous < Moods::count; ++previous)
                {
                    GlitchKidProcessor p;
                    setParam (p, "echo_fb", 77.0f); setParam (p, "dim_detune", 19.0f); setParam (p, "eq_b2_gain", -9.0f);
                    p.applyMood (previous);
                    setParam (p, "comp_makeup", 11.0f);
                    p.applyMood (target);
                    expect (snapshot (p) == reference, "Mood " + juce::String (target) + " after Mood " + juce::String (previous));
                }
            }
        }

        beginTest ("controls stay editable and automatable after a Mood: the host sees begin/changed/end, edits make it 'custom'");
        {
            GlitchKidProcessor p;
            CountingListener l;
            auto* sat = p.apvts.getParameter ("sat_drive");
            sat->addListener (&l);
            expectEquals (p.getActiveMood(), -1);
            p.applyMood (1); // DEMON: drive 9 dB
            expect (l.changes >= 1 && l.begins == 1 && l.ends == 1, "gesture reported to the host");
            expectEquals (p.getActiveMood(), 1);
            setParam (p, "echo_fb", 44.0f);
            expectWithinAbsoluteError (getParam (p, "echo_fb"), 44.0f, 0.1f);
            expectEquals (p.getActiveMood(), -1);   // edited: now custom
            p.applyMood (1);
            expectEquals (p.getActiveMood(), 1);    // re-applying restores it
            sat->removeListener (&l);
            expectEquals (p.applyMood (-1), 0);     // invalid index: nothing happens
            expectEquals (p.applyMood (17), 0);
        }

        beginTest ("saved state: a Mood's settings survive save and restore; old and default states still load");
        for (int m = 0; m < Moods::count; ++m)
        {
            GlitchKidProcessor a;
            a.applyMood (m);
            setParam (a, "gain", -2.5f);
            juce::MemoryBlock state;
            a.getStateInformation (state);
            GlitchKidProcessor b;
            b.setStateInformation (state.getData(), (int) state.getSize());
            expect (snapshot (a) == snapshot (b), "state round-trip, Mood " + juce::String (m));
            expectEquals (b.getActiveMood(), m);
            expectWithinAbsoluteError (getParam (b, "gain"), -2.5f, 0.05f);
        }
        {
            GlitchKidProcessor a, b;
            juce::MemoryBlock state;
            a.getStateInformation (state);
            b.applyMood (2);
            b.setStateInformation (state.getData(), (int) state.getSize());
            expectEquals (b.getActiveMood(), -1);
            expect (snapshot (a) == snapshot (b), "a default state restores the defaults");
        }

        beginTest ("missing parameters are skipped safely (no crash, no invalid access)");
        {
            const auto& mood = Moods::get (0);
            std::set<juce::String> known { "sat_drive", "echo_fb", "not_in_this_mood" };
            std::map<juce::String, float> got;
            const int applied = Moods::apply (mood, [&] (const char* id, float v) { if (known.count (id) == 0) return false; got[id] = v; return true; });
            expectEquals (applied, 2);
            expectEquals (got.at ("sat_drive"), 3.0f);
            expectEquals (Moods::apply (mood, [] (const char*, float) { return false; }), 0);
            // A processor without Dimension or echo parameters is simulated the same way for every Mood.
            for (int m = 0; m < Moods::count; ++m)
            {
                int dimSkipped = 0;
                Moods::apply (Moods::get (m), [&] (const char* id, float) { if (juce::String (id).startsWith ("dim_")) { ++dimSkipped; return false; } return true; });
                expectEquals (dimSkipped, 5);
            }
        }

        beginTest ("every Mood produces finite, bounded audio, mono and stereo, at several sample rates");
        for (int m = 0; m < Moods::count; ++m)
            for (double sr : { 44100.0, 48000.0, 96000.0 })
                for (int ch : { 1, 2 })
                {
                    GlitchKidProcessor p;
                    if (ch == 1) setLayout (p, juce::AudioChannelSet::mono());
                    p.prepareToPlay (sr, 512);
                    p.applyMood (m);
                    Buf in = speechLike (ch, (int) (sr * 3), 40 + m);
                    process (p, in);
                    expect (allFinite (in) && in.getMagnitude (0, in.getNumSamples()) < 4.0f,
                            juce::String (Moods::get (m).id) + " sr " + juce::String (sr) + " ch " + juce::String (ch));
                }

        beginTest ("Moods do not hide big level changes: output level within +-1.5 dB of the input on speech-like noise");
        for (int m = 0; m < Moods::count; ++m)
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 512);
            p.applyMood (m);
            Buf in = speechLike (2, 48000 * 4, 60 + m), out;
            out.makeCopyOf (in);
            process (p, out);
            const double db = juce::Decibels::gainToDecibels (out.getRMSLevel (0, 48000, 48000 * 3) / in.getRMSLevel (0, 48000, 48000 * 3));
            logMessage ("  " + juce::String (Moods::get (m).id) + ": level change " + juce::String (db, 2) + " dB");
            expect (std::abs (db) <= 1.5, juce::String (Moods::get (m).id) + " level change " + juce::String (db, 2) + " dB");
        }

        beginTest ("switching Moods during playback is smooth: finite, and no jump (a 30 Hz tone has tiny natural steps, so any unsmoothed change shows)");
        float worstRatio = 0.0f;
        for (int from = 0; from < Moods::count; ++from)
            for (int to = 0; to < Moods::count; ++to)
            {
                if (from == to) continue;
                GlitchKidProcessor p;
                p.prepareToPlay (48000.0, 256);
                p.applyMood (from);
                Buf warm = sine (48000, 48000.0, 30.0f, 0.4f);
                process (p, warm, 256);
                Buf before = sine (24000, 48000.0, 30.0f, 0.4f);
                process (p, before, 256);
                p.applyMood (to);
                Buf during = sine (48000, 48000.0, 30.0f, 0.4f);
                process (p, during, 256);
                expect (allFinite (during), "finite switching " + juce::String (from) + " -> " + juce::String (to));

                const float own = juce::jmax (maxStep (before, 1, before.getNumSamples()), maxStep (during, 36000, 48000));
                float worst = maxStep (during, 1, 36000);
                for (int ch = 0; ch < 2; ++ch) // across the block boundary where the Mood was applied
                    worst = juce::jmax (worst, std::abs (during.getSample (ch, 0) - before.getSample (ch, before.getNumSamples() - 1)));
                worstRatio = juce::jmax (worstRatio, worst / own);
                // The echo's 80 ms time glide bends the pitch of the repeats, so allow it some headroom; a jump would be ~50x.
                expect (worst <= 8.0f * own, "step switching " + juce::String (from) + " -> " + juce::String (to) + ": "
                                                 + juce::String (worst) + " vs the tone's own " + juce::String (own));
            }
        logMessage ("  worst Mood-switch step vs the tone's own step: " + juce::String (worstRatio, 2) + "x (limit 8x)");
    }
};

static MoodTests moodTests;
