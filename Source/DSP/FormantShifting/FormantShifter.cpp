#include "FormantShifter.h"
#include <algorithm>
#include <cmath>

namespace glitchkid::formant
{
namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr float insane = 1.0e6f;              // |x| at or above this (or NaN/Inf) counts as silence
constexpr float neutralSemitones = 1.0e-4f;   // below this the shift is treated as exactly neutral
constexpr float maxGainNepers = 3.4538776f;   // +-30 dB limit on the envelope correction (3 x ln 10)
constexpr float silentPower = 1.0e-12f;       // mean bin power below which a frame is left alone
constexpr double minLifterSeconds = 0.001;    // shortest quefrency cut-off of the envelope smoother
constexpr double maxLifterSeconds = 0.006;    // longest
constexpr double lifterOfPeriodLow = 0.9;     // cut-off as a fraction of the pitch period found in the cepstrum, for low voices ...
constexpr double lifterOfPeriodHigh = 0.5;    // ... and for high voices (fewer harmonics per formant: the envelope must be smoother)
constexpr double lowVoiceHz = 130.0, highVoiceHz = 220.0; // the fraction moves linearly between these pitches
constexpr float gainSmooth = 0.2f;            // per-frame smoothing of the envelope correction (no hop-rate modulation)
constexpr double lowestPitchHz = 65.0, highestPitchHz = 700.0; // search range of that cepstral peak
constexpr int trueEnvelopePasses = 3;         // refinements that pull the envelope up to the harmonic peaks
constexpr double smoothSeconds = 0.03;        // time constant of the shift smoothing
constexpr float levelSmooth = 0.3f;           // per-frame smoothing of the loudness compensation
constexpr float maxLevelNepers = 1.3862944f;  // +-12 dB limit on that compensation
}

int FormantShifter::frameSizeFor (double sr)
{
    int size = 256;
    while ((double) size < sr * 0.021 && size < 8192)
        size *= 2;
    return size;
}

void FormantShifter::Fft::prepare (int sz)
{
    size = sz;
    rev.assign ((size_t) sz, 0);
    int bits = 0;
    while ((1 << bits) < sz)
        ++bits;
    for (int i = 0; i < sz; ++i)
    {
        int r = 0;
        for (int b = 0; b < bits; ++b)
            if (i & (1 << b))
                r |= 1 << (bits - 1 - b);
        rev[(size_t) i] = r;
    }
    cosT.assign ((size_t) sz / 2, 0.0f);
    sinT.assign ((size_t) sz / 2, 0.0f);
    for (int k = 0; k < sz / 2; ++k)
    {
        cosT[(size_t) k] = (float) std::cos (2.0 * pi * k / sz);
        sinT[(size_t) k] = (float) std::sin (2.0 * pi * k / sz);
    }
}

void FormantShifter::Fft::transform (float* re, float* im, bool inverse) const noexcept
{
    for (int i = 0; i < size; ++i)
    {
        const int j = rev[(size_t) i];
        if (j > i)
        {
            std::swap (re[i], re[j]);
            std::swap (im[i], im[j]);
        }
    }
    const float sign = inverse ? 1.0f : -1.0f;
    for (int len = 2; len <= size; len <<= 1)
    {
        const int half = len >> 1, step = size / len;
        for (int i = 0; i < size; i += len)
            for (int k = 0; k < half; ++k)
            {
                const float wr = cosT[(size_t) (k * step)], wi = sign * sinT[(size_t) (k * step)];
                const int a = i + k, b = a + half;
                const float tr = re[b] * wr - im[b] * wi, ti = re[b] * wi + im[b] * wr;
                re[b] = re[a] - tr;
                im[b] = im[a] - ti;
                re[a] += tr;
                im[a] += ti;
            }
    }
}

void FormantShifter::prepare (double sr)
{
    sampleRate = sr;
    frameSize = frameSizeFor (sr);
    hop = frameSize / 4;
    bins = frameSize / 2 + 1;
    fft.prepare (frameSize);

    window.assign ((size_t) frameSize, 0.0f);
    for (int i = 0; i < frameSize; ++i) // periodic sqrt-Hann: analysis x synthesis = Hann, which sums to 2 at 75 % overlap
        window[(size_t) i] = (float) std::sqrt (0.5 - 0.5 * std::cos (2.0 * pi * i / frameSize));

    lifterLength = std::clamp ((int) std::lround (minLifterSeconds * sr), 8, frameSize / 4);

    for (auto& c : ch)
    {
        c.in.assign ((size_t) frameSize, 0.0f);
        c.out.assign ((size_t) frameSize, 0.0f);
        c.re.assign ((size_t) frameSize, 0.0f);
        c.im.assign ((size_t) frameSize, 0.0f);
    }
    cepRe.assign ((size_t) frameSize, 0.0f);
    cepIm.assign ((size_t) frameSize, 0.0f);
    logMag.assign ((size_t) bins, 0.0f);
    smoothed.assign ((size_t) bins, 0.0f);
    trueEnv.assign ((size_t) bins, 0.0f);
    binPower.assign ((size_t) bins, 0.0f);
    gain.assign ((size_t) bins, 1.0f);
    prevLogGain.assign ((size_t) bins, 0.0f);

    smoothCoef = 1.0f - (float) std::exp (-(double) hop / (smoothSeconds * sr));
    reset();
}

void FormantShifter::reset()
{
    for (auto& c : ch)
    {
        std::fill (c.in.begin(), c.in.end(), 0.0f);
        std::fill (c.out.begin(), c.out.end(), 0.0f);
    }
    n = 0;
    activeChannels = 0;
    levelNow = 0.0f;
    std::fill (prevLogGain.begin(), prevLogGain.end(), 0.0f);
    shiftNow = target.load (std::memory_order_relaxed);
}

void FormantShifter::setShiftSemitones (float semitones)
{
    target.store (std::isfinite (semitones) ? std::clamp (semitones, minShiftSemitones, maxShiftSemitones) : 0.0f,
                  std::memory_order_relaxed);
}

size_t FormantShifter::getMemoryBytes() const
{
    size_t f = window.size() + trueEnv.size() + binPower.size() + prevLogGain.size() + cepRe.size() + cepIm.size() + logMag.size() + smoothed.size() + gain.size()
               + fft.cosT.size() + fft.sinT.size();
    for (auto& c : ch)
        f += c.in.size() + c.out.size() + c.re.size() + c.im.size();
    return f * sizeof (float) + fft.rev.size() * sizeof (int);
}

void FormantShifter::process (float* const* channels, int numChannels, int numSamples)
{
    const int nch = std::clamp (numChannels, 1, maxChannels);
    if (activeChannels == 0)
        activeChannels = nch;
    const int used = std::min (nch, activeChannels);
    const int mask = frameSize - 1;
    const int64_t latency = frameSize - 1;

    for (int i = 0; i < numSamples; ++i)
    {
        for (int c = 0; c < used; ++c)
        {
            float v = channels[c][i];
            if (! (std::abs (v) < insane)) // false for NaN and +-Inf too
                v = 0.0f;
            ch[c].in[(size_t) (n & mask)] = v;
        }
        ++n;
        if (n >= frameSize && n % hop == 0)
            processFrame (n - frameSize);

        const int64_t t = n - 1 - latency; // the output sample that is final now
        for (int c = 0; c < used; ++c)
        {
            float y = 0.0f;
            if (t >= 0)
            {
                float& slot = ch[c].out[(size_t) (t & mask)];
                y = slot;
                slot = 0.0f;
            }
            channels[c][i] = std::abs (y) < insane ? y : 0.0f;
        }
    }
}

void FormantShifter::processBlock (const float* in, float* out, int numSamples)
{
    if (in != out)
        std::copy (in, in + numSamples, out);
    float* c[1] = { out };
    process (c, 1, numSamples);
}

void FormantShifter::processFrame (int64_t start)
{
    const int mask = frameSize - 1;
    const int used = activeChannels;

    const float wanted = target.load (std::memory_order_relaxed);
    shiftNow += (wanted - shiftNow) * smoothCoef;
    const bool neutral = std::abs (shiftNow) < neutralSemitones && std::abs (wanted) < neutralSemitones;
    if (neutral)
    {
        shiftNow = 0.0f;
        levelNow = 0.0f;
        std::fill (prevLogGain.begin(), prevLogGain.end(), 0.0f);
    }

    // Neutral: the overlap-add of the windowed frames is the input itself, so skip the transforms.
    if (neutral)
    {
        for (int c = 0; c < used; ++c)
            for (int i = 0; i < frameSize; ++i)
                ch[c].out[(size_t) ((start + i) & mask)] += 0.5f * window[(size_t) i] * window[(size_t) i]
                                                            * ch[c].in[(size_t) ((start + i) & mask)];
        return;
    }

    // 1. Windowed spectra of every channel.
    float power = 0.0f;
    for (int c = 0; c < used; ++c)
    {
        for (int i = 0; i < frameSize; ++i)
        {
            ch[c].re[(size_t) i] = ch[c].in[(size_t) ((start + i) & mask)] * window[(size_t) i];
            ch[c].im[(size_t) i] = 0.0f;
        }
        fft.transform (ch[c].re.data(), ch[c].im.data(), false);
    }

    // 2. Log magnitude of the channel-average power spectrum (one envelope for all channels).
    for (int k = 0; k < bins; ++k)
    {
        float p = 0.0f;
        for (int c = 0; c < used; ++c)
            p += ch[c].re[(size_t) k] * ch[c].re[(size_t) k] + ch[c].im[(size_t) k] * ch[c].im[(size_t) k];
        p /= (float) used;
        binPower[(size_t) k] = p;
        power += p;
        logMag[(size_t) k] = 0.5f * std::log (p + 1.0e-20f);
    }

    const bool silent = power / (float) bins < silentPower;
    if (silent)
    {
        std::fill (gain.begin(), gain.end(), 1.0f);
        std::fill (prevLogGain.begin(), prevLogGain.end(), 0.0f);
        levelNow = 0.0f;
    }
    else
    {
        // 3. Cepstrum of the log spectrum. Its peak at the pitch period tells where the harmonic ripple lives, so the
        //    smoother can keep as much formant detail as possible without letting the ripple through.
        const float inv = 1.0f / (float) frameSize;
        for (int k = 0; k < frameSize; ++k)
        {
            cepRe[(size_t) k] = logMag[(size_t) (k < bins ? k : frameSize - k)];
            cepIm[(size_t) k] = 0.0f;
        }
        fft.transform (cepRe.data(), cepIm.data(), true);
        const int qLow = std::max (8, (int) (sampleRate / highestPitchHz));
        const int qHigh = std::min (frameSize / 2 - 1, (int) (sampleRate / lowestPitchHz));
        int peakQ = qLow;
        for (int q = qLow; q <= qHigh; ++q)
            if (cepRe[(size_t) q] > cepRe[(size_t) peakQ])
                peakQ = q;
        const double pitchHz = sampleRate / peakQ;
        const double fraction = std::clamp (lifterOfPeriodLow - (lifterOfPeriodLow - lifterOfPeriodHigh) * (pitchHz - lowVoiceHz) / (highVoiceHz - lowVoiceHz),
                                            lifterOfPeriodHigh, lifterOfPeriodLow);
        const int cutoff = std::clamp ((int) (fraction * peakQ), lifterLength,
                                       std::min (frameSize / 2 - 1, (int) (maxLifterSeconds * sampleRate)));

        // Low-pass lifter (raised-cosine taper over its last quarter): log spectrum in, smooth log spectrum out.
        auto smooth = [&] (const float* logSpectrum, float* out)
        {
            for (int k = 0; k < frameSize; ++k)
            {
                cepRe[(size_t) k] = logSpectrum[(size_t) (k < bins ? k : frameSize - k)];
                cepIm[(size_t) k] = 0.0f;
            }
            fft.transform (cepRe.data(), cepIm.data(), true);
            const int taperStart = cutoff * 3 / 4;
            for (int k = 0; k < frameSize; ++k)
            {
                const int q = k < bins ? k : frameSize - k;
                float w = 0.0f;
                if (q < taperStart)
                    w = 1.0f;
                else if (q < cutoff)
                    w = (float) (0.5 + 0.5 * std::cos (pi * (q - taperStart + 1) / (double) (cutoff - taperStart + 1)));
                cepRe[(size_t) k] *= inv * w;
                cepIm[(size_t) k] = 0.0f;
            }
            fft.transform (cepRe.data(), cepIm.data(), false);
            for (int k = 0; k < bins; ++k)
                out[(size_t) k] = cepRe[(size_t) k];
        };

        // "True envelope": repeatedly smooth max(spectrum, envelope), so the envelope rides the harmonic peaks
        // instead of the average of peaks and valleys.
        smooth (logMag.data(), smoothed.data());
        for (int pass = 0; pass < trueEnvelopePasses; ++pass)
        {
            for (int k = 0; k < bins; ++k)
                trueEnv[(size_t) k] = std::max (logMag[(size_t) k], smoothed[(size_t) k]);
            smooth (trueEnv.data(), smoothed.data());
        }

        // 4. Warp the envelope in frequency (formant ratio) and turn the difference into a real gain.
        const float ratio = std::exp2 (shiftNow / 12.0f);
        for (int k = 0; k < bins; ++k)
        {
            const float src = (float) k / ratio; // where this bin's new envelope value comes from in the old envelope
            float warped;
            if (src >= (float) (bins - 1))
                warped = smoothed[(size_t) (bins - 1)];
            else
            {
                const int i0 = (int) src;
                const float f = src - (float) i0;
                warped = smoothed[(size_t) i0] + f * (smoothed[(size_t) i0 + 1] - smoothed[(size_t) i0]);
            }
            const float want = std::clamp (warped - smoothed[(size_t) k], -maxGainNepers, maxGainNepers);
            const float d = prevLogGain[(size_t) k] + (want - prevLogGain[(size_t) k]) * gainSmooth;
            prevLogGain[(size_t) k] = d;
            gain[(size_t) k] = std::exp (d);
        }

        // 5. Hold the overall loudness: a formant shift moves energy into louder or quieter parts of the spectrum.
        float before = 0.0f, after = 0.0f;
        for (int k = 0; k < bins; ++k)
        {
            before += binPower[(size_t) k];
            after += binPower[(size_t) k] * gain[(size_t) k] * gain[(size_t) k];
        }
        const float wantNepers = std::clamp (0.5f * std::log ((before + 1.0e-20f) / (after + 1.0e-20f)), -maxLevelNepers, maxLevelNepers);
        levelNow += (wantNepers - levelNow) * levelSmooth;
        const float level = std::exp (levelNow);
        for (int k = 0; k < bins; ++k)
            gain[(size_t) k] *= level;
    }

    // 6. Apply the gain, back to the time domain, window again and overlap-add.
    for (int c = 0; c < used; ++c)
    {
        for (int k = 0; k < frameSize; ++k)
        {
            const float g = gain[(size_t) (k < bins ? k : frameSize - k)];
            ch[c].re[(size_t) k] *= g;
            ch[c].im[(size_t) k] *= g;
        }
        fft.transform (ch[c].re.data(), ch[c].im.data(), true);
        const float scale = 0.5f / (float) frameSize; // 1/N for the inverse transform, 1/2 for the Hann overlap sum
        for (int i = 0; i < frameSize; ++i)
            ch[c].out[(size_t) ((start + i) & mask)] += ch[c].re[(size_t) i] * window[(size_t) i] * scale;
    }
}
} // namespace glitchkid::formant
