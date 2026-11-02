#include "TestHelpers.h"
#include "../Source/PluginEditor.h"

// Every destination of the editor opens, lays out and paints, with and without audio; with GLITCHKID_SNAPSHOT_DIR set, each is written as a PNG for review.
namespace
{
struct PageShotsTests : public juce::UnitTest
{
    PageShotsTests() : juce::UnitTest ("Every page", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("each rail destination and the Mastering Lab open, paint, and never change the sound");
        GlitchKidProcessor p;
        p.prepareToPlay (48000.0, 480);
        std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
        auto* ed = dynamic_cast<GlitchKidEditor*> (e.get());
        const auto before = p.captureSoundState();
        const auto dir = juce::SystemStats::getEnvironmentVariable ("GLITCHKID_SNAPSHOT_DIR", {});
        auto shot = [&] (const juce::String& name)
        {
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            expect (img.isValid());
            if (dir.isEmpty()) return;
            juce::File f = juce::File (dir).getChildFile ("page_" + name + ".png");
            f.deleteFile();
            juce::FileOutputStream os (f);
            if (os.openedOk()) juce::PNGImageFormat().writeImageToStream (img, os);
        };
        for (int row = 0; row < 11; ++row)
        {
            ed->uiSelectRailForTest (row);
            expectEquals (ed->uiRailIndexForTest(), row);
            for (int t = 0; t < 8; ++t) ed->uiTickForTest(); // the read-outs as they look after a moment
            shot (juce::String (row).paddedLeft ('0', 2));
        }
        for (auto& v : before.values)
            expectWithinAbsoluteError (p.captureSoundState().get (v.id, -999.0f), v.value, 1.0e-6f, "navigation changed " + v.id);

        beginTest ("honest pages: no developer paths on screen; BEAT and REFERENCE show no results before a real analysis; CREATIVE lights only what is on");
        {
            std::function<void (juce::Component&, juce::StringArray&)> texts = [&] (juce::Component& c, juce::StringArray& out)
            {
                for (auto* ch : c.getChildren())
                {
                    if (! ch->isVisible()) continue;
                    if (auto* l = dynamic_cast<juce::Label*> (ch)) out.add (l->getText());
                    if (auto* b = dynamic_cast<juce::Button*> (ch)) out.add (b->getButtonText());
                    texts (*ch, out);
                }
            };
            for (int row = 0; row < 11; ++row)
            {
                ed->uiSelectRailForTest (row);
                for (int t = 0; t < 4; ++t) ed->uiTickForTest();
                juce::StringArray all;
                texts (*ed, all);
                for (auto& t : all)
                    expect (! t.contains ("docs/") && ! t.contains (".md"), "row " + juce::String (row) + " shows a developer path: " + t);
                if (row == 8) // BEAT, nothing routed in: the empty state says what will appear and when; no suggestion rows, no APPLY
                {
                    expect (all.joinIntoString (" ").contains ("Suggestions appear here after"), "BEAT explains its empty panel");
                    expect (! all.contains ("APPLY SELECTED"), "nothing to apply before an analysis");
                    expect (all.contains ("NO BEAT? BUILD MY MIX  >"), "the honest alternative is offered");
                }
                if (row == 9) // REFERENCE, nothing analysed
                    expect (! all.joinIntoString (" ").containsIgnoreCase ("confidence: high"), "no confidence before measurements exist");
            }
            ed->uiSelectRailForTest (7);
            auto& lab = ed->uiLabPageForTest();
            expect (! lab.cardOnForTest (2));
            th::setParam (p, "mut_on", 1.0f);
            expect (lab.cardOnForTest (2), "the Mutation card lights when Mutation is on");
            th::setParam (p, "mut_on", 0.0f);
            expect (! lab.moreShownForTest());
            lab.toggleMoreForTest();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            expect (lab.moreShownForTest(), "MORE shows Mutation's Character and Blend");
        }
    }
};
static PageShotsTests pageShotsTests;
}
