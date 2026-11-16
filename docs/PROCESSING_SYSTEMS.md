# Professional processing systems: EQ LAB, SPACE, AIR LAB

Final report for the "EQ LAB + SPACE + AIR LAB" brief (phases 1-6). Every number below comes from the test suite (`GlitchKidTests`, Linux, Release, one core,
48 kHz stereo unless stated). Nothing here has been checked by ear on real vocals, or built on macOS, by this work.

## Parameters and compatibility

- 291 parameters. The new ones are appended after everything that existed, so every older parameter keeps its host index:
  102 EQ LAB band parameters, 96 EQ LAB dynamics parameters, then 17 SPACE parameters.
- The original four EQ bands keep their IDs (`eq_lo`, `eq_b1`, `eq_b2`, `eq_hi`) and, in an older session or preset, their old shapes (low shelf, bell, bell, high shelf, all on).
  Bands 5-16 load off. Old sessions sound the same, with one exception: AIR LAB (below).
- `air_link` is retired: it is still in the layout (same index, so old sessions and automation load), not automatable, defaults to off and does nothing.
- AIR LAB's sound changed for old sessions that had it on. This was accepted in the brief.

## EQ LAB (16 bands)

- **Engine** (`Source/DSP/ParametricEq.*`): TPT state-variable filters. The bell and shelf responses are the same as the old filters.
  - **Types:** bell, low/high shelf, low/high cut, notch, band pass and tilt.
  - **Slopes:** cuts from 6 to 96 dB/oct.
  - **Placement:** each band can run on stereo, left, right, mid or side.
  - **Changes:** a click-free fade when a band's type, slope, placement or on/off changes.
  - **Solo:** only heard while the EQ page is visible.
  - **Idle:** with every band off or flat, the EQ costs nothing.
- **Dynamics:** per band, a detector on the band itself. Threshold, range, attack and release, cutting above or lifting below the threshold.
  The live change is shown on the band and on the curve. Measured: a band set to −9 dB range moved −9.00 dB on a tone above threshold.
- **Interface:**
  - **Graph:** add, drag, duplicate and delete bands; a right-click menu; zoom and pan.
  - **Analyser:** the EQ's input (shaded) and output (line), with freeze, FFT size, averaging and a peak readout with the note name.
- **CPU:** 16 busy static bands 1.31 %; 16 dynamic bands 2.42 %.

## SPACE (algorithmic reverb)

- **Engine** (`Source/DSP/SpaceReverb.*`): an 8-line feedback delay network with two-band decay per line set from RT60, modulated reads, diffusion
  and early reflections. It runs after the echo and before Dimension.
- **Algorithms:** Room, Chamber, Plate, Hall, Cathedral, Ambience, Modulated Space, Experimental Space.
- **Characters:** Warm, Classic, Modern.
- **Freeze:** holds the tail.
- **No latency.** The processor reports the tail length to the host. Off and once the tail has gone, SPACE passes audio untouched.
- **The nine vocal presets, measured through the processor:**

  | Preset | Set RT60 | Measured RT60 |
  |---|---|---|
  | Tight Vocal Room | 0.6 s | 0.54 s |
  | Intimate Vocal | 1.1 s | 1.12 s |
  | Smooth Plate | 1.8 s | 1.77 s |
  | Bright Plate | 1.6 s | 1.40 s |
  | Wide Vocal Hall | 2.6 s | 2.34 s |
  | Deep Melodic Vocal | 3.4 s | 3.49 s |
  | Atmospheric Vocal | 5 s | 4.95 s |
  | Long Ambient Tail | 9 s | 8.03 s |
  | Experimental Vocal Space | 6 s | 5.34 s |

  - The level at each preset's mix stays within 1.2 dB of the dry signal.
  - The largest narrow peak in a tail (ringing) is 2.3-11 dB above its surroundings.
- **CPU:** 1.4-1.6 % per algorithm.

## AIR LAB v2 (MID AIR, HIGH AIR)

- **Engine** (`Source/DSP/AirLab.*`): parallel additions to the untouched signal. No latency, no oversampling.
  - **MID AIR (clarity):** a lift around 3.2 kHz plus density harmonics made from 1.2-2.4 kHz. How much is added follows the voice:
    a dull voice got +4.5 dB, a normal one +3.6 dB, a harsh one +1.5 dB.
  - **HIGH AIR (air):** a lift above 8.5 kHz plus 2nd and 3rd harmonics of the 2.5-5.5 kHz band, kept between 7.5 and 16 kHz. It also follows the voice:
    a normal voice got +0.97 dB, an already bright one +0.35 dB. Nothing folds back at 44.1 kHz and above.
  - **Independence:** MID AIR at 100 % changes 10-16 kHz by 0.39 dB; HIGH AIR at 100 % changes 2.5-5 kHz by −0.27 dB. The Link control is gone.
  - **Noise gate:** gaps, room tone and quiet breaths are not lifted. Noise 40 dB under a phrase changed by 0.17 dB; room tone under −60 dBFS was untouched.
  - **Kept from v1:** easing off on S sounds, MIX, OUTPUT, the 20 ms on/off crossfade, and bit-exact pass-through when off.
- **Interface:** a live readout beside the switch ("M +x.x  H +x.x") shows what each control is adding right now.
- **CPU:** 0.86 % with everything at maximum.

## Where each system is used

| | EQ LAB | SPACE | AIR LAB |
|---|---|---|---|
| **BUILD MY MIX** | Band 5 low cut from the measured low end. Band 6 dynamic de-esser when the S sounds measure strong. CLEAN LAB does both jobs when it is on. | Each profile's preset and mix, scaled by intensity. | Switched on. MID AIR 10-45 % from the presence share. HIGH AIR 5-50 % from the air share, capped at 15 % when the S sounds are strong. MIX 100 %, OUTPUT 0 dB. |
| **Profiles** | The four original bands. | A preset and mix per profile. | Left to the build. |
| **SPACE macro / HOME SPACE dial** | | `space_on`, `space_mix` (up to 30 %), `space_decay` (1-3 s), together with echo and width. | |
| **KID** | | Its depth moves switch SPACE on and scale it, with a cap. | Asks for "More HIGH AIR". |
| **HOME dials** | | | MID AIR and HIGH AIR, each its own parameter. |

## Whole chain

- **Signal path:** input → tune → formant → harmony → mutation → doubler → EQ LAB → CLEAN LAB corrective → compressor → CLEAN LAB de-esser → saturation
  → AIR LAB → stutter → echo → SPACE → dimension → gain / mix → limiter.
- **Default instance CPU:** 0.45 %.
- **Heaviest Build My Mix result:** about 7 % of one core (6.9-7.2 % across runs; which profile is heaviest changes from run to run). With AIR LAB off in the build, the heaviest was 5.0 %. A 5 s render is too short for exact timing.
- **Built mixes:** over every profile, the worst level change is 3.33 dB (3.17 dB with AIR LAB off) and the worst peak is 0.97 (the same with AIR LAB off).
- **Profiles:** levels are within 0.68 dB of each other. The closest pair of profiles still differs in 18 controls.
- **Tests:** 510 test groups, 0 failures.

## Known limits

- **Not listened to:** none of this has been tried by ear on real vocals. The adaptive thresholds in AIR LAB were tuned on synthetic signals.
- **Not built on macOS:** checked only by the Linux build and the tests.
- **Double brightening:** BUILD MY MIX still raises the static EQ (+1 dB presence, +1 dB high shelf) on dull voices on top of AIR LAB's lift.
- **ASTRO AutoMode** (`docs/AUTOMODE.md`) does not use SPACE, EQ bands 5-16 or AIR LAB. BUILD MY MIX does.
- **Old audit documents** (`docs/*_AUDIT.md`) are left as written; they describe the plug-in at the time they were written.
