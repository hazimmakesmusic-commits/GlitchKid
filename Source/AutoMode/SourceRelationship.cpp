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
    r.beatLowEndHeavy = beat.bandFraction[0] > 0.5f;

    // Bands 1-3 carry the voice. The voice is "covered" where its level in the band is not clearly above the beat's level in that band:
    // 6 dB or more above = no risk, level with the beat = about 0.5, 3 dB or more below = full risk.
    float worst = 0.35f; // below this a band is not reported as a problem
    for (int b = 1; b <= 3; ++b)
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
}
