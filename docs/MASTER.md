# GLITCHKID MASTER

A separate VST3 plug-in for mastering a **finished stereo mix**. It goes last on the DAW's master bus and processes the audio the host plays through it.
It is not the GlitchKid vocal plug-in and not GLITCHKID BEAT LOCK: it has its own target, processor, parameters, state and interface, and it never
reads or changes another plug-in's parameters.

## Files

| File | Role |
|---|---|
| `Source/Master/MasterParams.{h,cpp}` | The 9 parameter IDs (`gkm_*`), ranges, defaults, the layout |
| `Source/Master/MasterProcessor.{h,cpp}` | Buses, chunked processing, NaN/Inf guard, INPUT trim, bypass, A/B, state, live peaks for the meters |
| `Source/Master/MasterEditor.{h,cpp}` | The interface |
| `Source/Mastering/MasteringEngine.{h,cpp}`, `MasterDsp.{h,cpp}` | The mastering DSP, **shared** with the vocal plug-in's old Mastering mode (reused, not rewritten) |
| `Tests/MasterPluginTests.cpp` | 14 test groups ("GlitchKid Master (plug-in)"); the DSP's own tests are in `Tests/MasteringTests.cpp` |
| `CMakeLists.txt` | Target `GlitchKidMaster` (VST3, `PLUGIN_MANUFACTURER_CODE GlKd`, `PLUGIN_CODE GkMs`, categories `Fx Mastering`) |

## Identity and buses

- **Name:** `GlitchKid Master`, vendor `GlitchKid`. Plug-in code `GkMs`, unique within `GlKd` (GlitchKid is `Gkid`, Beat Lock `GkBl`).
- **Buses:** stereo, or mono, with input = output. Other layouts are refused.
- **Latency:** constant for a sample rate, reported to the host. It is the saturation's 6 samples, plus the limiter's 2 ms look-ahead, plus its
  true-peak detector's 8 samples. That makes 110 samples at 48 kHz.
- **Bypass:** the host's bypass is `gkm_bypass`. It plays the untouched input, delayed by exactly the latency and crossfaded over 20 ms (tested
  bit-exact).

## Signal chain

```
in -> NaN/Inf guard -> INPUT trim -> input meter -> EQ (style + TONE) -> glue compressor -> saturation (4x oversampled)
   -> width (mid untouched) -> true-peak limiter (LOUDNESS = drive, CEILING) -> OUTPUT trim -> output meter -> bypass crossfade -> out
```

Why this order (it is the existing engine's order, kept):
- **EQ before the compressor**, so the glue reacts to the shaped mix.
- **Saturation after the glue**, so it colours a controlled level. It is oversampled 4x, so its harmonics do not alias.
- **Width after saturation**, so the colour's harmonics are placed with the rest of the mix. The mid, and so the mono sum, is never changed.
- **The limiter last.** Only OUTPUT follows it, and OUTPUT can only lower the level, so the ceiling always holds.

There is no separate "loudness" stage. LOUDNESS is the drive into the limiter, and the meters show the real result. A target number is never shown as
achieved.

## Mastering DSP (reused from `Source/Mastering`)

- **Loudness:**
  - ITU-R BS.1770-4 / EBU R128 K-weighting, re-derived for any sample rate;
  - momentary (400 ms), short-term (3 s) and integrated (absolute gate −70 LUFS, relative gate −10 LU);
  - tested against the standard's sine and gating cases at 44.1, 48 and 96 kHz.
- **Glue compressor:** its threshold follows the mix's own integrated loudness, so quiet and loud mixes are treated alike.
- **Styles:** these are real processing recipes (EQ, glue, saturation, width, extra drive), tested to differ. INTENSITY scales each style.
  - **Clean & Balanced:** transparent: a little low-mid tidy-up, a touch of air, gentle 1.5:1 glue, no colour.
  - **Warm & Smooth:** fuller lows, softer highs, slow 2:1 glue and light saturation.
  - **Loud & Punchy:** tighter low-mids, more bite, 2.5:1 glue with a slower attack, extra drive into the limiter.
  - **Wide & Atmospheric:** open air on top and a wider side (never below about 120 Hz), with soft glue.
- **Width:**
  - mid/side, with the mid untouched;
  - only the side above about 120 Hz is widened;
  - the side is capped at 90 % of the mid's energy, so a mix cannot be pushed out of phase.
- **True-peak limiter:**
  - look-ahead, linked across channels;
  - the gain is computed from an oversampled detector, so peaks between samples are caught;
  - the gain is smooth (a window minimum and a moving average over the look-ahead), with a style-dependent release.
- **ORIGINAL (compare):** the untouched mix, time-aligned, played at the master's measured short-term loudness, and never above the ceiling, so the
  louder version never wins.

**Changed for this plug-in, in the shared DSP:**
- **The ceiling is a control:** `Controls::ceilingDbTp`, from −6 to −0.3 dBTP, default −1. The vocal plug-in's Mastering mode keeps the default.
- **A finer true-peak detector:** 8x oversampling, 16 taps per phase (it was 4x with 12 taps per phase, the BS.1770 example). The reason was a check
  with an exact FFT reconstruction on 18 kHz band-limited noise, at a −1 dBTP ceiling:
  - the old detector let the output reach −0.84 dBTP;
  - the new one −0.96 dBTP.

  This also changes the vocal plug-in's old Mastering mode: a slightly more accurate limiter and meters, and 2 more samples of latency, which it
  reports to the host.

## Parameters (all automatable, saved, restored)

| ID | Range | Default |
|---|---|---|
| `gkm_style` | Clean & Balanced, Warm & Smooth, Loud & Punchy, Wide & Atmospheric | Clean & Balanced |
| `gkm_intensity` | 0–100 % | 60 |
| `gkm_input` | −12 … +12 dB | 0 |
| `gkm_loudness` | 0–10 dB drive | 3 |
| `gkm_tone` | −100 (darker) … +100 (brighter) | 0 |
| `gkm_width` | 0–200 % | 100 |
| `gkm_ceiling` | −6 … −0.3 dBTP | −1 |
| `gkm_output` | −12 … 0 dB | 0 |
| `gkm_bypass` | on / off | off |

**State:** the parameters, `gkm_state_version = 1`, and the A and B slots.
- Unreadable or foreign data changes nothing.
- A missing parameter takes its default.
- ORIGINAL is a listening switch: it is never saved, and closing the window turns it off.

## Interface

- **Top bar:** the mark, the status, A/B (SET A, SET B, A, B), BYPASS, RESET. The status is built from real data:
  - MASTERING THE MIX;
  - NO AUDIO;
  - SILENCE ON THE INPUT;
  - BYPASSED;
  - ORIGINAL with the matching gain;
  - a warning when NaN/Inf samples arrived.
- **INPUT and OUTPUT:**
  - L/R peak bars, with the ceiling marked on the output;
  - momentary, short-term and integrated LUFS;
  - true peak, sample peak, correlation and clipped samples;
  - "-" where a reading is not available yet (integrated needs 400 ms above −70 LUFS).
- **STYLE:** the four styles and what the selected one really does.
- **GAIN REDUCTION:** the limiter and the glue compressor over the last 8 seconds.
- **CONTROLS:** INPUT, INTENSITY, LOUDNESS, TONE, WIDTH, CEILING, OUTPUT.
- **COMPARE:** ORIGINAL / PROCESSED, the current matching gain and the latency. RESET METERS starts the measurements again.
- **HOW TO EXPORT:** there is **no Export button**. The plug-in only sees what the host plays through it, so the master is exported by the DAW, and the
  panel gives the steps.

## Measured (Linux test machine; information, not a promise)

- **CPU:** 2.4 % of one core (stereo, 48 kHz, 512-sample blocks, Loud & Punchy at full intensity).
- **The ceiling, checked by an independent 16x reconstruction:** at most 0.03 dB over, at −1, −3 and −0.3 dBTP, on 18 kHz band-limited, hot,
  transient material with a loud tone near a quarter of the sample rate.
- **Block sizes:**
  - 64 to 8192 samples, including blocks larger than announced: within −94 dBFS of 512-sample processing;
  - 1 and 7 samples: within −63 dBFS (the glue threshold is updated per processed block).
- **No allocation on the audio thread** (tested at 512, 64 and 3000 samples).

## Limits and not verified

- **Not loaded in any DAW yet.** FL Studio, latency compensation in a real host and real mixes all still need a listening test.
- **Material with strong energy right up to Nyquist exceeds the ceiling between samples:** white noise plus full-scale Nyquist bursts measured
  about 2.3 dB over. The samples themselves always hold. Real masters rarely carry much above 18–20 kHz.
- **The constants are starting values** (the styles' recipes, the glue's threshold offset, release times). They have not been checked by listening.
- **No user preset files:** use the DAW's preset menu (it saves the plug-in state).
- **No offline export inside the plug-in.** The engine's offline renderer is still used by the vocal plug-in's old Mastering page.
- **Windows:** builds only through CI. Not tested in a DAW.
