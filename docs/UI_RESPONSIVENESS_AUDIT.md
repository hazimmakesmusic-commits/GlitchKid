# UI responsiveness audit

Written from the source at commit `380dac8` and from timings taken on the **unmodified interface** with the measurement harness in `Tests/UiPerfTests.cpp` (the only change to the editor before measuring was a test hook that runs one refresh on demand). All timings are Release builds on one core of a shared Linux container with JUCE's **software** renderer and no display: they say how the work compares between versions on the same machine, **not** how it feels on a Mac with GPU compositing, and they vary run to run (three runs are quoted as ranges). Nothing was measured in FL Studio.

## Measured on the unmodified interface (before)

| What | Result (3 runs) |
|---|---|
| One 30 Hz refresh (`timerCallback`), HOME, no audio | 0.11 - 0.18 ms |
| Same, HOME with audio flowing | 0.13 - 0.14 ms |
| Same, CLEAN LAB / BEAT LOCK | 0.07 - 0.11 ms |
| Same, BUILD / FINAL TOUCH | 0.014 - 0.023 ms |
| Switching workspace (layout + visibility, not the later repaint) | 0.010 - 0.014 ms |
| Dragging a knob (one parameter write) | about 0.001 ms |
| Dragging a macro slider (rewrites several parameters) | about 0.011 ms |
| Full-window repaint (software), per workspace | 3.3 - 9 ms (HOME's first frame 14 - 25 ms: first-use font and path caching) |
| HOME visualizer repaint alone | 0.47 - 0.50 ms |
| EQ graph repaint alone (CLEAN LAB, live spectrum) | 0.31 - 0.34 ms |
| Full-window repaint with the content at alpha 0.3 (what every workspace switch did for 5 frames) | 6.8 - 10.5 ms, against 5.6 - 8.0 ms for the same frame at full alpha: about +25 % |

**Conclusion of the measurements:** the per-tick refresh, layout and parameter-write costs are tiny (well under 1 % of a 33 ms frame). They are not what a user would feel as lag. The measurable costs are in painting.

## Findings

### Measured problems
1. **Workspace transition (measured cost).** `startFade()` set the whole content to alpha 0.3 and, for 5 ticks, nudged its transform sideways (a "glitch"). A component at alpha < 1 is rendered through an offscreen layer (+25 % per full repaint, measured), and moving the transform invalidates and repaints the whole window every frame. That is about 5 full-window repaints (3.5 - 10 ms each in software) per workspace switch, on top of a 5-frame (about 170 ms) dim-in during which the new workspace looked washed out. It also contradicts "immediate".
2. **The display never goes idle (by code and by test).** `SpectrumView::setTrace/setWave` called `repaint()` unconditionally on every 30 Hz tick, even in silence with identical data, so HOME redrew about 30 times per second for nothing (and `EqGraph::setSpectrum`, `ScopeStrip::push` did the same on CLEAN LAB / TONE / PITCH & KEY). The after-change redraw-request counter (below) is the measurement; the before figure is by code inspection (the call was unconditional).
3. **FFT work on a stopped transport.** The display kept analysing the same last 2048 samples every tick when nothing was arriving (the idle HOME tick cost 0.11 - 0.18 ms against 0.014 ms of the same page without the display work).

### Correctness problems (found by reading the code, then tested)
4. **Frozen display.** When the host stops calling the plug-in (transport stopped, in some hosts also a muted track), the taps keep their last samples; the spectrum and waveform stayed on the last frame as if live and the fall-off only ran while samples kept arriving. A stale display is a form of fake data.
5. **Frame-rate-dependent fall.** The spectrum fall was fixed per tick, so it would change speed if the refresh rate changed.
6. **Window minimum 60 %.** At 60 % the 9.5 - 11 pt labels render at about 5.7 - 6.6 px: below the size the original author's own comment calls readable. (Judgement, not a measurement: legibility cannot be measured headlessly.)

### Suspected problems (not measured)
7. Opening the preset menu lists the user-preset folder on the message thread (`listUserPresets`): negligible with a few presets, unmeasured with hundreds.
8. A 33 ms spectrum interval is visibly steppier than a 16 ms one on a fast display; whether this matters to users is a matter of feel and was not tested.
9. Mouse-wheel over a knob changes it (JUCE default). A stray wheel movement while the pointer rests on a knob can change a parameter. Left unchanged: it is the expected behaviour in most plug-ins and changing it needs a decision from you.

### Checked and fine
* The audio thread never does UI work. The display data crosses through bounded lock-free taps (`DisplayTap`); no FFT, allocation, lock or I/O happens in the audio callback for drawing.
* Knob, slider and parameter-write handling (0.001 - 0.011 ms per step); the 30 Hz refresh (0.014 - 0.19 ms); workspace layout (0.01 ms); popup menus and confirmations are asynchronous.
* Controls do not overlap or leave the window (new test, below); the layout is a fixed 960 x 600 design scaled uniformly with a fixed aspect ratio, so there is nothing to clip or collide at smaller sizes, but also no reflow: small windows simply render everything smaller.

## Plan
Remove the full-content fade and jitter; replace the transition with a brief scan-line wipe that repaints only a thin band. Run the display on its own ~60 Hz timer that exists only while a display is visible. Redraw only on change. Stop analysing when no new audio arrives and let the display decay and say NO SIGNAL. Cache the frequency-axis geometry and grid. Make the spectrum fall time-based. Raise the minimum window to 70 %. Shorten the tooltip delay slightly. Add tests and measurements.
