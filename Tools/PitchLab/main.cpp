// PitchLab - EXPERIMENTAL command-line harness for the Stage 4.1 pitch-correction prototype.
// Not part of the plugin. Build with -DGLITCHKID_BUILD_PITCHLAB=ON.
//
//   PitchLab --report                           accuracy / latency / CPU / memory on synthetic voices
//   PitchLab --demo  out_dir                    writes demo_dry.wav and demo_tuned.wav (an out-of-tune sung line)
//   PitchLab in.wav out.wav [key scale strength retuneMs mix]
//                                               tunes a mono (or first channel of a) dry vocal recording
//     key: 0=C .. 11=B   scale: 0 chromatic 1 major 2 natural minor 3 harmonic minor 4 maj pent 5 min pent
#include <juce_audio_formats/juce_audio_formats.h>
#include <chrono>
#include <cstdio>
#include "../../Source/Experimental/PitchCorrection/PitchCorrectorPrototype.h"

using namespace glitchkid::pitch;

namespace
{
constexpr double pi = 3.14159265358979323846;

std::vector<float> voice (double sr, int n, double (*freqAt) (double t))
{
    std::vector<float> v ((size_t) n);
    double phase = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double f0 = freqAt (i / sr);
        phase += f0 / sr;
        phase -= std::floor (phase);
        double s = 0, norm = 0;
        for (int h = 1; h * f0 < 0.45 * sr && h <= 12; ++h) { s += std::sin (2 * pi * h * phase) / h; norm += 1.0 / h; }
        const double env = 0.5 - 0.5 * std::cos (2 * pi * std::fmod (i / sr, 0.5) / 0.5); // 0.5 s note envelopes
        v[(size_t) i] = (float) (0.55 * s / norm * (0.15 + 0.85 * env));
    }
    return v;
}

// A little tune, 0.5 s per note, each one 10-45 cents off, with vibrato.
double sungLine (double t)
{
    static const int notes[] = { 60, 64, 67, 64, 62, 65, 69, 65 };
    static const double offsetCents[] = { 30, -35, 20, 45, -25, 40, -30, 15 };
    const int k = ((int) (t / 0.5)) % 8;
    const double midi = notes[k] + offsetCents[k] / 100.0 + 0.25 * std::sin (2 * pi * 5.5 * t);
    return 440.0 * std::pow (2.0, (midi - 69.0) / 12.0);
}

void writeWav (const juce::File& f, const std::vector<float>& x, double sr)
{
    juce::WavAudioFormat wav;
    f.deleteFile();
    std::unique_ptr<juce::OutputStream> fs (f.createOutputStream());
    if (fs == nullptr) { std::printf ("cannot write %s\n", f.getFullPathName().toRawUTF8()); return; }
    std::unique_ptr<juce::AudioFormatWriter> w = wav.createWriterFor (fs, juce::AudioFormatWriterOptions{}.withSampleRate (sr).withNumChannels (1).withBitsPerSample (24));
    if (w == nullptr) return;
    juce::AudioBuffer<float> b (1, (int) x.size());
    b.copyFrom (0, 0, x.data(), (int) x.size());
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

int report()
{
    std::printf ("GlitchKid pitch-correction prototype: synthetic-voice report (no real vocals tested)\n\n");
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        PitchCorrectorPrototype c;
        c.prepare (sr);
        PitchCorrectorPrototype::Settings s;
        s.strength = 1.0f; s.retuneSpeedMs = 20.0f;
        c.setSettings (s);
        std::printf ("sample rate %.0f Hz: latency %d samples = %.2f ms, state %.1f KiB\n", sr, c.getLatencySamples(),
                     c.getLatencySamples() * 1000.0 / sr, (double) c.getMemoryBytes() / 1024.0);

        const auto in = voice (sr, (int) (10 * sr), sungLine);
        std::vector<float> out (in.size());
        double best = 1e9;
        for (int rep = 0; rep < 3; ++rep) // best of three, to ignore scheduling noise
        {
            c.reset();
            const auto t0 = std::chrono::steady_clock::now();
            for (size_t i = 0; i < in.size(); i += 512)
                c.processBlock (in.data() + i, out.data() + i, (int) std::min<size_t> (512, in.size() - i));
            best = std::min (best, std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count());
        }
        std::printf ("  CPU: 10 s of audio processed in %.1f ms  =  %.2f %% of one core in real time (single thread)\n", best * 1000.0, best / 10.0 * 100.0);
    }

    // End-to-end: how far from the nearest semitone is the sung line before and after correction?
    // (Measured with the prototype's own YIN detector, so this is a consistency check, not an independent one.)
    {
        const double sr = 48000.0;
        const auto in = voice (sr, (int) (8 * sr), sungLine);
        auto meanDev = [&] (const std::vector<float>& x)
        {
            YinPitchDetector d;
            d.prepare (sr);
            double sum = 0; int count = 0;
            for (size_t i = 0; i < x.size(); ++i)
                if (d.push (x[i]) && d.result().voiced && std::fmod ((double) i / sr, 0.5) > 0.25 && i > (size_t) (0.6 * sr)) // second half of each note
                {
                    const double midi = 69.0 + 12.0 * std::log2 (d.result().frequencyHz / 440.0);
                    sum += std::abs (midi - std::round (midi)) * 100.0;
                    ++count;
                }
            return count > 0 ? sum / count : -1.0;
        };
        std::printf ("\nsung line, 8 notes 15-45 cents off with +-25 cent 5.5 Hz vibrato, chromatic, strength 100%%:\n"
                     "  mean distance from nearest semitone: input %.1f cents\n", meanDev (in));
        for (float speed : { 0.0f, 25.0f, 100.0f })
        {
            PitchCorrectorPrototype c;
            c.prepare (sr);
            PitchCorrectorPrototype::Settings s;
            s.retuneSpeedMs = speed;
            c.setSettings (s);
            std::vector<float> out (in.size());
            c.processBlock (in.data(), out.data(), (int) in.size());
            std::printf ("  retune speed %5.0f ms: %.1f cents\n", speed, meanDev (out));
        }
    }
    return 0;
}
}

int main (int argc, char** argv)
{
    if (argc >= 2 && juce::String (argv[1]) == "--report")
        return report();

    if (argc >= 3 && juce::String (argv[1]) == "--demo")
    {
        const double sr = 48000.0;
        const auto in = voice (sr, (int) (4 * sr), sungLine);
        PitchCorrectorPrototype c;
        c.prepare (sr);
        PitchCorrectorPrototype::Settings s;
        s.scale = ScaleQuantizer::Scale::chromatic; s.strength = 1.0f; s.retuneSpeedMs = 25.0f;
        c.setSettings (s);
        std::vector<float> out (in.size());
        c.processBlock (in.data(), out.data(), (int) in.size());
        const juce::File dir (argv[2]);
        dir.createDirectory();
        writeWav (dir.getChildFile ("demo_dry.wav"), in, sr);
        writeWav (dir.getChildFile ("demo_tuned.wav"), out, sr); // delayed by the latency; the dry file is not
        std::printf ("wrote demo_dry.wav and demo_tuned.wav in %s (tuned file is %d samples later)\n", argv[2], c.getLatencySamples());
        return 0;
    }

    if (argc >= 3)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (juce::File (argv[1])));
        if (r == nullptr) { std::printf ("cannot read %s\n", argv[1]); return 1; }
        juce::AudioBuffer<float> b ((int) r->numChannels, (int) r->lengthInSamples);
        r->read (&b, 0, b.getNumSamples(), 0, true, true);
        std::vector<float> in (b.getReadPointer (0), b.getReadPointer (0) + b.getNumSamples()), out (in.size());

        PitchCorrectorPrototype c;
        c.prepare (r->sampleRate);
        PitchCorrectorPrototype::Settings s;
        s.key = argc > 3 ? juce::String (argv[3]).getIntValue() : 0;
        s.scale = (ScaleQuantizer::Scale) juce::jlimit (0, ScaleQuantizer::numScales - 1, argc > 4 ? juce::String (argv[4]).getIntValue() : 0);
        s.strength = argc > 5 ? juce::String (argv[5]).getFloatValue() : 1.0f;
        s.retuneSpeedMs = argc > 6 ? juce::String (argv[6]).getFloatValue() : 20.0f;
        s.mix = argc > 7 ? juce::String (argv[7]).getFloatValue() : 1.0f;
        c.setSettings (s);
        c.processBlock (in.data(), out.data(), (int) in.size());
        // Drop the latency so the result lines up with the original recording.
        out.erase (out.begin(), out.begin() + (std::ptrdiff_t) std::min<size_t> (out.size(), (size_t) c.getLatencySamples()));
        out.resize (in.size(), 0.0f);
        writeWav (juce::File (argv[2]), out, r->sampleRate);
        std::printf ("wrote %s (latency %d samples removed so it lines up with the input)\n", argv[2], c.getLatencySamples());
        return 0;
    }

    std::printf ("usage: PitchLab --report | --demo out_dir | in.wav out.wav [key scale strength retuneMs mix]\n");
    return 1;
}
