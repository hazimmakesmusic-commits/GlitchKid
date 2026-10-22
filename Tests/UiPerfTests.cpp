#include "TestHelpers.h"
#include "../Source/PluginEditor.h"
#include <algorithm>

// UI cost measurements (Release, one core of whatever machine runs the tests, software rendering, headless). They LOG real timings and fail only on very
// generous regression limits: the numbers say how the work compares between versions on the same machine, not how it feels on a Mac with GPU compositing.
namespace
{
constexpr double fs = 48000.0;
constexpr int blockSize = 480;

struct Stat { double median = 0.0, p95 = 0.0, worst = 0.0; };

Stat statOf (std::vector<double> v)
{
    std::sort (v.begin(), v.end());
    return { v[v.size() / 2], v[(size_t) ((double) v.size() * 0.95)], v.back() };
}

juce::String fmt (const Stat& s) { return "median " + juce::String (s.median, 3) + " ms, p95 " + juce::String (s.p95, 3) + " ms, worst " + juce::String (s.worst, 3) + " ms"; }

juce::TextButton* findButton (juce::Component& root, const juce::String& text)
{
    for (auto* c : root.getChildren())
    {
        if (auto* b = dynamic_cast<juce::TextButton*> (c); b != nullptr && b->getButtonText() == text)
            return b;
        if (auto* r = findButton (*c, text))
            return r;
    }
    return nullptr;
}

template <typename T>
T* findOf (juce::Component& root)
{
    for (auto* c : root.getChildren())
    {
        if (auto* t = dynamic_cast<T*> (c); t != nullptr && t->isVisible())
            return t;
        if (auto* r = findOf<T> (*c))
            return r;
    }
    return nullptr;
}

void feed (GlitchKidProcessor& p, int blocks, int& counter)
{
    juce::MidiBuffer midi;
    for (int b = 0; b < blocks; ++b)
    {
        th::Buf buf (2, blockSize);
        for (int i = 0; i < blockSize; ++i)
        {
            const double t = (double) (counter * blockSize + i) / fs;
            const float v = 0.3f * (float) (std::sin (2.0 * 3.14159265 * 220.0 * t) + 0.5 * std::sin (2.0 * 3.14159265 * 1320.0 * t)) * (float) std::fabs (std::sin (2.0 * 3.14159265 * 2.0 * t));
            buf.setSample (0, i, v);
            buf.setSample (1, i, v);
        }
        p.processBlock (buf, midi);
        ++counter;
    }
}

double nowMs() { return juce::Time::getMillisecondCounterHiRes(); }

struct UiPerfTests : public juce::UnitTest
{
    UiPerfTests() : juce::UnitTest ("UI cost", "GlitchKid") {}

    void runTest() override
    {
        static const char* nav[5] = { "HOME", "CLEAN LAB", "BEAT LOCK", "BUILD MY MIX", "FINAL TOUCH" };
        beginTest ("measured: one 30 Hz refresh per workspace (idle and with audio), a workspace switch, a full repaint, a slider drag step");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (fs, blockSize);
            std::unique_ptr<juce::AudioProcessorEditor> base (p.createEditor());
            auto* ed = dynamic_cast<GlitchKidEditor*> (base.get());
            expect (ed != nullptr);
            auto* content = ed->getChildComponent (0);
            int counter = 0;
            for (int i = 0; i < 5; ++i)
            {
                ed->uiShowWorkspaceForTest (i);
                for (bool audio : { false, true })
                {
                    std::vector<double> t;
                    for (int k = 0; k < 300; ++k)
                    {
                        if (audio)
                            feed (p, 1, counter);
                        const double a = nowMs();
                        ed->uiTickForTest();
                        t.push_back (nowMs() - a);
                    }
                    const auto s = statOf (t);
                    logMessage (juce::String ("UI-COST tick  ") + nav[i] + (audio ? " (audio)" : " (idle) ") + ": " + fmt (s));
                    expect (s.median < 5.0, "a refresh costs well under a 30 Hz frame");
                }
            }

            // switching workspaces: the click handler (layout + visibility), not the later repaint
            {
                std::vector<double> t;
                for (int k = 0; k < 200; ++k)
                {
                    const double a = nowMs();
                    ed->uiShowWorkspaceForTest (k % 5);
                    t.push_back (nowMs() - a);
                }
                logMessage ("UI-COST workspace switch (showPage: layout and visibility, not the later repaint): " + fmt (statOf (t)));
            }

            // a full-window software repaint of each workspace, and of the visualizer on its own
            for (int i = 0; i < 5; ++i)
            {
                ed->uiShowWorkspaceForTest (i);
                feed (p, 10, counter);
                for (int k = 0; k < 3; ++k)
                    ed->uiTickForTest();
                std::vector<double> t;
                for (int k = 0; k < 15; ++k)
                {
                    const double a = nowMs();
                    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                    t.push_back (nowMs() - a);
                }
                logMessage (juce::String ("UI-COST full repaint ") + nav[i] + ": " + fmt (statOf (t)));
            }
            ed->uiShowWorkspaceForTest (0);
            feed (p, 20, counter);
            ed->uiTickForTest();
            if (auto* view = findOf<glitchkid::ui::SpectrumView> (*content))
            {
                std::vector<double> t;
                for (int k = 0; k < 40; ++k)
                {
                    const double a = nowMs();
                    auto img = view->createComponentSnapshot (view->getLocalBounds(), true, 1.0f);
                    t.push_back (nowMs() - a);
                }
                logMessage ("UI-COST visualizer repaint (HOME view only): " + fmt (statOf (t)));
            }
            else
                expect (false, "the HOME visualizer exists");

            // the EQ graph on its own (CLEAN LAB, with a live spectrum behind it) and a full repaint while a workspace fade is running (content at reduced alpha)
            ed->uiShowWorkspaceForTest (1);
            feed (p, 20, counter);
            ed->uiTickForTest();
            if (auto* graph = findOf<glitchkid::ui::EqGraph> (*content))
            {
                std::vector<double> t;
                for (int k = 0; k < 40; ++k)
                {
                    const double a = nowMs();
                    auto img = graph->createComponentSnapshot (graph->getLocalBounds(), true, 1.0f);
                    t.push_back (nowMs() - a);
                }
                logMessage ("UI-COST EQ graph repaint (CLEAN LAB, live spectrum): " + fmt (statOf (t)));
            }
            {
                std::vector<double> t, t1;
                for (int k = 0; k < 15; ++k)
                {
                    content->setAlpha (0.3f);
                    const double a = nowMs();
                    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                    t.push_back (nowMs() - a);
                    content->setAlpha (1.0f);
                    const double b = nowMs();
                    auto img2 = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                    t1.push_back (nowMs() - b);
                }
                logMessage ("UI-COST full repaint, content alpha 0.3 (as during the workspace fade): " + fmt (statOf (t)) + "; same frame at alpha 1: " + fmt (statOf (t1)));
            }

            // one frame of the new workspace transition: only the scan-line band is redrawn (a 42 px strip of the workspace area)
            {
                ed->uiShowWorkspaceForTest (3);
                const int w = ed->getWidth(), h = ed->getHeight();
                const juce::Rectangle<int> band (0, (int) ((102.0f + 0.5f * 458.0f - 20.0f) * (float) h / 600.0f), w, (int) (42.0f * (float) h / 600.0f));
                std::vector<double> t;
                for (int k = 0; k < 40; ++k)
                {
                    const double a = nowMs();
                    auto img = ed->createComponentSnapshot (band, true, 1.0f);
                    t.push_back (nowMs() - a);
                }
                logMessage ("UI-COST workspace transition frame (band only, new behaviour): " + fmt (statOf (t)));
            }

            // dragging an ordinary knob and a macro slider (the macro slider rewrites several parameters per step)
            ed->uiShowWorkspaceForTest (4, 1); // FINAL TOUCH > TONE
            juce::Slider* knob = nullptr;
            juce::Slider* macro = nullptr;
            std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
            {
                for (auto* ch : c.getChildren())
                {
                    if (auto* sl = dynamic_cast<juce::Slider*> (ch); sl != nullptr && sl->isVisible())
                    {
                        if (sl->getSliderStyle() == juce::Slider::RotaryHorizontalVerticalDrag && knob == nullptr)
                            knob = sl;
                        if (sl->getSliderStyle() == juce::Slider::LinearHorizontal && sl->getTextBoxPosition() == juce::Slider::NoTextBox && macro == nullptr)
                            macro = sl;
                    }
                    walk (*ch);
                }
            };
            walk (*content);
            if (knob != nullptr)
            {
                std::vector<double> t;
                for (int k = 0; k < 400; ++k)
                {
                    const double a = nowMs();
                    knob->setValue (500.0 + (double) (k % 200) * 10.0, juce::sendNotificationSync);
                    t.push_back (nowMs() - a);
                }
                logMessage ("UI-COST knob drag step (parameter write): " + fmt (statOf (t)));
            }
            if (macro != nullptr)
            {
                std::vector<double> t;
                for (int k = 0; k < 400; ++k)
                {
                    const double a = nowMs();
                    macro->setValue (0.3 + 0.4 * (double) (k % 50) / 50.0, juce::sendNotificationSync);
                    t.push_back (nowMs() - a);
                }
                logMessage ("UI-COST macro slider drag step (rewrites several parameters): " + fmt (statOf (t)));
            }
        }
    }
};
UiPerfTests uiPerfTests;
}
