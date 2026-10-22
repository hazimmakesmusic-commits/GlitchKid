# UI responsiveness and interaction polish: implementation

Linux, software rendering, synthetic audio only. Nothing here was run in FL Studio or on a Mac, and nobody has used the result by hand: "feels immediate" is **not** established by these numbers. The audit and the baseline timings are in `docs/UI_RESPONSIVENESS_AUDIT.md`. No DSP, parameter ID, preset format or saved state was touched (the sound tests all still pass unchanged).

## Root causes found and what was changed

| Root cause | Evidence | Change |
|---|---|---|
| Every workspace switch faded the whole content from alpha 0.3 and jittered its transform for 5 frames: an offscreen layer (+25 % per full repaint) and a full-window repaint per frame, with the new workspace looking dimmed for about 170 ms | measured: 6.8 - 10.5 ms per frame at alpha 0.3 against 5.6 - 8.0 ms at 1.0; about 5 full repaints | Removed. The new workspace is fully drawn and usable from the first frame. The transition is now a 100 ms scan-line wipe (MOTION on only) that repaints only a thin band: **0.19 - 0.29 ms per frame, measured**, against the 6.8 - 10.5 ms before |
| The live display requested a redraw every tick even when nothing changed (HOME, the EQ graph's spectrum, the input scope) | by code (unconditional `repaint()` in `SpectrumView::setTrace/setWave`, `EqGraph::setSpectrum`, `ScopeStrip::push`) | `SpectrumView` stores data and `commit()` repaints once, only if something changed; `EqGraph::setSpectrum` and `ScopeStrip::push` skip identical data. **Measured after:** a silent HOME makes 0 redraw requests in 400 ms; live audio makes them |
| The display analysed (FFT) and drew the same stale samples when the host stopped sending audio, and froze on the last frame | by code; idle HOME tick 0.11 - 0.18 ms | `TapFreshness`: no new samples for 0.25 s means not live; no FFT, the traces decay to empty and the view says NO SIGNAL. **Measured after:** idle HOME tick 0.014 - 0.022 ms (about 85 % less) |
| The display ran on the 30 Hz state timer | design | The spectrum and waveform now have their own 60 Hz timer that exists only while a display is visible (HOME, CLEAN LAB, BEAT LOCK, FINAL TOUCH > TONE) or a transition runs; it stops elsewhere (tested). The 30 Hz timer keeps state and meters |
| Spectrum fall was per tick, so it depended on the refresh rate | by code | Fall is 1.2 per second (the same as before at 30 Hz), tested equal at 30 and 60 Hz |
| Frequency-axis positions and the grid were recomputed on every paint; a new path per trace per paint | by code | Column positions, grid path and labels are cached per size / sample rate; trace paths are reused (`preallocateSpace`). **Measured: the visualizer's repaint cost did not change (0.47 - 0.50 ms before, 0.49 - 0.51 ms after).** The saving is the avoided repaints, not a cheaper paint |
| Window minimum 60 % made 9.5 - 11 pt labels about 6 px | arithmetic (not a legibility measurement) | Minimum is now 70 % (672 x 420); the maximum and the fixed aspect ratio are unchanged |
| Tooltip delay 500 ms | judgement | 400 ms (a tooltip still disappears on movement or click, JUCE behaviour) |

Also: a test hook (`uiTickForTest`, `uiShowWorkspaceForTest`) and an environment filter for the test runner (`GLITCHKID_TEST_ONLY`) were added so the costs can be measured without a host.

## Before / after measurements (median per call, 3 runs, one shared Linux core, software renderer)

| | Before | After |
|---|---|---|
| 30 Hz refresh, HOME idle | 0.11 - 0.18 ms | 0.014 - 0.022 ms |
| 30 Hz refresh + display, HOME with audio | 0.13 - 0.14 ms | 0.13 - 0.19 ms (unchanged: the same FFT and drawing work) |
| Workspace switch handler | 0.010 - 0.014 ms | 0.009 - 0.010 ms |
| One frame of the transition | 6.8 - 10.5 ms (full repaint at alpha 0.3) | 0.19 - 0.29 ms (band only) |
| Redraw requests per second, silent HOME | about 30 (by code) | 0 (measured) |
| HOME visualizer repaint | 0.47 - 0.50 ms | 0.49 - 0.51 ms (no change) |
| EQ graph repaint | 0.31 - 0.34 ms | 0.31 - 0.32 ms (no change) |
| Full-window repaint | 3.3 - 9 ms | 3.3 - 6.6 ms (unchanged within the noise) |
| Knob / macro drag step | 0.001 / 0.011 ms | unchanged |

These are CPU costs of the software renderer. They do not include the cost of presenting frames, and on macOS the compositor may change them. A claimed improvement exists only where the table shows one.

## Tests run

* `Tests/UiBehaviourTests.cpp` (new): the display decays and reports not-live when audio stops, recovers when it returns, and falls at the same rate at 30 and 60 Hz; a silent HOME requests no redraws and live audio does; the display timer runs only where a display is visible; a transition never changes the content's alpha or offset, finishes within a fraction of a second and does not exist with MOTION off; in every workspace and FINAL TOUCH category, at 70 %, 100 % and 150 % window size, interactive controls stay inside the window, no two overlap and no control from one workspace is left on another; every workspace renders at the three sizes.
* `Tests/UiPerfTests.cpp` (new): the timings above (they log; they fail only if a refresh exceeds 5 ms).
* Unchanged and passing: the DSP, limiter, CLEAN LAB, BEAT LOCK, presets, state, A/B, parameter and automation tests; the editor navigation and control tests.

## Results

| Category | Result |
|---|---|
| Compiled | Fresh Release build in a new directory outside `build/`; the VST3 target builds. |
| Automated-tested | The full suite (see the final report) and the same suite under AddressSanitizer + UndefinedBehaviorSanitizer. |
| Profiled | The harness above, on one Linux core with software rendering. |
| Manually verified in FL Studio | **No.** |

## Known limitations

* The layout is still a fixed 960 x 600 design scaled uniformly: small windows render everything smaller (the 70 % floor limits this); there is no reflow. A true reflow would be a larger change.
* Mouse-wheel over a knob changes it (JUCE default); left as is pending your decision.
* The spectrum timer is 60 Hz by timer, not synchronised to the display (`VBlankAttachment` was not used because it needs a native window, which these tests do not have). Visible smoothness on a real screen is unverified.
* The preset menu reads the preset folder on the UI thread when it opens (negligible for a few presets; not measured for hundreds).
* Software-rendered timings under-state GPU/CoreGraphics behaviour and vary with the machine's load.

## Manual verification steps (FL Studio, Mac)

1. Install the new build, put GlitchKid on a vocal track and play audio. HOME: the spectrum and waveform should move smoothly. Stop the transport: within about a quarter of a second the display should decay and show NO SIGNAL rather than freeze.
2. Click through HOME, CLEAN LAB, BEAT LOCK, BUILD MY MIX and FINAL TOUCH quickly. Controls should be usable at once; a thin pink line sweeps down for about a tenth of a second, with no dimming or sideways jolt. MOTION off removes the line.
3. Drag knobs and sliders while playing audio; the value and sound should follow without stutter. Open the preset menu, step presets with `<` `>`, and recall A and B; each should respond at once.
4. Resize the window from the smallest to the largest size on every workspace: nothing should overlap or be cut off, and the OUT read-out and CLIPPING warning stay visible. Judge whether the smallest labels are readable.
5. Watch FL Studio's CPU meter on a single GlitchKid with HOME open and again with BUILD MY MIX open; note any difference.
6. Tell me anything that still feels slow.
