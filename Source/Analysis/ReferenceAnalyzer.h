#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_events/juce_events.h>
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace glitchkid::analysis
{
// The tonal shape of an audio signal on a fixed grid: 19 third-octave bands from 200 Hz to 12.8 kHz, as dB RELATIVE to the mean of the bands, so the loudness of
// the source does not matter and only the balance does. Below 200 Hz is not compared: in a finished mix the bass and drums dominate it.
//
// Two profiles may be compared only when they were made from the same signal definition (`basis`). The plug-in's own display tap is the mono sum of the first two
// channels, so a stereo reference file is reduced the same way: a stereo file is never compared channel by channel against a mono vocal.
struct SpectralProfile
{
    static constexpr int numBands = 19;
    static float bandCentreHz (int band) { return 200.0f * std::pow (2.0f, (float) band / 3.0f); } // 200 Hz ... 12.8 kHz
    enum class Basis { monoSum }; // (left + right) / 2, or the single channel

    bool valid = false;
    Basis basis = Basis::monoSum;
    int usedBands = 0;               // the bands below 0.45 x the sample rate (all 19 at 44.1 kHz and up)
    std::array<float, numBands> shapeDb {};
    float activeSeconds = 0.0f;      // time that passed the activity gate and went into the average
    float totalSeconds = 0.0f;       // time analysed
    float variabilityDb = 0.0f;      // how much the tonal shape moves from frame to frame (an unsteady source is a weaker reference)
    int sourceChannels = 1;
    double sampleRate = 0.0;
    bool fullMix = false;            // set by the caller: a finished song is a full mix, an isolated vocal is not
};

// Turns a stream of mono samples into a SpectralProfile: 4096-point Hann frames with a hop of 2048 samples, third-octave band energies, then an activity gate
// (frames louder than -50 dBFS and within 30 dB of the 90th-percentile frame) and an average of the frame energies. Message thread or a worker thread only;
// never the audio thread (it allocates). The same class serves the reference file and the live vocal, so both are measured identically.
class SpectralAccumulator
{
public:
    static constexpr int fftOrder = 12, fftSize = 1 << fftOrder, hop = fftSize / 2, maxFrames = 6000;
    explicit SpectralAccumulator (double sampleRate);
    void pushSamples (const float* mono, int numSamples);   // any block size
    void resetStream();                                      // forget the partial frame (after a gap in the input); keeps the frames already measured
    int frameCount() const { return (int) levelDb.size(); }
    SpectralProfile finish (int sourceChannels) const;
    float framesSeconds() const { return (float) frameCount() * (float) hop / (float) sampleRate; }

private:
    void processFrame (const float* frame);
    double sampleRate;
    int usedBands = 0;
    std::vector<float> window, buffer;
    size_t start = 0;
    juce::dsp::FFT fft { fftOrder };
    std::vector<float> fftData;
    std::vector<float> levelDb;                                         // per frame: RMS in dBFS
    std::vector<std::array<float, SpectralProfile::numBands>> bandPower; // per frame: energy per band (linear)
    std::array<std::pair<int, int>, SpectralProfile::numBands> binRange {};
};

// The audio formats this build can really read, taken from JUCE's registered formats (nothing is advertised that is not registered). Used by the file chooser and by
// the error messages, so what the user is told always matches what the build can decode.
juce::String referenceFileWildcard();  // e.g. "*.wav;*.bwf;*.aiff;*.aif;*.flac;*.ogg;*.mp3" (plus the system formats on macOS)
juce::String referenceFormatNames();   // e.g. "WAV, AIFF, FLAC, Ogg Vorbis, MP3"

// Opens a local audio file with the formats above (by extension, then by content); nullptr if it cannot be read. Worker or message thread only.
std::unique_ptr<juce::AudioFormatReader> openAudioFile (const juce::File&);

// Length of an audio file in seconds without analysing it (0 if it cannot be opened). Reads only the header.
double referenceFileSeconds (const juce::File&);

struct ReferenceSection
{
    bool automatic = true;       // true: the most active `lengthSeconds` of the file
    double startSeconds = 0.0;   // used when automatic is false
    double lengthSeconds = 20.0;
};

struct ReferenceResult
{
    enum class Status { ok, cancelled, missingFile, unsupported, unreadable, silent, tooShort, sampleRateTooLow };
    Status status = Status::unreadable;
    juce::String message;        // plain English, for the interface
    SpectralProfile profile;     // valid only when status == ok
    double sectionStartSeconds = 0.0, sectionLengthSeconds = 0.0, fileSeconds = 0.0;
};

// Reads and analyses one local audio file on the CALLING thread (call it from a worker, never from the audio thread). Cancellable through `cancel`, with progress 0..1.
// Formats: whatever JUCE's basic formats read (WAV, AIFF, FLAC, Ogg Vorbis; MP3 is not enabled). The file is only read: nothing is copied, kept or written.
ReferenceResult analyseReferenceFile (const juce::File&, const ReferenceSection&, const std::atomic<bool>& cancel, const std::function<void (float)>& progress = {});

// Runs analyseReferenceFile on its own low-priority thread and hands the result back on the message thread. One job at a time.
class ReferenceJob : private juce::Thread
{
public:
    ReferenceJob() : juce::Thread ("GlitchKid reference") {}
    ~ReferenceJob() override { stopJob(); }
    bool start (const juce::File&, const ReferenceSection&, std::function<void (const ReferenceResult&)> onDone); // false if a job is already running
    void cancel() { cancelFlag.store (true); }
    bool isRunning() const { return busy.load(); }
    float progress() const { return progressValue.load(); }
    void stopJob(); // message thread: cancels, waits, and guarantees the callback can no longer run

private:
    void run() override;
    struct Shared { std::function<void (const ReferenceResult&)> done; bool alive = true; };
    std::shared_ptr<Shared> shared = std::make_shared<Shared>();
    juce::File file;
    ReferenceSection section;
    std::atomic<bool> cancelFlag { false }, busy { false };
    std::atomic<float> progressValue { 0.0f };
};
}
