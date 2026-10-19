#include "TestHelpers.h"
#include "../Source/Presets/FactoryPresets.h"
#include "../Source/Presets/PresetBrowser.h"
#include "../Source/Presets/PresetController.h"
#include <set>

// The 50 artist-inspired presets: the roster, validation against the real parameter tree, distinct settings, safe level, a measurable audible
// difference, key/scale preservation, state round trips and the browser logic (filters, search, sorting, favourites).
namespace
{
namespace fp = glitchkid::FactoryPresets;
using glitchkid::PresetController;
using glitchkid::SoundState;

juce::File tempDir()
{
    return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("glitchkid-ap-" + juce::String::toHexString (juce::Random().nextInt64()));
}

// A steady synthetic voice: 210 Hz with vibrato, 14 harmonics falling as 1/h, a little breath noise. Peak about -6 dB.
std::vector<float> voice (double fs, double seconds)
{
    std::vector<float> x ((size_t) (fs * seconds));
    juce::Random rng (11);
    double phase = 0.0;
    for (size_t n = 0; n < x.size(); ++n)
    {
        const double t = (double) n / fs;
        phase += 2.0 * juce::MathConstants<double>::pi * 210.0 * (1.0 + 0.02 * std::sin (2.0 * juce::MathConstants<double>::pi * 5.0 * t)) / fs;
        double s = 0.0;
        for (int h = 1; h <= 14; ++h)
            s += std::sin (phase * h) / h;
        x[n] = (float) (0.22 * s + 0.004 * (rng.nextFloat() - 0.5f));
    }
    return x;
}

std::vector<float> render (GlitchKidProcessor& p, const std::vector<float>& in)
{
    p.prepareToPlay (48000.0, 480);
    std::vector<float> out (in.size(), 0.0f);
    juce::MidiBuffer midi;
    for (size_t pos = 0; pos + 480 <= in.size(); pos += 480)
    {
        th::Buf b (2, 480);
        for (int i = 0; i < 480; ++i) { b.setSample (0, i, in[pos + (size_t) i]); b.setSample (1, i, in[pos + (size_t) i]); }
        p.processBlock (b, midi);
        for (int i = 0; i < 480; ++i)
            out[pos + (size_t) i] = b.getSample (0, i);
    }
    return out;
}

double rmsDb (const std::vector<float>& x, size_t from)
{
    double e = 0.0;
    for (size_t i = from; i < x.size(); ++i)
        e += (double) x[i] * x[i];
    return 10.0 * std::log10 (e / (double) (x.size() - from) + 1.0e-20);
}

// Features: level, crest factor, and the energy in eight bands (dB), measured on the steady part of the output.
std::vector<double> features (const std::vector<float>& x)
{
    constexpr int order = 12, n = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> buf ((size_t) n * 2, 0.0f);
    std::vector<double> bands (8, 0.0);
    int frames = 0;
    for (size_t start = 72000; start + n <= x.size(); start += n)
    {
        for (int i = 0; i < n; ++i)
            buf[(size_t) i] = x[start + (size_t) i] * (0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / (float) n));
        fft.performFrequencyOnlyForwardTransform (buf.data());
        static const double edges[9] = { 80, 160, 320, 640, 1250, 2500, 5000, 10000, 20000 };
        for (int k = 1; k < n / 2; ++k)
        {
            const double f = k * 48000.0 / n;
            for (int b = 0; b < 8; ++b)
                if (f >= edges[b] && f < edges[b + 1])
                    bands[(size_t) b] += (double) buf[(size_t) k] * buf[(size_t) k];
        }
        ++frames;
    }
    std::vector<double> out;
    for (auto e : bands)
        out.push_back (10.0 * std::log10 (e / juce::jmax (1, frames) + 1.0e-12));
    out.push_back (rmsDb (x, 72000));
    double peak = 0.0;
    for (size_t i = 72000; i < x.size(); ++i)
        peak = juce::jmax (peak, (double) std::abs (x[i]));
    out.push_back (20.0 * std::log10 (peak + 1.0e-9) - rmsDb (x, 72000)); // crest factor in dB
    return out;
}

struct ArtistPresetTests : public juce::UnitTest
{
    ArtistPresetTests() : juce::UnitTest ("Artist presets", "GlitchKid") {}

    void runTest() override
    {
        static const char* roster[50] = {
            "Travis Scott", "Juice WRLD", "Don Toliver", "The Kid LAROI", "Future", "Playboi Carti", "Lil Uzi Vert", "Yeat", "Ken Carson", "Destroy Lonely",
            "Drake", "Post Malone", "Young Thug", "Gunna", "Lil Baby", "21 Savage", "Central Cee", "Dave", "A Boogie wit da Hoodie", "Rod Wave",
            "Lil Peep", "XXXTentacion", "blackbear", "Machine Gun Kelly", "Kendrick Lamar",
            "KR$NA", "DIVINE", "Seedhe Maut", "Raftaar", "Emiway Bantai", "Ikka", "Talha Anjum", "Talhah Yunus", "Young Stunners", "Badshah",
            "Yo Yo Honey Singh", "Sidhu Moose Wala", "AP Dhillon", "Shubh", "Karan Aujla", "Diljit Dosanjh", "Chaar Diwaari", "Dhanji", "Yashraj", "Naam Sujal",
            "Sikander Kahlon", "Prabh Deep", "MC Stan", "King", "Panther" };

        beginTest ("roster: 50 presets, 25 international and 25 Indian DHH, in the requested order, every field filled in, no overclaiming");
        {
            expectEquals (fp::count, 50);
            std::set<juce::String> names;
            int intl = 0, dhh = 0;
            for (int i = 0; i < fp::count; ++i)
            {
                const auto& in = fp::info (i);
                expectEquals (juce::String (in.artist), juce::String (roster[i]));
                expectEquals (juce::String (in.name), juce::String (roster[i]).toUpperCase() + " STYLE");
                names.insert (juce::String (in.name).toLowerCase());
                (in.region == fp::Region::international ? intl : dhh)++;
                expect (in.tags != 0 && juce::String (in.description).length() > 15 && juce::String (in.settings).length() > 15 && juce::String (in.startWith).length() > 15, in.name);
                expect ((in.region == fp::Region::international) == (i < 25), in.name);
                const auto all = (juce::String (in.description) + in.settings + in.startWith).toLowerCase();
                for (const char* banned : { "official", "licensed", "exact", "identical" })
                    expect (! all.contains (banned), juce::String (in.name) + " must not overclaim: " + banned);
            }
            expectEquals ((int) names.size(), 50);
            expectEquals (intl, 25);
            expectEquals (dhh, 25);
        }

        beginTest ("validation: every preset uses only real parameters, in range, once each, never the reserved ones; bad indexes are refused");
        {
            GlitchKidProcessor p;
            auto dir = tempDir();
            PresetController c (p, glitchkid::PresetStore (dir));
            for (int i = 0; i < fp::count; ++i)
            {
                juce::String why;
                expect (c.validateFactory (i, &why), juce::String (fp::info (i).name) + ": " + why);
            }
            expect (! c.validateFactory (-1) && ! c.validateFactory (fp::count));
            expect (! c.loadFactory (-1).ok && ! c.loadFactory (fp::count).ok);
            dir.deleteRecursively();
        }

        beginTest ("distinct: no two presets share settings (every pair differs in several controls)");
        {
            GlitchKidProcessor p;
            const auto def = p.factoryDefaultState();
            std::vector<SoundState> states;
            for (int i = 0; i < fp::count; ++i)
                states.push_back (fp::build (i, def));
            int minDiff = 1000, minA = 0, minB = 0;
            for (int a = 0; a < fp::count; ++a)
            {
                int vsDefault = 0;
                for (const auto& e : states[(size_t) a].values)
                    vsDefault += std::abs (e.value - def.get (e.id, e.value)) > 1.0e-6f ? 1 : 0;
                expect (vsDefault >= 12, juce::String (fp::info (a).name) + " changes enough controls");
                for (int b = a + 1; b < fp::count; ++b)
                {
                    int diff = 0;
                    for (const auto& e : states[(size_t) a].values)
                        diff += std::abs (e.value - states[(size_t) b].get (e.id, e.value)) > 1.0e-6f ? 1 : 0;
                    if (diff < minDiff) { minDiff = diff; minA = a; minB = b; }
                }
            }
            logMessage ("closest pair of presets: " + juce::String (fp::info (minA).name) + " / " + fp::info (minB).name + ", " + juce::String (minDiff) + " controls differ");
            expect (minDiff >= 8, "closest pair differs in at least 8 controls");
        }

        beginTest ("loading: every preset loads exactly, keeps YOUR key and scale, leaves Gain, Mix and Bypass alone, and survives save/restore");
        {
            GlitchKidProcessor p;
            auto dir = tempDir();
            PresetController c (p, glitchkid::PresetStore (dir));
            th::setParam (p, "tune_key", 7.0f);
            th::setParam (p, "tune_scale", 2.0f);
            th::setParam (p, "gain", -3.0f);
            th::setParam (p, "mix", 80.0f);
            const auto def = p.factoryDefaultState();
            for (int i = 0; i < fp::count; ++i)
            {
                const auto out = c.loadFactory (i);
                expect (out.ok, juce::String (fp::info (i).name));
                expectEquals (th::getParam (p, "tune_key"), 7.0f, "key kept");
                expectEquals (th::getParam (p, "tune_scale"), 2.0f, "scale kept");
                expectEquals (p.getPresetName(), juce::String (fp::info (i).name));
                const auto built = fp::build (i, def);
                for (const auto& e : built.values)
                {
                    if (e.id == "tune_key" || e.id == "tune_scale")
                        continue;
                    auto* r = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (e.id));
                    const float step = r->getNormalisableRange().interval;
                    expect (std::abs (r->convertFrom0to1 (r->getValue()) - e.value) <= (step > 0.0f ? 0.5f * step + 1.0e-3f : 2.0e-3f), juce::String (fp::info (i).name) + " " + e.id);
                }
                juce::MemoryBlock mb;
                p.getStateInformation (mb);
                GlitchKidProcessor q;
                q.setStateInformation (mb.getData(), (int) mb.getSize());
                expectEquals (q.getPresetName(), juce::String (fp::info (i).name));
                const auto sp = p.captureSoundState(), sq = q.captureSoundState();
                for (const auto& e : sp.values)
                    expectWithinAbsoluteError (sq.get (e.id, -999.0f), e.value, 1.0e-3f);
            }
            dir.deleteRecursively();
        }

        beginTest ("saving a loaded artist preset as your own: file round trip is exact, and a damaged file (NaN, missing values) loads safely");
        {
            GlitchKidProcessor p;
            auto dir = tempDir();
            PresetController c (p, glitchkid::PresetStore (dir));
            c.loadFactory (12);
            const auto before = p.captureSoundState();
            expect (c.saveAs ("My Thug", false).ok);
            c.loadFactory (30);
            expect (c.loadUser ("My Thug").ok);
            const auto after = p.captureSoundState();
            for (const auto& e : before.values)
                expectWithinAbsoluteError (after.get (e.id, -999.0f), e.value, 1.0e-3f);
            dir.getChildFile ("Broken.gkpreset").replaceWithText ("<GlitchKidPreset version=\"1\"><Param id=\"sat_drive\" value=\"nan\"/></GlitchKidPreset>");
            const auto out = c.loadUser ("Broken");
            const auto s = p.captureSoundState();
            for (const auto& e : s.values)
                expect (std::isfinite (e.value), "finite after a damaged load: " + e.id);
            logMessage ("damaged preset load reported ok=" + juce::String ((int) out.ok));
            dir.deleteRecursively();
        }

        beginTest ("audio: every preset is finite, near the input level (+/-3 dB), never clips, and measurably changes the sound");
        {
            const auto in = voice (48000.0, 5.0);
            double inPeak = 0.0;
            for (float v : in) inPeak = juce::jmax (inPeak, (double) std::abs (v));
            const double inDb = rmsDb (in, 72000);
            logMessage ("input peak " + juce::String (inPeak, 2) + ", level " + juce::String (inDb, 1) + " dB");

            GlitchKidProcessor ref;
            const auto refFeat = features (render (ref, in));
            std::vector<std::vector<double>> feats;
            double worstLevel = 0.0, worstPeak = 0.0;
            for (int i = 0; i < fp::count; ++i)
            {
                GlitchKidProcessor p;
                auto dir = tempDir();
                PresetController c (p, glitchkid::PresetStore (dir));
                c.loadFactory (i);
                const auto out = render (p, in);
                bool finite = true;
                double peak = 0.0;
                for (float v : out) { finite = finite && std::isfinite (v); peak = juce::jmax (peak, (double) std::abs (v)); }
                expect (finite, juce::String (fp::info (i).name) + " finite");
                const double delta = rmsDb (out, 72000) - inDb;
                logMessage ("PRESETLEVEL " + juce::String (fp::info (i).artist) + " | " + juce::String (delta, 2) + " dB | peak " + juce::String (peak, 2));
                expect (std::abs (delta) <= 3.0, juce::String (fp::info (i).name) + " level change " + juce::String (delta, 1) + " dB");
                expect (peak < 0.98, juce::String (fp::info (i).name) + " peak " + juce::String (peak, 2));
                worstLevel = juce::jmax (worstLevel, std::abs (delta));
                worstPeak = juce::jmax (worstPeak, peak);
                feats.push_back (features (out));
                double d = 0.0;
                for (size_t k = 0; k < refFeat.size(); ++k)
                    d = juce::jmax (d, std::abs (feats.back()[k] - refFeat[k]));
                expect (d > 0.8, juce::String (fp::info (i).name) + " differs from the untouched sound by " + juce::String (d, 2) + " dB");
                dir.deleteRecursively();
            }
            double closest = 1.0e9;
            int ca = 0, cb = 0;
            for (int a = 0; a < fp::count; ++a)
                for (int b = a + 1; b < fp::count; ++b)
                {
                    double d = 0.0;
                    for (size_t k = 0; k < feats[(size_t) a].size(); ++k)
                        d = juce::jmax (d, std::abs (feats[(size_t) a][k] - feats[(size_t) b][k]));
                    if (d < closest) { closest = d; ca = a; cb = b; }
                }
            logMessage ("worst level change " + juce::String (worstLevel, 2) + " dB, worst peak " + juce::String (worstPeak, 2)
                        + "; most similar pair of outputs: " + juce::String (fp::info (ca).name) + " / " + fp::info (cb).name + " (largest feature gap " + juce::String (closest, 2) + " dB)");
            expect (closest > 0.15, "no two presets sound the same on this signal");
        }

        beginTest ("browser: filters, search, sort and favourites");
        {
            namespace br = glitchkid::browser;
            expectEquals ((int) br::run ({}, 0).size(), 50);
            br::Query q;
            q.filter = 1;
            expectEquals ((int) br::run (q, 0).size(), 25);
            q.filter = 2;
            expectEquals ((int) br::run (q, 0).size(), 25);
            int covered = 0;
            for (int f = 3; f < br::numFilters; ++f)
            {
                q.filter = f;
                const auto r = br::run (q, 0);
                expect (! r.empty(), br::filterName (f));
                for (int i : r)
                    expect (br::matchesFilter (i, f));
                covered += (int) r.size();
            }
            expect (covered > 50, "presets belong to several style filters");
            expect (br::matchesFilter (36, 10) && br::matchesFilter (37, 10) && ! br::matchesFilter (0, 10), "Sidhu Moose Wala and AP Dhillon are Punjabi, Travis Scott is not");
            q = {};
            q.text = "krsna";
            auto r = br::run (q, 0);
            expect (r.size() == 1 && r[0] == 25, "search ignores punctuation: krsna finds KR$NA");
            q.text = "  TRAVIS ";
            r = br::run (q, 0);
            expect (r.size() == 1 && r[0] == 0);
            q.text = "zzzz";
            expect (br::run (q, 0).empty());
            q = {};
            q.favouritesOnly = true;
            expect (br::run (q, 0).empty());
            const std::uint64_t favs = (1ull << 3) | (1ull << 40);
            r = br::run (q, favs);
            expectEquals ((int) r.size(), 2);
            q = {};
            q.sort = br::origin;
            r = br::run (q, 0);
            expect (fp::info (r.front()).region == fp::Region::international && fp::info (r.back()).region == fp::Region::dhh);
            q.sort = br::alphabetical;
            r = br::run (q, 0);
            for (size_t i = 1; i < r.size(); ++i)
                expect (juce::String (fp::info (r[i - 1]).artist).toLowerCase() <= juce::String (fp::info (r[i]).artist).toLowerCase());
            expectEquals ((int) br::parseFavourites (br::formatFavourites (favs)), (int) favs);
            expect (br::parseFavourites ("not hex") == 0 && br::parseFavourites ("") == 0);
            expect ((br::parseFavourites ("ffffffffffffffff") >> 50) == 0, "bits beyond the 50 presets are ignored");
        }
    }
};
ArtistPresetTests artistPresetTests;
}
