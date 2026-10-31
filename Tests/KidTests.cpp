#include "TestHelpers.h"
#include "../Source/Kid/KidDirector.h"
#include "../Source/Kid/KidSpeech.h"
#include "../Source/PluginEditor.h"

// KID, the vocal director: the words-to-proposal engine. Nothing reaches a parameter without the artist's Apply; KID never proposes tuning, Gain, Mix,
// the limiter or Bypass; follow-ups reshape the same proposal; every value is inside its parameter's own range.
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

        beginTest ("the allowlist is every parameter except Pitch Tune, Gain, Mix, the limiter and Bypass");
        {
            int allowed = 0, total = 0;
            for (auto* q : p.getParameters())
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (q))
                {
                    ++total;
                    allowed += Director::isAllowed (r->paramID) ? 1 : 0;
                }
            expectEquals (total, 69);
            expectEquals (allowed, 57);
            for (auto* id : { "tune_on", "tune_key", "gain", "mix", "bypass", "lim_on", "lim_ceiling" })
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
            expect (r.count ("clean_amount") && r.count ("eq_b1_gain") && r.count ("comp_thresh"), "cleaning, a mud cut and steadier compression");
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
            expect (u.proposal().empty() && line.contains ("didn't hear"));
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

// KID inside the real editor: the first page, nothing changes before Apply, Apply goes through the parameters, UNDO restores, the real analyser feeds "improve it".
struct KidEditorTests : public juce::UnitTest
{
    KidEditorTests() : juce::UnitTest ("KID page", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("type, review, Apply, UNDO; the analyser feeds improve it");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 480);
            std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
            auto* ed = dynamic_cast<GlitchKidEditor*> (e.get());
            expectEquals (ed->uiRailIndexForTest(), 0, "KID is the first page");
            auto& kid = ed->uiKidPanelForTest();
            expect (kid.isVisible() && kid.getParentComponent() != nullptr);
            const auto before = p.captureSoundState();
            auto same = [&] (const glitchkid::SoundState& a, const glitchkid::SoundState& b)
            {
                for (auto& v : a.values)
                    if (std::abs (b.get (v.id, -999) - v.value) > 1e-4f) return false;
                return true;
            };

            std::function<bool (juce::Component&, const juce::String&)> visibleButton = [&] (juce::Component& c, const juce::String& text)
            {
                for (auto* ch : c.getChildren())
                    if (ch->isVisible() && ((dynamic_cast<juce::TextButton*> (ch) != nullptr && dynamic_cast<juce::TextButton*> (ch)->getButtonText() == text) || visibleButton (*ch, text)))
                        return true;
                return false;
            };
            expect (! visibleButton (*ed, "RESET") && ! visibleButton (*ed, "HARD"), "no other workspace shows through the KID page");
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
            shot ("kid_ready");
            kid.sendForTest ("Make my vocal wider and more atmospheric");
            expect (! kid.directorForTest().proposal().empty(), "a take is proposed");
            kid.sendForTest ("Too much. Make it subtle");
            kid.sendForTest ("Keep the vocal upfront");
            shot ("kid_take");
            expect (same (before, p.captureSoundState()), "nothing changes before Apply");
            expectEquals (kid.phaseNameForTest(), juce::String ("READY TO APPLY"));
            const auto firstId = kid.directorForTest().proposal().front().id;
            kid.setSelectedForTest (firstId, false);
            const auto ticked = kid.directorForTest().selectedChanges();
            kid.applyForTest();
            const auto after = p.captureSoundState();
            expect (std::abs (after.get (firstId, -1) - before.get (firstId, -1)) < 1e-4f, "the unticked change is not applied");
            for (auto& v : ticked.values)
                expectWithinAbsoluteError (after.get (v.id, -999), v.value, 1e-3f, v.id);
            expect (ed->uiCanUndoForTest(), "UNDO is armed");
            ed->uiUndoForTest();
            expect (same (before, p.captureSoundState()), "UNDO restores the sound exactly");
            for (int i = 0; i < 7; ++i) kid.tickForTest();
            expect (kid.directorForTest().proposal().empty() && kid.phaseNameForTest().startsWith ("READY"), "KID starts again from the restored sound");

            if (! glitchkid::kid::SpeechModel::isInstalled())
                expect (! kid.micEnabledForTest(), "without the speech model the mic is off and typing still works");

            kid.sendForTest ("I don't know what it needs. Improve it");
            expectEquals (kid.phaseNameForTest(), juce::String ("ANALYSING YOUR VOCAL"));
            juce::MidiBuffer midi;
            juce::Random rng (7);
            for (int b = 0; b < 1000 && kid.phaseNameForTest() == "ANALYSING YOUR VOCAL"; ++b) // a sung-like tone with noise, played through the plug-in
            {
                juce::AudioBuffer<float> buf (2, 480);
                for (int i = 0; i < 480; ++i)
                {
                    const double t = (double) (b * 480 + i) / 48000.0;
                    const float v = 0.3f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * t) * (0.6f + 0.4f * (float) std::sin (t * 9.0)) + 0.02f * (rng.nextFloat() - 0.5f);
                    buf.setSample (0, i, v);
                    buf.setSample (1, i, v);
                }
                p.processBlock (buf, midi);
                if (b % 10 == 0) kid.tickForTest();
            }
            kid.tickForTest();
            expect (kid.phaseNameForTest() != "ANALYSING YOUR VOCAL", "the real analysis finished");
            expect (kid.lastKidLineForTest().contains ("I listened to"), kid.lastKidLineForTest());
            shot ("kid_improve");
            expect (same (before, p.captureSoundState()), "still nothing changed before Apply");
        }
    }
};
KidEditorTests kidEditorTests;
}
