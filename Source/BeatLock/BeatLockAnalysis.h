#pragma once
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>
#include "BeatLockParams.h"

// LEARN and ANALYZE for GLITCHKID BEAT LOCK.
//
// AnalysisFifo: the audio thread writes (vocal mono, instrumental mono, vocal-active flag) per sample while a capture is armed; the message thread drains it.
// Single producer, single consumer (juce::AbstractFifo), preallocated in the constructor: the writer never allocates, locks or waits. When the reader falls
// behind, the newest samples that do not fit are dropped and counted (never blocking the audio thread).
//
// ConflictAnalysis: message thread only. Hann-windowed 4096-point FFT frames (50 % overlap) of both signals. Only frames in which the audio thread judged the
// vocal active (at least 60 % of the frame's samples) count. For each third-octave region (100 Hz - 12.5 kHz) it accumulates the vocal's and the
// instrumental's average level, and how often the instrumental is within 6 dB of a relevant vocal region (a conflict). Nothing is estimated without data:
// with too little active vocal it reports exactly that.
namespace glitchkid::beatlock
{
class AnalysisFifo
{
public:
    static constexpr int capacity = 1 << 17; // about 2.7 s at 48 kHz

    AnalysisFifo() : fifo (capacity), vocal ((size_t) capacity), inst ((size_t) capacity), gate ((size_t) capacity) {}

    void setArmed (bool a) { armed.store (a, std::memory_order_release); }
    bool isArmed() const { return armed.load (std::memory_order_acquire); }

    // audio thread
    void write (const float* v, const float* i, const float* g, int n) noexcept;
    // message thread: up to `max` samples into the three arrays, returns how many
    int read (float* v, float* i, float* g, int max) noexcept;
    void clear() noexcept; // message thread, while disarmed
    int dropped() const { return droppedCount.load (std::memory_order_relaxed); }

private:
    juce::AbstractFifo fifo;
    std::vector<float> vocal, inst, gate;
    std::atomic<bool> armed { false };
    std::atomic<int> droppedCount { 0 };
};

struct Region
{
    float centreHz = 0.0f;
    float vocalDb = -120.0f, instDb = -120.0f; // average level over the active-vocal frames (dBFS, power per third octave)
    float conflictPct = 0.0f;                  // % of active-vocal frames in which this region conflicted
    bool vocalRelevant = false;                // the vocal has meaningful energy here (within 20 dB of its strongest region, above -70 dB)
};

struct AnalysisResult
{
    enum class Status { noSidechain, insufficientVocal, ready };
    Status status = Status::insufficientVocal;
    juce::String message;                     // one honest line
    float activeSeconds = 0.0f, totalSeconds = 0.0f;
    std::vector<Region> regions;
    std::vector<int> strongest;               // indices into regions, strongest conflict first (at most 3; only regions that really conflicted)
    float medianInstMinusVocalDb = 0.0f;      // over relevant regions of active frames
};

struct LearnProposal
{
    bool valid = false;
    std::array<float, numBands> freqHz {};
    float sensitivityDb = 0.0f;
    juce::StringArray reasons;
};

class ConflictAnalysis
{
public:
    enum class Mode { learn, analyze };
    static constexpr int fftOrder = 12, fftSize = 1 << fftOrder, hop = fftSize / 2;
    static constexpr float minActiveSeconds = 2.0f;

    ConflictAnalysis();
    void start (Mode, double sampleRate, float targetActiveSeconds, float timeoutSeconds);
    void feed (const float* v, const float* i, const float* g, int n); // any amount; frames are cut internally
    bool finished() const { return done; }
    bool running() const { return started && ! done; }
    float progress() const;                    // 0..1 toward the target amount of active vocal
    float activeSeconds() const { return active; }
    float elapsedSeconds() const { return total; }
    Mode mode() const { return currentMode; }
    void cancel() { started = false; done = false; }

    AnalysisResult result() const;
    // From a ready result: band frequencies at the vocal's most-conflicted regions (at least ~0.6 octave apart), ordered low to high, filled from the
    // vocal's strongest remaining regions, then from the current settings; sensitivity from the measured median gap. Invalid without enough data.
    static LearnProposal propose (const AnalysisResult&, const std::array<float, numBands>& currentFreqs);

    static const std::vector<float>& regionCentres(); // third-octave centres, 100 Hz - 12.5 kHz

private:
    void frame();
    Mode currentMode = Mode::analyze;
    double rate = 48000.0;
    float target = 6.0f, timeout = 30.0f, active = 0.0f, total = 0.0f;
    bool started = false, done = false;
    juce::dsp::FFT fft { fftOrder };
    std::vector<float> window, bufV, bufI, bufG, work;
    int fill = 0;
    std::vector<double> sumV, sumI;
    std::vector<int> conflictFrames;
    int activeFrames = 0;
    std::array<int, 161> gapHistogram {};      // I - V per relevant region, -80..+80 dB in 1 dB bins
    int gapCount = 0;
};
}
