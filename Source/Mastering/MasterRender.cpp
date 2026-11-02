#include "MasterRender.h"
#include "../Analysis/ReferenceAnalyzer.h"

namespace glitchkid::master
{
juce::String suitability (float lufs, float tp, int clipped, float corr, bool stereo)
{
    auto f1 = [] (float v) { return juce::String (v, 1); };
    if (lufs <= -70.0f)
        return "Nothing to measure yet: play the mix (or load it).";
    if (clipped > 0)
        return "The mix clips (" + juce::String (clipped) + " samples at full scale). Fix it in the mix first, for example lower the master fader: clipping can't be undone.";
    if (stereo && corr < 0.0f)
        return "The channels are partly out of phase (correlation " + juce::String (corr, 2) + "): parts may vanish in mono. Check the mix first.";
    if (tp > -0.5f && lufs > -11.0f)
        return "Already loud and limited (" + f1 (lufs) + " LUFS, peaks at " + f1 (tp) + " dBTP): little room left. Master gently, or bounce the mix without its limiter.";
    if (lufs < -35.0f)
        return "Very quiet (" + f1 (lufs) + " LUFS). That's fine: Loudness brings it up.";
    return "Ready to master: " + f1 (lufs) + " LUFS, peaks at " + f1 (tp) + " dBTP (" + f1 (-tp) + " dB of headroom).";
}

namespace
{
MixReport fromMeter (const LoudnessMeter& m, const juce::String& name, double seconds, double sr, int channels)
{
    MixReport r;
    r.ok = true;
    r.fileName = name;
    r.seconds = seconds;
    r.sampleRate = sr;
    r.channels = channels;
    r.integratedLufs = m.integratedLufs();
    r.truePeakDb = m.truePeakDb();
    r.samplePeakDb = m.samplePeakDb();
    r.correlation = m.correlation();
    r.clippedSamples = m.clippedSamples();
    r.message = suitability (r.integratedLufs, r.truePeakDb, r.clippedSamples, r.correlation, channels > 1);
    return r;
}

MixReport failed (const juce::String& m) { MixReport r; r.message = m; return r; }
}

MixReport analyseMix (const juce::File& file, const std::atomic<bool>& cancel, const std::function<void (float)>& progress)
{
    if (! file.existsAsFile())
        return failed ("That file could not be found.");
    auto reader = analysis::openAudioFile (file);
    if (reader == nullptr)
        return failed ("This file could not be opened as audio. Formats that can be opened here: " + analysis::referenceFormatNames() + ".");
    if (reader->lengthInSamples <= 0 || reader->sampleRate < 8000.0 || reader->numChannels == 0)
        return failed ("The file is empty or damaged.");
    const double sr = reader->sampleRate;
    const auto total = std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (maxRenderSeconds * sr));
    const int chs = (int) std::min (2u, reader->numChannels);
    LoudnessMeter m;
    m.prepare (sr);
    juce::AudioBuffer<float> buf (chs, 1 << 15);
    for (juce::int64 pos = 0; pos < total; pos += buf.getNumSamples())
    {
        if (cancel.load())
            return failed ("Cancelled.");
        const int n = (int) std::min<juce::int64> (buf.getNumSamples(), total - pos);
        if (! reader->read (&buf, 0, n, pos, true, chs > 1))
            return failed ("The file could not be read to the end (it may be damaged).");
        m.process (buf.getReadPointer (0), chs > 1 ? buf.getReadPointer (1) : nullptr, n);
        if (progress) progress ((float) (pos + n) / (float) total);
    }
    return fromMeter (m, file.getFileName(), (double) total / sr, sr, chs);
}

RenderResult renderMaster (const juce::File& source, const juce::File& destination, const Controls& controls, const std::atomic<bool>& cancel,
                           const std::function<void (float)>& progress)
{
    RenderResult res;
    auto fail = [&res] (const juce::String& m) { res.ok = false; res.message = m; return res; };
    if (! source.existsAsFile())
        return fail ("The mix file could not be found.");
    auto reader = analysis::openAudioFile (source);
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate < 8000.0 || reader->numChannels == 0)
        return fail ("The mix could not be opened as audio.");
    if (destination == source)
        return fail ("Choose a different file for the master: the mix is never overwritten.");
    const double sr = reader->sampleRate;
    const auto total = std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (maxRenderSeconds * sr));
    const int chs = (int) std::min (2u, reader->numChannels);

    constexpr int block = 4096;
    MasteringEngine engine;
    engine.setControls (controls);
    engine.prepare (sr, block);
    const int latency = engine.latencySamples();

    juce::TemporaryFile tmp (destination); // written next to the destination, swapped in only when complete
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (tmp.getFile().createOutputStream());
    if (os == nullptr)
        return fail ("Can't write to that folder.");
    auto writer = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (sr).withNumChannels (chs).withBitsPerSample (24));
    if (writer == nullptr)
        return fail ("Can't create a WAV file there.");

    LoudnessMeter before, after;
    before.prepare (sr);
    after.prepare (sr);
    juce::AudioBuffer<float> buf (chs, block);
    juce::int64 toSkip = latency, written = 0;
    // the mix, then `latency` samples of silence to flush the chain; the first `latency` output samples are dropped, so the master lines up with the mix
    for (juce::int64 pos = 0; pos < total + latency; pos += block)
    {
        if (cancel.load())
            return fail ("Cancelled: nothing was written.");
        const int n = (int) std::min<juce::int64> (block, total + latency - pos);
        buf.clear();
        const int fromFile = (int) juce::jlimit<juce::int64> (0, n, total - pos);
        if (fromFile > 0 && ! reader->read (&buf, 0, fromFile, pos, true, chs > 1))
            return fail ("The mix could not be read to the end (it may be damaged).");
        if (fromFile > 0)
            before.process (buf.getReadPointer (0), chs > 1 ? buf.getReadPointer (1) : nullptr, fromFile);
        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), chs, n);
        engine.process (view);
        const int skip = (int) std::min<juce::int64> (toSkip, n);
        toSkip -= skip;
        const int keep = (int) std::min<juce::int64> (n - skip, total - written);
        if (keep > 0)
        {
            after.process (buf.getReadPointer (0) + skip, chs > 1 ? buf.getReadPointer (1) + skip : nullptr, keep);
            if (! writer->writeFromAudioSampleBuffer (buf, skip, keep))
                return fail ("Writing the master failed (is the disk full?).");
            written += keep;
        }
        if (progress) progress ((float) (pos + n) / (float) (total + latency));
    }
    writer.reset(); // flush and close before the swap
    if (! tmp.overwriteTargetFileWithTemporary())
        return fail ("The master could not be saved there.");
    res.ok = true;
    res.output = destination;
    res.before = fromMeter (before, source.getFileName(), (double) total / sr, sr, chs);
    res.after = fromMeter (after, destination.getFileName(), (double) written / sr, sr, chs);
    auto f1 = [] (float v) { return juce::String (v, 1); };
    res.message = "Saved " + destination.getFileName() + " (24-bit WAV, " + juce::String (sr / 1000.0, 1) + " kHz): " + f1 (res.after.integratedLufs) + " LUFS, "
                + f1 (res.after.truePeakDb) + " dBTP, from " + f1 (res.before.integratedLufs) + " LUFS. Listen to it before you release it.";
    return res;
}

bool MasterJob::start()
{
    if (busy.load())
        return false;
    stopThread (15000);
    shared->alive = true;
    cancelFlag.store (false);
    progressValue.store (0.0f);
    busy.store (true);
    startThread (juce::Thread::Priority::low);
    return true;
}

bool MasterJob::analyse (const juce::File& f, std::function<void (const MixReport&)> onDone)
{
    if (busy.load()) return false;
    source = f;
    rendering = false;
    shared->analysed = std::move (onDone);
    return start();
}

bool MasterJob::render (const juce::File& from, const juce::File& to, const Controls& c, std::function<void (const RenderResult&)> onDone)
{
    if (busy.load()) return false;
    source = from;
    destination = to;
    controls = c;
    rendering = true;
    shared->rendered = std::move (onDone);
    return start();
}

void MasterJob::stopJob()
{
    shared->alive = false;
    cancelFlag.store (true);
    stopThread (15000);
    busy.store (false);
}

void MasterJob::run()
{
    auto progress = [this] (float v) { progressValue.store (v); };
    if (rendering)
    {
        auto r = renderMaster (source, destination, controls, cancelFlag, progress);
        busy.store (false);
        if (! threadShouldExit())
            juce::MessageManager::callAsync ([sh = shared, r = std::move (r)] { if (sh->alive && sh->rendered) sh->rendered (r); });
    }
    else
    {
        auto r = analyseMix (source, cancelFlag, progress);
        busy.store (false);
        if (! threadShouldExit())
            juce::MessageManager::callAsync ([sh = shared, r = std::move (r)] { if (sh->alive && sh->analysed) sh->analysed (r); });
    }
}
}
