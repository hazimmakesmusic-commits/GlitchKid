#pragma once
#include <cstddef>
#include <vector>

// EXPERIMENTAL (Stage 4.1): not part of the plugin. Compiled only into the test target and PitchLab.
//
// YIN fundamental-frequency detector (de Cheveigne & Kawahara, 2002), written from the published
// algorithm: difference function -> cumulative mean normalised difference -> absolute threshold ->
// parabolic interpolation. The input is averaged down to roughly 24 kHz first, so the cost does not
// grow with the sample rate. All buffers are allocated in prepare(); push() never allocates.
namespace glitchkid::pitch
{
class YinPitchDetector
{
public:
    struct Result
    {
        float frequencyHz = 0.0f;   // 0 when nothing usable was found
        float confidence = 0.0f;    // 1 - normalised difference at the chosen lag, 0..1
        bool voiced = false;        // periodic enough to trust
        float periodSamples = 0.0f; // period at the full sample rate
    };

    void prepare (double sampleRate, float minHz = 70.0f, float maxHz = 900.0f);
    void reset();

    // Absolute YIN threshold. Lower = stricter (fewer wrong octaves, more unvoiced frames).
    void setThreshold (float t) { threshold = t; }

    // Feed one sample (non-finite or absurdly large values count as silence). Returns true when a
    // fresh estimate is available in result() (once per hop, about every 5 ms).
    bool push (float x);
    const Result& result() const { return last; }

    // Length of the analysis window at the full sample rate: an estimate describes roughly the
    // last this-many samples.
    int getWindowSamples() const { return windowSamples; }
    size_t getMemoryBytes() const { return (ring.size() + lin.size() + cmndf.size()) * sizeof (float); }

private:
    void analyse();

    double sr = 48000.0;
    int factor = 1;       // averaging factor down to the analysis rate
    int accCount = 0;
    double accSum = 0.0;
    int minLag = 2, maxLag = 2, window = 2, hop = 1, sinceHop = 0, filled = 0, writePos = 0;
    int windowSamples = 0;
    double decRate = 16000.0;
    float threshold = 0.15f;
    std::vector<float> ring, lin, cmndf;
    Result last;
};
} // namespace glitchkid::pitch
