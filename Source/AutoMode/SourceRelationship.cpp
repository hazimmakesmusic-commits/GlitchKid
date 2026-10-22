#include "SourceRelationship.h"
#include <algorithm>
#include <cmath>

namespace glitchkid::automode
{
namespace
{
float bandDb (const analysis::VocalAnalysis& a, int band)
{
    return a.rmsDb + 10.0f * std::log10 (std::max (a.bandFraction[(size_t) band], 1.0e-6f));
}
std::string fmt1 (float v)
{
    char b[32];
    std::snprintf (b, sizeof (b), "%.1f", (double) v);
    return b;
}
}

const char* bandName (int band)
{
    static const char* names[analysis::VocalAnalysis::numBands] = { "low end (under 120 Hz)", "low-mids (120-500 Hz)", "mids (500 Hz-2 kHz)", "presence (2-5 kHz)", "air (above 5 kHz)" };
    return names[std::clamp (band, 0, analysis::VocalAnalysis::numBands - 1)];
}

Relationship compare (const analysis::VocalAnalysis& lead, const analysis::VocalAnalysis& beat)
{
    using S = analysis::VocalAnalysis::Status;
    Relationship r;
    if (lead.status != S::ready)
    {
        r.unavailable = "The lead vocal has not been analysed yet.";
        return r;
    }
    if (beat.status != S::ready)
    {
        r.unavailable = "The instrumental has not been analysed: it is not connected, or it was silent while listening.";
        return r;
    }
    r.valid = true;
    r.confidence = std::clamp (std::min (lead.activeSeconds, beat.activeSeconds) / analysis::VocalAnalyzer::maxActiveSeconds, 0.0f, 1.0f);
    r.vocalMinusBeatDb = lead.rmsDb - beat.rmsDb;
    r.beatVeryLoud = beat.peakDb > -1.0f;
    r.leadSibilance = lead.sibilanceIndex;
    r.leadPresenceShare = lead.bandFraction[3];
    r.leadAirShare = lead.bandFraction[4];
    r.beatLowEndHeavy = beat.bandFraction[0] > 0.5f;

    // Bands 1-3 carry the voice. The voice is "covered" where its level in the band is not clearly above the beat's level in that band:
    // 6 dB or more above = no risk, level with the beat = about 0.5, 3 dB or more below = full risk.
    float worst = 0.35f; // below this a band is not reported as a problem
    for (int b = 3; b >= 1; --b) // (on a tie the higher band wins: the words matter more than the low-mids)
    {
        const float diff = bandDb (lead, b) - bandDb (beat, b);
        r.maskRisk[(size_t) b] = std::clamp ((6.0f - diff) / 9.0f, 0.0f, 1.0f);
        if (r.maskRisk[(size_t) b] > worst)
        {
            worst = r.maskRisk[(size_t) b];
            r.worstBand = b;
        }
    }

    if (r.worstBand >= 0)
        r.reasons.push_back (std::string ("The beat has a lot of energy in the ") + bandName (r.worstBand) + ", where the voice lives: the voice is likely to be covered there.");
    if (r.vocalMinusBeatDb < -3.0f)
        r.reasons.push_back ("The beat is " + fmt1 (-r.vocalMinusBeatDb) + " dB louder than the lead on this track (digital levels, not LUFS).");
    else if (r.vocalMinusBeatDb > 6.0f)
        r.reasons.push_back ("The lead is " + fmt1 (r.vocalMinusBeatDb) + " dB louder than the beat: check the balance in your mixer.");
    if (r.beatLowEndHeavy)
        r.reasons.push_back ("More than half of the beat's energy is under 120 Hz, so keep the lead's low end light.");
    if (r.beatVeryLoud)
        r.reasons.push_back ("The beat peaks very close to full scale: there is little headroom to push the vocal up.");
    if (r.reasons.empty())
        r.reasons.push_back ("No obvious conflict between the voice and the beat in these five bands.");
    if (r.confidence < 0.4f)
        r.reasons.push_back ("Only a short stretch of audio was heard, so treat this as a rough guide.");
    return r;
}

float applyBeatMove (float base, const BeatMove& m)
{
    if (m.scale)
        return base > 0.0f ? base * m.factor : base;
    float v = base + m.delta;
    if (m.hasLimit)
        v = m.delta >= 0.0f ? std::min (v, std::max (base, m.limit)) : std::max (v, std::min (base, m.limit));
    return v;
}

std::vector<BeatMove> beatMoves (const Relationship& rel)
{
    std::vector<BeatMove> moves;
    if (! rel.valid)
        return moves;
    float clutter = 1.0f;
    if (rel.worstBand >= 2)
    {
        const float risk = rel.maskRisk[(size_t) rel.worstBand];
        // Never brighten a vocal that is already harsh: the same thresholds BUILD uses for "S sounds look strong" and "already bright".
        if (rel.leadSibilance > 0.35f || rel.leadAirShare > 0.12f)
        {
            moves.push_back ({ "", "PRESENCE", std::string ("The beat covers the voice at ") + bandName (rel.worstBand) + ", but presence was NOT raised: your vocal already has "
                               + (rel.leadSibilance > 0.35f ? "strong S sounds" : "a lot of energy above 5 kHz") + ", so more presence would add harshness. Try the beat's level or a cut in the beat instead.",
                               0.0f, false, 1.0f, false, 0.0f });
        }
        else
        {
            moves.push_back ({ "eq_b2_gain", "PRESENCE", std::string ("The beat competes with the voice at ") + bandName (rel.worstBand) + ": presence raised so the words stay clear (at most +4 dB in total).",
                               std::clamp (risk * 2.0f, 0.0f, 2.5f), false, 1.0f, true, 4.0f });
            clutter *= 0.85f;
        }
    }
    if (rel.maskRisk[1] > 0.35f)
    {
        moves.push_back ({ "eq_b1_gain", "CLARITY", "The beat is busy in the low-mids: the 400 Hz bell was cut a little (never below -6 dB in total).", -1.0f, false, 1.0f, true, -6.0f });
        moves.push_back ({ "eq_lo_gain", "CLARITY", "The beat is busy in the low-mids: low shelf lowered (never below -6 dB in total).", -1.0f, false, 1.0f, true, -6.0f });
    }
    if (rel.beatLowEndHeavy)
        moves.push_back ({ "eq_lo_gain", "CLARITY", "The beat is heavy under 120 Hz: the lead's low end was kept light (never below -6 dB in total).", -1.5f, false, 1.0f, true, -6.0f });
    if (rel.beatVeryLoud)
        clutter *= 0.9f;
    if (clutter < 0.999f)
        for (const char* id : { "echo_mix", "dim_mix" })
            moves.push_back ({ id, "SPACE", "The beat is crowded: this ambience was reduced so the vocal does not get lost.", 0.0f, true, clutter, false, 0.0f });
    return moves;
}
}
