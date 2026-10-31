#pragma once
#include "VocalAnalyzer.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace glitchkid::analysis
{
// What GlitchKid can tell from one vocal file (an isolated vocal: a full song gives meaningless pitch and key numbers). Read on a worker thread, never the audio thread.
// MEASURED values are computed directly from the samples. ESTIMATES are inferences and are labelled as such everywhere they are shown.
// The file is only read: nothing is copied, kept or written, and the audio is not played through the plug-in.
struct VocalFileReport
{
    enum class Status { ok, cancelled, missingFile, unsupported, unreadable, silent };
    static constexpr double maxSeconds = 180.0; // longer files: the first 3 minutes are analysed
    static constexpr int overviewColumns = 512;

    Status status = Status::unreadable;
    juce::String message;            // plain English, for the interface (the error, or a one-line summary)
    juce::String fileName;
    double fileSeconds = 0.0, analysedSeconds = 0.0, sampleRate = 0.0;
    int channels = 0;

    // MEASURED (whole analysed length)
    float peakDb = -120.0f;
    int clippedSamples = 0, nearClippedSamples = 0;  // at or above 0.999 / 0.98 full scale
    float activeSeconds = 0.0f;                       // 50 ms frames above -50 dBFS
    float loudP90Db = -120.0f, quietP10Db = -120.0f;  // percentiles of the 50 ms frame RMS over active frames
    float correlation = 1.0f;                         // left/right correlation (stereo files only)
    bool stereoIdentical = false;                     // a stereo file whose channels are the same: mono content
    VocalAnalysis tone;                               // levels, tone, dynamics and sibilance of the busiest 30 s (VocalAnalyzer, the same measure as live listening)
    std::vector<float> overview;                      // overviewColumns peak values 0..1 (for the waveform)

    // ESTIMATES
    bool noiseFloorKnown = false;   // only when the take has quiet moments clearly below the singing
    bool digitalSilenceGaps = false; // the quiet moments are digital silence: no noise to hear between phrases
    float noiseFloorDb = -120.0f;   // the level of the quietest moments (room, hiss, breaths, bleed: it cannot tell which)
    float voicedShare = 0.0f;       // share of the active time with a clear pitch (sung notes > rap > speech > noise)
    float voicedSeconds = 0.0f;
    float medianHz = 0.0f, lowHz = 0.0f, highHz = 0.0f; // median and 5-95 % range of the clear pitch
    float offNoteCents = 0.0f;      // median distance of the clear pitch from the nearest equal-tempered note (0 = on a note, 50 = between two)
    bool keyReliable = false;       // the key is reported ONLY when the evidence is strong
    int keyTonic = 0;               // 0 = C ... 11 = B (major); the relative minor has the same notes
    bool keyMinor = false;          // the stronger reading of the two keys that share these notes
    float keyStrength = 0.0f;       // correlation of the pitch-class histogram with the key profile (0..1)
    juce::String keyNote;           // why the key is or is not reported

    bool ok() const { return status == Status::ok; }
};

// Same formats as the reference loader (referenceFileWildcard / referenceFormatNames).
VocalFileReport analyseVocalFile (const juce::File&, const std::atomic<bool>& cancel, const std::function<void (float)>& progress = {});

// Plain-English lines for the interface, MEASURED first, then ESTIMATED; each line names which it is.
juce::StringArray describeVocalFile (const VocalFileReport&);
juce::String keyName (int tonic, bool minor); // e.g. "A minor"

// Runs analyseVocalFile on its own low-priority thread and hands the result back on the message thread. One job at a time.
class VocalFileJob : private juce::Thread
{
public:
    VocalFileJob() : juce::Thread ("GlitchKid vocal file") {}
    ~VocalFileJob() override { stopJob(); }
    bool start (const juce::File&, std::function<void (const VocalFileReport&)> onDone); // false if a job is already running
    bool isRunning() const { return busy.load(); }
    float progress() const { return progressValue.load(); }
    void stopJob(); // message thread: cancels, waits, and guarantees the callback can no longer run

private:
    void run() override;
    struct Shared { std::function<void (const VocalFileReport&)> done; bool alive = true; };
    std::shared_ptr<Shared> shared = std::make_shared<Shared>();
    juce::File file;
    std::atomic<bool> cancelFlag { false }, busy { false };
    std::atomic<float> progressValue { 0.0f };
};
}
