#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

struct EchoSettings
{
    bool on = false;
    bool sync = true;          // true: delay = note division at `bpm`; false: `timeMs`
    float timeMs = 350.0f;     // free time, 1..2000 ms
    int division = 4;          // index into LiquidEcho::divisionName()
    float feedbackPct = 35.0f; // 0..90 %
    float toneHz = 6000.0f;    // low-pass in the repeat path, 400..20000 Hz
    float widthPct = 0.0f;     // 0..100 %: right-channel delay is (1 + 0.5 * width) x the left
    float mixPct = 30.0f;      // 0 = dry only, 100 = repeats only
    float bpm = 120.0f;        // already resolved with GlitchStutter::resolveTempo()
};

// Liquid Echo: a tempo-aware stereo delay with feedback and tone.
//
//  - Each channel has a preallocated circular delay line (4-point Catmull-Rom interpolated read), sized in
//    prepare() for the longest delay (3 s, e.g. 1/2 note at 40 BPM) x the widest right channel (1.5x).
//  - Repeat path (per sample): in + feedback x delayed -> one-pole low-pass (Tone) -> DC blocker (~10 Hz)
//    -> delay line. Both filters are stable at every sample rate and never raise gain at DC or mid band,
//    and feedback is limited to 90 %, so repeats always decay.
//  - Time/tempo changes glide over 80 ms (tape-style: the repeats briefly bend in pitch) instead of
//    clicking. Feedback, tone, mix and on/off ramp over 20 ms.
//  - Width: the right channel's delay is 1 + 0.5 x width times the left's, so a mono vocal gets a stereo echo
//    (at 100 % the right repeats land a dotted-ratio later). It changes timing only, never level.
//  - Mix is a crossfade: out = dry x (1 - mix) + delayed x mix. Off (and settled), the stage is skipped, the
//    delay line is cleared once, and the audio is a bit-exact copy of the input. Switching off fades out and
//    cuts the remaining tail; it does not ring out.
//  - Zero latency. NaN/Inf input is replaced by 0; if the repeat path ever overflows, only its filter memory
//    is cleared (the delay line only ever receives finite values), so one bad sample cannot poison later audio.
//  - 1-2 channels. No allocation after prepare().
class LiquidEcho
{
public:
    static constexpr int numDivisions = 11;
    static constexpr float maxFeedbackPct = 90.0f;
    static constexpr double maxDelaySeconds = 3.0;

    static const char* divisionName (int index);
    static double divisionBeats (int index); // quarter-note beats per delay
    // Delay of the left channel in samples for these settings, clamped to the supported range.
    static double delaySamplesFor (const EchoSettings&, double sampleRate);

    void prepare (double sampleRate);
    void reset(); // clear the delay line and filter memory
    void setSettings (const EchoSettings&); // once per block; audio thread safe
    void process (juce::AudioBuffer<float>&);

    bool isActive() const { return active; }

private:
    float readLine (int ch, double delay) const;
    void snapSmoothers();

    double sampleRate = 48000.0;
    int ringSize = 0, writePos = 0;
    std::vector<float> line[2];

    EchoSettings target;
    bool active = false;   // false = idle: line cleared, output is the input
    bool snap = true;      // jump (not glide) to the next settings
    juce::SmoothedValue<double> delay[2] { juce::SmoothedValue<double> (4.0), juce::SmoothedValue<double> (4.0) };
    juce::SmoothedValue<float> feedback { 0.0f }, toneCoef { 0.5f }, wet { 0.0f };

    double lp[2] {}, dcX[2] {}, dcY[2] {};
    double dcR = 0.999;
};
