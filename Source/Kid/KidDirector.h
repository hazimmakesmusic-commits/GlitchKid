#pragma once
#include "../AutoMode/MixEngine.h"
#include "../Presets/SoundState.h"
#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <vector>

namespace glitchkid::kid
{
// KID, the vocal director: turns what the artist says (typed, tapped or transcribed) into a PROPOSAL of changes to GlitchKid's own parameters.
// It never touches audio or plug-in state: the editor applies the ticked changes, through the normal parameter calls, only when the artist presses Apply.
// Deterministic rule tables (no model): words -> intent -> a musical direction with memory -> goal-directed decisions -> validation -> proposal.
enum Dim { bright = 0, warm, presence, punch, width, depth, clean, exp, numDims };

// What one sentence asked for.
struct IntentFrame
{
    std::array<float, numDims> ops {};  // added to the direction (scaled by "a little" / "very")
    float scale = 0.0f;                  // "too much" 0.5, "a little less" 0.7, "subtle" 0.4 (0 = none)
    bool more = false, improve = false, reset = false;
    juce::StringArray locks;             // "front", "presence", "char", "warm"
    juce::StringArray flags;             // "air", "mud", "ess", "glitch", "natural"
    juce::String role;                   // "", "lead" or "adlib"
    int profile = 0;                     // 1..numProfiles: a producer profile was asked for (by name or by an artist reference), 0 = none
    juce::String artist;                 // the artist reference that was heard ("" = none): a direction only, never a copy of anyone's chain
    int tune = 0;                        // 0 = not mentioned, 1 natural, 2 light, 3 hard (macros::TuneStyle + 1), -1 = Pitch Tune off
    int cleanLevel = 0;                  // Clean My Vocal: 1 gentle, 2 balanced, 3 aggressive (0 = not asked)
    bool buildMix = false;               // "build my mix": a whole mix from the profile and the analysis
    juce::StringArray heard, notes;      // the words used; honest notes (level and tuning are yours, there is no reverb)
    bool touched (Dim d) const { return std::abs (ops[(size_t) d]) > 1e-6f; }
    bool anyOps() const;
    bool understood() const;             // false: nothing in the sentence was a sound word
};

IntentFrame parse (const juce::String& text);

// The four original producer profiles. Each is a FOUNDATION (one of the factory starting profiles, fitted to the voice by the existing Build My Mix engine),
// a Pitch Tune style and a small direction on top. They are production directions written for GlitchKid, not anyone's vocal chain or an endorsement.
struct ProducerProfile
{
    const char* name;          // "Melodic Pop-Rap"
    const char* tagline;       // what it is meant to feel like
    const char* foundation;    // FactoryPresets name
    int tune;                  // 1 natural, 2 light, 3 hard
    std::array<float, numDims> direction;
    const char* flag;          // extra KID flag (nullptr = none)
};
constexpr int numProfiles = 4;
const ProducerProfile& producerProfile (int index); // index 1..numProfiles
juce::String artistDisclaimer (const juce::String& artist, int profile);

// The profile's foundation from the existing Build My Mix engine (automode::buildMix): fitted to `lead` when a vocal was heard, otherwise the factory profile as
// saved (labelled). Pitch Tune, Gain, Mix, CLEAN, AIR and the limiter are carried over from `current`, exactly as the BUILD page does. profile 0 = the clean default.
automode::MixProposal buildFoundation (int profile, const SoundState& current, const SoundState& factoryDefault, const analysis::VocalAnalysis& lead, const automode::RangeLookup&);

// Measurements KID may use; only fields the engine really measures (VocalAnalysis, CleanLab::Applied).
struct AnalysisFacts
{
    bool valid = false;
    float activeSeconds = 0.0f, sibilanceIndex = 0.0f, dynamicRangeDb = 0.0f, mudHz = 0.0f; // mudHz: CLEAN's detected mud frequency, 0 = not measured (CLEAN off)
    std::array<float, 5> bandFraction {};
    int clippedSamples = 0;
    // From a loaded vocal file (VocalFileReport) when there is one; estimates, used only as stated.
    bool fromFile = false;
    bool keyReliable = false;            // only then may KID set Pitch Tune's key and scale
    int keyTonic = 0;
    bool keyMinor = false;
    bool noiseKnown = false;
    float noiseFloorDb = -120.0f;
    float voicedShare = 0.0f, offNoteCents = 0.0f;
};

struct Change
{
    enum class Tag { added, up, down, same, manual };
    juce::String id;
    float from = 0.0f, to = 0.0f;
    juce::String reason;
    Tag tag = Tag::added;
    bool selected = true;
};

class Director
{
public:
    explicit Director (automode::RangeLookup);

    static bool isAllowed (const juce::String& id); // the allowlist: every parameter except Gain, Mix, the Output Limiter and Bypass (Pitch Tune's key and scale only from a reliable estimate)
    static int stageOf (const juce::String& id);     // 1..17 in the real processing order (0 = unknown)
    static bool needsAnalysis (const IntentFrame&);  // "improve it" and cleaning requests read the analyser first

    void begin (const SoundState& current);          // the sound proposals start from (after Apply, Undo or a manual edit)
    juce::String respond (IntentFrame, const AnalysisFacts* = nullptr); // updates the memory, rebuilds the proposal, returns KID's line

    // Build My Mix: the profile's foundation (automode::buildMix, the existing engine) becomes the starting point of the proposal; KID's direction, tuning and
    // cleanup are layered on top. Cleared by Apply, cancel and start over. A foundation that is not usable is ignored.
    void setFoundation (const automode::MixProposal&);
    bool hasFoundation() const { return ! foundation.empty(); }
    int profile() const { return profileIndex; }
    int tuneStyle() const { return tuneChoice; }
    int cleanLevel() const { return cleanChoice; }
    juce::String summary() const;                    // one explainable sentence made from the proposal's real rows ("Added gentle pitch correction, ...")

    const std::vector<Change>& proposal() const { return rows; }
    const juce::StringArray& removed() const { return removedIds; }
    const juce::StringArray& refused() const { return refusedIds; } // ids the rules asked for that the validator dropped
    const std::array<float, numDims>& direction() const { return dir; }
    const std::set<juce::String>& kept() const { return locks; }
    const juce::String& role() const { return roleName; }
    bool natural() const { return flags.count ("natural") > 0; }
    int selectedCount() const;

    void setSelected (const juce::String& id, bool);
    void noteManualEdit (const SoundState& now);     // a proposed parameter moved by hand: its row is unticked, the proposal is rebuilt from the new sound
    SoundState selectedChanges() const;              // only the ticked rows, ready for GlitchKidProcessor::applyParameterChanges
    void accepted (const SoundState& now);           // after Apply: the direction is done, kept constraints and role stay
    void cancel();                                   // drop the proposal; the sound was never changed
    void startOver();                                // forget everything

private:
    automode::RangeLookup ranges;
    SoundState base;
    std::array<float, numDims> dir {};
    std::set<juce::String> locks, flags, unticked, manual;
    juce::String roleName { "lead" };
    std::vector<Dim> lastTouched;
    std::vector<automode::MixChange> foundation;
    juce::String foundationName;
    int profileIndex = 0, tuneChoice = 0, cleanChoice = 0;
    bool keyKnown = false, keyIsMinor = false;
    int keyRoot = 0;
    std::vector<Change> rows;
    std::map<juce::String, float> previous;
    juce::StringArray removedIds, refusedIds;
    void updateDirection (const IntentFrame&);
    void rebuild();
    std::vector<Change> decide();
    juce::String reply (const IntentFrame&, const AnalysisFacts*) const;
};
}
