#include "TestHelpers.h"
#include "../Source/Analysis/ReferenceAnalyzer.h"
#include "../Source/AutoMode/ReferenceMatch.h"
#include "../Source/DSP/ParametricEq.h"
#include "../Source/UI/PlaygroundWidgets.h"
#include "../Source/PluginEditor.h"

// REFERENCE MATCH ("Reference-Inspired EQ"): the analysis (off the audio thread, cancellable, honest errors, mono-sum basis) and the pure matching engine
// (bounded, explained, reviewable, never high confidence for a full mix). Synthetic signals only: nothing here says how it sounds on a real song.
namespace
{
namespace an = glitchkid::analysis;
namespace am = glitchkid::automode;

std::vector<float> noise (int n, int seed, float amp = 0.1f)
{
    juce::Random rng (seed);
    std::vector<float> v ((size_t) n);
    for (auto& s : v)
        s = (rng.nextFloat() * 2.0f - 1.0f) * amp;
    return v;
}

// noise through the real ParametricEq: band 3 is the high shelf, band 1 and 2 the bells
std::vector<float> shaped (std::vector<float> in, double sr, int band, float hz, float gainDb, float q = 0.8f)
{
    ParametricEq eq;
    eq.prepare (sr);
    eq.setBand (band, { hz, gainDb, q });
    juce::AudioBuffer<float> b (1, (int) in.size());
    b.copyFrom (0, 0, in.data(), (int) in.size());
    eq.reset();
    eq.process (b); // the first ~20 ms is the gain smoothing; the analysis averages over seconds
    return std::vector<float> (b.getReadPointer (0), b.getReadPointer (0) + in.size());
}

an::SpectralProfile profileOf (const std::vector<float>& x, double sr)
{
    an::SpectralAccumulator acc (sr);
    acc.pushSamples (x.data(), (int) x.size());
    return acc.finish (1);
}

juce::File writeWav (const juce::File& dir, const juce::String& name, double sr, int channels, const std::vector<std::vector<float>>& data)
{
    auto f = dir.getChildFile (name);
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream());
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, (unsigned) channels, 16, {}, 0));
    if (w != nullptr)
    {
        os.release();
        juce::AudioBuffer<float> b (channels, (int) data[0].size());
        for (int c = 0; c < channels; ++c)
            b.copyFrom (c, 0, data[(size_t) c].data(), (int) data[(size_t) c].size());
        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }
    return f;
}

am::RangeLookup rangesOf (GlitchKidProcessor& p)
{
    return p.makeRangeLookup();
}

am::MatchInput inputOf (const an::SpectralProfile& ref, const an::SpectralProfile& voc, float intensity = 0.6f)
{
    am::MatchInput in;
    in.reference = ref;
    in.vocal = voc;
    in.intensity = intensity;
    in.current.set ("eq_b1_gain", 0.0f); in.current.set ("eq_b1_freq", 400.0f);
    in.current.set ("eq_b2_gain", 0.0f); in.current.set ("eq_b2_freq", 3000.0f);
    in.current.set ("eq_hi_gain", 0.0f); in.current.set ("eq_hi_freq", 8000.0f);
    return in;
}

struct ReferenceMatchTests : public juce::UnitTest
{
    ReferenceMatchTests() : juce::UnitTest ("Reference Match", "GlitchKid") {}

    void runTest() override
    {
        const double sr = 48000.0;
        const auto plain = noise (48000 * 20, 3);

        beginTest ("analysis: a white-noise shape rises 3 dB per octave (band energies), is independent of level, and finds a +6 dB high shelf");
        {
            const auto p = profileOf (plain, sr);
            expect (p.valid && p.usedBands == 19 && p.activeSeconds > 15.0f);
            expectWithinAbsoluteError (p.shapeDb[18] - p.shapeDb[0], 18.06f, 1.2f); // 6 octaves at 3.01 dB
            std::vector<float> quiet = plain;
            for (auto& s : quiet) s *= 0.1f;
            const auto q = profileOf (quiet, sr);
            for (int b = 0; b < 19; ++b)
                expectWithinAbsoluteError (q.shapeDb[(size_t) b], p.shapeDb[(size_t) b], 0.3f);
            const auto bright = profileOf (shaped (plain, sr, 3, 3000.0f, 6.0f), sr);
            expectWithinAbsoluteError (bright.shapeDb[17] - p.shapeDb[17] - (bright.shapeDb[2] - p.shapeDb[2]), 6.0f, 1.5f);
            expect (p.variabilityDb < 3.0f, "steady noise is a steady tone: " + juce::String (p.variabilityDb, 2));
        }

        beginTest ("analysis: silence, a short clip and a clip with a gap are reported honestly");
        {
            expect (! profileOf (std::vector<float> (48000 * 6, 0.0f), sr).valid);
            const auto shortP = profileOf (noise (48000, 5), sr);
            expect (shortP.valid && shortP.activeSeconds < 2.0f);
            auto gap = noise (48000 * 10, 6);
            std::fill (gap.begin() + 48000 * 3, gap.begin() + 48000 * 8, 0.0f);
            const auto g = profileOf (gap, sr);
            expect (g.valid && g.activeSeconds > 4.0f && g.activeSeconds < 6.5f, "only the sounding part counts: " + juce::String (g.activeSeconds, 1));
        }

        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("glitchkid_reference_tests");
        dir.createDirectory();
        std::atomic<bool> noCancel { false };

        beginTest ("files: WAV at 44.1, 48 and 96 kHz, mono and stereo, give the same tonal shape; the section is the loudest 20 s");
        {
            const auto base = profileOf (plain, sr);
            for (double rate : { 44100.0, 48000.0, 96000.0 })
            {
                const auto x = noise ((int) (rate * 24), 3);
                auto f = writeWav (dir, "noise_" + juce::String ((int) rate) + ".wav", rate, 1, { x });
                float last = 0.0f;
                bool monotonic = true;
                const auto r = an::analyseReferenceFile (f, {}, noCancel, [&] (float v) { monotonic = monotonic && v >= last - 1.0e-4f; last = v; });
                expect (r.status == an::ReferenceResult::Status::ok, r.message);
                expect (monotonic && last > 0.99f, "progress goes forward to the end");
                expectWithinAbsoluteError ((float) r.sectionLengthSeconds, 20.0f, 0.1f);
                for (int b = 0; b < 19; ++b)
                    expectWithinAbsoluteError (r.profile.shapeDb[(size_t) b], base.shapeDb[(size_t) b], 1.0f);
                expect (r.profile.fullMix, "a reference file is treated as a full mix");
            }
            auto st = writeWav (dir, "stereo.wav", sr, 2, { noise (48000 * 22, 3), noise (48000 * 22, 3) });
            const auto rs = an::analyseReferenceFile (st, {}, noCancel);
            expect (rs.status == an::ReferenceResult::Status::ok && rs.profile.sourceChannels == 2, rs.message);
            // a quiet intro, then the loud part: the automatic section is the loud part
            auto intro = noise (48000 * 30, 8, 0.01f);
            auto loud = noise (48000 * 20, 9, 0.2f);
            intro.insert (intro.end(), loud.begin(), loud.end());
            auto fi = writeWav (dir, "intro.wav", sr, 1, { intro });
            const auto ri = an::analyseReferenceFile (fi, {}, noCancel);
            expect (ri.status == an::ReferenceResult::Status::ok && ri.sectionStartSeconds > 25.0, "the loud part is chosen: starts at " + juce::String (ri.sectionStartSeconds, 1) + " s");
            an::ReferenceSection manual;
            manual.automatic = false; manual.startSeconds = 2.0; manual.lengthSeconds = 10.0;
            const auto rm = an::analyseReferenceFile (fi, manual, noCancel);
            expect (rm.status == an::ReferenceResult::Status::ok && std::abs (rm.sectionStartSeconds - 2.0) < 0.1, "a chosen section is honoured");
        }

        beginTest ("files: a stereo file is compared as a mono sum, never channel by channel (an out-of-phase pair sums to silence)");
        {
            auto left = noise (48000 * 10, 4);
            auto right = left;
            for (auto& s : right) s = -s;
            auto f = writeWav (dir, "outofphase.wav", sr, 2, { left, right });
            const auto r = an::analyseReferenceFile (f, {}, noCancel);
            expect (r.status == an::ReferenceResult::Status::silent, "mono sum is silent: " + r.message);
        }

        beginTest ("files: missing, empty, unsupported, silent, too short and low-rate files give clear errors, and a cancel stops the analysis");
        {
            expect (an::analyseReferenceFile (dir.getChildFile ("nope.wav"), {}, noCancel).status == an::ReferenceResult::Status::missingFile);
            auto junk = dir.getChildFile ("junk.wav");
            junk.replaceWithText ("this is not audio");
            expect (an::analyseReferenceFile (junk, {}, noCancel).status == an::ReferenceResult::Status::unsupported);
            auto txt = dir.getChildFile ("notes.mp3");
            txt.replaceWithText ("not an mp3");
            expect (an::analyseReferenceFile (txt, {}, noCancel).status == an::ReferenceResult::Status::unsupported, "MP3 is not enabled");
            expect (an::analyseReferenceFile (writeWav (dir, "silent.wav", sr, 1, { std::vector<float> (48000 * 8, 0.0f) }), {}, noCancel).status == an::ReferenceResult::Status::silent);
            expect (an::analyseReferenceFile (writeWav (dir, "short.wav", sr, 1, { noise (48000 * 2, 2) }), {}, noCancel).status == an::ReferenceResult::Status::tooShort);
            expect (an::analyseReferenceFile (writeWav (dir, "low.wav", 22050.0, 1, { noise (22050 * 10, 2) }), {}, noCancel).status == an::ReferenceResult::Status::sampleRateTooLow);
            std::atomic<bool> stop { true };
            expect (an::analyseReferenceFile (dir.getChildFile ("noise_48000.wav"), {}, stop).status == an::ReferenceResult::Status::cancelled);
        }

        beginTest ("job: runs off the calling thread, reports progress, delivers on the message thread, and cannot call back after it is stopped");
        {
            an::ReferenceJob job;
            bool done = false;
            an::ReferenceResult got;
            expect (job.start (dir.getChildFile ("noise_48000.wav"), {}, [&] (const an::ReferenceResult& r) { done = true; got = r; }));
            expect (! job.start (dir.getChildFile ("noise_48000.wav"), {}, {}), "only one job at a time");
            for (int i = 0; i < 400 && ! done; ++i)
                juce::MessageManager::getInstance()->runDispatchLoopUntil (25);
            expect (done && got.status == an::ReferenceResult::Status::ok, "delivered");
            bool called = false;
            an::ReferenceJob second;
            second.start (dir.getChildFile ("noise_48000.wav"), {}, [&] (const an::ReferenceResult&) { called = true; });
            second.stopJob();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
            expect (! called, "no callback after stopJob");
        }

        // ---- the engine ----
        GlitchKidProcessor proc;
        const auto ranges = rangesOf (proc);
        const auto ref = profileOf (plain, sr);
        auto refBright = profileOf (shaped (plain, sr, 3, 3000.0f, 8.0f), sr);
        refBright.fullMix = true;
        auto refFlat = ref;
        refFlat.fullMix = true;
        auto voc = ref; // the dry vocal measured the same way
        voc.fullMix = false;

        beginTest ("engine: an identical tone proposes nothing; a brighter reference proposes bounded presence/air boosts with a reason each; a darker one cuts");
        {
            const auto same = am::proposeReferenceInspiredEq (inputOf (refFlat, voc), ranges);
            expect (same.status == am::MatchProposal::Status::nothingToChange && same.changes.empty(), same.message);
            const auto up = am::proposeReferenceInspiredEq (inputOf (refBright, voc, 1.0f), ranges);
            expect (up.status == am::MatchProposal::Status::ok && ! up.changes.empty(), up.message);
            for (const auto& c : up.changes)
            {
                expect (c.id == "eq_b2_gain" || c.id == "eq_hi_gain" || c.id == "eq_b1_gain", "only EQ gains: " + c.id);
                expect ((c.to > c.from || c.id == "eq_b1_gain") && std::abs (c.to - c.from) <= 3.0f + 1.0e-3f && c.to <= 6.0f && (c.id != "eq_b2_gain" || c.to <= 4.0f), c.id + " bounded");
                expect (c.reason.isNotEmpty() && c.reason.contains ("dB"), "explained");
            }
            auto dark = ref;
            dark.fullMix = true;
            const auto darkRef = profileOf (shaped (plain, sr, 3, 3000.0f, -8.0f), sr);
            auto d = darkRef; d.fullMix = true;
            const auto down = am::proposeReferenceInspiredEq (inputOf (d, voc, 1.0f), ranges);
            expect (down.status == am::MatchProposal::Status::ok);
            for (const auto& c : down.changes)
                expect ((c.to < c.from || c.id == "eq_b1_gain") && c.to >= -6.0f, c.id + " cut within the limit"); // (the body bell may move the other way: the comparison is of balance, so a much brighter or darker reference also changes the relative body)
        }

        beginTest ("engine: intensity scales the proposal (zero asks for nothing), is pure and repeatable, and never touches Gain, Mix or Pitch Tune");
        {
            float last = 0.0f;
            for (float i : { 0.0f, 0.25f, 0.5f, 1.0f })
            {
                const auto p = am::proposeReferenceInspiredEq (inputOf (refBright, voc, i), ranges);
                float total = 0.0f;
                for (const auto& c : p.changes) total += std::abs (c.to - c.from);
                expect (total >= last - 1.0e-4f, "monotonic in intensity");
                last = total;
                if (i == 0.0f)
                    expect (p.changes.empty());
            }
            const auto a = am::proposeReferenceInspiredEq (inputOf (refBright, voc, 0.7f), ranges), b = am::proposeReferenceInspiredEq (inputOf (refBright, voc, 0.7f), ranges);
            expect (a.changes.size() == b.changes.size());
            for (size_t i = 0; i < a.changes.size(); ++i)
                expect (a.changes[i].id == b.changes[i].id && a.changes[i].to == b.changes[i].to);
            for (const auto& c : a.changes)
                expect (c.id != "gain" && c.id != "mix" && ! c.id.startsWith ("tune_") && c.id != "eq_lo_gain", c.id);
        }

        beginTest ("engine: a band already at its limit is left alone, the presence bell never exceeds +4 dB, and the current EQ is taken into account");
        {
            auto in = inputOf (refBright, voc, 1.0f);
            in.current.set ("eq_b2_gain", 3.5f);
            in.current.set ("eq_hi_gain", 6.0f);
            const auto p = am::proposeReferenceInspiredEq (in, ranges);
            for (const auto& c : p.changes)
            {
                expect (c.id != "eq_hi_gain", "the high shelf was already at +6 dB");
                expect (c.id != "eq_b2_gain" || c.to <= 4.0f + 1.0e-3f, "presence stays within +4 dB");
            }
            // the vocal already has +8 dB at the top from its current EQ: nothing more is asked of the high shelf
            auto with = inputOf (refBright, voc, 1.0f);
            with.currentEqDb = [] (float hz) { return hz >= 2400.0f ? 8.0f : 0.0f; };
            const auto q = am::proposeReferenceInspiredEq (with, ranges);
            for (const auto& c : q.changes)
                expect (c.id == "eq_b1_gain" || c.to - c.from < 1.0f, "the current EQ already did the work: " + c.id);
        }

        beginTest ("engine: confidence is never high for a full mix and is lowered by short or unsteady sources; mismatched or missing analyses are refused");
        {
            for (auto in : { inputOf (refBright, voc), inputOf (refFlat, voc) })
                expect (am::proposeReferenceInspiredEq (in, ranges).confidence != am::Confidence::high);
            auto shortRef = refBright;
            shortRef.activeSeconds = 3.0f;
            const auto s = am::proposeReferenceInspiredEq (inputOf (shortRef, voc), ranges);
            expect (s.confidence == am::Confidence::low && s.confidenceReasons.size() >= 2);
            auto noRef = refBright; noRef.valid = false;
            expect (am::proposeReferenceInspiredEq (inputOf (noRef, voc), ranges).status == am::MatchProposal::Status::referenceMissing);
            auto noVoc = voc; noVoc.valid = false;
            expect (am::proposeReferenceInspiredEq (inputOf (refBright, noVoc), ranges).status == am::MatchProposal::Status::vocalMissing);
            auto few = voc; few.usedBands = 8;
            expect (am::proposeReferenceInspiredEq (inputOf (refBright, few), ranges).status == am::MatchProposal::Status::incompatible);
            const auto lim = am::proposeReferenceInspiredEq (inputOf (refBright, voc), ranges);
            expect (lim.limitations.size() >= 2 && lim.limitations[0].contains ("full mix"));
        }

        beginTest ("engine: only ticked changes are applied, inside the ranges, and the real plug-in's parameters follow with the count unchanged");
        {
            GlitchKidProcessor p2;
            const auto proposal = am::proposeReferenceInspiredEq (inputOf (refBright, voc, 1.0f), p2.makeRangeLookup());
            expect (proposal.changes.size() >= 1);
            std::vector<bool> none (proposal.changes.size(), false), all (proposal.changes.size(), true);
            const auto cur = proposal.sound; // any base
            auto base = inputOf (refBright, voc).current;
            const auto untouched = am::applyMatchChanges (proposal, none, base, p2.makeRangeLookup());
            expect (untouched.get ("eq_b2_gain", 99.0f) == 0.0f, "nothing ticked, nothing changes");
            std::vector<am::MixChange> applied;
            const auto after = am::applyMatchChanges (proposal, all, base, p2.makeRangeLookup(), &applied);
            expect (applied.size() == proposal.changes.size());
            p2.applyParameterChanges (after);
            for (const auto& c : applied)
                expectWithinAbsoluteError (th::getParam (p2, c.id.toRawUTF8()), c.to, 0.06f);
            expectEquals (p2.getParameters().size(), 69);
            (void) cur;
        }

        dir.deleteRecursively();
    }
};
ReferenceMatchTests referenceMatchTests;


// Writes `data` with any JUCE format that can write (WAV, AIFF, FLAC, Ogg Vorbis); returns an empty File if the writer cannot be made.
juce::File writeWith (juce::AudioFormat& format, const juce::File& dir, const juce::String& name, double sr, int channels, const std::vector<std::vector<float>>& data, int quality = 0)
{
    auto f = dir.getChildFile (name);
    f.deleteFile();
    std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream());
    if (os == nullptr)
        return {};
    std::unique_ptr<juce::AudioFormatWriter> w (format.createWriterFor (os.get(), sr, (unsigned) channels, 16, {}, quality));
    if (w == nullptr)
        return {};
    os.release();
    juce::AudioBuffer<float> b (channels, (int) data[0].size());
    for (int c = 0; c < channels; ++c)
        b.copyFrom (c, 0, data[(size_t) c].data(), (int) data[(size_t) c].size());
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    return f;
}

struct ReferenceFormatTests : public juce::UnitTest
{
    ReferenceFormatTests() : juce::UnitTest ("Reference Match file formats", "GlitchKid") {}

    void runTest() override
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("glitchkid_reference_formats");
        dir.createDirectory();
        const juce::File fixtures (GLITCHKID_TEST_FIXTURES);
        std::atomic<bool> noCancel { false };
        const auto base = profileOf (noise (48000 * 20, 3), 48000.0); // white noise: the tone every generated file below should show

        auto checkShape = [&] (const an::ReferenceResult& r, float tolerance, const juce::String& what)
        {
            expect (r.status == an::ReferenceResult::Status::ok, what + ": " + r.message);
            if (r.status != an::ReferenceResult::Status::ok)
                return;
            for (int b = 0; b < r.profile.usedBands; ++b)
                if (std::abs (r.profile.shapeDb[(size_t) b] - base.shapeDb[(size_t) b]) > tolerance)
                {
                    expect (false, what + ": band " + juce::String (b) + " differs by " + juce::String (r.profile.shapeDb[(size_t) b] - base.shapeDb[(size_t) b], 2) + " dB");
                    return;
                }
        };

        beginTest ("formats: the chooser filter and the list shown to the user come from the formats this build can really read, and include every format we advertise");
        {
            const auto wildcard = an::referenceFileWildcard();
            for (const char* ext : { "*.wav", "*.aiff", "*.aif", "*.flac", "*.ogg", "*.mp3" })
                expect (wildcard.contains (ext), juce::String (ext) + " is offered: " + wildcard);
            const auto names = an::referenceFormatNames();
            for (const char* n : { "WAV", "AIFF", "FLAC", "Ogg Vorbis", "MP3" })
                expect (names.contains (n), juce::String (n) + " is listed: " + names);
           #if JUCE_MAC
            expect (wildcard.contains ("*.m4a"), "macOS offers M4A through CoreAudio");
           #else
            expect (! wildcard.contains ("*.m4a"), "no decoder for M4A on this platform, so it is not offered");
           #endif
            expect (! wildcard.contains ("*.opus") && ! names.contains ("Opus"), "Opus is registered by JUCE but not tested, so it is not offered");
            logMessage ("REFERENCE formats offered: " + wildcard + "  |  " + names);
        }

        beginTest ("formats: WAV, AIFF, FLAC and Ogg Vorbis each open and analyse at 44.1, 48 and 96 kHz, mono and stereo, with the right tonal shape");
        {
            juce::WavAudioFormat wav; juce::AiffAudioFormat aiff; juce::FlacAudioFormat flac; juce::OggVorbisAudioFormat ogg;
            struct Fmt { const char* name; juce::AudioFormat* format; const char* ext; float tolerance; int quality; };
            for (const Fmt& f : { Fmt { "WAV", &wav, "wav", 1.0f, 0 }, Fmt { "AIFF", &aiff, "aiff", 1.0f, 0 }, Fmt { "FLAC", &flac, "flac", 1.0f, 5 }, Fmt { "Ogg Vorbis", &ogg, "ogg", 2.5f, 6 } })
                for (double rate : { 44100.0, 48000.0, 96000.0 })
                    for (int channels : { 1, 2 })
                    {
                        const auto x = noise ((int) (rate * 24), 3);
                        const auto file = writeWith (*f.format, dir, juce::String (f.name).removeCharacters (" ") + "_" + juce::String ((int) rate) + "_" + juce::String (channels) + "." + f.ext, rate, channels,
                                                     channels == 1 ? std::vector<std::vector<float>> { x } : std::vector<std::vector<float>> { x, x }, f.quality);
                        const auto label = juce::String (f.name) + " " + juce::String ((int) rate) + " Hz " + (channels == 1 ? "mono" : "stereo");
                        expect (file.existsAsFile(), label + ": the test file could be written");
                        if (! file.existsAsFile())
                            continue;
                        const auto r = an::analyseReferenceFile (file, {}, noCancel);
                        checkShape (r, f.tolerance, label);
                        expect (r.profile.sourceChannels == channels && std::abs (r.profile.sampleRate - rate) < 1.0, label + ": channels and sample rate are read correctly");
                        expectWithinAbsoluteError ((float) r.sectionLengthSeconds, 20.0f, 0.2f);
                    }
        }

        beginTest ("formats: MP3 (a real encoded file) opens and analyses, mono or stereo");
        {
            const auto mp3 = fixtures.getChildFile ("noise_stereo_44100.mp3");
            expect (mp3.existsAsFile(), "fixture present: " + mp3.getFullPathName());
            const auto r = an::analyseReferenceFile (mp3, {}, noCancel);
            checkShape (r, 2.5f, "MP3 stereo 44.1 kHz");
            expect (r.profile.sourceChannels == 2 && std::abs (r.profile.sampleRate - 44100.0) < 1.0);
            expectWithinAbsoluteError ((float) r.fileSeconds, 8.0f, 0.3f);
            const auto mono = an::analyseReferenceFile (fixtures.getChildFile ("noise_mono_48000.mp3"), {}, noCancel);
            checkShape (mono, 2.5f, "MP3 mono 48 kHz");
            expect (mono.profile.sourceChannels == 1 && std::abs (mono.profile.sampleRate - 48000.0) < 1.0);
        }

        beginTest ("formats: M4A/AAC opens where the platform provides a decoder (macOS CoreAudio) and is refused with a clear message elsewhere");
        {
            const auto m4a = fixtures.getChildFile ("noise_mono_48000.m4a");
            expect (m4a.existsAsFile(), "fixture present");
            const auto r = an::analyseReferenceFile (m4a, {}, noCancel);
           #if JUCE_MAC
            checkShape (r, 3.0f, "M4A mono 48 kHz");
           #else
            expect (r.status == an::ReferenceResult::Status::unsupported && r.message.contains ("MP3") && r.message.contains ("FLAC"), "no decoder here, clearly said: " + r.message);
            logMessage ("REFERENCE M4A on this platform: " + r.message);
           #endif
        }

        beginTest ("formats: a file is recognised by its content when its extension is wrong or missing, and a file with an unknown extension is not trusted blindly");
        {
            juce::FlacAudioFormat flac;
            const auto x = noise (48000 * 12, 3);
            const auto real = writeWith (flac, dir, "tone.flac", 48000.0, 1, { x }, 5);
            expect (real.existsAsFile());
            const auto renamed = dir.getChildFile ("tone_flac.dat");
            real.copyFileTo (renamed);
            checkShape (an::analyseReferenceFile (renamed, {}, noCancel), 1.0f, "FLAC inside a .dat file");
            const auto noExt = dir.getChildFile ("tone_noext");
            real.copyFileTo (noExt);
            checkShape (an::analyseReferenceFile (noExt, {}, noCancel), 1.0f, "FLAC without an extension");
            const auto mp3AsWav = dir.getChildFile ("really_mp3.wav");
            fixtures.getChildFile ("noise_stereo_44100.mp3").copyFileTo (mp3AsWav);
            checkShape (an::analyseReferenceFile (mp3AsWav, {}, noCancel), 2.5f, "an MP3 named .wav");
        }

        beginTest ("formats: corrupt, truncated, empty and non-audio files give clear errors and never crash; the message lists what can be opened");
        {
            juce::OggVorbisAudioFormat ogg;
            juce::FlacAudioFormat flac;
            const auto x = noise (48000 * 12, 3);
            for (auto [name, file] : { std::pair<const char*, juce::File> { "ogg", writeWith (ogg, dir, "c.ogg", 48000.0, 1, { x }, 6) }, { "flac", writeWith (flac, dir, "c.flac", 48000.0, 1, { x }, 5) },
                                       { "mp3", fixtures.getChildFile ("noise_stereo_44100.mp3") } })
            {
                juce::MemoryBlock all;
                expect (file.loadFileAsData (all) && all.getSize() > 4000, juce::String (name) + " source present");
                // garbage in the middle (a damaged file)
                juce::MemoryBlock damaged (all);
                for (size_t i = all.getSize() / 3; i < all.getSize() / 3 + 6000 && i < all.getSize(); ++i)
                    static_cast<char*> (damaged.getData())[i] = (char) (i * 31);
                auto fd = dir.getChildFile (juce::String ("damaged_") + name + "." + name);
                fd.replaceWithData (damaged.getData(), damaged.getSize());
                const auto rd = an::analyseReferenceFile (fd, {}, noCancel);
                expect (rd.status != an::ReferenceResult::Status::cancelled && rd.message.isNotEmpty(), juce::String (name) + " damaged: " + rd.message); // (a decoder may conceal a damaged stretch and still produce audio: that is reported as analysed, not as an error)
                logMessage (juce::String ("REFERENCE damaged ") + name + ": status " + juce::String ((int) rd.status) + ", " + rd.message);
                // truncated to a few hundred bytes (a broken download)
                auto ft = dir.getChildFile (juce::String ("cut_") + name + "." + name);
                ft.replaceWithData (all.getData(), 300);
                const auto rt = an::analyseReferenceFile (ft, {}, noCancel);
                expect (rt.status != an::ReferenceResult::Status::ok && rt.message.isNotEmpty(), juce::String (name) + " truncated: " + rt.message);
            }
            auto empty = dir.getChildFile ("empty.flac");
            empty.replaceWithText ({});
            const auto re = an::analyseReferenceFile (empty, {}, noCancel);
            expect (re.status != an::ReferenceResult::Status::ok && re.message.isNotEmpty(), "empty file: " + re.message);
            auto text = dir.getChildFile ("readme.txt");
            text.replaceWithText ("not audio at all");
            const auto rx = an::analyseReferenceFile (text, {}, noCancel);
            expect (rx.status == an::ReferenceResult::Status::unsupported && rx.message.contains ("WAV") && rx.message.contains ("MP3"), "non-audio: " + rx.message);
            expect (an::analyseReferenceFile (dir, {}, noCancel).status == an::ReferenceResult::Status::missingFile, "a folder is not a file");
        }

        beginTest ("formats: cancel and progress work for every format, a long file is read in blocks, and a silent file of every format is reported silent");
        {
            juce::FlacAudioFormat flac; juce::OggVorbisAudioFormat ogg; juce::AiffAudioFormat aiff;
            const auto long1 = noise (48000 * 150, 5);
            auto f = writeWith (flac, dir, "long.flac", 48000.0, 1, { long1 }, 0);
            std::atomic<bool> stop { false };
            int calls = 0;
            const auto cancelled = an::analyseReferenceFile (f, {}, stop, [&] (float v) { if (++calls == 3 && v < 0.9f) stop.store (true); });
            expect (cancelled.status == an::ReferenceResult::Status::cancelled, "cancelled part-way: " + cancelled.message);
            const double t0 = juce::Time::getMillisecondCounterHiRes();
            const auto whole = an::analyseReferenceFile (f, {}, noCancel);
            logMessage ("REFERENCE 150 s FLAC analysed in " + juce::String (juce::Time::getMillisecondCounterHiRes() - t0, 0) + " ms (section " + juce::String (whole.sectionLengthSeconds, 1) + " s)");
            expect (whole.status == an::ReferenceResult::Status::ok && std::abs (whole.sectionLengthSeconds - 20.0) < 0.2, whole.message);
            const std::vector<float> zeros (48000 * 8, 0.0f);
            for (auto* fmt : std::initializer_list<juce::AudioFormat*> { &flac, &ogg, &aiff })
            {
                const auto s = writeWith (*fmt, dir, "silent_" + fmt->getFormatName().removeCharacters (" -") + (fmt->getFileExtensions()[0]), 48000.0, 1, { zeros }, 5);
                if (s.existsAsFile())
                    expect (an::analyseReferenceFile (s, {}, noCancel).status == an::ReferenceResult::Status::silent, fmt->getFormatName() + " silent");
            }
        }

        dir.deleteRecursively();
    }
};
ReferenceFormatTests referenceFormatTests;

void saveShot (GlitchKidEditor& ed, const char* name)
{
    const juce::String dir = juce::SystemStats::getEnvironmentVariable ("GLITCHKID_SNAPSHOT_DIR", {});
    if (dir.isEmpty())
        return;
    auto img = ed.createComponentSnapshot (ed.getLocalBounds(), true, 1.0f);
    juce::File f = juce::File (dir).getChildFile (name);
    f.deleteFile();
    juce::FileOutputStream os (f);
    if (os.openedOk())
        juce::PNGImageFormat().writeImageToStream (img, os);
}

struct ReferenceUiTests : public juce::UnitTest
{
    ReferenceUiTests() : juce::UnitTest ("Reference Match interface", "GlitchKid") {}

    static glitchkid::SoundState snapshot (GlitchKidProcessor& p) { return p.captureSoundState(); }

    void runTest() override
    {
        const double sr = 48000.0;
        const auto plain = noise (48000 * 20, 3);
        auto refBright = profileOf (shaped (plain, sr, 3, 3000.0f, 8.0f), sr);
        refBright.fullMix = true;
        auto voc = profileOf (plain, sr);

        beginTest ("saved tab numbers keep their meaning: 0-4 as before, 5 is REFERENCE, 6 KID, 7 LAB, junk behaves as before; choosing the tab remembers it");
        {
            for (int saved : { 0, 1, 2, 3, 4, 5, 6, 7 })
            {
                GlitchKidProcessor p;
                p.setUiInt ("uiLevel", saved);
                std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
                expectEquals (dynamic_cast<GlitchKidEditor*> (e.get())->uiPageForTest(), saved);
            }
            for (auto [saved, expected] : { std::pair<int, int> { 9, 4 }, { -3, 0 }, { 8, 4 } })
            {
                GlitchKidProcessor p;
                p.setUiInt ("uiLevel", saved);
                std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
                expectEquals (dynamic_cast<GlitchKidEditor*> (e.get())->uiPageForTest(), expected, "saved " + juce::String (saved));
            }
            GlitchKidProcessor p;
            std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
            auto* ed = dynamic_cast<GlitchKidEditor*> (e.get());
            ed->uiShowWorkspaceForTest (5);
            expectEquals (ed->uiPageForTest(), 5);
            expect (ed->uiReferencePanelForTest().isVisible() && ed->uiReferencePanelForTest().getParentComponent() != nullptr);
            ed->uiShowWorkspaceForTest (0);
            expect (ed->uiReferencePanelForTest().getParentComponent() == nullptr, "the panel is only part of the window while its own tab is open");
            expectEquals (p.getParameters().size(), 69);
        }

        beginTest ("the eleven rail rows are stacked without touching each other, wide enough to read, and clear of the header controls, at every window size");
        {
            GlitchKidProcessor p;
            std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
            auto* ed = dynamic_cast<GlitchKidEditor*> (e.get());
            for (int size : { 70, 100, 150 })
            {
                ed->setSize (1120 * size / 100, 680 * size / 100);
                for (int i = 0; i < 10; ++i)
                {
                    const auto a = ed->uiBarButtonBoundsForTest (i), next = ed->uiBarButtonBoundsForTest (i + 1);
                    expect (a.getBottom() <= next.getY(), "rail row " + juce::String (i) + " ends before the next row");
                    expect (a.getWidth() >= 140 && a.getHeight() >= 28, "rail row wide and tall enough");
                }
                expect (! ed->uiBarButtonBoundsForTest (10).intersects (ed->uiBarButtonBoundsForTest (11)), "the rail never reaches the header controls");
                expect (ed->uiBarButtonBoundsForTest (11).getRight() < ed->uiBarButtonBoundsForTest (12).getX(), "DRY ends before MOTION");
                expect (ed->uiBarButtonBoundsForTest (10).getBottom() <= 510, "the rail ends above its note and the level read-out");
            }
        }

        beginTest ("proposal review: nothing changes until Apply; unticked changes are not applied; only EQ gains move; Undo restores the exact sound; no high confidence");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (sr, 480);
            std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
            auto* ed = dynamic_cast<GlitchKidEditor*> (e.get());
            ed->uiShowWorkspaceForTest (5);
            auto& panel = ed->uiReferencePanelForTest();
            expect (! panel.isApplyEnabledForTest(), "no proposal before both analyses exist");
            saveShot (*ed, "reference_idle.png");
            const auto before = snapshot (p);
            panel.setProfilesForTest (refBright, voc);
            panel.setIntensityForTest (1.0f);
            expect (panel.proposalForTest().status == am::MatchProposal::Status::ok && panel.visibleRowsForTest() >= 1);
            expect (panel.isApplyEnabledForTest() && ! panel.isConfidenceHighForTest());
            saveShot (*ed, "reference_proposal.png");
            for (const auto& v : before.values)
                expectWithinAbsoluteError (th::getParam (p, v.id.toRawUTF8()), v.value, 1.0e-3f);
            const int rows = panel.visibleRowsForTest();
            for (int i = 0; i < rows; ++i)
                panel.setTickForTest (i, false);
            panel.applyForTest();
            for (const auto& v : before.values)
                expectWithinAbsoluteError (th::getParam (p, v.id.toRawUTF8()), v.value, 1.0e-3f);
            expect (! ed->uiCanUndoForTest(), "nothing ticked, nothing applied, nothing to undo");
            for (int i = 0; i < rows; ++i)
                panel.setTickForTest (i, true);
            panel.applyForTest();
            const auto after = snapshot (p);
            int moved = 0;
            for (const auto& v : after.values)
            {
                const float was = before.get (v.id, v.value);
                if (std::abs (v.value - was) > 1.0e-3f)
                {
                    ++moved;
                    expect (v.id == "eq_b1_gain" || v.id == "eq_b2_gain" || v.id == "eq_hi_gain", "only EQ gains may move: " + v.id);
                    expect (std::abs (v.value - was) <= 3.05f && std::abs (v.value) <= 6.05f);
                }
            }
            expect (moved >= 1 && ed->uiCanUndoForTest());
            for (const char* id : { "gain", "mix", "tune_on", "tune_key", "tune_scale", "tune_strength", "tune_speed", "eq_lo_gain" })
                expectWithinAbsoluteError (after.get (id, -99.0f), before.get (id, -99.0f), 1.0e-4f, id);
            expectEquals (p.getParameters().size(), 69);
            ed->uiUndoForTest();
            for (const auto& v : before.values)
                expectWithinAbsoluteError (th::getParam (p, v.id.toRawUTF8()), v.value, 1.0e-3f);
        }

        beginTest ("your vocal is measured from the plug-in input while it plays; too little audio is refused; both analyses are needed before anything is proposed");
        {
            GlitchKidProcessor p;
            p.prepareToPlay (sr, 480);
            std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
            auto* ed = dynamic_cast<GlitchKidEditor*> (e.get());
            ed->uiShowWorkspaceForTest (5);
            auto& panel = ed->uiReferencePanelForTest();
            panel.setReferenceProfileForTest (refBright);
            auto feed = [&] (int seconds, float amp)
            {
                juce::MidiBuffer midi;
                for (int blockIndex = 0; blockIndex < seconds * 100; ++blockIndex)
                {
                    auto b = th::makeNoise (2, 480, 100 + blockIndex);
                    b.applyGain (amp * 2.0f);
                    p.processBlock (b, midi);
                    if (blockIndex % 3 == 0)
                        ed->uiTickForTest();
                }
            };
            panel.startVocalForTest();
            expect (panel.vocalListeningForTest(), "listening starts");
            feed (1, 0.2f);
            panel.stopVocalForTest();
            expect (panel.statusTextForTest().contains ("Not enough"), "one second is refused: " + panel.statusTextForTest());
            expect (panel.proposalForTest().status == am::MatchProposal::Status::vocalMissing, "status after refusal: " + juce::String ((int) panel.proposalForTest().status));
            panel.startVocalForTest();
            feed (8, 0.2f);
            panel.stopVocalForTest();
            expect (! panel.vocalListeningForTest() && panel.statusTextForTest().contains ("Analysed"), panel.statusTextForTest());
            expect (panel.proposalForTest().status != am::MatchProposal::Status::vocalMissing && panel.proposalForTest().status != am::MatchProposal::Status::referenceMissing, "status after analysis: " + juce::String ((int) panel.proposalForTest().status) + " " + panel.statusTextForTest());
            expect (panel.proposalForTest().confidence != am::Confidence::high, "never high");
        }
    }
};
ReferenceUiTests referenceUiTests;
}
