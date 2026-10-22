# Output limiter and bounded recommendations: implementation

Linux, synthetic signals only. Nothing here was tested in FL Studio, on a Mac or on Windows, pluginval is not installed here, and nothing was listened to. The audit that preceded this work is `docs/OUTPUT_LIMITER_QUALITY_AUDIT.md`.

## What was changed

1. **A real output limiter** (`Source/DSP/OutputLimiter.{h,cpp}`), optional, bypassable and off by default, the last stage of the output path.
2. **Meters fixed.** The input and output peak meters read channel 0 only; they now read the louder channel. The output peak is taken **after** the limiter, so the meter, the CLIPPING warning, the chain strip and the OUT read-out show what actually leaves the plug-in.
3. **BEAT LOCK and beat-informed BUILD are bounded** (`Source/AutoMode/SourceRelationship.*`, `BeatLock.cpp`, `MixEngine.cpp`).
4. **FINAL TOUCH > DYNAMICS** gets an OUTPUT LIMITER section (switch, ceiling, drive, release, a real gain-reduction bar, a post-limiter sample-peak read-out).

Files: `Source/DSP/OutputLimiter.{h,cpp}` (new), `Source/PluginProcessor.{h,cpp}`, `Source/PluginEditor.{h,cpp}`, `Source/UI/PlaygroundWidgets.h`, `Source/AutoMode/SourceRelationship.{h,cpp}`, `Source/AutoMode/BeatLock.cpp`, `Source/AutoMode/MixEngine.cpp`, `Source/Presets/PresetController.cpp`, `CMakeLists.txt`, tests (`Tests/OutputLimiterTests.cpp` new; `LevelUiTests`, `AlterEgoTests`, `FactoryPresetTests` updated), docs.

## The limiter: algorithm

Look-ahead **sample-peak** limiter, one gain linked across both channels (the stereo image does not move). The look-ahead is 1.5 ms (72 samples at 48 kHz, 16 to 256 at any rate).
For each input sample (after the drive) the gain that would bring it to the ceiling is `ceiling / peak` (1 below the ceiling). The smallest of these over a window of `lookahead + 1` samples is taken, the window is averaged (a smooth ramp that reaches the needed gain exactly when the peak leaves the delay line), and the gain may rise only at the release rate: `gain = min (ramp, previous gain recovering toward 1)`. The delayed audio is multiplied by that gain. Since the gain is never above what any sample in the window needs, every output sample is at or below the ceiling; a final clamp covers float rounding only. Cost is a fixed 2 x 73 operations per sample, with fixed arrays (no allocation, no locks, no I/O); non-finite input is treated as 0.

* **Placement:** after the Mix/Bypass crossfade, i.e. after Pitch Tune ... compressor, saturation, AIR LAB, echo, Dimension, output Gain and the dry/processed blend. It also runs on the DRY (bypass) return path, so level and delay are consistent when you compare; switch the limiter off for a pure dry.
* **Latency:** while the limiter is **on** it delays the output by its look-ahead and that is added to the reported latency (the same 10 Hz message-thread update that reports the Pitch Tune delay); while it is **off** it adds nothing and the reported latency is exactly what it was. The on/off switch is a 20 ms crossfade between the undelayed signal and the limited, delayed one, so toggling does not click; it is not meant to be automated rapidly. During that crossfade the output may briefly exceed the ceiling.
* **Sample peak, not true peak.** The ceiling is enforced on samples. Between samples a reconstructed (D/A or oversampled) waveform can be higher. Measured offline with 8x windowed-sinc interpolation: a worst-case sine at a quarter of the sample rate overshoots the ceiling by 3.01 dB in true peak; a voice-like signal 18 dB over the ceiling overshoots by 0.01 dB. **No true-peak protection is claimed**; keep the ceiling a little below 0 dBFS (default -1 dBFS, range -12 to -0.1 dBFS, so it cannot be set to full scale).
* **It is not a substitute for gain staging.** Drive is not compensated afterwards; the DYNAMICS page says to set the vocal level with Gain, the compressor makeup and AIR LAB trim first.

## New parameter IDs (appended after `clean_ess`, so every existing host index is unchanged; 69 parameters)

| ID | Range | Default |
|---|---|---|
| `lim_on` | off / on | off |
| `lim_ceiling` | -12 to -0.1 dBFS | -1.0 |
| `lim_drive` | 0 to 24 dB | 0 |
| `lim_release` | 10 to 500 ms | 80 |

They are part of a sound (A/B, user presets) and of the saved state. A profile (LOAD PROFILE) and BUILD MY VOCAL MIX never change them. An older session without them loads with the defaults (limiter off), so it sounds and reports latency exactly as before.

## Bounded recommendations: what was wrong and the chosen limits

* **The screenshot case.** "PRESENCE BELL (3 kHz) 10.2 -> 11.5 dB" had correct units and mapping (dB of `eq_b2_gain`, range +-15). The 10.2 dB was already in the user's EQ (profiles never exceed +3 dB; BUILD clamps to +-6 dB) and BEAT LOCK added a relative +1.3 dB with no ceiling on the result, and without looking at the vocal itself.
* **Bounds now.** Each beat move carries an absolute limit on its **result**: the presence bell is raised by at most 2.5 dB per analysis and never above **+4 dB in total**; the body bell and the low shelf are cut by at most 1 to 1.5 dB per analysis and never below **-6 dB in total**. A value already beyond a limit is left alone (not pulled back): at 10.2 dB the presence suggestion shows "no change". Echo and width amounts are only ever reduced. The same function (`applyBeatMove`) is used by BEAT LOCK and by BUILD, and BUILD still clamps every EQ band to +-6 dB.
* **No brightening of a harsh vocal.** The presence boost is withheld when the vocal's own measurements say it is already bright or sibilant (sibilance index above 0.35, or more than 12 % of its energy above 5 kHz: the same thresholds BUILD already used for "S sounds look strong" and "already bright"). The interface then shows a PRESENCE row, not applicable, with the reason ("NOT raised: ...") instead of a change.
* **Why these numbers.** They are conservative engineering choices for a starting point, not scientific constants: a few dB of broad presence is a normal vocal move, while a +10 dB bell is an extreme tone change that a heuristic estimate from five coarse bands should never make. They can be changed in `beatMoves()`; anything beyond them is still available by hand in FINAL TOUCH.
* Every suggestion still has a measured reason, is shown with its real current and proposed value, can be unticked, is applied once per analysis and is undone with UNDO. Nothing is applied automatically.
* CLEAN LAB's own bounds are unchanged (low-mid bell 3.5 dB, 6 dB per resonance, 9 dB of de-essing) and were re-tested on dark, sibilant, noisy and silent input.
* Graphs: the EQ graph and CLEAN LAB overlay are computed from the real filter formulas and compared with the real DSP by existing tests (0.15 dB and 0.7 dB); the spectrum behind them is the real FFT of the input. Nothing is drawn that is not measured or calculated from the real settings.

## Not changed (and why)

* **Dry/processed loudness is not matched.** DRY | PROCESSED is the real bypass with no level compensation. A louder processed signal tends to sound better; match it by hand with Gain or AIR LAB trim. Automatic matching would need a loudness measurement on both paths; not built.
* No reverb, noise reduction, or true-peak limiting exists or is claimed.

## Tests (Linux)

`Tests/OutputLimiterTests.cpp`: containment at 44.1 / 48 / 96 kHz and ceilings -0.1 to -12 dB (loud noise, bursts, impulses, mono); transparency below the ceiling (a delayed copy bit for bit) and recovery after an impulse; sustained loud sine held at the ceiling without pumping; stereo gain linking; silence, NaN and Inf; on/off crossfade has no click; parameters moving while it runs; sample peak versus true peak (measured and logged); latency at five sample rates and CPU; in the real plug-in: 69 parameters with the four new ones appended last, off by default, reported latency unchanged when off and increased when on, the ceiling cannot reach 0 dBFS; a hot chain (+12 dB gain, makeup, saturation, CLEAN LAB, AIR LAB) really exceeds full scale without the limiter and stays under the ceiling with it, in mono and stereo, DRY and PROCESSED; both-channel and post-limiter meters and the gain-reduction meter; save/restore, an older state without the limiter parameters, A/B and presets, and BUILD/profiles leaving them alone; no allocation over 400 blocks with the limiter and CLEAN LAB on and DRY toggled; the bounds on presence and cuts from many starting values (including 10.2 dB); no presence boost for bright or sibilant vocals in BEAT LOCK and in BUILD; measured low / mid / high / dark / sibilant / noisy / silent vocals; CLEAN LAB limits on the same kinds of input. `Tests/LevelUiTests.cpp`: the limiter switch and knobs in the real editor are the real parameters, and the peak read-out is post-limiter and says CLIPPING only when the output really clips.

## Results

| Category | Result |
|---|---|
| Compiled and tested | Fresh Release build in a new directory outside `build/`, the full suite, and the same suite under AddressSanitizer + UndefinedBehaviorSanitizer: see the final report. The VST3 builds. |
| DSP verified offline | Containment, transparency, latency, crossfade continuity, non-finite handling, sample-rate coverage, true-peak overshoot measurement, CPU (about 0.6 % of one core for the limiter alone, stereo 48 kHz) on synthetic signals. |
| Listening-tested | **No.** Nobody has listened to the limiter. Release, drive and the 1.5 ms look-ahead are untested by ear (a short look-ahead can sound edgy on sustained bass; the 80 ms release default is a first guess). |
| Verified in FL Studio | **No.** Host latency compensation for the on-demand look-ahead is untested. |
| Not verified | pluginval (not installed), macOS and Windows builds, real vocals and mixes, true-peak behaviour in a D/A chain, automating the limiter switch. |

## Remaining work

Listening tests; consider true-peak detection (oversampled detector) if real material shows inter-sample overs; consider loudness-matched dry/processed audition; consider a per-band cap on BUILD's profile EQ if profiles are changed.

## FL Studio verification steps (manual)

1. Install the new build (`docs/BUILDING.md`), rescan, put GlitchKid on a vocal track, play a loud vocal.
2. FINAL TOUCH > DYNAMICS: with the limiter OFF raise OUTPUT GAIN until the top bar says CLIPPING (also confirm on FL's own mixer meter). Switch the limiter ON with the ceiling at -1 dB: CLIPPING should stop, the gain-reduction bar should move, and FL's meter should not pass about -1 dB. Note any difference between GlitchKid's reading and FL's.
3. Latency: FL's plug-in delay compensation should shift by about 1.5 ms when the limiter is switched on (check against a duplicate track). Listen for any flam or phasing when toggling during playback.
4. Listen at several ceilings, drives and releases on a loud vocal and on a vocal with a bass-heavy beat; judge pumping, edginess and dulling.
5. DRY | PROCESSED with the limiter on and off. Save and reopen the project; the limiter settings must return.
6. BEAT LOCK: with your own EQ already boosted near 3 kHz, the PRESENCE suggestion should show no change above +4 dB, and no presence boost for a harsh or sibilant vocal.
