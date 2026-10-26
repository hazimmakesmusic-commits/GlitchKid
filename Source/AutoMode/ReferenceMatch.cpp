#include "ReferenceMatch.h"
#include <algorithm>
#include <cmath>

namespace glitchkid::automode
{
namespace
{
using analysis::SpectralProfile;
constexpr float maxStepDb = 3.0f, maxTotalDb = 6.0f, presenceMaxDb = 4.0f, deadZoneDb = 0.5f, fullMixShare = 0.5f;

struct BandSpec { const char* gainId; const char* freqId; float defaultFreq; bool shelf; const char* name; const char* region; };
const BandSpec specs[3] = {
    { "eq_b1_gain", "eq_b1_freq", 400.0f, false, "Body bell", "in the body (low-mids)" },
    { "eq_b2_gain", "eq_b2_freq", 3000.0f, false, "Presence bell", "in the presence range" },
    { "eq_hi_gain", "eq_hi_freq", 8000.0f, true, "High shelf", "in the air range" } };

float clampSnap (const RangeLookup& ranges, const juce::String& id, float v, bool& known)
{
    Range r;
    known = ranges && ranges (id, r);
    if (! known)
        return v;
    v = juce::jlimit (r.lo, r.hi, v);
    return r.snap ? r.snap (v) : v;
}
}

MatchProposal proposeReferenceInspiredEq (const MatchInput& in, const RangeLookup& ranges)
{
    MatchProposal p;
    p.sound = in.current;
    p.limitations.add ("A finished song is a full mix: its tone includes the instruments and the mastering, not only the vocal. This is a hint toward its overall tone, not a copy of its vocal.");
    p.limitations.add ("Only the broad tone from 200 Hz to 12 kHz is compared, and only the body, presence and high-shelf EQ gains can change. Below 200 Hz, dynamics, stereo width, ambience and loudness are not compared.");
    if (! in.reference.valid)
    {
        p.status = MatchProposal::Status::referenceMissing;
        p.message = "Analyse a reference file first.";
        return p;
    }
    if (! in.vocal.valid)
    {
        p.status = MatchProposal::Status::vocalMissing;
        p.message = "Analyse your vocal first (play it through the plug-in).";
        return p;
    }
    const int n = std::min (in.reference.usedBands, in.vocal.usedBands);
    if (in.reference.basis != in.vocal.basis || n < 12)
    {
        p.status = MatchProposal::Status::incompatible;
        p.message = "These two analyses cannot be compared (they were not measured in the same way, or too few bands are available).";
        return p;
    }
    p.bandsCompared = n;

    // the vocal as it sounds now = its measured shape plus the current EQ's response; both shapes are re-centred over the compared bands
    std::array<float, SpectralProfile::numBands> processed {}, target {};
    double pm = 0.0, tm = 0.0;
    for (int b = 0; b < n; ++b)
    {
        const float hz = SpectralProfile::bandCentreHz (b);
        processed[(size_t) b] = in.vocal.shapeDb[(size_t) b] + (in.currentEqDb ? in.currentEqDb (hz) : 0.0f);
        target[(size_t) b] = in.reference.shapeDb[(size_t) b];
        pm += (double) processed[(size_t) b];
        tm += (double) target[(size_t) b];
    }
    pm /= n;
    tm /= n;
    std::array<float, SpectralProfile::numBands> diff {}, smooth {};
    for (int b = 0; b < n; ++b)
        diff[(size_t) b] = (target[(size_t) b] - (float) tm) - (processed[(size_t) b] - (float) pm);
    for (int b = 0; b < n; ++b) // a light smoothing: narrow bumps belong to instruments and rooms, not to a tone you would EQ for
        smooth[(size_t) b] = 0.25f * diff[(size_t) std::max (0, b - 1)] + 0.5f * diff[(size_t) b] + 0.25f * diff[(size_t) std::min (n - 1, b + 1)];
    p.differenceDb = diff;

    // confidence: never high for a full mix; lowered by anything that makes the comparison less trustworthy
    p.confidence = in.reference.fullMix ? Confidence::medium : Confidence::high;
    auto lower = [&p] (const juce::String& why) { p.confidence = Confidence::low; p.confidenceReasons.add (why); };
    if (in.reference.fullMix)
        p.confidenceReasons.add ("The reference is a full mix, so the most this can reach is medium confidence.");
    if (in.reference.activeSeconds < 8.0f)
        lower ("The reference section is short (" + juce::String (in.reference.activeSeconds, 1) + " s of sound).");
    if (in.reference.variabilityDb > 6.0f)
        lower ("The reference's tone changes a lot from moment to moment (" + juce::String (in.reference.variabilityDb, 1) + " dB), so its average is a weaker guide.");
    if (in.vocal.activeSeconds < 6.0f)
        lower ("Only " + juce::String (in.vocal.activeSeconds, 1) + " s of your vocal was measured.");
    if (in.vocal.variabilityDb > 7.0f)
        lower ("Your vocal's tone changes a lot (" + juce::String (in.vocal.variabilityDb, 1) + " dB), so its average is a weaker guide.");

    p.evidence.add ("Reference: " + juce::String (in.reference.activeSeconds, 1) + " s of sound analysed" + (in.reference.sourceChannels > 1 ? " (stereo file, reduced to a mono sum like your vocal)" : "") + ".");
    p.evidence.add ("Your vocal: " + juce::String (in.vocal.activeSeconds, 1) + " s measured, as it sounds with your current EQ.");
    p.evidence.add (juce::String (n) + " third-octave bands compared, " + juce::String ((int) SpectralProfile::bandCentreHz (0)) + " Hz to " + juce::String ((int) SpectralProfile::bandCentreHz (n - 1)) + " Hz.");

    const float intensity = juce::jlimit (0.0f, 1.0f, in.intensity);
    const float share = (in.reference.fullMix ? fullMixShare : 1.0f) * (p.confidence == Confidence::low ? 0.6f : 1.0f) * intensity;
    juce::String found;
    for (const auto& spec : specs)
    {
        const float f0 = in.current.get (spec.freqId, spec.defaultFreq);
        double sum = 0.0, wsum = 0.0;
        float lowHz = 1.0e9f, highHz = 0.0f;
        for (int b = 0; b < n; ++b)
        {
            const float hz = SpectralProfile::bandCentreHz (b);
            const float w = spec.shelf ? (hz >= f0 / 1.2f ? 1.0f : 0.0f) : std::max (0.0f, 1.0f - std::abs (std::log2 (hz / f0)) / 0.9f);
            if (w <= 0.0f)
                continue;
            sum += (double) w * smooth[(size_t) b];
            wsum += (double) w;
            lowHz = std::min (lowHz, hz);
            highHz = std::max (highHz, hz);
        }
        if (wsum <= 0.0)
            continue;
        const float avg = (float) (sum / wsum);
        found += juce::String (spec.name) + " " + juce::String (avg, 1) + " dB; ";
        const float step = juce::jlimit (-maxStepDb, maxStepDb, avg * share);
        const float cur = in.current.get (spec.gainId, 0.0f);
        const float limit = std::string (spec.gainId) == "eq_b2_gain" ? presenceMaxDb : maxTotalDb;
        if (std::abs (step) < deadZoneDb)
            continue;
        if ((step > 0.0f && cur >= limit) || (step < 0.0f && cur <= -maxTotalDb))
            continue; // already at or beyond the limit in that direction: left alone
        bool known = false;
        const float target0 = juce::jlimit (-maxTotalDb, limit, cur + step);
        const float to = clampSnap (ranges, spec.gainId, target0, known);
        if (! known || std::abs (to - cur) < 0.05f)
            continue;
        MixChange c;
        c.id = spec.gainId;
        c.from = cur;
        c.to = to;
        c.reason = "The reference is about " + juce::String (std::abs (avg), 1) + " dB " + (avg > 0.0f ? "brighter or fuller" : "thinner or darker") + " " + spec.region + " (" + juce::String ((int) lowHz) + " to " + juce::String ((int) highHz)
                   + " Hz) than your vocal sounds now. Only part of that is requested (" + juce::String (juce::roundToInt (intensity * 100.0f)) + " % intensity, a full mix is not a vocal): "
                   + juce::String (to - cur, 1) + " dB, so the " + spec.name + " goes from " + juce::String (cur, 1) + " to " + juce::String (to, 1) + " dB.";
        p.changes.push_back (c);
        p.sound.set (c.id, to);
    }
    p.evidence.add ("Reference minus your vocal as it sounds now: " + found.trimEnd().trimCharactersAtEnd (";") + ".");
    if (p.changes.empty())
    {
        p.status = MatchProposal::Status::nothingToChange;
        p.sound = in.current;
        p.message = "Your vocal's tone is already close to the reference in the compared range, so no EQ change is proposed.";
        return p;
    }
    p.status = MatchProposal::Status::ok;
    p.message = juce::String ((int) p.changes.size()) + " Reference-Inspired EQ change" + (p.changes.size() == 1 ? "" : "s") + " proposed. Review them, then apply.";
    return p;
}

SoundState applyMatchChanges (const MatchProposal& proposal, const std::vector<bool>& ticked, const SoundState& current, const RangeLookup& ranges, std::vector<MixChange>* applied)
{
    SoundState out = current;
    if (applied != nullptr)
        applied->clear();
    for (size_t i = 0; i < proposal.changes.size() && i < ticked.size(); ++i)
    {
        if (! ticked[i])
            continue;
        const auto& c = proposal.changes[i];
        bool known = false;
        const float v = clampSnap (ranges, c.id, c.to, known);
        if (! known || std::abs (v - current.get (c.id, c.from)) < 1.0e-4f)
            continue;
        out.set (c.id, v);
        if (applied != nullptr)
        {
            auto change = c;
            change.from = current.get (c.id, c.from);
            change.to = v;
            applied->push_back (change);
        }
    }
    return out;
}
}
