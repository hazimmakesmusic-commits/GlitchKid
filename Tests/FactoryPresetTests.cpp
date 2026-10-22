#include "TestHelpers.h"
#include "../Source/Presets/FactoryPresets.h"
#include "../Source/Presets/PresetController.h"

// Stage 4.10: the three built-in artist-style presets. They are lists of overrides on the Factory Default sound, applied through the
// same path as user presets, so these tests prove they are discoverable, load the intended values, are repeatable and stay legal.
namespace
{
using glitchkid::FactoryPresets::count;
using glitchkid::PresetController;
using glitchkid::SoundState;

juce::File tempDir()
{
    return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("glitchkid-fp-" + juce::String::toHexString (juce::Random().nextInt64()));
}

SoundState capture (GlitchKidProcessor& p) { return p.captureSoundState(); }

struct FactoryPresetTests : public juce::UnitTest
{
    FactoryPresetTests() : juce::UnitTest ("Factory presets", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("all factory presets exist with distinct names and are reserved");
        {
            expectEquals (count, 8);
            for (int i = 0; i < count; ++i)
            {
                const juce::String n = glitchkid::FactoryPresets::info (i).name;
                expect (n.isNotEmpty());
                expectEquals (glitchkid::FactoryPresets::indexOfName (n.toLowerCase()), i);
                expect (PresetController::isFactoryName (n));
                for (int j = i + 1; j < count; ++j)
                    expect (n != juce::String (glitchkid::FactoryPresets::info (j).name));
            }
            expect (PresetController::isFactoryName ("Factory Default"));
            expect (! PresetController::isFactoryName ("My Sound"));
        }

        beginTest ("each preset sets every sound parameter, in range, to its intended values");
        {
            GlitchKidProcessor p;
            auto dir = tempDir();
            PresetController c (p, glitchkid::PresetStore (dir));
            const auto def = p.factoryDefaultState();
            for (int i = 0; i < count; ++i)
            {
                const auto out = c.loadFactory (i);
                expect (out.ok);
                expectEquals (p.getPresetName(), juce::String (glitchkid::FactoryPresets::info (i).name));
                expect (! p.isPresetModified());
                const auto built = glitchkid::FactoryPresets::build (i, def);
                expectEquals ((int) built.values.size(), (int) def.values.size());
                int changed = 0;
                for (const auto& e : built.values)
                {
                    auto* r = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (e.id));
                    expect (r != nullptr, e.id);
                    if (r == nullptr) continue;
                    const auto range = r->getNormalisableRange();
                    expect (e.value >= range.start - 1.0e-4f && e.value <= range.end + 1.0e-4f, "in range: " + e.id);
                    const float step = range.interval;
                    expect (std::abs (r->convertFrom0to1 (r->getValue()) - e.value) <= (step > 0.0f ? 0.5f * step + 1.0e-3f : 2.0e-3f), "applied: " + e.id);
                    changed += std::abs (e.value - def.get (e.id, e.value)) > 1.0e-6f ? 1 : 0;
                }
                expect (changed > 10, "preset differs from the default in many parameters");
                expect (p.apvts.state.getProperty ("egoId").toString().isEmpty());
            }
            dir.deleteRecursively();
        }

        beginTest ("loading is repeatable whatever the plug-in was doing before");
        {
            GlitchKidProcessor p;
            auto dir = tempDir();
            PresetController c (p, glitchkid::PresetStore (dir));
            for (int i = 0; i < count; ++i)
            {
                c.loadFactory (i);
                const auto first = capture (p);
                for (auto* q : p.getParameters())
                    if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (q))
                        if (r->paramID != "bypass")
                            r->setValueNotifyingHost (juce::Random (i + 5).nextFloat());
                c.loadFactory ((i + 1) % count);
                c.loadFactory (i);
                const auto again = capture (p);
                expectEquals ((int) again.values.size(), (int) first.values.size());
                for (const auto& e : first.values)
                    if (! e.id.startsWith ("tune_") && ! e.id.startsWith ("air_") && ! e.id.startsWith ("clean_") && ! e.id.startsWith ("lim_") && e.id != "gain" && e.id != "mix") // tuning, Gain and Mix are yours: a profile keeps whatever you had
                        expectWithinAbsoluteError (again.get (e.id, -999.0f), e.value, 1.0e-3f);
            }
            dir.deleteRecursively();
        }

        beginTest ("loading keeps A/B snapshots and Bypass, and Factory Default still works");
        {
            GlitchKidProcessor p;
            auto dir = tempDir();
            PresetController c (p, glitchkid::PresetStore (dir));
            setParam (p, "gain", -6.0f);
            p.captureAbSlot (0);
            setParam (p, "bypass", 1.0f);
            c.loadFactory (1);
            expectWithinAbsoluteError (getParam (p, "bypass"), 1.0f, 1.0e-3f);
            expect (p.recallAbSlot (0));
            expectWithinAbsoluteError (getParam (p, "gain"), -6.0f, 1.0e-3f);
            c.loadFactoryDefault();
            expectWithinAbsoluteError (getParam (p, "tune_on"), 0.0f, 1.0e-3f);
            expect (! c.loadFactory (-1).ok && ! c.loadFactory (count).ok);
            dir.deleteRecursively();
        }

        beginTest ("a user cannot save over a built-in name; user presets still save");
        {
            GlitchKidProcessor p;
            auto dir = tempDir();
            PresetController c (p, glitchkid::PresetStore (dir));
            expect (! c.saveAs (glitchkid::FactoryPresets::info (0).name, true).ok);
            expect (! c.saveAs (juce::String (glitchkid::FactoryPresets::info (1).name).toUpperCase(), false).ok);
            expect (! c.saveAs ("factory default", true).ok);
            expect (c.saveAs ("Mine", false).ok);
            dir.deleteRecursively();
        }

        beginTest ("browser: list order, factory/user kinds, stepping wraps in both directions and skips a damaged file");
        {
            GlitchKidProcessor p;
            auto dir = tempDir();
            PresetController c (p, glitchkid::PresetStore (dir));
            setParam (p, "gain", -3.0f);
            expect (c.saveAs ("Zed", false).ok);
            expect (c.saveAs ("Alpha", false).ok);
            const auto names = c.allPresetNames();
            expectEquals (names.size(), 1 + count + 2);
            expectEquals (names[0], juce::String ("Factory Default"));
            expectEquals (names[1], juce::String (glitchkid::FactoryPresets::info (0).name));
            expectEquals (names[1 + count], juce::String ("Alpha"));
            expectEquals (names[2 + count], juce::String ("Zed"));
            expect (c.kindOf ("Alpha") == PresetController::Kind::user);
            expect (c.kindOf (names[2]) == PresetController::Kind::factory);
            expect (c.kindOf ("Factory Default") == PresetController::Kind::factory);
            expect (c.kindOf ("") == PresetController::Kind::none && c.kindOf ("nothing") == PresetController::Kind::none);

            p.clearPresetName();
            expect (c.step (1).ok);
            expectEquals (p.getPresetName(), names[0]);
            for (int i = 1; i < names.size(); ++i)
            {
                expect (c.step (1).ok);
                expectEquals (p.getPresetName(), names[i]);
            }
            expect (c.step (1).ok);
            expectEquals (p.getPresetName(), names[0]); // wrapped
            expect (c.step (-1).ok);
            expectEquals (p.getPresetName(), names[names.size() - 1]);
            expect (p.isPresetModified() == false);

            // A damaged user file is skipped in both directions.
            dir.getChildFile ("Zed.gkpreset").replaceWithText ("not a preset");
            c.loadFactory (count - 1);
            c.loadByName ("Alpha");
            expect (c.step (1).ok);
            expectEquals (p.getPresetName(), names[0]);
            expect (c.step (-1).ok);
            expectEquals (p.getPresetName(), juce::String ("Alpha")); // Zed is damaged: skipped backwards too
            dir.deleteRecursively();
        }

        beginTest ("saving your own version of a factory preset: new user preset, factory definitions unchanged, both recall correctly");
        {
            GlitchKidProcessor p;
            auto dir = tempDir();
            PresetController c (p, glitchkid::PresetStore (dir));
            const auto def = p.factoryDefaultState();
            std::vector<SoundState> before;
            for (int i = 0; i < count; ++i)
                before.push_back (glitchkid::FactoryPresets::build (i, def));

            c.loadFactory (1);
            expectEquals (PresetController::variationName (p.getPresetName()), p.getPresetName() + " My Version");
            expectEquals (PresetController::variationName ("Factory Default"), juce::String());
            expectEquals (PresetController::variationName ("Mine"), juce::String ("Mine"));
            setParam (p, "sat_drive", 9.0f);
            setParam (p, "echo_mix", 55.0f);
            expect (p.isPresetModified());
            const auto saved = c.saveAs (PresetController::variationName (p.getPresetName()), false);
            expect (saved.ok);
            expect (c.kindOf (saved.name) == PresetController::Kind::user);
            expect (! p.isPresetModified());
            expectEquals (p.getPresetName(), saved.name);

            for (int i = 0; i < count; ++i) // definitions identical to before the save
            {
                const auto now = glitchkid::FactoryPresets::build (i, def);
                expectEquals ((int) now.values.size(), (int) before[(size_t) i].values.size());
                for (const auto& e : now.values)
                    expectEquals (before[(size_t) i].get (e.id, -999.0f), e.value, e.id);
            }
            c.loadFactory (1); // the original is back, without the tweaks
            expectWithinAbsoluteError (getParam (p, "sat_drive"), before[1].get ("sat_drive", -1.0f), 1.0e-3f);
            expectWithinAbsoluteError (getParam (p, "echo_mix"), before[1].get ("echo_mix", -1.0f), 1.0e-3f);
            expect (! p.isPresetModified());
            expect (c.loadUser (saved.name).ok); // and the variation keeps them
            expectWithinAbsoluteError (getParam (p, "sat_drive"), 9.0f, 0.1f);
            expectWithinAbsoluteError (getParam (p, "echo_mix"), 55.0f, 0.1f);
            expect (! c.saveAs (glitchkid::FactoryPresets::info (1).name, true).ok); // still cannot replace the original
            dir.deleteRecursively();
        }

        beginTest ("A/B with factory presets: A and B hold different presets and recalling restores each exactly");
        {
            GlitchKidProcessor p;
            auto dir = tempDir();
            PresetController c (p, glitchkid::PresetStore (dir));
            c.loadFactory (0);
            p.captureAbSlot (0);
            const auto a = capture (p);
            c.loadFactory (5);
            p.captureAbSlot (1);
            const auto b = capture (p);
            expect (p.recallAbSlot (0));
            for (const auto& e : a.values)
                expectWithinAbsoluteError (getParam (p, e.id.toRawUTF8()), e.value, 1.0e-3f);
            expect (p.recallAbSlot (1));
            for (const auto& e : b.values)
                expectWithinAbsoluteError (getParam (p, e.id.toRawUTF8()), e.value, 1.0e-3f);
            expect (std::abs (a.get ("echo_on", 0.0f) - b.get ("echo_on", 0.0f)) > 0.5f, "the two slots really differ");
            dir.deleteRecursively();
        }

        beginTest ("state save/restore keeps the preset name and every value");
        {
            GlitchKidProcessor a;
            auto dir = tempDir();
            PresetController c (a, glitchkid::PresetStore (dir));
            c.loadFactory (2);
            juce::MemoryBlock mb;
            a.getStateInformation (mb);
            GlitchKidProcessor b;
            b.setStateInformation (mb.getData(), (int) mb.getSize());
            expectEquals (b.getPresetName(), juce::String (glitchkid::FactoryPresets::info (2).name));
            const auto sa = capture (a), sb = capture (b);
            for (const auto& e : sa.values)
                expectWithinAbsoluteError (sb.get (e.id, -999.0f), e.value, 1.0e-3f);
            dir.deleteRecursively();
        }

        beginTest ("audio is finite and sensible with each preset (mono and stereo, odd block sizes, NaN input)");
        {
            for (int channels = 1; channels <= 2; ++channels)
                for (int i = 0; i < count; ++i)
                {
                    GlitchKidProcessor p;
                    if (channels == 1) setLayout (p, juce::AudioChannelSet::mono());
                    auto dir = tempDir();
                    PresetController c (p, glitchkid::PresetStore (dir));
                    p.prepareToPlay (48000.0, 480);
                    c.loadFactory (i);
                    juce::MidiBuffer midi;
                    bool finite = true;
                    float peak = 0.0f;
                    for (int blk = 0; blk < 120; ++blk)
                    {
                        Buf b = makeNoise (channels, blk % 3 == 0 ? 173 : 480, blk + 1);
                        b.applyGain (0.25f);
                        if (blk == 20) b.setSample (0, 5, std::numeric_limits<float>::quiet_NaN());
                        p.processBlock (b, midi);
                        for (int ch = 0; ch < channels; ++ch)
                            for (int s = 0; s < b.getNumSamples(); ++s)
                            {
                                finite &= std::isfinite (b.getSample (ch, s));
                                peak = juce::jmax (peak, std::abs (b.getSample (ch, s)));
                            }
                    }
                    expect (finite, "preset " + juce::String (i) + " ch " + juce::String (channels));
                    expect (peak < 4.0f, "no runaway level: " + juce::String (peak));
                    dir.deleteRecursively();
                }
        }
    }
};
FactoryPresetTests factoryPresetTests;
}
