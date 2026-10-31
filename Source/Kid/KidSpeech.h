#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <atomic>
#include <functional>
#include <vector>

struct whisper_context;

namespace glitchkid::kid
{
// KID's push-to-talk speech, entirely on the user's machine.
//  * CommandMic: a SEPARATE input device (the system's default input, never ASIO), opened only while the artist holds the mic button. Its audio goes to a
//    private buffer for transcription and is discarded; it never reaches processBlock, the analyser, the display taps or the saved session.
//  * SpeechModel: where the recognition model lives, and an opt-in download that is verified (SHA-1 from whisper.cpp's model list) before it is ever loaded.
//  * Recogniser: whisper.cpp on a background thread; results come back on the message thread.
// Nothing here runs on the audio thread of the plug-in.

juce::String sha1Hex (juce::InputStream&); // lower-case hex SHA-1 of the whole stream (used to verify the model file)

class CommandMic : private juce::AudioIODeviceCallback
{
public:
    static constexpr double maxSeconds = 10.0;
    CommandMic();
    ~CommandMic() override;
    bool start (juce::String& error); // opens the default input device and records (at most maxSeconds)
    void stop();                      // stops and closes the device; the recording stays until take16k()
    bool isRecording() const { return recording.load(); }
    float level() const { return lastLevel.load(); } // recent peak, 0..1 (for the waveform)
    juce::String deviceName() const { return name; }
    std::vector<float> take16k();     // the recording as mono 16 kHz (what the recogniser needs), then cleared
    static std::vector<float> resampleTo16k (const float* mono, int numSamples, double sourceRate);

private:
    juce::AudioDeviceManager devices;
    std::vector<float> buffer;
    std::atomic<int> written { 0 };
    std::atomic<bool> recording { false };
    std::atomic<float> lastLevel { 0.0f };
    double rate = 48000.0;
    juce::String name;
    void audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const*, int numOut, int numSamples, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override {}
};

struct SpeechModel
{
    static constexpr const char* fileName = "ggml-base.en.bin";
    static constexpr const char* url = "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin";
    static constexpr const char* sha1 = "137c40403d78fd54d454da0f9bd998f78703390c"; // whisper.cpp v1.9.5 models/README.md
    static constexpr juce::int64 approxBytes = 147951465;
    static juce::File folder(); // ~/Library/Application Support/GlitchKid/KID (macOS), %APPDATA%\GlitchKid\KID (Windows)
    static juce::File file() { return folder().getChildFile (fileName); }
    static bool isInstalled() { return file().existsAsFile() && file().getSize() > 1000000; }
};

// Opt-in download of the model on a background thread, verified before it replaces anything.
class ModelDownloader : private juce::Thread
{
public:
    ModelDownloader();
    ~ModelDownloader() override;
    void start (std::function<void (bool ok, juce::String message)> onDone); // onDone runs on the message thread
    void cancel();
    bool isRunning() const { return isThreadRunning(); }
    float progress() const { return fraction.load(); }
    static bool verify (const juce::File&, juce::String& error);

private:
    std::function<void (bool, juce::String)> done;
    std::atomic<float> fraction { 0.0f };
    void run() override;
};

class Recogniser : private juce::Thread
{
public:
    Recogniser();
    ~Recogniser() override;
    static bool available();                   // false when the plug-in was built without speech
    // Transcribes on the background thread; onText runs on the message thread with the text, or an empty text and the reason.
    void transcribe (std::vector<float> samples16k, std::function<void (juce::String text, juce::String error)> onText);
    bool isBusy() const { return isThreadRunning(); }

private:
    whisper_context* ctx = nullptr;
    std::vector<float> pending;
    std::function<void (juce::String, juce::String)> callback;
    void run() override;
};
}
