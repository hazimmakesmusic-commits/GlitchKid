# Deliverable 9 — Phased implementation roadmap

## Approach

Small, testable, reversible steps. Each phase ends with its tests passing in CI (from Phase 1 on) and a short report.

- **Sizes** are relative (S/M/L/XL) [A]; no calendar dates are promised.
- **Phase 0 is this document set.** Production work starts only after it is reviewed.

## Why the brief's phase order was changed

| Change | Reason |
|---|---|
| **Licensing and the build/CI matrix move to Phases 0–1** | Commercial release is blocked until the JUCE licence is confirmed (`LICENSES.md:10` [D]). AU, Windows and universal builds are the riskiest unknowns, and they are cheap to discover early and expensive late. |
| **Reverb R&D starts in Phase 1, in parallel** | It is the only new DSP and the largest sound-quality risk. Genre tuning (Phase 3) cannot finish without it. |
| **The layered model and macro *architecture* move into Phase 2** (tuning stays in Phase 5) | Generate (Phase 3) must reset macros and the UI must show effective values. Building Generate on today's overwrite model would mean rebuilding it later. |
| **State versioning comes first** | Every later change touches saved state; migration must exist before the first V1 field is added. |

## Phase 0 — Audit, specification and decisions (this document set)

| | |
|---|---|
| Tasks | Review Deliverables 1–10; answer the decisions in §9; confirm the JUCE licence tier |
| Dependencies | — |
| Risks | The licence answer changes the plan (for example, AGPL is incompatible with closed source) |
| Acceptance criteria | Owner sign-off on scope, the 7 genres, the layered model, the launch gates (Deliverable 8 §2.4) and the visual direction |
| Tests | — |
| Definition of done | Decisions recorded in `docs/DECISIONS.md` |

## Phase 1 — Foundation (L)

| | |
|---|---|
| Tasks | 1. `stateVersion` + migration framework + fixture sessions from today's build (Deliverable 3 §4).<br>2. Build options: `GLITCHKID_LAB_FEATURES` (default OFF) and `GLITCHKID_SPEECH` OFF for V1.<br>3. CI: macOS universal (VST3 + AU), Windows x64 (VST3), Linux; both configurations.<br>4. Host bypass via `getBypassParameter()`; constant limiter latency; report tails.<br>5. Offline render CLI + metric suite (Deliverable 8 §2.3, §2.6).<br>6. CPU and latency baseline measurement.<br>7. **Parallel: reverb R&D** (Deliverable 4 §5): prototypes, metrics, first listening.<br>8. Fix stale docs (README, TESTING). |
| Dependencies | Phase 0 decisions |
| Risks | AU or Windows build issues (whisper excluded helps); host bypass behaviour differs between wrappers [T]; CI minutes |
| Acceptance criteria | Both configurations build on all three OS in CI. All existing tests pass in both (Lab = today's 463 groups). pluginval strictness 10 and `auval` pass. Migration tests load every fixture. The render CLI output is bit-identical to the plug-in. A CPU baseline is published. |
| Tests | Migration, bypass alignment, render equivalence, validators |
| Definition of done | Green CI on main; a baseline report committed |

## Phase 2 — Five-stage workflow and the layered model (XL)

| | |
|---|---|
| Tasks | 1. Layered sound model: macro parameters (`macro_*`), Beat Lock layer slot, ownership, effective-value reads in `read*Settings()` (Deliverable 4 §2).<br>2. Macro curves v0 (Deliverable 4 §3), PUNCH loudness compensation.<br>3. New UI shell: top bar, flow column, MIX tab, five stage tabs with independent Advanced, status line (Deliverable 7).<br>4. Stage panels on existing parameters (TUNE, CLEAN, SHAPE, SPACE without reverb, FINISH).<br>5. Compare: matched bypass by default; A/B; Previous mix; `UndoManager`.<br>6. Remove the rail, START dials, MANDALA and psychedelic layers; hide Lab screens in V1. |
| Dependencies | Phase 1 (state version, CI) |
| Risks | Effective-value display and editing complexity; automation interplay [T]; UI regressions |
| Acceptance criteria | All layers neutral = bit-exact. Macros are automatable and never write detail parameters. Every V1 control reachable in ≤ 2 clicks. Every existing V1-class parameter is still reachable (in Advanced where needed). FL Studio smoke test passes [T]. |
| Tests | Deliverable 8 §3 rows: layered model, macros, bypass, state round trip; UI behaviour and paint-budget tests updated |
| Definition of done | The new UI ships in internal builds; an owner usability pass with notes |

## Phase 3 — Genres and one-click Generate (L)

| | |
|---|---|
| Tasks | 1. Reverb and ducking integrated (from the parallel R&D) with permanent parameter IDs.<br>2. 7 genre profiles (Deliverable 5) generated into `Profiles.inc`.<br>3. `MixEngine` extensions: reverb, ducking, CLEAN amount, AIR rules.<br>4. Generate flow and states (listening, too little audio, re-generate confirmation).<br>5. Genre tuning rounds with the corpus (Deliverable 8 §2.1). |
| Dependencies | Phase 2; reverb R&D gate passed |
| Risks | **Genre sound quality** (the primary launch risk); reverb character; corpus licensing |
| Acceptance criteria | Generate is deterministic and every value is in range. Per genre: the objective metrics are inside their corridors, and an internal listening round shows Generate ≥ raw + 15 on ≥ 90 % of items (preliminary for the Phase 6 gate). |
| Tests | Generate unit tests, golden renders per genre, metric suite |
| Definition of done | 7 frozen genre profiles with a tuning log |

## Phase 4 — Beat Lock, automatic (L)

| | |
|---|---|
| Tasks | 1. State machine and UI (Deliverable 6 §6).<br>2. Analysis upgrades: third-octave bands, LUFS/true peak, stereo, active-frame masking; beat signature.<br>3. New rules (reverb, ducking, de-ess coupling) behind research gates.<br>4. Layer application, ON/OFF compare, Re-adapt, new-beat prompt.<br>5. Validation corpus of vocal × beat pairs. |
| Dependencies | Phase 2 (layer), Phase 3 (genre scaling, reverb) |
| Risks | Sidechain routing differs per host [T]; masking estimates may not track perception [R]; transient-density usefulness [R] |
| Acceptance criteria | Deliverable 6 §8 pass conditions on the corpus. Never re-adapts silently. Manual edits always win (tests). |
| Tests | Deliverable 6 §8 |
| Definition of done | Beat Lock works in at least FL Studio and Reaper, Mac and Windows |

## Phase 5 — Macros, pitch suggestion, A/B, bypass and saving finalised (M)

| | |
|---|---|
| Tasks | 1. Macro curve tuning by ear (keeping the bounds and monotonicity).<br>2. Live key evidence and the TUNE suggestion card (Use / Keep mine).<br>3. Presets: genre tags, the `<Generation>` element, browser grouped by genre.<br>4. Full undo coverage.<br>5. Settings: true bypass option; constant latency option if any host needs it. |
| Dependencies | Phases 2–4 |
| Risks | Key estimate reliability on rap (mostly unvoiced) → hidden when not reliable (by design) |
| Acceptance criteria | Key suggestion never changes key or scale by itself (test). Suggestion accuracy on the sung corpus ≥ 90 % where shown [A]. Presets and sessions round-trip every field. |
| Tests | Deliverable 8 §3 state rows; key-suggestion corpus test |
| Definition of done | Feature-complete V1 candidate |

## Phase 6 — Cross-DAW validation and audio-quality optimisation (L)

| | |
|---|---|
| Tasks | 1. Full host matrix (Deliverable 8 §4) on Mac (arm64 and Intel) and Windows.<br>2. CPU optimisation to the budget.<br>3. Formal listening panel against the launch gates (Deliverable 8 §2.4).<br>4. Fix what fails; repeat. |
| Dependencies | Phase 5 |
| Risks | Host-specific bugs (latency changes, bypass, sidechain); failing the sound gate (time for re-tuning is planned) |
| Acceptance criteria | Every host/OS pair to be claimed passes every case. Sound gates met. CPU within budget. 8-hour stability run clean. |
| Tests | Deliverable 8 §2, §4, §5 |
| Definition of done | Release candidate; a support matrix document listing only passed host/OS pairs |

## Phase 7 — Commercial polish and launch readiness (M)

| | |
|---|---|
| Tasks | 1. First-run guidance (inline, skippable).<br>2. Manual and help with host routing guides (verified steps).<br>3. Installers: macOS pkg, signed and notarised; Windows installer, signed.<br>4. Licence/EULA, privacy statement (no data collection).<br>5. Marketing-claims review against Deliverable 2 §1.<br>6. Launch checklist (Deliverable 10). |
| Dependencies | Phase 6; the licence confirmed (Phase 0) |
| Risks | Apple notarisation of a plug-in bundle; Windows SmartScreen reputation; claims creep |
| Acceptance criteria | Deliverable 10 fully checked, with evidence links |
| Tests | Install/uninstall on clean machines; upgrade over an internal beta |
| Definition of done | Go/no-go signed by the owner |

## Workstreams across phases

| Workstream | Phases | Note |
|---|---|---|
| Reverb DSP | 1 → 3 | Gate before genre freeze |
| Corpus and listening panel | 1 (prepare) → 3, 4, 6 | Rights-cleared material; panel recruitment early |
| Host testing | 1 (smoke) → 6 (full) | Two Macs (arm64 and Intel) and one Windows machine needed |
| Deferred features (Lab build) | All | Kept compiling and tested; no feature work |

## Decisions needed from the owner (before Phase 1)

1. JUCE licence tier (or another licensing path); repository visibility.
2. Approve the 7 genres, or trim to 5 (ATMOSPHERIC and NATURAL first to go).
3. Approve the layered model and automatable macros (Deliverable 4 §2).
4. Bypass dry path: remove the user limiter and add a matched-safety limiter, or keep today's behaviour (Deliverable 4 §6).
5. Whether Generate switches CLEAN on (proposed), which changes today's rule that profiles keep CLEAN as the user set it.
6. The launch sound gates and panel (Deliverable 8 §2.4).
7. Platforms at launch: Intel Macs (universal) yes or no; Windows yes (required for “cross-DAW” as briefed).
8. Visual direction and the Glitch Relic artwork's place.
9. Minimum window scale 80 % (proposed) versus 70 % today.
