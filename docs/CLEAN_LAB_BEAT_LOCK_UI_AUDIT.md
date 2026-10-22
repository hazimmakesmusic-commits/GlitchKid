# CLEAN LAB / BEAT LOCK / UI audit

Read-only audit done before this milestone's changes. Linux only: nothing here was tested in FL Studio, on macOS or on Windows.

## 1. Audio engine

- JUCE AudioProcessor with an APVTS of 65 parameters. IDs are permanent; new parameters are only ever appended.
- Chain: input → Pitch Tune → Formant → Harmony → Mutation → Doubler → user EQ → CLEAN LAB (corrective + resonance) → compressor → CLEAN LAB (de-esser) → saturation (4× oversampled, 6 samples latency) → AIR LAB → stutter → echo → Dimension → gain → Mix/Bypass crossfade against a delay-matched dry copy.
- Formant, Harmony, Mutation, Stutter and Alter Ego are hidden from the UI but kept for old presets and sessions. BUILD switches them off.
- There is no reverb. SPACE can only mean the existing echo and width (Dimension).
- Latency is 6 samples, reported to the host, identical on every path including bypass.

## 2. CLEAN LAB (exists, verified from the previous milestone)

- `Source/DSP/CleanLab.*`, zero latency. A 2048-point FFT side-chain (hop 512) learns a movement-weighted long-term spectrum. It finds persistent excess (up to 3 resonance bells, ≤ 6 dB), a low-mid bell (≤ 3.5 dB), a rumble high-pass at 70 Hz, and a de-esser (sibilance/body ratio, 7.5 kHz bell, ≤ 9 dB).
- Real parameters `clean_*`, real telemetry (`getCleanApplied()`), a real EQ graph overlay and a genuine bypass (`clean_enable`).
- Gap: it lives as one category inside the MODIFY page. It has no level of its own and no before/after of its own beyond the global DRY/PROCESSED switch.

## 3. Beat integration

- An optional aux input bus "Instrumental" exists. It is off by default, measured only, never mixed into the output.
- `getInstrumentalStatus()` reports notConnected / connectedSilent / live from real bus state and real signal.
- A second `VocalAnalyzer` listens to that bus. `startSourceAnalysis()` starts both. `automode::compare()` gives per-band masking risk, vocal-minus-beat level, low-end-heavy and very-loud flags.
- BUILD MY VOCAL MIX has a beat-informed mode that uses it. Without a real, analysed beat it stays vocal-only and says so.
- A mixed stereo input is measured as "the vocal". It cannot be split into two signals. That has to stay stated in the UI.
- FL Studio routing of the Instrumental input was never verified. The routing steps are documented but untested.
- Gap: no dedicated BEAT LOCK workflow (analyse, see the evidence, choose, apply, undo). The beat rules were private to BUILD.

## 4. UI status

- 960×600 design size, scaled. Pages TUNE / AUTO / MODIFY, MODIFY has five categories (PITCH & KEY, TONE, DYNAMICS, SPACE, CLEAN). Header with brand, tabs, DRY/PROCESSED and MOTION. Chain strip, preset bar with A/B.
- Smooth vector look (Inter, rounded panels, violet/pink on near-black). It is not the requested 8-bit look and has no level-select home.
- Real data feeds exist for everything a pixel meter needs: input/output peak, gain reduction, CLEAN telemetry, analyzer status and progress, beat status.
- No beat level meter existed on the processor. One is needed for BEAT LOCK.

## 5. Compatibility risks

- Parameter IDs and the count (65) must not change. This milestone adds no DSP parameters: BEAT LOCK edits existing EQ, echo and width parameters.
- UI-only preferences are stored as plain properties. New keys only; old states fall back to defaults.
- Old tests that look for MODIFY categories and the TUNE / AUTO / MODIFY names must be updated deliberately.

## 6. Plan

1. Shared `beatMoves()` used by both BUILD and BEAT LOCK.
2. Pure, tested `analyseBeatLock` / `applyBeatLock`.
3. Beat peak telemetry.
4. 8-bit level-select UI with the four levels.
5. Tests, fresh-directory build, measurements, docs.
