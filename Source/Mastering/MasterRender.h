#pragma once
#include "MasteringEngine.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include <functional>
#include <memory>

// Mastering a FILE (offline, on a worker thread; never the audio thread): read a finished mix, measure it, and render it through a fresh MasteringEngine
// with the current settings to a 24-bit WAV at the file's own sample rate. The result is time-aligned with the source (the chain's latency is removed)
// and as long as it. The source file is only read.
namespace glitchkid::master
{
struct MixReport
{
    bool ok = false;
    juce::String message, fileName;
    double seconds = 0.0, sampleRate = 0.0;
    int channels = 0;
    float integratedLufs = LoudnessMeter::silence, truePeakDb = LoudnessMeter::silence, samplePeakDb = LoudnessMeter::silence, correlation = 1.0f;
    int clippedSamples = 0;
};

// Whether a mix is ready to master, in plain English, from real measurements (the live input or a file). Never a guess about the music itself.
juce::String suitability (float integratedLufs, float truePeakDb, int clippedSamples, float correlation, bool stereo);

static constexpr double maxRenderSeconds = 20.0 * 60.0;

MixReport analyseMix (const juce::File&, const std::atomic<bool>& cancel, const std::function<void (float)>& progress = {});

struct RenderResult
{
    bool ok = false;
    juce::String message;
    juce::File output;
    MixReport before, after; // measured on the source and on the rendered master
};

RenderResult renderMaster (const juce::File& source, const juce::File& destination, const Controls&, const std::atomic<bool>& cancel,
                           const std::function<void (float)>& progress = {});

// One job at a time on a low-priority thread; the result comes back on the message thread.
class MasterJob : private juce::Thread
{
public:
    MasterJob() : juce::Thread ("GlitchKid mastering") {}
    ~MasterJob() override { stopJob(); }
    bool analyse (const juce::File&, std::function<void (const MixReport&)> onDone);
    bool render (const juce::File& source, const juce::File& destination, const Controls&, std::function<void (const RenderResult&)> onDone);
    bool isRunning() const { return busy.load(); }
    float progress() const { return progressValue.load(); }
    void cancel() { cancelFlag.store (true); }
    void stopJob();

private:
    void run() override;
    struct Shared { std::function<void (const MixReport&)> analysed; std::function<void (const RenderResult&)> rendered; bool alive = true; };
    std::shared_ptr<Shared> shared = std::make_shared<Shared>();
    juce::File source, destination;
    Controls controls;
    bool rendering = false;
    std::atomic<bool> cancelFlag { false }, busy { false };
    std::atomic<float> progressValue { 0.0f };
    bool start();
};
}
