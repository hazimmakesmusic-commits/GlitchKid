#include "VocalFileAnalysis.h"
#include "ReferenceAnalyzer.h"
#include "../DSP/PitchCorrection/YinPitchDetector.h"
#include <algorithm>
#include <cmath>

namespace glitchkid::analysis
{
namespace
{
constexpr float activeGateDb = -50.0f, silenceDb = -100.0f;

float toDb (double meanSquare) { return meanSquare > 1.0e-12 ? (float) (10.0 * std::log10 (meanSquare)) : -120.0f; }

float percentile (std::vector<float> v, float p) // copies: called a handful of times on a few thousand values
{
    if (v.empty())
        return -120.0f;
    const auto k = (size_t) juce::jlimit (0, (int) v.size() - 1, (int) std::floor (p * (float) (v.size() - 1) + 0.5f));
    std::nth_element (v.begin(), v.begin() + (long) k, v.end());
    return v[k];
}

VocalFileReport fail (VocalFileReport::Status s, const juce::String& m)
{
    VocalFileReport r;
    r.status = s;
    r.message = m;
    return r;
}

// Krumhansl-Kessler key profiles (probe-tone ratings), index 0 = the tonic.
constexpr float majorProfile[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
constexpr float minorProfile[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };

float correlate (const std::array<double, 12>& hist, const float* profile, int tonic)
{
    double mh = 0, mp = 0;
    for (int i = 0; i < 12; ++i) { mh += hist[(size_t) i]; mp += profile[i]; }
    mh /= 12.0; mp /= 12.0;
    double sxy = 0, sxx = 0, syy = 0;
    for (int i = 0; i < 12; ++i)
    {
        const double x = hist[(size_t) ((i + tonic) % 12)] - mh, y = profile[i] - mp;
        sxy += x * y; sxx += x * x; syy += y * y;
    }
    return sxx > 0 && syy > 0 ? (float) (sxy / std::sqrt (sxx * syy)) : 0.0f;
}

// Fills the key fields. Relative major/minor share their notes (C major = A minor), which is all Pitch Tune needs, so the evidence has to beat the best key with a
// DIFFERENT set of notes, not its relative.
void estimateKey (VocalFileReport& r, const std::array<double, 12>& hist)
{
    double total = 0;
    int used = 0;
    for (double h : hist) total += h;
    for (double h : hist) used += total > 0 && h / total >= 0.03 ? 1 : 0;
    if (r.voicedSeconds < 8.0f)
    {
        r.keyNote = "Not enough held notes for a key (" + juce::String (r.voicedSeconds, 1) + " s; 8 s are needed).";
        return;
    }
    if (used < 5)
    {
        r.keyNote = "The take uses too few different notes (" + juce::String (used) + ") to tell a key.";
        return;
    }
    float best = -2.0f, otherSet = -2.0f;
    int bestKey = 0;
    for (int k = 0; k < 24; ++k)
    {
        const float c = correlate (hist, k < 12 ? majorProfile : minorProfile, k % 12);
        if (c > best) { best = c; bestKey = k; }
    }
    auto noteSet = [] (int k) { return k < 12 ? k : (k % 12 + 3) % 12; }; // the major tonic with the same notes
    for (int k = 0; k < 24; ++k)
        if (noteSet (k) != noteSet (bestKey))
            otherSet = std::max (otherSet, correlate (hist, k < 12 ? majorProfile : minorProfile, k % 12));
    r.keyStrength = juce::jmax (0.0f, best);
    r.keyMinor = bestKey >= 12;
    r.keyTonic = bestKey % 12;
    const float margin = best - otherSet;
    r.keyReliable = best >= 0.6f && margin >= 0.08f;
    const auto relative = keyName (r.keyMinor ? (r.keyTonic + 3) % 12 : (r.keyTonic + 9) % 12, ! r.keyMinor);
    r.keyNote = r.keyReliable
        ? "Strongest fit " + keyName (r.keyTonic, r.keyMinor) + " (same notes as " + relative + "), clearly ahead of the next key."
        : "No key is clearly ahead (best fit " + keyName (r.keyTonic, r.keyMinor) + ", " + juce::String (best, 2) + ", lead " + juce::String (margin, 2) + "), so none is reported.";
}
}

juce::String keyName (int tonic, bool minor)
{
    static const char* names[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    return juce::String (names[((tonic % 12) + 12) % 12]) + (minor ? " minor" : " major");
}

VocalFileReport analyseVocalFile (const juce::File& file, const std::atomic<bool>& cancel, const std::function<void (float)>& progress)
{
    auto report = [&] (float v) { if (progress) progress (juce::jlimit (0.0f, 1.0f, v)); };
    if (! file.existsAsFile())
        return fail (VocalFileReport::Status::missingFile, "That file could not be found.");
    auto reader = openAudioFile (file);
    if (reader == nullptr)
        return fail (VocalFileReport::Status::unsupported, "This file could not be opened as audio. Formats that can be opened here: " + referenceFormatNames() + ".");
    if (reader->lengthInSamples <= 0 || reader->numChannels == 0 || reader->sampleRate < 8000.0)
        return fail (VocalFileReport::Status::unreadable, "The file is empty or damaged.");

    VocalFileReport r;
    r.fileName = file.getFileName();
    r.sampleRate = reader->sampleRate;
    r.channels = (int) reader->numChannels;
    r.fileSeconds = (double) reader->lengthInSamples / r.sampleRate;
    const auto total = std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (VocalFileReport::maxSeconds * r.sampleRate));
    r.analysedSeconds = (double) total / r.sampleRate;
    const int channels = std::min (2, r.channels);
    const int frameLen = juce::jmax (1, juce::roundToInt (r.sampleRate * 0.05));

    pitch::YinPitchDetector yin;
    yin.prepare (r.sampleRate, 70.0f, 900.0f);
    std::vector<float> frameDb, heldMidi;
    frameDb.reserve ((size_t) (total / frameLen + 1));
    std::vector<float> frameMidi; // clear pitch estimates of the current frame, kept only if the frame is active
    std::array<double, 12> pcHist {};
    r.overview.assign ((size_t) VocalFileReport::overviewColumns, 0.0f);
    double frameSq = 0, lr = 0, ll = 0, rr = 0;
    int framePos = 0;
    long estimates = 0, voicedEstimates = 0;
    float peak = 0.0f, prevMidi = -1.0f;
    bool identical = channels > 1;

    juce::AudioBuffer<float> buf (channels, 1 << 16);
    for (juce::int64 pos = 0; pos < total; pos += buf.getNumSamples())
    {
        if (cancel.load())
            return fail (VocalFileReport::Status::cancelled, "Cancelled.");
        const int n = (int) std::min<juce::int64> (buf.getNumSamples(), total - pos);
        if (! reader->read (&buf, 0, n, pos, true, channels > 1))
            return fail (VocalFileReport::Status::unreadable, "The file could not be read to the end (it may be damaged).");
        const float* L = buf.getReadPointer (0);
        const float* R = buf.getReadPointer (channels > 1 ? 1 : 0);
        for (int i = 0; i < n; ++i)
        {
            const float a = std::isfinite (L[i]) ? L[i] : 0.0f, b = std::isfinite (R[i]) ? R[i] : 0.0f;
            const float m = channels > 1 ? 0.5f * (a + b) : a;
            const float mag = std::max (std::abs (a), std::abs (b));
            peak = std::max (peak, mag);
            r.clippedSamples += mag >= 0.999f ? 1 : 0;
            r.nearClippedSamples += mag >= 0.98f ? 1 : 0;
            if (channels > 1)
            {
                lr += (double) a * b; ll += (double) a * a; rr += (double) b * b;
                identical = identical && std::abs (a - b) < 1.0e-6f;
            }
            auto& col = r.overview[(size_t) ((pos + i) * VocalFileReport::overviewColumns / total)];
            col = std::max (col, std::min (1.0f, mag));
            frameSq += (double) m * m;
            if (yin.push (m))
            {
                const auto& e = yin.result();
                ++estimates;
                if (e.voiced && e.confidence >= 0.85f && e.frequencyHz > 0.0f)
                    frameMidi.push_back (69.0f + 12.0f * std::log2 (e.frequencyHz / 440.0f));
                else
                    frameMidi.push_back (-1.0f);
            }
            if (++framePos == frameLen)
            {
                const float db = toDb (frameSq / frameLen);
                frameDb.push_back (db);
                if (db > activeGateDb)
                    for (float midi : frameMidi)
                    {
                        if (midi < 0.0f) { prevMidi = -1.0f; continue; }
                        ++voicedEstimates;
                        heldMidi.push_back (midi);
                        if (prevMidi >= 0.0f && std::abs (midi - prevMidi) < 0.3f) // a held note, not a glide between notes
                            pcHist[(size_t) (((int) std::lround (midi) % 12 + 12) % 12)] += 1.0;
                        prevMidi = midi;
                    }
                else
                    prevMidi = -1.0f;
                frameMidi.clear();
                frameSq = 0;
                framePos = 0;
            }
        }
        report (0.85f * (float) (pos + n) / (float) total);
    }

    r.peakDb = juce::Decibels::gainToDecibels (peak, -120.0f);
    std::vector<float> active, quiet;
    int silentFrames = 0;
    for (float db : frameDb)
    {
        if (db > activeGateDb) active.push_back (db);
        if (db <= silenceDb) ++silentFrames;
    }
    r.activeSeconds = (float) active.size() * 0.05f;
    if (peak < 1.0e-4f || r.activeSeconds < 0.5f)
        return fail (VocalFileReport::Status::silent, "No vocal could be heard in this file (it is silent or very quiet).");
    r.loudP90Db = percentile (active, 0.9f);
    r.quietP10Db = percentile (active, 0.1f);
    if (channels > 1)
    {
        r.correlation = ll > 0 && rr > 0 ? (float) (lr / std::sqrt (ll * rr)) : 1.0f;
        r.stereoIdentical = identical;
    }

    // Noise floor: the quiet moments clearly below the singing (25 dB under the loud frames). Without enough of them there is nothing to measure it from.
    for (float db : frameDb)
        if (db > silenceDb && db < r.loudP90Db - 25.0f)
            quiet.push_back (db);
    const auto minQuiet = std::max<size_t> (10, frameDb.size() / 50); // at least 0.5 s and 2 % of the take
    if (quiet.size() >= minQuiet)
    {
        r.noiseFloorKnown = true;
        r.noiseFloorDb = percentile (quiet, 0.25f);
    }
    else if ((size_t) silentFrames >= minQuiet)
        r.digitalSilenceGaps = true;

    // Pitch (only the active, clearly pitched part).
    r.voicedShare = estimates > 0 && ! active.empty() ? juce::jlimit (0.0f, 1.0f, (float) voicedEstimates / ((float) estimates * (float) active.size() / (float) frameDb.size())) : 0.0f;
    r.voicedSeconds = (float) voicedEstimates * (float) r.analysedSeconds / (float) juce::jmax (1L, estimates);
    if (heldMidi.size() >= 200)
    {
        const float mid = percentile (heldMidi, 0.5f);
        r.medianHz = 440.0f * std::pow (2.0f, (mid - 69.0f) / 12.0f);
        r.lowHz = 440.0f * std::pow (2.0f, (percentile (heldMidi, 0.05f) - 69.0f) / 12.0f);
        r.highHz = 440.0f * std::pow (2.0f, (percentile (heldMidi, 0.95f) - 69.0f) / 12.0f);
        std::vector<float> cents;
        cents.reserve (heldMidi.size());
        for (float midi : heldMidi)
            cents.push_back (100.0f * std::abs (midi - std::round (midi)));
        r.offNoteCents = percentile (cents, 0.5f);
    }
    estimateKey (r, pcHist);

    // Tone, dynamics and sibilance: the busiest 30 s, measured exactly as live listening is (VocalAnalyzer).
    const int window = juce::jmin ((int) frameDb.size(), 600);
    int bestStart = 0, count = 0, bestCount = -1;
    for (int f = 0; f < (int) frameDb.size(); ++f)
    {
        count += frameDb[(size_t) f] > activeGateDb ? 1 : 0;
        if (f >= window)
            count -= frameDb[(size_t) (f - window)] > activeGateDb ? 1 : 0;
        if (f >= window - 1 && count > bestCount) { bestCount = count; bestStart = f - window + 1; }
    }
    const int toneLen = window * frameLen;
    std::vector<float> mono ((size_t) toneLen);
    juce::AudioBuffer<float> tb (channels, toneLen);
    if (cancel.load())
        return fail (VocalFileReport::Status::cancelled, "Cancelled.");
    if (! reader->read (&tb, 0, toneLen, (juce::int64) bestStart * frameLen, true, channels > 1))
        return fail (VocalFileReport::Status::unreadable, "The file could not be read to the end (it may be damaged).");
    for (int i = 0; i < toneLen; ++i)
        mono[(size_t) i] = channels > 1 ? 0.5f * (tb.getSample (0, i) + tb.getSample (1, i)) : tb.getSample (0, i);
    r.tone = VocalAnalyzer::analyzeOffline (mono.data(), toneLen, r.sampleRate);
    report (1.0f);

    r.status = VocalFileReport::Status::ok;
    r.message = "Analysed " + r.fileName + " (" + juce::String (r.analysedSeconds, 1) + " s"
              + (r.fileSeconds > r.analysedSeconds + 0.05 ? juce::String (" of ") + juce::String (r.fileSeconds, 1) + " s" : juce::String()) + ").";
    return r;
}

juce::StringArray describeVocalFile (const VocalFileReport& r)
{
    juce::StringArray out;
    if (! r.ok())
    {
        out.add (r.message);
        return out;
    }
    auto db = [] (float v) { return juce::String (v, 1) + " dBFS"; };
    out.add ("MEASURED  " + r.fileName + ": " + juce::String (r.analysedSeconds, 1) + " s analysed"
             + (r.fileSeconds > r.analysedSeconds + 0.05 ? " (the first 3 minutes of " + juce::String (r.fileSeconds, 1) + " s)" : juce::String())
             + ", " + juce::String (r.sampleRate / 1000.0, 1) + " kHz, " + (r.channels > 1 ? "stereo" : "mono"));
    out.add ("MEASURED  Peak " + db (r.peakDb) + (r.clippedSamples > 0 ? ", " + juce::String (r.clippedSamples) + " clipped samples (GlitchKid cannot repair clipping)" : ", no clipping"));
    out.add ("MEASURED  Loud parts " + db (r.loudP90Db) + ", quiet parts " + db (r.quietP10Db) + " (50 ms RMS), voice present " + juce::String (r.activeSeconds, 1) + " s");
    if (r.channels > 1)
        out.add ("MEASURED  " + (r.stereoIdentical ? juce::String ("Both channels are identical: mono content in a stereo file")
                                                   : "Left/right correlation " + juce::String (r.correlation, 2) + (r.correlation > 0.95f ? " (nearly mono)" : "")));
    if (r.tone.status == VocalAnalysis::Status::ready)
    {
        const auto& f = r.tone.bandFraction;
        out.add ("MEASURED  Tone (the busiest stretch, up to 30 s): low-mids " + juce::String (juce::roundToInt (f[1] * 100.0f)) + " %, presence " + juce::String (juce::roundToInt (f[3] * 100.0f))
                 + " %, air " + juce::String (juce::roundToInt (f[4] * 100.0f)) + " % of the energy; level movement " + juce::String (r.tone.dynamicRangeDb, 1) + " dB");
        out.add ("ESTIMATE  Sibilance index " + juce::String (r.tone.sibilanceIndex, 2) + (r.tone.sibilanceIndex > 0.25f ? " (bright S sounds are frequent)" : ""));
    }
    if (r.noiseFloorKnown)
        out.add ("ESTIMATE  Quietest moments at about " + db (r.noiseFloorDb) + " (room, hiss, breaths or bleed: it cannot tell which). GlitchKid has no noise reduction.");
    else if (r.digitalSilenceGaps)
        out.add ("ESTIMATE  The gaps between phrases are digital silence (edited or gated): no background noise to measure.");
    else
        out.add ("ESTIMATE  Not enough quiet moments to estimate the background noise.");
    out.add ("ESTIMATE  Clear pitch in " + juce::String (juce::roundToInt (r.voicedShare * 100.0f)) + " % of the time the voice is present (sung notes score high, rap and speech lower)");
    if (r.medianHz > 0.0f)
        out.add ("ESTIMATE  Pitch around " + juce::String (juce::roundToInt (r.medianHz)) + " Hz (range " + juce::String (juce::roundToInt (r.lowHz)) + "-" + juce::String (juce::roundToInt (r.highHz))
                 + " Hz), typically " + juce::String (juce::roundToInt (r.offNoteCents)) + " cents from the nearest note");
    out.add ("ESTIMATE  Key: " + (r.keyReliable ? keyName (r.keyTonic, r.keyMinor) + ". " : juce::String ("not reported. ")) + r.keyNote);
    return out;
}

bool VocalFileJob::start (const juce::File& f, std::function<void (const VocalFileReport&)> onDone)
{
    if (busy.load())
        return false;
    stopThread (15000);
    file = f;
    shared->done = std::move (onDone);
    shared->alive = true;
    cancelFlag.store (false);
    progressValue.store (0.0f);
    busy.store (true);
    startThread (juce::Thread::Priority::low);
    return true;
}

void VocalFileJob::stopJob()
{
    shared->alive = false; // a result already posted will find this and do nothing
    cancelFlag.store (true);
    stopThread (15000);
    busy.store (false);
}

void VocalFileJob::run()
{
    auto result = analyseVocalFile (file, cancelFlag, [this] (float v) { progressValue.store (v); });
    busy.store (false);
    if (threadShouldExit())
        return;
    juce::MessageManager::callAsync ([sh = shared, r = std::move (result)]
    {
        if (sh->alive && sh->done)
            sh->done (r);
    });
}
}
