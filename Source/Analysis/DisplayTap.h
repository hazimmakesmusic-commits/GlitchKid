#pragma once
#include <atomic>
#include <array>
#include <cmath>
#include <cstdint>

namespace glitchkid::analysis
{
// A bounded, lock-free copy of the most recent audio, for the interface's waveform and spectrum views only.
// The audio thread writes (push: no allocation, no locks, no waiting; a non-finite sample is stored as 0); the message thread reads (copyLatest).
// One writer, one reader. The reader can see a block that is half old and half new at the very edge of the window: that is harmless for a display and
// is why this is never used for any processing decision. Samples are stored mono (the average of the first two channels).
class DisplayTap
{
public:
    static constexpr std::uint32_t size = 8192; // about 170 ms at 48 kHz; a power of two so the index can wrap

    void push (const float* left, const float* right, int numSamples) noexcept
    {
        if (left == nullptr || numSamples <= 0)
            return;
        std::uint32_t w = writePos.load (std::memory_order_relaxed);
        for (int i = 0; i < numSamples; ++i)
        {
            const float v = right != nullptr ? 0.5f * (left[i] + right[i]) : left[i];
            data[(w + (std::uint32_t) i) & (size - 1)].store (std::isfinite (v) ? v : 0.0f, std::memory_order_relaxed);
        }
        writePos.store (w + (std::uint32_t) numSamples, std::memory_order_release);
    }

    // How many samples have ever been written (wraps at 2^32). The reader compares it between looks: a count that stops moving means nothing is arriving.
    std::uint32_t writeCount() const noexcept { return writePos.load (std::memory_order_acquire); }

    // The newest `n` samples (n <= size), oldest first. Samples that were never written are 0.
    void copyLatest (float* dst, int n) const noexcept
    {
        if (dst == nullptr || n <= 0 || (std::uint32_t) n > size)
            return;
        const std::uint32_t w = writePos.load (std::memory_order_acquire);
        for (int i = 0; i < n; ++i)
            dst[i] = data[(w - (std::uint32_t) n + (std::uint32_t) i) & (size - 1)].load (std::memory_order_relaxed);
    }

private:
    std::array<std::atomic<float>, size> data {};
    std::atomic<std::uint32_t> writePos { 0 };
};
}
