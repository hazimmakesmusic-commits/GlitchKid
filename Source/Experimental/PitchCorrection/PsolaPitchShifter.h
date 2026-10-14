#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

// EXPERIMENTAL (Stage 4.1). Streaming pitch shifter for ONE voice, using time-domain pitch-synchronous
// overlap-add (TD-PSOLA style): the input is cut into Hann-windowed grains two periods long, taken one
// period apart, and the grains are laid back down at a different spacing. Spacing = period / ratio, so
// the pitch changes by `ratio` while the duration is kept by repeating or skipping whole periods.
//
// Grains are NOT aligned to glottal pulses (no pitch-mark search): they are simply one detected period
// apart. That is enough for a feasibility test; mark alignment is a known quality upgrade.
//
// It needs the current period of the input from outside (setPeriod). The output is the input delayed by
// a constant getLatencySamples(), whatever the period or ratio. No allocation after prepare().
namespace glitchkid::pitch
{
class PsolaPitchShifter
{
public:
    static constexpr float minRatio = 0.5f, maxRatio = 2.0f;

    void prepare (double sampleRate, float lowestF0Hz);
    void reset();

    // Period of the input voice in samples. Clamped to what prepare() allows.
    void setPeriod (float samples);
    // Output pitch / input pitch, clamped to 0.5 .. 2.
    void setRatio (float ratio);

    float process (float x);

    int getLatencySamples() const { return latency; }
    float getDefaultPeriod() const { return defaultPeriod; }
    size_t getMemoryBytes() const { return (in.size() + acc.size() + wsum.size()) * sizeof (float); }

private:
    void emitGrain();
    float readInput (double pos) const;

    std::vector<float> in, acc, wsum;
    int64_t mask = 0;
    int latency = 0;
    float minPeriod = 20.0f, maxPeriod = 600.0f, defaultPeriod = 320.0f;

    float period = 320.0f, ratio = 1.0f;
    int64_t n = 0;      // index of the sample being processed
    double sc = 0.0;    // where the next grain goes in the output
    double ac = 0.0;    // where the next grain is taken from in the input
    bool first = true;
};
} // namespace glitchkid::pitch
