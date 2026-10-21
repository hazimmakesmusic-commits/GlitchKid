# Vocal Lab "extreme cyberpunk" redesign: audit (written before the redesign)

Repository state when the audit was done: branch `claude/glitchkid-plugin-architecture-3o6w74`, working tree clean (everything from the previous milestones is committed, last commit `66f4d91`). No uncommitted or untracked user work existed. This task does not commit, push, stash or switch branches.

## 1. Current UI structure
- `Source/PluginEditor.{h,cpp}`: one `GlitchKidEditor`. A 960 x 600 design layout is drawn into a `content` component that is scaled to the window (resizable 60 %-150 %, aspect kept). Always visible: header (name, TUNE | AUTO | MODIFY, DRY | PROCESSED, MOTION), a processing-chain strip, a preset bar with A/B. `layoutContent()` hides every control that is not on the current page.
- `Source/UI/PlaygroundWidgets.{h,cpp}`: palette tokens, Inter font, look-and-feel, `ProfileCard`, `NoteKey`, `ScopeStrip`, `BuildButton`, `ChainStrip`, dial drawing.
- Palette now: graphite + violet `#8a5cf0`, **acid lime** (primary action / active), cyan (signal), pink (DRY / warnings). This conflicts with the locked palette of this task (lime and cyan must go).

## 2. What already works (verified in code and by tests)
- TUNE: key pad, scale chips, tuning on/off, NATURAL / LIGHT / HARD (real strength + speed presets), real Pitch Tune read-out (detected and target note from the DSP), real input-level strip.
- AUTO: eight profiles, LOAD PROFILE, BACK TO MY SOUND, BUILD MY VOCAL MIX (real analyser, real `automode::buildMix`, applies real parameters, honest failure on silence), MIX INTENSITY, result block with real before/after values, WHAT CHANGED, DRY | PROCESSED, source status (input / vocal / instrumental).
- MODIFY: four categories, TONE / PUNCH / SPACE high-level sliders (real bounded parameter combinations), detailed knobs, RESET, SAVE PRESET.
- Real data available to the UI: input and output peak, compressor gain reduction, Pitch Tune detected / target note, analyser state and progress, instrumental connection status, every parameter value.
- State: `uiPage`, `uiCategory`, `uiMotion`, `uiProfile` saved with the session; presets (`.gkpreset`), A/B and sound state are separate and tested.

## 3. What is missing (will not be faked)
- **No de-esser**: there is no de-esser parameter or DSP. The sibilance measurement exists only inside the analyser and influences BUILD (it can lower the AIR gain). The UI will not show a de-esser.
- **No reverb**: "Space" is echo (delay), Dimension (width) and the Doubler.
- **No spectrum analyser** (the audio thread produces peaks only). Nothing resembling a live spectrum will be drawn.
- **No EQ graph yet**: the EQ is shown as four knobs. It has four bands with real parameters: low shelf (freq, gain), bell 1 (freq, gain, Q), bell 2 (freq, gain, Q), high shelf (freq, gain); RBJ cookbook biquads in `ParametricEq`. The graph will be computed from those parameters with the same formulas. Because the DSP file will not be touched, a test compares the graph's numbers with the real `ParametricEq` output.
- No FL Studio, Mac or Windows testing is possible in this environment.

## 4. Controls and their connection
- Every knob uses a `SliderAttachment`, every toggle / combo an attachment to a real APVTS parameter. High-level sliders write real parameters through `MacroEngine` and are read back from them. Buttons call real actions (preset controller, build engine, bypass parameter).
- Disconnected or decorative: none known at the end of the previous milestone. Duplicated information: the `TONE` knob row (AIR / PRESENCE / LOW / BODY gains) would duplicate the new EQ graph, so the graph replaces those four knobs; the parameters stay.
- Misleading risks to avoid in this redesign: any pitch line, spectrum, gain-reduction or "AI score" that is not measured; a de-esser or reverb that does not exist; a glow animation that is not tied to real activity.

## 5. State, automation and compatibility risks
- No parameter ID, range, default or type will change; no DSP file will be modified; saved state and presets keep loading. The only code outside the UI that may change is `MacroEngine` (already extended in the previous milestone) and nothing else.
- Dragging an EQ point must use `beginChangeGesture` / `setValueNotifyingHost` / `endChangeGesture` so the host sees real automation.
- Repaint and CPU: the graph path is rebuilt only when a parameter changed; the grid and static decoration are drawn only when the window repaints; animation timers run at the existing 30 Hz and only repaint when something moves (idle = no repaint).

## 6. Safe implementation sequence
1. Design tokens (locked palette) and widget restyle.
2. Shell: header, chain strip, panels with thin technical details (grid, corner brackets, header scanline, chromatic fringe on the wordmark), calm workspace transition.
3. TUNE restyle (target-note highlight from the real detector).
4. AUTO restyle (BUILD MY VOCAL MIX in ultraviolet with pink details, states tied to real state).
5. MODIFY reorganisation (TONE page built around the EQ graph).
6. EQ graph from real parameters, draggable, tested against the real DSP.
7. Polish, tests, fresh build, screenshots, documentation.

---

# Result (written after the redesign)

## Design tokens (`Source/UI/PlaygroundWidgets.h`)
Void Black `#08060D` (window), Deep Graphite `#15111B` (panels; `#1B1523` for raised surfaces), Ultraviolet `#C026FF` (primary accent: selection, focus, the main action, EQ), Cyber Pink `#FF3DAE` (secondary: real processing activity, the note being tuned to, success, the signal line), Pale Chrome `#E8DDF2` (text), Muted Lavender Grey `#9B91A5` (secondary text), Divider Violet `#392044` (borders). Red marks clipping or errors. Acid lime and cyan were removed from the code. A test checks the tokens.

## What was implemented
- **Shell:** faint technical grid behind the workspace, header with fine scanlines and an ultraviolet-to-pink signal line, wordmark with a slight chromatic fringe, thin panels with ultraviolet corner brackets, a very short horizontal glitch (about 5 frames) when the workspace changes (off when MOTION is off). The window stays 960 x 600, resizable 60 %-150 %.
- **TUNE:** same verified controls, restyled. New: the key that Pitch Tune is really pulling the voice to gets a pink outline (from the real detector's target note; nothing while tuning is off or no note is sung).
- **AUTO:** BUILD MY VOCAL MIX is now a chamfered ultraviolet block with a pink detail line: idle (short pink line), hover (pink outline), pressed, waiting for audio (a sweep), analysing (ultraviolet fill following the real analysis time with a pink scan line at the progress point), ready (full-width pink line, "BUILD AGAIN"), failure (red). All states come from the real analyser and build state.
- **MODIFY > TONE:** a real EQ response graph (below) with the four band points, band selector, the selected band's FREQ / GAIN / Q knobs, WARMTH, and the TONE slider. The four separate gain knobs were replaced by the graph and selected-band knobs (same parameters).
- **Chain strip:** active stage dots are pink, the travelling signal dot is pink, level bars are ultraviolet.

## EQ graph: how it is kept honest
- The curve is the sum of the four RBJ biquad responses computed from the current parameter values with the same formulas as `ParametricEq` (bands within 0.05 dB of flat are skipped, as the DSP does). It uses the processor's sample rate.
- `Tests/MacroTests.cpp` runs the real `ParametricEq` on sine tones at seven frequencies with all four bands active and requires the graph's number to match the measured output within 0.15 dB. This passed.
- Dragging a point writes `eq_*_freq` and `eq_*_gain` with host change gestures; the wheel changes a bell's Q; double-click flattens a band. The knobs, presets, BUILD and automation move the points and curve (`refresh()` runs on the 30 Hz timer and repaints only when a parameter changed). Tested: drag raises the real gain and frequency, the point follows the parameters, double-click flattens, the knobs and graph agree.
- No spectrum analyser exists, so none is drawn; the page says so.

## Still absent (not faked)
- De-esser (no DSP, no parameter), reverb (Space = echo + width + doubler), live spectrum, pitch-history line. The compressor's GR meter and IN / OUT meters are real.
- The Pitch Tune read-out is a text line (real sung note, cents, target), not a graph.

## Compatibility
No DSP file, parameter ID, range, default or type changed. `PlaygroundWidgets` now includes `juce_audio_processors` (for the graph's APVTS access). State, presets and automation behave as before (the full suite passes).
