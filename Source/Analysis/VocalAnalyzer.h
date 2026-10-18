#pragma once
#include <array>
#include <atomic>
#include <cstdint>

namespace glitchkid::analysis
{
// Plain-data result of one analysis pass. Every number is an ESTIMATE from the audio that went through the plug-in while it was
// listening; none of it identifies a key, a scale or "the ideal mix". Levels are dBFS (digital, not LUFS). Band fractions are shares of
// the energy of the active (voiced/loud enough) frames, not calibrated spectra.
struct VocalAnalysis
{
    enum class Status { idle = 0, listening, ready, tooShort, tooQuiet, invalid };
    static constexpr int numBands = 5; // < 120 Hz, 120-500, 500-2k, 2k-5k, > 5k

    Status status = Status::idle;
    float totalSeconds = 0.0f;   // audio seen while listening
    float activeSeconds = 0.0f;  // part of it above the activity gate (-50 dB per 100 ms frame)
    float peakDb = -120.0f;      // highest sample magnitude
    float rmsDb = -120.0f;       // RMS over the active frames
    int clippedSamples = 0;      // samples at or above 0.999 full scale
    int nonFiniteSamples = 0;    // NaN/Inf samples seen (ignored by the measurements)
    float dynP10Db = -120.0f, dynP50Db = -120.0f, dynP90Db = -120.0f; // percentiles of the 100 ms frame RMS over active frames
    float dynamicRangeDb = 0.0f; // dynP90 - dynP10: an indicator of how much the level moves, not a loudness-range standard
    std::array<float, numBands> bandFraction {};
    float sibilanceIndex = 0.0f; // share of active frames whose > 5 kHz energy exceeds half of the 500 Hz - 5 kHz energy: a rough estimate only
};

// Listens to the plug-in input (mono sum of the first two channels) only between start() and stop()/automatic end, from the audio thread.
// No allocation, locks or I/O in process(). Results cross to the UI through atomics: the status is published last, with release ordering,
// and read first, with acquire ordering, so a final result is always complete. start()/stop() are message-thread requests picked up at the
// next block. Analysis advances only while the host plays audio through the plug-in.
class VocalAnalyzer
{
public:
    static constexpr float minActiveSeconds = 2.0f;   // fewer active seconds than this is "too short"
    static constexpr float maxActiveSeconds = 8.0f;   // enough: finishes by itself
    static constexpr float maxTotalSeconds = 30.0f;   // gives up (with whatever it has) so it never listens forever

    void prepare (double sampleRate);                  // allocation-free; may be called again on a rate change
    void start() { request.store (1, std::memory_order_release); }  // begin a fresh analysis at the next block
    void stop() { request.store (2, std::memory_order_release); }   // finish now with what has been heard

    void process (const float* const* channels, int numChannels, int numSamples) noexcept; // audio thread

    bool isListening() const { return status.load (std::memory_order_acquire) == (int) VocalAnalysis::Status::listening; }
    float liveActiveSeconds() const { return liveActive.load (std::memory_order_relaxed); }
    float liveTotalSeconds() const { return liveTotal.load (std::memory_order_relaxed); }
    VocalAnalysis fetch() const;                       // message thread: the last published result (status idle/listening carry no measurements)

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void setLowpass (double sampleRate, double freq);
        double tick (double x) noexcept { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
        void reset() { z1 = z2 = 0; }
    };
    static constexpr int numLowpass = VocalAnalysis::numBands - 1;
    static constexpr int histBins = 81; // 1 dB bins, -80..0 dBFS (frame RMS)

    void reset() noexcept;
    void finish() noexcept;
    void publish (const VocalAnalysis&) noexcept;

    double fs = 48000.0;
    int frameLength = 4800;
    std::array<Biquad, numLowpass> lp;

    // audio-thread accumulators
    int framePos = 0;
    double frameTotal = 0.0;
    std::array<double, VocalAnalysis::numBands> frameBand {};
    std::array<double, VocalAnalysis::numBands> bandSum {};
    double activeEnergySum = 0.0;
    long activeFrames = 0, totalSamples = 0;
    long sibilantFrames = 0;
    std::array<int, histBins> hist {};
    float peak = 0.0f;
    int clipped = 0, nonFinite = 0;
    bool listening = false;

    std::atomic<int> request { 0 };
    std::atomic<int> status { (int) VocalAnalysis::Status::idle };
    std::atomic<float> liveActive { 0.0f }, liveTotal { 0.0f };
    static constexpr int numFields = 16;
    std::array<std::atomic<float>, numFields> fields {};
};
}
