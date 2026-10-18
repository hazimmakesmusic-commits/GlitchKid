# Changelog

Format: newest first. Version 0.1.0 is the CMake project version; nothing has been released to the public.

## Unreleased
- AutoMode Stage 1: **vocal analysis and automatic settings** (ASTRO MODE profile only). ANALYZE listens to the incoming vocal (audio thread, no allocation or locks; results cross to the window through atomics) and measures peak, RMS level, clipped samples, invalid samples, level percentiles and a dynamics indicator, five coarse band shares and a rough sibilance estimate. APPLY then runs a deterministic, documented rules engine (`Source/AutoMode/AutoMix.cpp`) once and sets existing controls only through the normal parameter calls; nothing is applied automatically, and an unusable analysis changes nothing. No key or scale detection, no pitch statistics, no new DSP. Intensity is saved with the session (`autoIntensity`, default 0.6). Tests: `Tests/AutoModeTests.cpp` (15 groups; 271 pass on Linux).
- Stage 2, Preset Intelligence: the preset bar gained `<` and `>` step buttons (Factory Default, the built-in presets, then your presets A-Z; wraps; skips a damaged user file), shows `FACTORY:` or `USER:` before the name, and SAVE on a built-in preset suggests "<name> My Version" (built-in presets themselves can never be changed or replaced). No new DSP or parameters; preset/A-B/user-save code was already in place and is unchanged apart from `PresetController::allPresetNames/loadByName/step/kindOf/variationName`. Tests: 3 new groups in `Tests/FactoryPresetTests.cpp` (256 groups pass on Linux). Loading a preset is not a preview: it replaces the live sound (press SET A first, then use A to return).
- Stage 4.10: **three built-in artist-style presets** in the preset menu (FACTORY section): TRAVIS SCOTT STYLE (ASTRO MODE: dark, spacious melodic rap), JUICE WRLD STYLE (HEARTBREAK: intimate, clear, moderate effects), CHEROMANI+ STYLE (DIGITAL ALTER EGO: hard Pitch Tune, FORMANT +3 st, Mutation, bright and wide).
  They are original starting points built only from existing controls (no new DSP, no new parameters, IDs unchanged), defined as overrides on the Factory Default sound in `Source/Presets/FactoryPresets.cpp` (names in one table), so every parameter is always set and loading is repeatable. There is no reverb in GlitchKid: Liquid Echo + Dimension + Doubler stand in for "atmosphere".
  Loading reports the Pitch Tune delay (and FORMANT delay in the third) to the host. Built-in names are reserved: a user preset cannot be saved under them. Tests: `Tests/FactoryPresetTests.cpp`. Naming real artists in a product that is sold carries trademark / implied-endorsement risk: see the report.
- Stage 4.8: **Vocal Doubler and Harmony voice.**
  - **Vocal Doubler** (`dbl_on`, `dbl_amount`, `dbl_width`, `dbl_detune`, `dbl_timing`): a SYNTHETIC double, not a second recorded take. Two copies of the lead, 0.85x and 1.25x TIMING (5-45 ms) behind it, each drifting on its own smooth random path (so the pitch wobbles
    by up to DETUNE cents, 0-25, and the pattern never repeats), each softened by an 8 kHz low-pass; WIDTH 100 % puts them in the stereo sides only, so the mono sum equals the dry input exactly. Zero latency, bit-exact when off, about +0.2 dB at the defaults
    (never more than +0.73 dB on speech-like noise, 25-100 % amount), about 0.4-0.8 % of one core. It is not Dimension: Dimension is a fixed 9/13 ms sine-wave chorus with a high-passed wet; the Doubler's timing is yours to set, its wander is aperiodic and it keeps its low end.
  - **Harmony voice** (`harm_on`, `harm_interval`, `harm_level`): ONE extra voice at a FIXED interval (nine choices from Octave DOWN (-12) to Octave UP (+12), direction in the name), level -24..0 dB. It reuses Pitch Tune's detector and PSOLA shifters through a new optional
    fixed-interval mode of `PitchCorrector` (default off: Pitch Tune is unchanged, bit for bit). NOT key- or scale-aware, ONE voice, prototype quality. It is faded out while the singer is unvoiced, never feeds itself, follows Pitch Tune/FORMANT and sits before Vocal Mutation.
    Adds the Pitch Tune delay (1383 / 1504 / 3004 samples = 31.3 ms at 44.1 / 48 / 96 kHz) only while on; reported to the host, the dry path and Mix follow. About 2-3 % of one core.
  - **MOODS:** CELESTIAL (wide airy halo), THE CLONE (layered) and ALTERED STATE (loose, unsteady) use the Doubler; VOIDWALKER and THE PARASITE only switch it off. Alter Egos own 39 values now; they never touch Harmony (it adds latency). Levels stay within 0.68 dB of the input.
  - **Window:** a "DOUBLE / HARMONY" row between Vocal Mutation and Liquid Echo (1060x1280, opens at 62 %). **Listening tool:** `DoubleLab` (`-DGLITCHKID_BUILD_DOUBLELAB=ON`) renders your dry vocal six ways.
  - 54 parameters; 26 new test groups (245 in total), 24 deliberate breaks caught (one survived at first and led to a refactor and a stronger test), ASan/UBSan clean, pluginval SUCCESS. Synthetic signals only: nothing has been auditioned on a real vocal or checked in FL Studio.
- Stage 4.6: **MOODS + Vocal Mutation, user presets and A/B comparison.**
  - **MOODS now drives Vocal Mutation.** Alter Egos own four more parameters (34 in total, `AlterEgos::numOwned`): CELESTIAL a slow UP shimmer (8-14 % MUTATION, blend 18-28 %), VOIDWALKER a DOWN shift for a hollow undertone (30-45 %, CHARACTER 8 -> 0, blend 22-38 %),
    THE PARASITE ROBOT ring modulation (45-68 %, blend 35-60 %), ALTERED STATE alien UP (25-58 %, CHARACTER 75 -> 100, blend 30-52 %). THE CLONE does not use Mutation (Dimension is its detuned second voice): it only switches Mutation OFF and leaves the other three
    values alone (new per-value `active` flag). Chaos moves the Mutation values linearly like the rest; Surprise Me does not vary Mutation. Because a dry/wet blend of two different signals is quieter, the four tables' compressor makeup was re-tuned from measurements:
    every Alter Ego at Chaos 0, 0.5 and 1 is within 0.70 dB of the input level on speech-like noise (budget 1 dB). The existing identities, names and other values are unchanged.
  - **User presets.** A preset is a small versioned XML file (`.gkpreset`, format version 1) holding every sound parameter in plain units plus the Alter Ego selection; Bypass is not part of a sound. Folder on macOS:
    `~/Library/Application Support/GlitchKid/Presets/User/`. Names are cleaned (no path separators, no hidden or reserved names, 60 characters), duplicates are compared case-insensitively and never replaced without an explicit confirmation, writes go to a temporary file first, damaged,
    foreign, empty, oversized or outdated files are reported and never applied, a missing folder is created on the first save, missing parameters in older presets take their defaults. A built-in "Factory Default" restores every parameter to its default. Everything runs on the message thread.
  - **A/B comparison.** SET A / SET B capture the whole sound, A / B recall it, A>B / B>A copy. Only those explicit actions change a slot, so switching never alters a stored sound. The slots, their active marker and the preset name are saved in the DAW session. Loading a preset keeps both snapshots and
    clears the active marker (capture the old sound into A before loading, the new one into B after).
  - **Window:** a new bar under the title (PRESET menu, SAVE, DELETE, status line, A/B buttons); window now 1060x1148, opens at 68 %.
  - **Fix found on the way:** non-finite input (NaN, +-Inf) used to travel through the dry copy that Bypass and Mix 0-99 % use and could reappear delayed; it is now zeroed once at the start of every block (finite audio untouched).
  - 19 new test groups (219 in total), 21 + 1 deliberate breaks checked (one was an equivalent mutant), ASan/UBSan clean, pluginval SUCCESS. Not yet checked on the Mac or in FL Studio. No new dependency.
- Stage 4.5: **Vocal Mutation** is now in the plug-in: a single-sideband frequency shifter / ring modulator for robotic and alien voices (not a waveshaper: it moves every
  partial by the same number of Hz, so a harmonic voice becomes inharmonic; the saturator cannot do that). Four new parameters, default off (`mut_on`, `mut_amount`,
  `mut_character`, `mut_blend`; 46 in total), a "VOCAL MUTATION" row in the window between Pitch Tune + FORMANT and Liquid Echo with a read-out of the type and the shift in Hz
  (window now 1060x1112, opens at 70%). MUTATION 0..100 % sets the shift (0..600 Hz, squared curve; 0 = bit-exact), CHARACTER sweeps DOWN (alien, dark) - ROBOT (ring modulation) - UP
  (alien, bright) at equal loudness, BLEND is dry to fully mutated. Zero latency (latency stays 6 samples, nothing to report), sits after Pitch Tune/FORMANT and before the EQ. Measured on synthetic
  signals: unwanted sideband at most -41.1 dB, level within 0.03 dB across Character, a harmonic voice stays 100 % harmonic through the saturator and drops to 0.0 % through
  Mutation, about 0.15-0.4 % of one core at 44.1/48 kHz. Alter Egos never touch it (tested). 22 new test groups (200 in total, 15 deliberate breaks all caught), ASan/UBSan clean, pluginval
  SUCCESS. Not yet auditioned on a real vocal and not yet checked in FL Studio. No new dependency. See `docs/ARCHITECTURE.md` (Vocal Mutation) and `docs/DECISIONS.md` 24.
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
