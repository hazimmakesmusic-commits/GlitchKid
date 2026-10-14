# Architecture

What exists today (Stage 3.1 plus hardening). Nothing planned is described as built.

## Overview
One JUCE audio plug-in (VST3 only, macOS arm64 first) with an editor window. All processing is local and in
32-bit float; no network, no hosted service. Plug-in codes: manufacturer `GlKd`, plug-in `Gkid` (never change).

```
input -> EQ -> compressor -> saturation -> Glitch Stutter -> output gain -> Mix -> output
   \______________ dry copy, delayed by 6 samples (FixedDelay) ______________/
```

## Files
| File | Role |
|---|---|
| `Source/PluginProcessor.*` | 23 parameters (`AudioProcessorValueTreeState`), `processBlock`, state save/restore, meters, tempo lookup |
| `Source/PluginEditor.*` | Window 1060x500 (not resizable): knobs, IN/OUT meters, gain-reduction meter, tempo readout, 30 Hz timer |
| `Source/DSP/ParametricEq.*` | 4 bands (low shelf, 2 bells, high shelf), RBJ cookbook biquads in double precision |
| `Source/DSP/Compressor.*` | Feed-forward, stereo-linked peak detector, soft knee, attack/release on the gain reduction |
| `Source/DSP/Saturation.*` | `tanh(kx)/tanh(k)` waveshaper, 4x oversampled (JUCE polyphase IIR half-band) |
| `Source/DSP/GlitchStutter.*` | Beat repeat of the slice before switch-on; per-sample state machine |
| `Source/DSP/FixedDelay.h` | Constant delay used for the dry/bypass path and the idle saturation stage |
| `Source/DSP/Finite.h` | `zeroNonFinite`: replaces NaN/Inf with 0 |
| `Tests/` | Unit tests (see `TESTING.md`) |

## Per-block flow (`PluginProcessor::processChunk`)
1. Resolve host tempo once per block (play head BPM, clamped 40..300, fallback 120).
2. Copy the input to the dry buffer and delay it by `Saturation::latencySamples` (6).
3. If Bypass is on, or Mix is 0 %, and the crossfade has settled: output the exact delayed dry copy, reset the
   stages, return (almost no CPU).
4. Otherwise run EQ, compressor, saturation, stutter, smoothed gain; then crossfade with the dry copy if Mix < 100 %
   or a Bypass/Mix change is still ramping.
5. Update peak meters.
Host blocks larger than the dry buffer are split into chunks.

## Design rules
- **Off means bit-exact.** Values within half a parameter step of neutral snap to exactly neutral, and idle stages
  are skipped. (Found on the Mac: the 0.1 dB grid produced 8.9e-07 dB at "0 dB" on Apple Silicon.)
- **Constant latency, always reported:** 6 samples (0.125 ms at 48 kHz). It is the saturation oversampler's latency;
  the dry/bypass path and an idle saturation stage are delayed by the same amount.
- **Smoothing:** gain, Mix, EQ settings and drive ramp over 20 ms; EQ coefficients refresh every 32 samples while moving.
- **Real-time safe:** no allocation, locks or I/O on the audio thread; buffers are sized in `prepareToPlay`
  (measured: 0 audio-thread heap allocations over 6,000 blocks in the audit).
- **Non-finite audio** (see below).
- **Parameter IDs are permanent.** All have version hint 1 and must never be renamed:
  `gain, bypass, eq_lo_freq, eq_lo_gain, eq_b1_freq, eq_b1_gain, eq_b1_q, eq_b2_freq, eq_b2_gain, eq_b2_q,
  eq_hi_freq, eq_hi_gain, comp_thresh, comp_ratio, comp_attack, comp_release, comp_knee, comp_makeup, sat_drive,
  stutter_on, stutter_rate, stutter_mix, mix`.
- **State:** XML via `copyState`/`replaceState`; unknown or garbage state is ignored; parameters missing from an
  older save keep their defaults. The saved state has no version attribute yet.

## NaN / Inf handling (recovery design)
A recursive (IIR) filter keeps its past output in memory, so one NaN or Inf sample would stay in that memory and
corrupt every later sample, even after clean audio returns. Two stages hold such memory: the EQ biquads and the
saturation oversampler. Each now protects itself:
- **EQ:** NaN/Inf input samples are replaced with 0 before filtering (always, even when all bands are flat). If the
  filter memory ever becomes non-finite or larger than 1e30 (about 600 dB, impossible from real audio) it is zeroed,
  and any non-finite output sample is replaced with 0.
- **Saturation:** NaN/Inf input is replaced with 0 before the delay and oversampler. If any output sample is
  non-finite (overflow from an absurdly large finite input) the oversampler memory is reset and those samples become 0.
- The compressor and stutter already recover within the same block; they see finite audio from the EQ.

Trade-offs: the bad sample becomes silence (a tiny click) rather than being repaired, and a corrupted filter is
cleared (one short transient) rather than the whole plug-in being muted, so the rest of the chain and the dry path
keep working. Cost: one `isfinite` test per sample per protected stage. Not covered: the dry/bypass path and a
Mix < 100 % blend pass the host's input through unchanged, so a NaN from the host is passed through there for
those samples only; it does not poison anything. Valid audio is unaffected (the tests compare against stages that
never saw bad data, exactly).

## What does not exist
Presets, Liquid Echo, Dimension, MOODS, pitch correction, other plug-in formats, resizable UI, host-grid phase-lock
for the stutter.
