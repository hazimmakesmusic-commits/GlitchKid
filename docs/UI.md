# GlitchKid interface: a vocal-effects workstation

The window is one fixed design (1160 x 720) scaled to fit; it is resizable (60 % to 150 %, fixed aspect ratio) and every control is part of that scaled layout, so nothing overlaps or clips at any size.
Five zones:

| Zone | What it holds |
|---|---|
| A. Top bar | Crown + GLITCHKID wordmark; preset `<` name `>` (menu); A/B (SET A, A, B, SET B); input and output activity meters (real peaks); settings gear |
| B. Input / output (left) | Input and output level meters, a clip light (lights red at 0 dBFS, holds about 1 s), the OUTPUT GAIN knob, and the DRY / PROCESSED monitor (the real, delay-matched Bypass) |
| C. Workspace (centre) | Tabs HOME / EFFECTS / PRESETS / SETTINGS. Only the current tab's controls are shown |
| D. FX CHAIN (right) | The eleven effects in their REAL processing order; click a row to open it on EFFECTS; the switch at the left turns the effect on or off where it has one |
| E. Status strip | Small mascot, "MAKE YOUR VOICE WEIRD.", the status message, processing / dry state, reported latency and the version |

## HOME
Three Sound World pods (load the built-in presets), QUICK TWEAKS (DARKER, MORE SPACE, HARDER TUNE, WEIRDER: fixed, bounded changes), AUTO-MIX (ANALYZE VOCAL, INTENSITY, APPLY SOUND; see `docs/AUTOMODE.md`), the SPACE / CHARACTER / PUNCH / CHAOS dials and a rolling input scope.
Mappings and limits: see the macro table in `Source/Macros/MacroEngine.h`. Nothing is applied until you press a button.

## EFFECTS
A grid of the eleven effects (icon, name, state light) and, below it, the controls of the SELECTED effect only. Selecting from the grid or from the chain is the same thing (saved as `uiEffect`).
Effects with a real on/off parameter (Pitch Tune, Harmony, Mutation, Doubler, Stutter, Echo, Dimension) show an ON switch; Formant, EQ, Compressor and Saturation are always in the chain, and are shown lit only when they are away from their neutral setting
(Formant not 0 st, any EQ gain, compressor ratio above 1:1, saturation drive above 0 dB). Tile and chain state are read from the real parameters every frame, so the two always agree with the controls.

## PRESETS
The preset list (FACTORY and USER tags; click to load), SAVE PRESET, DELETE (user presets only, asks first), RESET (Factory Default, asks first), A>B / B>A, and the MOODS (Alter Egos) with CHAOS, BLEND and SURPRISE ME.

## SETTINGS
Animations on/off (`uiMotion`), SHOW PRESET FOLDER, and plain information: version, sample rate, reported latency, where presets are saved, what is fixed (the effect order).

## What is deliberately NOT here
- **No input-gain stage exists in the engine**, so there is no input-gain knob: the knob on the left is the real OUTPUT GAIN and is labelled so.
- **No reverb exists.** Liquid Echo, Dimension and the Doubler are the atmosphere tools.
- **The effect order is fixed by the engine.** The FX CHAIN shows it but cannot reorder, add or remove effects; the panel says so. Reordering would need new engine work.
- No on/off switch is drawn for effects that have no on/off parameter.

## Design system
Colour tokens (`Source/UI/PlaygroundWidgets.h`): background `#08090D`, panel `#101219`, raised panel `#151923`, border `#252B38`, primary purple `#963BFF`, neon violet `#B56AFF`, electric lime `#B7FF3C` (active states and feedback only), cyan `#24D8F2` and pink `#FF4FA3` (to tell effects apart: pink for Mutation/Stutter, cyan for Doubler/Echo/Dimension),
muted text `#9299AA`, text `#F4F3F8`, disabled `#454957`, error / clipping `#FF496A`. Typeface: **Inter** (SIL OFL), compiled into the plug-in from `Assets/Fonts` (see `LICENSES.md`); headings use Inter Bold. Space Grotesk is not bundled (it could not be downloaded in the build environment): to use it,
add the font file to `Assets/Fonts`, list it in `juce_add_binary_data` in `CMakeLists.txt` and point `fontOf` at it.
Knobs are one look everywhere (a ring of LED segments around a dark knurled cap), label above, value below, double-click resets to the default. Toggles are small pill switches (lime when on). Animations are minimal and switch off with SETTINGS > ANIMATIONS.

## Safety and testing
The interface runs on the message thread (30 Hz timer); it reads parameters and atomics and writes only through the normal parameter and preset calls (`captureSoundState` / `applyParameterChanges`, `PresetController`). It never touches the audio thread. No parameter ID, preset or saved state changed.
Tests (`Tests/MacroTests.cpp`): the effect model (order, real enable parameters, activity), navigation through the real buttons, each page showing only its own controls, chain switches changing real parameters, and renders of every page, every effect, three window sizes and every AutoMode state
(set `GLITCHKID_SNAPSHOT_DIR` to write them as PNG). A rendered image being "not blank" does not mean the interface looks good: look at the snapshots, and in a real host.
