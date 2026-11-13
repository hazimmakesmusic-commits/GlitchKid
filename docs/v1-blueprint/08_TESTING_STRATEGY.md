# Deliverable 8 — Testing strategy

Unit tests prove maths and safety; they do not prove taste (`docs/TESTING.md` [D]). V1 adds what is missing:

- a repeatable **sound-quality evaluation**;
- **host testing**;
- **performance measurement**;
- **state-restoration** coverage;
- **CI**.

**House rules, kept:**

- New tests are mutation-checked: deliberately broken code must fail them [D].
- **A test counts as passed only when it was run and its log (date + build hash) is kept.** The build-stamp mechanism already prints the source commit and tree state [V].

## 1. Layers

| Layer | Exists today | V1 adds |
|---|---|---|
| Unit (`GlitchKidTests`) | 463 groups [V]; 467 run 2026-11-05 on Linux | Layered model, macros, reverb, ducking, Generate per genre, Beat Lock automation, migration, host-bypass path |
| Audio regression (golden renders) | None | Offline render tool + tolerance comparisons |
| Objective sound metrics | None | Metric suite on every render (§2.6) |
| Listening tests | None formal (owner's ear) [D] | Blind, loudness-matched protocol (§2) |
| Host compatibility | Owner's FL Studio checks [D] | Matrix of 4 DAWs × 2 OS × formats (§4) |
| Plug-in validators | pluginval strictness 5 on Linux, date unknown [D] | pluginval strictness 10 on every platform; `auval` for AU; Steinberg VST3 validator |
| Performance | None measured [D] | CPU and latency benchmarks (§5) |
| Sanitizers | ASan/UBSan on Linux, historical [D] | In CI, plus real-time-safety checks [R] |
| CI | None [V] | macOS universal, Windows x64, Linux; V1 and Lab configurations |

## 2. Sound-quality evaluation (the primary launch gate)

### 2.1 Corpus

| Kind | Content |
|---|---|
| Vocals (≥ 36 takes, ≥ 30 s each) | Every brief category: clean studio, noisy home, bright, dark, deep, high-register, melodic rap, aggressive rap, soft. At least 4 per genre (overlap allowed). |
| Instrumentals (≥ 21) | 3 per genre: sparse, dense, bass-heavy or wide |
| Rights | Own or licensed recordings only. Audio is kept **outside the repository**; a manifest with SHA-256 hashes is kept inside, so every run uses the same files. |

### 2.2 Conditions compared per item

| Code | Condition |
|---|---|
| a | Raw vocal |
| b | GlitchKid Generate (genre default, macros centred) |
| c | GlitchKid Generate + a tester's macro refinement (2-minute limit, the realistic use) |
| d | Engineer reference mix of the same vocal, using standard tools (EQ, compression, de-essing, saturation, reverb/delay) |
| e | Previous GlitchKid build (regression) |

For Beat Lock items: (b) with Beat Lock ON vs OFF, in the full mix.

### 2.3 Level matching and rendering

- **Offline render tool (new).** A small CLI built from the processor renders a file through a saved state. It produces the bit-identical result the plug-in would in real time (offline equals real time is asserted by a test).
- **Vocal-only comparisons:** integrated loudness (BS.1770, reusing the existing meter) matched within ±0.5 LU.
- **Full-mix comparisons:** the vocal-to-beat ratio is matched, then the sum is matched.
- **Peaks:** true peak ≤ −1 dBTP after matching.
- **Never compare unmatched audio,** and never report a loudness difference as a quality gain.

### 2.4 Listening protocol

- **Method:** blind, randomised, hidden labels, MUSHRA-style 0–100 ratings plus a forced-choice preference.
- **Attributes:**
  - overall;
  - tonal balance;
  - intelligibility;
  - harshness / sibilance;
  - artifacts and naturalness;
  - space / placement.
- **Panel:** at least 5 listeners (≥ 2 experienced mix engineers, ≥ 2 target-customer artists); calibrated headphones or monitors; sessions ≤ 45 minutes.

**Launch thresholds (proposed [A], for owner approval):**

| Condition | Gate |
|---|---|
| Generate (b) | Mean overall ≥ (d) − 10 points across the corpus, and ≥ (a) + 15 on ≥ 90 % of items |
| Artifacts | No item where (b) or (c) is rated below 60 for artifacts/naturalness |
| Harshness | (b) not worse than (d) by > 10 points on harshness/sibilance, on average and per genre |
| Beat Lock | ON preferred or tied on ≥ 70 % of pairs; never worse on intelligibility by panel majority |
| Pitch correction | Natural and Light styles not rated below 70 for artifacts on sung material |

### 2.5 Pitch-correction specific

- Sustained notes, fast runs, vibrato, breathy tones, rap (mostly unvoiced) and noisy home takes.
- Measure:
  - glitches (spectral-flux spikes at grain boundaries) [R];
  - pitch accuracy against the target note;
  - added noise.
- The roadmap's “grainy” risk [D] is tested here before launch.

### 2.6 Objective metrics (every render, logged and compared with the previous build)

| Metric | Use | Tolerance |
|---|---|---|
| Integrated loudness, LRA, true peak | Matching; dynamics character | — |
| Third-octave balance vs the genre target corridor | Tonal balance | ± 3 dB after matching [A] |
| Sibilance ratio (5–10 kHz vs 1–5 kHz in sibilant frames) | De-essing effectiveness | Not above the raw vocal's [A] |
| Presence-band prominence | Harshness proxy | — |
| Noise floor in the gaps | Noise added by processing | ≤ raw + 1 dB [A] |
| THD+N at −18, −6, −1 dBFS sine; aliasing on sweeps | Distortion | — |
| Stereo correlation; mono fold-down loss | Stereo stability | Correlation ≥ 0; loss ≤ 1 dB |
| Latency (impulse) | Matches the reported value | Exact |
| Render time | CPU proxy | Per block |

## 3. Unit and regression tests to add (by deliverable)

| Area | Tests |
|---|---|
| Layered model (Deliverable 4 §2) | All layers neutral = bit-exact; offsets bounded and clamped; UI writes detail = shown − offsets; automation of macros never writes detail parameters |
| Macros | Monotonic, continuous at 0, each moves only its own dimension; PUNCH loudness compensation within ±1 dB on test vocals [A] |
| Reverb | RT60 within ±10 % of setting; no runaway at maximum settings; tail spectral flatness (no ringing) above a threshold [R]; mix 0 = bit-exact; mono fold-down; no denormals |
| Ducking | Gain reduction follows the dry envelope; zero latency |
| Generate | Deterministic per genre and input; every value in range; Tune, Gain and Mix untouched; the adaptation rules (Deliverable 5 §3) |
| Beat Lock | Deliverable 6 §8 |
| State | Round trip of every field (Deliverable 2 §9); migration fixtures (Deliverable 3 §4); corrupt and foreign data rejected; presets read forward and back |
| Bypass | Host bypass = internal bypass, sample-aligned (null test with latency); matched-compare gain correct; no clicks |
| Golden renders | Per genre on short fixture vocals: exact hash for deterministic paths; tolerance for others |

## 4. DAW compatibility matrix

| Host | macOS (arm64 / x86_64) | Windows x64 | Format |
|---|---|---|---|
| FL Studio (current) | ✓ | ✓ | VST3 |
| Ableton Live 12 | ✓ | ✓ | VST3 (and AU on Mac) |
| Reaper (current) | ✓ | ✓ | VST3 (and AU on Mac) |
| Logic Pro (current) | ✓ | — | AU only |

**Test cases (each host × OS):**

1. Scan, load, open/close the editor, remove (no crash, no leak).
2. Mono and stereo tracks; correct output channels.
3. Instrumental sidechain routing; bus status shown; no beat audio leaks into the output.
4. Automation: record and play back the 4 macros and 3 detail parameters; offline bounce equals real-time playback.
5. Presets: save, load, overwrite confirmation, factory genres.
6. Session save, close, reopen: every field restored exactly (compare a render before and after).
7. Sample-rate change mid-session (44.1 → 48 → 96 kHz): no clicks or crashes; the sound is the same apart from rate effects.
8. Buffer sizes 32 → 2048: identical renders (block-size independence).
9. Host bypass and internal Bypass: time-aligned, click-free; latency compensation correct with Tune on and off (null test against a dry duplicate track).
10. Latency reporting: the host's compensation matches; behaviour when Tune switches during playback.
11. CPU: 1 and 10 instances at 48 kHz / 128 samples, measured in the host's meter and with Activity Monitor or Task Manager.
12. UI: Retina and Windows 125 %/200 % scaling; keyboard focus; shortcuts not stolen.

Results go in a dated sheet (host version, OS, build hash, pass/fail, notes). **Support is claimed only for host/OS pairs that pass every case.**

## 5. Performance and robustness

| Check | Method |
|---|---|
| CPU per stage and total | Offline timing harness, 10 runs, median; in-host spot checks |
| Real-time safety | No allocation or locks on the audio thread: allocation hook in tests; clang RealtimeSanitizer where available [R] |
| Stability | 8-hour playback with random automation; parameter fuzzing; NaN/Inf injection (existing tests extended) |
| Memory | Instance memory; no growth over time |

## 6. Continuous integration (new)

- **Triggers:** every push and pull request.
- **Builds:**
  - macOS universal (arm64 + x86_64): VST3 + AU;
  - Windows x64: VST3;
  - Linux: tests;
  - each in both the V1 and Lab configurations.
- **Gates:** unit tests, pluginval strictness 10, `auval`, render-regression metrics, sanitizers on Linux.
- **Artefacts:** the plug-ins and the logs.

The repository is public today (`docs/ROADMAP.md` [D]), so CI minutes and visibility are an owner decision.
