#include "KidSpeech.h"
#if GLITCHKID_SPEECH
 #include <whisper.h>
#endif

namespace glitchkid::kid
{
// ---- SHA-1 (FIPS 180-4), for verifying the downloaded model; JUCE has SHA-256 and MD5 but whisper.cpp publishes SHA-1 ----
juce::String sha1Hex (juce::InputStream& in)
{
    uint32_t h[5] = { 0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u };
    auto rol = [] (uint32_t x, int n) { return (x << n) | (x >> (32 - n)); };
    auto block = [&] (const uint8_t* p)
    {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i)
            w[i] = (uint32_t) p[4 * i] << 24 | (uint32_t) p[4 * i + 1] << 16 | (uint32_t) p[4 * i + 2] << 8 | (uint32_t) p[4 * i + 3];
        for (int i = 16; i < 80; ++i)
            w[i] = rol (w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; ++i)
        {
            const uint32_t f = i < 20 ? (b & c) | (~b & d) : i < 40 ? b ^ c ^ d : i < 60 ? (b & c) | (b & d) | (c & d) : b ^ c ^ d;
            const uint32_t k = i < 20 ? 0x5A827999u : i < 40 ? 0x6ED9EBA1u : i < 60 ? 0x8F1BBCDCu : 0xCA62C1D6u;
            const uint32_t t = rol (a, 5) + f + e + k + w[i];
            e = d; d = c; c = rol (b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    };
    uint8_t buf[64];
    uint64_t total = 0;
    int fill = 0;
    for (;;)
    {
        const int n = in.read (buf + fill, 64 - fill);
        if (n <= 0)
            break;
        fill += n;
        total += (uint64_t) n;
        if (fill == 64)
        {
            block (buf);
            fill = 0;
        }
    }
    buf[fill++] = 0x80;
    if (fill > 56)
    {
        std::fill (buf + fill, buf + 64, (uint8_t) 0);
        block (buf);
        fill = 0;
    }
    std::fill (buf + fill, buf + 56, (uint8_t) 0);
    const uint64_t bits = total * 8;
    for (int i = 0; i < 8; ++i)
        buf[56 + i] = (uint8_t) (bits >> (56 - 8 * i));
    block (buf);
    juce::String hex;
    for (auto v : h)
        hex << juce::String::toHexString ((juce::int64) v).paddedLeft ('0', 8);
    return hex;
}

// ---- command microphone ----
CommandMic::CommandMic() = default;

CommandMic::~CommandMic() { stop(); }

bool CommandMic::start (juce::String& error)
{
    stop();
    written = 0;
    if (buffer.empty())
        buffer.resize ((size_t) (maxSeconds * 192000.0)); // 10 s at up to 192 kHz, allocated once, on the message thread, before the device starts
    // The system's default input; ASIO is not compiled in, so a host's exclusive ASIO interface is never touched.
    error = devices.initialiseWithDefaultDevices (1, 0);
    auto* dev = devices.getCurrentAudioDevice();
    if (error.isNotEmpty() || dev == nullptr || dev->getActiveInputChannels().countNumberOfSetBits() == 0)
    {
        if (error.isEmpty())
            error = "No microphone was found for KID. You can still type.";
        devices.closeAudioDevice();
        return false;
    }
    name = dev->getName();
    recording = true;
    devices.addAudioCallback (this);
    return true;
}

void CommandMic::stop()
{
    if (! recording.exchange (false) && devices.getCurrentAudioDevice() == nullptr)
        return;
    devices.removeAudioCallback (this);
    devices.closeAudioDevice();
    lastLevel = 0.0f;
}

void CommandMic::audioDeviceAboutToStart (juce::AudioIODevice* d) { rate = d->getCurrentSampleRate(); }

void CommandMic::audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut, int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    for (int c = 0; c < numOut; ++c)
        if (out[c] != nullptr)
            juce::FloatVectorOperations::clear (out[c], numSamples);
    if (! recording.load() || numIn == 0 || in[0] == nullptr)
        return;
    const int limit = juce::jmin ((int) buffer.size(), (int) (maxSeconds * rate));
    int w = written.load();
    float peak = 0.0f;
    for (int i = 0; i < numSamples && w < limit; ++i, ++w)
    {
        float v = 0.0f;
        int used = 0;
        for (int c = 0; c < numIn; ++c)
            if (in[c] != nullptr) { v += in[c][i]; ++used; }
        v = used > 0 ? v / (float) used : 0.0f;
        buffer[(size_t) w] = v;
        peak = juce::jmax (peak, std::abs (v));
    }
    written = w;
    lastLevel = peak;
}

std::vector<float> CommandMic::resampleTo16k (const float* mono, int numSamples, double sourceRate)
{
    if (mono == nullptr || numSamples <= 0 || sourceRate <= 0.0)
        return {};
    const double ratio = sourceRate / 16000.0;
    std::vector<float> out ((size_t) std::floor ((double) numSamples / ratio));
    juce::LagrangeInterpolator interp;
    interp.process (ratio, mono, out.data(), (int) out.size(), numSamples, 0);
    return out;
}

std::vector<float> CommandMic::take16k()
{
    const int n = written.exchange (0);
    return resampleTo16k (buffer.data(), n, rate);
}

// ---- model ----
juce::File SpeechModel::folder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
#if JUCE_MAC
        .getChildFile ("Application Support")
#endif
        .getChildFile ("GlitchKid").getChildFile ("KID");
}

bool ModelDownloader::verify (const juce::File& f, juce::String& error)
{
    juce::FileInputStream in (f);
    if (! in.openedOk())
    {
        error = "The downloaded file could not be read.";
        return false;
    }
    if (sha1Hex (in) != SpeechModel::sha1)
    {
        error = "The downloaded speech model did not match its published checksum, so it was not used.";
        return false;
    }
    return true;
}

ModelDownloader::ModelDownloader() : juce::Thread ("KID model download") {}
ModelDownloader::~ModelDownloader() { cancel(); }

void ModelDownloader::start (std::function<void (bool, juce::String)> onDone)
{
    if (isThreadRunning())
        return;
    done = std::move (onDone);
    fraction = 0.0f;
    startThread (juce::Thread::Priority::low);
}

void ModelDownloader::cancel() { stopThread (8000); }

void ModelDownloader::run()
{
    auto finish = [this] (bool ok, juce::String msg)
    {
        juce::MessageManager::callAsync ([cb = done, ok, msg] { if (cb) cb (ok, msg); });
    };
    const auto target = SpeechModel::file();
    if (! SpeechModel::folder().createDirectory())
        return finish (false, "Could not create " + SpeechModel::folder().getFullPathName());
    const auto part = target.getSiblingFile (juce::String (SpeechModel::fileName) + ".part");
    part.deleteFile();
    auto stream = juce::URL (SpeechModel::url).createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress).withConnectionTimeoutMs (15000).withNumRedirectsToFollow (8));
    if (stream == nullptr)
        return finish (false, "Could not reach the download server. Check your internet connection and try again.");
    {
        juce::FileOutputStream out (part);
        if (! out.openedOk())
            return finish (false, "Could not write the model file.");
        juce::HeapBlock<char> chunk (1 << 16);
        juce::int64 got = 0;
        const auto total = stream->getTotalLength() > 0 ? stream->getTotalLength() : SpeechModel::approxBytes;
        while (! threadShouldExit())
        {
            const int n = stream->read (chunk.get(), 1 << 16);
            if (n <= 0)
                break;
            out.write (chunk.get(), (size_t) n);
            got += n;
            fraction = (float) juce::jlimit (0.0, 1.0, (double) got / (double) total);
        }
        out.flush();
    }
    if (threadShouldExit())
    {
        part.deleteFile();
        return finish (false, "Download cancelled.");
    }
    juce::String error;
    if (! verify (part, error))
    {
        part.deleteFile();
        return finish (false, error);
    }
    target.deleteFile();
    if (! part.moveFileTo (target))
        return finish (false, "Could not save the model file.");
    finish (true, "Speech is ready.");
}

// ---- recogniser ----
Recogniser::Recogniser() : juce::Thread ("KID speech") {}

Recogniser::~Recogniser()
{
    stopThread (15000);
#if GLITCHKID_SPEECH
    if (ctx != nullptr)
        whisper_free (ctx);
#endif
}

bool Recogniser::available()
{
#if GLITCHKID_SPEECH
    return true;
#else
    return false;
#endif
}

void Recogniser::transcribe (std::vector<float> samples, std::function<void (juce::String, juce::String)> onText)
{
    if (isThreadRunning())
        return;
    pending = std::move (samples);
    callback = std::move (onText);
    startThread (juce::Thread::Priority::low);
}

void Recogniser::run()
{
    auto finish = [this] (juce::String text, juce::String error)
    {
        juce::MessageManager::callAsync ([cb = callback, text, error] { if (cb) cb (text, error); });
    };
#if GLITCHKID_SPEECH
    if (pending.size() < 16000 / 4)
        return finish ({}, "That was too short to hear. Hold the mic while you speak.");
    if (ctx == nullptr)
    {
        if (! SpeechModel::isInstalled())
            return finish ({}, "Speech isn't set up yet: download the speech model first.");
        whisper_log_set ([] (ggml_log_level, const char*, void*) {}, nullptr); // keep the host's console quiet
        auto cp = whisper_context_default_params();
        cp.use_gpu = false;
        ctx = whisper_init_from_file_with_params (SpeechModel::file().getFullPathName().toRawUTF8(), cp);
        if (ctx == nullptr)
            return finish ({}, "The speech model could not be loaded. Download it again.");
    }
    auto wp = whisper_full_default_params (WHISPER_SAMPLING_GREEDY);
    wp.n_threads = juce::jlimit (1, 4, juce::SystemStats::getNumCpus() / 2);
    wp.language = "en";
    wp.translate = false;
    wp.no_context = true;
    wp.no_timestamps = true;
    wp.single_segment = true;
    wp.print_special = wp.print_progress = wp.print_realtime = wp.print_timestamps = false;
    wp.suppress_blank = true;
    if (whisper_full (ctx, wp, pending.data(), (int) pending.size()) != 0)
        return finish ({}, "Speech recognition failed. Try again, or type.");
    juce::String text;
    for (int i = 0; i < whisper_full_n_segments (ctx); ++i)
        text << juce::String::fromUTF8 (whisper_full_get_segment_text (ctx, i));
    text = text.trim();
    pending.clear();
    if (text.isEmpty() || text.startsWithChar ('[')) // e.g. "[BLANK_AUDIO]"
        return finish ({}, "I didn't catch any words. Try again a little closer to the mic.");
    finish (text, {});
#else
    finish ({}, "This build of GlitchKid has no speech recognition. You can still type.");
#endif
}
}
