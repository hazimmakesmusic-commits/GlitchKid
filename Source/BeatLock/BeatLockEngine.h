#pragma once
#include <array>
#include <atomic>
#include <cmath>
#include "BeatLockParams.h"

// GLITCHKID BEAT LOCK: the DSP engine. The INSTRUMENTAL (main input) is processed; the VOCAL (sidechain) is only listened to.
//
//   sidechain (mono sum) -> speech band (HP 100 Hz, LP 10 kHz) -> vocal activity detector (adaptive noise floor, hysteresis, hold) + voiced estimate
//                        -> 8 band-pass detectors -> V_b (dB)
//   instrumental L/R     -> 8 band-pass detectors per channel -> linked power mean -> I_b (dB)
//   every 16 samples:    conflict -> target cut per band -> neighbour smoothing -> attack/release -> C_b
//   instrumental L/R     -> 8 dynamic bells (minimum-phase TPT state-variable filters, one coefficient set for both channels) -> output
//
// The vocal never reaches the output: the engine only reads the sidechain pointers. Zero latency (nothing is buffered). A band whose cut is exactly 0 dB is not
// processed at all, so with no sidechain, a silent vocal, AMOUNT 0 or after bypass the output is bit-identical to the input once the release has finished.
// Real-time safe: no allocation, locks or I/O after prepare(). Control decisions are made at fixed 16-sample steps counted across blocks, so the result does not
// depend on the host's block size.
namespace glitchkid::beatlock
{
struct BandSettings
{
    bool on = true;
    float freqHz = 1000.0f;
    float q = 1.4f;
    float maxCutDb = -12.0f;   // <= 0
};

struct Settings
{
    float amount = 0.5f;       // 0..1
    float maxCutDb = -4.0f;    // <= 0
    float sensitivityDb = 0.0f;
    float focusLow = 0.5f, focusMid = 1.0f, focusHigh = 0.75f; // 0..1
    float attackMs = 15.0f, releaseMs = 180.0f;
    bool bypass = false;
    std::array<BandSettings, numBands> bands {};
    Settings();
};

// One topology-preserving-transform state-variable filter (Simper). Response (s normalised to the warped centre, k = 1/Q):
//   v1 = s / (s^2 + k s + 1) * x,  v2 = 1 / (s^2 + k s + 1) * x.   Band-pass (0 dB peak) = k v1, low-pass = v2, high-pass = x - k v1 - v2.
//   Bell: y = x + k (A^2 - 1) v1 with k = 1 / (Q A), A = 10^(dB/40):  H = (s^2 + k A^2 s + 1) / (s^2 + k s + 1), |H(centre)| = A^2 = dB.
struct Svf
{
    double g = 0.0, k = 1.0, a1 = 1.0, a2 = 0.0, a3 = 0.0;
    void set (double fs, double hz, double q);
    void setG (double fs, double hz);              // only g (the bell recomputes k and a1..a3 per sample from its gain)
    void coeffs() { a1 = 1.0 / (1.0 + g * (g + k)); a2 = g * a1; a3 = g * a2; }
    struct State { double ic1 = 0.0, ic2 = 0.0; void clear() { ic1 = ic2 = 0.0; } };
    inline void tick (State& s, double x, double& v1, double& v2) const
    {
        const double v3 = x - s.ic2;
        v1 = a1 * s.ic1 + a2 * v3;
        v2 = s.ic2 + a2 * s.ic1 + a3 * v3;
        s.ic1 = 2.0 * v1 - s.ic1;
        s.ic2 = 2.0 * v2 - s.ic2;
    }
    // the exact magnitude (dB) of a bell at `hz` (this filter's centre/Q, `gainDb`), for drawing and tests
    static double bellResponseDb (double hz, double centreHz, double q, double gainDb, double fs);
};

enum class SidechainStatus { notConnected = 0, silent = 1, live = 2 };

class Engine
{
public:
    static constexpr int maxChannels = 2;
    static constexpr int controlStep = 16;

    void prepare (double sampleRate);
    void reset();                                   // clears every filter, envelope and cut (a fresh start)
    void setSettings (const Settings& s) { settings = s; }
    const Settings& getSettings() const { return settings; }

    // In place on the instrumental. `sc` may be null or have 0 channels (no sidechain). Optional per-sample outputs for the analysis FIFO and displays:
    // scMonoOut (the vocal, mono), gateOut (1 while the vocal is judged active, else 0); each may be null, else it must hold n samples.
    void process (float* const* main, int numMain, const float* const* sc, int numSc, int n, float* scMonoOut = nullptr, float* gateOut = nullptr);

    // Readouts (any thread): the live state, updated every control step.
    float cutDb (int band) const { return shownCut[(size_t) band].load (std::memory_order_relaxed); }
    float vocalBandDb (int band) const { return shownV[(size_t) band].load (std::memory_order_relaxed); }
    float instrumentalBandDb (int band) const { return shownI[(size_t) band].load (std::memory_order_relaxed); }
    float gate() const { return shownGate.load (std::memory_order_relaxed); }       // 0..1
    float voiced() const { return shownVoiced.load (std::memory_order_relaxed); }   // 0..1
    float vocalLevelDb() const { return shownLevel.load (std::memory_order_relaxed); }
    float noiseFloorDb() const { return shownFloor.load (std::memory_order_relaxed); }
    bool anyBandActive() const;

    // focus weight of a band at `hz` (smooth crossovers around 500 Hz and 2.5 kHz in log frequency)
    static float focusWeight (float hz, float low, float mid, float high);

private:
    void control (bool connected);
    double coef (double seconds, double rate) const { return 1.0 - std::exp (-1.0 / (seconds * rate)); }

    Settings settings;
    double fs = 48000.0, controlRate = 3000.0;
    int stepLeft = 0;

    // sidechain: speech band, activity, voiced estimate
    Svf scHp, scLp, scVoicedLp, scVoicedHp;
    Svf::State scHpS, scLpS, scVoicedLpS, scVoicedHpS;
    double levelEnv = 0.0, lowEnv = 0.0, highEnv = 0.0;
    double aLevelAtt = 0, aLevelRel = 0, aVoiceEnv = 0;
    double floorDb = -100.0, peakDb = -120.0, gateSm = 0.0, voicedSm = 0.0;
    bool floorInit = false, gateOpen = false;
    int holdLeft = 0;
    int acquireLeft = 0;    // after a (re)connection: the noise floor is learned for 100 ms before the gate may open
    int lastNumMain = 2;
    bool anyBandActiveCache = false; // set at each control step: false = no band runs, so the processed path equals the input

    // band detectors (vocal mono, instrumental per channel)
    struct BandDet
    {
        Svf vf;                 // detector filter: two cascaded band-pass sections at the band's centre / Q (4th order: about -40 dB two octaves away,
                                // so a vocal or an instrumental far from a band does not register in it)
        Svf::State vS, iS[maxChannels], vS2, iS2[maxChannels];
        double vEnv = 0.0, iEnv[maxChannels] {};
        float detFreq = -1.0f, detQ = -1.0f;
    };
    std::array<BandDet, numBands> det;
    double aDetAtt = 0, aDetRel = 0;

    // dynamic bells
    struct Bell
    {
        Svf f;                  // g from the (smoothed) frequency; k, a1..a3 per sample from the gain
        Svf::State s[maxChannels];
        double cutDb = 0.0;     // C_b (dB, <= 0)
        double ampNow = 1.0, ampStep = 0.0, ampTarget = 1.0; // A = 10^(C/40), ramped per sample across a control step
        double freqSm = 1000.0, invQ = 1.0 / 1.4;
        bool active = false;
    };
    std::array<Bell, numBands> bell;

    // bypass crossfade (1 = processed, 0 = dry)
    double wet = 1.0, wetStep = 0.0;

    std::array<std::atomic<float>, numBands> shownCut {}, shownV {}, shownI {};
    std::atomic<float> shownGate { 0.0f }, shownVoiced { 0.0f }, shownLevel { -120.0f }, shownFloor { -100.0f };
};
}
