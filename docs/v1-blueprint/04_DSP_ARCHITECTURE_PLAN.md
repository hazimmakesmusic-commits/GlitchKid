# Deliverable 4 — DSP architecture plan

How the existing engine supports the simplified product. The default answer is to **reuse**: the chain, smoothing, safety rules and engines stay. Changes are limited to:

- the layered sound model (§2);
- one new processing block, reverb and ducking (§5);
- bypass and latency fixes (§6);
- analysis upgrades (§7).

Tags as in Deliverable 1.

## 1. Principles carried over [V]

- **Real-time safety.** No allocation, locks or I/O on the audio thread.
- **Robust input.** Every stage guards against NaN/Inf.
- **Neutral means neutral.** Parameters at neutral are bit-exact and idle stages are skipped (`docs/DECISIONS.md` #3).
- **Determinism.** `-ffp-contract=off`.
- **Smooth changes.** 20 ms parameter smoothing; EQ coefficients refresh every 32 samples while moving.
- **Pure engines.** Message-thread engines are pure functions of their inputs, so they can be tested and repeated.

## 2. The layered sound model (new; the core change)

Today a macro or engine simply overwrites parameters (Deliverable 1 §6). V1 needs three things to coexist without fighting:

- a **generated mix**;
- **macro refinements**;
- a **Beat Lock adaptation** layer;
- with the artist's **manual edits** winning over all of them.

**Effective value** of a parameter = **detail parameter** (baseline + manual edits) **+ macro offset + Beat Lock delta**, clamped to the parameter's range.

| Layer | Stored as | Written by | Read by |
|---|---|---|---|
| Detail parameters | Today's APVTS parameters [V] | Generate (one gesture per parameter), manual edits, presets | Audio thread via `read*Settings()` [V] |
| Macro offsets | **4 new automatable parameters** (`macro_tone`, `macro_punch`, `macro_space`, `macro_presence`, −100…+100, default 0) | Artist, host automation | Audio thread computes offsets per block (cheap closed-form curves, §3) |
| Beat Lock deltas | **Session state** (not parameters): a small fixed table of `{param, delta}` for the allowed parameters, mirrored to atomics | Beat Lock engine (message thread), once per beat | Audio thread adds them in `read*Settings()` |
| Ownership | Session state: the set of parameters the artist edited after the last Generate or Beat Lock | UI gesture handler | Beat Lock: a claimed parameter gets **no** Beat Lock delta |

### Rules

- **Neutral layers cost nothing.** With every macro at 0 and no Beat Lock delta, the effective value equals the detail parameter bit for bit, preserving the existing neutral-default guarantee.
- **The UI shows effective values.** Dragging a detail control writes `detail = shown − offsets`, so what you set is what you hear.
- **Automation is clean.**
  - Host automation of a macro moves only the macro's offset.
  - Automation of a detail parameter moves only the baseline.
  - Neither overwrites the other.
- **Rejected alternatives.**

| Alternative | Why rejected |
|---|---|
| Macros write parameters (today) | Destroys the generated baseline |
| Recompute everything on the message thread | Macros can't be automated, and the host sees dozens of parameter writes per gesture |

## 3. Macro curves (offsets at macro value m ∈ [−1, 1])

| Macro | Offsets | Notes |
|---|---|---|
| TONE | `eq_hi_gain += 4·m` dB; `eq_lo_gain −= 2·m` dB | A tilt; shelf frequencies stay the generated ones |
| PRESENCE | `eq_b2_gain += 3·m` dB; AIR LAB presence `+= 25·m` points; `clean_ess += 15·max(m, 0)` points | Total presence bell capped at +6 dB; de-essing follows presence upward only |
| PUNCH | `comp_thresh += (m>0 ? −8·m : −6·m)` dB; `comp_ratio ×= (m>0 ? 1+0.6·m : 1+0.3·m)`; `comp_attack ×= (1 + 0.5·max(m, 0))`; **`comp_makeup += ΔGR(m)`** | ΔGR is the change in static gain reduction at the generation's measured vocal RMS, from the compressor's own static curve. Perceived loudness stays roughly constant [A] [T]. |
| SPACE | `rev_mix ×= g(m)`; `echo_mix ×= g(m)`; `dim_mix ×= (1 + 0.5·m)`; `space_duck += 30·max(m, 0)` points; with `g(m) = 1 + m` (0 at −1, 1× at 0, 2× at +1) | At m = −1 the reverb and echo sends are 0: fully dry. Feedback and decay stay generated; SPACE never adds runaway repeats. |

- **Bounds.** Every offset result is clamped to the parameter's range.
- **Pinned by tests.** The curves are monotonic and continuous at 0, and each macro moves only its own dimension.
- **Tuning.** These curves are starting points [A]. They are tuned by listening (Deliverable 8 §2), and must stay monotonic and bounded after tuning.

## 4. Stages and the processing order

The processing order stays exactly as today [V]; only grouping and visibility change. In the table, *(Lab)* means the stage is compiled only in Lab builds and bypassed in V1.

| UI stage | DSP stages (chain position) |
|---|---|
| TUNE | 1 Pitch Tune |
| *(Lab)* | 2 Formant · 3 Harmony · 4 Mutation |
| SPACE (internal texture) | 5 Doubler |
| SHAPE | 6 EQ |
| CLEAN | 7 corrective · 9 de-esser |
| SHAPE | 8 Compressor · 10 Saturation · 11 AIR LAB |
| *(Lab)* | 12 Stutter |
| SPACE | 13 Echo → **13b Reverb (new)** → 14 Dimension |
| FINISH | 15 Gain · 16 Mix · 17 Limiter |

**Deliberate differences between UI order and processing order** (documented; do not “fix” without listening tests):

- CLEAN's corrective EQ runs **after** SHAPE's tonal EQ, so it corrects what the tone stage might exaggerate (existing decision [D]).
- The Doubler runs before the EQ, so the doubles get the same tone and dynamics as the voice [V].

## 5. New DSP: reverb and ducking (the largest sound-quality risk)

### Requirements

- Vocal-focused plate and room characters.
- Smooth, dense tail: no metallic ringing, no flutter.
- Pre-delay 0–120 ms; decay 0.3–6 s; damping / tone; a low-cut on the reverb input (default 250 Hz).
- Mono-compatible; stereo output; zero added latency.
- CPU ≤ 2 % of an M1 performance core at 48 kHz [A].
- Click-free parameter changes.
- Deterministic.
- Tail behaviour reported (`getTailLengthSeconds`, currently 0 [V]).

### Options

| Option | Quality | Cost | Licence | Verdict |
|---|---|---|---|---|
| `juce::dsp::Reverb` (Freeverb) | Known metallic, dated character | Low | JUCE | **Prototype only** |
| In-house FDN (8 lines, Jot-style), modulated delays, early-reflection taps | High with careful tuning | Medium | Own | **Recommended** |
| Dattorro plate (published topology) | Classic vocal plate | Low–medium | Algorithm published; implementation own | **Recommended as the Plate voicing** |
| Convolution with synthetic impulse responses | Very realistic, static | Higher CPU, IR management | Own impulse responses | Later, if needed |

### Proposed parameters (permanent IDs; V1)

| ID | Range / values |
|---|---|
| `rev_mix` | % send-equivalent wet |
| `rev_type` | Plate / Room / Hall |
| `rev_size` | — |
| `rev_decay` | s |
| `rev_predelay` | ms |
| `rev_tone` | damping |
| `rev_lowcut` | Hz |

Defaults are neutral (mix 0 = bit-exact pass-through, stage idle).

### Ducking

- A new `space_duck` parameter makes the echo and reverb wet signals dip while the dry vocal is active: an envelope follower on the dry signal drives a gain on the wet paths.
- Common in vocal mixing because it keeps words clear while tails fill the gaps [D: industry practice].
- Zero latency.

### R&D gate

The reverb ships only after passing the measurement tests in Deliverable 8 §3 (Reverb row) and the listening gate in Deliverable 8 §2.4:

- modal density;
- ringing detection;
- decay accuracy;
- mono fold-down;
- CPU.

It starts in Phase 1, in parallel, because it gates genre tuning.

## 6. Bypass, compare and latency

| Issue (Deliverable 1) | Change |
|---|---|
| Host bypass not linked [V] | Override `getBypassParameter()` to return the existing `bypass` parameter. JUCE's wrappers then route the host's bypass to the latency-aligned crossfade [A: confirm in the pinned JUCE]. [T] in every host. |
| Compare matched only on START [V] | Level-matched compare **on by default** whenever Bypass is used; a setting allows true bypass. |
| Limiter runs on the dry path [V] | Bypassed audio skips the user limiter. A fixed, transparent safety limiter (ceiling −0.3 dBFS) runs **only while matching raises the dry level**. **Decision needed:** the alternative is to keep today's behaviour and document it. |
| Runtime latency changes [V] | The limiter's look-ahead is always applied and reported (the dry path is delayed even when the limiter is off). Latency then changes only when Tune is switched. **[T] per host**; if any host misbehaves, add a “Constant latency” option that always reports the Tune latency. |
| `getTailLengthSeconds` = 0 [V] | Report echo and reverb tail lengths so hosts don't cut tails on stop or offline bounce. |

## 7. Analysis upgrades

| Capability | Plan | Reuse |
|---|---|---|
| **Live key evidence** (TUNE suggestion) | Copy the analyser input to a ring buffer (audio thread, lock-free). A worker thread runs YIN at a reduced rate on voiced frames and builds the same pitch-class histogram and key fit as `VocalFileAnalysis`. It publishes a result only when `keyReliable`. | `YinPitchDetector` [V], key method in `VocalFileAnalysis` [V] |
| Finer beat/vocal spectra | Add the 19-band third-octave profile (200 Hz–12.8 kHz) to both live analysers | `ReferenceAnalyzer` / `SpectralProfile` [V] |
| Beat loudness, headroom | Short-term/integrated LUFS and true peak for the instrumental | Mastering's BS.1770 meter [V] (moves out of `Mastering/` into a shared analysis module) |
| Stereo image of the beat | Per-band mid/side energy and correlation (the analyser sums to mono today [V]) | New, small |
| Transient/rhythmic density | Onset rate and crest per band [R]: useful for ducking depth and delay division choice | New; research gate (Deliverable 6) |
| Beat signature | A compact spectral and loudness fingerprint, to detect “a different beat” without re-analysing continuously | New |

All analysis runs on the message or worker thread except sample capture.

## 8. Generate, Previous mix, undo

- **Generate:**
  - `MixEngine::buildMix` [V], extended with genre targets for the new reverb and ducking, AIR LAB presence and FINISH limiter defaults;
  - deterministic;
  - writes detail parameters in **one undo transaction**;
  - resets macros to 0;
  - clears the Beat Lock layer and ownership, then re-applies Beat Lock if a beat analysis exists.
- **Previous mix:** the full state before the last Generate or genre change (detail parameters + macros + Beat Lock layer), swappable.
- **Undo/redo:** a `juce::UndoManager` over parameter gestures and engine applications. It replaces the single-step undo [V] and the separate KID history.

## 9. Performance budget (to measure; [A] targets)

| Block | Target at 48 kHz / 128 samples (M1 performance core) |
|---|---|
| Today's V1 stages (EQ, CLEAN, compressor, saturation 4×, AIR, Echo, Dimension, Doubler, limiter) | Measure first; expect ≤ 2.5 % |
| Pitch Tune (when on) | Measure; expect ≤ 1.5 % |
| Reverb + ducking | ≤ 2 % |
| **Total V1** | **≤ 5 %**; Windows reference ≤ 8 % |

There is no measured CPU figure anywhere in the repository today [D]. Measuring is Phase 1 work (Deliverable 8 §5).

## 10. Build configuration

| Option | V1 product | Lab build |
|---|---|---|
| `GLITCHKID_SPEECH` (exists) | OFF | ON |
| `GLITCHKID_LAB_FEATURES` (new) | OFF: Formant, Harmony, Mutation, Stutter, Mastering, KID, Reference Match not compiled or registered | ON: all of today's features, same parameter IDs |
| `FORMATS` | VST3 + **AU** | VST3 |
| macOS architectures | **arm64 + x86_64** (universal) | arm64 |
| Windows x64 | **New target** (MSVC, VST3) | — |

CI builds both configurations and runs each one's tests (Deliverable 8 §6).
