#include "TestHelpers.h"
#include "../Source/PluginEditor.h"
#include "../Source/Analysis/DisplaySpectrum.h"
#include <set>

// UI behaviour that must hold for the interface to feel immediate and trustworthy: the display never freezes on stale audio, nothing is redrawn when nothing
// changed, timers run only when needed, a workspace transition never delays or dims the controls, and no control overlaps another or leaks between workspaces.
namespace
{
constexpr double fs = 48000.0;
constexpr int blockSize = 480;

void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

void feedTone (GlitchKidProcessor& p, int blocks, float amp, int& counter)
{
    juce::MidiBuffer midi;
    for (int b = 0; b < blocks; ++b)
    {
        th::Buf buf (2, blockSize);
        for (int i = 0; i < blockSize; ++i)
        {
            const double t = (double) (counter * blockSize + i) / fs;
            const float v = amp * (float) (std::sin (2.0 * 3.14159265 * 220.0 * t) + 0.4 * std::sin (2.0 * 3.14159265 * 1700.0 * t));
            buf.setSample (0, i, v);
            buf.setSample (1, i, v);
        }
        p.processBlock (buf, midi);
        ++counter;
    }
}

template <typename T>
T* findOf (juce::Component& root)
{
    for (auto* c : root.getChildren())
    {
        if (auto* t = dynamic_cast<T*> (c); t != nullptr)
            return t;
        if (auto* r = findOf<T> (*c))
            return r;
    }
    return nullptr;
}

void collectInteractive (juce::Component& content, juce::Component& root, std::vector<juce::Component*>& out)
{
    for (auto* c : root.getChildren())
    {
        if (c->isVisible())
        {
            const bool interactive = dynamic_cast<juce::Button*> (c) != nullptr || dynamic_cast<juce::Slider*> (c) != nullptr || dynamic_cast<juce::ComboBox*> (c) != nullptr
                                     || dynamic_cast<glitchkid::ui::SpectrumView*> (c) != nullptr || dynamic_cast<glitchkid::ui::EqGraph*> (c) != nullptr
                                     || dynamic_cast<glitchkid::ui::ChainStrip*> (c) != nullptr;
            if (interactive)
                out.push_back (c);
            else
                collectInteractive (content, *c, out);
        }
    }
}

struct UiBehaviourTests : public juce::UnitTest
{
    UiBehaviourTests() : juce::UnitTest ("UI behaviour", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("the display never freezes on stale audio: when nothing is being written it decays to empty and says so");
        {
            using namespace glitchkid::analysis;
            DisplayTap tap;
            std::vector<float> x (4096);
            for (size_t n = 0; n < x.size(); ++n)
                x[n] = 0.5f * (float) std::sin (2.0 * 3.14159265 * 1000.0 * (double) n / fs);
            tap.push (x.data(), nullptr, 4096);
            DisplaySpectrum sp;
            WaveColumns wave;
            sp.update (tap, fs, 1.0f / 60.0f);
            wave.update (tap, 2400, 1.0f / 60.0f);
            expect (sp.hasSignal() && wave.hasSignal, "live while samples are arriving");
            float top = 0.0f;
            for (float v : sp.levels())
                top = std::max (top, v);
            expect (top > 0.5f);
            for (int i = 0; i < 6; ++i) // 6 x 60 ms with no new samples: the transport has stopped
            {
                sp.update (tap, fs, 0.06f);
                wave.update (tap, 2400, 0.06f);
            }
            expect (! sp.hasSignal() && ! wave.hasSignal, "no new samples for a third of a second: not live");
            for (int i = 0; i < 30; ++i)
                sp.update (tap, fs, 0.05f);
            for (float v : sp.levels())
                expectEquals (v, 0.0f); // fully decayed, not frozen
            tap.push (x.data(), nullptr, 480); // audio returns
            sp.update (tap, fs, 1.0f / 60.0f);
            expect (sp.hasSignal(), "and it comes back when audio does");
            // the fall rate is per second, so 30 Hz and 60 Hz updates decay alike
            DisplaySpectrum a, b;
            DisplayTap t2;
            t2.push (x.data(), nullptr, 4096);
            a.update (t2, fs, 1.0f / 30.0f);
            b.update (t2, fs, 1.0f / 60.0f);
            DisplayTap empty;
            std::vector<float> zeros (4096, 0.0f);
            empty.push (zeros.data(), nullptr, 4096);
            for (int i = 0; i < 3; ++i) // 0.1 s either way
            {
                a.update (empty, fs, 1.0f / 30.0f);
            }
            for (int i = 0; i < 6; ++i)
                b.update (empty, fs, 1.0f / 60.0f);
            float ma = 0.0f, mb = 0.0f;
            for (int c = 0; c < DisplaySpectrum::columns; ++c)
            {
                ma = std::max (ma, a.levels()[(size_t) c]);
                mb = std::max (mb, b.levels()[(size_t) c]);
            }
            expectWithinAbsoluteError (ma, mb, 0.02f);
        }

        beginTest ("nothing is redrawn when nothing changed: a silent HOME asks for no redraws; audio asks for them; the timers run only when needed");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (fs, blockSize);
            p.setUiInt ("uiLevel", 0);
            std::unique_ptr<juce::AudioProcessorEditor> base (p.createEditor());
            auto* ed = dynamic_cast<GlitchKidEditor*> (base.get());
            auto* content = ed->getChildComponent (0);
            auto* view = findOf<glitchkid::ui::SpectrumView> (*content);
            expect (ed != nullptr && view != nullptr);
            if (view == nullptr)
                return;
            expect (ed->uiVisualTimerRunningForTest(), "the display timer runs on HOME");
            pump (150);
            const int before = view->getRepaintRequestsForTest();
            pump (400);
            expectEquals (view->getRepaintRequestsForTest(), before, "a silent display requests no redraws");
            int counter = 0;
            feedTone (p, 40, 0.3f, counter);
            const int beforeAudio = view->getRepaintRequestsForTest();
            for (int i = 0; i < 20; ++i)
            {
                feedTone (p, 2, 0.3f, counter);
                pump (16);
            }
            expect (view->getRepaintRequestsForTest() > beforeAudio + 3, "live audio does redraw it");
            ed->uiShowWorkspaceForTest (3); // BUILD MY VOCAL MIX has no live display
            pump (200);
            expect (! ed->uiVisualTimerRunningForTest(), "the display timer stops where nothing needs it");
            ed->uiShowWorkspaceForTest (4, 2); // FINAL TOUCH > DYNAMICS
            pump (200);
            expect (! ed->uiVisualTimerRunningForTest());
            ed->uiShowWorkspaceForTest (4, 1); // FINAL TOUCH > TONE (the EQ graph shows the live spectrum)
            expect (ed->uiVisualTimerRunningForTest());
            ed->uiShowWorkspaceForTest (1);
            expect (ed->uiVisualTimerRunningForTest());
            pump (200);
            expect (ed->uiVisualTimerRunningForTest(), "still running on CLEAN LAB after the transition");
        }

        beginTest ("a workspace transition never dims, moves or delays the controls: full alpha, no offset, brief, off with MOTION");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (fs, blockSize);
            std::unique_ptr<juce::AudioProcessorEditor> base (p.createEditor());
            auto* ed = dynamic_cast<GlitchKidEditor*> (base.get());
            auto* content = ed->getChildComponent (0);
            ed->uiShowWorkspaceForTest (3);
            expect (ed->uiTransitionRunningForTest(), "the scan-line wipe starts");
            expectEquals (content->getAlpha(), 1.0f);
            expectEquals (content->getTransform().getTranslationX(), 0.0f);
            for (int i = 0; i < 8; ++i)
            {
                pump (15);
                expectEquals (content->getAlpha(), 1.0f);
                expectEquals (content->getTransform().getTranslationX(), 0.0f);
            }
            pump (150);
            expect (! ed->uiTransitionRunningForTest(), "it is over within a fraction of a second");
            p.setUiFlag ("uiMotion", false);
            std::unique_ptr<juce::AudioProcessorEditor> still (p.createEditor());
            auto* ed2 = dynamic_cast<GlitchKidEditor*> (still.get());
            ed2->uiShowWorkspaceForTest (2);
            expect (! ed2->uiTransitionRunningForTest(), "MOTION off: no transition at all");
        }

        beginTest ("layout: in every workspace and category, interactive controls stay inside the window, never overlap, and no control from one workspace is left on another; at several window sizes");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (fs, blockSize);
            std::unique_ptr<juce::AudioProcessorEditor> base (p.createEditor());
            auto* ed = dynamic_cast<GlitchKidEditor*> (base.get());
            auto* content = ed->getChildComponent (0);
            const int w0 = ed->getWidth(), h0 = ed->getHeight();
            struct View { int workspace, category; juce::String name; };
            std::vector<View> views = { { 0, 0, "HOME" }, { 1, 0, "CLEAN LAB" }, { 2, 0, "BEAT LOCK" }, { 3, 0, "BUILD" },
                                        { 4, 0, "FINAL > PITCH" }, { 4, 1, "FINAL > TONE" }, { 4, 2, "FINAL > DYNAMICS" }, { 4, 3, "FINAL > SPACE" } };
            std::vector<std::set<juce::Component*>> sets;
            for (int size : { 70, 100, 150 })
            {
                ed->setSize (w0 * size / 100, h0 * size / 100);
                sets.clear();
                for (const auto& v : views)
                {
                    ed->uiShowWorkspaceForTest (v.workspace, v.category);
                    std::vector<juce::Component*> items;
                    collectInteractive (*content, *content, items);
                    expect (items.size() > 5, v.name + " shows controls");
                    std::set<juce::Component*> set (items.begin(), items.end());
                    sets.push_back (set);
                    for (size_t i = 0; i < items.size(); ++i)
                    {
                        const auto r = content->getLocalArea (items[i], items[i]->getLocalBounds());
                        expect (juce::Rectangle<int> (0, 0, w0, h0).contains (r), v.name + ": a control sticks out of the window: " + items[i]->getName() + " " + r.toString());
                        for (size_t j = i + 1; j < items.size(); ++j)
                        {
                            const auto o = r.getIntersection (content->getLocalArea (items[j], items[j]->getLocalBounds()));
                            if (o.getWidth() * o.getHeight() > 4)
                                expect (false, v.name + ": two controls overlap: " + items[i]->getName() + " / " + items[j]->getName() + " " + o.toString());
                        }
                    }
                }
            }
            // no leaked controls: what is on two different workspaces is on all of them (the persistent bar), nothing else
            std::set<juce::Component*> everywhere = sets[0];
            for (size_t i = 1; i < sets.size(); ++i)
                for (auto it = everywhere.begin(); it != everywhere.end();)
                    it = sets[i].count (*it) ? std::next (it) : everywhere.erase (it);
            for (size_t i = 0; i < 4; ++i) // the five workspaces (FINAL TOUCH's categories share its tab buttons, so compare across workspaces only)
                for (size_t j = i + 1; j < 4; ++j)
                    for (auto* c : sets[i])
                        if (sets[j].count (c) && ! everywhere.count (c))
                            expect (false, "a control is visible on two different workspaces: " + c->getName());
            const int final0 = 4;
            for (size_t i = 0; i < 4; ++i)
                for (auto* c : sets[i])
                    if (sets[(size_t) final0].count (c) && ! everywhere.count (c))
                        expect (false, "a control from " + views[i].name + " is still shown in FINAL TOUCH");
            expect (ed->getWidth() >= 0);
        }

        beginTest ("every workspace renders at the smallest, normal and largest window size (snapshots, no blank frames)");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (fs, blockSize);
            std::unique_ptr<juce::AudioProcessorEditor> base (p.createEditor());
            auto* ed = dynamic_cast<GlitchKidEditor*> (base.get());
            const int w0 = ed->getWidth(), h0 = ed->getHeight();
            for (int size : { 70, 100, 150 })
                for (int ws = 0; ws < 5; ++ws)
                {
                    ed->setSize (w0 * size / 100, h0 * size / 100);
                    ed->uiShowWorkspaceForTest (ws);
                    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                    std::set<juce::uint32> colours;
                    for (int y = 0; y < img.getHeight(); y += 9)
                        for (int x = 0; x < img.getWidth(); x += 9)
                            colours.insert (img.getPixelAt (x, y).getARGB());
                    expect (img.isValid() && colours.size() > 15, "workspace " + juce::String (ws) + " at " + juce::String (size) + " % renders");
                }
        }
    }
};
UiBehaviourTests uiBehaviourTests;
}
