#pragma once
#include "../AutoMode/AutoMix.h"
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
    juce::StringArray heard, notes;      // the words used; honest notes (level and tuning are yours, there is no reverb)
    bool touched (Dim d) const { return std::abs (ops[(size_t) d]) > 1e-6f; }
    bool anyOps() const;
    bool understood() const;             // false: nothing in the sentence was a sound word
};

IntentFrame parse (const juce::String& text);

// Measurements KID may use; only fields the engine really measures (VocalAnalysis, CleanLab::Applied).
struct AnalysisFacts
{
    bool valid = false;
    float activeSeconds = 0.0f, sibilanceIndex = 0.0f, dynamicRangeDb = 0.0f, mudHz = 0.0f; // mudHz: CLEAN's detected mud frequency, 0 = not measured (CLEAN off)
    std::array<float, 5> bandFraction {};
    int clippedSamples = 0;
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

    static bool isAllowed (const juce::String& id); // the allowlist: every parameter except Pitch Tune, Gain, Mix, the Output Limiter and Bypass
    static int stageOf (const juce::String& id);     // 1..17 in the real processing order (0 = unknown)
    static bool needsAnalysis (const IntentFrame&);  // "improve it" and cleaning requests read the analyser first

    void begin (const SoundState& current);          // the sound proposals start from (after Apply, Undo or a manual edit)
    juce::String respond (IntentFrame, const AnalysisFacts* = nullptr); // updates the memory, rebuilds the proposal, returns KID's line

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
    std::vector<Change> rows;
    std::map<juce::String, float> previous;
    juce::StringArray removedIds, refusedIds;
    void updateDirection (const IntentFrame&);
    void rebuild();
    std::vector<Change> decide();
    juce::String reply (const IntentFrame&, const AnalysisFacts*) const;
};
}
