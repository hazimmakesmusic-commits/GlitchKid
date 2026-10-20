# Vocal Lab UI: audit and result

Scope: the interface only. DSP, parameter IDs, saved state and the mix engine are unchanged (one small addition: a TONE macro in `MacroEngine`, which only writes two existing EQ parameters). Linux results only; nothing here was tested in FL Studio, on a Mac or on Windows.

## What was there (verified in the code before editing)

| Area | Finding |
|---|---|
| Architecture | One `GlitchKidEditor` (`Source/PluginEditor.*`) drawing a fixed 1160 x 720 layout scaled into the window (resizable 60 %-150 %). `layoutContent()` hides every control not on the current page. Widgets in `Source/UI/PlaygroundWidgets.*`. |
| Look | "Alien game": alien mascot in a saucer, hull plates, glowing LED knobs, scanlines, glass screen, big decorative page titles, sigils on profile cards. 1139-line widget file, most of it unused (world pods, effect tiles, chain rows, macro dials, gear, crown). |
| Parameters | 54 APVTS parameters, IDs permanent. Knobs use `SliderAttachment`, toggles/combos use `ButtonAttachment`/`ComboBoxAttachment`; the editor never owns parameter state. |
| State | `uiPage`, `uiCategory`, `uiMotion` saved with the session (not part of a sound or preset). Presets (`.gkpreset`), A/B and sound state are separate (`SoundState`, `PresetStore`, `PresetController`). |
| Mix engine | `automode::buildMix` is real and tested (see `docs/VOCAL_MIXING_MILESTONE_AUDIT.md`). BUILD MY VOCAL MIX already called it. |
| Real signals available | Input and output peak, gain reduction, Pitch Tune's detected and target note, analyser state and progress, instrumental connection status, every parameter value. Nothing else (no spectrum, no waveform of the processed signal, no key detection). |
| Visible defects | Overlap-prone titles, heavy borders around everything, redundant text (status line, tooltip-like banners), oversized profile and tune areas, a mascot reacting to the input. |

## What it is now

**Identity** from real signals and restrained materials: graphite surfaces, one thin divider style, Inter type, muted ultraviolet for selection, acid lime only for the primary action and genuine "processing" states, cyan only for signal information, pink for DRY and warnings. No gradients, glows, scanlines, mascot or artwork. The mascot, saucer, hull, glass screen, sigils, glitch/scramble effects and all unused widgets were deleted.

**Layout** (design size 960 x 600, default window 960 x 600, resizable 60 %-150 % with the aspect ratio kept):
- Header: GLITCHKID / VOCAL LAB, the three workspaces TUNE | AUTO | MODIFY, DRY | PROCESSED (the real Bypass), MOTION.
- Processing chain (always visible): INPUT -> TUNE -> TONE -> DYNAMICS -> SPACE -> OUTPUT. INPUT and OUTPUT show the real peak level. The four stages are the real stages of the engine; a lime dot means the stage is really doing something (read from the actual parameters through `fx::isActive`; everything is off while DRY). Clicking a stage opens its MODIFY category. While a real signal arrives (and MOTION is on) one small dot travels the path. If a session from before still has a hidden effect (formant, harmony, mutation, stutter) switched on, the strip says so in pink; BUILD switches them off.
- Preset bar: `<` `>`, preset menu, SAVE, DELETE, status, A/B.

**TUNE**: twelve-key note pad (dots = notes the scale allows), six scale chips, TUNING switch, NATURAL / LIGHT / HARD, a one-line summary of the real state, the real Pitch Tune read-out (sung note, cents, target note) and a real input-level strip. It says GlitchKid does not detect your key.

**AUTO**: 1 STARTING PROFILE (8 compact cards, name + one line; click reads, double-click or LOAD PROFILE loads, BACK TO MY SOUND undoes); 2 BUILD (a wide BUILD MY VOCAL MIX button that is the strongest element: lime when ready, a sweep while waiting for audio, a progress fill that follows the real analysis time, pink on failure; a status line; the input / vocal / instrumental status; MIX INTENSITY); 3 RESULT (state of the last build, the first fitted settings with their real before/after values, how many settings came from the profile, WHAT CHANGED for the full explanation). The selected profile is saved in the session (`uiProfile`), so AUTO, MODIFY and a reopened window agree.

**MODIFY**: exactly four categories. Each of TONE, DYNAMICS and SPACE starts with one high-level slider that is a real bounded combination of parameters and is read back from them: TONE (darker to brighter: AIR -6..+6 dB and PRESENCE -1.5..+1.5 dB), PUNCH (softer to more upfront: compressor ratio, threshold, makeup), SPACE (dry to spacious: echo, width, doubler). PITCH & KEY has the tuning styles. Below them, the detailed knobs with visible values. BACK TO AUTO, RESET, SAVE PRESET.

**Removed from the interface**: mascot and all creature artwork, the Character tab and macro, SURPRISE ME, Alter Ego buttons, quick-tweak buttons (replaced by the high-level sliders), A>B / B>A copy buttons, the old page titles, the hull and screen, "ASTRO MODE" and "Sound World" wording. `autoModePresetName` ("AutoMode ASTRO MODE") still exists in the processor for the old ASTRO-only `applyAutoMode` path, which nothing in the interface calls any more; it is documented in `docs/AUTOMODE.md`.

## Compatibility

All 54 parameter IDs, ranges and defaults are unchanged. Sessions and presets that used hidden effects still load and sound the same until BUILD or RESET. Saved UI preferences: `uiPage`, `uiCategory` (0-3), `uiMotion`, new `uiProfile` (-1 when absent). No DSP file changed.

## Verification (Linux)

- Fresh build in a new temp directory outside `build/`, plug-in target and test target: see the final report.
- `Tests/MacroTests.cpp` builds the real editor and: renders every page and category and three window sizes; drives the real buttons (workspaces, key pad, scale chips, tuning styles, profile select / LOAD / BACK, BUILD end to end with a vocal-like signal and with silence, DRY / PROCESSED, categories); checks there is no Character tab; moves a knob and checks the real parameter moves (and the reverse); drags the TONE slider and checks the EQ parameters; clicks a chain stage and checks it opens its category; checks the selected profile is saved in the session.
- `Tests/MacroTests.cpp` and `Tests/MixEngineTests.cpp` cover macros (incl. TONE), the engine and its limits; `Tests/PresetTests.cpp` and `Tests/FactoryPresetTests.cpp` cover preset save / load / A/B and state restore.
- Screenshots: `GLITCHKID_SNAPSHOT_DIR=/some/folder GlitchKidTests` writes PNGs of every page and category.

## Not verified / limits

- No FL Studio, Mac or Windows test; no listening test of the new build.
- Host automation was not driven by a real host; only the attachments are tested (control <-> parameter).
- The travelling dot and sweep animations were checked only as still frames.
- No spectrum or waveform of the processed signal exists, so none is drawn.
- Tooltips on the knobs show after a 0.5 s hover; they were not reviewed in a real host.
