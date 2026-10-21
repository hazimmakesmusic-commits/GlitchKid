# CLEAN LAB: audit (read-only phase, written before any implementation)

Repository state: branch `claude/glitchkid-plugin-architecture-3o6w74`, HEAD `c72e3f1`. The only uncommitted item is the untracked `docs/GLITCHKID_EXPERIENCE_DIFFERENTIATION_AUDIT.md` from the previous task; it is the user's work and stays untouched. Nothing in this milestone is committed or pushed.
Status classes: **V** implemented and verified, **I** implemented but insufficiently verified, **P** partial, **D** planned only, **M** missing.

## 1-10. Findings

| # | Item | Status | Evidence and limits |
|---|---|---|---|
| 1 | EQ and response graph | **V** | `ParametricEq` (4 RBJ bands: low shelf, 2 bells, high shelf; double precision; 20 ms smoothing; flat bands skipped). `EqGraph` computes the combined response with the same formulas; a test compares it with the real DSP within 0.15 dB. The graph draws the four manual bands only. |
| 2 | Vocal analysis and bands | **P** | `VocalAnalyzer`: 5 coarse bands (<120, 120-500, 500-2k, 2k-5k, >5k Hz), crest, dynamic range, clipping, a rough sibilance index, only during a user-started listening pass (up to 8 s). No FFT-resolution spectrum, no narrow-peak measurement, no continuous analysis. Not usable for finding resonances. |
| 3 | AIR LAB and its sibilance protection | **V** (synthetic) | `AirLab`: parallel presence/air with a mono-linked sibilance duck (7.5 kHz envelope against a 1 kHz body envelope). It only limits the highs AIR LAB itself adds; it does not de-ess the vocal. |
| 4 | Compressor, pitch correction, output gain | **V** (offline) | Compressor: broadband, feed-forward, zero latency, real GR meter. Pitch Tune: ~31 ms latency while on. Output gain and Mix smoothed 20 ms. No listening validation on real vocals. |
| 5 | Audio-thread safety and parameter updates | **V** | Parameters read through `getRawParameterValue` atomics once per block, smoothers per stage, UI telemetry through atomics (`consumeInputPeak`, `getTuneDetectedMidi`, GR peak). Tests assert no allocation per block and finite output with NaN/Inf input. No ThreadSanitizer run recorded. |
| 6 | Latency and CPU | **V** latency / **I** CPU | Reported latency = saturation 6 samples (+ Pitch Tune etc. when on); AIR LAB adds none (test). CPU is only measured per module offline (AIR LAB about 0.5 % of one core); no whole-plug-in or host measurement exists. |
| 7 | BUILD MY VOCAL MIX | **V** (engine) / **I** (sound) | `automode::buildMix` is pure and tested. It already contains corrective rules from the coarse analysis: low shelf -2 dB when >30 % of energy is under 120 Hz, the 400 Hz bell -1.5 dB when the low-mids exceed 55 %, the high shelf lowered when the sibilance index is high (+ similar beat-driven rules). CLEAN LAB must not apply the same corrections a second time. |
| 8 | Dry/processed and A/B | **V** / **P** | Real bypass (delay-matched, 20 ms crossfade) and two A/B slots work and are tested. Neither is loudness-matched. |
| 9 | Parameter IDs, state, preset format | **V** | 60 APVTS parameters (IDs permanent, new ones appended last so host indexes do not move). `SoundState` captures every parameter except `bypass`; missing ids in older presets/states load as defaults; `.gkpreset` files are atomic and hostile-file tested. |
| 10 | Tests and build methods | **V** | `GlitchKidTests` (JUnit-style `juce::UnitTest`): 321 groups pass on Linux (last fresh run). Build: CMake + JUCE 9.0.4, `-DGLITCHKID_BUILD_TESTS=ON`. macOS/Windows builds and host validation: not possible in this environment (Linux only, no DAW). pluginval: not installed here (`which pluginval` empty); it will not be downloaded. clang-tidy exists but is not part of the project's checks. |

## Specific facts that decide the design
- **De-esser: missing (confirmed).** No de-esser parameter, DSP or UI exists. Searched `Source/` for "deess"/"de-ess": only comments in AIR LAB and the analyser.
- **Resonance suppression: missing (confirmed).** The only dynamic frequency-dependent processing is AIR LAB's own duck.
- **FFT:** `juce_dsp` is already linked (`juce::dsp::FFT` and `Oversampling` are in use), so a spectral detector needs no new dependency; the formant shifter has its own radix-2 FFT.
- The EQ's four bands are the *user's*; the graph and the manual controls must stay independent of any automatic correction.
- All DSP stages are zero-latency except the saturation (fixed 6 samples) and optional Pitch Tune; CLEAN LAB can and should be zero-latency.

## Decision: proceed, in a bounded form
The architecture supports a safe, bounded addition: a self-contained module with IIR processing (no latency), an FFT used only as a *side-chain detector* (the audio itself is never resynthesised), appended parameters with neutral defaults (off), telemetry through atomics, and no change to any existing stage's code. Nothing unresolved blocks it.

### Signal-chain position (reasoned)
`... user EQ -> CLEAN LAB (rumble, low-mid build-up, resonance) -> compressor -> CLEAN LAB (de-esser) -> saturation -> AIR LAB -> ...`
- The corrective stage sits **after the user EQ**: its detectors see the signal the user already shaped, so a cut the user made is not repeated (this is the "no double correction with manual EQ" rule), and it sits **before the compressor** so resonances and rumble do not drive the compressor or get amplified by its makeup gain.
- The de-esser sits **after the compressor**: compression lowers vowels more than consonants and raises the *relative* sibilance, so the de-esser must see the post-compressor balance; it sits **before saturation and AIR LAB** so harmonics added later are not de-essed away and the sibilance seen by AIR LAB's own duck is already tamed.
- One module, two call points, one set of parameters. Both stages are zero-latency.

### Interaction with BUILD MY VOCAL MIX
BUILD never changes CLEAN LAB's parameters (they join the keep-list with Gain, Mix, tuning and AIR LAB). When CLEAN LAB is on with a non-zero amount, BUILD **skips its own low-shelf, low-mid and sibilance EQ corrections** and says so in the evidence; its other moves (dynamics, makeup, presence, saturation) stay. This is the "avoid double processing" rule, and it is tested.

### Scope limits (explicit)
Not in this milestone: noise removal, breath removal, dereverberation, spectral resynthesis, pitch-dependent processing, per-phrase automation, lookahead (which would add latency).
