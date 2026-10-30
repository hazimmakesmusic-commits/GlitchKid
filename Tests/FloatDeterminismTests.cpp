#include "TestHelpers.h"

// Numeric determinism. Compilers that fuse a*b+c into one rounding (Apple Silicon by default, x86 with FMA enabled) gave different bits for the same source depending on
// where it was inlined, and moved parameter values one rounding step off their grid (0.1 dB steps computed as -12 + 0.1f * 120 gave 1.8e-7 instead of 0). The project
// builds with -ffp-contract=off; these tests fail if that stops being true or if a parameter stops reading back exactly.
namespace
{
#if defined (__GNUC__) || defined (__clang__)
__attribute__ ((noinline))
#endif
float mulSub (float a, float b, float c) { return a * b - c; }

#if defined (__GNUC__) || defined (__clang__)
__attribute__ ((noinline))
#endif
double mulSubD (double a, double b, double c) { return a * b - c; }

struct FloatDeterminismTests : public juce::UnitTest
{
    FloatDeterminismTests() : juce::UnitTest ("Numeric determinism", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("the compiler does not fuse multiply-add: 0.1f * 120 - 12 is exactly 0 and 0.01 * 50 - 0.5 is exactly 0 (fused they are 1.8e-7 and 1.0e-17)");
        {
            volatile float a = 0.1f, b = 120.0f, c = 12.0f;
            volatile double d = 0.01, e = 50.0, f = 0.5;
            expectEquals (mulSub (a, b, c), 0.0f, "float: " + juce::String (mulSub (a, b, c), 9));
            expectEquals (mulSubD (d, e, f), 0.0, "double: " + juce::String (mulSubD (d, e, f), 20));
        }

        beginTest ("every parameter whose default is a whole number reads back exactly that number, in a fresh plug-in and after a state restore");
        {
            GlitchKidProcessor p;
            int checked = 0;
            for (auto* q : p.getParameters())
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (q))
                {
                    const auto& range = r->getNormalisableRange();
                    if (! juce::approximatelyEqual (range.skew, 1.0f))
                        continue; // a skewed (logarithmic) range has no simple closed form to compare with
                    const double ideal = (double) range.start + (double) (range.end - range.start) * (double) r->getDefaultValue();
                    if (std::abs (ideal - std::round (ideal)) > 1.0e-4)
                        continue; // not a whole number (for example 0.3 or 80.0 ms steps are checked elsewhere)
                    expectEquals (th::getParam (p, r->paramID.toRawUTF8()), (float) std::round (ideal), r->paramID + " reads " + juce::String (th::getParam (p, r->paramID.toRawUTF8()), 9));
                    ++checked;
                }
            expect (checked > 40, "most of the 69 parameters have whole-number defaults: " + juce::String (checked));
            juce::MemoryBlock mb;
            p.getStateInformation (mb);
            GlitchKidProcessor restored;
            restored.setStateInformation (mb.getData(), (int) mb.getSize());
            for (const char* id : { "gain", "air_trim", "formant_shift", "eq_b2_gain", "lim_ceiling", "clean_amount" })
                expectEquals (th::getParam (restored, id), th::getParam (p, id), juce::String (id) + " survives a save and restore exactly");
        }

        beginTest ("whole-number values set from the host read back exactly (gain -4 dB, EQ gain, limiter ceiling, Formant semitones)");
        {
            GlitchKidProcessor p;
            struct Case { const char* id; float value; };
            for (const Case& c : { Case { "gain", -4.0f }, Case { "gain", 6.0f }, Case { "eq_b1_gain", -3.0f }, Case { "eq_b2_gain", 4.0f }, Case { "eq_hi_gain", -6.0f },
                                   Case { "lim_ceiling", -1.0f }, Case { "lim_ceiling", -6.0f }, Case { "formant_shift", 5.0f }, Case { "formant_shift", -8.0f }, Case { "air_trim", 0.0f } })
            {
                th::setParam (p, c.id, c.value);
                expectEquals (th::getParam (p, c.id), c.value, juce::String (c.id) + " set to " + juce::String (c.value) + " reads " + juce::String (th::getParam (p, c.id), 9));
            }
        }
    }
};
FloatDeterminismTests floatDeterminismTests;
}
