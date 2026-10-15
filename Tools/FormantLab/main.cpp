// FormantLab - EXPERIMENTAL command-line harness for the Stage 4.3 formant shifter. Not part of the plugin.
// Build with -DGLITCHKID_BUILD_FORMANTLAB=ON.
//
//   FormantLab --report                       latency, CPU, memory, pitch preservation and formant accuracy on synthetic vowels
//   FormantLab --demo out_dir                 writes demo_dry.wav and demo_formant_down6.wav / demo_formant_up6.wav (synthetic vowels)
//   FormantLab in.wav out.wav semitones       formant-shifts a mono or stereo dry vocal recording (-12 .. +12 semitones;
//                                             negative = bigger / darker voice, positive = smaller / brighter voice)
#include <juce_audio_formats/juce_audio_formats.h>
#include <chrono>
#include <cstdio>
#include "../../Source/Experimental/FormantShifting/FormantShifter.h"

using namespace glitchkid::formant;

namespace
{
constexpr double pi = 3.14159265358979323846;

struct Vowel { const char* name; double F[3], B[3]; };
const Vowel vowels[] = { { "a", { 700, 1200, 2600 }, { 90, 110, 170 } }, { "i", { 300, 2300, 3000 }, { 60, 100, 150 } },
                         { "u", { 320, 800, 2500 }, { 60, 90, 150 } } };

double envelope (const Vowel& v, double f)
{
    double g = 1.0;
    for (int i = 0; i < 3; ++i)
    {
        const double Q = v.F[i] / v.B[i], r = f / v.F[i];
        g *= 1.0 / std::sqrt ((1 - r * r) * (1 - r * r) + (r / Q) * (r / Q));
    }
    return g;
}

// A sung line of vowels (a, i, u, a), 0.6 s each, with gentle vibrato; f0 glides between notes.
std::vector<float> demoVoice (double sr, int n)
{
    static const double notes[] = { 130.8, 164.8, 196.0, 164.8 };
    std::vector<float> x ((size_t) n);
    double phase = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        const int seg = std::min ((int) (t / 0.6), 3);
        const double f0 = notes[seg] * (1.0 + 0.008 * std::sin (2 * pi * 5.5 * t));
        phase += f0 / sr;
        phase -= std::floor (phase);
        const double within = std::fmod (t, 0.6);
        const double env = std::min (1.0, std::min (within / 0.05, (0.6 - within) / 0.05)); // 50 ms fades between vowels
        double s = 0.0;
        for (int h = 1; h * f0 < 0.45 * sr; ++h)
            s += envelope (vowels[seg % 3], h * f0) / h * std::cos (2 * pi * h * phase);
        x[(size_t) i] = (float) (s * 0.012 * env);
    }
    return x;
}

void writeWav (const juce::File& f, const std::vector<std::vector<float>>& channels, double sr)
{
    juce::WavAudioFormat wav;
    f.deleteFile();
    std::unique_ptr<juce::OutputStream> fs (f.createOutputStream());
    if (fs == nullptr) { std::printf ("cannot write %s\n", f.getFullPathName().toRawUTF8()); return; }
    auto w = wav.createWriterFor (fs, juce::AudioFormatWriterOptions{}.withSampleRate (sr).withNumChannels ((int) channels.size()).withBitsPerSample (24));
    if (w == nullptr) return;
    juce::AudioBuffer<float> b ((int) channels.size(), (int) channels[0].size());
    for (size_t c = 0; c < channels.size(); ++c)
        b.copyFrom ((int) c, 0, channels[c].data(), (int) channels[c].size());
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

// Shifts `channels` in place and drops the latency so the result lines up with the input.
void shiftAligned (std::vector<std::vector<float>>& channels, double sr, float semitones)
{
    FormantShifter fs;
    fs.prepare (sr);
    fs.setShiftSemitones (semitones);
    fs.reset();
    const size_t n = channels[0].size(), lat = (size_t) fs.getLatencySamples();
    for (auto& c : channels)
        c.resize (n + lat, 0.0f); // flush the last samples through the latency
    for (size_t s = 0; s < n + lat; s += 512)
    {
        const int len = (int) std::min<size_t> (512, n + lat - s);
        float* ch[2] = { channels[0].data() + s, channels.size() > 1 ? channels[1].data() + s : nullptr };
        fs.process (ch, (int) std::min<size_t> (channels.size(), 2), len);
    }
    for (auto& c : channels)
    {
        c.erase (c.begin(), c.begin() + (std::ptrdiff_t) lat);
        c.resize (n);
    }
}

double rms (const std::vector<float>& v, size_t from, size_t len)
{
    double s = 0;
    for (size_t i = from; i < from + len && i < v.size(); ++i)
        s += (double) v[i] * v[i];
    return std::sqrt (s / (double) std::max<size_t> (len, 1));
}

int report()
{
    std::printf ("GlitchKid formant shifter: synthetic-voice report (no real vocals tested)\n\n");
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        FormantShifter fs;
        fs.prepare (sr);
        std::printf ("sample rate %.0f Hz: frame %d samples, latency %d samples = %.2f ms, state %.1f KiB (mono and stereo share it)\n", sr,
                     FormantShifter::frameSizeFor (sr), fs.getLatencySamples(), fs.getLatencySamples() * 1000.0 / sr, (double) fs.getMemoryBytes() / 1024.0);
        const auto voice = demoVoice (sr, (int) (10 * sr));
        for (int channels : { 1, 2 })
        {
            double best = 1e9;
            for (int rep = 0; rep < 3; ++rep) // best of three, to ignore scheduling noise
            {
                std::vector<std::vector<float>> data ((size_t) channels, voice);
                fs.reset();
                fs.setShiftSemitones (5.0f);
                const auto t0 = std::chrono::steady_clock::now();
                for (size_t s = 0; s < voice.size(); s += 512)
                {
                    const int len = (int) std::min<size_t> (512, voice.size() - s);
                    float* ch[2] = { data[0].data() + s, channels > 1 ? data[1].data() + s : nullptr };
                    fs.process (ch, channels, len);
                }
                best = std::min (best, std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count());
            }
            std::printf ("  CPU, %s, shifting by +5 semitones: 10 s of audio in %.1f ms = %.2f %% of one core in real time\n",
                         channels == 1 ? "mono  " : "stereo", best * 1000.0, best / 10.0 * 100.0);
        }
        // neutral costs almost nothing
        {
            std::vector<std::vector<float>> data (2, voice);
            fs.reset();
            fs.setShiftSemitones (0.0f);
            const auto t0 = std::chrono::steady_clock::now();
            for (size_t s = 0; s < voice.size(); s += 512)
            {
                const int len = (int) std::min<size_t> (512, voice.size() - s);
                float* ch[2] = { data[0].data() + s, data[1].data() + s };
                fs.process (ch, 2, len);
            }
            const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::printf ("  CPU, stereo, neutral (0 semitones): %.2f %%\n", sec / 10.0 * 100.0);
        }
    }

    // Level and pitch on the demo line.
    const double sr = 48000.0;
    const auto voice = demoVoice (sr, (int) (2.4 * sr));
    std::printf ("\nlevel change of the demo vowel line (the shifter holds loudness):\n");
    for (float st : { -6.0f, -3.0f, 3.0f, 6.0f })
    {
        std::vector<std::vector<float>> d { voice };
        shiftAligned (d, sr, st);
        std::printf ("  %+3.0f semitones: %+.2f dB\n", st, 20.0 * std::log10 (rms (d[0], 12000, 96000) / rms (voice, 12000, 96000)));
    }
    return 0;
}
} // namespace

int main (int argc, char** argv)
{
    if (argc >= 2 && juce::String (argv[1]) == "--report")
        return report();

    if (argc >= 3 && juce::String (argv[1]) == "--demo")
    {
        const double sr = 48000.0;
        const auto voice = demoVoice (sr, (int) (2.4 * sr));
        const juce::File dir (argv[2]);
        dir.createDirectory();
        writeWav (dir.getChildFile ("demo_dry.wav"), { voice }, sr);
        for (float st : { -6.0f, 6.0f })
        {
            std::vector<std::vector<float>> d { voice };
            shiftAligned (d, sr, st);
            writeWav (dir.getChildFile (st < 0 ? "demo_formant_down6.wav" : "demo_formant_up6.wav"), d, sr);
        }
        std::printf ("wrote demo_dry.wav, demo_formant_down6.wav and demo_formant_up6.wav in %s (same pitch in all three)\n", argv[2]);
        return 0;
    }

    if (argc >= 4)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (juce::File (argv[1])));
        if (r == nullptr) { std::printf ("cannot read %s\n", argv[1]); return 1; }
        const int chs = (int) std::min<unsigned> (r->numChannels, 2);
        juce::AudioBuffer<float> b ((int) r->numChannels, (int) r->lengthInSamples);
        r->read (&b, 0, b.getNumSamples(), 0, true, true);
        std::vector<std::vector<float>> data;
        for (int c = 0; c < chs; ++c)
            data.emplace_back (b.getReadPointer (c), b.getReadPointer (c) + b.getNumSamples());
        const auto original = data[0];
        const float st = juce::jlimit (FormantShifter::minShiftSemitones, FormantShifter::maxShiftSemitones, juce::String (argv[3]).getFloatValue());
        shiftAligned (data, r->sampleRate, st);
        writeWav (juce::File (argv[2]), data, r->sampleRate);
        std::printf ("wrote %s: formants %+.1f semitones, %d channel(s); level change %+.2f dB; latency %d samples removed so it lines up with the input\n",
                     argv[2], (double) st, chs, 20.0 * std::log10 (rms (data[0], 0, original.size()) / std::max (rms (original, 0, original.size()), 1.0e-12)),
                     FormantShifter::latencyFor (r->sampleRate));
        return 0;
    }

    std::printf ("usage: FormantLab --report | --demo out_dir | in.wav out.wav semitones\n");
    return 1;
}
