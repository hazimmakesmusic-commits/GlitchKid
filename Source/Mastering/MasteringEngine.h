#pragma once
#include "MasterDsp.h"
#include "../DSP/ParametricEq.h"
#include "../DSP/Compressor.h"
#include "../DSP/Saturation.h"
#include "../DSP/FixedDelay.h"

// GLITCHKID MASTERING: a separate chain for a finished stereo mix (the plug-in on the master bus in Mastering mode, or an offline render of a file).
// It never runs on the vocal chain. Order:
//   input meter -> EQ (corrective + tone, the existing ParametricEq) -> glue compressor (the existing Compressor, threshold set relative to the mix's own
//   integrated loudness, so a quiet and a loud mix are treated alike) -> saturation (the existing Saturation, 4x oversampled) -> width (mid untouched)
//   -> true-peak limiter (drive = Loudness, ceiling -1 dBTP) -> output trim (never above 0 dB) -> output meter.
// ORIGINAL (the compare) is the input delayed by the chain's latency and played at the master's measured loudness, so the louder version never wins.
namespace glitchkid::master
{
enum class Style { cleanBalanced = 0, warmSmooth, loudPunchy, wideAtmospheric };
constexpr int numStyles = 4;
const char* styleName (Style);
const char* styleDescription (Style);

struct Controls
{
    Style style = Style::cleanBalanced;
    float intensity = 0.6f;   // 0..1: how much of the style's EQ, compression, saturation and width is used
    float loudnessDb = 3.0f;  // 0..10: drive into the limiter
    float tone = 0.0f;        // -1 (darker) .. +1 (brighter)
    float width = 1.0f;       // 0..2, multiplies the style's width
    float outputDb = 0.0f;    // -12..0, after the limiter
};

// The real processing a set of controls resolves to (pure and deterministic; the tests check that the styles really differ).
struct Chain
{
    std::array<EqBandSettings, 4> eq;
    CompressorSettings comp;      // thresholdDb here is RELATIVE to the integrated loudness of the input (LU above it)
    float satDriveDb = 0.0f;
    float width = 1.0f;
    float driveDb = 3.0f, ceilingDbTp = -1.0f, releaseMs = 120.0f;
    float outputDb = 0.0f;
};
Chain resolve (const Controls&);

class MasteringEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void setControls (const Controls&);
    void setCompareOriginal (bool original) { compareOriginal = original; }
    int latencySamples() const { return Saturation::latencySamples + limiter.latencySamples(); }
    void process (juce::AudioBuffer<float>&); // 1-2 channels, in place, any block size up to maxBlockSize

    const LoudnessMeter& inputMeter() const { return inMeter; }
    const LoudnessMeter& outputMeter() const { return outMeter; }
    float limiterReductionDb() const { return limiter.gainReductionDb(); }
    float compressorReductionDb() const { return compGr.load (std::memory_order_relaxed); }
    float compareGainDb() const { return matchDb.load (std::memory_order_relaxed); } // what ORIGINAL is raised by to match the master
    void resetMeters() { inMeter.reset(); outMeter.reset(); wetMeter.reset(); }

private:
    double fs = 48000.0;
    Controls controls;
    Chain chain;
    ParametricEq eq;
    Compressor comp;
    Saturation sat;
    StereoWidth width;
    MasterLimiter limiter;
    LoudnessMeter inMeter, outMeter, wetMeter; // wet: the master itself, before the compare switch (for the loudness match)
    FixedDelay dryDelay;
    juce::AudioBuffer<float> dry;
    juce::SmoothedValue<float> wet { 1.0f }, trim { 1.0f }, match { 1.0f };
    bool compareOriginal = false;
    float inputPeakHold = 0.0f; // so a level-matched ORIGINAL is never raised into clipping
    std::atomic<float> compGr { 0.0f }, matchDb { 0.0f };
};
}
