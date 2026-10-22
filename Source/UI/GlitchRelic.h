#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace glitchkid::ui
{
// THE GLITCH RELIC: GlitchKid's signature image. An abstract liquid-chrome form: a twisting ribbon knotted on itself, lit by a chrome environment with a hard
// horizon, rimmed in ultraviolet and cyber pink, haloed by a soft violet glow, fading to nothing at its edges. It is drawn procedurally (no photograph, no
// 3D engine, no external file): the same size always produces the same pixels. It is a decorative layer and carries no information. It is meant to be
// rendered once into an image and reused (render () takes some milliseconds; do not call it per frame), and it can be saved as a PNG for marketing.
// The output is premultiplied-alpha ARGB with a transparent background; use it dimmed and only behind plates that carry no text.
juce::Image renderGlitchRelic (int width, int height);

// Renders the relic once on a background thread (never on the message thread: it takes a few hundred milliseconds in software) and delivers it back on the
// message thread. The finished image is kept for the life of the process, so a second editor gets it at once. The owner must call stop () before it is destroyed
// (a plug-in must never leave a thread running).
class GlitchRelicLoader : private juce::Thread
{
public:
    GlitchRelicLoader() : juce::Thread ("GlitchKid relic") {}
    ~GlitchRelicLoader() override { stop(); }
    void start (std::function<void (const juce::Image&)> onReady);
    void stop(); // message thread: after this the callback can never run and the thread is gone
    void waitForTest()
    {
        waitForThreadToExit (15000);
       #if JUCE_MODAL_LOOPS_PERMITTED
        juce::MessageManager::getInstance()->runDispatchLoopUntil (30);
       #endif
    }

private:
    struct Shared
    {
        std::function<void (const juce::Image&)> ready;
        bool alive = true; // message thread only
    };
    void run() override;
    std::shared_ptr<Shared> shared = std::make_shared<Shared>();
};
}
