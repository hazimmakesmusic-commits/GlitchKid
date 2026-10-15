# Changelog

Format: newest first. Version 0.1.0 is the CMake project version; nothing has been released to the public.

## Unreleased
- Stage 4.4: **FORMANT** is now in the plug-in: one new parameter, `formant_shift` (-12..+12 semitones, default 0 = off; 42 parameters in total), a FORMANT knob in
  the window's row (now "PITCH TUNE + FORMANT"; the note read-out moved under the ON switch). It changes the voice's size/character (negative = bigger and darker, positive =
  smaller and brighter) without changing the notes, works with or without Pitch Tune, and follows it in the chain. Off (0) is bit-exact and adds no latency (6 samples
  as before); away from 0 it adds 21-23 ms (reported to the host; the dry path, Mix and Bypass follow; 20 ms crossfade when switched; no dip on engage), about 53 ms with
  Pitch Tune as well. About 2.5-3 % of one core while shifting. The plug-in's output equals the standalone module's, delayed by 6 samples, bit for bit. Alter Egos never
  touch it. 11 new test groups (178 in total). The module moved from `Source/Experimental/FormantShifting/` to `Source/DSP/FormantShifting/`.
- Stage 4.3: formant shifting feasibility prototype: `FormantShifter` moves the spectral
  envelope (the formants) of a mono or stereo voice by -12..+12 semitones while the harmonics, and so the pitch, stay put (STFT cepstral envelope warping with a
  real gain: no phase change). No new dependency, no production source changed; an optional `FormantLab` command-line harness
  (`-DGLITCHKID_BUILD_FORMANTLAB=ON`) and 16 new test groups (167 total). Measured on synthetic vowels only: fundamental within 0.09 cents, latency a constant
  21-23 ms, about 2.5 % of one core, level held within 1.7 dB, zero allocations. Weaker for very high voices (about 300 Hz and up) and extreme shifts; the owner's listening test on one real vocal (+3, -3, neutral) passed. See `docs/FORMANT_SHIFTING_FEASIBILITY.md`.
- Stage 4.2: **Pitch Tune** is now in the plug-in: vocal pitch correction for one voice, first in the chain (before the EQ). Five new parameters, default off
  (`tune_on`, `tune_key`, `tune_scale`, `tune_strength`, `tune_speed`; 41 in total), a Pitch Tune row in the window above Liquid Echo with a live
  `SUNG A3 +23c -> A3` read-out (window 1060x980, opens at 74%). While on it adds a constant 31.3 ms at 48 kHz (reported to the host; the dry path, Mix and
  Bypass follow it; switching crossfades over 20 ms with no gap); while off it adds nothing (still 6 samples, bit-exact) and only listens (about 1 % of a
  core). Stereo and mono. Alter Egos never touch it. 14 new test groups (151 in total), mutation-checked, ASan/UBSan clean, pluginval SUCCESS. The owner
  reported it working in FL Studio on the Mac (not independently measured). No new dependency. See `docs/ARCHITECTURE.md` (Pitch Tune) and `docs/PITCH_CORRECTION_FEASIBILITY.md`.
- Stage 4.1: pitch-correction feasibility prototype: YIN detector, key/scale quantiser, TD-PSOLA-style shifter and corrector (now `Source/DSP/PitchCorrection/`), an
  optional `PitchLab` command-line harness (`-DGLITCHKID_BUILD_PITCHLAB=ON`) and 14 test groups. Measured on synthetic voices: 0 octave errors, median
  error under 2.5 cents, 31 ms latency, about 2 % of one core; the owner's listening test on one real recording sounded natural.
- Stage 3.4: **MOODS 2.0**: five Alter Egos (CELESTIAL, THE CLONE, VOIDWALKER, THE PARASITE, ALTERED STATE; they replace the first five Moods) and a Mood
  Engine: **CHAOS** (intensity of the selected Alter Ego, safe limits per parameter), **BLEND** (the existing main Mix, now in the Mood Engine strip) and
  **SURPRISE ME** (seeded, bounded variation of a short allowlist). They set the EQ, compressor, saturation, Liquid Echo and Dimension through the normal
  parameter system; Output Gain, Bypass, Blend and Stutter are never touched. No new DSP, no new automatable parameters; the selection is saved as three
  state properties (older sessions load unchanged). MORPH deferred. 16 new test groups (123 total). See `docs/ARCHITECTURE.md`.
- Editor: the window is now resizable (50% to 150%, fixed aspect ratio) and opens at 85%, because the 780 px design height did not fit a laptop screen in FL Studio. Layout and audio unchanged.
- Stage 3.3: **Dimension**, a stereo thickening/widening effect for vocals (two modulated chorus voices split into mid/side; width,
  detune, depth, mix) after Liquid Echo. 5 new parameters (`dim_*`), default off; no latency added; editor gains a Dimension row
  (window 1060x780). 15 new test groups (107 total). Mono sum equals the dry signal at Width 100 %. See `docs/ARCHITECTURE.md`.
- Stage 3.2: **Liquid Echo**, a tempo-aware stereo delay (on/off, sync or free time, 11 note divisions, feedback to 90 %, tone, width, own mix)
  after Glitch Stutter. 8 new parameters (`echo_*`), default off; no latency added; editor gains a Liquid Echo row (window 1060x640).
  15 new test groups (92 total). See `docs/ARCHITECTURE.md`.
- Fixed: a NaN or Inf sample could permanently silence the EQ and saturation (their filter memory kept the bad
  value). Both stages now replace non-finite input with 0 and clear their filter memory if it overflows. New
  tests (`Tests/RobustnessTests.cpp`, 6 groups). See `docs/ARCHITECTURE.md`.
- Build: JUCE is now pinned to commit `3fb5cbc0340758596dac39f7c8de47d562b4896e` (release 9.0.4) instead of the tag.
- Docs: refreshed `docs/STAGE0_PLAN.md`; added `docs/ARCHITECTURE.md`, `docs/BUILDING.md`, `docs/TESTING.md`,
  `docs/ROADMAP.md` and this file.

## Stage 3.1 — Glitch Stutter
Beat repeat of the slice before switch-on; rates 1/4 to 1/32; host tempo (fallback 120 BPM, shown in the window); own Mix.

## Stage 2.5 — Mix
Dry/processed crossfade using the latency-aligned dry copy.

## Stage 2.4 — Saturation
tanh waveshaper, Drive 0..18 dB, 4x oversampled, constant 6-sample reported latency.

## Stage 2.3 — Compressor
Feed-forward, soft knee, gain-reduction meter.

## Stage 2.2 — EQ
4-band parametric EQ; bypass crossfade.

## Stage 2.1 and fixes
Peak meters, unit tests, channel safety. Fixed gain "0 dB" not being exactly unity on Apple Silicon; fixed editor member shadowing.

## Stage 1A — Foundation
Minimal JUCE/CMake VST3 with gain and bypass; loaded in FL Studio on Apple Silicon.

## Stage 0
Environment, licence findings and plan (`docs/STAGE0_PLAN.md`).
