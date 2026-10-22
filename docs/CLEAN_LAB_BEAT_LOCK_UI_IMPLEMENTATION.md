# CLEAN LAB, BEAT LOCK and the Experimental Lab interface: implementation

Linux, synthetic signals only. Nothing here was tested in FL Studio, on macOS or on Windows, pluginval is not installed on the build machine, and nothing was listened to with a real vocal. Read the limitations before trusting any claim. The audit is `docs/CLEAN_LAB_BEAT_LOCK_UI_AUDIT.md`.

## What was changed in this pass

The DSP (CLEAN LAB, the dynamic resonance suppressor, the de-esser, BEAT LOCK analysis) already existed and was tested in the two previous milestones. The audit confirmed it against the source, so it was **not rebuilt or duplicated**. This pass:

1. Replaced the rejected 8-bit level-select interface with the **Experimental Lab** interface: five workspaces (HOME, CLEAN LAB, BEAT LOCK, BUILD MY VOCAL MIX, FINAL TOUCH) behind a compact persistent navigation bar; sharp chamfered panels, bold tracked typography, a restrained neon glow, thin header scanlines, a very short glitch on workspace changes (off with MOTION).
2. Added a **real-time spectrum and waveform pipeline** (audio taps, message-thread FFT) and used it on HOME (vocal input, output, and the instrumental only when live), behind the CLEAN LAB and TONE EQ graphs (vocal spectrum) and on BEAT LOCK (vocal and live beat).
3. Reorganised CLEAN LAB (simple controls first; the section trims behind ADVANCED) and BEAT LOCK (a spectrum view added).

## Files

New: `Source/Analysis/DisplayTap.h`, `Source/Analysis/DisplaySpectrum.{h,cpp}`, `Source/UI/LabKit.{h,cpp}`, `docs/CLEAN_LAB_BEAT_LOCK_UI_IMPLEMENTATION.md`.
Removed (my own, uncommitted-era files from the rejected direction): `Source/UI/PixelKit.{h,cpp}`.
Modified: `Source/PluginProcessor.{h,cpp}` (three taps), `Source/PluginEditor.{h,cpp}` (workspaces, navigation, displays), `Source/UI/PlaygroundWidgets.{h,cpp}` (back to vector drawing; sharp chamfers, a glow helper, the EQ graph spectrum underlay), `CMakeLists.txt`, tests (`Tests/LevelUiTests.cpp`, `Tests/MacroTests.cpp`), `docs/UI.md`, `docs/CLEAN_LAB_BEAT_LOCK_UI_AUDIT.md`, `docs/CLEAN_LAB_BEAT_LOCK_IMPLEMENTATION.md` (marked superseded for the UI), `CHANGELOG.md`.

## DSP techniques (unchanged, summarised)

* CLEAN LAB (`Source/DSP/CleanLab.*`): a 2048-point FFT side-chain (hop 512) with a movement-weighted long-term spectrum. Corrective EQ: a 70 Hz high-pass blended in only for measured rumble, one wide low-mid bell (<= 3.5 dB). Dynamic resonance suppression: up to three narrow bells (<= 6 dB each) from a persistent-excess detector that learns only while the voice moves, with slow slot movement. De-esser: a sibilance-to-body ratio drives a wide 7.5 kHz bell (<= 9 dB x depth, ratio 4, 0.5 ms attack, 40 ms release). Zero latency. The CLEAN amount scales every part, not the output level. No noise, room, click, breath or plosive reduction is claimed or present.
* BEAT LOCK (`Source/AutoMode/BeatLock.*`, `SourceRelationship.*`): analyses the vocal and the Instrumental input independently (peak, RMS, five broad bands), compares them (level difference, masking estimate per band, low-end and loudness flags), and proposes bounded changes to the vocal's own EQ, echo and width with a reason each. BALANCE is advice only.

## Signal-chain order (unchanged)

input -> NaN/Inf zeroing -> Pitch Tune -> Formant -> Harmony -> Mutation -> Doubler -> user EQ -> CLEAN LAB (corrective + resonance) -> compressor -> CLEAN LAB (de-esser) -> saturation (4x oversampled) -> AIR LAB -> stutter -> echo -> Dimension -> gain -> Mix/Bypass crossfade. The reason for CLEAN LAB's place: it corrects before the compressor so the compressor does not push resonances and mud up, and the de-esser sits after it so it acts on the level the compressor produced. Reordering stages was not done.

## New parameter IDs

None. The parameter count is 65 (tested). New saved UI preference (not a parameter, not part of a sound, preset or A/B): `uiCleanAdv` (the ADVANCED switch). `uiLevel` keeps its 0-4 meaning with 0 = HOME. Older states without them load with the defaults (HOME, advanced hidden).

## Visualization: data source and thread safety

* **Source.** Three `DisplayTap`s in the processor: the vocal input **after the non-finite clean-up and before any processing**, the output **after processing** (both the processed and the dry/bypass return paths), and the Instrumental input (only filled while that bus is enabled). Samples are the mono average of the first two channels.
* **Transfer.** Each tap is a fixed 8192-sample ring of relaxed `std::atomic<float>` with one release-published write index (one writer, one reader). `push()` on the audio thread does no allocation, locking, waiting or I/O and stores 0 for a non-finite sample. The UI copies the newest samples with `copyLatest()`. At the very edge of the window a copy may mix old and new samples; that is harmless for a display and the data is never used for processing. The existing real-time allocation test still passes.
* **Analysis.** All of it on the message thread at the editor's 30 Hz timer, only while a view that needs it is visible: a Hann-windowed 2048-point FFT folded into 96 log-spaced columns (20 Hz to the lower of 20 kHz and 0.45 x the sample rate), 0 = -90 dBFS to 1 = -6 dBFS (a sine of amplitude A reads about A), fast attack, fixed-rate fall. The waveform is min/max per column of the newest 2400 samples. Non-finite samples are zeroed; silence (below about -90 dBFS) draws nothing and the view says NO SIGNAL.
* **Labels.** VOCAL INPUT = this track's input before processing; OUTPUT = after processing; INSTRUMENTAL INPUT = the aux bus as received, shown only when `getInstrumentalStatus()` is `live` and it carries signal. If the beat is not routed, no instrumental trace exists. A mixed stereo track is shown as the vocal input and is never split.
* **Sample rates and buffers.** The tap is independent of the block size; the column frequencies follow the host sample rate (tested at 44.1, 48 and 96 kHz).

## Latency and CPU

Reported latency 6 samples with and without CLEAN LAB, AIR LAB and a live Instrumental input (Pitch Tune adds its own documented delay when on). CPU, from `Level interface / cost and latency` (Release, one core of this Linux container, 10 s of synthetic stereo audio in 480-sample blocks, as a fraction of real time): default settings 0.3-0.6 %; CLEAN LAB + AIR LAB on, a live Instrumental input and a running analysis 1.5-3 % (two runs; the container is shared, so the figures move with its load). The taps add one relaxed store per sample per tap. The display work is on the UI thread and measured only through the tests below, not separately. These figures say nothing about a Mac, FL Studio or a real session.

## Tests run (Linux)

* Fresh Release build in a new directory outside `build/`; the full suite; the same suite under AddressSanitizer + UndefinedBehaviorSanitizer; the VST3 target builds (results in the final report below).
* New: display taps and spectrum (wrap and order, a 1 kHz sine at 44.1 / 48 / 96 kHz peaks at its own frequency, far columns low, silence draws nothing, NaN / Inf are zeroed); the real plug-in's taps (a +12 dB EQ boost appears in the output trace and not the input trace; mono layout; no beat tap without a second input); workspace navigation (every nav button, resizing, the sound and preset name unchanged, the workspace remembered across a state save and restore, a missing preference opens HOME); renders of HOME with and without a signal and CLEAN LAB with a signal.
* Updated: HOME tiles (four, in order, none locked, statuses from real state, opening a workspace never completes it), the persistent bar, every workspace opens its own controls, CLEAN LAB controls behind ADVANCED, BUILD end to end, undo, the chain opens FINAL TOUCH categories, UI preferences.
* Unchanged and still passing: CLEAN LAB (resonances, sibilance, vowels, quiet and loud, silence, mono/stereo, bypass, amount law, state, interplay with EQ, AIR LAB, pitch and BUILD, non-finite input), BEAT LOCK engine and editor flow (no beat, silent beat, levels, overlap, repeatability, apply, undo), MixEngine, Sources, presets, A/B, state, real-time allocation.

## Known limitations

* No real vocal or real beat was auditioned; whether CLEAN LAB's corrections and BEAT LOCK's suggestions sound better is **not established**. The CLEAN LAB detectors were tuned on synthetic voices.
* The Instrumental input is unverified in FL Studio (and any host). If the host does not enable the bus, BEAT LOCK stays at "no beat input" and the vocal-only workflow remains.
* The five-band masking figures are estimates; digital levels are not LUFS; nothing is automatic source separation.
* The spectrum is a display (2048-point FFT, 96 columns), not a measurement tool, and updates at 30 Hz.
* No reverb exists, so none is shown or recommended; SPACE is echo, width and doubling.
* The hidden creative effects have no controls (kept only for old sessions).
* The EQ graph handles and the dial/slider drawings were sharpened only slightly; the look was judged from headless renders, not on a real screen or in a host window.
* No pluginval, Windows or macOS build here.

## FL Studio validation status

Not done. The VST3 compiles and the editor renders headlessly. Whether FL Studio exposes the Instrumental sidechain, how it routes to it, and how the window behaves in its wrapper are all unverified.

## Real-vocal listening-test plan

1. Build and install (`docs/BUILDING.md`); insert GlitchKid on a real lead-vocal mixer track in FL Studio on the Mac. Open HOME and play the vocal: the VOCAL INPUT trace and waveform should move with it, and VOCAL IN / OUT should read sensible levels. Watch for any mismatch between what you hear and the display.
2. CLEAN LAB. Use three vocals: a clean one, a boxy / muddy one, a harsh and sibilant one. Switch CLEAN LAB on and play for at least 10 seconds so it learns. Switch it off and on at the same position, at **matched loudness** (use Gain or AIR LAB TRIM). Judge: rumble, mud, harsh resonances, S sounds, and whether the voice lost body, sounds dull, lisps, or gets metallic. Try CLEAN at 25, 50 and 100 %. Note anything that sounds worse.
3. BEAT LOCK. Route the beat to the Instrumental input (`docs/SOURCES.md`). The BEAT meter and the instrumental trace should move with the beat; if not, stop (the routing is not working). ANALYZE BEAT with vocal and beat together, read each reason, untick what you disagree with, APPLY SELECTED, compare with UNDO at the same position. Repeat on three songs and record which suggestions helped.
4. BUILD MY VOCAL MIX with and without the beat; compare, then continue in FINAL TOUCH.
5. Save a session, reopen it, confirm all parameters and the workspace are restored; resize the window; automate a CLEAN or EQ parameter from FL Studio and confirm the controls follow it.
