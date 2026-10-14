#include "TestHelpers.h"
#include "../Source/DSP/ParametricEq.h"

// NaN / Inf must never permanently poison a stage that holds recursive (IIR) state.
namespace
{
constexpr float nan = std::numeric_limits<float>::quiet_NaN();
constexpr float inf = std::numeric_limits<float>::infinity();

bool allFinite (const Buf& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (ch, i)))
                return false;
    return true;
}

// Puts NaN, +Inf and -Inf into the block (on both channels, at different positions); returns a copy
// in which those samples are 0 - what a stage should be equivalent to seeing.
Buf poison (Buf& b)
{
    Buf zeroed;
    zeroed.makeCopyOf (b);
    const int n = b.getNumSamples();
    const float bad[3] = { nan, inf, -inf };
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int k = 0; k < 3; ++k)
        {
            const int i = (n / 4) * (k + 1) + ch;
            b.setSample (ch, i, bad[k]);
            zeroed.setSample (ch, i, 0.0f);
        }
    return zeroed;
}

// Gives every sample the same huge finite value of alternating sign (float max is 3.4e38).
void hugeBurst (Buf& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            b.setSample (ch, i, (i & 1) ? -3.0e38f : 3.0e38f);
}

void setEq (ParametricEq& eq)
{
    eq.setBand (0, { 100.0f, 9.0f, 0.7071f });
    eq.setBand (1, { 300.0f, 12.0f, 8.0f }); // narrow and high gain: rings for a while
    eq.setBand (2, { 3000.0f, -6.0f, 2.0f });
    eq.setBand (3, { 9000.0f, 6.0f, 0.7071f });
}

constexpr int block = 512;
} // namespace

struct RobustnessTests : public juce::UnitTest
{
    RobustnessTests() : juce::UnitTest ("Non-finite input", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("EQ: NaN/Inf give finite output and are equivalent to a zero sample; later audio is unaffected");
        {
            ParametricEq a, b;
            a.prepare (48000.0); b.prepare (48000.0);
            setEq (a); setEq (b);
            bool finite = true, same = true;
            for (int blk = 0; blk < 24; ++blk)
            {
                Buf in = makeNoise (2, block, blk + 1), ref;
                ref.makeCopyOf (in);
                if (blk == 3)
                    ref = poison (in); // ref is the same block with the bad samples zeroed
                a.process (in);
                b.process (ref);
                finite &= allFinite (in);
                same &= juce::exactlyEqual (maxAbsDiff (in, ref), 0.0f); // exactly the zero-substitution result, also long after
            }
            expect (finite, "output finite in every block");
            expect (same, "identical to an EQ that saw zeros, so nothing stays poisoned");
        }

        beginTest ("EQ: huge finite input (overflow) cannot lock the filter; clean audio recovers");
        {
            ParametricEq a, fresh;
            a.prepare (48000.0); fresh.prepare (48000.0);
            setEq (a); setEq (fresh);
            Buf burst (2, block);
            hugeBurst (burst);
            a.process (burst);
            expect (allFinite (burst), "output finite during the burst");
            Buf x, y;
            for (int blk = 0; blk < 40; ++blk)
            {
                x = makeNoise (2, block, blk + 1);
                y.makeCopyOf (x);
                a.process (x);
                fresh.process (y);
                expect (allFinite (x), "finite after the burst");
            }
            expect (maxAbsDiff (x, y) < 1.0e-4f, "after recovery matches an EQ that never saw the burst");
        }

        beginTest ("Saturation: NaN/Inf give finite output and are equivalent to a zero sample; later audio is unaffected");
        for (float drive : { 6.0f, 18.0f })
        {
            Saturation a, b;
            a.prepare (48000.0, block); b.prepare (48000.0, block);
            a.setDriveDb (drive); b.setDriveDb (drive);
            bool finite = true, same = true;
            for (int blk = 0; blk < 24; ++blk)
            {
                Buf in = makeNoise (2, block, blk + 1), ref;
                ref.makeCopyOf (in);
                if (blk == 3)
                    ref = poison (in);
                a.process (in);
                b.process (ref);
                finite &= allFinite (in);
                same &= juce::exactlyEqual (maxAbsDiff (in, ref), 0.0f);
            }
            expect (finite, "output finite, drive " + juce::String (drive));
            expect (same, "identical to a stage that saw zeros, drive " + juce::String (drive));
        }

        beginTest ("Saturation: NaN while idle (drive 0) does not poison it when drive is switched on later");
        {
            Saturation a, b;
            a.prepare (48000.0, block); b.prepare (48000.0, block);
            Buf in = makeNoise (2, block), ref;
            ref.makeCopyOf (in);
            ref = poison (in);
            a.process (in); b.process (ref);
            expect (allFinite (in), "idle output finite");
            a.setDriveDb (12.0f); b.setDriveDb (12.0f);
            bool same = true;
            for (int blk = 0; blk < 10; ++blk)
            {
                Buf x = makeNoise (2, block, blk + 5), y;
                y.makeCopyOf (x);
                a.process (x); b.process (y);
                same &= allFinite (x) && juce::exactlyEqual (maxAbsDiff (x, y), 0.0f);
            }
            expect (same, "finite and identical to the zero-substitution run");
        }

        beginTest ("Saturation: huge finite input (overflow) stays finite and recovers");
        {
            Saturation a, fresh;
            a.prepare (48000.0, block); fresh.prepare (48000.0, block);
            a.setDriveDb (18.0f); fresh.setDriveDb (18.0f);
            Buf burst (2, block);
            hugeBurst (burst);
            a.process (burst);
            expect (allFinite (burst), "output finite during the burst");
            Buf x, y;
            for (int blk = 0; blk < 40; ++blk)
            {
                x = makeNoise (2, block, blk + 1);
                y.makeCopyOf (x);
                a.process (x);
                fresh.process (y);
                expect (allFinite (x), "finite after the burst");
            }
            expect (maxAbsDiff (x, y) < 1.0e-4f, "after recovery matches a stage that never saw the burst");
        }

        beginTest ("Processor: EQ and saturation together (and each alone) recover; mono and stereo; valid audio unchanged");
        for (int mode = 0; mode < 3; ++mode)
            for (int channels : { 1, 2 })
            {
                GlitchKidProcessor a, b;
                for (auto* p : { &a, &b })
                {
                    if (channels == 1)
                        setLayout (*p, juce::AudioChannelSet::mono());
                    p->prepareToPlay (48000.0, block);
                    if (mode != 1) setParam (*p, "eq_b1_gain", 9.0f);
                    if (mode != 0) setParam (*p, "sat_drive", 12.0f);
                }
                juce::MidiBuffer midi;
                bool finite = true, same = true;
                for (int blk = 0; blk < 30; ++blk)
                {
                    Buf x = makeNoise (channels, block, blk + 1), y;
                    y.makeCopyOf (x);
                    if (blk == 5)
                    {
                        y = poison (x);
                        // every channel gets NaN/Inf; the reference sees zeros
                    }
                    a.processBlock (x, midi);
                    b.processBlock (y, midi);
                    finite &= allFinite (x);
                    same &= juce::exactlyEqual (maxAbsDiff (x, y), 0.0f);
                }
                const juce::String what = "mode " + juce::String (mode) + ", " + juce::String (channels) + " ch";
                expect (finite, "finite output, " + what);
                expect (same, "same as the zero-substitution run, " + what);
            }
    }
};

static RobustnessTests robustnessTests;
