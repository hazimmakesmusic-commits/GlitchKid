# Changelog

Format: newest first. Version 0.1.0 is the CMake project version; nothing has been released to the public.

## Unreleased
- Stage 3.4: **MOODS**, five built-in one-tap vocal starting points (DREAM, DEMON, ALIEN, RAGE, GHOST) that set the EQ, compressor, saturation, Liquid
  Echo and Dimension through the normal parameter system. No new DSP and no new saved state. Output Gain, Bypass, Mix and Stutter are never touched.
  The lit button follows the controls (CUSTOM once you edit). Editor gains a MOODS strip (window designed at 1060x830, opens at 80%). 10 new test groups
  (117 total). See `docs/ARCHITECTURE.md`.
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
