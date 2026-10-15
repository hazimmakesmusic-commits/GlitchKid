# Formant shifting: feasibility prototype (Stage 4.3)

**Status: experimental. Not in the plugin.** The production signal path, parameters, GUI, MOODS and Pitch Tune are unchanged. The prototype is compiled only into the test target (`GlitchKidTests`) and the optional `FormantLab` command-line harness. (The milestone prompt called this "Stage 4.2"; that number was already used for Pitch Tune, so it is Stage 4.3 here.)

## What formant shifting is (and is not)
The **pitch** of a voice is how fast the vocal folds vibrate (the harmonics at f0, 2 f0, 3 f0 ...). The **formants** are the resonances of the throat and mouth that shape the loudness of those harmonics: they are what make a voice sound big, small, male, female, nasal or open. Pitch shifting moves the harmonics (and drags the formants with them: the "chipmunk" effect). **Formant shifting moves the formants and leaves the harmonics alone.** Resampling, or moving every spectral component, is a pitch shift and is not what this module does.

## Question
Can GlitchKid change the perceived size/character of a vocal, without changing its notes, with no paid or hosted dependency and acceptable latency and CPU?

## Answer so far
**Technically feasible; sound quality on real voices is not yet judged.** A self-written STFT spectral-envelope warper moves the formants of synthetic vowels the right way while the fundamental stays within **0.09 cents** (worst of 60 cases), costs about **2.5 % of one core** at 44.1/48 kHz and adds a constant **21-23 ms**. It is weaker for very high voices (f0 of about 300 Hz and above) and for extreme shifts, and has never been heard on a real vocal. It is **ready for a listening test, not for integration.**

## Files (all new; nothing existing was modified except `CMakeLists.txt` and the docs)
| File | Role |
|---|---|
| `Source/Experimental/FormantShifting/FormantShifter.{h,cpp}` | The shifter: mono/stereo, per-sample, constant latency, own radix-2 FFT |
| `Tests/FormantShiftTests.cpp` | 16 test groups, including a global allocation counter |
| `Tools/FormantLab/main.cpp` | `--report`, `--demo dir`, or `in.wav out.wav semitones` |
| `CMakeLists.txt` | the module and tests added to `GlitchKidTests` only; new optional `GLITCHKID_BUILD_FORMANTLAB` target. The plugin target's source list is unchanged. |

## Approach, and why
Three methods were weighed:

| Method | Verdict |
|---|---|
| **PSOLA grain resampling** (reuse the Pitch Tune shifter: resample each pitch-synchronous grain) | Rejected. It needs a pitch period at every instant; on unvoiced sounds and "s" it falls back to a fixed period and buzzes. It would also tie formant shifting to the pitch tracker. |
| **LPC pole shifting** (fit an all-pole model, move the poles, re-filter the residual) | Rejected for now. Needs polynomial root-finding and stable time-varying filters; on high voices LPC locks onto single harmonics, giving exactly the metallic/hollow artifacts the milestone warns about. Highest risk. |
| **STFT cepstral envelope warping** (chosen) | Estimate the spectral envelope per frame, warp it in frequency, multiply the spectrum by (warped envelope / original envelope). |

**Chosen method, in detail** (`FormantShifter.cpp`):
1. STFT: frame 1024 samples at 44.1/48 kHz (2048 at 88.2/96 kHz, about 21-23 ms), hop = frame / 4, periodic sqrt-Hann windows (analysis x synthesis = Hann, which overlap-adds to exactly 2).
2. Envelope: log magnitude of the channel-average power spectrum, then **cepstral smoothing**: the cepstrum's peak at the pitch period tells where the harmonic ripple lives, and a low-pass "lifter" just below it (90 % of the period for low voices, 50 % for voices above about 220 Hz; between 1 and 6 ms) keeps the formant shape and removes the ripple. Three "true envelope" passes (smooth `max(spectrum, envelope)`) make the envelope follow the harmonic peaks rather than the average of peaks and valleys.
3. Warp: `new(f) = old(f / ratio)`, ratio = 2^(semitones/12), linear interpolation.
4. Gain: `exp(new - old)` limited to +-30 dB, smoothed from frame to frame (20 % per hop, which stops it modulating the harmonics), multiplied onto the complex spectrum as a **real** number: **no phase change, no spectral line moves**, so the pitch is preserved by construction.
5. Loudness: the overall level is held (limited to +-12 dB) because a formant shift moves energy into louder or quieter parts of the spectrum.
6. Overlap-add. Neutral (0 semitones) skips the transforms entirely and overlap-adds the windowed input, which reconstructs the input exactly (measured error 3e-8, about -140 dBFS).

**Why this suits GlitchKid:** needs no pitch detector (independent of Pitch Tune, correct on noise, breath and "s"), stereo uses ONE gain for both channels so the image cannot move, per-sample processing so the output never depends on the host's block size, no dependency, no locks (own FFT; JUCE's FFT takes a spin-lock).

**Control and range:** `setShiftSemitones(-12 .. +12)` (formant ratio 0.5 .. 2). Negative = bigger/darker, positive = smaller/brighter. Changes are smoothed over about 30 ms and the setter is thread-safe (atomic). Useful range expected to be about -6 .. +6; beyond that the artifacts below grow.

## Third-party dependencies and licences
**None added.** Existing candidates, for the record: Signalsmith Stretch (MIT, has a formant control; the licence file was read in Stage 4.1), Rubber Band (GPL-2, commercial licence needed for closed source), SoundTouch (licence file not verified). None was needed. The FFT is a 60-line radix-2 written for this module.

## Measurements (Linux x86-64, GCC Release, synthetic vowels only)
- **Latency:** frame size - 1, constant: **1023 samples = 23.2 ms at 44.1 kHz, 21.3 ms at 48 kHz; 2047 samples = 23.2 ms at 88.2 kHz, 21.3 ms at 96 kHz.** Measured by cross-correlating noise through the shifter at 0, +6 and -6 semitones (peak at the reported lag +-1). The first frame of output is the overlap-add filling up (as in any latency).
- **Neutral:** output = input delayed by the latency, worst difference **3e-8** (peak 0.3), at 7 sample rates 22.05-192 kHz, mono and stereo.
- **Fundamental preserved:** 5 pitches (100-400 Hz) x 6 shifts (-12..+12) x 2 rates = 60 cases: worst error **0.09 cents**, and never more than +0.09 cents more than the untouched voice measures (autocorrelation, independent of the module).
- **Formants really move:** three vowels with known formants (a, i, u) x f0 110/150/220/300 Hz x shifts -6/-3/+3/+6, compared with the same vowel synthesised with moved formants and bandwidths: mean harmonic-level error (mean offset removed) **6.74 dB if nothing is done, 2.94 dB with the shifter**; the spectral centroid moves like the ideal vowel's in 40 of 41 clear cases. Dense harmonics (f0 <= 150 Hz): worst 2.86 dB. At 220 Hz every case improves. At **300 Hz several cases are no better than (a few worse than) doing nothing**, because harmonics 300 Hz apart cannot show where a 60-100 Hz-wide formant sits. Any envelope method has this ambiguity.
- **Artifacts:** energy that is not on a harmonic of f0, relative to total: mean **-42.6 dB**, worst **-28.4 dB** over 90 vowel/pitch/shift cases (the clean input measures -58 dB). Before frame-to-frame gain smoothing and a pitch-dependent smoother this was as bad as -1 dB (modulation sidebands at the hop rate); the test now guards it.
- **Level:** within **1.7 dB** of the input for every shift tested (+-12 included), thanks to the loudness hold.
- **Noise and "s":** silence stays exactly silent; tiny signals (1e-9..1e-4) never exceed 8x their input; DC and a 4-9 kHz noise burst inside a vowel stay finite and under 2x the input peak; band-limited noise (5-9 kHz) keeps its level within 6 dB for shifts up to +-6 and shows no spurs stronger than twice the input's own. At +-12 semitones a band-limited "s" loses level: a shifter can only re-weight energy that exists, and there is no content to move into the vacated band.
- **Pitch jump** (150 -> 300 Hz) with a shift: both pitches preserved within 3 cents, output bounded.
- **CPU (Release, 10 s of audio, block 512, best of 3):** mono **about 2.3 % (44.1 kHz) / 2.5 % (48 kHz) / 6 % (96 kHz)** of one core, stereo about 2.6-3.1 % (44.1/48 kHz) / 6.6 % (96 kHz), neutral 0.13-0.36 % (two runs differed by up to 0.5 points: it is a noisy measurement). Not optimised (no real-FFT packing, no vDSP).
- **Memory:** 64 KiB at 44.1/48 kHz, 128 KiB at 96 kHz, fixed.
- **Real-time safety:** a global allocation counter shows **0 heap allocations** in `process()` for mono, stereo, shifting, neutral, NaN input and a changing shift. No locks, no I/O, no logging; the frame loops are bounded (fixed N). `setShiftSemitones` is lock-free from any thread (tested with a second thread hammering it).
- **Not measured:** Apple Silicon speed, FL Studio behaviour, any real voice.

## Tests (`Tests/FormantShiftTests.cpp`, 16 groups; 167 in total)
Latency (reported and measured), neutral at 7 rates, fundamental preservation, vowel formant movement against an ideal moved-formant vowel, no modulation artifacts, loudness hold, silence and very low-level input (including DC), voiced/unvoiced transitions with a noise burst, sibilant-like noise, abrupt pitch change, mono = stereo and shared-gain stereo balance, 7 sample rates x 9 block sizes (bit-identical to block size 1), parameter changes while processing (identical for any block split; click bound; a second thread calling the setter), NaN/+-Inf/huge samples (bit-identical to zeros, immediate recovery), buffer boundaries / reset / repeated processing, allocation count.
**What synthetic tests show:** the maths: harmonics stay put, the envelope moves, no modulation sidebands, no allocation, no crash on bad input. **What they cannot show:** how a real voice sounds (breathiness, sibilants, vibrato, jitter, consonants, room), and whether artifacts are acceptable to a listener. A sine wave was never used as evidence of formant shifting.
Mutation-checked (9 deliberate breaks, each caught): warp direction inverted, neutral path mis-scaled, NaN guard removed, loudness hold removed, gain smoothing removed, an allocation added to the frame, `reset()` forgetting the gain memory, output read before the overlap-add is complete, an envelope smoother that is far too short. (A tenth, removing a 60 dB spectrum floor, was *not* caught; measuring showed the floor did nothing useful, so it was deleted.)

## Known artifacts and limitations
- **High voices:** above about 250 Hz the envelope cannot be separated from the harmonics; expect weaker or wrong formant placement. (300 Hz results are bounded but can be worse than nothing.)
- **Extreme shifts** (beyond about +-6 semitones): the gain limit (+-30 dB) and the empty parts of the spectrum limit the effect; band-limited fricatives lose level.
- **Noisy frames** get a gain estimated from a noisy spectrum; it is smoothed but may sound slightly unstable ("musical noise") on breathy sounds. Unknown until listened to.
- **Frame length (21-23 ms)** smears fast consonants slightly; there is no transient handling.
- **Not pitch-adaptive in a tracked sense:** it reads the pitch from each frame's cepstrum, not from a tracker; octave jumps of that estimate only change how smooth the envelope is.
- **Latency 21-23 ms**, on top of Pitch Tune's 31 ms if both are used (about 53 ms together).
- **Startup:** the first frame of output is a fade-in.
- **Channel count** must stay constant until `reset()`.

## Listening test (owner, on the Mac)
1. Supply **a short, dry vocal recording**: your own voice, 20-40 s, close mic, no reverb/delay/compression, a mix of sustained notes, a few note changes, and spoken words with "s", "sh", "t", "k" sounds. WAV, mono or stereo, 44.1 or 48 kHz. (Do not use copyrighted recordings.)
2. Build the harness (a separate folder; do not touch `build/`):
   `cmake -S . -B build-formant -DCMAKE_BUILD_TYPE=Release -DGLITCHKID_BUILD_FORMANTLAB=ON && cmake --build build-formant --target FormantLab`
3. Try the built-in demo first (a synthetic vowel line, so you know what to listen for): `FormantLab --demo demo` then play `demo_dry.wav`, `demo_formant_down6.wav` (bigger voice), `demo_formant_up6.wav` (smaller voice). The three should have the same notes.
4. On your recording: `FormantLab dry.wav out_down3.wav -3`, `... +3`, `... -6`, `... +6`, and `... 0` (neutral). The program is somewhere under `build-formant/FormantLab_artefacts/`; the output has the latency removed, so it lines up with the input.
5. Checklist:
   - Does the **fundamental pitch stay stable** (same melody, no warble)?
   - Does the voice sound **lower/bigger or higher/smaller** without the notes changing?
   - Are **consonants and "s" sounds** still intelligible (not smeared, not lisping, not hissy)?
   - Is it **stable on sustained notes and note transitions** (no wobble, flutter or "underwater" movement)?
   - Are artifacts **acceptable at +-3** (moderate)? at +-6?
   - Does **0 (neutral)** sound identical to the original?

## Owner listening result (Mac, one recording)
The owner ran the demo and shifted one dry vocal at +3, -3 and 0 and replied "yes it works" to the checklist (notes stay, voice sounds bigger/smaller, consonants intelligible, no warble, neutral identical). The reply was a single line, so per-item detail, the shifts beyond +-3, and the voice's pitch range were not recorded. One singer, one take: evidence, not proof (breathy, high or very low voices and shifts of +-6 and beyond are untested by ear).

## Readiness
**Ready for listening tests, not for integration.** Decision gate after listening: (a) natural enough at +-3..+-5 -> integrate (parameter `formant_shift` -12..+12, off by default; place it after Pitch Tune; reported latency while on; a shared crossfade/dry-path design like Pitch Tune); (b) artifacts on breathy or high voices -> improve the envelope estimator (LPC-based, or temporal smoothing tuned on real recordings) and re-test; (c) latency judged too high -> shorter frame with lower pitch-resolution (a "low-latency" mode) or hide it behind a "print" workflow.
