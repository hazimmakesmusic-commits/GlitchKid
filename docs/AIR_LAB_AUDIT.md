# AIR LAB: audit (written before the module was added)

Repository state: branch `claude/glitchkid-plugin-architecture-3o6w74`, working tree clean, last commit `a02fb59`. Nothing here is committed by this task.

## 1. Current DSP architecture and signal-chain order
`GlitchKidProcessor::processChunk` (Source/PluginProcessor.cpp), per block:
input -> Pitch Tune (optional, ~31 ms latency while on) -> Formant (optional, off in the UI) -> Harmony (optional, off in the UI) -> Vocal Mutation (off in the UI) -> Vocal Doubler -> 4-band EQ -> compressor -> saturation (tanh, 4x IIR-oversampled, fixed 6 samples of latency) -> glitch stutter -> liquid echo -> Dimension -> smoothed output gain -> Mix/Bypass crossfade against a delay-matched dry path. Peak meters tap input and output. Bypass or 0 % mix skips the whole chain and resets the stages.

## 2. What exists that is relevant
| Need | State |
|---|---|
| EQ | `ParametricEq`: low shelf, two bells, high shelf (RBJ biquads, double precision, smoothed over 20 ms, flat bands skipped). Generic tone shaping; no dynamics, no harmonics. |
| Saturation | `Saturation`: odd-harmonic tanh waveshaper, 4x oversampled, whole-band (not frequency-selective). |
| Dynamics | `Compressor` (broadband, with a gain-reduction meter). |
| De-esser | **None.** There is no de-esser DSP or parameter. The analyser measures sibilance but only for BUILD decisions. |
| Output gain | `gain` (-60..+12 dB) smoothed, and `mix` (wet/dry). |
| Reverb | None. |

Nothing in the plug-in boosts "air" or "presence" with sibilance awareness: the EQ can boost the same region but with a fixed gain and no protection.

## 3. Parameters, state, presets, UI conventions
- 54 APVTS parameters, IDs permanent, all created in `createLayout()` with `ParameterID { id, 1 }`. Host parameter order follows creation order, so new parameters must be **appended at the end** to keep every existing automation index.
- `SoundState` captures every parameter except `bypass`; presets (`.gkpreset`), A/B slots and profiles are lists of id/value. `applySoundState` uses the parameter default for any id an older state or preset lacks, and `replaceState` leaves absent parameters at their defaults. New parameters with neutral defaults therefore keep old sessions and presets sounding identical.
- Profiles (`FactoryPresets`) are built on the factory default; `PresetController::loadFactory` and `MixEngine` carry the user's own tuning, Gain and Mix over. AIR LAB settings belong in the same "yours" list (BUILD and LOAD PROFILE must not silently switch your Air off).
- UI conventions: `ParamKnob`/sliders with APVTS attachments, ultraviolet / cyber-pink design tokens (`Source/UI/PlaygroundWidgets.h`), the TONE page of MODIFY holds the EQ graph.

## 4. Tests and build
`GlitchKidTests` (JUCE UnitTest console target, `-DGLITCHKID_BUILD_TESTS=ON`): 309 groups at the start of this task, all passing. Two tests assert the parameter count (54).
Build: `cmake -S . -B <dir> -DCMAKE_BUILD_TYPE=Release -DGLITCHKID_BUILD_TESTS=ON && cmake --build <dir> -j8`, then `<dir>/GlitchKidTests_artefacts/Release/GlitchKidTests`.
Not available here: pluginval, a DAW.

## 5. Design decision: what AIR LAB is
Evaluated options:
1. A fixed high shelf: rejected (the EQ already does this, and it brightens sibilance as much as vowels).
2. A dynamic high-frequency shelf only: smooth and alias-free, but only boosts what is there; it cannot add openness to a dull source.
3. A harmonic exciter only: adds new high content, but a plain waveshaper is level-dependent, aliases and goes harsh on sibilance.
4. **Chosen: a parallel, sibilance-aware combination, all linear filters plus one band-limited polynomial shaper, zero latency.**
   - PRESENCE: a band-pass around 3.4 kHz (Q 0.8) added in parallel (so the lift is a smooth bell), scaled by a sibilance duck.
   - AIR: (a) a gentle 2nd-order high-pass sparkle above ~9 kHz, plus (b) harmonics generated from the 2.5-5.5 kHz band by a low-order polynomial (x squared and x cubed, applied to an envelope-normalised, steeply band-limited signal, so harmonics reach at most about 3 x 5.5 kHz = 16.5 kHz and stay below Nyquist at 44.1 kHz and above: no oversampling and no extra latency are needed), high-passed above ~6 kHz and low-passed at ~16 kHz. The shaped signal is scaled by the same envelope, so the amount of harmonics follows the input level instead of exploding on loud notes, and tiny signals produce almost nothing (floor in the normaliser).
   - Sibilance duck (shared by both paths and both channels, so the stereo image stays consistent): the ratio of 7.5 kHz energy to body energy (about 1 kHz) from the mono sum; when sibilants dominate the added brightness is reduced (attack about 2 ms, release about 60 ms). This is a mitigation, not a guarantee that harshness cannot occur, and it is tested on bright and sibilant material.
   - MIX scales the added content (the enhancement is parallel, so MIX is the wet/dry blend of "processed" and "dry"); OUTPUT TRIM is a smoothed gain (-12..+12 dB) applied with the module; a smoothed on/off crossfade (20 ms) gives a click-free bypass that is bit-exact when off; a smooth knee limits the module's own output to just below full scale, only where the module made the signal louder than the dry input.
   - LINK is a UI behaviour (moving PRESENCE moves AIR with it and the reverse); it is a saved on/off parameter and has no DSP effect.
- **Chain position: after saturation, before stutter / echo / Dimension.** Reasoning: after the compressor so the dynamics already tamed the vowel/sibilant balance; after saturation so the new highs are not re-saturated or compressed; before echo and Dimension so repeats and width share the same tone; before the output gain so Gain still sets the final level. A de-esser would belong immediately before it, but none exists.
- **Latency: unchanged.** All processing is IIR and per-sample; no look-ahead and no oversampling.
- **Defaults:** AIR LAB is off (`air_on` = false), so a fresh plug-in and every old session or preset are bit-identical to before. When switched on the defaults are subtle: PRESENCE 30 %, AIR 30 %, MIX 100 %, TRIM 0 dB, LINK on.

## 6. New parameters (appended after `mix`; IDs are permanent from now on)
`air_on` (bool, off), `air_presence` (0..100 %, 30), `air_air` (0..100 %, 30), `air_link` (bool, on), `air_mix` (0..100 %, 100), `air_trim` (-12..+12 dB, 0).

## 7. Risks
- Parameter count tests (54 -> 60) must be updated, not loosened.
- Two profile-related lists (carry-over in `loadFactory`, keep-list in `MixEngine`, reserved ids in `validateFactory`) must learn the new ids, otherwise BUILD would switch AIR LAB off.
- Exciter aliasing and sibilance harshness are tested, not assumed.
- No host or pluginval run is possible here; CPU is measured offline only.

---

# Result (written after implementation)

## What was implemented
- `Source/DSP/AirLab.{h,cpp}`: the module exactly as designed in section 5 (parallel presence band-pass, air sparkle high-pass, envelope-normalised polynomial harmonics from a steeply band-limited 2.5-5.5 kHz band, mono-linked sibilance duck, smooth 20 ms controls and on/off crossfade, output knee). One reduction after measuring: the harmonic path's maximum gain went from 1.6 to 0.9 because the first value lifted an air-poor test source by 13 dB at 100 %.
- Integration in `GlitchKidProcessor`: six parameters appended after `mix` (host indexes 54-59), `readAirSettings()`, `airLab.prepare/process/reset`, placed after the saturation and before the stutter/echo, reset in the settled-bypass branch. Latency is unchanged.
- `MixEngine` (keep-list), `PresetController::loadFactory` (carry-over) and `validateFactory` (reserved ids) know the AIR LAB ids, so BUILD and LOAD PROFILE leave your AIR LAB settings alone.
- UI (MODIFY > TONE): the EQ graph was shortened and an AIR LAB strip added under it: AIR LAB switch, LINK, PRESENCE and AIR sliders (the two main controls), MIX and TRIM. The processing chain's TONE stage also lights up while AIR LAB is on. The strip looks dimmed while the module is off.
- Not implemented on purpose: a separate "bypass" parameter (the AIR LAB switch is the module bypass, with a click-free crossfade; the global DRY button remains the whole-plug-in comparison), a live meter for the module (there is nothing to measure that the output meter does not already show).

## Measurements (Linux, release build, 48 kHz unless stated; synthetic sources)
| Test | Result |
|---|---|
| Presence band (2.5-5 kHz) lift on a voice-like source | 50 %: +3.0 dB, 100 %: +5.3 dB; below 400 Hz unchanged (< 0.5 dB) |
| Air band (9-16 kHz) lift on a source with a faint noise floor | 30 % (default): +2.3 dB, 50 %: +4.4 dB, 100 %: +8.8 dB; body (100 Hz-2 kHz) unchanged (< 0.3 dB) |
| Content above 9 kHz created from a source with none | more than 30 dB above its (near-silent) level (the measured figure is meaningless in absolute terms because the source is empty there) |
| Aliasing, 10 kHz sine at 44.1 kHz | folded component at 14.1 kHz is -71.9 dB re the fundamental. A 5 kHz sine produces its 3rd harmonic at 15 kHz (-38 dB), which is the intended harmonic, not an alias |
| Sibilant noise (above 5 kHz) vs voiced material, 5-12 kHz | +0.19 dB vs +4.55 dB: the duck keeps sibilance almost untouched on this signal |
| Full-scale-ish noise (0.8) with maximum settings | never above 1.0001 |
| Latency | unchanged (test compares `getLatencySamples()` with AIR LAB on and off) |
| CPU, AIR LAB alone, stereo, 480-sample blocks, all controls at maximum | about 0.5 % of one core at 48 kHz (10 s of audio in 0.05 s). Offline, single run, indicative only; no host CPU meter was used |

## Tests (`Tests/AirLabTests.cpp`, plus updated count / carry-over tests)
Neutral and off bit-exactness, presence and air lift, MIX and TRIM, mono / stereo consistency and no crosstalk, silence and -100 dBFS signals, hot and non-finite input, no click when switching on, block-size independence and repeatability, aliasing, sibilant vs voiced behaviour, processor integration (60 parameters, new ones last, neutral defaults, output unchanged while off, changed while on, latency unchanged, state round trip, an older state without AIR LAB loads with it off). Editor test: the switch and sliders are attached to the real parameters.

## Not verified
- No pluginval (not installed here), no FL Studio or any host, no listening test on real vocals; the sources above are synthetic.
- The LINK behaviour (moving one slider by hand moves the other) needs a real mouse and was not exercised by the headless test; the attachments and the parameter are tested.
- "Harsh or sibilant output cannot occur" is not claimed: the duck lowers the risk, which was measured on one synthetic sibilant signal.
