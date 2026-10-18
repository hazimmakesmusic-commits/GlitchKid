// DoubleLab - command-line listening harness for the Vocal Doubler and the Harmony voice (Stage 4.8). Not part of the plug-in.
// Build with -DGLITCHKID_BUILD_DOUBLELAB=ON.
//
//   DoubleLab --report                 latency, CPU and memory of both effects on a synthetic voice
//   DoubleLab --demo out_dir           writes the same set of files as below for a synthetic sung line (no recording needed)
//   DoubleLab in.wav out_dir           renders your dry vocal (mono or stereo, any sample rate) six ways into out_dir, all time-aligned to the input:
//       01_dry.wav               the input (a mono file is copied to both channels, so the width of the doubles can be heard)
//       02_double_default.wav    Vocal Doubler at the plug-in's defaults (Double 50 %, Width 70 %, Detune 8 cents, Timing 18 ms)
//       03_double_strong.wav     Double 100 %, Width 100 % (mono-safe), Detune 15 cents, Timing 26 ms
//       04_harmony_fifth_up.wav  the lead plus one voice a fifth ABOVE (+7 semitones) at -9 dB
//       05_harmony_octave_down.wav  the lead plus one voice an octave BELOW (-12 semitones) at -9 dB
//       06_double_and_harmony.wav   default double plus a major third UP (+4) harmony at -9 dB
//   Listening notes: the doubles are made from the same take, so they follow the same words and phrasing exactly; only timing and pitch wobble
//   differ. The harmony is a FIXED interval (not scale-aware) and works on one voice at a time; it is a creative effect and sounds most natural for a
//   few semitones. Nothing in this tool is a claim about sound quality: it only lets you hear it.
#include <juce_audio_formats/juce_audio_formats.h>
#include <chrono>
#include <cstdio>
#include "../../Source/DSP/VocalDoubling/VocalDoubler.h"
#include "../../Source/DSP/Harmony/HarmonyVoice.h"

using glitchkid::harmony::HarmonyVoice;

namespace
{
constexpr double pi = 3.14159265358979323846;
using Channels = std::vector<std::vector<float>>;

Channels demoVoice (double sr, double seconds)
{
    static const double notes[] = { 130.8, 164.8, 196.0, 220.0, 196.0, 164.8 };
    const int n = (int) (seconds * sr);
    std::vector<float> x ((size_t) n);
    double phase = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        const int seg = std::min ((int) (t / 0.8), 5);
        const double f0 = notes[seg] * (1.0 + 0.006 * std::sin (2 * pi * 5.5 * t));
        phase += f0 / sr;
        phase -= std::floor (phase);
        const double within = std::fmod (t, 0.8);
        const double env = std::min (1.0, std::min (within / 0.04, (0.8 - within) / 0.06));
        double s = 0.0;
        for (int h = 1; h * f0 < 0.45 * sr && h <= 14; ++h)
            s += std::sin (2 * pi * h * phase) / (double) h;
        x[(size_t) i] = (float) (0.25 * s * env);
    }
    return { x, x };
}

void writeWav (const juce::File& f, const Channels& channels, double sr)
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

Channels doubleIt (const Channels& in, double sr, const DoublerSettings& s)
{
    VocalDoubler d;
    d.setSettings (s);
    d.prepare (sr);
    Channels out = in;
    const size_t n = in[0].size();
    for (size_t start = 0; start < n; start += 512)
    {
        const int len = (int) std::min<size_t> (512, n - start);
        float* ch[2] = { out[0].data() + start, out[1].data() + start };
        juce::AudioBuffer<float> view (ch, 2, len);
        d.process (view);
    }
    return out;
}

// The lead plus the harmony voice, with the delay removed so the result lines up with the input.
Channels harmonise (const Channels& in, double sr, float semitones, float levelDb)
{
    HarmonyVoice h;
    h.setSettings ({ semitones, levelDb });
    h.prepare (sr);
    const size_t n = in[0].size(), lat = (size_t) h.getLatencySamples();
    Channels out = in;
    for (auto& c : out)
        c.resize (n + lat, 0.0f); // flush the last samples through the latency
    for (size_t start = 0; start < n + lat; start += 512)
    {
        const int len = (int) std::min<size_t> (512, n + lat - start);
        float* ch[2] = { out[0].data() + start, out[1].data() + start };
        h.process (ch, 2, len);
    }
    for (auto& c : out)
    {
        c.erase (c.begin(), c.begin() + (std::ptrdiff_t) lat);
        c.resize (n);
    }
    return out;
}

void renderAll (const Channels& dry, double sr, const juce::File& dir)
{
    dir.createDirectory();
    DoublerSettings def; def.on = true;
    DoublerSettings strong; strong.on = true; strong.amountPct = 100.0f; strong.widthPct = 100.0f; strong.detuneCents = 15.0f; strong.timingMs = 26.0f;
    writeWav (dir.getChildFile ("01_dry.wav"), dry, sr);
    writeWav (dir.getChildFile ("02_double_default.wav"), doubleIt (dry, sr, def), sr);
    writeWav (dir.getChildFile ("03_double_strong.wav"), doubleIt (dry, sr, strong), sr);
    writeWav (dir.getChildFile ("04_harmony_fifth_up.wav"), harmonise (dry, sr, 7.0f, -9.0f), sr);
    writeWav (dir.getChildFile ("05_harmony_octave_down.wav"), harmonise (dry, sr, -12.0f, -9.0f), sr);
    writeWav (dir.getChildFile ("06_double_and_harmony.wav"), doubleIt (harmonise (dry, sr, 4.0f, -9.0f), sr, def), sr);
    std::printf ("wrote 6 files to %s (all time-aligned with 01_dry.wav)\n", dir.getFullPathName().toRawUTF8());
}

int report()
{
    std::printf ("GlitchKid Vocal Doubler + Harmony: synthetic-voice report (no real vocals tested)\n\n");
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("sample rate %.0f Hz: Doubler latency 0 samples; Harmony latency %d samples = %.2f ms (same as Pitch Tune)\n", sr,
                     HarmonyVoice::latencyFor (sr), HarmonyVoice::latencyFor (sr) * 1000.0 / sr);
        const auto voice = demoVoice (sr, 10.0);
        {
            DoublerSettings s; s.on = true;
            VocalDoubler d; d.setSettings (s); d.prepare (sr);
            Channels data = voice;
            const auto t0 = std::chrono::steady_clock::now();
            for (size_t start = 0; start < voice[0].size(); start += 512)
            {
                const int len = (int) std::min<size_t> (512, voice[0].size() - start);
                float* ch[2] = { data[0].data() + start, data[1].data() + start };
                juce::AudioBuffer<float> view (ch, 2, len);
                d.process (view);
            }
            const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::printf ("  Doubler, stereo, defaults: %.2f %% of one core\n", sec / 10.0 * 100.0);
        }
        {
            HarmonyVoice h; h.setSettings ({ 7.0f, -9.0f }); h.prepare (sr);
            Channels data = voice;
            const auto t0 = std::chrono::steady_clock::now();
            for (size_t start = 0; start < voice[0].size(); start += 512)
            {
                const int len = (int) std::min<size_t> (512, voice[0].size() - start);
                float* ch[2] = { data[0].data() + start, data[1].data() + start };
                h.process (ch, 2, len);
            }
            const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::printf ("  Harmony (a fifth up), stereo: %.2f %% of one core\n", sec / 10.0 * 100.0);
        }
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
        renderAll (demoVoice (48000.0, 5.0), 48000.0, juce::File (argv[2]));
        return 0;
    }
    if (argc >= 3)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (juce::File (argv[1])));
        if (reader == nullptr) { std::printf ("cannot read %s\n", argv[1]); return 1; }
        const int n = (int) reader->lengthInSamples;
        juce::AudioBuffer<float> b ((int) juce::jmin (2u, reader->numChannels), n);
        reader->read (&b, 0, n, 0, true, true);
        Channels dry (2);
        for (int c = 0; c < 2; ++c)
        {
            dry[(size_t) c].resize ((size_t) n);
            const int src = juce::jmin (c, b.getNumChannels() - 1); // a mono file feeds both channels
            std::copy (b.getReadPointer (src), b.getReadPointer (src) + n, dry[(size_t) c].begin());
        }
        renderAll (dry, reader->sampleRate, juce::File (argv[2]));
        return 0;
    }
    std::printf ("usage: DoubleLab --report | DoubleLab --demo out_dir | DoubleLab in.wav out_dir\n");
    return 1;
}
