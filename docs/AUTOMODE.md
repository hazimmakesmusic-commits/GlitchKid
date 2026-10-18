# AutoMode (v1): vocal analysis and automatic starting settings

AutoMode measures a dry vocal while it plays through GlitchKid and, on request, proposes values for GlitchKid's **existing** controls using one
profile, **ASTRO MODE** (spacious, atmospheric, modern melodic-rap *inspired*; a broad starting point, not a copy of anyone's chain).
It is a deterministic rules engine. There is no trained model, no cloud, no paid service.

## Architecture

| Piece | File | Runs on | Job |
|---|---|---|---|
| `VocalAnalyzer` | `Source/Analysis/VocalAnalyzer.{h,cpp}` | audio thread (live) or the caller's thread (`analyzeOffline`) | measures, publishes one `VocalAnalysis` |
| `automode::compute` | `Source/AutoMode/AutoMix.{h,cpp}` | message thread | pure function: analysis + profile + intensity + current sound -> proposed sound |
| `GlitchKidProcessor::previewAutoMode / applyAutoMode` | `Source/PluginProcessor.cpp` | message thread | reads ranges from the real parameters, applies through the normal parameter calls |
| AutoMode row | `Source/PluginEditor.cpp` | message thread | ANALYZE / STOP, status, INTENSITY, APPLY |

Flow: ANALYZE -> (host plays the vocal) -> analysis finishes (8 s of vocal, or 30 s total, or STOP) -> status "Ready" -> user sets INTENSITY -> APPLY.
Nothing is applied automatically and nothing changes while listening. After APPLY every control is an ordinary, editable parameter; use SET A first to compare.

## What is measured (`VocalAnalysis`)

Measured on the mono sum of the first two input channels, ignoring non-finite samples. "Active" = a 100 ms frame whose RMS is above -50 dBFS.

| Metric | Unit / method | Limits |
|---|---|---|
| `peakDb` | highest sample magnitude, dBFS | sample peak, not true-peak (no inter-sample peaks) |
| `rmsDb` | RMS over active frames, dBFS | digital level, not LUFS |
| `crestDb` | `peakDb - rmsDb` (3 dB = sine, speech ~12-20 dB, heavily compressed = low) | one loud peak raises it |
| `clippedSamples` | samples >= 0.999 full scale | a hard-clipped take shows many; one stray peak shows few |
| `nearClippedSamples` | samples >= 0.98 (includes the clipped ones) | "pressed against the ceiling" |
| `nonFiniteSamples` | NaN/Inf count; > 1 % of all samples = status `invalid` | |
| `dynP10/P50/P90Db`, `dynamicRangeDb` | percentiles of the 100 ms frame RMS over active frames (1 dB bins); range = P90 - P10 | an indicator of how much the level moves, not a loudness-range standard |
| `bandFraction[5]` | share of active energy below 120 Hz / 120-500 / 500-2 k / 2-5 k / above 5 kHz | 2nd-order Butterworth low-passes, band = difference of neighbours: coarse, leaks into neighbours |
| `sibilanceIndex` | share of active frames where the > 5 kHz energy exceeds half of the 500 Hz-5 kHz energy | **an estimate only**; bright or breathy vocals and hi-hats also score high |
| `activeSeconds`, `totalSeconds`, `status` | `ready`, `tooShort` (< 2 s active), `tooQuiet`, `invalid`, `listening`, `idle` | |

## What it cannot determine

Musical key or scale, note-level pitch or pitch stability (GlitchKid has no standalone pitch detector that is verified for this), vocal gender or genre,
true sibilance, the room, or what "the ideal mix" is. Band shares from one recording never prove a tonal problem. The numbers only steer conservative choices.

## How measurements steer ASTRO MODE (`intensity` i = 0..1; below 0.05 changes nothing)

Only these controls are written: Pitch Tune on/strength/speed, EQ (all four bands), compressor (all six), saturation, Liquid Echo (on, sync, division, feedback, tone, width, mix),
Dimension (all five), Doubler (all five). **Never touched:** Gain, Mix, Bypass, key, scale, FORMANT, Mutation, Harmony, Stutter, Alter Ego. Every value is clamped to the real parameter range and snapped to its step.

* **Pitch Tune** on when i >= 0.15; strength `60 + 25i`; speed `30 - 18i` ms. Not analysis-driven.
* **Low shelf** `+1.5i` dB, but 0 when the < 120 Hz share > 0.30.
* **400 Hz bell** `-2i` dB; `-3.5i` when the < 120 Hz share > 0.30 or the 120-500 Hz share > 0.55.
* **3 kHz bell** `+2i` when the 2-5 kHz share < 0.06; `-1i` when > 0.22; else `+0.5i`.
* **8 kHz shelf** 0 when the > 5 kHz share < 0.015 (already dark); `-4.5i` when `sibilanceIndex` > 0.35; else `-3i`.
* **Compressor** threshold = median frame level - 3 dB (kept in -40..-12); ratio blended from 1:1 toward 2 / 3 / 4 for a dynamic range < 8 / 8-18 / > 18 dB; attack 12 ms, release 120 ms, knee 8 dB;
  makeup = half the estimated gain reduction at the P90 level (<= 6 dB).
  **Guards:** level < -40 dB -> at most 2:1 and no makeup; dynamic range < 6 dB **and** crest < 9 dB (already compressed) -> 1.5:1 and no makeup.
* **Saturation** `4i` dB; 0 when clipped samples >= 3, near-clipped >= 10 or peak > -0.5 dB; halved when the > 5 kHz share > 0.12.
* **Atmosphere** (no reverb exists; delay + width stand in): Echo 1/4 dotted, feedback `40a`, mix `28a`, tone 2.5 kHz, width 70 %; Dimension mix `35a`; Doubler amount `35a`, width 80 %.
  `a = i`, or `0.6i` when the 2-5 kHz share < 0.06 (words may be hard to hear).

**Fallbacks.** A missing/non-finite metric is replaced by a typical-vocal value and noted (missing dynamics -> compressor fixed at -18 dB, 2.5:1, no makeup; broken band shares -> no tonal decisions).
If the whole analysis is unusable, `compute` changes nothing by default; with `allowFallback = true` it proposes the same typical-vocal defaults and says "not measurements of this recording".
(The window uses the default: it never applies a proposal that is not based on a finished analysis.)

## Thread safety and real-time design

* The audio thread only runs `VocalAnalyzer::process` while listening: a fixed ~4 biquads, a few multiply-adds and a 81-bin histogram per sample/frame; no allocation, locks, I/O or FFT (verified by an allocation-counting test).
  This is the "streaming" design: a constant-cost running measurement instead of storing the recording, so no full-buffer analysis ever happens on the audio thread. `analyzeOffline` is for whole buffers on another thread.
* The UI starts/stops through one atomic request; the result crosses as atomics, the status written last with release ordering and read first with acquire ordering.
* Applying happens on the message thread through ordinary parameter calls (host gestures + each effect's own smoothing). Pitch Tune switching on changes the reported latency (about 31 ms).

## Build and test

```
cmake -S . -B /tmp/gk-build -DGLITCHKID_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/gk-build -j 8
/tmp/gk-build/GlitchKidTests_artefacts/Release/GlitchKidTests
```
Tests: `Tests/AutoModeTests.cpp` (synthetic signals: silence, empty, tone, quiet vocal-like, near-clipping, bright, wide dynamics, non-finite; determinism; ranges; fallbacks).
They show the measurements and rules behave as documented. They do **not** show that the result sounds good.

## Before a real-world listening test

Mac build + FL Studio: analyse several real dry vocals (male/female, quiet/loud, dark/bright), compare APPLY at 40/60/80 % with SET A, and note any rule that misfires.
Not yet done: reverb-like atmosphere (needs a reverb DSP), pitch/key-aware settings, further profiles.
