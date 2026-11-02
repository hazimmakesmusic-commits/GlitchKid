# The GlitchKid Mastering Lab

A separate workspace for mastering a **finished stereo mix**. It never runs on your vocal.

## Two ways to use it

1. **On your master bus (real time).**
   * Put a second GlitchKid on the master bus, press **Mastering** on its instrument screen, and switch **Mastering mode: on**.
   * That instance now runs the mastering chain instead of the vocal chain. Its vocal settings stay saved, just unused.
   * Your vocal instance is unaffected.
   * Bounce the song from your DAW as usual.
2. **From a file (offline).**
   * **Load mix** measures a finished mix file.
   * **Export master** renders it with the current settings to a 24-bit WAV at the file's own sample rate. The master is the same length as the mix and time-aligned with it.
   * The mix file is only read, never changed.
   * There is no playback inside the plug-in: listen to the exported file in your DAW.

The workflow: Load mix (or play it) → choose a style → adjust → compare → export (or bounce).

## The screen

* **Input**, from real measurements:
  * loudness to ITU-R BS.1770-4 (integrated, or short-term until there is enough to integrate);
  * sample peak and true peak;
  * clipping;
  * stereo correlation;
  * whether the mix is ready to master: clipping, phase problems, already limited, very quiet.
* **Style:** four original starting points.

  | Style | What it does |
  |---|---|
  | Clean & Balanced | A little low-mid tidy-up, a touch of air, gentle 1.5:1 glue, no colour |
  | Warm & Smooth | Fuller lows, softer highs, slow 2:1 glue, light saturation |
  | Loud & Punchy | Tighter low-mids, more bite, 2.5:1 glue with a slower attack, extra drive into the limiter |
  | Wide & Atmospheric | Air on top and a wider side (never below 120 Hz), soft glue |

  They are real processing settings (`master::resolve`). They are starting points, not guaranteed results.
* **Intensity** sets how much of the style is used. At 0 only the limiter is left.
* **Loudness** is the drive into the limiter.
* **Tone** tilts darker or brighter.
* **Width** sets the stereo width above 120 Hz.
* **Output:**
  * loudness (short-term and integrated);
  * true peak against the fixed **-1 dBTP** ceiling;
  * limiting and glue gain reduction (a warning above 6 dB of limiting);
  * clipping;
  * output gain, which can only go down.
* **Original / Master** switches between the untouched mix and the master.
  * Original is time-aligned and played at the master's measured loudness, so the louder version never wins.
  * Original is never raised into clipping.
* **Reset meters** restarts the loudness measurement, for example at the top of the song.

## The chain

1. Input meter.
2. EQ (the existing 4-band EQ): corrective plus tone.
3. Glue compressor (the existing compressor).
   * Its threshold sits a few LU above the mix's own integrated loudness, so quiet and loud mixes are treated alike.
   * No makeup gain.
4. Saturation (the existing tanh stage, 4x oversampled).
5. Width.
   * The mid is untouched, so the mono sum is exactly the input's.
   * Only content above 120 Hz is widened, through a 4th-order high-pass.
   * The side can never grow past 90 % of the mid's energy.
6. True-peak limiter (new).
   * Look-ahead of 2 ms plus the detector's delay; inter-sample peaks are detected by 4x oversampling.
   * A smooth gain that arrives in time, with a style-dependent release.
   * Ceiling -1 dBTP.
7. Output trim (0 dB or below).
8. Output meter.

Latency is about 2.3 ms at 48 kHz, reported to the host while Mastering mode is on.

## Tested (in the build environment)

* **Loudness:** 1 kHz at -23 and -33 dBFS reads -23 and -33 LUFS (±0.1) at 44.1, 48 and 96 kHz.
* **Gating:** EBU Tech 3341-style cases 3 and 4 read -23 LUFS (±0.1).
* **True peak:** a quarter-rate sine at 45° reads its real peak (3 dB above its samples).
* **Limiter:** below the ceiling it is an exact delay. Driven 10 dB into it, no sample goes over -1 dBFS and the true peak stays at most 0.2 dB over -1 dBTP, at all three rates.
* **Width:**
  * unity is transparent;
  * the mono sum is unchanged at every width;
  * mono stays mono;
  * 40 Hz side content is not widened;
  * independent channels at width 200 % don't go out of phase.
* **The whole chain**, at all three rates, every style, on quiet, loud, transient-heavy, bass-heavy, wide and mono material:
  * no sample over -1 dBFS and the true peak at most 0.25 dB over -1 dBTP;
  * no clipping, no NaN;
  * mono stays mono;
  * quiet mixes come up.
* **ORIGINAL** is time-aligned and within 1 LU of the master.
* **Export** is the same length as the mix, sample-aligned (24-bit precision), under the ceiling, and the source is unchanged.
* **Off by default:** the vocal chain is still bit-exact, mastering settings aren't part of vocal presets, A/B or Kid, and state is saved and restored.
* **CPU:** about 3 % of one core at 48 kHz and 6 % at 96 kHz on the build machine.

## Not verified

* Listening. The tests check measurements, not taste. Every master should be judged by ear and on several systems before release.
* Inside FL Studio or other hosts.
* Real songs. The tests use synthetic mixes.
* Results are not guaranteed to be "industry-ready".
* No dither is applied when writing 24-bit files.
* Export writes WAV only.
* Reference-track matching is not part of mastering. The existing Reference page compares a vocal's tone only.
