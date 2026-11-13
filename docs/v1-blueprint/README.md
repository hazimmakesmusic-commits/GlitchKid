# GlitchKid V1 blueprint

The technical and product blueprint for turning GlitchKid into a focused vocal-mixing plug-in:

> **GlitchKid turns a raw vocal into a professional starting mix as quickly and effortlessly as possible — without taking control away from the artist.**

**Status: for review.** No production code, DSP, parameter, preset, build file or test was changed to write these documents. Production work starts only after review (Deliverable 9, Phase 0).

- **Source audited:** branch `claude/glitchkid-plugin-architecture-3o6w74`, commit `4a16b44` (2026-11-13).
- **The source code is the authority.** Where a document disagrees with it, the code wins (see Deliverable 1 §16).

## Evidence tags (used in every document)

| Tag | Meaning |
|---|---|
| **[V]** | Verified by reading the source; file and line given |
| **[D]** | Stated in the repository's docs, not re-verified |
| **[A]** | Assumption, inference or proposed value |
| **[T]** | Needs testing (host, hardware or listening) |
| **[R]** | Needs research before it can be relied on |

## The ten deliverables

| # | Document | What it settles |
|---|---|---|
| 1 | [Current product audit](01_CURRENT_PRODUCT_AUDIT.md) | What exists, works, is incomplete or broken; doc-vs-code discrepancies |
| 2 | [V1 specification](02_V1_SPECIFICATION.md) | Journey, five stages, macros, compare, saving, non-functional requirements |
| 3 | [Feature decision matrix](03_FEATURE_DECISION_MATRIX.md) | Keep / simplify / defer / remove for every feature; deprecation and compatibility |
| 4 | [DSP architecture plan](04_DSP_ARCHITECTURE_PLAN.md) | Layered sound model, macro curves, new reverb, bypass and latency, analysis |
| 5 | [Genre profiles](05_GENRE_PROFILES.md) | 7 launch genres, adaptation rules, tuning process |
| 6 | [Beat Lock specification](06_BEAT_LOCK_SPEC.md) | What can be measured, rules, automatic activation, validation |
| 7 | [UX architecture](07_UX_ARCHITECTURE.md) | Window structure, states, interactions, visual direction |
| 8 | [Testing strategy](08_TESTING_STRATEGY.md) | Sound-quality gate, host matrix, performance, CI |
| 9 | [Implementation roadmap](09_IMPLEMENTATION_ROADMAP.md) | Phases 0–7 with tasks, risks, acceptance criteria; owner decisions |
| 10 | [Launch readiness checklist](10_LAUNCH_READINESS_CHECKLIST.md) | Objective release conditions |

## Headline findings

1. **The core already exists.** A deterministic, analysis-fitted mix engine (Build My Mix), Beat Lock rules, CLEAN/AIR/EQ/compressor/saturation, pitch correction, A/B, safe presets and a loudness-matched compare engine are all in the code, with 463 unit-test groups.
2. **No reverb exists.** The brief's SPACE stage needs new DSP. This is the largest sound-quality risk.
3. **Macros overwrite the generated mix** and there is no PRESENCE macro. V1 needs a layered model where macros are offsets (Deliverable 4 §2).
4. **Beat Lock is manual today.** Automatic activation, a switchable layer and ownership rules are new.
5. **Key detection exists only for a loaded file** and is used only by KID. TUNE needs a live suggestion.
6. **Cross-DAW is not possible yet:**
   - VST3 only (no AU, so no Logic Pro);
   - Apple Silicon only;
   - no Windows build;
   - no host but FL Studio has ever been tried;
   - the host bypass is unmapped.
7. **No session-state versioning.** It is needed before any V1 change to the saved state.
8. **Commercial blockers:** the JUCE licence tier is unconfirmed; there is no signing, notarisation or installer.
9. **Nothing has been released to the public.** V1 is the one chance to define a clean, permanent parameter set; compatibility today affects only the owner's projects.
10. **One bug in deferred code:** Reference Match can move a band 6 dB in one step, against its own documented bound.

## Not verified in this audit

- **Tests not re-run.** The unit tests were last run on 2026-11-05 (467 groups, 0 failures) on code identical to the audited commit. They were not re-run, because this session has no build and the brief forbids building.
- **JUCE behaviour.** Statements about JUCE internals (parameter identity, the host-bypass wiring) are marked [A]: the JUCE source is not in the repository.
- **Hosts, CPU, listening.** No host behaviour, CPU figure or listening judgement was measured.
