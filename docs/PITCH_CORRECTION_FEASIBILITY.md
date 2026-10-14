# Pitch correction: feasibility study (Stage 4.1)

**Status: the feasibility question is answered (see below) and the module is now part of the plug-in as Pitch Tune (Stage 4.2, see `ARCHITECTURE.md`).** This document keeps the Stage 4.1 findings and measurements; the code moved from `Source/Experimental/PitchCorrection/` to `Source/DSP/PitchCorrection/` and `PitchCorrectorPrototype` became `PitchCorrector` (now mono or stereo). `PitchLab` still builds from the same module.

## Question
Can GlitchKid deliver musical, low-latency vocal tuning for ₹0 upfront, with no paid or hosted dependency?

## Answer so far
**Technically feasible, quality not yet judged.** A mono YIN detector + scale quantiser + TD-PSOLA-style shifter, all written in-house, tunes synthetic voices accurately at about 2 % of one CPU core. Whether it sounds *good* on a real voice is **unknown**: nothing here has been listened to. Latency (about 31 ms) is the main product risk, see below.

## What was built (`Source/DSP/PitchCorrection/`, first in `Source/Experimental/`)
| File | Role |
|---|---|
| `YinPitchDetector` | YIN F0 estimator. Input averaged down to ~24 kHz, 5 ms hop, window ~2 x the longest period. Outputs frequency, confidence (1 - normalised difference), voiced flag, period. Silence gate at about -70 dBFS RMS. |
| `ScaleQuantizer` | Nearest note in a key (C..B) and scale (chromatic, major, natural/harmonic minor, major/minor pentatonic); target hysteresis of 0.15 semitone so a singer between two notes does not flip the target. |
| `PsolaPitchShifter` | Streaming pitch-synchronous overlap-add: Hann grains two periods wide, one detected period apart, laid down at period / ratio spacing; windows are normalised by their sum; fractional positions use cubic interpolation. Ratio limited to 0.5..2. Constant latency whatever the pitch. |
| `PitchCorrector` | Ties them together: voiced check (YIN voiced and confidence >= 0.85), 3-point median of the period, hold of 60 ms when the voice drops out, strength (0..1 of the distance to the target), retune speed (one-pole glide of the correction, 0 = instant), dry/wet mix, bit-exact bypass. One sample at a time, so the result cannot depend on the host block size. |
| `Tools/PitchLab/main.cpp` | `--report` (measurements), `--demo dir` (writes an out-of-tune synthetic line and its corrected version), or `in.wav out.wav [key scale strength retuneMs mix]` to tune a dry recording. |

Real-time safety: all buffers are allocated in `prepare()`; `process()` does no allocation, locking, logging or file I/O (no `std::vector` growth, no `new`). Non-finite samples and anything with magnitude >= 1e6 are treated as 0 before they reach any state (same policy as the production stages); a test proves the result is bit-identical to feeding zeros.

## Why this algorithm
- **YIN** (de Cheveigne and Kawahara, 2002): well documented, no training data, handles octave errors better than plain autocorrelation (the absolute threshold picks the *first* dip), costs about 10 M multiply-adds per second here.
- **TD-PSOLA-style shifting:** monophonic voice is exactly its use case, cheap, and keeps the vocal's own spectral envelope (no "chipmunk" formant shift for small corrections, which are the normal case). A phase vocoder smears transients and needs FFTs; granular two-tap delay lines were rejected after reasoning about comb filtering at the splice.
- **Known quality limit:** grains are one period apart but not aligned to glottal pulses (no pitch-mark search). Large shifts (more than about 4 semitones) are expected to sound rougher. Pulse alignment is the first upgrade if listening tests say "grainy". Formant control is deliberately out of scope for this stage.

## Dependencies and licences
**None added.** Candidates were checked from their authoritative licence files:
| Candidate | Licence | Verdict |
|---|---|---|
| Signalsmith Stretch | MIT | usable, not needed (time-stretch/pitch library, polyphonic-oriented) |
| sevagh/pitch-detection (YIN/pYIN/MPM) | MIT | usable, not needed |
| Rubber Band | GPL-2 (commercial licence sold separately) | **not usable** in a closed-source product without buying the commercial licence |
| aubio | GPL-3 | **not usable** in a closed-source product |
| SoundTouch | licence file could not be fetched | **not verified**; commonly cited as LGPL, treat as unverified until read |
The in-house code is original, written from the published YIN paper and the textbook PSOLA description; no third-party source was copied.

## Measurements (Linux x86-64, GCC Release, synthetic voices only)
- **Detector accuracy:** 9 pitches E2 (82 Hz) to G5 (784 Hz) at 44.1, 48 and 96 kHz (27 cases): **0 octave/harmonic errors**, worst median error **2.3 cents**, worst single frame under 15 cents (test limit), voiced on at least 95 % of frames.
- **Vibrato** (+-40 cents at 5.5 Hz): 0 of 160 frames more than 35 cents off. **Pitch jump** 196 -> 311 Hz: first correct estimate **30 ms** after the jump, no wrong frames afterwards.
- **Noise and silence:** white noise 0 of ~400 frames voiced at both rates; silence and a -100 dBFS tone never voiced.
- **Shifter:** 3 pitches x 7 intervals (-12..+12 semitones) x 2 rates: output frequency within 8 cents of the target (independent autocorrelation measurement). With ratio 1 and a whole-number period the output equals the input delayed by the latency to within 3e-8. Level changes follow the spectral envelope (about -6 dB at +12 semitones on the 1/h test voice; no gain correction is applied).
- **End to end (PitchLab report, sung line 15-45 cents off with +-25 cent vibrato, chromatic, strength 100 %):** mean distance from the nearest semitone 26.4 cents in; **3.8 cents** out at retune speed 0 ms, **12.5** at 25 ms, **21.1** at 100 ms (the vibrato is only partly removed at slower speeds, as intended). Measured with the same detector, so a consistency check, not an independent one.
- **Latency:** **1504 samples = 31.3 ms at 48 kHz** (1383 at 44.1, 3004 at 96 kHz), constant, set by the lowest supported pitch (80 Hz): latency = 2.5 x longest period + 4. It covers the whole path (the detector looks at older audio than it needs, so it adds nothing on top). Raising the lowest pitch to 120 Hz would cut it to about 21 ms but would not track low male voices.
- **CPU:** 10 s of audio in 160-216 ms = **1.6 % (44.1 kHz) to 2.2 % (96 kHz) of one core**, single voice, Release, block 512. **Memory:** 59 KiB per voice at 48 kHz (114 KiB at 96 kHz), fixed.
- **Not measured:** Apple Silicon speed, FL Studio behaviour, any real voice.

## Tests (`Tests/PitchCorrectionTests.cpp`, 14 groups, 137 in total)
Accuracy and octave errors across range and rates; vibrato; silence/noise/tiny signals; abrupt jump; NaN/+-Inf/huge samples (bit-identical to zeros, immediate recovery); every scale's exact note list in two keys plus nearest-note and hysteresis cases; known intervals through the shifter; exact delayed-copy at ratio 1; corrector pulls a +35 cent note onto the note, strength 0.5/0, speed, mix 0, bit-exact bypass; block-size independence (1 to 100000 samples) and reset equals fresh; note-to-note jump bounded and settles; noise level preserved; odd settings (NaN, negative key) sanitised.
Mutation-checked (all caught): shifter ignoring the ratio, YIN reporting an octave low, detector NaN guard removed, a wrong bit in the major scale (survived the first version of the tests, which is why the exact note-list test exists), strength ignored, corrector NaN guard removed.
Also run: the whole existing suite (passes), GCC ASan + UBSan on everything (clean), 0 compiler warnings, pluginval strictness 5 on the unchanged plugin (SUCCESS).

## What this does not prove
Sound quality on a real voice (breathy, sibilant, fast runs, vibrato at the edge of a note), behaviour with backing noise or reverb in the recording, polyphonic input (not supported), and any macOS/FL Studio behaviour. A synthetic vowel is the easy case.

## Listening test (owner, on the Mac)
1. Record or export **a short, dry, mono vocal**: 20-40 s, close mic, no reverb/delay/compression, singing a simple melody with some held notes, ideally a few notes a bit flat or sharp, plus a few seconds of silence/breath at the start and a spoken sentence. WAV, 44.1 or 48 kHz. Use your own voice (nothing copyrighted).
2. `cmake -S . -B build-lab -DGLITCHKID_BUILD_PITCHLAB=ON && cmake --build build-lab --target PitchLab` (a separate folder; do not touch `build/`).
3. `PitchLab dry.wav tuned.wav <key> <scale> 1.0 20` (the program is somewhere under `build-lab/PitchLab_artefacts/`; add `-DCMAKE_BUILD_TYPE=Release` to the first command for a fast build) (key 0=C..11=B; scale 0 chromatic, 1 major, 2 natural minor ...). The output has the latency removed, so it lines up with the input. Try retune speed 0, 20, 80 and strength 0.5.
4. Listen for: warbling or "robot" on held notes, doubled or hollow tone, clicks at note changes, breaths/"s" sounds being mangled, wrong notes (octave jumps).
Note: this is a command-line check of the prototype; there is no plugin UI and nothing to load in FL yet.

## Owner listening result (Mac, one recording)
One solo dry vocal, 190.8 s at 44.1 kHz, peak 0.87, chromatic, strength 100 %, retune 0 ms and 20 ms: voice found in 55 % of the recording (average 263 Hz), average correction 23 cents, largest 57 cents. Owner's verdict: **sounds natural**; the correction was too small to hear against the original (the singer was already close), so a deliberately wrong scale (F# minor pentatonic) was used as a control and the owner confirmed the pitch change was audible ("it works"). One singer, one take, no backing: this is evidence, not proof of general quality (fast runs, breathy/low/high voices and live-monitoring latency are untested).

## Readiness
**Ready for listening tests, not for production integration.** Decision gate after listening: (a) acceptable -> productise (parameters, UI, latency reporting, stereo handling, tail/transition polish); (b) grainy -> add pitch-mark alignment, re-test; (c) latency judged too high for live monitoring -> offer it as a "print/track" effect or lower the lowest tracked pitch.
