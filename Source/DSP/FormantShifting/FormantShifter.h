#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

// Formant shifter (Stage 4.3 prototype, productised in Stage 4.4).
//
// Formant shifter for vocals: moves the spectral ENVELOPE (the vocal-tract resonances that give a voice its
// size and character) while the harmonics, and so the pitch, stay where they are.
//
// Method: short-time Fourier transform (75 % overlap, sqrt-Hann windows). For every frame the spectral
// envelope is estimated by cepstral smoothing (the log spectrum is low-pass "liftered" in the quefrency
// domain, which removes the harmonic ripple and keeps the formant shape; the cut-off follows the pitch
// found in the cepstrum and is tighter for high voices, whose sparse harmonics cannot be separated from the
// envelope; three "true envelope" passes make the envelope follow the harmonic peaks), the envelope is warped
// in frequency by the formant ratio, and the frame's spectrum is multiplied by the REAL gain (warped envelope /
// original envelope), smoothed from frame to frame so it cannot modulate the harmonics, with the overall
// loudness held constant. A real gain changes no phases and moves no spectral lines, so the
// fundamental is untouched by construction; it also needs no pitch detector and treats noise (breath, "s")
// as just another spectrum. Stereo uses ONE gain computed from both channels, so the stereo image cannot move.
//
// Latency: exactly frameSize - 1 samples (1023 at 44.1/48 kHz, 2047 at 88.2/96 kHz), constant.
// Per-sample processing, so the output does not depend on the host block size. All buffers are allocated in
// prepare(); process() allocates nothing, takes no lock and does no I/O. The FFT is a small built-in radix-2.
namespace glitchkid::formant
{
class FormantShifter
{
public:
    static constexpr int maxChannels = 2;
    static constexpr float minShiftSemitones = -12.0f, maxShiftSemitones = 12.0f; // formant ratio 0.5 .. 2

    // STFT frame size for a sample rate (about 21 ms, a power of two).
    static int frameSizeFor (double sampleRate);
    static int latencyFor (double sampleRate) { return frameSizeFor (sampleRate) - 1; }

    // Allocates. Call from the message thread, never while processing.
    void prepare (double sampleRate);
    void reset();

    // Formant shift in semitones of the envelope, clamped to -12..+12 (positive = smaller / brighter voice,
    // negative = bigger / darker). 0 = neutral. Safe to call from any thread while audio is running; the
    // value is applied smoothly (30 ms) at the next frame.
    void setShiftSemitones (float semitones);
    float getShiftSemitones() const { return target.load (std::memory_order_relaxed); }

    // In place. channels: 1 or 2 pointers; the channel count must stay the same until reset().
    void process (float* const* channels, int numChannels, int numSamples);
    void processBlock (const float* in, float* out, int numSamples); // mono convenience, in == out is fine

    int getLatencySamples() const { return frameSize > 0 ? frameSize - 1 : 0; }
    size_t getMemoryBytes() const;

private:
    struct Fft
    {
        void prepare (int n);
        void transform (float* re, float* im, bool inverse) const noexcept; // inverse is NOT scaled
        int size = 0;
        std::vector<int> rev;
        std::vector<float> cosT, sinT;
    };

    void processFrame (int64_t start);

    double sampleRate = 48000.0;
    int frameSize = 0, hop = 0, bins = 0, lifterLength = 0, activeChannels = 0;
    Fft fft;
    std::vector<float> window;
    struct Channel { std::vector<float> in, out, re, im; };
    Channel ch[maxChannels];
    std::vector<float> cepRe, cepIm, logMag, smoothed, trueEnv, binPower, gain, prevLogGain;
    float levelNow = 0.0f;   // smoothed loudness-compensation gain (natural log)

    std::atomic<float> target { 0.0f };
    float shiftNow = 0.0f;   // smoothed shift in semitones, updated once per frame
    float smoothCoef = 0.1f; // per-frame smoothing
    int64_t n = 0;           // index of the next input sample
};
} // namespace glitchkid::formant
