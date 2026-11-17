#include "TestHelpers.h"
#include "../Source/PluginEditor.h"

long threadAllocations(); // FormantShiftTests.cpp replaces the global operator new and counts per thread

// HOME's live vocal: the clock runs from the moment the editor is built (a host shows the window afterwards), the level comes from exactly the samples
// the plug-in processed since the last frame (any rate, any block size, no missed transient), silence and a stopped plug-in show nothing, and leaving
// HOME or closing the window never leaves stale frames.
namespace
{
struct HomeWaveformTests : public juce::UnitTest
{
    HomeWaveformTests() : juce::UnitTest ("Home live vocal", "GlitchKid") {}

    static void feed (GlitchKidProcessor& p, int total, int block, const std::function<float (int)>& sample)
    {
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buf (2, block);
        for (int done = 0; done < total; done += block)
        {
            const int n = juce::jmin (block, total - done);
            juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), 2, n);
            for (int i = 0; i < n; ++i)
                view.setSample (0, i, sample (done + i)), view.setSample (1, i, view.getSample (0, i));
            p.processBlock (view, midi);
        }
    }

    void runTest() override
    {
        beginTest ("HOME's clock runs as soon as the editor exists, before any host shows the window, and keeps running across ADVANCED MIXING and back");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 480);
            std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
            auto* ed = dynamic_cast<GlitchKidEditor*> (e.get());
            auto& main = ed->uiMainScreenForTest();
            expectEquals (ed->uiPageForTest(), 6, "a new instance opens on HOME");
            expect (main.clockRunningForTest(), "running before the window is on screen (the bug: it waited for a visibility change that never came)");
            ed->uiSelectRailForTest (3);
            ed->uiSelectRailForTest (0);
            expect (main.clockRunningForTest(), "still running after ADVANCED MIXING and back");
        }

        beginTest ("real input: the level is the input's own peak at every sample rate and block size");
        for (double sr : { 44100.0, 48000.0, 96000.0 })
            for (int block : { 1, 32, 480, 4096 })
            {
                GlitchKidProcessor p;
                p.prepareToPlay (sr, block);
                std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
                auto& main = dynamic_cast<GlitchKidEditor*> (e.get())->uiMainScreenForTest();
                const float amp = juce::Decibels::decibelsToGain (-12.0f);
                feed (p, (int) (sr * 0.05), block, [&] (int i) { return amp * (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * i / sr); });
                main.tickForTest();
                const auto tag = juce::String (sr, 0) + " Hz, block " + juce::String (block);
                expectWithinAbsoluteError (main.inputDbForTest(), -12.0f, 0.3f, tag);
                expect (main.signalForTest() && main.inputEnvelopeForTest (0) > 0.7f, tag + ": shown as signal");
            }

        beginTest ("a single-sample transient anywhere since the last frame is caught (96 kHz, 80 ms between frames, 4096-sample blocks)");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (96000.0, 4096);
            std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
            auto& main = dynamic_cast<GlitchKidEditor*> (e.get())->uiMainScreenForTest();
            feed (p, 4096, 4096, [] (int) { return 0.0f; });
            main.tickForTest();
            feed (p, 7680, 4096, [] (int i) { return i == 100 ? 0.5f : 0.0f; }); // early in the window: the old reader (the newest 1600 samples) missed it
            main.tickForTest();
            expectWithinAbsoluteError (main.inputDbForTest(), -6.02f, 0.05f);
        }

        beginTest ("silence is shown as silence; a plug-in that stops processing shows nothing (no stale level)");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 480);
            std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
            auto& main = dynamic_cast<GlitchKidEditor*> (e.get())->uiMainScreenForTest();
            feed (p, 4800, 480, [] (int) { return 0.0f; });
            main.tickForTest();
            expect (main.inputDbForTest() <= -99.0f && main.inputEnvelopeForTest (0) == 0.0f && ! main.signalForTest(), "digital silence");
            feed (p, 4800, 480, [] (int i) { return 0.4f * (float) std::sin (0.05 * i); });
            main.tickForTest();
            expect (main.signalForTest());
            main.tickForTest(); // no new audio since the last frame (the host stopped calling the plug-in)
            expect (main.inputDbForTest() <= -99.0f && main.inputEnvelopeForTest (0) == 0.0f, "nothing new: no level is invented");
            expect (main.inputEnvelopeForTest (1) > 0.0f, "the frame before still shows what was really played");
        }

        beginTest ("leaving HOME clears its history; coming back and closing / reopening the window start fresh");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 480);
            std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
            auto* ed = dynamic_cast<GlitchKidEditor*> (e.get());
            auto& main = ed->uiMainScreenForTest();
            for (int k = 0; k < 5; ++k)
            {
                feed (p, 1600, 480, [] (int i) { return 0.5f * (float) std::sin (0.03 * i); });
                main.tickForTest();
            }
            expect (main.inputEnvelopeForTest (4) > 0.0f);
            ed->uiSelectRailForTest (3); // ADVANCED MIXING: HOME leaves the window
            float left = 0.0f;
            for (int k = 0; k < 240; ++k) left = juce::jmax (left, main.inputEnvelopeForTest (k));
            expectEquals (left, 0.0f, "no frame from before is kept");
            ed->uiSelectRailForTest (0);
            feed (p, 1600, 480, [] (int i) { return 0.25f * (float) std::sin (0.03 * i); });
            main.tickForTest();
            expectWithinAbsoluteError (main.inputDbForTest(), -12.04f, 0.2f, "back on HOME: the live level at once");
            expectEquals (main.inputEnvelopeForTest (1), 0.0f, "and nothing older than this visit");

            e.reset();
            feed (p, 4800, 480, [] (int i) { return 0.5f * (float) std::sin (0.03 * i); }); // audio while the window is closed
            std::unique_ptr<juce::AudioProcessorEditor> e2 (p.createEditor());
            auto& main2 = dynamic_cast<GlitchKidEditor*> (e2.get())->uiMainScreenForTest();
            expect (main2.clockRunningForTest() && main2.inputEnvelopeForTest (0) == 0.0f, "a reopened window starts with an empty history and a running clock");
            feed (p, 960, 480, [] (int i) { return 0.5f * (float) std::sin (0.03 * i); });
            main2.tickForTest();
            expect (main2.signalForTest(), "and shows the vocal on its first frame");
        }

        beginTest ("the audio thread does not allocate while HOME draws the vocal");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (48000.0, 480);
            std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
            auto& main = dynamic_cast<GlitchKidEditor*> (e.get())->uiMainScreenForTest();
            juce::MidiBuffer midi;
            juce::AudioBuffer<float> buf (2, 480);
            auto fill = [&buf] { for (int c = 0; c < 2; ++c) for (int i = 0; i < 480; ++i) buf.setSample (c, i, 0.3f * (float) std::sin (0.02 * i)); };
            for (int k = 0; k < 20; ++k) { fill(); p.processBlock (buf, midi); } // warm up
            long allocations = 0;
            for (int k = 0; k < 50; ++k)
            {
                fill();
                const long before = threadAllocations();
                p.processBlock (buf, midi);
                allocations += threadAllocations() - before;
                main.tickForTest();
            }
            expectEquals (allocations, 0L, "allocations in processBlock");
        }
    }
};

static HomeWaveformTests homeWaveformTests;
}
