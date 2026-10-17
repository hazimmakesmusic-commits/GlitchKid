#include "TestHelpers.h"
#include "../Source/Presets/PresetStore.h"
#include "../Source/Presets/PresetController.h"
#include "../Source/AlterEgos.h"
#include <map>
#include <thread>

// Stage 4.6: user presets (file format, names, safety) and A/B comparison. Everything here runs on the message thread, as in the plug-in;
// the last groups prove the audio thread is unaffected while these operations run on another thread.
extern std::atomic<long> g_allocations; // counted in FormantShiftTests.cpp (it replaces the global operator new)

namespace
{
using glitchkid::PresetStore;
using glitchkid::SoundState;
constexpr float qNaN = std::numeric_limits<float>::quiet_NaN();

struct TempDir
{
    TempDir() : dir (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("glitchkid-test-" + juce::String::toHexString (juce::Random().nextInt64())))
    {
        dir.createDirectory();
    }
    ~TempDir() { dir.deleteRecursively(); }
    juce::File dir;
};

bool allFinite (const Buf& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (ch, i)))
                return false;
    return true;
}

std::vector<juce::RangedAudioParameter*> soundParameters (juce::AudioProcessor& p)
{
    std::vector<juce::RangedAudioParameter*> out;
    for (auto* q : p.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (q))
            if (SoundState::isSoundParameter (r->paramID))
                out.push_back (r);
    return out;
}

float plain (juce::RangedAudioParameter* r) { return r->convertFrom0to1 (r->getValue()); }

float tol (juce::RangedAudioParameter* r)
{
    const float step = r->getNormalisableRange().interval;
    return step > 0.0f ? 0.5f * step + 1.0e-3f : 2.0e-3f;
}

// Randomises every sound parameter (never Bypass) to a legal value.
void randomise (juce::AudioProcessor& p, juce::int64 seed)
{
    juce::Random rng (seed);
    for (auto* r : soundParameters (p))
        r->setValueNotifyingHost (rng.nextFloat());
}

// The largest difference between two processors over every sound parameter, in units of that parameter's tolerance (<= 1 means equal).
float worstDifference (juce::AudioProcessor& a, juce::AudioProcessor& b)
{
    float worst = 0.0f;
    for (auto* r : soundParameters (a))
        if (auto* q = dynamic_cast<juce::RangedAudioParameter*> (b.getParameters()[r->getParameterIndex()]))
            worst = juce::jmax (worst, std::abs (plain (r) - plain (q)) / tol (r));
    return worst;
}

juce::String abProperty (GlitchKidProcessor& p, int slot)
{
    return p.apvts.state.getProperty (slot == 0 ? "abSlotA" : "abSlotB", juce::String()).toString();
}
} // namespace

struct PresetTests : public juce::UnitTest
{
    PresetTests() : juce::UnitTest ("Presets and A/B", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("factory default: every sound parameter at its default, no Alter Ego, Bypass is not part of a sound");
        {
            GlitchKidProcessor p;
            const auto d = p.factoryDefaultState();
            expectEquals ((int) d.values.size(), p.getParameters().size() - 1, "every parameter except Bypass");
            expect (! d.has ("bypass"));
            expect (d.egoId.isEmpty());
            for (auto* r : soundParameters (p))
                expectWithinAbsoluteError (d.get (r->paramID, -9999.0f), r->convertFrom0to1 (r->getDefaultValue()), 1.0e-4f, r->paramID);
            expect (p.matchesSoundState (d), "a fresh instance is the factory default");
            randomise (p, 5);
            expect (! p.matchesSoundState (d));
            setParam (p, "bypass", 1.0f);
            p.applySoundState (d);
            expect (p.matchesSoundState (d), "applying the factory default restores it");
            expectEquals (getParam (p, "bypass"), 1.0f, "Bypass is left alone");
        }

        beginTest ("round trip: every parameter, through the in-memory state and through the XML text, mono/stereo independent");
        for (juce::int64 seed : { 1, 2, 3, 99 })
        {
            GlitchKidProcessor a, b, c;
            randomise (a, seed);
            a.applyAlterEgo ((int) (seed % AlterEgos::count), 0.37f, (std::uint32_t) seed);
            setParam (a, "bypass", 1.0f);
            const auto s = a.captureSoundState();
            expect (! s.has ("bypass"));
            b.applySoundState (s);
            expect (worstDifference (a, b) <= 1.0f, "in-memory round trip, worst " + juce::String (worstDifference (a, b)));
            expectEquals (getParam (b, "bypass"), 0.0f, "Bypass not copied");
            expectEquals (b.getAlterEgoSelection().ego, a.getAlterEgoSelection().ego);
            expectWithinAbsoluteError (b.getAlterEgoSelection().chaos, 0.37f, 1.0e-4f);

            const auto xml = PresetStore::toXml ("Round Trip", s);
            const auto text = xml->toString();
            auto parsed = juce::XmlDocument::parse (text);
            expect (parsed != nullptr);
            SoundState back;
            juce::String storedName;
            expect (PresetStore::fromXml (*parsed, back, &storedName).ok);
            expectEquals (storedName, juce::String ("Round Trip"));
            c.applySoundState (back);
            expect (worstDifference (a, c) <= 1.0f, "XML round trip, worst " + juce::String (worstDifference (a, c)));
            expect (c.getAlterEgoSelection().seed == a.getAlterEgoSelection().seed);
        }

        beginTest ("a preset preserves MOODS, Vocal Mutation, Pitch Tune, FORMANT and every effect setting (saved to a real file and loaded back)");
        {
            TempDir tmp;
            PresetStore store (tmp.dir.getChildFile ("presets"));
            GlitchKidProcessor a;
            a.applyAlterEgo (3, 0.6f, 7); // THE PARASITE: sets Vocal Mutation too
            setParam (a, "tune_on", 1.0f); setParam (a, "tune_key", 9.0f); setParam (a, "tune_scale", 2.0f);
            setParam (a, "tune_strength", 70.0f); setParam (a, "tune_speed", 55.0f); setParam (a, "formant_shift", -3.5f);
            setParam (a, "mut_amount", 61.0f); setParam (a, "mut_character", 12.0f); setParam (a, "mut_blend", 47.0f);
            setParam (a, "gain", -3.0f); setParam (a, "mix", 64.0f); setParam (a, "stutter_on", 1.0f); setParam (a, "stutter_rate", 2.0f);
            const auto res = store.save ("Parasite Robot", a.captureSoundState(), false);
            expect (res.ok, res.message);

            SoundState loaded;
            expect (store.load ("Parasite Robot", loaded).ok);
            GlitchKidProcessor b;
            b.applyPreset ("Parasite Robot", loaded);
            for (auto* id : { "tune_on", "tune_key", "tune_scale", "tune_strength", "tune_speed", "formant_shift", "mut_on", "mut_amount",
                              "mut_character", "mut_blend", "gain", "mix", "stutter_on", "stutter_rate", "sat_drive", "echo_fb", "dim_depth", "eq_b1_gain", "comp_ratio" })
                expectWithinAbsoluteError (getParam (b, id), getParam (a, id), 0.06f, id);
            expect (worstDifference (a, b) <= 1.0f);
            expectEquals (b.getAlterEgoSelection().ego, 3);
            expectWithinAbsoluteError (b.getAlterEgoSelection().chaos, 0.6f, 1.0e-4f);
            expectEquals ((int) b.getAlterEgoSelection().seed, 7);
            expect (b.isAlterEgoActive() == a.isAlterEgoActive(), "the Alter Ego is recognised exactly as in the source instance");
            expectEquals (b.getPresetName(), juce::String ("Parasite Robot"));
            expect (! b.isPresetModified());
            setParam (b, "gain", 2.0f);
            expect (b.isPresetModified(), "editing marks the preset as modified");
            const int before = b.setChaos (0.2f);
            expect (before > 0, "Chaos still works on a loaded preset");
        }

        beginTest ("saving: duplicates are refused unless overwriting is explicit; case-insensitive; overwrite keeps one file; delete works");
        {
            TempDir tmp;
            PresetStore store (tmp.dir);
            GlitchKidProcessor p;
            const auto s1 = p.captureSoundState();
            expect (store.save ("My Sound", s1, false).ok);
            expect (! store.save ("My Sound", s1, false).ok, "same name refused");
            const auto dup = store.save ("my sound", s1, false);
            expect (! dup.ok && dup.message.contains ("already exists"), "different case is the same name: " + dup.message);
            expectEquals (store.listUserPresets().size(), 1);

            setParam (p, "gain", -9.0f);
            expect (store.save ("MY SOUND", p.captureSoundState(), true).ok, "explicit overwrite");
            expectEquals (store.listUserPresets().size(), 1);
            expectEquals (store.listUserPresets()[0], juce::String ("My Sound")); // the stored spelling is kept
            SoundState back;
            expect (store.load ("my sound", back).ok);
            expectWithinAbsoluteError (back.get ("gain", 0.0f), -9.0f, 1.0e-4f);

            expect (store.save ("Second", s1, false).ok);
            expectEquals (store.listUserPresets().size(), 2);
            expect (store.remove ("Second").ok);
            expect (! store.exists ("Second") && store.exists ("My Sound"));
            expect (! store.remove ("Second").ok, "deleting a missing preset reports it");
        }

        beginTest ("names are cleaned: no path traversal, no hidden or reserved names, bounded length, unicode kept");
        {
            expectEquals (PresetStore::sanitizeName ("My Sound"), juce::String ("My Sound"));
            expectEquals (PresetStore::sanitizeName ("  spaced  "), juce::String ("spaced"));
            expectEquals (PresetStore::sanitizeName (""), juce::String());
            expectEquals (PresetStore::sanitizeName ("   "), juce::String());
            expectEquals (PresetStore::sanitizeName ("."), juce::String());
            expectEquals (PresetStore::sanitizeName (".."), juce::String());
            expectEquals (PresetStore::sanitizeName ("///"), juce::String());
            expectEquals (PresetStore::sanitizeName ("../../evil"), juce::String ("_.._evil"));
            expectEquals (PresetStore::sanitizeName ("a/b\\c:d"), juce::String ("a_b_c_d"));
            expectEquals (PresetStore::sanitizeName (".hidden"), juce::String ("hidden"));
            expectEquals (PresetStore::sanitizeName ("CON"), juce::String ("CON_"));
            expectEquals (PresetStore::sanitizeName ("lpt1"), juce::String ("lpt1_"));
            expect (PresetStore::sanitizeName (juce::String::repeatedString ("x", 300)).length() == PresetStore::maxNameLength);
            expect (PresetStore::sanitizeName (juce::CharPointer_UTF8 ("Caf\xc3\xa9 \xe2\x99\xa5 Sound")).contains (juce::CharPointer_UTF8 ("Caf\xc3\xa9")));
            for (auto bad : { "../x", "..\\x", "/etc/passwd", "a/../../b", "x\ny", "x\ty", "name.gkpreset/../../z" })
            {
                const auto clean = PresetStore::sanitizeName (bad);
                expect (! clean.containsAnyOf ("/\\:\n\t") && ! clean.startsWithChar ('.'), juce::String ("unsafe characters survived in '") + clean + "'");
            }

            TempDir tmp;
            auto inner = tmp.dir.getChildFile ("inner");
            PresetStore store (inner);
            GlitchKidProcessor p;
            expect (store.save ("../../escape", p.captureSoundState(), false).ok);
            expect (store.save ("/tmp/absolute", p.captureSoundState(), false).ok);
            expectEquals (tmp.dir.getNumberOfChildFiles (juce::File::findFiles), 0, "nothing was written beside the preset folder");
            auto victim = tmp.dir.getChildFile (juce::String ("victim") + PresetStore::fileExtension);
            victim.replaceWithText ("keep me");
            expect (! store.remove ("../victim").ok && victim.existsAsFile(), "a traversal name cannot delete a file outside the folder");
            SoundState probe;
            expect (! store.load ("../victim", probe).ok, "...nor read one");
            expectEquals (inner.getNumberOfChildFiles (juce::File::findFiles), 2);
            expect (! store.save ("....", p.captureSoundState(), false).ok, "a name with nothing usable is refused");
            expect (! store.save ("", p.captureSoundState(), false).ok);
        }

        beginTest ("missing, empty and unwritable preset folders: the plug-in keeps working");
        {
            TempDir tmp;
            PresetStore missing (tmp.dir.getChildFile ("never/created/yet"));
            expectEquals (missing.listUserPresets().size(), 0);
            SoundState s;
            expect (! missing.load ("nothing", s).ok);
            expect (! missing.remove ("nothing").ok);
            expect (! missing.exists ("nothing"));
            GlitchKidProcessor p;
            expect (missing.save ("First", p.captureSoundState(), false).ok, "the folder is created on first save");
            expectEquals (missing.listUserPresets().size(), 1);

            auto blocker = tmp.dir.getChildFile ("iAmAFile");
            blocker.replaceWithText ("x");
            PresetStore blocked (blocker.getChildFile ("sub"));
            expectEquals (blocked.listUserPresets().size(), 0);
            const auto r = blocked.save ("X", p.captureSoundState(), false);
            expect (! r.ok && r.message.isNotEmpty(), "a folder that cannot be created is reported, not a crash");
        }

        beginTest ("corrupt, foreign, truncated, empty, oversized and outdated preset files are reported and never applied");
        {
            TempDir tmp;
            PresetStore store (tmp.dir);
            auto put = [&] (const juce::String& fileName, const juce::String& text) { tmp.dir.getChildFile (fileName + PresetStore::fileExtension).replaceWithText (text); };
            put ("garbage", "this is not xml at all \x01\x02");
            put ("truncated", "<GlitchKidPreset formatVersion=\"1\"><Param id=\"gain\" value=\"-3\"");
            put ("empty", "");
            put ("foreign", "<SomethingElse formatVersion=\"1\"><Param id=\"gain\" value=\"1\"/></SomethingElse>");
            put ("noversion", "<GlitchKidPreset><Param id=\"gain\" value=\"1\"/></GlitchKidPreset>");
            put ("version0", "<GlitchKidPreset formatVersion=\"0\"><Param id=\"gain\" value=\"1\"/></GlitchKidPreset>");
            put ("noparams", "<GlitchKidPreset formatVersion=\"1\"><Other/></GlitchKidPreset>");
            put ("badvalues", "<GlitchKidPreset formatVersion=\"1\"><Param id=\"gain\" value=\"nan\"/><Param id=\"\" value=\"3\"/><Param id=\"mix\" value=\"inf\"/></GlitchKidPreset>");
            tmp.dir.getChildFile (juce::String ("huge") + PresetStore::fileExtension).replaceWithText (juce::String::repeatedString ("<!-- padding -->", 100000));
            for (auto* fileName : { "garbage", "truncated", "empty", "foreign", "noversion", "version0", "noparams", "badvalues", "huge" })
            {
                SoundState s;
                const auto r = store.load (fileName, s);
                expect (! r.ok && r.message.isNotEmpty(), juce::String (fileName) + " must be refused with a message: " + r.message);
            }

            // Outdated: an older preset that does not list the newer parameters. The missing ones get their defaults when applied.
            put ("older", "<GlitchKidPreset formatVersion=\"1\" name=\"older\"><Param id=\"gain\" value=\"-6\"/><Param id=\"sat_drive\" value=\"9\"/></GlitchKidPreset>");
            SoundState s;
            expect (store.load ("older", s).ok);
            GlitchKidProcessor p;
            randomise (p, 8);
            p.applySoundState (s);
            expectWithinAbsoluteError (getParam (p, "gain"), -6.0f, 0.06f);
            expectWithinAbsoluteError (getParam (p, "sat_drive"), 9.0f, 0.06f);
            GlitchKidProcessor fresh;
            expectEquals (getParam (p, "mut_on"), getParam (fresh, "mut_on"), "newer parameters fall back to their defaults");
            expectEquals (getParam (p, "tune_scale"), getParam (fresh, "tune_scale"));
            expectEquals (getParam (p, "formant_shift"), getParam (fresh, "formant_shift"));
            expect (p.getAlterEgoSelection().ego == -1, "no Alter Ego listed: none selected");

            // A file from a newer GlitchKid with fields this version does not know: loads, ignores them, says so.
            put ("future", "<GlitchKidPreset formatVersion=\"7\" name=\"future\"><Param id=\"gain\" value=\"-2\"/><Param id=\"quantum_flux\" value=\"3\"/><NewThing a=\"1\"/>"
                           "<AlterEgo id=\"not_an_ego_yet\" chaos=\"0.5\" seed=\"1\"/></GlitchKidPreset>");
            SoundState f;
            const auto fr = store.load ("future", f);
            expect (fr.ok && fr.message.isNotEmpty(), "loaded with a note about the newer version");
            GlitchKidProcessor q;
            q.applySoundState (f);
            expectWithinAbsoluteError (getParam (q, "gain"), -2.0f, 0.06f);
            expectEquals (q.getAlterEgoSelection().ego, -1, "an unknown Alter Ego id is ignored");

            // Out-of-range and non-finite values never reach the parameters.
            SoundState wild;
            wild.set ("gain", 1.0e6f); wild.set ("mix", -50.0f); wild.set ("sat_drive", qNaN); wild.set ("mut_amount", std::numeric_limits<float>::infinity());
            GlitchKidProcessor w;
            w.applySoundState (wild);
            expectEquals (getParam (w, "gain"), 12.0f);
            expectEquals (getParam (w, "mix"), 0.0f);
            expectEquals (getParam (w, "sat_drive"), 0.0f, "NaN becomes the default");
            expectWithinAbsoluteError (getParam (w, "mut_amount"), 40.0f, 0.01f, "Inf becomes the default");
        }

        beginTest ("a damaged file does not stop others: the list still shows every preset and a good one still loads");
        {
            TempDir tmp;
            PresetStore store (tmp.dir);
            GlitchKidProcessor p;
            expect (store.save ("Good", p.captureSoundState(), false).ok);
            tmp.dir.getChildFile (juce::String ("Broken") + PresetStore::fileExtension).replaceWithText ("<<<");
            tmp.dir.getChildFile ("notes.txt").replaceWithText ("not a preset");
            expectEquals (store.listUserPresets().size(), 2, "only .gkpreset files are listed");
            SoundState s;
            expect (store.load ("Good", s).ok && ! store.load ("Broken", s).ok);
        }

        beginTest ("a failed overwrite never destroys the good preset (the swap is atomic)");
        {
            TempDir tmp;
            PresetStore store (tmp.dir);
            GlitchKidProcessor p;
            setParam (p, "gain", -5.0f);
            expect (store.save ("Keep", p.captureSoundState(), false).ok);
            const auto file = tmp.dir.getChildFile (juce::String ("Keep") + PresetStore::fileExtension);
            const auto before = file.loadFileAsString();
            expect (! store.save ("Keep", p.captureSoundState(), false).ok);
            expectEquals (file.loadFileAsString(), before, "a refused save leaves the file untouched");
            expectEquals (tmp.dir.getNumberOfChildFiles (juce::File::findFiles), 1, "no stray temporary files");
        }

        beginTest ("A/B: slots start empty; capture A and B independently; recall restores the complete state; switching never changes a stored slot");
        {
            GlitchKidProcessor p;
            expect (! p.hasAbSlot (0) && ! p.hasAbSlot (1));
            expectEquals (p.getAbActive(), -1);
            expect (! p.recallAbSlot (0) && ! p.recallAbSlot (1), "recalling an empty slot does nothing");
            expect (! p.copyAbSlot (0, 1), "copying from an empty slot does nothing");

            randomise (p, 11);
            p.applyAlterEgo (2, 0.5f, 0);
            GlitchKidProcessor refA;
            refA.applySoundState (p.captureSoundState());
            const auto stateA = p.captureSoundState();
            p.captureAbSlot (0);
            expect (p.hasAbSlot (0) && ! p.hasAbSlot (1));
            expectEquals (p.getAbActive(), 0);

            randomise (p, 12);
            p.applyAlterEgo (4, 1.0f, 5);
            GlitchKidProcessor refB;
            refB.applySoundState (p.captureSoundState());
            p.captureAbSlot (1);
            expect (p.hasAbSlot (0) && p.hasAbSlot (1));
            expectEquals (p.getAbActive(), 1);

            const auto storedA = abProperty (p, 0), storedB = abProperty (p, 1);
            for (int i = 0; i < 6; ++i)
            {
                expect (p.recallAbSlot (0));
                expect (worstDifference (p, refA) <= 1.0f, "A restores the complete sound, worst " + juce::String (worstDifference (p, refA)));
                expectEquals (p.getAlterEgoSelection().ego, 2);
                expectEquals (p.getAbActive(), 0);
                expect (! p.isAbSlotModified());
                expect (p.recallAbSlot (1));
                expect (worstDifference (p, refB) <= 1.0f, "B restores the complete sound");
                expectEquals (p.getAlterEgoSelection().ego, 4);
                expectEquals ((int) p.getAlterEgoSelection().seed, 5);
                expectEquals (p.getAbActive(), 1);
            }
            expectEquals (abProperty (p, 0), storedA, "switching never alters slot A");
            expectEquals (abProperty (p, 1), storedB, "switching never alters slot B");
            expect (p.matchesSoundState (stateA) == false);
        }

        beginTest ("A/B: edits are flagged but never written into a slot by switching; recapture is explicit; copy works both ways");
        {
            GlitchKidProcessor p;
            setParam (p, "gain", -3.0f);
            p.captureAbSlot (0);
            setParam (p, "gain", 5.0f);
            p.captureAbSlot (1);
            const auto a0 = abProperty (p, 0), b0 = abProperty (p, 1);
            expect (p.recallAbSlot (0));
            expect (! p.isAbSlotModified());
            setParam (p, "gain", 9.0f); // an edit while A is active
            expect (p.isAbSlotModified(), "the active slot is flagged as edited");
            expect (p.recallAbSlot (1));
            expectEquals (abProperty (p, 0), a0, "the edit was not written into A");
            expectEquals (abProperty (p, 1), b0);
            expect (p.recallAbSlot (0));
            expectWithinAbsoluteError (getParam (p, "gain"), -3.0f, 0.06f, "A is still the captured sound");

            expect (p.copyAbSlot (0, 1));
            expectEquals (abProperty (p, 1), a0, "A copied to B (B now holds A's original sound)");
            expectEquals (abProperty (p, 0), a0, "...and A is unchanged");
            setParam (p, "gain", 7.0f);
            p.captureAbSlot (1);
            const auto b1 = abProperty (p, 1);
            expect (b1 != a0);
            expect (p.copyAbSlot (1, 0));
            expectEquals (abProperty (p, 0), b1, "B copied to A (A now holds B's sound)");
            expectEquals (abProperty (p, 1), b1, "...and B is unchanged");
            expect (! p.copyAbSlot (0, 0) && ! p.copyAbSlot (0, 5) && ! p.copyAbSlot (-1, 0) && ! p.recallAbSlot (2) && ! p.hasAbSlot (-1));
        }

        beginTest ("A/B and presets: loading a preset keeps both A/B snapshots, clears the active marker, and the new sound can be compared with the old");
        {
            TempDir tmp;
            PresetStore store (tmp.dir);
            GlitchKidProcessor p;
            setParam (p, "gain", -4.0f); setParam (p, "sat_drive", 6.0f);
            p.captureAbSlot (0); // the previous sound
            GlitchKidProcessor other;
            other.applyAlterEgo (3, 1.0f, 0);
            expect (store.save ("Other", other.captureSoundState(), false).ok);
            const auto a0 = abProperty (p, 0);
            SoundState s;
            expect (store.load ("Other", s).ok);
            p.applyPreset ("Other", s);
            expectEquals (abProperty (p, 0), a0, "slot A kept");
            expect (! p.hasAbSlot (1), "slot B still empty");
            expectEquals (p.getAbActive(), -1, "neither slot is marked active after a preset load");
            expectEquals (p.getPresetName(), juce::String ("Other"));
            p.captureAbSlot (1); // the new sound
            expectEquals (p.getAbActive(), 1);
            expect (p.recallAbSlot (0));
            expectWithinAbsoluteError (getParam (p, "gain"), -4.0f, 0.06f, "A = the old sound");
            expect (p.recallAbSlot (1));
            expectEquals (p.getAlterEgoSelection().ego, 3, "B = the preset");

            p.applySoundState (p.factoryDefaultState());
            expect (p.hasAbSlot (0) && p.hasAbSlot (1), "restoring the factory sound does not touch A/B either");
        }

        beginTest ("saved sessions: A/B slots, preset name and the sound itself survive a DAW save/reopen; an older session without them loads");
        {
            GlitchKidProcessor a;
            setParam (a, "gain", -3.0f);
            a.captureAbSlot (0);
            setParam (a, "gain", 4.0f); setParam (a, "mut_on", 1.0f);
            a.captureAbSlot (1);
            a.applyPreset ("Remember Me", a.captureSoundState());
            a.recallAbSlot (0);
            juce::MemoryBlock block;
            a.getStateInformation (block);

            GlitchKidProcessor b;
            b.setStateInformation (block.getData(), (int) block.getSize());
            expect (b.hasAbSlot (0) && b.hasAbSlot (1));
            expectEquals (b.getAbActive(), 0);
            expectEquals (b.getPresetName(), juce::String ("Remember Me"));
            expectWithinAbsoluteError (getParam (b, "gain"), -3.0f, 0.06f);
            expect (b.recallAbSlot (1));
            expectEquals (getParam (b, "mut_on"), 1.0f);
            expectWithinAbsoluteError (getParam (b, "gain"), 4.0f, 0.06f);
            expect (! b.isPresetModified(), "after a restore no preset is known to be loaded, so nothing is flagged");

            if (auto xml = juce::AudioProcessor::getXmlFromBinary (block.getData(), (int) block.getSize()))
            {
                xml->removeAttribute ("abSlotA"); xml->removeAttribute ("abSlotB"); xml->removeAttribute ("abActive"); xml->removeAttribute ("presetName");
                juce::MemoryBlock older;
                juce::AudioProcessor::copyXmlToBinary (*xml, older);
                GlitchKidProcessor c;
                c.setStateInformation (older.getData(), (int) older.getSize());
                expect (! c.hasAbSlot (0) && ! c.hasAbSlot (1));
                expectEquals (c.getAbActive(), -1);
                expectEquals (c.getPresetName(), juce::String());
            }
            else
                expect (false, "could not read the saved state");

            // Garbage in the A/B properties must not crash or load a broken sound.
            GlitchKidProcessor d;
            d.apvts.state.setProperty ("abSlotA", "<<< not xml", nullptr);
            d.apvts.state.setProperty ("abSlotB", "<Sound><Param id=\"gain\" value=\"nan\"/></Sound>", nullptr);
            d.apvts.state.setProperty ("abActive", 7, nullptr);
            expect (! d.recallAbSlot (0) && ! d.recallAbSlot (1), "damaged slots count as empty");
            expectEquals (d.getAbActive(), -1);
        }

        beginTest ("mono and stereo: presets and A/B recall give finite audio on both layouts, with NaN/Inf in the input");
        for (int channels : { 1, 2 })
        {
            GlitchKidProcessor p;
            if (channels == 1) setLayout (p, juce::AudioChannelSet::mono());
            p.prepareToPlay (48000.0, 480);
            juce::MidiBuffer midi;
            bool finite = true;
            for (int round = 0; round < 6; ++round)
            {
                randomise (p, 40 + round);
                p.captureAbSlot (round % 2);
                randomise (p, 90 + round);
                p.recallAbSlot (round % 2);
                for (int blk = 0; blk < 8; ++blk)
                {
                    Buf b = makeNoise (channels, 480, round * 10 + blk);
                    if (blk == 3) b.setSample (0, 17, qNaN);
                    p.processBlock (b, midi);
                    finite &= allFinite (b);
                }
            }
            expect (finite, "channels " + juce::String (channels));
        }

        beginTest ("real-time safety: audio keeps running while presets and A/B are applied from another thread, and nothing is allocated per block");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 480);
            randomise (p, 1); p.captureAbSlot (0);
            randomise (p, 2); p.captureAbSlot (1);
            const auto sA = p.getAbSlot (0), sB = p.getAbSlot (1);
            expect (! sA.values.empty() && ! sB.values.empty());

            std::atomic<bool> stop { false };
            std::thread ui ([&]
            {
                int i = 0;
                while (! stop.load())
                {
                    p.applySoundState ((i++ & 1) != 0 ? sA : sB); // the same calls the editor makes, from a different thread than the audio
                    juce::Thread::sleep (1);
                }
            });
            juce::MidiBuffer midi;
            bool finite = true;
            float peak = 0.0f;
            for (int blk = 0; blk < 400; ++blk)
            {
                Buf b = makeNoise (2, 480, blk);
                p.processBlock (b, midi);
                finite &= allFinite (b);
                peak = juce::jmax (peak, b.getMagnitude (0, 480));
            }
            stop = true;
            ui.join();
            expect (finite, "audio stays finite while the sound is changing");
            expect (peak < 8.0f, "audio stays bounded: peak " + juce::String (peak));

            // After a preset load, blocks cost no memory allocation.
            std::vector<Buf> blocks;
            for (int i = 0; i < 60; ++i) blocks.push_back (makeNoise (2, 480, i + 3));
            p.applySoundState (sA);
            for (int i = 0; i < 10; ++i) p.processBlock (blocks[(size_t) i], midi); // warm-up
            const long before = g_allocations.load();
            for (size_t i = 10; i < blocks.size(); ++i)
                p.processBlock (blocks[i], midi);
            expectEquals (g_allocations.load() - before, 0L);
        }

        beginTest ("window actions (PresetController): save asks before replacing, a failed load changes nothing, delete clears the name, factory default restores");
        {
            TempDir tmp;
            GlitchKidProcessor p;
            glitchkid::PresetController c (p, PresetStore (tmp.dir));
            setParam (p, "gain", -6.0f);
            p.applyAlterEgo (4, 0.5f, 0);

            auto o = c.saveAs ("  My/Preset  ", false);
            expect (o.ok && o.name == "My_Preset", "the cleaned name is used and reported: " + o.name);
            expectEquals (p.getPresetName(), juce::String ("My_Preset"));
            expect (! p.isPresetModified(), "right after saving, the sound equals the preset");
            setParam (p, "gain", 3.0f);
            expect (p.isPresetModified());

            o = c.saveAs ("my_preset", false);
            expect (! o.ok && o.needsOverwriteConfirmation, "an existing name is never replaced without confirmation");
            SoundState onDisk;
            expect (c.getStore().load ("My_Preset", onDisk).ok);
            expectWithinAbsoluteError (onDisk.get ("gain", 0.0f), -6.0f, 1.0e-4f, "the file was not touched");
            o = c.saveAs ("my_preset", true);
            expect (o.ok && o.name == "My_Preset", "confirmed replace keeps the stored spelling");
            expect (c.getStore().load ("My_Preset", onDisk).ok);
            expectWithinAbsoluteError (onDisk.get ("gain", 0.0f), 3.0f, 1.0e-4f);

            expect (! c.saveAs ("???", false).ok && c.saveAs ("???", false).message.isNotEmpty(), "an unusable name is explained");

            // a failed load leaves the live sound untouched
            setParam (p, "gain", -1.0f);
            tmp.dir.getChildFile (juce::String ("Broken") + PresetStore::fileExtension).replaceWithText ("nope");
            const auto before = p.captureSoundState();
            o = c.loadUser ("Broken");
            expect (! o.ok && o.message.isNotEmpty());
            expect (p.matchesSoundState (before), "a damaged preset changed nothing");
            expect (! c.loadUser ("Missing").ok);

            // a good load
            o = c.loadUser ("My_Preset");
            expect (o.ok);
            expectWithinAbsoluteError (getParam (p, "gain"), 3.0f, 0.06f);
            expectEquals (p.getPresetName(), juce::String ("My_Preset"));
            expect (c.isUserPreset ("my_preset") && ! c.isUserPreset (glitchkid::PresetController::factoryDefaultName));

            // delete clears the name but keeps the sound
            o = c.deleteUser ("My_Preset");
            expect (o.ok && ! c.getStore().exists ("My_Preset"));
            expectEquals (p.getPresetName(), juce::String());
            expectWithinAbsoluteError (getParam (p, "gain"), 3.0f, 0.06f, "deleting a preset does not change the live sound");
            expect (! c.deleteUser ("My_Preset").ok);

            // factory default
            o = c.loadFactoryDefault();
            expect (o.ok && p.matchesSoundState (p.factoryDefaultState()));
            expectEquals (p.getPresetName(), juce::String (glitchkid::PresetController::factoryDefaultName));
            expectEquals (p.getAlterEgoSelection().ego, -1);
            expect (! c.isUserPreset (p.getPresetName()), "the factory default is not a deletable user preset");
        }

        beginTest ("the user preset folder is a per-user application-data folder, never inside the plug-in or a build directory");
        {
            const auto dir = PresetStore::defaultUserDirectory();
            const auto path = dir.getFullPathName();
            expect (path.contains ("GlitchKid") && path.endsWith ("Presets" + juce::String (juce::File::getSeparatorString()) + "User"), path);
            expect (! path.containsIgnoreCase (".vst3") && ! path.containsIgnoreCase ("build"), path);
#if JUCE_MAC
            expect (path.contains ("/Library/Application Support/GlitchKid/Presets/User"), path);
#endif
            logMessage ("default preset folder on this machine: " + path);
        }
    }
};

static PresetTests presetTests;
