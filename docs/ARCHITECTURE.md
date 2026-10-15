# Architecture

What exists today (Stage 4.2: Pitch Tune, on top of MOODS 2.0, Dimension, Liquid Echo and hardening). Nothing planned is described as built.

## Overview
One JUCE audio plug-in (VST3 only, macOS arm64 first) with an editor window. All processing is local and in
32-bit float; no network, no hosted service. Plug-in codes: manufacturer `GlKd`, plug-in `Gkid` (never change).

```
input -> [Pitch Tune, optional] -> EQ -> compressor -> saturation -> Glitch Stutter -> Liquid Echo -> Dimension -> output gain -> Mix -> output
   \______ dry copy, delayed by 6 samples (+ the Pitch Tune delay while Tune is on) ______/
```

## Files
| File | Role |
|---|---|
| `Source/PluginProcessor.*` | 41 parameters (`AudioProcessorValueTreeState`), `processBlock`, state save/restore, meters, tempo lookup, Pitch Tune latency reporting |
| `Source/PluginEditor.*` | Window designed at 1060x980 and resizable (50% to 150%, fixed aspect ratio); the whole interface is laid out at the design size inside one content component and scaled to fit, default 74%: knobs, IN/OUT meters, gain-reduction meter, tempo and Pitch Tune read-outs, 30 Hz timer |
| `Source/DSP/PitchCorrection/*` | Pitch Tune: `YinPitchDetector` (F0), `ScaleQuantizer` (key/scale), `PsolaPitchShifter` (TD-PSOLA-style, one per channel), `PitchCorrector` (ties them together, mono/stereo) |
| `Source/DSP/ParametricEq.*` | 4 bands (low shelf, 2 bells, high shelf), RBJ cookbook biquads in double precision |
| `Source/DSP/Compressor.*` | Feed-forward, stereo-linked peak detector, soft knee, attack/release on the gain reduction |
| `Source/DSP/Saturation.*` | `tanh(kx)/tanh(k)` waveshaper, 4x oversampled (JUCE polyphase IIR half-band) |
| `Source/DSP/GlitchStutter.*` | Beat repeat of the slice before switch-on; per-sample state machine |
| `Source/DSP/LiquidEcho.*` | Tempo-aware stereo delay with feedback, tone, width and its own mix |
| `Source/DSP/Dimension.*` | Stereo thickening/widening: two modulated chorus voices split into mid and side |
| `Source/AlterEgos.*` | MOODS 2.0: the five Alter Ego tables, the Chaos mapping and the Surprise variation (data and maths only, no DSP) |
| `Source/DSP/FixedDelay.h` | Constant delay used for the dry/bypass path and the idle saturation stage |
| `Source/DSP/Finite.h` | `zeroNonFinite`: replaces NaN/Inf with 0 |
| `Tests/` | Unit tests (see `TESTING.md`) |

## Per-block flow (`PluginProcessor::processChunk`)
1. Resolve host tempo once per block (play head BPM, clamped 40..300, fallback 120).
2. Copy the input to the dry buffer and delay it by `Saturation::latencySamples` (6). A second copy is delayed by the Pitch Tune
   latency as well (always, so it is valid the moment Tune is switched on); while Tune is on (or ramping) the dry buffer is the
   crossfade between the two, using the 20 ms Tune blend ramp.
3. If Bypass is on, or Mix is 0 %, and the crossfade has settled: output the exact delayed dry copy, reset the
   stages, return (almost no CPU). Pitch Tune only listens (`feed`) in this state.
4. Otherwise: Pitch Tune (if on or ramping: zero non-finite input, keep a straight-through copy, correct in place, crossfade by the
   Tune blend; if off: only `feed`, i.e. record the history), then run EQ, compressor, saturation, stutter, Liquid Echo, Dimension, smoothed gain;
   then crossfade with the dry copy if Mix < 100 % or a Bypass/Mix change is still ramping.
5. Update peak meters.
Host blocks larger than the dry buffer are split into chunks.

## Design rules
- **Off means bit-exact.** Values within half a parameter step of neutral snap to exactly neutral, and idle stages
  are skipped. (Found on the Mac: the 0.1 dB grid produced 8.9e-07 dB at "0 dB" on Apple Silicon.)
- **Latency, always reported:** 6 samples (0.125 ms at 48 kHz) with Pitch Tune off. It is the saturation oversampler's latency;
  the dry/bypass path and an idle saturation stage are delayed by the same amount. With Pitch Tune on it is 6 plus
  `PitchCorrector::latencyFor(sampleRate)` (1504 samples = 31.3 ms at 48 kHz, 1383 at 44.1, 3004 at 96), reported through a
  message-thread timer (10 Hz) so the audio thread never calls the host; the dry path is delayed by the same amount.
- **Smoothing:** gain, Mix, EQ settings and drive ramp over 20 ms; EQ coefficients refresh every 32 samples while moving.
- **Real-time safe:** no allocation, locks or I/O on the audio thread; buffers are sized in `prepareToPlay`
  (measured: 0 audio-thread heap allocations over 6,000 blocks in the audit).
- **Non-finite audio** (see below).
- **Parameter IDs are permanent.** All have version hint 1 and must never be renamed:
  `gain, bypass, eq_lo_freq, eq_lo_gain, eq_b1_freq, eq_b1_gain, eq_b1_q, eq_b2_freq, eq_b2_gain, eq_b2_q,
  eq_hi_freq, eq_hi_gain, comp_thresh, comp_ratio, comp_attack, comp_release, comp_knee, comp_makeup, sat_drive,
  stutter_on, stutter_rate, stutter_mix, mix, echo_on, echo_sync, echo_time, echo_div, echo_fb, echo_tone,
  echo_width, echo_mix, dim_on, dim_width, dim_detune, dim_depth, dim_mix, tune_on, tune_key, tune_scale, tune_strength, tune_speed`.
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

## Liquid Echo
A stereo delay after Glitch Stutter. Parameters (all new, version hint 1, default OFF so a fresh instance is unchanged):

| ID | Range | Default |
|---|---|---|
| `echo_on` | on/off | off |
| `echo_sync` | free time / tempo-synced | synced |
| `echo_time` | 1 to 2000 ms (used when sync is off) | 350 ms |
| `echo_div` | 1/2, 1/4, 1/4 dotted, 1/4 triplet, 1/8, 1/8 dotted, 1/8 triplet, 1/16, 1/16 dotted, 1/16 triplet, 1/32 (used when sync is on) | 1/8 |
| `echo_fb` | 0 to 90 % | 35 % |
| `echo_tone` | 400 Hz to 20 kHz (low-pass on the repeats) | 6 kHz |
| `echo_width` | 0 to 100 % | 0 % |
| `echo_mix` | 0 to 100 % (0 = dry, 100 = repeats only) | 30 % |

How it works: each channel has a circular delay line allocated in `prepare()` for 3 s x 1.5 (the longest synced delay, 1/2 note at
40 BPM, times the widest right channel), read with 4-point Catmull-Rom interpolation. The repeat path per sample is
`input + feedback x delayed -> one-pole low-pass (Tone) -> DC blocker (~10 Hz) -> delay line`. Output is
`dry x (1 - mix) + delayed x mix`.
- **Tempo:** the host BPM is read once per block (same code as Glitch Stutter: clamped 40..300, 120 BPM fallback if missing/invalid;
  the window already shows which). A synced delay is `beats x 60 / BPM`, clamped to the buffer.
- **No clicks:** delay time glides over 80 ms; feedback, tone and on/off/mix ramp over 20 ms. Trade-off: a time or tempo change
  bends the pitch of the repeats while it glides (like a tape echo); a very large jump bends a lot.
- **Stability:** feedback is limited to 90 % in the DSP (not only in the parameter range), both filters are stable at every sample rate,
  the in-loop DC blocker prevents DC build-up, and every sample written to the delay line is checked: a value that is non-finite or
  larger than 1e30 is replaced by 0 and only the filter memory is cleared. NaN/Inf input is replaced by 0 first. The plug-in is never muted.
- **Width** offsets the right channel's delay to (1 + 0.5 x width) times the left's, so a mono vocal gets a stereo echo. It changes
  timing, not level. In a mono layout it has no effect.
- **Off:** the stage fades out over 20 ms, the delay line is cleared once, and the stage is skipped (bit-exact, almost no CPU).
  Switching off therefore cuts the tail instead of letting it ring out.
- **Latency:** none added. The reported latency is still 6 samples, and the echo sees the same time-aligned signal as the dry path.
- **Not done:** ping-pong, ducking, modulation, tempo-locked phase (the delay is a delay, not a beat-grid-locked pattern), and
  `getTailLengthSeconds()` still returns 0, so a host that stops the transport may cut the tail.

## Dimension
Stereo thickening and widening after Liquid Echo. New parameters (version hint 1, default OFF so a fresh instance is unchanged):

| ID | Range | Default |
|---|---|---|
| `dim_on` | on/off | off |
| `dim_width` | 0 to 100 % (0 = centred thickening, 100 = stereo spread) | 60 % |
| `dim_detune` | 0 to 20 cents (peak pitch swing of the modulation) | 6 cents |
| `dim_depth` | 0 to 100 % (level of the thickening voices) | 50 % |
| `dim_mix` | 0 to 100 % (0 = dry, 100 = dry + full effect) | 100 % |

Technique (a Roland-"Dimension"-style dual chorus voice design): the input is summed to mono and high-passed at about 200 Hz
(one pole), then read by two chorus voices (base delays 9 ms and 13 ms) whose delays are modulated by two slow sine LFOs
(0.37 Hz and 0.53 Hz, so the movement never repeats exactly). Detune sets the LFO excursion so the peak pitch swing equals the
chosen cents. The voice sum is the mid part M and the voice difference the side part S; Width cross-fades them with equal power
(cos/sin), so Width does not change the wet level. Output: `L = dry + g (M cos t + S sin t)`, `R = dry + g (M cos t - S sin t)`,
`g = 0.6 x Depth x Mix`. Because the wet is left/right symmetric it cannot pan the vocal; a hard-left input stays left.
- **Mono compatibility (measured, see `TESTING.md`):** at Width 100 % only S is added and S cancels exactly in a mono sum, so the
  mono sum equals the dry signal. At Width 0 % the wet is plain chorus, which colours a mono sum: with Depth 100 and Detune 0 the
  worst single frequency is -6.8 dB / +4.0 dB; at the defaults (Width 60, Depth 50) it is -1.5 / +1.4 dB. Width therefore trades
  stereo width against mono colouring only through the M/S balance; nothing is ever lost in mono at full Width.
- **Level:** at Depth 100 the output is about +0.55 dB above the dry level for noise, and Width changes it by less than 0.1 dB.
- **Why not simply widen with mid/side:** a pure M/S widener does nothing to a mono vocal. The voices decorrelate L and R (a mono
  vocal's L/R correlation falls from 1.00 to about 0.75 at Width 100, Depth 100) and add movement.
- **Detune** is a creative thickening effect (a slow modulated delay), not pitch correction.
- **Mono layout:** there is no side channel, so only the M part is added and Width has no effect.
- **Safety:** NaN/Inf input is replaced by 0; if the high-pass memory overflows only it is cleared (the delay line only receives
  finite values). Off (settled) clears the delay line once and skips the stage (bit-exact); on/off and mix ramp over 20 ms; Detune
  and Width are smoothed; the LFO phase never resets on block boundaries. Zero latency, no allocation after `prepare()`.
- **Limits:** one fixed LFO rate pair (no rate control: it would add a control without improving the effect); Depth and Mix
  overlap (both scale the wet), Mix exists to match the other effects; the effect is built for vocals and is not tempo-synced.

## MOODS 2.0: Alter Egos and the Mood Engine
Five curated vocal identities (**CELESTIAL, THE CLONE, VOIDWALKER, THE PARASITE, ALTERED STATE**) plus three playable controls
(**CHAOS, BLEND, SURPRISE ME**). All of it lives in `Source/AlterEgos.*` (pure data and maths) and the editor; no new DSP.

**How it works.** An Alter Ego is a table of the 30 parameters of the EQ, compressor, saturation drive, Liquid Echo and Dimension.
`GlitchKidProcessor::applyAlterEgo(ego, chaos, seed)` (message thread only) computes the values and sets them through the normal JUCE
parameter calls (`beginChangeGesture`, `setValueNotifyingHost`, `endChangeGesture`), so the knobs show exactly what you hear, FL Studio records
it like automation, and every effect's own smoothing applies. Nothing in MOODS runs on the audio thread. An Alter Ego never touches Output Gain,
Bypass, Blend (the main Mix) or Glitch Stutter.

- **Selection state:** three ValueTree properties in the plug-in state (`alterEgo`, `alterEgoChaos`, `alterEgoSeed`). They only restore the
  macro positions in the window; the parameters themselves are saved as before. Restoring a state never re-applies an Alter Ego, so your edits
  and automation are never overwritten. No new automatable parameter was added (36 before, 36 now), so older sessions load unchanged and a state
  without these properties means "no Alter Ego".
- **Active or custom:** `isAlterEgoActive()` compares the current parameter values with what the selection would produce. The button is lit while
  they match; as soon as you edit a control it reads CUSTOM (and remembers the last Alter Ego).
- **Availability:** each Alter Ego lists the effects it requires and the ones it only uses when present. It is available only if every required
  effect exists in the build (`AlterEgos::implementedEffects`); otherwise its button is disabled with an explanation. Optional effects that are
  missing are skipped parameter by parameter. All five are available today. None uses Glitch Stutter (it repeats captured audio, so it stays a
  deliberate user choice).

| Alter Ego | Intent | Requires (optional) | Saturation 0 to 100 % Chaos | Echo (synced) | Dimension |
|---|---|---|---|---|---|
| CELESTIAL | polished, glossy, airy | EQ, comp, sat, echo (Dimension) | 3 to 6 dB | 1/4 dotted, fb 35 to 50 %, mix 16 to 30 % | width 60 to 85, detune 6 to 10 |
| THE CLONE | fuller, layered, centred | EQ, comp, sat, Dimension (echo) | 2 to 5 dB | 1/8 slap, fb 20 to 35 %, mix 6 to 14 % | width 55 to 85, detune 8 to 16, depth 70 to 100 |
| VOIDWALKER | dark, distant, cinematic | EQ, comp, sat, echo (Dimension) | 3 to 6 dB | 1/4, fb 48 to 65 %, tone 1.6 kHz to 900 Hz, mix 24 to 42 % | width 40 to 80, detune 5 to 9 |
| THE PARASITE | distorted, textured, unnerving | EQ, comp, sat (echo, Dimension) | 9 to 16 dB | 1/32 flutter, fb 30 to 60 %, mix 8 to 22 % | width 60 to 100, detune 10 to 20 |
| ALTERED STATE | psychedelic, experimental | EQ, comp, sat, echo, Dimension | 5 to 9 dB | 1/8 triplet, fb 55 to 75 %, tone 4.5 to 7.5 kHz, mix 25 to 42 % | width 100, detune 14 to 20, depth 80 to 100 |

**CHAOS (0 to 100 %).** The table stores, for every parameter, its value at Chaos 0 and at Chaos 100 %. Chaos moves each continuous parameter in a
straight line between the two (11 to 20 parameters per Alter Ego move, depending on the identity: EQ gains and Q, compressor ratio and speed, saturation, echo
feedback/tone/width/mix, Dimension width/detune/depth/mix). Switches (`echo_on`, `echo_sync`, `echo_div`, `echo_time`, `dim_on`) never move, so Chaos
cannot make an effect appear or an echo time jump. Every end value is inside a safe limit (drive at most 16 dB, echo feedback at most 75 % while
the echo DSP itself caps at 90 %, detune at most 20 cents). The compressor makeup is part of the mapping and was tuned from measurements so the
level on speech-like noise stays within 0.65 dB across the whole Chaos range (more saturation does not make it louder). Chaos 0 is exactly the
documented base; the result is deterministic and clamped to 0..1.

**BLEND** is the plug-in's existing main Mix parameter (`mix`, unchanged ID, so automation still works): `dry x (1 - blend) + processed x blend`
with the dry path delayed by the same 6 samples as the processed path. 0 % is bit-exact the delayed input, 100 % the full processed sound.
Because the effects keep their own mixes inside the chain and Blend is the single outer crossfade, nothing is mixed twice. It replaced the
left-column Mix knob (the same parameter); the Alter Egos never set it.

**SURPRISE ME** applies a seeded variation on top of the current Chaos result: for each parameter on a short allowlist
(`eq_b1_freq/gain`, `eq_b2_freq/gain`, `eq_hi_gain`, `echo_fb/tone/width/mix`, `dim_width/detune/depth/mix`) it adds a random amount within that
parameter's own jitter and clamps to hard limits (e.g. echo feedback 15 to 70 %). It skips echo parameters when the echo is off and Dimension
parameters when Dimension is off. It never varies switches, echo division and time (the rhythm is part of the identity), the compressor,
saturation or makeup (level), or anything outside the 30 owned parameters. The generator is a tiny integer xorshift, so the same
(Alter Ego, Chaos, seed) gives the same values on every platform. The seed is stored with the selection; pressing the Alter Ego button again
removes the variation.

**MORPH** was assessed and deferred. A continuous morph between two Alter Egos needs, per parameter, an interpolation rule (linear for gains and
mixes, geometric for frequencies and times) and a rule for switches and the echo division (hold, then change under a fade), plus a stored second
selection and tests for the intermediate states. Chaos already gives a safe continuous intensity axis; a Morph is a separate future stage.

## Pitch Tune (Stage 4.2; prototype and measurements in `PITCH_CORRECTION_FEASIBILITY.md`)
Vocal pitch correction for one voice, first in the chain. Five parameters, **default off** (so a fresh instance changes nothing and adds no
latency): `tune_on`, `tune_key` (C..B), `tune_scale` (chromatic, major, natural/harmonic minor, major/minor pentatonic), `tune_strength`
(0..100 %, how much of the distance to the target note is corrected), `tune_speed` (0..400 ms glide of the correction; 0 = instant).
- **Detection:** YIN on the channel average (about 24 kHz, 5 ms hop, window about 2 longest periods), 80..800 Hz, voiced only when confidence is
  at least 0.85; a 3-point median of the period; 60 ms hold when the voice drops out. Unvoiced audio (breaths, "s", noise) is passed through
  uncorrected.
- **Shifting:** one TD-PSOLA-style shifter per channel (Hann grains two periods wide, one period apart), all driven by the same period and ratio,
  so stereo images do not move. Not aligned to glottal pulses; no formant control. Re-anchors its timing at the start of each voiced stretch so
  the output matches the reported latency; a later change of period can leave up to half a period of timing offset (a phase shift on a held note).
- **Latency and switching:** the output is always delayed by `latencyFor(sampleRate)` (2.5 x the longest period, 80 Hz lowest) while Tune is on. The
  reported latency and the dry path follow the switch. Switching crossfades over 20 ms between the straight-through signal and the tuned (delayed)
  signal; while off, the corrector keeps listening (`feed`: history only, no analysis) so that switching on has no silent gap.
- **Order:** before the EQ, so the rest of the chain works on the tuned voice. Mix/Blend 0 % is the original voice (delayed to match).
- **Safety:** non-finite input is zeroed before the straight-through copy; the corrector treats NaN/Inf/|x| >= 1e6 as 0. A test proves bit-exact
  equality with the same audio containing zeros, both always-on and while switching.
- **Cost:** about 2 to 3 % of one core while on (about 1 % while off and listening), 100 to 230 KiB of fixed memory (see the measurements in the feasibility doc).
- **UI:** a Pitch Tune row above Liquid Echo (ON, KEY, SCALE, STRENGTH, SPEED) and a read-out `SUNG A3 +23c -> A3` / `listening...` / `OFF`.
- Alter Egos, Chaos and Surprise never touch the Pitch Tune controls.

## What does not exist
User presets (saving and browsing your own), other plug-in formats, host-grid phase-lock
for the stutter, formant shifting in the plug-in (a tested prototype exists in `Source/Experimental/FormantShifting/`, Stage 4.3), polyphonic pitch correction, pitch correction of a mixed (non-solo) signal. Moods have not been tuned by ear on real vocals.
