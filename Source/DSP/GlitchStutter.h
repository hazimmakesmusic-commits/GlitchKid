#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

struct StutterSettings
{
    bool on = false;
    int rateIndex = 1;       // 0 = 1/4, 1 = 1/8, 2 = 1/16, 3 = 1/32 note
    float mixPercent = 100.0f;
    float bpm = 120.0f;      // already resolved with GlitchStutter::resolveTempo()
};

// Glitch Stutter (beat repeat): while on, the stage repeats the most recent slice of the incoming audio
// (the slice that played just before it was switched on), one slice length = one rhythmic step.
//
//  - Real audio only: the slice is copied from a continuously recorded history of this stage's input.
//    Nothing is generated; the loop never feeds back into the history, so it cannot build up or run away.
//  - Slice length = rate (1/4..1/32 note) at the given tempo, rounded to whole samples. The loop starts
//    when the effect is switched on: its repeats are as long as the rhythmic step, but not locked to the
//    host beat grid (switch it on at the moment you want the grid to start; automation clips in a DAW are
//    normally on the grid).
//  - Clicks: engaging and releasing are 3 ms fades between the live audio and the loop; the loop seam is a
//    crossfade into the audio that preceded the slice (up to 4 ms), so the repeat is continuous.
//  - Changing the rate (or a tempo change of more than 2 %) while on re-captures the slice through the
//    same 3 ms fades.
//  - Mix is a wet/dry blend of loop and live audio. At 0 % (or off) the stage is skipped and the audio is
//    a bit-exact copy of the input.
//  - Behaviour is per sample, so it does not depend on how the host splits audio into blocks.
//  - 1-2 channels. Latency 0. All memory is allocated in prepare().
class GlitchStutter
{
public:
    static constexpr int numRates = 4;
    static constexpr float fallbackBpm = 120.0f, minBpm = 40.0f, maxBpm = 300.0f;

    static double rateInBeats (int rateIndex); // quarter-note beats per slice: 1, 0.5, 0.25, 0.125

    // Turn a host tempo (maybe missing, NaN, zero or absurd) into a usable one. `usedHost` is false when the
    // fallback (120 BPM) was used. Valid host tempos are clamped to 40..300 BPM.
    static float resolveTempo (bool hostHasTempo, double hostBpm, bool& usedHost);

    void prepare (double sampleRate);
    void reset();
    void setSettings (const StutterSettings&); // once per block; audio thread safe
    void process (juce::AudioBuffer<float>&);

    int getLoopLengthSamples() const { return state == State::idle ? 0 : loopLength; }
    bool isActive() const { return state != State::idle; }

private:
    enum class State { idle, active, releasing };

    int lengthFor (int rateIndex, float bpm) const;
    void capture (int rateIndex, float bpm);
    float loopSample (int channel) const;

    double sampleRate = 48000.0;
    int maxLength = 0, maxCrossfade = 0, ringSize = 0;
    std::vector<float> ring[2], frozen[2];
    int writePos = 0;

    State state = State::idle;
    int loopLength = 0, crossfade = 0, playPos = 0, loopRate = 0;
    float loopBpm = 120.0f;

    bool wantOn = false;
    int wantedRate = 1;
    float wantedBpm = 120.0f;
    juce::SmoothedValue<float> ramp { 0.0f };   // 3 ms engage / release
    juce::SmoothedValue<float> mixAmount { 1.0f }; // 20 ms
};
