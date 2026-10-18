#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

struct DoublerSettings
{
    bool on = false;
    float amountPct = 50.0f; // 0..100: level of the doubled voices
    float widthPct = 70.0f;  // 0..100: 0 = doubles in the centre, 100 = pure stereo spread (mono-safe)
    float detuneCents = 8.0f; // 0..25: the largest pitch wobble of a double, in cents
    float timingMs = 18.0f;  // 5..45: how far behind the lead the doubles sit (average)
};

// Vocal Doubler (Stage 4.8): a SYNTHETIC double. Two extra voices are made from the one input vocal; each is a delayed copy of the
// lead, a little late, a little unsteady in time and in pitch, and a little darker, so the result thickens and widens the vocal the way a
// second recorded take does. It is NOT a second take: nothing here changes the words, the phrasing or the vibrato, only the timing and pitch
// wobble are made up, so on long held notes or very exposed vocals it sounds like what it is (a studio "ADT"-style double).
//
//  - Timing: the voices sit 0.85 x and 1.25 x TIMING behind the lead (5-45 ms), so they are separated from the lead and from each other.
//  - Drift: each voice's delay wanders along a smooth random path (cosine-interpolated between random points every 0.28-0.66 s, different for
//    each voice, deterministic seeds: the same input always gives the same output). The wander's speed is a pitch wobble; its size is scaled so
//    the largest wobble equals DETUNE cents. Unlike Dimension's steady sine-wave chorus this never repeats, and unlike Dimension the doubles
//    keep their low end and their timing is yours to set. The mean pitch is not shifted (it is a wobble, not a detune).
//  - Each double is also softened by a one-pole low-pass at 8 kHz, so doubled "s" and "t" sounds stay less lispy than the lead.
//  - Width: the doubles are split into centre m = (v1+v2)/2 and side s = (v1-v2)/2: L += g (cos a m + sin a s), R += g (cos a m - sin a s),
//    a = Width x 90 degrees. At Width 100 only the side is added, which cancels exactly when L and R are summed to mono (the mono sum equals the
//    dry input); at lower Width the centre part is added too, which makes the vocal thicker but gives a mono sum some comb colouring (measured in the tests).
//  - Level: g = 0.7 x Amount (a double at 100 % is about 3 dB below the lead, and the output is at most a few dB above the input).
//  - Off (and settled): skipped, bit-exact, memory cleared once. ZERO latency: the lead is never delayed. The input is summed to mono for the
//    doubles (a stereo input keeps its own L/R and gets the doubles added). NaN/Inf input is replaced by 0. No allocation after prepare().
class VocalDoubler
{
public:
    static constexpr float maxWet = 0.7f;
    static constexpr float lowpassHz = 8000.0f;
    static constexpr float minTimingMs = 5.0f, maxTimingMs = 45.0f, maxDetuneCents = 25.0f;
    static constexpr double delayScale[2] = { 0.85, 1.25 };
    static constexpr double driftSeconds[2] = { 0.37, 0.53 }; // average segment length of each voice's wander

    void prepare (double sampleRate);
    void reset();
    void setSettings (const DoublerSettings&); // once per block; audio-thread safe
    void process (juce::AudioBuffer<float>&);  // 1 or 2 channels

    bool isActive() const { return active; }
    double getVoiceDelaySamples (int voice) const { return lastDelay[voice]; } // for tests: where voice 0 or 1 is reading now

private:
    struct Wander
    {
        std::uint32_t state = 1;
        double from = 0.0, to = 0.0;  // path end points, in -1..+1
        double pos = 0.0, length = 1.0; // samples into this segment / its length in samples
        double averageSeconds = 0.4;
        double value (double sampleRate);
        void reseed (std::uint32_t seed, double avgSeconds, double sampleRate);
    private:
        double unit();
        void next (double sampleRate);
    };

    float readVoice (double delaySamples) const;

    double sampleRate = 48000.0;
    int ringSize = 0, writePos = 0;
    std::vector<float> ring;
    DoublerSettings target;
    bool active = false;
    juce::SmoothedValue<float> wet { 0.0f }, widthAngle { 0.0f }, excursion { 0.0f }, timing { 18.0f }; // excursion in samples
    Wander wander[2];
    double lowState[2] { 0.0, 0.0 };
    double lowCoef = 0.5;
    double lastDelay[2] { 0.0, 0.0 };
};
