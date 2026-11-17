# GLITCHKID BEAT LOCK

A separate VST3 plug-in that **processes the instrumental** to make frequency room for the vocal. The vocal arrives on the plug-in's sidechain input and is
**only listened to**: it never reaches the output, and nothing about the vocal (or the GlitchKid vocal plug-in) is changed.

```
INSTRUMENTAL ──► GLITCHKID BEAT LOCK (main in) ──► dynamic EQ ──► PROCESSED INSTRUMENTAL (main out)
VOCAL ──► sidechain send ──► "Vocal Sidechain" input ──► detection only
```

Not to be confused with the BEAT page of the GlitchKid vocal plug-in (suggestions for the vocal's own controls). Renaming that page to FIT TO BEAT is a
separate, unapproved task.

## Files

| File | Role |
|---|---|
| `Source/BeatLock/BeatLockParams.{h,cpp}` | The 41 parameter IDs, ranges and defaults (`bl_*`), the layout |
| `Source/BeatLock/BeatLockEngine.{h,cpp}` | The DSP: vocal activity, band detectors, conflict, dynamic bells, bypass fade |
| `Source/BeatLock/BeatLockAnalysis.{h,cpp}` | The analysis FIFO (audio → message thread) and LEARN / ANALYZE |
| `Source/BeatLock/BeatLockProcessor.{h,cpp}` | Buses, parameters, state, host bypass, display taps, sidechain status |
| `Source/BeatLock/BeatLockEditor.{h,cpp}` | The interface |
| `Tests/BeatLockPluginTests.cpp` | 14 test groups (in `GlitchKidTests`) |
| `CMakeLists.txt` | Target `GlitchKidBeatLock` (VST3, `PLUGIN_MANUFACTURER_CODE GlKd`, `PLUGIN_CODE GkBl`) |

Shared with GlitchKid, unchanged: `Analysis/DisplayTap`, `Analysis/EqAnalyzer`, `UI/PlaygroundWidgets` (look and feel, fonts, colours), `UI/Brand`, `UI/Theme`,
`GlitchKidAssets`. `ParametricEq` is linked only because the shared UI file needs it; Beat Lock's filters are its own (EQ LAB is untouched).

## Plug-in identity and buses

- Name `GlitchKid Beat Lock`, vendor `GlitchKid`, VST3 categories `Fx|EQ`. VST3 class ID `ABCDEF019182FAEB476C4B64476B426C` (GlitchKid's is `…476B6964`).
- Main bus: mono or stereo, input = output. Sidechain bus `Vocal Sidechain`: disabled, mono or stereo, **enabled by default**.
- Latency: 0 samples (nothing is buffered). Tail: 0.
- The host's bypass is `bl_bypass` (`getBypassParameter`).

## DSP (`BeatLockEngine`)

All decisions are made at fixed 16-sample control steps counted across blocks, so the output does not depend on the host's block size (tested bit-exact for
block sizes 1, 7, 64, 480, 1024 and 4096).

**Filters.** One TPT state-variable filter (Simper) per section. With `g = tan(πf/fs)`, `k = 1/Q`: band-pass (0 dB peak) `k·v1`, low-pass `v2`,
high-pass `x − k·v1 − v2`. The dynamic bell is `y = x + k(A² − 1)·v1` with `A = 10^(dB/40)` and `k = 1/(Q·A)`, whose response is
`(s² + kA²s + 1)/(s² + ks + 1)`: exactly the set gain at the centre (tested to 1e-9 dB), exactly unity at 0 dB, never a boost when cutting.

**Vocal activity (sidechain).**
- **Input:** the sidechain is summed to mono; non-finite samples become 0. It is band-limited to the speech range (HP 100 Hz, LP 10 kHz).
- **Level:** a power envelope with 5 ms attack and 25 ms release.
- **Adaptive noise floor:**
  - for the first 100 ms after a (re)connection the floor follows the level both ways, so constant hiss or room tone becomes the floor;
  - after that it falls fast (50 ms) and rises slowly (1.5 s, at most 10 dB/s);
  - it never goes below −100 dB.
- **Gate:**
  - opens above max(floor + 9 dB, −55 dBFS, recent peak − 30 dB);
  - closes below max(floor + 5 dB, −58 dBFS, peak − 36 dB), after an 80 ms hold;
  - the gate value is smoothed: 5 ms to open, 40 ms to close.
- **Voiced or unvoiced:** energy below 1 kHz against energy above 4 kHz, smoothed over 20 ms. Below 3 kHz, only voiced sound may drive a cut. S sounds and breaths can drive only the top bands.

**Band detectors.** Two cascaded band-pass sections (4th order) per band, about −22 dB 1.5 octaves away. They run on the vocal (mono) and on each
instrumental channel, each with a power envelope (10 ms attack, 100 ms release). The instrumental is stereo-linked: the power mean of the channels.

**Conflict (per band, every control step).** V = vocal band level, I = instrumental band level (dB).
- **Vocal relevance:** `r = clamp((V − max(Vmax − 20, −60)) / 10, 0, 1)`.
- **Instrumental presence:** `p = clamp((I − max(−60, Imax − 30)) / 12, 0, 1)`.
- **Excess:** `x = I − V + 6 + sensitivity`, through a 6 dB soft knee to give `y`.
- **Raw cut:**

  ```
  raw = −min(limit, amount · focus · 0.5 · y) · r · gate · voicing
  ```

  `limit` is the milder of the global and the band's maximum cut. `focus` is a smooth blend of LOW, MID and HIGH FOCUS around 500 Hz and 2.5 kHz.
- **Broad, not notchy:** a band where the vocal is also relevant takes at least 35 % of the sum of its neighbours' raw cuts.
- **Target:** `max(spread · p, −limit)`.
- **Ballistics:** ATTACK when a cut deepens, RELEASE when it recovers. A cut within 0.05 dB of 0, with nothing asking for more, finishes at exactly 0 dB.

**Dynamic EQ.**
- **Bells:** 8 bells in series. The gain is ramped per sample across each control step, so automation and cut changes don't click (tested).
- **Stereo:** one coefficient set for both channels, so the stereo image cannot move (tested: R = L/2 in gives R = L/2 out).
- **Idle bands:** a band at exactly 0 dB is not processed at all. With no sidechain, a silent vocal, AMOUNT 0, or after a release, the output is bit-identical to the input.
- **Bypass:** a 20 ms crossfade, then the input untouched.

## Parameters (all automatable, saved, restored)

| ID | Range | Default |
|---|---|---|
| `bl_amount` | 0–100 % | 50 |
| `bl_maxcut` | −12 … 0 dB | −4 |
| `bl_sensitivity` | −12 … +12 dB | 0 |
| `bl_focus_low` / `bl_focus_mid` / `bl_focus_high` | 0–100 % | 50 / 100 / 75 |
| `bl_attack` | 1–100 ms | 15 |
| `bl_release` | 30–1000 ms | 180 |
| `bl_bypass` | on/off | off |
| `bl_b1…8_on` | on/off | on |
| `bl_b1…8_freq` | 100–12 000 Hz | 180, 300, 500, 850, 1400, 2400, 4000, 7000 |
| `bl_b1…8_q` | 0.5–4 | 1.4 |
| `bl_b1…8_maxcut` | −12 … 0 dB | −12 (so the global MAX CUT governs) |

**State:** the parameter tree plus `bl_state_version = 1`.
- Unreadable or foreign data changes nothing.
- A missing parameter takes its default.
- Unknown values are ignored.

## Threads

| Audio thread | Message thread |
|---|---|
| The engine; display taps (`DisplayTap`); per-band readouts and status (atomics); the analysis FIFO (only while armed; overflow is dropped and counted, never waited for) | The 30 Hz interface: spectra (`EqAnalyzer`), the real cut curve (the bell formula at the engine's live cuts), status; LEARN / ANALYZE: drains the FIFO and analyses |

The audio callback is chunked to 512 samples with preallocated scratch. There is no allocation (tested on the audio thread), no lock and no file access.

## LEARN and ANALYZE (`BeatLockAnalysis`)

- **Capture:** a 4096-point Hann FFT with 50 % overlap of the vocal and the instrumental, over the same window.
- **Which frames count:** only those in which the audio thread judged the vocal active (at least 60 % of the frame's samples).
- **Measured per third-octave region (100 Hz – 12.7 kHz):**
  - the vocal's and the instrumental's average level;
  - how often the instrumental was within 6 dB of a relevant vocal region (a "conflict").
- **ANALYZE** (8 s of active vocal, 30 s timeout) shows that report and the strongest conflict regions. It changes nothing (tested).
- **LEARN** (6 s, 30 s timeout) proposes:
  - band frequencies at the most-conflicted regions (spaced ≥ 0.6 octave, then closer), then at the vocal's strongest regions, then kept from your settings, sorted;
  - a sensitivity from the measured median instrumental-minus-vocal gap.
- **Applying:** LEARN shows current → proposed, and changes nothing until APPLY. APPLY sets only the eight frequencies and the sensitivity (tested).
- **No sidechain:** LEARN and ANALYZE say so and capture nothing.
- **Less than 2 s of active vocal:** "Not enough vocal was heard", with no proposal.

## Interface

- **Top bar:** emblem, title, the real status, a vocal-key level meter, BYPASS and RESET. The status is one of:
  - `PROCESSING THE INSTRUMENTAL`;
  - `NO VOCAL SIDECHAIN — PASSING THROUGH`;
  - `VOCAL SILENT — PASSING THROUGH`;
  - `BYPASSED — PASSING THROUGH`.
- **Routing line:** `INSTRUMENTAL IN → BEAT LOCK → INSTRUMENTAL OUT` · `VOCAL = KEY ONLY`.
- **Graph:**
  - the instrumental's input and output spectra, and the vocal key's spectrum (dashed);
  - the real combined cut curve;
  - 8 band markers (drag for frequency, scroll for Q, double-click for on/off) with live cut values;
  - a VOCAL ACTIVE lamp.
- **Main controls:** AMOUNT, MAX CUT, SENSITIVITY, LOW, MID and HIGH FOCUS, ATTACK, RELEASE.
- **ADVANCED:** the eight-band editor (on, frequency, Q, max cut, live cut).
- **Panels:** LEARN and ANALYZE, plus HOW TO ROUTE (the FL Studio section is labelled **not yet verified**).
- **Resizing:** the window scales as a whole from 70 % to 150 % of 1120 × 680.

## Measured (Linux test machine; information, not a promise)

- **CPU:** 0.80–0.90 % of one core (stereo instrumental plus stereo sidechain, 48 kHz, 512-sample blocks, every band cutting).
- **Synthetic conflict test** (a sung-like key at 850 Hz over pink noise, AMOUNT 100 %, MAX CUT −6 dB):
  - 700–1000 Hz cut by about 7 dB;
  - 6–8 kHz −0.16 dB;
  - 120–200 Hz −0.46 dB.
- **Attack and release:** 63 % of the cut is reached about 25 ms after the vocal starts. After it stops, 90 % is recovered in about 0.8 s:
  - the gate closes once the level has fallen;
  - then an 80 ms hold;
  - then RELEASE 180 ms.
- **Vocal leakage:** none. A silent instrumental with a loud vocal gives exactly silent output. Around the vocal's frequency, the output is −150 dB under the instrumental's own tone.

## Not verified

- **Not tested in any DAW.** In particular, FL Studio's sidechain delivery to the `Vocal Sidechain` input is unverified.
- **No real recordings yet:** none of this has been checked against real vocals and real beats, or by listening. The constants above are starting values.
- **Not built on macOS or Windows here.** Windows builds only through the CI workflow, once it is pushed.
