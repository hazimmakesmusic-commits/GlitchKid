#include "TestHelpers.h"
#include "../Source/PluginEditor.h"
#include "../Source/Presets/FactoryPresets.h"

// Screens for the website, rendered from the real editor at 2x (only when GLITCHKID_WEB_SHOTS_DIR is set; otherwise this group does nothing).
// The plug-in is driven the way a user would: choose a starting sound on HOME, press BUILD MY MIX while a vocal-like test signal plays through the
// processor, switch TUNE and CLEAN LAB on, then visit each page while the signal keeps playing. Nothing is drawn or edited by hand.
// The signal is synthetic (a sung-like tone with vibrato, phrases and breath noise), not a recording; the website must never present it as one.
namespace
{
struct WebsiteShotsTests : public juce::UnitTest
{
    WebsiteShotsTests() : juce::UnitTest ("Website shots", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("real editor screens for the website, at 2x");
        const auto dir = juce::SystemStats::getEnvironmentVariable ("GLITCHKID_WEB_SHOTS_DIR", {});
        if (dir.isEmpty())
        {
            logMessage ("GLITCHKID_WEB_SHOTS_DIR not set: skipped");
            return;
        }
        juce::File (dir).createDirectory();

        GlitchKidProcessor p;
        p.prepareToPlay (48000.0, 480);
        std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
        auto* ed = dynamic_cast<GlitchKidEditor*> (e.get());
        auto& main = ed->uiMainScreenForTest();

        // a sung-like phrase: 196 Hz (G3) with vibrato, harmonics falling with frequency, syllable envelope, short gaps, a little breath noise
        juce::Random rng (7);
        double phase = 0.0;
        juce::int64 n = 0;
        juce::MidiBuffer midi;
        auto play = [&] (int blocks)
        {
            for (int b = 0; b < blocks; ++b)
            {
                juce::AudioBuffer<float> buf (2, 480);
                for (int i = 0; i < 480; ++i, ++n)
                {
                    const double t = (double) n / 48000.0, bar = std::fmod (t, 2.4);
                    const double env = bar < 2.0 ? std::pow (std::sin (juce::MathConstants<double>::pi * bar / 2.0), 0.6) * (0.75 + 0.25 * std::sin (2.0 * juce::MathConstants<double>::pi * 2.5 * t)) : 0.0;
                    phase += 2.0 * juce::MathConstants<double>::pi * 196.0 * (1.0 + 0.012 * std::sin (2.0 * juce::MathConstants<double>::pi * 5.2 * t)) / 48000.0;
                    double s = 0.0;
                    for (int h = 1; h <= 30; ++h)
                        s += std::sin (phase * h) / std::pow ((double) h, 1.15) * (h > 10 && h < 18 ? 1.6 : 1.0); // a little presence around 2-3.5 kHz
                    const float v = (float) (0.16 * env * s + 0.003 * (rng.nextFloat() - 0.5f));
                    buf.setSample (0, i, v);
                    buf.setSample (1, i, v);
                }
                p.processBlock (buf, midi);
                if (b % 3 == 0)
                {
                    ed->uiTickForTest();
                    main.tickForTest();
                }
            }
        };
        auto shot = [&] (const juce::String& shotName)
        {
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
            expect (img.isValid() && img.getWidth() == ed->getWidth() * 2, shotName);
            juce::File f = juce::File (dir).getChildFile (shotName + ".png");
            f.deleteFile();
            juce::FileOutputStream os (f);
            if (os.openedOk())
                juce::PNGImageFormat().writeImageToStream (img, os);
        };

        // HOME: choose Modern Melodic Rap, BUILD MY MIX listens to the signal and builds
        ed->uiSelectRailForTest (0);
        main.genreForTest().setSelectedId (3, juce::sendNotificationSync); // the second starting sound
        main.pressForTest ("BUILD MY MIX");
        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
        for (int k = 0; k < 12; ++k)
        {
            play (100);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (400);
        logMessage ("built: " + p.getPresetName());
        expect (p.getPresetName().startsWith ("Built: "), "a mix was built: " + p.getPresetName());
        th::setParam (p, "tune_on", 1.0f);
        th::setParam (p, "clean_on", 1.0f);
        th::setParam (p, "clean_amount", 55.0f);
        play (500); // CLEAN LAB learns the voice; the meters and live views settle on the playing signal

        const struct { int rail; const char* name; } pages[] = {
            { 0, "web_home" }, { 1, "web_sounds" }, { 3, "web_tune" }, { 2, "web_clean" }, { 4, "web_shape" }, { 6, "web_space" }, { 5, "web_finish" } };
        for (const auto& pg : pages)
        {
            ed->uiSelectRailForTest (pg.rail);
            play (60);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (150); // page transition finished
            play (12);
            shot (pg.name);
        }
    }
};
static WebsiteShotsTests websiteShotsTests;
}
