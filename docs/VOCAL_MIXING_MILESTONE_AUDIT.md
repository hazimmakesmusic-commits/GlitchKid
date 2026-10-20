# Vocal Mixing First: milestone audit

Goal: "Get a great starting vocal mix with one click. Then make it yours."
Results below are from Linux only (headless tests and rendered snapshots). Nothing here was tested on a Mac, in FL Studio (apart from the earlier BUILD screenshot) or on Windows.

## A. What exists (audit)

| Area | State | Evidence |
|---|---|---|
| Pitch correction (Pitch Tune) | Real DSP, key/scale aware, strength + speed, ~31 ms latency reported | `PitchTuneTests` ("Key and scale are honoured"), `PitchCorrectionTests` |
| Tuning styles NATURAL / LIGHT / HARD | Fixed strength/speed pairs (50 %/80 ms, 75 %/35 ms, 100 %/0 ms), read back from the knobs | `MacroTests` |
| EQ, compressor, saturation, echo, Dimension, Doubler | Real DSP, tested | their own test files |
| Reverb | Does not exist. "Space" = echo + width + doubler. Not claimed anywhere. | |
| Formant, harmony, mutation, stutter, Alter Ego | Exist and are tested, but are creative effects (formant is unverified in FL). Hidden from the UI; parameters kept so old sessions load. | `FormantShiftTests`, `MutationTests`, ... |
| Vocal analysis | Peak, RMS, crest, clipping, dynamic range, 5 band shares, sibilance. No pitch/key detection. | `AutoModeTests` |
| Beat analysis | Optional "Instrumental" input; only measured, never mixed. Routing in FL Studio is still unverified. | `SourceTests` |
| BUILD MY VOCAL MIX | Applies real parameters (proved in an FL Studio screenshot earlier) | `MixEngineTests`, editor test |

## B. Mix engine (`automode::buildMix`)

Pure function: profile values, then bounded, explained adjustments from the analysis (compressor threshold follows the level within +-8 dB, makeup reduced for hot peaks, saturation halved for hot input, tone corrections capped at +-6 dB, beat findings only when the beat was really analysed). Intensity scales effect amounts (echo, width, doubler, drive) from 50 % to 100 %.
- Never touches Gain, Mix, Bypass, tuning, key or scale. Switches off retired creative effects and says why.
- Vocal-only works without a beat. Too-short or unreliable analysis gives a conservative profile-only mix, labelled "STARTING MIX (profile only, not fitted to your voice)".
- Repeat builds do not drift (same input, same output). BACK TO MY SOUND restores the sound from before the build.
- WHAT CHANGED lists every change with its reason.

## C. Profiles

Eight, original, generated from one table (`tools/gen_profiles.py`), no artist names. Calibrated with real processing in `Tests/ProfileTests.cpp`: every profile within 0.6 dB of the input level, peaks 0.52-0.94 (never clipping), all finite, and each profile differs measurably from the others (EQ, compression or space settings). Tuning, key, scale, Gain and Mix are never part of a profile; loading one keeps yours.

## D. Interface

TUNE (key note pad, scale chips, tuning style), AUTO (8 profile cards, LOAD PROFILE, BACK TO MY SOUND, BUILD MY VOCAL MIX, intensity, WHAT CHANGED, DRY / PROCESSED), MODIFY (PITCH & KEY, TONE, DYNAMICS, SPACE). The Character tab, SURPRISE ME, Alter Ego, the 50-artist gallery and its filters are removed. The overlapping scramble-heading effect is removed. Tooltips and source-status wording were updated. The mascot has idle / listening / analysing / ready states.

## E. Compatibility

All 54 parameter IDs are unchanged and still saved. Sessions that used the retired effects still load; hidden effects keep their saved values, and the first BUILD switches them off (reported in WHAT CHANGED). Old presets still load. Retired artist presets are gone from the menu (their names are no longer reserved).

## What is NOT done / risks

- No FL Studio or Mac listening test of the new build; the checklist is below.
- Whether the profiles sound "great" is a listening judgement; only levels, safety and difference are measured.
- The sidechain/instrumental routing in FL Studio is unverified, so BEAT INFORMED is untested in a real host.
- No key detection; the user sets key and scale.
- The three tuning styles are tested at corrector level (strength, speed, key, scale) but not compared on a real vocal.

## FL Studio listening checks (Mac)

1. Pull the branch, `cmake --build build -j8`, copy `GlitchKid.vst3` to `~/Library/Audio/Plug-Ins/VST3/`, rescan plug-ins.
2. TUNE: choose the key and scale of the song, press HARD, sing/play a vocal: it should snap to scale notes. Compare NATURAL, LIGHT, HARD.
3. AUTO: for each of the eight profiles press LOAD PROFILE and compare with DRY: levels should be similar, character clearly different.
4. Pick a profile, press BUILD MY VOCAL MIX, play ~8 s of the vocal. Check the message, WHAT CHANGED, and that Gain and your key did not move.
5. Press BACK TO MY SOUND: the sound returns. Build again twice: the result should not drift.
6. Silence or a very quiet vocal: expect a clear message, not a broken sound.
7. MODIFY: only four categories; every knob moves its parameter and automates in FL.
8. Save the project, reopen: sound, key and page restore.
