# Deliverable 3 — Feature decision matrix

Every existing feature, classified and decided. Facts come from Deliverable 1; the tags are explained there.

**Classes**

| Class | Meaning |
|---|---|
| **A** | Essential for V1 |
| **B** | Useful but not essential |
| **C** | Creative expansion for a later version |
| **D** | Redundant or unnecessary |
| **E** | Technically incomplete or unreliable |

**Decisions:** Keep · Simplify · Defer · Remove · Investigate.

**A key fact shapes the whole strategy:** nothing has been released to the public (`CHANGELOG.md:3`). Today, compatibility affects only the owner's own projects and presets. V1 is therefore the one moment to define a clean, permanent product surface. After V1, every parameter ID is permanent (`docs/DECISIONS.md` #2).

## 1. Audio processing

| Feature | Purpose today | Class | Decision | Covered elsewhere? | Presets / sessions | DSP architecture impact | Safe to remove from V1? |
|---|---|---|---|---|---|---|---|
| Pitch Tune (`tune_*`) | Pitch correction | A | **Keep · Investigate** quality on real vocals [T] | — | Unchanged | None | — |
| EQ (4 bands, 10 params) | Tone shaping and correction | A | **Keep** (SHAPE) | — | Unchanged | Macros add offsets (Deliverable 4) | — |
| CLEAN LAB (corrective + de-esser) | Rumble, mud, resonances, S sounds | A | **Keep** (CLEAN) | — | Unchanged | None | — |
| Compressor | Dynamics | A | **Keep · Investigate** an RMS/program-dependent detector option for vocals [R] | — | Unchanged | PUNCH offsets | — |
| Saturation | Warmth | A | **Keep** (SHAPE · Warmth) | — | Unchanged | None | — |
| AIR LAB | Presence and air that ease off on S sounds | A | **Keep** as the PRESENCE macro's engine and SHAPE's Air | Overlaps EQ Air, but sibilance-aware | Unchanged | PRESENCE offsets | — |
| Liquid Echo | Tempo delay | A | **Keep** (SPACE · Delay); add ducking [new] | — | Unchanged | SPACE offsets | — |
| Dimension | Stereo width | A | **Keep** (SPACE · Width) | — | Unchanged | SPACE offsets | — |
| Gain, Mix | Output level, dry/wet | A | **Keep** (FINISH) | — | Unchanged | None | — |
| Output Limiter | Output safety | A | **Keep · Simplify**: on by default in generated mixes; latency always reported (Deliverable 4 §6) | — | Unchanged | Constant latency | — |
| Bypass + level-matched compare | Compare with the original | A | **Keep · Simplify**: matched by default; map host bypass; take the limiter off the dry path | — | Unchanged | Bypass path (Deliverable 4 §6) | — |
| **Reverb** | — (missing) | A | **New DSP** (SPACE · Reverb) | No: Echo/Dimension are not reverb | New parameters | New stage (Deliverable 4 §5) | — |
| Doubler (`dbl_*`) | Synthetic double | B | **Simplify:** keep the DSP as an internal SPACE texture used by genres; no V1 control | Dimension widens; Doubler thickens | 7 of 8 profiles use it [V]: keeping it preserves their sound | None | Not removed |
| Formant (`formant_shift`) | Voice size | C | **Defer** | — | Sessions with it ≠ 0 change sound in V1 (owner only) | Removes ~21 ms optional latency path | Yes, behind a build flag |
| Harmony (`harm_*`) | One fixed-interval voice | C + E | **Defer**: prototype quality, not key-aware [V] | — | Same | Removes ~31 ms optional path | Yes |
| Mutation (`mut_*`) | Ring mod / frequency shift | C | **Defer** | — | Same | — | Yes |
| Glitch Stutter (`stutter_*`) | Beat repeat | C | **Defer** | — | Same | — | Yes |

## 2. Engines, data and tools

| Feature | Purpose today | Class | Decision | Notes, compatibility, safety |
|---|---|---|---|---|
| Factory profiles (8) | Starting sounds | A | **Simplify** into 7 retuned genres (Deliverable 5) | Profile names in sessions/presets become genre IDs via a mapping table. The old names keep loading (§3). |
| Build My Mix (`MixEngine`) | Analysis-fitted starting mix | A | **Keep · extend** as the Generate engine (new SPACE/reverb targets, PRESENCE, stage outputs) | Deterministic; already pure and tested [V] |
| Beat Lock (`BeatLock`, `SourceRelationship`) | Fit to the instrumental | A | **Keep · extend:** automatic, layered, finer analysis (Deliverable 6) | Existing rules and tests stay the base |
| Instrumental input bus | Beat input | A | **Keep** | — |
| `VocalAnalyzer` | Live measurements | A | **Keep · extend** (live key evidence; Deliverable 4 §7) | — |
| `VocalFileAnalysis` | Offline vocal-file report, key estimate | B | **Keep the engine** (its key method feeds TUNE). **Defer the file-drop UI.** | — |
| `VocalDiagnosis` (KID) | Plain-language findings | B | **Investigate:** reuse findings for CLEAN telemetry and Generate's evidence lines | Part of `Kid/`; would need moving out of KID |
| Macros TONE/PUNCH/SPACE | Big controls | A | **Simplify / redesign:** offset model, add PRESENCE, real automatable parameters (Deliverable 4 §2) | Old macros wrote ordinary parameters, so nothing in old sessions references them |
| Quick commands (DARKER, MORE SPACE, HARDER TUNE) | One-tap nudges | D | **Remove** | Redundant with macros; not stored anywhere |
| Tune styles (Natural/Light/Hard) | Strength and speed presets | A | **Keep** | Read back from values [V] |
| User presets (`PresetStore`) | Save/recall | A | **Keep · extend:** genre tag, generation context | Format v1 readers ignore unknown elements [V] (forward-compatible) |
| A/B slots | Compare | A | **Keep** | In session [V] |
| Undo (single “Back to my sound”) | Undo builds | A | **Simplify:** Previous mix + one undo/redo history (Deliverable 4 §8) | — |
| Reference Match | Tonal hint from a song | C | **Defer.** Fix the 3 dB bound bug before any future release. | Session flag/UI only; no parameters |
| KID (rules, panel) | Assistant | C | **Defer** | Not in sound state; KID history not saved [V] |
| KID speech (whisper.cpp) | Push-to-talk | C | **Defer:** build with `GLITCHKID_SPEECH=OFF` | Removes a large dependency, the model download, mic permission and network use from V1 |
| Alter Egos + Mood Engine | Creative identities, Chaos, Surprise | D | **Remove from V1** (orphaned in the UI [V]); keep the code until V1.x cleanup | Presets/sessions carry `<AlterEgo>`. V1 ignores it (unknown elements are ignored [V]); the parameters it set are ordinary values and still load. |
| AutoMix / “ASTRO MODE” | Older auto mix | D | **Remove** `applyAutoMode`/`previewAutoMode` and the decision code; keep the shared types (`RangeLookup`, `Result`) used by `MixEngine` | Orphaned [V]. `AutoModeTests.cpp` mixes kinds: retire its rules-engine and ASTRO MODE groups; **keep** its `VocalAnalyzer` groups (the analyser is A-class) [V] |
| Mastering Lab (`master_*`, chain, export) | Master a full mix | D for this product | **Remove from the V1 build** (flag), keep the code | Sessions with `master_on = 1` (owner only) would open as a vocal chain; V1 shows a notice |

## 3. Interface

| Feature | Class | Decision | Why |
|---|---|---|---|
| 11-item rail | D | **Remove**; replaced by Session view + 5 stage tabs (Deliverable 7) | Too many destinations for the V1 promise |
| START seven dials | D | **Remove**; replaced by the 4 macros + Generate | Mixed concepts (Tune, Clean, Mix, Output are stage controls) |
| SOUNDS page | A | **Simplify** into genre cards + Generate | — |
| CLEAN / TUNE / TONE / DYNAMICS / SPACE pages | A | **Simplify** into TUNE · CLEAN · SHAPE (Tone + Dynamics) · SPACE · FINISH | Matches the five locked stages |
| Shared Advanced switch | A | **Keep the concept**, one per stage | Fixes the shared-switch bug |
| CREATIVE, REFERENCE, MASTER, KID screens | C / D | **Defer** with their features | — |
| MANDALA, psychedelic paint, op-art rings | D | **Remove** from V1 | Conflicts with the brief's visual direction (no excessive gradients, no concept art) |
| Glitch Relic artwork | — | **Decision needed** (owner-supplied art) | Brand asset; may suit an About screen, not the workspace |
| MOTION preference | A | **Keep** (reduced motion) | Accessibility |
| Display taps / spectrum / ribbon | B | **Simplify** to an input/output meter and an optional waveform | Brief: no unnecessary meters |

## 4. Deprecation and backward-compatibility strategy

### Rules

1. **V1 defines the permanent parameter set.** It contains:
   - the A-class parameters;
   - the Doubler (internal);
   - new reverb, ducking and macro parameters.

   From V1's release, IDs are never renamed or reused (existing rule).
2. **Deferred features keep their implementation** in the repository, behind a build option (proposed `GLITCHKID_LAB_FEATURES`, default OFF). A Lab build compiles them, registers their parameters (with the same IDs as today) and runs their tests in CI, so deferred code cannot rot silently.
3. **Remove means: not in the V1 product build.** Code with no future (AutoMix decision logic, quick commands) is deleted in V1. Orphaned-but-maybe-useful code (Alter Egos) is deleted after V1 ships, once no migration test needs it.
4. **Host automation safety.** JUCE's VST3 and AU wrappers derive parameter identity from the string ID unless legacy IDs are forced [A: JUCE behaviour, to confirm against the pinned JUCE source]. The code does not force legacy IDs: there is no `JUCE_FORCE_USE_LEGACY_PARAM_IDS` [V]. Removing a parameter should therefore not shift the others. **[T] Verify in every V1 host before release**, because existing source comments assumed index stability mattered.

### Session migration (state version)

| Step | Behaviour |
|---|---|
| Write | `stateVersion` attribute on the state root (V1 = **2**; absent = legacy **1**). |
| Load v1 | Map profile/preset names to genre IDs where they match. Initialise macros at 0. Mark the Beat Lock layer empty. Keep every V1 parameter value. Ignore removed parameters' values. |
| Deferred effect active in a v1 session | e.g. `mut_on = 1`, `formant_shift ≠ 0`, `master_on = 1`. Show once: *“This project used features that aren't in this version (Mutation). They're switched off here.”* The project is not modified until the user saves. |
| Load a newer state in an older build | Unknown elements ignored (existing behaviour [V]). Document “not supported” beyond that. |
| Tests | Fixture sessions captured from today's build (each deferred feature on; Alter Ego set; A/B filled; master mode on), loaded by the V1 migration tests. |

### Preset migration

- Preset format v1 is kept. V1 adds an optional `<Generation genre=".." tone=".." punch=".." space=".." presence=".."/>` element, which v1 readers already ignore [V].
- Presets from today load as flattened sounds. Values for removed parameters are ignored, and the preset list shows “(older preset)”.

### Order of work

1. State version and migration tests first (Phase 1).
2. Build flag and the CI matrix (Lab and V1 builds).
3. UI removal.
4. Code deletion of D-class items.

No step deletes the owner's files or presets.
