# Parameter map — GlitchKid Vocal Lab redesign prototype v1 (revision 2)

Source of truth: `Source/PluginProcessor.cpp` (`GlitchKidProcessor::createLayout`) and `Source/PluginProcessor.h` at base commit `c3653aa` (working tree unchanged in `Source/` for this phase). The table below was generated from the prototype's own parameter list (`index.html`, section 1) so it cannot drift from what the page shows; every ID, range, default and choice list in that list was copied from `createLayout`.

* **Verified** = the ID, range, unit, default and processor destination are taken from the repository. In the prototype the control changes only a JavaScript value, never the plug-in.
* **Simulated** = the prototype shows a value or behaviour that the plug-in computes from audio (meters, telemetry, analysis results).
* **Unresolved** = needs a decision or engineering work before it can be wired.

All 69 parameters are represented. Workspaces in revision 2: TUNE, CLEAN, TONE, DYNAMICS, SPACE, FINISH and the separate EXPERIMENTAL LAB (Formant, Harmony, Mutation, Stutter). Moving controls between workspaces does not move anything in the signal chain. Stage numbers refer to the real processing order: 1 Pitch Tune · 2 Formant · 3 Harmony · 4 Mutation · 5 Doubler · 6 4-band EQ · 7 CLEAN corrective · 8 Compressor · 9 CLEAN de-esser · 10 Saturation · 11 AIR LAB · 12 Stutter · 13 Echo · 14 Dimension · 15 Gain · 16 Mix · 17 Output Limiter.

## 1. Parameters (69)

| # | Parameter ID | Label in prototype | Range | Unit | Default | Enable dependency | Processor destination | Workspace | Mapping |
|---|---|---|---|---|---|---|---|---|---|
| 1 | `gain` | Gain | -60 … 12 | dB | 0 | — | Smoothed output gain, stage #15 (before Mix) | FINISH | Verified |
| 2 | `bypass` | Bypass | off / on | — | off | — | 20 ms crossfade to the delay-matched dry path; the output limiter still runs | top bar Dry/Processed | Verified |
| 3 | `eq_lo_freq` | Low freq | 20 … 500 (skew centre 100) | Hz | 100 | none (0 dB gain = transparent) | ParametricEq low shelf, stage #6 | TONE | Verified |
| 4 | `eq_lo_gain` | Low | -15 … 15 | dB | 0 | none (0 dB gain = transparent) | ParametricEq low shelf, stage #6 | TONE | Verified |
| 5 | `eq_b1_freq` | Body freq | 100 … 5000 (skew centre 700) | Hz | 400 | none (0 dB gain = transparent) | ParametricEq bell 1 ("Body"), stage #6 | TONE | Verified |
| 6 | `eq_b1_gain` | Body | -15 … 15 | dB | 0 | none (0 dB gain = transparent) | ParametricEq bell 1 ("Body"), stage #6 | TONE | Verified |
| 7 | `eq_b1_q` | Body Q | 0.3 … 10 (skew centre 1.5) | — | 1 | none (0 dB gain = transparent) | ParametricEq bell 1 ("Body"), stage #6 | TONE | Verified |
| 8 | `eq_b2_freq` | Pres. freq | 500 … 16000 (skew centre 3000) | Hz | 3000 | none (0 dB gain = transparent) | ParametricEq bell 2 ("Presence"), stage #6 | TONE | Verified |
| 9 | `eq_b2_gain` | Presence | -15 … 15 | dB | 0 | none (0 dB gain = transparent) | ParametricEq bell 2 ("Presence"), stage #6 | TONE | Verified |
| 10 | `eq_b2_q` | Pres. Q | 0.3 … 10 (skew centre 1.5) | — | 1 | none (0 dB gain = transparent) | ParametricEq bell 2 ("Presence"), stage #6 | TONE | Verified |
| 11 | `eq_hi_freq` | Air freq | 1000 … 18000 (skew centre 6000) | Hz | 8000 | none (0 dB gain = transparent) | ParametricEq high shelf ("Air"), stage #6 | TONE | Verified |
| 12 | `eq_hi_gain` | Air | -15 … 15 | dB | 0 | none (0 dB gain = transparent) | ParametricEq high shelf ("Air"), stage #6 | TONE | Verified |
| 13 | `comp_thresh` | Threshold | -60 … 0 | dB | -18 | none (ratio 1:1 = no compression) | Compressor, stage #8 | DYNAMICS | Verified |
| 14 | `comp_ratio` | Ratio | 1 … 20 (skew centre 4) | :1 | 1 | none (ratio 1:1 = no compression) | Compressor, stage #8 | DYNAMICS | Verified |
| 15 | `comp_attack` | Attack | 0.1 … 100 (skew centre 10) | ms | 10 | none (ratio 1:1 = no compression) | Compressor, stage #8 | DYNAMICS | Verified |
| 16 | `comp_release` | Release | 10 … 1000 (skew centre 100) | ms | 100 | none (ratio 1:1 = no compression) | Compressor, stage #8 | DYNAMICS | Verified |
| 17 | `comp_knee` | Knee | 0 … 24 | dB | 6 | none (ratio 1:1 = no compression) | Compressor, stage #8 | DYNAMICS | Verified |
| 18 | `comp_makeup` | Makeup | 0 … 24 | dB | 0 | none (ratio 1:1 = no compression) | Compressor, stage #8 | DYNAMICS | Verified |
| 19 | `sat_drive` | Drive | 0 … 18 | dB | 0 | none (0 dB = clean) | Saturation (oversampled), stage #10 | TONE | Verified |
| 20 | `stutter_on` | Stutter | off / on | — | off | — (this is the switch) | GlitchStutter, stage #12 | EXPERIMENTAL LAB | Verified |
| 21 | `stutter_rate` | Rate | 0=1/4, 1=1/8, 2=1/16, 3=1/32 | — | 1 (1/8) | stutter_on | GlitchStutter, stage #12 | EXPERIMENTAL LAB | Verified |
| 22 | `stutter_mix` | Mix | 0 … 100 | % | 100 | stutter_on | GlitchStutter, stage #12 | EXPERIMENTAL LAB | Verified |
| 23 | `echo_on` | Echo | off / on | — | off | — (this is the switch) | LiquidEcho, stage #13 | SPACE | Verified |
| 24 | `echo_sync` | Sync | off / on | — | on | echo_on | LiquidEcho, stage #13 | SPACE | Verified |
| 25 | `echo_time` | Time | 1 … 2000 (skew centre 300) | ms | 350 | echo_on; used only while echo_sync is off | LiquidEcho, stage #13 | SPACE | Verified |
| 26 | `echo_div` | Division | 0=1/2, 1=1/4, 2=1/4 dotted, 3=1/4 triplet, 4=1/8, 5=1/8 dotted, 6=1/8 triplet, 7=1/16, 8=1/16 dotted, 9=1/16 triplet, 10=1/32 | — | 4 (1/8) | echo_on; used only while echo_sync is on | LiquidEcho, stage #13 | SPACE | Verified |
| 27 | `echo_fb` | Feedback | 0 … 90 | % | 35 | echo_on | LiquidEcho, stage #13 | SPACE | Verified |
| 28 | `echo_tone` | Tone | 400 … 20000 (skew centre 3000) | Hz | 6000 | echo_on | LiquidEcho, stage #13 | SPACE | Verified |
| 29 | `echo_width` | Width | 0 … 100 | % | 0 | echo_on | LiquidEcho, stage #13 | SPACE | Verified |
| 30 | `echo_mix` | Mix | 0 … 100 | % | 30 | echo_on | LiquidEcho, stage #13 | SPACE | Verified |
| 31 | `tune_on` | Pitch Tune | off / on | — | off | — (this is the switch) | PitchCorrector, stage #1 | TUNE | Verified |
| 32 | `tune_key` | Key | 0=C, 1=C#, 2=D, 3=D#, 4=E, 5=F, 6=F#, 7=G, 8=G#, 9=A, 10=A#, 11=B | — | 0 (C) | tune_on | PitchCorrector, stage #1 | TUNE | Verified |
| 33 | `tune_scale` | Scale | 0=Chromatic, 1=Major, 2=Natural minor, 3=Harmonic minor, 4=Major pentatonic, 5=Minor pentatonic | — | 0 (Chromatic) | tune_on | PitchCorrector, stage #1 | TUNE | Verified |
| 34 | `tune_strength` | Strength | 0 … 100 | % | 100 | tune_on | PitchCorrector, stage #1 | TUNE | Verified |
| 35 | `tune_speed` | Speed | 0 … 400 (skew centre 40) | ms | 20 | tune_on | PitchCorrector, stage #1 | TUNE | Verified |
| 36 | `formant_shift` | Shift | -12 … 12 | st | 0 | active when ≠ 0 (no separate switch) | FormantShifter, stage #2 | EXPERIMENTAL LAB | Verified |
| 37 | `mut_on` | Mutation | off / on | — | off | — (this is the switch) | VocalMutation, stage #4 | EXPERIMENTAL LAB | Verified |
| 38 | `mut_amount` | Amount | 0 … 100 | % | 40 | mut_on | VocalMutation, stage #4 | EXPERIMENTAL LAB | Verified |
| 39 | `mut_character` | Character | 0 … 100 | % | 50 | mut_on | VocalMutation, stage #4 | EXPERIMENTAL LAB | Verified |
| 40 | `mut_blend` | Blend | 0 … 100 | % | 100 | mut_on | VocalMutation, stage #4 | EXPERIMENTAL LAB | Verified |
| 41 | `harm_on` | Harmony | off / on | — | off | — (this is the switch) | HarmonyVoice, stage #3 | EXPERIMENTAL LAB | Verified |
| 42 | `harm_interval` | Interval | 0=Octave DOWN (-12), 1=Fifth DOWN (-7), 2=Major 3rd DOWN (-4), 3=Minor 3rd DOWN (-3), 4=Minor 3rd UP (+3), 5=Major 3rd UP (+4), 6=Fourth UP (+5), 7=Fifth UP (+7), 8=Octave UP (+12) | — | 7 (Fifth UP (+7)) | harm_on | HarmonyVoice, stage #3 | EXPERIMENTAL LAB | Verified |
| 43 | `harm_level` | Level | -24 … 0 | dB | -9 | harm_on | HarmonyVoice, stage #3 | EXPERIMENTAL LAB | Verified |
| 44 | `dbl_on` | Doubler | off / on | — | off | — (this is the switch) | VocalDoubler, stage #5 | SPACE | Verified |
| 45 | `dbl_amount` | Amount | 0 … 100 | % | 50 | dbl_on | VocalDoubler, stage #5 | SPACE | Verified |
| 46 | `dbl_width` | Width | 0 … 100 | % | 70 | dbl_on | VocalDoubler, stage #5 | SPACE | Verified |
| 47 | `dbl_detune` | Detune | 0 … 25 | ct | 8 | dbl_on | VocalDoubler, stage #5 | SPACE | Verified |
| 48 | `dbl_timing` | Timing | 5 … 45 | ms | 18 | dbl_on | VocalDoubler, stage #5 | SPACE | Verified |
| 49 | `dim_on` | Dimension | off / on | — | off | — (this is the switch) | Dimension, stage #14 | SPACE | Verified |
| 50 | `dim_width` | Width | 0 … 100 | % | 60 | dim_on | Dimension, stage #14 | SPACE | Verified |
| 51 | `dim_detune` | Detune | 0 … 20 | ct | 6 | dim_on | Dimension, stage #14 | SPACE | Verified |
| 52 | `dim_depth` | Depth | 0 … 100 | % | 50 | dim_on | Dimension, stage #14 | SPACE | Verified |
| 53 | `dim_mix` | Mix | 0 … 100 | % | 100 | dim_on | Dimension, stage #14 | SPACE | Verified |
| 54 | `mix` | Mix | 0 … 100 | % | 100 | — | Dry/wet blend (wetMix), stage #16 (after Gain) | FINISH | Verified |
| 55 | `air_on` | Air Lab | off / on | — | off | — (this is the switch) | AirLab, stage #11 | TONE | Verified |
| 56 | `air_presence` | Presence | 0 … 100 | % | 30 | air_on | AirLab, stage #11 | TONE | Verified |
| 57 | `air_air` | Air | 0 … 100 | % | 30 | air_on | AirLab, stage #11 | TONE | Verified |
| 58 | `air_link` | Link | off / on | — | on | air_on (links Presence and Air) | AirLab, stage #11 | TONE | Verified |
| 59 | `air_mix` | Mix | 0 … 100 | % | 100 | air_on | AirLab, stage #11 | TONE | Verified |
| 60 | `air_trim` | Trim | -12 … 12 | dB | 0 | air_on | AirLab, stage #11 | TONE | Verified |
| 61 | `clean_on` | Clean | off / on | — | off | — (this is the switch) | CleanLab: corrective #7, de-esser #9 | CLEAN | Verified |
| 62 | `clean_amount` | Clean | 0 … 100 | % | 50 | clean_on | CleanLab: corrective #7, de-esser #9 | CLEAN | Verified |
| 63 | `clean_tone` | Corrective EQ | 0 … 100 | % | 100 | clean_on | CleanLab: corrective #7, de-esser #9 | CLEAN | Verified |
| 64 | `clean_res` | Resonance | 0 … 100 | % | 100 | clean_on | CleanLab: corrective #7, de-esser #9 | CLEAN | Verified |
| 65 | `clean_ess` | De-esser | 0 … 100 | % | 100 | clean_on | CleanLab: corrective #7, de-esser #9 | CLEAN | Verified |
| 66 | `lim_on` | Limiter | off / on | — | off | — (this is the switch) | OutputLimiter, stage #17 (last; also on the dry path) | FINISH | Verified |
| 67 | `lim_ceiling` | Ceiling | -12 … -0.1 | dBFS | -1 | lim_on | OutputLimiter, stage #17 (last; also on the dry path) | FINISH | Verified |
| 68 | `lim_drive` | Drive | 0 … 24 | dB | 0 | lim_on | OutputLimiter, stage #17 (last; also on the dry path) | FINISH | Verified |
| 69 | `lim_release` | Release | 10 … 500 | ms | 80 | lim_on | OutputLimiter, stage #17 (last; also on the dry path) | FINISH | Verified |

Notes on the table:
* Label differences in the prototype UI (same parameter): `sat_drive` is shown as **Warmth**, `clean_amount` as **Clean amount**, `mix` as **Dry/wet mix**; the 12 `tune_key` values are shown as note chips; `tune_strength` + `tune_speed` are also set by the **Natural / Light / Hard** style chips (see section 2).
* Skewed knobs use JUCE's `setSkewForCentre` behaviour (the prototype reproduces it so positions match).
* `tune_scale` names come from `ScaleQuantizer::name`; `echo_div` from `LiquidEcho::divisionName`; `harm_interval` from `harmony::intervalInfo` (default 7 = Fifth UP); `stutter_rate` from `createLayout`.
* Latency (reported to the host, verified): 6 samples always; Pitch Tune ≈31 ms while on; Formant ≈21 ms while ≠ 0; Harmony ≈ Pitch Tune's latency while on; Output Limiter ≈1.5 ms while on. The prototype's latency read-out adds these approximations at 48 kHz.

## 2. Multi-parameter controls (verified mappings)

| Prototype control | Writes | Exact mapping | Source |
|---|---|---|---|
| Quick control **Tone** | `eq_hi_gain`, `eq_b2_gain` | hi = −6…+6 dB, b2 = −1.5…+1.5 dB, linear in the slider; read back from `(eq_hi_gain + 6) / 12` | `MacroEngine.cpp` |
| Quick control **Punch** | `comp_ratio`, `comp_thresh`, `comp_attack`, `comp_release`, `comp_knee`, `comp_makeup` | ratio 1 + 5v; threshold −12 → −26 dB; attack 15 ms; release 110 ms; knee 6 dB; makeup min(4, 0.8·(ratio−1)); read back from `(comp_ratio − 1) / 5` | `MacroEngine.cpp` |
| Quick control **Space** | `echo_on`, `dim_on`, `dbl_on`, `echo_mix`, `echo_fb`, `echo_width`, `dim_mix`, `dbl_amount` | switches on at v ≥ 0.02; echo mix 60v %; feedback 15 → 45 %; echo width 20 → 75 %; Dimension mix 45v %; Doubler 40v %; read back from `echo_mix / 60` while Echo is on | `MacroEngine.cpp` |
| Tuning style **Natural / Light / Hard** | `tune_strength`, `tune_speed`, `tune_on` | 50 %/80 ms, 75 %/35 ms, 100 %/0 ms; switches Tune on; style read back within ±3 % and ±5 ms | `MacroEngine.h` |
| AIR LAB **Link** | `air_link` (and moves `air_presence`/`air_air` together) | Prototype simplification: copies the moved value to the other control. The plug-in's exact link behaviour is in the editor (`linkAir`) — **Unresolved** for the redesign (keep the existing behaviour). | `PluginEditor.cpp` |

## 3. Non-parameter controls

| Prototype control | Plug-in counterpart | Status |
|---|---|---|
| Preset name, ‹ › step, preset browser, Load | `PresetController` (`allPresetNames`, `step`, `loadFactory`, `loadFactoryDefault`, `loadUser`); starting profiles from `FactoryPresets` / `Profiles.inc` | Verified behaviour: a profile load keeps the 22 parameters `tune_on, tune_strength, tune_speed, tune_key, tune_scale, gain, mix, air_*` (6), `clean_*` (5), `lim_*` (4) and sets everything else to Factory Default + the profile's overrides. Browsing groups are a **proposal** (no categories exist). |
| "● modified" | `GlitchKidProcessor::isPresetModified()` | Verified that it exists; the prototype compares the live state with the loaded/applied sound. |
| Save current sound as preset… (in the preset browser) | `PresetController::saveAs (name, overwrite)` (asks before overwriting) | Not simulated. |
| A / B buttons (labels: empty / active / unsaved / saved), Save to A/B, confirmation dialogs | Processor: `captureAbSlot`, `recallAbSlot`, `isAbSlotModified`, `copyAbSlot` (2 slots, saved in the session) | Verified API. The rules are **proposals** to implement in the editor: an empty slot captures the current sound; an occupied slot is recalled; leaving a slot with unsaved changes asks (Cancel / Switch without saving / Save and switch); "Save to A/B" on an occupied slot asks before overwriting; nothing overwrites a slot silently. The plug-in today has SET A / SET B buttons that capture without asking. |
| Undo (one level) | Editor `undo` struct + `backToMine()` | Verified (build, profile load, Beat Lock, Reference apply). |
| Dry / Processed | `bypass` parameter | Verified. |
| Beat Lock panel | `automode::analyseBeatLock` / `applyBeatLock` + editor | Verified concept; the analysis, band bars and suggestion values shown are **Simulated**. |
| Reference Match panel | `ReferencePanel`, `ReferenceAnalyzer`, `automode::ReferenceMatch` | Verified concept and limits; file choosing not simulated; values **Simulated**. |
| Build My Mix review | `automode::buildMix` (pure) + editor | See section 4. |
| Signal display, IN/OUT/COMP GR/LIMITER meters, Tune read-out, CLEAN telemetry and CLEAN correction curve, EQ curve | Display taps + `DisplaySpectrum`, peak/GR getters, `getTuneDetectedMidi/TargetMidi`, `getCleanApplied`, `EqGraph::responseDb` | Data sources verified; **every value shown in the prototype is Simulated**. The prototype's EQ curve and CLEAN correction curve use simplified shapes, not the plug-in's formulas, and are labelled "approximate" / "illustrative". The CLEAN telemetry fields shown (rumble, low-mid, up to three resonances, de-ess reduction, learned seconds) exist in `CleanLab::Applied`. |
| Latency read-out | `getLatencySamples()` | Simulated approximation. |

## 4. Build My Mix review — what is engine fact and what is prototype

| Item | Status |
|---|---|
| Target = Factory Default + the direction's overrides; the same 22 parameters and Bypass are never changed | Verified (`MixEngine.cpp`) |
| Intensity scales `echo_mix`, `dim_mix`, `dbl_amount`, `sat_drive` (100 % = as saved, 0 % = half) | Verified intent (`amountIds`, editor tooltip); exact formula should be re-checked during implementation |
| `mut_on`, `harm_on`, `stutter_on` switched off and `formant_shift` set to 0, with reasons | Verified (reason text in the prototype is shortened: "Undo" instead of "BACK TO MY SOUND") |
| `changes` lists only parameters that differ, with from/to/reason; direction-sourced reasons read "From the … profile." | Verified |
| The two "fitted" changes (threshold −2 dB, makeup −1 dB) and their measurements | **Simulated** (wording modelled on the engine) |
| One checkbox per proposed change (group checkboxes only tick/untick their rows), Current vs Proposed columns, stale detection per change, Cancel / × / workspace switch change nothing, apply only ticked changes, "● modified" after a partial apply | **Proposal** (UI behaviour; no engine change). Verified in the prototype by an automated interaction test. |
| Display grouping: EQ per band (`eq_lo`, `eq_b1`, `eq_b2`, `eq_hi`), then by prefix (`comp`, `sat`, `echo`, `dim`, `dbl`, `mut`, `harm`, `stutter`, `formant`). Groups are for reading only; each change is selected individually. | **Proposal** |
