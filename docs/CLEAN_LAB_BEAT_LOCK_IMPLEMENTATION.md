# CLEAN LAB, BEAT LOCK and the level-select interface: implementation

Linux, synthetic signals only. Nothing here was tested in FL Studio, on macOS or on Windows, and pluginval is not installed on the build machine. Read the limitations before trusting any claim.

## What was implemented

| Part | State |
|---|---|
| CLEAN LAB (corrective EQ, dynamic resonance suppression, de-esser, CLEAN amount, genuine bypass, real EQ graph overlay) | Already built and tested in the previous milestone (`docs/CLEAN_LAB_IMPLEMENTATION.md`). Verified again here, unchanged DSP. It now has its own level (LEVEL 01) with the real EQ graph under the section controls. |
| BEAT LOCK engine | New: `automode::analyseBeatLock`, `automode::applyBeatLock`. |
| BEAT LOCK screen (LEVEL 02) | New: analyse, evidence, suggestions with reasons, tick to reject, apply, undo, honest no-beat fallback. |
| Real beat meter | New: `GlitchKidProcessor::consumeBeatPeak()` (one atomic, raised from the peak the aux bus code already measured). |
| 8-bit level-select interface | New: level-select home, persistent bar, four levels, pixel kit. |

## Files

* `Source/AutoMode/SourceRelationship.{h,cpp}`: `BeatMove` and `beatMoves()`, the one place that decides which control a beat relationship moves and why. `compare()` is unchanged.
* `Source/AutoMode/MixEngine.cpp`: the beat-informed block of BUILD now calls `beatMoves()` (same behaviour as before; the existing MixEngine tests still pass).
* `Source/AutoMode/BeatLock.{h,cpp}`: the pure BEAT LOCK engine.
* `Source/PluginProcessor.{h,cpp}`: `beatPeak` atomic and `consumeBeatPeak()`.
* `Source/UI/PixelKit.{h,cpp}`: pixel font (5x7), stepped boxes, dither, scanlines, `PixelLabel`, `PixelBar`, `BandCompare`, `LevelCard`.
* `Source/UI/PlaygroundWidgets.{h,cpp}`: every widget moved to stepped corners and pixel segments (dial, sliders, toggles, buttons, panels, chain, scope, profile cards, note keys, BUILD button, EQ graph frame and handles).
* `Source/PluginEditor.{h,cpp}`: five pages (level select, CLEAN LAB, BEAT LOCK, BUILD MY VOCAL MIX, FINAL TOUCH), persistent bar, level statuses, BEAT LOCK workflow.
* Tests: `Tests/BeatLockTests.cpp`, `Tests/LevelUiTests.cpp`, `Tests/MacroTests.cpp` (navigation rewritten), `Tests/CleanLabTests.cpp` (preference key).
* Docs: `docs/CLEAN_LAB_BEAT_LOCK_UI_AUDIT.md`, `docs/UI.md`, `docs/SOURCES.md`, `CHANGELOG.md`.

## BEAT LOCK: how it works

1. **Is there a beat?** `getInstrumentalStatus()` reports `notConnected`, `connectedSilent` or `live` from the real bus state and the real signal. Only `live` can start an analysis. Pressing ANALYZE BEAT with `notConnected` or `connectedSilent` starts nothing and says why (`analyseBeatLock({}, {}, input).message`). A track that already holds vocal and beat mixed is measured as the vocal; the page says so.
2. **Analysis.** `startSourceAnalysis()` starts the vocal analyser (the track's input) and the beat analyser (the Instrumental input). When the vocal analysis ends, the beat analyser is stopped too, so both have heard the same material. Progress shown is the vocal analyser's real active seconds. The analysers are the existing `VocalAnalyzer` (peak, RMS, five bands, sibilance).
3. **Relationship.** `compare()` gives vocal-minus-beat level, a masking risk per band, whether the beat is low-end heavy or very loud, and a confidence from how much audio was heard.
4. **Recommendations** (`beatMoves()`, shared with BUILD):
   * BALANCE: advice only, never applied. If the beat is more than 6 dB louder or the vocal more than 6 dB louder, it says so and names your mixer fader. GlitchKid cannot move it.
   * PRESENCE: `eq_b2_gain` +0 to +2.5 dB (twice the worst risk, capped) when the beat covers the voice at 2 kHz and above.
   * CLARITY: `eq_b1_gain` -1 dB and `eq_lo_gain` -1 dB when the beat is busy in the low-mids (risk above 0.35); `eq_lo_gain` -1.5 dB when the beat is heavy under 120 Hz.
   * SPACE: `echo_mix` and `dim_mix` are scaled down (never up) when the beat is crowded. There is no reverb, so only the existing echo and width amounts are touched, and only if they are already on.
5. **Apply.** `applyBeatLock` applies only the ticked, non-advisory items, clamps every value to its parameter range (and grid), skips a scale move when the base is zero, and returns the list of real from/to changes. The editor stores the previous sound first (the shared UNDO), then uses the normal `applyParameterChanges`. Apply is available once per analysis so adjustments cannot stack by accident. UNDO (bar) or UNDO BEAT LOCK restores the previous sound exactly.
6. **Reject or modify.** Every suggestion has a tick. Untick to reject. Afterwards every parameter is still a normal control: change it in FINAL TOUCH.
7. **No beat.** The suggestion list is empty, nothing can be applied, and VOCAL-ONLY: BUILD MY MIX opens the BUILD level, which works with the vocal alone.

BEAT LOCK adds **no parameters**: it edits existing, already-saved controls. The count stays at 65; IDs, order, defaults, automation and saved states are untouched.

## CLEAN LAB (unchanged DSP)

Chain position: user EQ -> CLEAN LAB corrective + resonance -> compressor -> CLEAN LAB de-esser. Parameters `clean_on`, `clean_amount`, `clean_tone`, `clean_res`, `clean_ess` (appended after AIR LAB in the previous milestone). Zero added latency. See `docs/CLEAN_LAB_IMPLEMENTATION.md` for the algorithms and limits. The new level only reorganises the controls: switch and amount on top, the three section trims with their real read-outs, and the real EQ graph with CLEAN LAB's pink overlay.

## Parameter IDs

No new parameter IDs in this milestone. UI preferences (not parameters, saved in the session state): `uiLevel` (new, 0 = level select, 1-4 = the levels; replaces `uiPage`, which old sessions may still contain and which is ignored), `uiCategory` (now 0-3), `uiMotion`, `uiProfile`. An older session loads with the level-select screen.

## Signal chain order (unchanged)

input -> Pitch Tune -> Formant -> Harmony -> Mutation -> Doubler -> user EQ -> CLEAN LAB (corrective + resonance) -> compressor -> CLEAN LAB (de-esser) -> saturation (4x oversampled) -> AIR LAB -> stutter -> echo -> Dimension -> gain -> Mix/Bypass crossfade against the delay-matched dry copy. The Instrumental input is measured only; it never reaches the output.

## Latency and CPU

Latency: 6 samples (the saturation oversampler) with and without CLEAN LAB, AIR LAB and a live Instrumental input; BEAT LOCK adds nothing to the audio path (the test compares the reported latency). Pitch Tune, when switched on, adds its own documented delay as before.
CPU, measured by the `Level interface / cost and latency` test (Release, one core of this Linux container, 10 s of synthetic stereo audio in 480-sample blocks, as a fraction of real time): default settings 0.37 %; CLEAN LAB + AIR LAB on, a live Instrumental input and a running analysis 2.38 %. The new audio-thread work for BEAT LOCK is one atomic update per block. These figures say nothing about a Mac, FL Studio or real session loads.

## UI behaviour

* The level-select card statuses and the persistent bar come from real state (see `docs/UI.md`). Opening a level never marks it complete, and no level is locked.
* The pixel meters (vocal, beat, home screen) draw only measured peaks. An input with no source is dimmed and empty; the beat bar is empty while nothing is connected.
* The band chart draws data only from a valid BEAT LOCK result; otherwise it says NOT ANALYSED.
* MOTION off: no page fade and no travelling dot; meters still follow the audio.

## Tests

`Tests/BeatLockTests.cpp` (engine): no beat, silent beat, not analysed, beat not analysed (no measurement quoted), a loud crowded beat (levels, BALANCE advice first, a presence move with reason), a sparse beat (no overlap moves), repeated analysis is identical, apply (only ticked items, ranges, listed changes, invalid result changes nothing), applied to the real processor and undone, parameter count still 65.
`Tests/LevelUiTests.cpp` (real editor): BEAT LOCK without a beat (message, no analysis started, no fake progress, vocal-only fallback opens BUILD); BEAT LOCK with a real second input (analysis starts on both analysers, STOP NOW appears while listening, one tick per adjustment, values shown, all-rejected apply changes nothing, apply moves real parameters, apply cannot stack, UNDO restores); cost and latency.
`Tests/MacroTests.cpp`: the level-select screen (four cards in order, none locked, statuses from real state, opening a level does not complete it), the persistent bar, every card opens its level, FINAL TOUCH categories, CLEAN LAB controls, BUILD end to end, undo, the chain opens FINAL TOUCH categories, UI preferences saved and ignored by presets, and a snapshot of every level at several sizes.
Results on Linux: fresh Release build in a new directory, 345 test groups, 0 failures; the same suite under AddressSanitizer + UndefinedBehaviorSanitizer, 0 reports. The earlier suites (CLEAN LAB, AIR LAB, MixEngine, Sources, state, presets, real-time allocation) all still pass. The VST3 target builds.

## Limitations

* Linux and synthetic signals only. No real vocal or real beat was listened to. Whether the suggestions sound better is **not** established; the manual procedure below is how to find out.
* The Instrumental input has never been verified in FL Studio (or any host). If FL Studio does not enable the extra bus, BEAT LOCK stays at NO BEAT INPUT and says so. The vocal-only workflow remains.
* Five coarse bands are not a spectrum analyser or LUFS. The masking figures are estimates.
* Analysis advances only while the host plays audio. With nothing playing, ANALYZE BEAT keeps waiting (up to the analyser's 30 s limit), then reports it could not analyse.
* Nothing is guaranteed about other hosts' sidechain behaviour (a silent unrouted input looks like a silent beat).
* The pixel font is upper case only and scales in whole pixels; at odd window sizes pixel edges can look slightly uneven.
* The EQ graph handles and some labels were restyled, not redesigned.
* No pluginval run (not installed here). No Windows or macOS build.

## FL Studio status

Not tested. The VST3 builds on Linux and the interface renders headlessly. Whether FL Studio shows the Instrumental sidechain input, how it routes to it, and how the window scales in its wrapper are all unverified.

## Manual listening procedure (Mac, FL Studio)

1. Build and install the plug-in (`docs/BUILDING.md`), rescan plug-ins in FL Studio, and insert GlitchKid on the vocal's mixer track.
2. Play 20-30 seconds of the vocal with the beat. In GlitchKid, open LEVEL 01 CLEAN LAB, switch it on, and keep playing for at least 10 seconds so it learns. Switch it off and on several times at the same playback position. Use OUTPUT TRIM or Gain so both versions are equally loud before judging. Listen for rumble, mud, harsh resonance and S sounds; note anything that sounds worse.
3. Route the beat to the plug-in's Instrumental input (see `docs/SOURCES.md`; the track's mixer send must go to the plug-in's sidechain input). Open LEVEL 02 BEAT LOCK. The status should change from NO BEAT INPUT to BEAT ARRIVING - READY TO ANALYZE and the BEAT meter should move with the beat. If it does not, stop: the routing is not working.
4. Press ANALYZE BEAT while the vocal and the beat play together. Wait for the suggestions. Read each reason. Untick any you do not agree with. Press APPLY SELECTED. Compare with UNDO (and with DRY / PROCESSED) at the same playback position.
5. Repeat on a different song. Write down which suggestions helped and which did not.
6. Check LEVEL 03 (BUILD MY VOCAL MIX) and LEVEL 04 (FINAL TOUCH) with the same vocal, and resize the window.
