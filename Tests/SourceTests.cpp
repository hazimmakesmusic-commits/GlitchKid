#include "TestHelpers.h"
#include "../Source/AutoMode/SourceRelationship.h"
#include <atomic>

extern std::atomic<long> g_allocations; // counts every heap allocation (defined in FormantShiftTests.cpp)

// Milestone 2: source access and measurement. The plug-in has one extra, optional input bus ("Instrumental"). These tests prove it is only ever
// listened to (never reaches the output), that its status is reported truthfully, that the analysis of a synthetic lead and beat finds the conflicts
// that were built in, and that none of it allocates on the audio thread. Synthetic signals only: they show the maths, not how a real session sounds.
namespace
{
namespace au = glitchkid::analysis;
namespace am = glitchkid::automode;

constexpr double fs = 48000.0;
constexpr int blockSize = 480;

std::vector<float> lead (double seconds)
{
    std::vector<float> x ((size_t) (fs * seconds));
    for (size_t n = 0; n < x.size(); ++n)
    {
        const double t = (double) n / fs, ph = std::fmod (t, 0.5);
        const double env = ph > 0.4 ? 0.0 : std::sin (3.14159265 * ph / 0.4) * 0.3;
        double s = 0.0;
        for (int h = 1; h <= 14; ++h)
            s += std::sin (2.0 * 3.14159265 * 210.0 * h * t) / h;
        x[n] = (float) (env * s * 0.6);
    }
    return x;
}

// A sum of sines at the given frequencies (steady, so its band shares are clear).
std::vector<float> tones (std::initializer_list<double> freqs, double seconds, float amp)
{
    std::vector<float> x ((size_t) (fs * seconds));
    int k = 0;
    for (double f : freqs)
    {
        for (size_t n = 0; n < x.size(); ++n)
            x[n] += amp * (float) std::sin (2.0 * 3.14159265 * f * (double) n / fs + 0.7 * k);
        ++k;
    }
    return x;
}

bool setupLayout (GlitchKidProcessor& p, int mainChannels, int auxChannels)
{
    juce::AudioProcessor::BusesLayout l;
    const auto set = mainChannels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
    l.inputBuses.add (set);
    l.inputBuses.add (auxChannels == 0 ? juce::AudioChannelSet::disabled() : auxChannels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
    l.outputBuses.add (set);
    return p.setBusesLayout (l);
}

// Streams `in` through the plug-in in host-style blocks: a buffer holding the main channels followed by the instrumental channels.
std::vector<float> stream (GlitchKidProcessor& p, const std::vector<float>& in, const std::vector<float>* beat, int mainChannels, int auxChannels, bool prepare = true)
{
    if (prepare)
        p.prepareToPlay (fs, blockSize);
    std::vector<float> out (in.size(), 0.0f);
    juce::MidiBuffer midi;
    for (size_t pos = 0; pos + blockSize <= in.size(); pos += blockSize)
    {
        th::Buf b (mainChannels + auxChannels, blockSize);
        for (int i = 0; i < blockSize; ++i)
        {
            for (int ch = 0; ch < mainChannels; ++ch)
                b.setSample (ch, i, in[pos + (size_t) i]);
            for (int ch = 0; ch < auxChannels; ++ch)
                b.setSample (mainChannels + ch, i, beat != nullptr ? (*beat)[pos + (size_t) i] : 0.0f);
        }
        p.processBlock (b, midi);
        for (int i = 0; i < blockSize; ++i)
            out[pos + (size_t) i] = b.getSample (0, i);
    }
    return out;
}

struct SourceTests : public juce::UnitTest
{
    SourceTests() : juce::UnitTest ("Sources", "GlitchKid") {}

    void runTest() override
    {
        beginTest ("layouts: the instrumental input is optional (off, mono or stereo); the main bus rules are unchanged");
        {
            GlitchKidProcessor p;
            expectEquals (p.getBusCount (true), 2);
            expect (! p.getBus (true, 1)->isEnabled(), "off by default: a fresh instance is exactly the old single-input plug-in");
            auto layout = [&] (juce::AudioChannelSet in, juce::AudioChannelSet aux, juce::AudioChannelSet out)
            {
                juce::AudioProcessor::BusesLayout l;
                l.inputBuses.add (in);
                l.inputBuses.add (aux);
                l.outputBuses.add (out);
                return p.isBusesLayoutSupported (l);
            };
            const auto st = juce::AudioChannelSet::stereo(), mo = juce::AudioChannelSet::mono(), off = juce::AudioChannelSet::disabled();
            expect (layout (st, off, st), "stereo, no instrumental");
            expect (layout (st, st, st), "stereo + stereo instrumental");
            expect (layout (st, mo, st), "stereo + mono instrumental");
            expect (layout (mo, st, mo), "mono + stereo instrumental");
            expect (layout (mo, off, mo), "mono, no instrumental");
            expect (! layout (st, juce::AudioChannelSet::create5point1(), st), "a surround instrumental is refused");
            expect (! layout (mo, off, st), "mismatched main in/out refused");
            expect (! layout (juce::AudioChannelSet::create5point1(), off, juce::AudioChannelSet::create5point1()), "a surround main bus is refused");
            expect (setupLayout (p, 2, 2), "can set stereo + stereo");
            expect (p.getBus (true, 1)->isEnabled(), "instrumental bus enabled");
            expect (setupLayout (p, 2, 0), "can switch it off again");
            expect (! p.getBus (true, 1)->isEnabled(), "instrumental bus disabled");
        }

        beginTest ("isolation: with the instrumental on, the output is bit-identical to the same plug-in without it (stereo and mono main)");
        {
            const auto voice = lead (3.0);
            const auto beat = tones ({ 55.0, 110.0, 800.0, 1500.0 }, 3.0, 0.4f);
            for (int mainCh = 1; mainCh <= 2; ++mainCh)
            {
                GlitchKidProcessor a, b;
                for (auto* p : { &a, &b })
                {
                    th::setParam (*p, "tune_on", 1.0f);
                    th::setParam (*p, "echo_on", 1.0f);
                    th::setParam (*p, "sat_drive", 6.0f);
                    th::setParam (*p, "comp_ratio", 4.0f);
                }
                expect (setupLayout (a, mainCh, 0) && setupLayout (b, mainCh, 2));
                const auto outA = stream (a, voice, nullptr, mainCh, 0);
                const auto outB = stream (b, voice, &beat, mainCh, 2);
                bool same = true;
                for (size_t i = 0; i < outA.size(); ++i)
                    same = same && std::memcmp (&outA[i], &outB[i], sizeof (float)) == 0;
                expect (same, "the beat never reaches the output (main channels = " + juce::String (mainCh) + ")");
            }
        }

        beginTest ("status: not connected, connected but silent, live; a short host buffer cannot be read past its end");
        {
            GlitchKidProcessor p;
            expect (setupLayout (p, 2, 0));
            stream (p, lead (0.5), nullptr, 2, 0);
            expect (p.getInstrumentalStatus() == GlitchKidProcessor::SourceStatus::notConnected);
            expect (setupLayout (p, 2, 2));
            stream (p, lead (2.0), nullptr, 2, 2); // bus on, carrying zeros
            expect (p.getInstrumentalStatus() == GlitchKidProcessor::SourceStatus::connectedSilent);
            const auto beat = tones ({ 100.0, 900.0 }, 2.0, 0.3f);
            stream (p, lead (2.0), &beat, 2, 2);
            expect (p.getInstrumentalStatus() == GlitchKidProcessor::SourceStatus::live);

            // a host that hands over only the main channels although the bus is on: no crash, reported as not connected
            p.prepareToPlay (fs, blockSize);
            th::Buf small (2, blockSize);
            small.clear();
            juce::MidiBuffer midi;
            p.processBlock (small, midi);
            expect (p.getInstrumentalStatus() == GlitchKidProcessor::SourceStatus::notConnected);
        }

        beginTest ("analysis: the instrumental is measured only when it is connected; lead and beat results are separate");
        {
            const auto voice = lead (12.0);
            {
                GlitchKidProcessor p;
                expect (setupLayout (p, 2, 0));
                p.prepareToPlay (fs, blockSize);
                p.startSourceAnalysis();
                stream (p, voice, nullptr, 2, 0, false);
                expect (p.getAnalyzer().fetch().status == au::VocalAnalysis::Status::ready);
                expect (p.getInstrumentalAnalyzer().fetch().status != au::VocalAnalysis::Status::ready, "no beat was heard, so no beat analysis exists");
                expect (! am::compare (p.getAnalyzer().fetch(), p.getInstrumentalAnalyzer().fetch()).valid);
            }
            {
                GlitchKidProcessor p;
                expect (setupLayout (p, 2, 2));
                p.prepareToPlay (fs, blockSize);
                p.startSourceAnalysis();
                const auto bass = tones ({ 38.0, 47.0, 58.0 }, 12.0, 0.25f);
                stream (p, voice, &bass, 2, 2, false);
                const auto lv = p.getAnalyzer().fetch(), bv = p.getInstrumentalAnalyzer().fetch();
                expect (lv.status == au::VocalAnalysis::Status::ready && bv.status == au::VocalAnalysis::Status::ready);
                expect (bv.bandFraction[0] > 0.75f, "a bass-only beat is nearly all low end: " + juce::String (bv.bandFraction[0], 2));
                const auto rel = am::compare (lv, bv);
                expect (rel.valid && rel.beatLowEndHeavy && rel.confidence > 0.9f);
                expect (rel.maskRisk[2] < 0.35f && rel.maskRisk[3] < 0.35f, "a bass beat does not cover the voice's mids");
            }
            {
                GlitchKidProcessor p;
                expect (setupLayout (p, 2, 2));
                p.prepareToPlay (fs, blockSize);
                p.startSourceAnalysis();
                const auto pad = tones ({ 620.0, 910.0, 1330.0, 1810.0 }, 12.0, 0.22f); // a pad sitting right on the voice
                stream (p, voice, &pad, 2, 2, false);
                const auto rel = am::compare (p.getAnalyzer().fetch(), p.getInstrumentalAnalyzer().fetch());
                expect (rel.valid && rel.worstBand == 2, "a mid-range pad is found competing in the mids (worst band " + juce::String (rel.worstBand) + ")");
                expect (rel.maskRisk[2] > 0.5f && ! rel.reasons.empty());
                for (const auto& r : rel.reasons)
                    logMessage ("finding: " + juce::String (r));
            }
        }

        beginTest ("relationship maths on constructed analyses: unavailable states, confidence, level and headroom findings");
        {
            au::VocalAnalysis lead, beat;
            expect (! am::compare (lead, beat).valid && ! am::compare (lead, beat).unavailable.empty());
            lead.status = au::VocalAnalysis::Status::ready;
            lead.rmsDb = -20.0f; lead.activeSeconds = 8.0f; lead.bandFraction = { 0.05f, 0.4f, 0.4f, 0.1f, 0.05f };
            auto r = am::compare (lead, beat);
            expect (! r.valid && r.unavailable.find ("instrumental") != std::string::npos);
            beat = lead;
            beat.rmsDb = -10.0f; beat.peakDb = -0.2f; beat.activeSeconds = 1.0f;
            r = am::compare (lead, beat);
            expect (r.valid && r.beatVeryLoud && std::abs (r.vocalMinusBeatDb + 10.0f) < 1.0e-3f);
            expect (r.confidence < 0.2f, "one second of audio is low confidence");
            expect (r.worstBand >= 1, "an equally shaped but 10 dB louder beat covers the voice");
            bool loud = false, rough = false;
            for (const auto& s : r.reasons) { loud = loud || s.find ("louder than the lead") != std::string::npos; rough = rough || s.find ("rough guide") != std::string::npos; }
            expect (loud && rough);
            beat.rmsDb = -40.0f; beat.peakDb = -20.0f; beat.activeSeconds = 8.0f;
            r = am::compare (lead, beat);
            expect (r.valid && r.worstBand < 0 && ! r.beatVeryLoud && r.confidence > 0.99f, "a much quieter beat is no conflict");
        }

        beginTest ("safety: NaN/Inf on the instrumental input is ignored, output stays finite; nothing allocates on the audio thread");
        {
            GlitchKidProcessor p;
            expect (setupLayout (p, 2, 2));
            auto beat = tones ({ 100.0, 1000.0 }, 2.0, 0.3f);
            for (size_t i = 0; i < beat.size(); i += 97)
                beat[i] = (i % 2) ? std::numeric_limits<float>::quiet_NaN() : std::numeric_limits<float>::infinity();
            p.prepareToPlay (fs, blockSize);
            p.startSourceAnalysis();
            const auto out = stream (p, lead (2.0), &beat, 2, 2, false);
            for (float v : out)
                expect (std::isfinite (v));
            expect (p.getInstrumentalAnalyzer().fetch().nonFiniteSamples > 0 || p.getInstrumentalAnalyzer().fetch().status == au::VocalAnalysis::Status::listening);

            GlitchKidProcessor q;
            expect (setupLayout (q, 2, 2));
            q.prepareToPlay (fs, blockSize);
            q.startSourceAnalysis();
            juce::MidiBuffer midi;
            th::Buf b (4, blockSize);
            for (int ch = 0; ch < 4; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    b.setSample (ch, i, 0.1f * std::sin (0.05f * (float) (i + ch)));
            for (int i = 0; i < 50; ++i)
                q.processBlock (b, midi);
            long allocated = 0;
            int firstBlock = -1;
            for (int i = 0; i < 400; ++i)
            {
                const long before = g_allocations.load();
                q.processBlock (b, midi);
                const long now = g_allocations.load() - before;
                if (now > 0 && firstBlock < 0)
                    firstBlock = i;
                allocated += now;
            }
            if (allocated > 0)
                logMessage ("first allocation in block " + juce::String (firstBlock));
            expectEquals ((int) allocated, 0, "no heap allocation while processing with the instrumental on and analysing");
        }
    }
};
SourceTests sourceTests;
}
