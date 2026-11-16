#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

// SPACE: GlitchKid's algorithmic reverb (original design; the textbook building blocks are public: Schroeder allpass diffusers, a feedback delay
// network with a Householder matrix, frequency-dependent decay filters, modulated delay lines).
//
//   input -> pre-delay -> early reflections (an algorithm's own tap pattern, toned by the character) ---------------------------------+
//                      -> input diffusion (2-6 allpass stages) -> 8-line FDN (Householder mixing, modulated reads, per-line two-band decay   |
//                         filters set from the decay time, optional allpass inside each line, optional octave-up shimmer) -> late ------+
//   wet = early x Early + late x Late -> width (mid/side) -> low cut / high cut -> out = dry x cos(mix) + wet x sin(mix) (equal power)
//
//  - Decay is the real RT60: each line's loop gain is 10^(-3 x length / (RT60 x fs)); highs decay over RT60 x a ratio set by Damping and the
//    character, lows over RT60 x the character's low ratio, split by a one-pole crossover inside the loop.
//  - The eight algorithms differ in line lengths, early-reflection patterns, diffusion stages, density inside the tank and modulation; Experimental
//    adds an octave-up shimmer fed back into the tank. WARM / CLASSIC / MODERN change the decay crossover, the low and high decay ratios, the
//    modulation character and the tone of the early reflections.
//  - Freeze: the input to the tank fades out and the loop gains rise to an RT60 of ten minutes with no damping, so the tail holds.
//  - Switching SPACE off fades its input out and lets the tail ring out; the stage goes idle (bit-exact pass-through, no CPU) once the tail is under
//    -100 dBFS. An algorithm change fades the wet signal out and back in (the tank is cleared), so it never clicks.
//  - Real-time safe: every buffer is allocated in prepare() for the largest size; no allocation, lock or I/O in process(). Zero latency.
//    1 or 2 channels (mono in: mono out, the wet mixed to the one channel). Non-finite input is replaced by 0; the tank is guarded against overflow.
class SpaceReverb
{
public:
    struct Settings
    {
        bool on = false;
        int algorithm = 3;        // see algorithmName()
        int character = 1;        // 0 WARM, 1 CLASSIC, 2 MODERN
        float mix = 0.2f;         // 0..1
        float decaySeconds = 1.8f;// RT60, 0.2..20
        float predelayMs = 20.0f; // 0..250
        float size = 0.5f;        // 0..1: scales every delay of the algorithm from 0.5x to 1.5x
        float diffusion = 0.7f;   // 0..1
        float damping = 0.5f;     // 0..1: how much faster the highs die than the lows
        float lowCutHz = 150.0f;  // on the wet signal
        float highCutHz = 9000.0f;
        float modRateHz = 0.6f;
        float modDepth = 0.3f;    // 0..1
        float width = 1.0f;       // 0..1.5 (side gain of the wet signal)
        float early = 0.5f;       // 0..1
        float late = 0.8f;        // 0..1
        bool freeze = false;
    };

    static constexpr int numAlgorithms = 8, numCharacters = 3, numLines = 8;
    static const char* algorithmName (int);  // Room, Chamber, Plate, Hall, Cathedral, Ambience, Modulated Space, Experimental Space
    static const char* characterName (int);  // Warm, Classic, Modern

    void prepare (double sampleRate);
    void reset();                            // clear every buffer
    void setSettings (const Settings&);      // once per block; audio thread safe
    void process (juce::AudioBuffer<float>&);
    bool isActive() const { return active; }
    double tailSeconds() const;              // for the host: the pre-delay and the decay (a minute when frozen)

private:
    struct Delay // a power-of-two circular buffer
    {
        std::vector<float> buf;
        int mask = 0;
        void allocate (int minSize);
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
        float at (int writePos, int delay) const { return buf[(size_t) ((writePos - delay) & mask)]; }
        float linear (int writePos, float delay) const;
        float cubic (int writePos, float delay) const;
    };
    struct Allpass
    {
        Delay d;
        int length = 1;
        float process (int writePos, float x, float g);
    };

    void configure();          // the algorithm's lengths and patterns at the current size (on a structural change)
    void updateCoefficients(); // decay gains, filters, modulation (every 32 samples)
    float applyCuts (int ch, float x);

    double fs = 48000.0;
    Settings target, shown;
    int structureAlgo = -1, w = 0, idleBlocks = 0;
    bool active = false;

    Delay pre[2], lines[numLines], shimmerBuf;
    Allpass inDiff[2][6], tankAp[numLines];
    int numDiff = 4;
    bool tankDiffusion = false;
    float lineBase[numLines] {}, lineLen[numLines] {}, gLow[numLines] {}, gHigh[numLines] {}, dampState[numLines] {}, dampA = 0.5f, modDepthSamples = 0.0f;
    double lfoPhase[numLines] {}, lfoStep[numLines] {};
    struct Tap { int delay; int channel; float gain; };
    std::array<Tap, 16> taps {};
    int numTaps = 0;
    float erState[2] {}, erA = 0.5f, shimmer = 0.0f, shimmerPhase = 0.0f;
    float cutHpS[2][2] {}, cutLpS[2][2] {}, hpG = 0.0f, hpK = 1.4142f, lpG = 1.0f, lpK = 1.4142f;
    float predelaySamples = 0.0f;

    juce::SmoothedValue<float> mixS { 0.0f }, inGain { 0.0f }, xfade { 1.0f }, freezeS { 0.0f }, sizeS { 1.0f }, predelayS { 0.0f };
    int chunkLeft = 0;
};
