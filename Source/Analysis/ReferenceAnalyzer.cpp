#include "ReferenceAnalyzer.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace glitchkid::analysis
{
SpectralAccumulator::SpectralAccumulator (double sr) : sampleRate (sr)
{
    window.resize ((size_t) fftSize);
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) fftSize);
    fftData.assign ((size_t) fftSize * 2, 0.0f);
    buffer.reserve ((size_t) fftSize * 8);
    const double binHz = sampleRate / (double) fftSize;
    for (int b = 0; b < SpectralProfile::numBands; ++b)
    {
        const double centre = (double) SpectralProfile::bandCentreHz (b), lo = centre * std::pow (2.0, -1.0 / 6.0), hi = centre * std::pow (2.0, 1.0 / 6.0);
        if (hi > 0.45 * sampleRate)
            break;
        binRange[(size_t) b] = { std::max (1, (int) std::ceil (lo / binHz)), std::min (fftSize / 2 - 1, (int) std::floor (hi / binHz)) };
        if (binRange[(size_t) b].second < binRange[(size_t) b].first)
            break;
        usedBands = b + 1;
    }
}

void SpectralAccumulator::resetStream()
{
    buffer.clear();
    start = 0;
}

void SpectralAccumulator::pushSamples (const float* mono, int n)
{
    if (mono == nullptr || n <= 0)
        return;
    buffer.insert (buffer.end(), mono, mono + n);
    while (buffer.size() - start >= (size_t) fftSize && frameCount() < maxFrames)
    {
        processFrame (buffer.data() + start);
        start += (size_t) hop;
    }
    if (start >= (size_t) 65536 || frameCount() >= maxFrames)
    {
        buffer.erase (buffer.begin(), buffer.begin() + (std::ptrdiff_t) std::min (start, buffer.size()));
        start = 0;
        if (frameCount() >= maxFrames)
            buffer.clear();
    }
}

void SpectralAccumulator::processFrame (const float* frame)
{
    double sumSq = 0.0;
    for (int i = 0; i < fftSize; ++i)
    {
        const float v = std::isfinite (frame[i]) ? frame[i] : 0.0f;
        sumSq += (double) v * v;
        fftData[(size_t) i] = v * window[(size_t) i];
    }
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
    fft.performRealOnlyForwardTransform (fftData.data(), true);
    std::array<float, SpectralProfile::numBands> power {};
    for (int b = 0; b < usedBands; ++b)
    {
        double sum = 0.0;
        for (int k = binRange[(size_t) b].first; k <= binRange[(size_t) b].second; ++k)
        {
            const double re = fftData[(size_t) k * 2], im = fftData[(size_t) k * 2 + 1];
            sum += re * re + im * im;
        }
        power[(size_t) b] = (float) sum;
    }
    levelDb.push_back (10.0f * std::log10 ((float) (sumSq / fftSize) + 1.0e-12f));
    bandPower.push_back (power);
}

SpectralProfile SpectralAccumulator::finish (int sourceChannels) const
{
    SpectralProfile p;
    p.sampleRate = sampleRate;
    p.sourceChannels = sourceChannels;
    p.usedBands = usedBands;
    p.totalSeconds = framesSeconds();
    const int n = frameCount();
    if (n < 1 || usedBands < 8)
        return p;
    auto sorted = levelDb;
    std::sort (sorted.begin(), sorted.end());
    const float p90 = sorted[std::min ((size_t) n - 1, (size_t) ((double) n * 0.9))];
    const float gate = std::max (-50.0f, p90 - 30.0f);
    std::vector<int> active;
    for (int i = 0; i < n; ++i)
        if (levelDb[(size_t) i] >= gate)
            active.push_back (i);
    if (active.empty())
        return p;
    std::array<double, SpectralProfile::numBands> mean {};
    for (int i : active)
        for (int b = 0; b < usedBands; ++b)
            mean[(size_t) b] += (double) bandPower[(size_t) i][(size_t) b];
    std::array<float, SpectralProfile::numBands> db {};
    double dbMean = 0.0;
    for (int b = 0; b < usedBands; ++b)
    {
        db[(size_t) b] = 10.0f * std::log10 ((float) (mean[(size_t) b] / (double) active.size()) + 1.0e-20f);
        dbMean += (double) db[(size_t) b];
    }
    dbMean /= usedBands;
    for (int b = 0; b < usedBands; ++b)
        p.shapeDb[(size_t) b] = db[(size_t) b] - (float) dbMean;
    // how unsteady the tone is: the spread of each active frame's own (mean-removed) shape around the average shape
    double acc = 0.0;
    long count = 0;
    for (int i : active)
    {
        double fm = 0.0;
        std::array<float, SpectralProfile::numBands> fd {};
        for (int b = 0; b < usedBands; ++b)
        {
            fd[(size_t) b] = 10.0f * std::log10 (bandPower[(size_t) i][(size_t) b] + 1.0e-20f);
            fm += (double) fd[(size_t) b];
        }
        fm /= usedBands;
        for (int b = 0; b < usedBands; ++b)
        {
            const double d = (double) fd[(size_t) b] - fm - (double) p.shapeDb[(size_t) b];
            acc += d * d;
            ++count;
        }
    }
    p.variabilityDb = (float) std::sqrt (acc / (double) std::max (1L, count));
    p.activeSeconds = (float) active.size() * (float) hop / (float) sampleRate;
    p.valid = std::isfinite (p.variabilityDb);
    return p;
}

namespace
{
ReferenceResult fail (ReferenceResult::Status s, const juce::String& m)
{
    ReferenceResult r;
    r.status = s;
    r.message = m;
    return r;
}

// Reads `count` frames starting at `from` and returns the mono sum ((L+R)/2, or the single channel); false if a read fails.
bool readMono (juce::AudioFormatReader& reader, juce::int64 from, int count, juce::AudioBuffer<float>& scratch, std::vector<float>& out)
{
    const int channels = (int) std::min<unsigned> (2u, reader.numChannels);
    scratch.setSize (channels, count, false, false, true);
    if (! reader.read (&scratch, 0, count, from, true, channels > 1))
        return false;
    out.resize ((size_t) count);
    for (int i = 0; i < count; ++i)
        out[(size_t) i] = channels > 1 ? 0.5f * (scratch.getSample (0, i) + scratch.getSample (1, i)) : scratch.getSample (0, i);
    return true;
}
}

ReferenceResult analyseReferenceFile (const juce::File& file, const ReferenceSection& section, const std::atomic<bool>& cancel, const std::function<void (float)>& progress)
{
    auto report = [&] (float v) { if (progress) progress (juce::jlimit (0.0f, 1.0f, v)); };
    if (! file.existsAsFile())
        return fail (ReferenceResult::Status::missingFile, "That file could not be found.");
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
        return fail (ReferenceResult::Status::unsupported, "This file type cannot be read. Use a WAV, AIFF, FLAC or Ogg Vorbis file.");
    if (reader->lengthInSamples <= 0 || reader->numChannels == 0 || reader->sampleRate <= 0.0)
        return fail (ReferenceResult::Status::unreadable, "The file is empty or damaged.");
    if (reader->sampleRate < 32000.0)
        return fail (ReferenceResult::Status::sampleRateTooLow, "The sample rate is too low to compare tone up to 12 kHz (32 kHz or higher is needed).");

    const double sr = reader->sampleRate;
    const juce::int64 total = reader->lengthInSamples;
    ReferenceResult result;
    result.fileSeconds = (double) total / sr;
    const int block = 1 << 16;
    juce::AudioBuffer<float> scratch;
    std::vector<float> mono;

    // choose the section: the user's, or the loudest window (pass 1 reads at most the first 15 minutes and only measures each frame's energy)
    juce::int64 secStart = 0, secLen = total;
    const juce::int64 want = (juce::int64) (std::max (4.0, section.lengthSeconds) * sr);
    if (! section.automatic)
    {
        secStart = juce::jlimit<juce::int64> (0, total - 1, (juce::int64) (std::max (0.0, section.startSeconds) * sr));
        secLen = std::min (want, total - secStart);
    }
    else if (total > want)
    {
        const juce::int64 scanEnd = std::min (total, (juce::int64) (900.0 * sr));
        const int hopS = SpectralAccumulator::hop;
        std::vector<double> energy;
        energy.reserve ((size_t) (scanEnd / hopS) + 1);
        double acc = 0.0;
        int inHop = 0;
        for (juce::int64 pos = 0; pos < scanEnd; pos += block)
        {
            if (cancel.load())
                return fail (ReferenceResult::Status::cancelled, "Cancelled.");
            const int n = (int) std::min<juce::int64> (block, scanEnd - pos);
            if (! readMono (*reader, pos, n, scratch, mono))
                return fail (ReferenceResult::Status::unreadable, "The file could not be read.");
            for (float v : mono)
            {
                acc += (double) v * v;
                if (++inHop == hopS)
                {
                    energy.push_back (acc);
                    acc = 0.0;
                    inHop = 0;
                }
            }
            report (0.4f * (float) pos / (float) scanEnd);
        }
        const size_t win = std::max<size_t> (1, (size_t) (want / hopS));
        double best = -1.0, run = 0.0;
        size_t bestAt = 0;
        for (size_t i = 0; i < energy.size(); ++i)
        {
            run += energy[i];
            if (i >= win)
                run -= energy[i - win];
            if (i + 1 >= std::min (win, energy.size()) && run > best)
            {
                best = run;
                bestAt = i + 1 - std::min (win, i + 1);
            }
        }
        secStart = (juce::int64) bestAt * hopS;
        secLen = std::min (want, total - secStart);
    }
    else
        report (0.4f);

    SpectralAccumulator acc (sr);
    for (juce::int64 pos = 0; pos < secLen; pos += block)
    {
        if (cancel.load())
            return fail (ReferenceResult::Status::cancelled, "Cancelled.");
        const int n = (int) std::min<juce::int64> (block, secLen - pos);
        if (! readMono (*reader, secStart + pos, n, scratch, mono))
            return fail (ReferenceResult::Status::unreadable, "The file could not be read.");
        acc.pushSamples (mono.data(), n);
        report (0.4f + 0.6f * (float) pos / (float) std::max<juce::int64> (1, secLen));
    }
    result.sectionStartSeconds = (double) secStart / sr;
    result.sectionLengthSeconds = (double) secLen / sr;
    result.profile = acc.finish ((int) reader->numChannels);
    result.profile.fullMix = true; // a file the user supplies as a reference is a finished piece of music unless they say otherwise
    if (! result.profile.valid)
    {
        result.status = ReferenceResult::Status::silent;
        result.message = "No usable sound was found in the chosen section (it is silent or too quiet).";
        return result;
    }
    if (result.profile.activeSeconds < 4.0f)
    {
        result.status = ReferenceResult::Status::tooShort;
        result.message = "Less than 4 seconds of sound was found in the chosen section. Choose a longer or louder section.";
        return result;
    }
    result.status = ReferenceResult::Status::ok;
    result.message = "Analysed " + juce::String (result.profile.activeSeconds, 1) + " s of sound.";
    report (1.0f);
    return result;
}

bool ReferenceJob::start (const juce::File& f, const ReferenceSection& s, std::function<void (const ReferenceResult&)> onDone)
{
    if (busy.load())
        return false;
    stopThread (15000);
    file = f;
    section = s;
    shared->done = std::move (onDone);
    shared->alive = true;
    cancelFlag.store (false);
    progressValue.store (0.0f);
    busy.store (true);
    startThread (juce::Thread::Priority::low);
    return true;
}

void ReferenceJob::stopJob()
{
    shared->alive = false; // a result already posted will find this and do nothing
    cancelFlag.store (true);
    stopThread (15000);
    busy.store (false);
}

void ReferenceJob::run()
{
    auto result = analyseReferenceFile (file, section, cancelFlag, [this] (float v) { progressValue.store (v); });
    busy.store (false);
    if (threadShouldExit())
        return;
    juce::MessageManager::callAsync ([sh = shared, r = std::move (result)]
    {
        if (sh->alive && sh->done)
            sh->done (r);
    });
}
}
