#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/Analysis/VocalFileAnalysis.h"

namespace
{
using namespace glitchkid::analysis;

juce::File writeWav (const juce::File& dir, const juce::String& name, double sr, const std::vector<std::vector<float>>& data)
{
    auto f = dir.getChildFile (name);
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
    auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (sr).withNumChannels ((int) data.size()).withBitsPerSample (24));
    if (w != nullptr)
    {
        juce::AudioBuffer<float> b ((int) data.size(), (int) data[0].size());
        for (size_t c = 0; c < data.size(); ++c)
            b.copyFrom ((int) c, 0, data[c].data(), (int) data[c].size());
        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }
    return f;
}

// A sung line: harmonic tones (5 partials) on the given MIDI notes, with short breaths of quiet noise at `noiseDb` RMS between phrases.
std::vector<float> melody (double sr, const std::vector<std::pair<int, float>>& notes, int repeats, float noiseDb, juce::int64 seed = 7)
{
    juce::Random rng (seed);
    const float noiseAmp = std::sqrt (3.0f) * juce::Decibels::decibelsToGain (noiseDb);
    std::vector<float> x;
    double phase = 0.0;
    for (int r = 0; r < repeats; ++r)
    {
        for (auto [midi, seconds] : notes)
        {
            const double hz = 440.0 * std::pow (2.0, (midi - 69) / 12.0);
            for (int i = 0; i < (int) (seconds * sr); ++i)
            {
                phase += juce::MathConstants<double>::twoPi * hz / sr;
                float s = 0.0f;
                for (int k = 1; k <= 5; ++k)
                    s += (float) std::sin (phase * k) / (float) k;
                x.push_back (0.25f * s + noiseAmp * (rng.nextFloat() * 2.0f - 1.0f));
            }
        }
        for (int i = 0; i < (int) (0.4 * sr); ++i) // breath between phrases: only the background noise
            x.push_back (noiseAmp * (rng.nextFloat() * 2.0f - 1.0f));
    }
    return x;
}

const std::vector<std::pair<int, float>> aMinorLine { { 57, 1.0f }, { 60, 0.5f }, { 64, 1.0f }, { 62, 0.5f }, { 60, 0.5f }, { 59, 0.5f }, { 57, 1.0f },
                                                      { 55, 0.5f }, { 53, 0.5f }, { 52, 1.0f }, { 57, 1.0f } };

struct VocalFileTests : public juce::UnitTest
{
    VocalFileTests() : juce::UnitTest ("Vocal file analysis", "GlitchKid") {}

    void runTest() override
    {
        juce::TemporaryFile tmpDir;
        const auto dir = tmpDir.getFile();
        dir.createDirectory();
        const double sr = 48000.0;
        std::atomic<bool> noCancel { false };

        beginTest ("a sung A-minor line: the key is reported (C major / A minor notes), pitch is clear, the background noise is estimated, nothing is clipped");
        {
            const auto f = writeWav (dir, "line.wav", sr, { melody (sr, aMinorLine, 3, -70.0f) });
            float lastProgress = -1.0f;
            bool monotonic = true;
            const auto r = analyseVocalFile (f, noCancel, [&] (float p) { monotonic = monotonic && p >= lastProgress; lastProgress = p; });
            expect (r.ok(), r.message);
            expect (monotonic && std::abs (lastProgress - 1.0f) < 1.0e-6f, "progress runs 0..1");
            expectEquals (r.channels, 1);
            expectEquals (r.clippedSamples, 0);
            expect (r.keyReliable, r.keyNote);
            const int noteSet = r.keyMinor ? (r.keyTonic + 3) % 12 : r.keyTonic;
            expectEquals (noteSet, 0, "the notes of C major / A minor, got " + keyName (r.keyTonic, r.keyMinor));
            expect (r.voicedShare > 0.7f, "voiced share " + juce::String (r.voicedShare));
            expect (r.medianHz > 180.0f && r.medianHz < 280.0f, "median " + juce::String (r.medianHz));
            expect (r.offNoteCents < 10.0f, "in-tune synthetic notes: " + juce::String (r.offNoteCents) + " cents");
            expect (r.noiseFloorKnown, "noise floor estimated");
            expectWithinAbsoluteError (r.noiseFloorDb, -70.0f, 6.0f, "noise floor " + juce::String (r.noiseFloorDb));
            expect (r.tone.status == VocalAnalysis::Status::ready, "tone measured");
            expectEquals ((int) r.overview.size(), VocalFileReport::overviewColumns);
            const auto lines = describeVocalFile (r);
            for (const auto& l : lines)
                logMessage ("  " + l);
            for (const auto& l : lines)
                expect (l.startsWith ("MEASURED  ") || l.startsWith ("ESTIMATE  "), "every line says what it is: " + l);
            expect (lines.joinIntoString (" ").contains ("no noise reduction"), "honest about noise reduction");
        }

        beginTest ("too little singing, unpitched noise, or only two notes: no key is reported");
        {
            const auto shortLine = analyseVocalFile (writeWav (dir, "short.wav", sr, { melody (sr, { { 57, 1.0f }, { 60, 1.0f }, { 64, 1.0f } }, 1, -90.0f) }), noCancel);
            expect (shortLine.ok() && ! shortLine.keyReliable, shortLine.keyNote);
            juce::Random rng (3);
            std::vector<float> noise ((size_t) (sr * 12));
            for (auto& s : noise) s = 0.2f * (rng.nextFloat() * 2.0f - 1.0f);
            const auto rap = analyseVocalFile (writeWav (dir, "noise.wav", sr, { noise }), noCancel);
            expect (rap.ok() && ! rap.keyReliable && rap.voicedShare < 0.2f, "noise: voiced " + juce::String (rap.voicedShare) + ", " + rap.keyNote);
            const auto twoNotes = analyseVocalFile (writeWav (dir, "two.wav", sr, { melody (sr, { { 57, 1.0f }, { 64, 1.0f } }, 6, -90.0f) }), noCancel);
            expect (twoNotes.ok() && ! twoNotes.keyReliable, twoNotes.keyNote);
        }

        beginTest ("stereo: identical channels are reported as mono content, clipping is counted, edited silence is not called noise");
        {
            auto x = melody (sr, aMinorLine, 2, -200.0f); // two phrases: two edited-silent breaths
            for (size_t i = 10000; i < 10100; ++i) x[i] = 1.0f;
            const auto r = analyseVocalFile (writeWav (dir, "stereo.wav", sr, { x, x }), noCancel);
            expect (r.ok(), r.message);
            expectEquals (r.channels, 2);
            expect (r.stereoIdentical, "identical channels");
            expect (r.clippedSamples >= 100, "clipped " + juce::String (r.clippedSamples));
            expect (! r.noiseFloorKnown && r.digitalSilenceGaps, "digital-silence gaps");
            expect (describeVocalFile (r).joinIntoString (" ").contains ("cannot repair clipping"), "honest about clipping");
        }

        beginTest ("errors: missing, not audio, silent, cancelled; the job reports on the message thread");
        {
            expect (analyseVocalFile (dir.getChildFile ("nope.wav"), noCancel).status == VocalFileReport::Status::missingFile);
            const auto txt = dir.getChildFile ("notes.wav");
            txt.replaceWithText ("not audio at all");
            expect (analyseVocalFile (txt, noCancel).status == VocalFileReport::Status::unsupported);
            const auto silent = writeWav (dir, "silent.wav", sr, { std::vector<float> ((size_t) sr * 3, 0.0f) });
            expect (analyseVocalFile (silent, noCancel).status == VocalFileReport::Status::silent);
            std::atomic<bool> cancel { true };
            expect (analyseVocalFile (writeWav (dir, "c.wav", sr, { melody (sr, aMinorLine, 1, -80.0f) }), cancel).status == VocalFileReport::Status::cancelled);

            VocalFileJob job;
            std::atomic<bool> done { false };
            VocalFileReport got;
            expect (job.start (dir.getChildFile ("line.wav"), [&] (const VocalFileReport& rep) { got = rep; done = true; }));
            for (int i = 0; i < 600 && ! done.load(); ++i)
                juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
            expect (done.load() && got.ok(), "job finished: " + got.message);
            job.stopJob();
        }
        dir.deleteRecursively();
    }
};

static VocalFileTests vocalFileTests;
}
