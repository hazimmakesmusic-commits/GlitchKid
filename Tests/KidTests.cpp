#include "TestHelpers.h"
#include "../Source/Kid/KidDirector.h"
#include "../Source/Kid/KidSpeech.h"
#include "../Source/PluginEditor.h"

// KID, the vocal director: the words-to-proposal engine. Nothing reaches a parameter without the artist's Apply; KID never proposes Gain, Mix,
// the limiter or Bypass, and Pitch Tune's key and scale only from a reliable estimate; follow-ups reshape the same proposal; every value is inside its parameter's own range.
namespace
{
using glitchkid::kid::Director;
using glitchkid::kid::parse;

std::map<juce::String, glitchkid::kid::Change> rowsOf (const Director& d)
{
    std::map<juce::String, glitchkid::kid::Change> m;
    for (auto& r : d.proposal())
        m[r.id] = r;
    return m;
}

struct KidTests : public juce::UnitTest
{
    KidTests() : juce::UnitTest ("KID vocal director", "GlitchKid") {}

    void runTest() override
    {
        GlitchKidProcessor p;
        const auto start = p.captureSoundState();
        auto make = [&] { Director d (p.makeRangeLookup()); d.begin (start); return d; };
        auto say = [] (Director& d, const char* text) { return d.respond (parse (text)); };

        beginTest ("the allowlist is every parameter except Gain, Mix, the limiter and Bypass");
        {
            int allowed = 0, total = 0;
            for (auto* q : p.getParameters())
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (q))
                {
                    ++total;
                    allowed += Director::isAllowed (r->paramID) ? 1 : 0;
                }
            expectEquals (total, 69);
            expectEquals (allowed, 62);
            for (auto* id : { "gain", "mix", "bypass", "lim_on", "lim_ceiling", "lim_drive", "lim_release" })
                expect (! Director::isAllowed (id), id);
        }

        beginTest ("a proposal changes nothing; every value is inside its range and on its step");
        {
            auto d = make();
            say (d, "Make my vocal wider and more atmospheric");
            expect (! d.proposal().empty());
            expectWithinAbsoluteError (p.captureSoundState().get ("dbl_width", -1), start.get ("dbl_width", -1), 1e-6f, "the engine never writes the processor");
            auto lookup = p.makeRangeLookup();
            for (auto& r : d.proposal())
            {
                glitchkid::automode::Range range;
                expect (lookup (r.id, range), r.id);
                expect (r.to >= range.lo && r.to <= range.hi, r.id + " in range");
                expectWithinAbsoluteError (r.to, range.snap (r.to), 1e-4f, r.id + " on its step");
                expect (r.reason.isNotEmpty() && r.selected);
            }
        }

        beginTest ("wider -> too much, make it subtle -> keep the vocal upfront: the same take is reshaped");
        {
            auto d = make();
            say (d, "Make my vocal wider and more atmospheric");
            auto r1 = rowsOf (d);
            expect (r1.count ("dbl_width") && r1["dbl_width"].to > r1["dbl_width"].from, "wider raises Doubler width");
            expect (r1.count ("dim_width") && r1["dim_width"].to > r1["dim_width"].from, "wider raises Dimension width");
            for (auto& [id, r] : r1)
                if (id == "dbl_width" || id == "dim_width" || id == "dbl_amount" || id == "echo_fb")
                    expect (r.to >= r.from, id + " never moves against the request");
            expect (! r1.count ("echo_tone") || r1["echo_tone"].to <= r1["echo_tone"].from, "more space never brightens the echo");

            say (d, "Too much. Make it subtle");
            auto r2 = rowsOf (d);
            expect (r2["dbl_width"].to < r1["dbl_width"].to && r2["dbl_width"].to > r1["dbl_width"].from, "subtle eases the same change");
            for (auto& [id, r] : r2)
                expect (r1.count (id) > 0, "the follow-up adds no new change: " + id);
            expect (r2["dbl_width"].tag == glitchkid::kid::Change::Tag::down);

            say (d, "Keep the vocal upfront");
            auto r3 = rowsOf (d);
            auto value = [&] (const char* id) { return r3.count (id) ? r3[id].to : start.get (id, 0); };
            expect (value ("echo_mix") <= 12.0f && value ("dbl_amount") <= 28.0f && value ("dim_mix") <= 30.0f, "you stay in front");
            expect (! r3.count ("eq_b2_gain") || r3["eq_b2_gain"].to >= start.get ("eq_b2_gain", 0), "Presence is never lowered");
            expect (r3.count ("dbl_width") && r3["dbl_width"].to > start.get ("dbl_width", 0), "the width is kept");
            expect (d.kept().count ("front") == 1);
        }

        beginTest ("darker -> a little less -> keep the presence");
        {
            auto d = make();
            say (d, "Make it darker");
            auto a = rowsOf (d);
            say (d, "A little less");
            auto b = rowsOf (d);
            say (d, "Keep the presence");
            auto c = rowsOf (d);
            const float hi = start.get ("eq_hi_gain", 0);
            expect (a["eq_hi_gain"].to < hi && b["eq_hi_gain"].to < hi && b["eq_hi_gain"].to > a["eq_hi_gain"].to, "still darker, but less so");
            expect (a.count ("eq_b2_gain") && ! c.count ("eq_b2_gain") && c.count ("eq_hi_gain"), "keeping presence drops the presence cut only");
            expect (d.removed().contains ("eq_b2_gain"));
        }

        beginTest ("conflicts: wide but keep it upfront");
        {
            auto d = make();
            say (d, "Make it wide but keep it upfront");
            auto r = rowsOf (d);
            auto value = [&] (const char* id) { return r.count (id) ? r[id].to : start.get (id, 0); };
            expect (r.count ("dbl_width") > 0, "width comes from the doubles");
            expect ((value ("echo_on") < 0.5f || value ("echo_mix") <= 12.0f) && value ("dbl_amount") <= 28.0f, "echo (off, or at most 12 %) and doubles stay under you");
            expect (! r.count ("eq_b2_gain") || r["eq_b2_gain"].to >= start.get ("eq_b2_gain", 0));
        }

        beginTest ("ad-libs that sound distant");
        {
            auto d = make();
            say (d, "Make my ad-libs sound distant");
            auto r = rowsOf (d);
            expectEquals (d.role(), juce::String ("adlib"));
            expect (r.count ("echo_div") && std::abs (r["echo_div"].to - 1.0f) < 1e-6f, "1/4 throws");
            expect (r.count ("eq_lo_gain") && r["eq_lo_gain"].to < start.get ("eq_lo_gain", 0), "thinner low end");
            expect (r.count ("echo_mix") && r["echo_mix"].to > start.get ("echo_mix", 0), "more echo");
        }

        beginTest ("improve it reads only real measurements");
        {
            auto d = make();
            const auto f = parse ("I don't know what it needs. Improve it");
            expect (Director::needsAnalysis (f) && f.improve);
            glitchkid::kid::AnalysisFacts facts;
            facts.valid = true;
            facts.activeSeconds = 6.2f;
            facts.sibilanceIndex = 0.31f;
            facts.dynamicRangeDb = 13.8f;
            facts.bandFraction = { 0.06f, 0.41f, 0.31f, 0.15f, 0.07f };
            const auto line = d.respond (f, &facts);
            auto r = rowsOf (d);
            expect (r.count ("clean_amount") && r.count ("comp_ratio"), "CLEAN for the measured mud and S's, steadier compression for the measured level jumps");
            expect (! r.count ("eq_b1_freq"), "no fixed 300 Hz cut: the live read has no frequency, so CLEAN's own detector finds it");
            expect (line.contains ("6.2 s"));
            auto e = make();
            const auto noData = e.respond (f, nullptr);
            expect (e.proposal().empty() && noData.contains ("press play"), "without an analysis KID asks for audio instead of guessing");
        }

        beginTest ("never-touched parameters, honest refusals, unknown words");
        {
            std::set<juce::String> seen;
            for (auto* t : { "make it louder", "autotune it hard", "wider", "darker", "aggressive", "warm", "airy", "clean", "distant", "dreamy", "experimental",
                             "glitchy", "natural melodic rap", "ad-libs distant", "close and intimate", "softer", "narrow", "bright", "reverb please", "very bright and very wide" })
            {
                auto d = make();
                say (d, t);
                for (auto& r : d.proposal())
                    seen.insert (r.id);
                expect (d.refused().isEmpty(), juce::String ("nothing refused for: ") + t);
            }
            for (auto& id : seen)
                expect (Director::isAllowed (id), id);
            expect (seen.size() > 25, "the rules reach a wide set of controls");

            auto d = make();
            auto line = say (d, "make it louder");
            expect (d.proposal().empty() && line.contains ("Level stays yours"));
            line = say (d, "add reverb");
            expect (line.contains ("no reverb") && ! d.proposal().empty(), "reverb is answered honestly and built from Echo and Dimension");
            auto u = make();
            line = say (u, "purple elephants");
            expect (u.proposal().empty() && line.contains ("didn't catch"));
            expect (! parse ("purple elephants").understood());
        }

        beginTest ("the validator refuses ids it does not know");
        {
            Director d ([] (const juce::String&, glitchkid::automode::Range&) { return false; }); // no ranges: nothing can pass
            d.begin (start);
            d.respond (parse ("make it wider"));
            expect (d.proposal().empty() && d.refused().contains ("dbl_width"));
        }

        beginTest ("untick, manual override, apply only the ticked rows, start over");
        {
            auto d = make();
            say (d, "make it wider");
            const auto first = d.proposal().front().id;
            d.setSelected (first, false);
            expectEquals (d.selectedCount(), (int) d.proposal().size() - 1);
            expect (! d.selectedChanges().has (first), "an unticked row is not applied");

            auto edited = start;
            edited.set ("dim_width", start.get ("dim_width", 0) + 7.0f);
            d.noteManualEdit (edited);
            auto r = rowsOf (d);
            expect (r.count ("dim_width") && ! r["dim_width"].selected && r["dim_width"].tag == glitchkid::kid::Change::Tag::manual, "your move wins");

            GlitchKidProcessor q;
            const auto before = q.captureSoundState();
            Director dq (q.makeRangeLookup());
            dq.begin (before);
            dq.respond (parse ("darker and more aggressive"));
            const auto ticked = dq.selectedChanges();
            q.applyParameterChanges (ticked);
            const auto after = q.captureSoundState();
            for (auto& e : before.values)
            {
                const float want = ticked.has (e.id) ? ticked.get (e.id, 0) : e.value;
                expectWithinAbsoluteError (after.get (e.id, -999), want, 1e-3f, e.id);
            }

            say (d, "keep the vocal upfront");
            say (d, "start over");
            expect (d.kept().empty() && d.proposal().empty(), "start over forgets everything");
        }

        auto never = [this] (const Director& d, const juce::String& what)
        {
            for (auto& r : d.proposal())
                expect (r.id != "gain" && r.id != "mix" && r.id != "bypass" && ! r.id.startsWith ("lim_"), what + ": " + r.id);
        };

        beginTest ("producer profiles: by name and by artist reference (a direction, never a copy); each sets its tuning style and its character");
        {
            using glitchkid::kid::producerProfile;
            for (int i = 1; i <= glitchkid::kid::numProfiles; ++i)
            {
                const auto f = parse (juce::String ("give me the ") + producerProfile (i).name + " sound");
                expectEquals (f.profile, i, producerProfile (i).name);
                auto d = make();
                d.respond (f);
                auto r = rowsOf (d);
                static const float strength[] = { 50.0f, 75.0f, 100.0f };
                expect (r.count ("tune_on") > 0, producerProfile (i).name + juce::String (": tuning on"));
                const float got = r.count ("tune_strength") ? r["tune_strength"].to : start.get ("tune_strength", -1.0f); // no row when the value is already right
                expectWithinAbsoluteError (got, strength[producerProfile (i).tune - 1], 1e-3f, producerProfile (i).name + juce::String (": tuning style"));
                expect (! r.count ("tune_key"), "no key without a reliable estimate");
                never (d, producerProfile (i).name);
                expect (d.refused().isEmpty());
                if (i == 2) expect (r.count ("clean_on") && r.count ("echo_on") && r.count ("dbl_on"), "Atmospheric: ambience, echo and mud control");
                if (i == 3) expect (! r.count ("echo_on") && ! r.count ("mut_on"), "Intimate: no ambience, no Lab effects");
                if (i == 4) expect (r.count ("mut_on") && r["mut_on"].to > 0.5f, "Experimental: Mutation");
            }
            auto d = make();
            const auto line = say (d, "Make this vocal sound like The Kid LAROI.");
            expectEquals (d.profile(), 1);
            expect (line.contains ("can't copy") && line.contains ("Melodic Pop-Rap"), line);
            const auto unknown = parse ("make it sound like Bob Nobody");
            expect (unknown.profile == 0 && unknown.notes.joinIntoString (" ").contains ("don't have a direction"));
            const auto wide = parse ("Give me a wide melodic rap vocal");
            expect (wide.profile == 1 && wide.ops[glitchkid::kid::width] > 0.0f, "profile and words together");
        }

        beginTest ("tuning words: harder, less robotic, off; the key only from a reliable file estimate");
        {
            auto d = make();
            auto line = say (d, "Make the tuning harder");
            auto r = rowsOf (d);
            expect (r.count ("tune_on") && r.count ("tune_speed") && std::abs (r["tune_speed"].to) < 1e-3f, "hard: instant snap");
            expect (line.contains ("can't tell your key") && ! r.count ("tune_key"), line);

            const auto f = parse ("Make it more emotional and less robotic");
            expect (f.tune == 1 && f.ops[glitchkid::kid::warm] > 0.0f && f.ops[glitchkid::kid::exp] <= 0.0f, "less robotic is natural tuning, not more Mutation");

            glitchkid::kid::AnalysisFacts facts;
            facts.valid = facts.fromFile = facts.keyReliable = true;
            facts.keyTonic = 9;
            facts.keyMinor = true;
            auto k = make();
            line = k.respond (parse ("autotune it"), &facts);
            r = rowsOf (k);
            expect (r.count ("tune_key") && std::abs (r["tune_key"].to - 9.0f) < 1e-3f && std::abs (r["tune_scale"].to - 2.0f) < 1e-3f, "A natural minor from the estimate");
            expect (line.contains ("A minor") && line.contains ("estimate"), line);

            GlitchKidProcessor q;
            setParam (q, "tune_on", 1.0f);
            Director off (q.makeRangeLookup());
            off.begin (q.captureSoundState());
            off.respond (parse ("no autotune please"));
            r = rowsOf (off);
            expect (r.count ("tune_on") && r["tune_on"].to < 0.5f, "off");
        }

        beginTest ("Clean My Vocal: Gentle / Balanced / Aggressive move CLEAN only, and say what it cannot do");
        {
            const float amounts[] = { 35.0f, 60.0f, 90.0f };
            const char* words[] = { "clean my vocal gently", "clean my vocal", "clean my vocal aggressively" };
            for (int i = 0; i < 3; ++i)
            {
                auto d = make();
                const auto f = parse (words[i]);
                expectEquals (f.cleanLevel, i + 1);
                expect (! Director::needsAnalysis (f), "fixed, conservative levels: no waiting for audio");
                const auto line = d.respond (f);
                auto r = rowsOf (d);
                expect (r.count ("clean_amount") && std::abs (r["clean_amount"].to - amounts[i]) < 1e-3f, words[i]);
                for (auto& row : d.proposal())
                    expect (row.id.startsWith ("clean_"), juce::String (words[i]) + " touches only CLEAN: " + row.id);
                expect (line.contains ("isn't noise reduction"), line);
                expect (line.contains ("thin") == (i == 2), "only Aggressive warns about the tone");
            }
        }

        beginTest ("Build My Mix: the existing engine's foundation plus KID's tuning and cleanup, explainable, applied only through Apply");
        {
            glitchkid::analysis::VocalAnalysis none;
            const auto fp = glitchkid::kid::buildFoundation (1, start, p.factoryDefaultState(), none, p.makeRangeLookup());
            expect (fp.usable && fp.message.contains ("not fitted"), fp.message);
            const auto again = glitchkid::kid::buildFoundation (1, start, p.factoryDefaultState(), none, p.makeRangeLookup());
            expectEquals ((int) again.changes.size(), (int) fp.changes.size(), "deterministic");

            auto d = make();
            d.setFoundation (fp);
            expect (d.hasFoundation());
            auto f = parse ("build my mix, melodic pop rap");
            expect (f.buildMix && f.profile == 1 && Director::needsAnalysis (f));
            glitchkid::kid::AnalysisFacts facts;
            facts.valid = true;
            facts.sibilanceIndex = 0.4f;
            facts.bandFraction = { 0.05f, 0.3f, 0.35f, 0.2f, 0.1f };
            const auto line = d.respond (f, &facts);
            auto r = rowsOf (d);
            for (auto& c : fp.changes)
                expect (r.count (c.id) > 0, "foundation row " + c.id);
            expect (r.count ("tune_on") && r.count ("clean_on"), "tuning from the profile, gentle cleanup from the sibilance");
            expect (d.summary().startsWith ("Added smooth pitch correction") && line.contains (d.summary()), d.summary());
            never (d, "build my mix");

            GlitchKidProcessor q;
            q.applyParameterChanges (d.selectedChanges());
            expectWithinAbsoluteError (getParam (q, "gain"), getParam (p, "gain"), 1e-6f);
            expectWithinAbsoluteError (getParam (q, "mix"), getParam (p, "mix"), 1e-6f);
            d.accepted (q.captureSoundState());
            expect (! d.hasFoundation() && d.proposal().empty() && d.profile() == 0, "Apply ends the build");
        }

        // a third-octave tone shape (dB relative to the mean) for the diagnosis tests: a gentle natural slope, then whatever the test adds
        auto shape = [] (std::initializer_list<std::pair<int, float>> bumps)
        {
            std::array<float, glitchkid::analysis::SpectralProfile::numBands> s {};
            for (int b = 0; b < (int) s.size(); ++b)
                s[(size_t) b] = 6.0f - 1.2f * (float) b; // about -3.6 dB per octave
            for (auto [b, db] : bumps)
                s[(size_t) b] += db;
            return s;
        };
        auto fileFacts = [&] (std::initializer_list<std::pair<int, float>> bumps)
        {
            glitchkid::kid::AnalysisFacts f;
            f.valid = f.fromFile = f.shapeKnown = true;
            f.activeSeconds = 12.0f;
            f.peakDb = -6.0f;
            f.rmsDb = -20.0f;
            f.crestDb = 14.0f;
            f.dynamicRangeDb = 8.0f;
            f.bandFraction = { 0.04f, 0.3f, 0.4f, 0.18f, 0.08f };
            f.shapeDb = shape (bumps);
            return f;
        };

        beginTest ("diagnosis: measured vs estimated, frequencies only where the analysis supports them, broad vs narrow harshness");
        {
            const auto none = glitchkid::kid::diagnose (fileFacts ({}));
            expect (! none.has ("harsh") && ! none.has ("mud"), "a plain slope has nothing to fix");
            const auto broad = glitchkid::kid::diagnose (fileFacts ({ { 10, 10.0f }, { 11, 10.0f }, { 12, 10.0f }, { 13, 10.0f }, { 14, 10.0f } }));
            const auto* h = broad.find ("harsh");
            expect (h != nullptr && ! h->narrow && ! h->measured && h->hz >= 2000.0f && h->hz <= 5100.0f, "broad, forward upper mids (an estimate)");
            const auto narrow = glitchkid::kid::diagnose (fileFacts ({ { 10, 8.0f }, { 11, 8.0f }, { 12, 16.0f }, { 13, 8.0f }, { 14, 8.0f } }));
            const auto* n = narrow.find ("harsh");
            expect (n != nullptr && n->narrow && std::abs (n->hz - glitchkid::analysis::SpectralProfile::bandCentreHz (12)) < 1.0f, "concentrated at 3.2 kHz");
            const auto mud = glitchkid::kid::diagnose (fileFacts ({ { 2, 9.0f } }));
            const auto* m = mud.find ("mud");
            expect (m != nullptr && std::abs (m->hz - glitchkid::analysis::SpectralProfile::bandCentreHz (2)) < 1.0f && m->detail.contains ("above your voice's own slope"), "mud located where it stands out");
            auto live = fileFacts ({});
            live.fromFile = live.shapeKnown = false;
            live.bandFraction = { 0.05f, 0.45f, 0.25f, 0.2f, 0.05f };
            const auto* lm = glitchkid::kid::diagnose (live).find ("mud");
            expect (lm != nullptr && lm->hz <= 0.0f, "the live five-band read claims no frequency");
            auto rec = fileFacts ({});
            rec.clippedSamples = 40;
            rec.noiseKnown = true;
            rec.noiseFloorDb = -42.0f;
            const auto r = glitchkid::kid::diagnose (rec);
            expect (r.has ("clipping") && r.find ("clipping")->measured && r.find ("clipping")->plain.contains ("can't repair"), "clipping is a recording problem");
            expect (r.has ("noise") && ! r.find ("noise")->measured && r.find ("noise")->plain.contains ("no noise reduction"), "noise is an estimate, and there is no noise reduction");
            expect (glitchkid::kid::diagnose ({}).empty(), "no analysis, no findings");
        }

        beginTest ("the plan fits the recording: corrections with evidence, and taste adapted to what is already there");
        {
            auto d = make();
            auto broadFacts = fileFacts ({ { 10, 10.0f }, { 11, 10.0f }, { 12, 10.0f }, { 13, 10.0f }, { 14, 10.0f } });
            d.respond (parse ("build my mix"), &broadFacts);
            auto r = rowsOf (d);
            expect (r.count ("eq_b2_gain") && r["eq_b2_gain"].to < -1.0f && r.count ("eq_b2_q") && r["eq_b2_q"].to < 1.0f, "broad harshness: a wide cut");
            expect (r["eq_b2_freq"].detail.contains ("2-5 kHz sits"), "the reason carries the measurement");

            auto n = make();
            auto narrowFacts = fileFacts ({ { 10, 8.0f }, { 11, 8.0f }, { 12, 16.0f }, { 13, 8.0f }, { 14, 8.0f } });
            n.respond (parse ("build my mix"), &narrowFacts);
            r = rowsOf (n);
            expect (r.count ("eq_b2_q") && r["eq_b2_q"].to > 2.0f && std::abs (r["eq_b2_freq"].to - glitchkid::analysis::SpectralProfile::bandCentreHz (12)) < 30.0f,
                    "narrow harshness: a narrower cut where it was found");
            n.respond (parse ("make it brighter"));
            r = rowsOf (n);
            expect (r["eq_b2_gain"].to < 0.0f, "brighter never lifts upper mids that were diagnosed sharp");

            auto b = make();
            auto brightFacts = fileFacts ({ { 16, 30.0f }, { 17, 30.0f }, { 18, 30.0f } });
            b.respond (parse ("make it brighter"), &brightFacts);
            auto plain = make();
            plain.respond (parse ("make it brighter"));
            expect (rowsOf (b)["eq_hi_gain"].to < rowsOf (plain)["eq_hi_gain"].to, "an already-bright vocal gets less top");

            auto c = make();
            auto squashed = fileFacts ({});
            squashed.crestDb = 7.0f;
            squashed.dynamicRangeDb = 3.0f;
            c.respond (parse ("make it punchy"), &squashed);
            expect (rowsOf (c)["comp_ratio"].to <= 2.5f + 1e-3f, "a compressed take gets light compression");

            for (auto* dd : { &d, &n, &b, &c })
            {
                never (*dd, "fitted plan");
                expect (dd->refused().isEmpty());
            }
        }

        beginTest ("feedback reshapes the same mix: less reverb, more raw, more like the reference, keep the centre strong");
        {
            auto d = make();
            say (d, "Make the reverb more atmospheric");
            const float wet = rowsOf (d)["echo_mix"].to;
            expect (wet > 10.0f);
            say (d, "less reverb");
            auto r = rowsOf (d);
            expect (r.count ("echo_mix") && r["echo_mix"].to < wet && r["echo_mix"].to > 0.0f, "about half, not off");
            say (d, "Kid, reduce the reverb by about half");
            expect (rowsOf (d)["echo_mix"].to < r["echo_mix"].to, "and again");

            auto f = make();
            f.setFoundation (glitchkid::kid::buildFoundation (1, start, p.factoryDefaultState(), {}, p.makeRangeLookup()));
            say (f, "make me sound like the kid laroi");
            const float profileEcho = rowsOf (f)["echo_mix"].to;
            say (f, "less reverb");
            expect (rowsOf (f)["echo_mix"].to < profileEcho, "less of the profile's own echo, even though KID never added any");
            say (f, "more like that reference");
            say (f, "less polished, more raw");
            expectEquals (f.tuneStyle(), 1, "raw means natural tuning");
            expect (f.natural());
            say (f, "make it wider but keep the center strong");
            expect (f.kept().count ("front") > 0);
            say (f, "start over");
            expect (f.proposal().empty(), "start over is the sound we started from");
        }

        beginTest ("questions are answered from the real plan and analysis, and never change the sound");
        {
            auto d = make();
            auto f = fileFacts ({ { 10, 10.0f }, { 11, 10.0f }, { 12, 10.0f }, { 13, 10.0f }, { 14, 10.0f } });
            d.respond (parse ("build my mix"), &f);
            const auto rows = d.proposal().size();
            const auto q = parse ("Why did you reduce the high mids?");
            expect (q.ask == glitchkid::kid::IntentFrame::Ask::why && q.topic == "harsh" && ! q.anyOps(), "a why question, about the upper mids");
            const auto why = d.respond (q);
            expect (why.contains ("Upper mids eased") && why.contains ("2-5 kHz sits"), why);
            expectEquals ((int) d.proposal().size(), (int) rows, "a question changes nothing");
            expect (d.respond (parse ("why did you change the tuning?")).contains ("didn't change the tuning"));
            expect (d.respond (parse ("Kid, explain what you changed")).contains ("The main reasons"));
            const auto a = d.respond (parse ("analyze my vocal"));
            expect (a.contains ("Measured:") && a.contains ("Estimate:"), a);
            expect (d.respond (parse ("what key is it in?")).contains ("can't tell the key"), "no reliable key, no guess");
            auto e = make();
            expect (e.respond (parse ("analyze my vocal")).contains ("haven't heard"));
            expect (Director::needsAnalysis (parse ("analyze my vocal")));
        }

        beginTest ("the plan as a whole sound: the start sound with the plan on top; level, the limiter and controls moved by hand stay as they are");
        {
            auto d = make();
            say (d, "wider and warmer");
            auto now = start;
            now.set ("gain", 4.0f);
            now.set ("sat_drive", 9.0f);
            const auto target = d.plan (now, { "sat_drive" });
            expectWithinAbsoluteError (target.get ("gain", 0.0f), 4.0f, 1e-6f, "Gain is yours");
            expectWithinAbsoluteError (target.get ("sat_drive", 0.0f), 9.0f, 1e-6f, "a hand-moved control is left alone");
            expect (target.get ("dbl_on", 0.0f) > 0.5f, "the plan is in it");
        }
    }
};
KidTests kidTests;

// KID's speech plumbing: the model is verified before use, the command audio is resampled for the recogniser, and every failure is an honest message.
struct KidSpeechTests : public juce::UnitTest
{
    KidSpeechTests() : juce::UnitTest ("KID speech", "GlitchKid") {}

    void runTest() override
    {
        using namespace glitchkid::kid;
        beginTest ("SHA-1 matches the FIPS 180 test vectors");
        {
            auto hash = [] (const juce::String& s) { juce::MemoryInputStream in (s.toRawUTF8(), s.getNumBytesAsUTF8(), false); return sha1Hex (in); };
            expectEquals (hash ("abc"), juce::String ("a9993e364706816aba3e25717850c26c9cd0d89d"));
            expectEquals (hash (""), juce::String ("da39a3ee5e6b4b0d3255bfef95601890afd80709"));
            expectEquals (hash ("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"), juce::String ("84983e441c3bd26ebaae4aa1f95129e5e54670f1"));
            juce::MemoryBlock million (1000000, false);
            million.fillWith ((juce::uint8) 'a');
            juce::MemoryInputStream in (million, false);
            expectEquals (sha1Hex (in), juce::String ("34aa973cd4c4daa4f61eeb2bdbad27316534016f"));
        }

        beginTest ("a model file that does not match the published checksum is refused");
        {
            juce::TemporaryFile tmp;
            tmp.getFile().replaceWithText ("not a speech model");
            juce::String error;
            expect (! ModelDownloader::verify (tmp.getFile(), error));
            expect (error.contains ("checksum"));
            expect (SpeechModel::file().getFileName() == "ggml-base.en.bin" && SpeechModel::folder().getFullPathName().contains ("GlitchKid"));
        }

        beginTest ("command audio is resampled to 16 kHz for the recogniser");
        {
            std::vector<float> s48 (48000);
            for (size_t i = 0; i < s48.size(); ++i)
                s48[i] = (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * (double) i / 48000.0);
            const auto out = CommandMic::resampleTo16k (s48.data(), (int) s48.size(), 48000.0);
            expectEquals ((int) out.size(), 16000);
            float peak = 0.0f;
            for (auto v : out) peak = juce::jmax (peak, std::abs (v));
            expect (peak > 0.9f && peak < 1.05f, "the tone survives resampling");
            expect (CommandMic::resampleTo16k (nullptr, 0, 48000.0).empty());
        }

        beginTest ("without a model, speech fails with a message and never crashes");
        {
            if (SpeechModel::isInstalled())
                logMessage ("(a speech model is installed on this machine: the no-model path is not exercised)");
            else
            {
                Recogniser r;
                juce::String text, error = "pending";
                r.transcribe (std::vector<float> (16000, 0.0f), [&] (juce::String t, juce::String e) { text = t; error = e; });
                for (int i = 0; i < 100 && error == "pending"; ++i)
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
                expect (text.isEmpty() && error.isNotEmpty() && error != "pending", error);
            }
            Recogniser shortOne;
            juce::String err = "pending";
            shortOne.transcribe (std::vector<float> (100, 0.0f), [&] (juce::String, juce::String e) { err = e; });
            for (int i = 0; i < 100 && err == "pending"; ++i)
                juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
            expect (err.isNotEmpty() && err != "pending", "too-short audio is reported, not transcribed");
        }
    }
};
KidSpeechTests kidSpeechTests;

// KID inside the real editor: the whole window, four areas, a mix that is heard straight away, feedback, questions, hand moves respected, step-by-step undo.
struct KidEditorTests : public juce::UnitTest
{
    KidEditorTests() : juce::UnitTest ("KID page", "GlitchKid") {}

    static juce::File writeLine (const juce::File& f)
    {
        juce::WavAudioFormat wav;
        f.deleteFile();
        std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
        auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (48000.0).withNumChannels (1).withBitsPerSample (24));
        juce::AudioBuffer<float> b (1, 48000 * 14);
        const int notes[] = { 57, 60, 64, 62, 60, 59, 57, 55, 53, 52, 57, 64, 60, 57 };
        double ph = 0.0;
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const double hz = 440.0 * std::pow (2.0, (notes[i / 48000] - 69) / 12.0);
            ph += juce::MathConstants<double>::twoPi * hz / 48000.0;
            b.setSample (0, i, 0.25f * (float) (std::sin (ph) + 0.5 * std::sin (2.0 * ph) + 0.33 * std::sin (3.0 * ph)));
        }
        if (w != nullptr) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        return f;
    }

    void runTest() override
    {
        GlitchKidProcessor p;
        p.prepareToPlay (48000.0, 480);
        std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
        auto* ed = dynamic_cast<GlitchKidEditor*> (e.get());
        ed->uiShowKidForTest (true); // the instrument is the first view; Kid is one click away
        auto& kid = ed->uiKidPanelForTest();
        using Status = glitchkid::ui::KidPanel::Status;
        auto shot = [&] (const juce::String& name)
        {
            const auto dir = juce::SystemStats::getEnvironmentVariable ("GLITCHKID_SNAPSHOT_DIR", {});
            if (dir.isEmpty()) return;
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::File f = juce::File (dir).getChildFile (name + ".png");
            f.deleteFile();
            juce::FileOutputStream os (f);
            if (os.openedOk()) juce::PNGImageFormat().writeImageToStream (img, os);
        };
        std::function<bool (juce::Component&, const juce::String&)> visibleButton = [&] (juce::Component& c, const juce::String& text)
        {
            for (auto* ch : c.getChildren())
                if (ch->isVisible() && ((dynamic_cast<juce::Button*> (ch) != nullptr && dynamic_cast<juce::Button*> (ch)->getButtonText() == text) || visibleButton (*ch, text)))
                    return true;
            return false;
        };
        auto same = [] (const glitchkid::SoundState& a, const glitchkid::SoundState& b)
        {
            for (auto& v : a.values)
                if (std::abs (b.get (v.id, -999) - v.value) > 1e-4f) return false;
            return true;
        };

        beginTest ("the main screen: the whole window, four areas, one main action, the detailed pages one click away");
        {
            expectEquals (ed->uiRailIndexForTest(), 0, "KID is the first page");
            expect (kid.isVisible() && kid.getParentComponent() != nullptr && kid.getBounds() == kid.getParentComponent()->getLocalBounds(), "Kid fills the window");
            expect (visibleButton (*ed, "BUILD MY MIX") && visibleButton (*ed, "Load vocal") && visibleButton (*ed, "Original") && visibleButton (*ed, "Processed"));
            expect (! visibleButton (*ed, "TUNE") && ! visibleButton (*ed, "SAVE") && ! visibleButton (*ed, "MOTION"), "no rail, preset bar or header buttons on this screen");
            expect (kid.statusForTest() == Status::ready && kid.statusTextForTest() == "Ready");
            if (! glitchkid::kid::SpeechModel::isInstalled())
                expect (! kid.micEnabledForTest(), "without the speech model the mic is off and typing still works");
            shot ("kid_ready");
            {
                juce::Image img (juce::Image::ARGB, kid.getWidth(), kid.getHeight(), true);
                juce::Graphics g (img);
                const auto t0 = juce::Time::getMillisecondCounterHiRes();
                for (int i = 0; i < 30; ++i)
                {
                    kid.tickForTest();
                    kid.paintEntireComponent (g, true);
                }
                const double ms = (juce::Time::getMillisecondCounterHiRes() - t0) / 30.0;
                logMessage ("  KID screen: " + juce::String (ms, 2) + " ms per full repaint (software renderer)");
                expect (ms < 30.0, "a full repaint stays cheap: " + juce::String (ms, 2) + " ms");
            }
            kid.pressForTest ("Tune");
            expectEquals (ed->uiRailIndexForTest(), 4, "Advanced: Tune opens the TUNE page");
            expect (visibleButton (*ed, "MAIN") && visibleButton (*ed, "SAVE"), "the rail and the preset bar are back on the detailed pages");
            ed->uiSelectRailForTest (0);
            expect (kid.isVisible() && ! visibleButton (*ed, "SAVE"));
        }

        beginTest ("a vocal file: analysed off the audio thread, then Kid says what it found");
        juce::TemporaryFile tmp (".wav");
        {
            kid.loadFileForTest (writeLine (tmp.getFile()));
            expect (kid.statusForTest() == Status::analyzing);
            for (int i = 0; i < 500 && kid.fileBusyForTest(); ++i)
                juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
            expect (kid.fileReportForTest().ok(), kid.fileReportForTest().message);
            expect (kid.lastKidLineForTest().startsWith ("I've got"), kid.lastKidLineForTest());
            expect (kid.directorForTest().facts().fromFile && kid.directorForTest().facts().shapeKnown, "the tone shape reaches Kid");
            shot ("kid_file");
        }

        const auto before = p.captureSoundState();
        beginTest ("a request becomes a mix you hear straight away; Gain, Mix and Bypass stay yours");
        {
            kid.sendForTest ("Make me sound like The Kid LAROI");
            expect (kid.builtForTest() && kid.mainActionTextForTest() == "UPDATE MY MIX");
            expect (kid.lastKidLineForTest().contains ("can't copy"), kid.lastKidLineForTest());
            expect (! same (before, p.captureSoundState()), "the mix is applied");
            expect (getParam (p, "tune_on") > 0.5f, "smooth tuning is on");
            for (auto* id : { "gain", "mix", "lim_on" })
                expectWithinAbsoluteError (getParam (p, id), before.get (id, -1.0f), 1e-6f, id);
            expect (getParam (p, "bypass") < 0.5f, "Bypass untouched");
            expectEquals (kid.undoDepthForTest(), 1);
            shot ("kid_build");
        }

        beginTest ("feedback updates the same mix; questions change nothing; controls moved by hand are left alone");
        {
            const float echo = getParam (p, "echo_mix");
            kid.sendForTest ("less reverb");
            expect (getParam (p, "echo_mix") < echo || getParam (p, "echo_on") < 0.5f, "less echo");
            const auto now = p.captureSoundState();
            kid.sendForTest ("Why did you change the tuning?");
            expect (kid.lastKidLineForTest().containsIgnoreCase ("tuning"), kid.lastKidLineForTest());
            expect (same (now, p.captureSoundState()), "a question changes nothing");
            kid.sendForTest ("purple elephants");
            expect (kid.lastKidLineForTest().contains ("didn't catch") && same (now, p.captureSoundState()));

            setParam (p, "sat_drive", 11.0f);
            for (int i = 0; i < 6; ++i) kid.tickForTest();
            expect (kid.manualForTest().count ("sat_drive") > 0, "noticed the hand move");
            kid.sendForTest ("make it warmer and more aggressive");
            expectWithinAbsoluteError (getParam (p, "sat_drive"), 11.0f, 0.06f, "your setting wins");
            expect (kid.lastKidLineForTest().contains ("moved by hand"), kid.lastKidLineForTest());
            shot ("kid_chat");
        }

        beginTest ("before / after at matched loudness, and step-by-step undo back to your sound");
        {
            kid.compareForTest (true);
            expect (getParam (p, "bypass") > 0.5f && p.isLevelMatchedCompare());
            kid.compareForTest (false);
            while (kid.undoDepthForTest() > 0)
                kid.pressForTest ("Undo");
            setParam (p, "sat_drive", before.get ("sat_drive", 0.0f));
            expect (same (before, p.captureSoundState()), "every Kid step undone: your sound exactly");
            expect (! kid.builtForTest() && kid.mainActionTextForTest() == "BUILD MY MIX");
        }

        beginTest ("no file: Kid listens to the live input first, then builds; a missing file is an honest error");
        {
            GlitchKidProcessor q;
            q.prepareToPlay (48000.0, 480);
            std::unique_ptr<juce::AudioProcessorEditor> e2 (q.createEditor());
            dynamic_cast<GlitchKidEditor*> (e2.get())->uiShowKidForTest (true);
            auto& k2 = dynamic_cast<GlitchKidEditor*> (e2.get())->uiKidPanelForTest();
            k2.sendForTest ("I don't know what it needs. Improve it");
            expect (k2.waitingForTest() && k2.statusForTest() == Status::listening, "waiting for the vocal");
            juce::MidiBuffer midi;
            juce::Random rng (7);
            for (int b = 0; b < 1000 && k2.waitingForTest(); ++b) // a sung-like tone with noise, played through the plug-in
            {
                juce::AudioBuffer<float> buf (2, 480);
                for (int i = 0; i < 480; ++i)
                {
                    const double t = (double) (b * 480 + i) / 48000.0;
                    const float v = 0.3f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * t) * (0.6f + 0.4f * (float) std::sin (t * 9.0)) + 0.02f * (rng.nextFloat() - 0.5f);
                    buf.setSample (0, i, v);
                    buf.setSample (1, i, v);
                }
                q.processBlock (buf, midi);
                if (b % 10 == 0) k2.tickForTest();
            }
            k2.tickForTest();
            expect (! k2.waitingForTest() && k2.builtForTest(), "the real analysis finished and the mix was built");
            expect (k2.lastKidLineForTest().contains ("I listened to"), k2.lastKidLineForTest());
            k2.loadFileForTest (tmp.getFile().getSiblingFile ("does-not-exist.wav"));
            for (int i = 0; i < 200 && k2.fileBusyForTest(); ++i)
                juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
            expect (k2.lastKidLineForTest().contains ("could not be found"), k2.lastKidLineForTest());
        }
        e.reset();
        expect (! p.isLevelMatchedCompare(), "closing the editor leaves bypass exact");
    }
};
KidEditorTests kidEditorTests;
}
