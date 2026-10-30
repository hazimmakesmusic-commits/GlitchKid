# Vocal Lab shell: audit, concept and first vertical slice

Presentation and navigation only. No DSP, parameter (69), parameter ID, preset, saved state, undo or A/B behaviour changed.

## Audit (before the change)
* One fixed 960 x 600 layout scaled uniformly (70-150 %). `GlitchKidEditor::layoutContent()` positions every control of one page in absolute design pixels and hides the rest.
* Navigation was spread over three bars: six top tabs (HOME, CLEAN LAB, BEAT LOCK, BUILD MY MIX, FINAL TOUCH, REFERENCE), four FINAL TOUCH category tabs below them, and a 48 px chain strip that repeated the four FINAL TOUCH stages. Names mixed job words (BUILD) with feature words (BEAT LOCK, CLEAN LAB), and the way into TUNE, TONE, DYNAMICS and SPACE was two clicks deep.
* Help was a floating tooltip window (800 ms), which covered the controls it described.
* Every control is attached to the processor (APVTS attachments or `ParamKnob`); the display data (spectrum, waveform, levels, CLEAN telemetry, tune detector, instrumental status) comes from lock-free taps and the 30 Hz / 60 Hz editor timers. Undo is `{SoundState, name, valid}`; A/B and presets go through `PresetController`.
* Saved UI state: `uiLevel` (page 0-5) and `uiCategory` (0-3). Tests drive the editor through button text, `uiShowWorkspaceForTest(page, category)` and these saved numbers.

## Concept
Graphite and near-black, one ultraviolet accent, no hero art. The product is organised by the job you are doing, in the order a vocal is worked, instead of by feature names.

| Rail row | Job | Opens (saved page / category) |
|---|---|---|
| START | See the real signal, know the next step | 0 |
| BUILD | Pick a starting sound, build a first mix | 3 |
| CLEAN | Rumble, mud, resonances, S sounds | 1 |
| TUNE | Key, scale, strength, speed | 4 / 0 |
| TONE | Darker-brighter, EQ, air, warmth | 4 / 1 |
| DYNAMICS | Compressor, output gain, limiter | 4 / 2 |
| SPACE | Echo, width, doubling | 4 / 3 |
| BEAT | Vocal against the instrumental | 2 |
| REFERENCE | Reference-Inspired EQ | 5 |

* **Signal rail** (left, 168 px): the wordmark, the nine rows in three groups (SESSION, SHAPE THE VOICE, CONTEXT), the real vocal-in / out levels at the foot. A dot on CLEAN, TUNE, TONE, DYNAMICS and SPACE is lit only while that stage really changes the sound (from the parameters and the CLEAN telemetry; none while DRY). It replaces the top tabs, the FINAL TOUCH tabs and the chain strip.
* **Header**: the workspace name and its one-line job, with the real DRY | PROCESSED switch, UNDO and MOTION.
* **Info line** (above the preset and A/B bar): the help text of the control under the pointer; a fixed place, nothing floats. It also shows the warning when an older session still has retired effects switched on (this used to be in the chain strip).
* **START**: the real input/output display, one sentence that says the next step from real state (no signal yet / no sound chosen / CLEAN off / refine), and four path cards (BUILD, CLEAN, SHAPE, BEAT) with their real statuses; the card for the next step is emphasised. It only suggests; every workspace stays open and nothing is ever locked. No diagnosis is claimed.
* Existing workspaces keep their layouts (written in the old coordinates and shifted into the area beside the rail by `layoutContent`). FINAL TOUCH lost its tab row, its duplicate BACK TO BUILD and SAVE PRESET buttons (the rail and the preset bar do those jobs); RESET stays.
* Saved sessions open on the matching row: the rail maps onto `uiLevel` / `uiCategory` unchanged.

Design size is now 1128 x 600 (rail 168 + workspace 960). Resize limits and fixed aspect work as before.

## Files changed
`Source/PluginEditor.{h,cpp}` (shell, rail, header, info line, START, layout offsets), `Source/UI/PlaygroundWidgets.{h,cpp}` (rail row look; `ChainStrip` and the floating-tooltip look removed), `Tests/ShellTests.cpp` (new), `Tests/MacroTests.cpp`, `LevelUiTests.cpp`, `UiBehaviourTests.cpp`, `UiPerfTests.cpp`, `ReferenceMatchTests.cpp`, `VisualTests.cpp` (navigation names, sizes; the tooltip-bounds test removed with the tooltip look), `CMakeLists.txt` (the new test file).

## Not done in this slice
* The preset popup menu is unchanged (a browser with user presets, favourites and search is not built).
* No separate OUTPUT or FINISH workspace: the limiter and output gain stay on DYNAMICS, RESET on the refine workspaces, and Save in the preset bar.
* The other workspaces were moved, not redesigned. TONE and CLEAN still show a curve plus controls; SPACE has no visual (there is no reverb and nothing real to draw).
* The BUILD page's continue button still reads FINAL TOUCH.
* Seen only in Linux software renders. Not run on a Mac, in a DAW or in FL Studio; plug-in window behaviour in hosts is unverified.
