# AI vocal mix: audit and plan (verified against the source, not the old plans)

## Audit (branch `claude/glitchkid-plugin-architecture-3o6w74`)
- **Built and tested (Linux unit tests):** gain/bypass, 4-band EQ, compressor, saturation (4x oversampled), Glitch Stutter, Liquid Echo, Dimension, Pitch Tune (about 31 ms latency), Formant (about 21 ms), Harmony (fixed interval, not key-aware), Doubler, Vocal Mutation, Alter Egos, user presets + A/B, 50 artist-inspired factory presets with a gallery browser, `VocalAnalyzer` (lead only) and the ASTRO-MODE rules engine (`automode::compute`), macros, three-step interface (TUNE / AUTO / MODIFY).
- **Not built:** reverb (Echo + Dimension + Doubler stand in), key detection (the key is chosen by hand), ad-lib separation, natural-language control, preference learning, CI, Windows build, signing.
- **Buses before this change:** one stereo/mono input, one output, no sidechain. That meant the plug-in could never hear the beat.
- **Never tested:** FL Studio on Windows; macOS only by the owner by hand. ASan/pluginval not re-run since the UI stages.
- **Thread safety:** the analyser runs on the audio thread without allocation or locks and publishes through atomics (tested).

## Dependency-ordered plan
1. **Audit + foundation (this document).**
2. **Source access + analysis (done in this stage, see below).**
3. Three validated presets as the core set (CLEAN START, MELODY MODE, IN YOUR FACE) chosen from the existing 50, with real-vocal listening.
4. GlitchKid Brain: rank the presets from the lead + beat measurements, with confidence and plain-English reasons (deterministic rules, using `SourceRelationship`).
5. AutoMix workflow: propose, audition, accept, intensity, per-module bypass, undo (reuses `applyAutoMode`, `PresetController`, the undo already in the gallery).
6. Lead / ad-lib integration: needs a second instance or a documented routing; not possible from one input.
7. Core library of about 12 references; 8. interface pass; 9. constrained natural-language controls; 10. local preference scoring and release readiness.
The 5-7 minute target is NOT measured and is not claimed.

## Milestone 2: what was built
- An optional second input bus "Instrumental" (off by default; disabled/mono/stereo accepted). It is only measured: never mixed into the output (proved bit-identical on stereo and mono main buses).
- `getInstrumentalStatus()`: not connected / connected but silent / live. `startSourceAnalysis()` starts the lead and instrumental analysers together.
- `automode::compare()` (`Source/AutoMode/SourceRelationship.*`): per-band masking risk of the beat against the voice, level difference, low-end density, headroom, confidence and plain-English findings. Estimates from five coarse bands.
- AUTO shows the source status line and, after ANALYZE, the first beat finding or why the beat was not analysed.
- Tests: `Tests/SourceTests.cpp` (layouts, isolation, status, analysis on synthetic lead/beat, relationship maths, NaN/Inf on the aux input, no heap allocation).
- Limits: synthetic signals only; **no FL Studio / macOS / Windows test of the sidechain input yet**; ad-libs not available. See `docs/SOURCES.md`.
