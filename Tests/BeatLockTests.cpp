#include "TestHelpers.h"
#include "../Source/AutoMode/BeatLock.h"

// BEAT LOCK: the pure engine (honest states, evidence-based items, apply with ranges), and that applying it to the real plug-in moves only real parameters.
namespace
{
namespace au = glitchkid::analysis;
namespace am = glitchkid::automode;

au::VocalAnalysis lead()
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

au::VocalAnalysis crowded()
{
    auto b = lead();
    b.rmsDb = -12.0f; b.peakDb = -0.4f;
    b.bandFraction = { 0.10f, 0.20f, 0.55f, 0.10f, 0.05f };
    return b;
}

au::VocalAnalysis sparse()
{
    auto b = lead();
    b.rmsDb = -30.0f; b.peakDb = -18.0f;
    b.bandFraction = { 0.60f, 0.05f, 0.05f, 0.05f, 0.25f }; // nothing in the voice's bands
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

struct BeatLockTests : public juce::UnitTest
{
    BeatLockTests() : juce::UnitTest ("Beat lock", "GlitchKid") {}

    void runTest() override
    {
        using S = am::BeatLockResult::Status;

        beginTest ("no beat connected, silent beat, no analysis: honest status, nothing valid, no items");
        {
            const auto none = am::analyseBeatLock (lead(), crowded(), am::InstrumentalInput::notConnected);
            expect (none.status == S::noBeatInput && ! none.valid && none.items.empty());
            expect (none.message.contains ("BEAT REQUIRED") && none.message.contains ("BUILD MY VOCAL MIX"));
            const auto silent = am::analyseBeatLock (lead(), crowded(), am::InstrumentalInput::connectedSilent);
            expect (silent.status == S::beatSilent && ! silent.valid && silent.items.empty());
            au::VocalAnalysis idle;
            const auto notYet = am::analyseBeatLock (idle, idle, am::InstrumentalInput::live);
            expect (notYet.status == S::notAnalysed && ! notYet.valid);
            const auto noBeatAnalysis = am::analyseBeatLock (lead(), idle, am::InstrumentalInput::live);
            expect (noBeatAnalysis.status == S::beatNotAnalysed && ! noBeatAnalysis.valid);
            expect (noBeatAnalysis.facts.isEmpty(), "no measurement is quoted without a beat analysis");
        }

        beginTest ("a crowded, loud beat: valid, measured levels, BALANCE advice first, a presence move with a reason");
        {
            const auto r = am::analyseBeatLock (lead(), crowded(), am::InstrumentalInput::live);
            expect (r.valid && r.status == S::ready);
            expectWithinAbsoluteError (r.vocalMinusBeatDb, -8.0f, 0.01f);
            expect (! r.items.empty() && r.items[0].advisory && r.items[0].move.category == "BALANCE");
            expect (r.items[0].move.id.empty());
            expect (juce::String (r.items[0].move.reason).contains ("louder than your vocal"));
            bool presence = false;
            for (const auto& it : r.items)
            {
                expect (it.move.reason.size() > 10);
                presence = presence || it.move.id == "eq_b2_gain";
            }
            expect (presence);
            expect (r.worstBand >= 2);
        }

        beginTest ("a sparse beat that does not overlap the voice: no overlap-driven moves; relative level changes only the advice");
        {
            const auto r = am::analyseBeatLock (lead(), sparse(), am::InstrumentalInput::live);
            expect (r.valid);
            for (const auto& it : r.items)
                expect (it.move.id != "eq_b2_gain" && it.move.id != "eq_b1_gain", "no move without the evidence");
            expect (juce::String (r.items[0].move.reason).contains ("louder than the beat"));
        }

        beginTest ("repeating the analysis gives the same answer");
        {
            const auto a = am::analyseBeatLock (lead(), crowded(), am::InstrumentalInput::live);
            const auto b = am::analyseBeatLock (lead(), crowded(), am::InstrumentalInput::live);
            expectEquals ((int) a.items.size(), (int) b.items.size());
            for (size_t i = 0; i < a.items.size(); ++i)
                expect (a.items[i].move.id == b.items[i].move.id && a.items[i].move.delta == b.items[i].move.delta);
        }

        beginTest ("apply: only ticked, non-advisory items move; values stay in range; changes list the reasons; nothing else changes");
        {
            GlitchKidProcessor p;
            const auto r = am::analyseBeatLock (lead(), crowded(), am::InstrumentalInput::live);
            const auto cur = p.captureSoundState();
            std::vector<bool> none (r.items.size(), false);
            std::vector<am::MixChange> ch;
            const auto same = am::applyBeatLock (r, none, cur, rangesOf (p), &ch);
            expect (ch.empty());
            for (const auto& e : cur.values)
                expectWithinAbsoluteError (same.get (e.id, -999.0f), e.value, 1.0e-6f);

            std::vector<bool> all (r.items.size(), true);
            const auto out = am::applyBeatLock (r, all, cur, rangesOf (p), &ch);
            expect (! ch.empty());
            for (const auto& c : ch)
            {
                expect (c.reason.isNotEmpty());
                am::Range rg;
                expect (rangesOf (p) (c.id, rg));
                expect (c.to >= rg.lo - 1.0e-4f && c.to <= rg.hi + 1.0e-4f);
            }
            int moved = 0;
            for (const auto& e : cur.values)
                moved += std::abs (out.get (e.id, e.value) - e.value) > 1.0e-4f ? 1 : 0;
            expectEquals (moved, (int) ch.size());

            // an invalid result changes nothing
            const auto bad = am::analyseBeatLock (lead(), crowded(), am::InstrumentalInput::notConnected);
            std::vector<bool> any (4, true);
            const auto same2 = am::applyBeatLock (bad, any, cur, rangesOf (p), nullptr);
            for (const auto& e : cur.values)
                expectWithinAbsoluteError (same2.get (e.id, -999.0f), e.value, 1.0e-6f);
        }

        beginTest ("applied to the real plug-in and undone: the parameters really move and come back; the parameter count is unchanged");
        {
            GlitchKidProcessor p;
            expectEquals (p.getParameters().size(), 65);
            const auto r = am::analyseBeatLock (lead(), crowded(), am::InstrumentalInput::live);
            const auto before = p.captureSoundState();
            std::vector<bool> all (r.items.size(), true);
            const auto after = am::applyBeatLock (r, all, before, rangesOf (p), nullptr);
            p.applyParameterChanges (after);
            expect (p.apvts.getRawParameterValue ("eq_b2_gain")->load() > before.get ("eq_b2_gain", 0.0f));
            p.applyParameterChanges (before);
            expectWithinAbsoluteError (p.apvts.getRawParameterValue ("eq_b2_gain")->load(), before.get ("eq_b2_gain", 0.0f), 1.0e-4f);
        }
    }
};
BeatLockTests beatLockTests;
}
