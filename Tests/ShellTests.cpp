#include "TestHelpers.h"
#include "../Source/PluginEditor.h"

// The Vocal Lab shell: the signal rail is the navigation and maps onto the saved workspace numbers, its dots and the START next step are read from real state,
// and hover help appears in the fixed info line instead of floating over the controls.
namespace
{
constexpr double fs = 48000.0;
constexpr int blockSize = 480;

void feedTone (GlitchKidProcessor& p, int blocks, float amp)
{
    juce::MidiBuffer midi;
    for (int b = 0; b < blocks; ++b)
    {
        th::Buf buf (2, blockSize);
        for (int i = 0; i < blockSize; ++i)
        {
            const float v = amp * (float) std::sin (2.0 * 3.14159265 * 220.0 * (double) (b * blockSize + i) / fs);
            buf.setSample (0, i, v);
            buf.setSample (1, i, v);
        }
        p.processBlock (buf, midi);
    }
}

juce::Component* findWithTooltip (juce::Component& root)
{
    for (auto* c : root.getChildren())
    {
        if (auto* t = dynamic_cast<juce::TooltipClient*> (c); t != nullptr && c->isVisible() && c->getWidth() > 0 && t->getTooltip().isNotEmpty() && dynamic_cast<juce::Button*> (c) != nullptr)
            return c;
        if (auto* r = findWithTooltip (*c))
            return r;
    }
    return nullptr;
}

struct ShellTests : public juce::UnitTest
{
    ShellTests() : juce::UnitTest ("Vocal Lab shell", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("the rail rows open the same saved workspace numbers as before, and sessions saved before the rail open on the matching row");
        {
            // old session -> row: (uiLevel, uiCategory) -> rail index
            struct Case { int level, category, row; };
            for (const auto& c : { Case { 0, 0, 0 }, { 3, 0, 1 }, { 1, 0, 2 }, { 4, 0, 3 }, { 4, 1, 4 }, { 4, 2, 5 }, { 4, 3, 6 }, { 2, 0, 7 }, { 5, 0, 8 } })
            {
                GlitchKidProcessor p;
                p.setUiInt ("uiLevel", c.level);
                p.setUiInt ("uiCategory", c.category);
                std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
                auto* ed = dynamic_cast<GlitchKidEditor*> (e.get());
                expectEquals (ed->uiRailIndexForTest(), c.row, "saved workspace " + juce::String (c.level) + "/" + juce::String (c.category) + " selects its row");
            }
            GlitchKidProcessor p;
            std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
            auto* ed = dynamic_cast<GlitchKidEditor*> (e.get());
            for (int row = 0; row < 9; ++row)
            {
                ed->uiSelectRailForTest (row);
                expectEquals (ed->uiRailIndexForTest(), row);
            }
            ed->uiSelectRailForTest (5);
            expectEquals (p.getUiInt ("uiLevel", -1), 4);
            expectEquals (p.getUiInt ("uiCategory", -1), 2);
            expectEquals (p.getParameters().size(), 69);
        }

        beginTest ("the rail dots light only for stages that really change the sound; DRY lights none");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (fs, blockSize);
            std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
            auto* ed = dynamic_cast<GlitchKidEditor*> (e.get());
            ed->uiTickForTest();
            for (int row : { 2, 3, 4, 5, 6 })
                expect (! ed->uiRailDotForTest (row), "row " + juce::String (row) + " is idle by default");
            th::setParam (p, "tune_on", 1.0f);
            th::setParam (p, "echo_on", 1.0f);
            ed->uiTickForTest();
            expect (ed->uiRailDotForTest (3) && ed->uiRailDotForTest (6), "TUNE and SPACE are lit");
            expect (! ed->uiRailDotForTest (4) && ! ed->uiRailDotForTest (5), "TONE and DYNAMICS stay unlit");
            th::setParam (p, "bypass", 1.0f);
            ed->uiTickForTest();
            for (int row : { 2, 3, 4, 5, 6 })
                expect (! ed->uiRailDotForTest (row), "nothing is processed while DRY: row " + juce::String (row));
        }

        beginTest ("START says what is true: no signal, no sound chosen, CLEAN off, then refine");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (fs, blockSize);
            std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
            auto* ed = dynamic_cast<GlitchKidEditor*> (e.get());
            for (int i = 0; i < 40; ++i)
                ed->uiTickForTest();
            expect (ed->uiNextStepForTest().containsIgnoreCase ("No signal"), ed->uiNextStepForTest());
            feedTone (p, 20, 0.3f);
            ed->uiTickForTest();
            expect (ed->uiNextStepForTest().containsIgnoreCase ("starting sound"), ed->uiNextStepForTest());
            p.applyPreset ("My sound", p.captureSoundState());
            ed->uiTickForTest();
            expect (ed->uiNextStepForTest().containsIgnoreCase ("CLEAN"), ed->uiNextStepForTest());
            th::setParam (p, "clean_on", 1.0f);
            feedTone (p, 20, 0.3f);
            ed->uiTickForTest();
            expect (ed->uiNextStepForTest().containsIgnoreCase ("Refine"), ed->uiNextStepForTest());
        }

        beginTest ("hover help shows in the info line, returns to the default when the pointer leaves, and nothing floats");
        {
            GlitchKidProcessor p;
            std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
            auto* ed = dynamic_cast<GlitchKidEditor*> (e.get());
            expect (ed->uiHelpTextForTest().containsIgnoreCase ("Point at any control"));
            auto* c = findWithTooltip (*ed);
            expect (c != nullptr);
            if (c != nullptr)
            {
                ed->uiHoverForTest (c);
                expectEquals (ed->uiHelpTextForTest(), dynamic_cast<juce::TooltipClient*> (c)->getTooltip());
                ed->uiHoverForTest (nullptr);
                expect (ed->uiHelpTextForTest().containsIgnoreCase ("Point at any control"));
            }
        }
    }
};
ShellTests shellTests;
}
