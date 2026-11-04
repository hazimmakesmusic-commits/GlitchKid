# Implementation plan — GlitchKid Vocal Playground v2

**No production code was written in this phase.** This plan maps every proposed UI feature to what the repository really contains today. It was checked against the source on 2026-11-04, with the uncommitted visual-pass changes from the previous sprint present in the working tree.

## Legend
| Tag | Meaning |
|---|---|
| **EXISTS** | The capability is in the plug-in today (file named) |
| **UI-ONLY** | Needs editor work only; the processor and engine APIs already exist |
| **NEW-ENG** | Needs new engineering beyond the editor (processor, engine, settings, threading) |
| **UNVERIFIED** | An assumption not yet proven (usually host behaviour) |
| **DEFERRED** | Not for the first release of this redesign |

## 1. Feature map

### Frame and navigation
| Proposed feature | Production today | Tag | Work |
|---|---|---|---|
| One 1120 × 680 frame, scaled as a whole | `PluginEditor.cpp`: design 1120 × 680; `setResizeLimits (70 %…150 %)` with a fixed aspect ratio | EXISTS | Keep it. **Recommend raising the minimum to 80 %:** the smallest text is 12 px, which is 9.6 px at 80 % and 8.4 px at 70 % |
| Rail: BOOTH / SHAPE / CHARACTER / SPACE / SCENES + KID | Rail with 11 rows in 5 groups (`railItems`) | UI-ONLY | New rail model. **Open question:** where BEAT, REFERENCE and MASTER go (see §4) |
| Top bar: loaded sound, ‹ ›, A │ B, Save, Undo, DRY │ PROCESSED, Help | Preset bar at the bottom (‹ › / SAVE / DELETE / SET A / SET B / A / B); ORIGINAL / PROCESSED / UNDO / MOTION at the top | UI-ONLY | Re-layout; same calls |
| Unsaved indicator (text + dot) | `GlitchKidProcessor::isPresetModified()` | EXISTS → UI-ONLY | Show it as text in the top bar |
| Chain strip 1 → 17, lit when on | Order fixed in `processChunk`; `kid::Director::stageOf (id)` already maps parameters to stages 1…17 | UI-ONLY | One activity rule per module, mirroring the prototype's `modOn` (e.g. Formant is "on" when not 0; the compressor when ratio > 1) |
| Help line + accessible descriptions | Help line exists ("Point at any control…"); tooltips exist | EXISTS → UI-ONLY | Set `juce::AccessibilityHandler` descriptions. **UNVERIFIED:** VoiceOver / Narrator in FL Studio |
| MOTION and reduced motion | `uiMotion` flag exists | EXISTS | Also read the OS reduced-motion setting: **NEW-ENG (small)**, per platform |

### Booth
| Feature | Today | Tag | Work |
|---|---|---|---|
| Signature visual from real levels | `DisplayTap` input and output taps; `MainScreen` ribbon and Mandala use them | EXISTS → UI-ONLY | Draw the v2 form from the taps and the parameter state. **Budget:** a repaint under 16 ms on the software renderer (today's Mandala test is about 16 ms; keep a test like it) |
| Big moves Tone / Punch / Space | `Macros/MacroEngine.cpp` (formulas copied exactly into the prototype) | EXISTS | — |
| Signal card: none / receiving / clipping | Input and output peaks (`consumeInputPeak`, `outputPeak`); the editor shows CLIP for the *output* at ≥ 0.999 | UI-ONLY | Add *input* clipping from the input peak. `VocalAnalyzer` already counts clipped and near-clipped samples |
| Next step card | — | UI-ONLY | Pure UI rules (prototype `viewBooth`) |
| Mastering mode state | `isMasteringMode()` | EXISTS | Show it as in the prototype |
| "Not processing" state | No defined trigger | UNVERIFIED / DEFERRED | Define real triggers first (e.g. unsupported layout, non-finite input bursts) |

### SHAPE / CHARACTER / SPACE
| Feature | Today | Tag | Work |
|---|---|---|---|
| All 68 sound parameters with real ranges and units | `createLayout` (+ 7 `master_*`, out of scope here) | EXISTS | Re-house the existing attachments |
| Stage badge and neighbours per module | — | UI-ONLY | Static table (prototype `MOD`, `STAGE_NAME`) |
| Advanced / More disclosure | TUNE/TONE `ADVANCED` flag (`uiShapeAdv`), LAB `MORE` | EXISTS (partly) | Generalise. **Decision needed:** remember per module or not |
| Module Reset (undoable) | LAB per-card Reset (uses host gestures) | EXISTS (CHARACTER) → UI-ONLY elsewhere | Same pattern for every module |
| Tuning style chips | `macros` tune styles 50/80, 75/35, 100/0 | EXISTS | — |
| AIR LAB Link behaviour | Editor `linkAir` | EXISTS | Keep the existing behaviour (the prototype only toggles the parameter) |

### Scenes and presets
| Feature | Today | Tag | Work |
|---|---|---|---|
| 8 factory profiles + Factory Default | `Presets/FactoryPresets.*`, `Profiles.inc`. Loading keeps the 22 user-owned parameters | EXISTS | Card UI uses `FactoryPresets::info` (name, tagline, description, settings, start-with). **UNVERIFIED:** whether the profiles were listening-tested — this plan can't confirm it |
| Thumbnails drawn from real values | — | UI-ONLY | Offline render of the visual per profile |
| Preview a scene without loading | — | **NEW-ENG** | Same engine as Audition (§2) |
| Scenes › Yours | `PresetController::allPresetNames`, `loadUser` | EXISTS → UI-ONLY | — |
| Save / Save As / Update with confirmation | `PresetController::saveAs (name, overwrite)` returns `needsOverwriteConfirmation`; already tested ("save asks before replacing") | EXISTS → UI-ONLY | Name validation copy as in the prototype. **Check:** the existing validator's character rules |
| Delete with confirmation | PresetController delete | EXISTS → UI-ONLY | — |
| Concept scenes (AFTER HOURS …) | — | DEFERRED | Sound design, a listening test, and a naming and trademark check |

### A/B and undo
| Feature | Today | Tag | Work |
|---|---|---|---|
| Empty slot captures, occupied slot recalls, confirm before overwrite or leaving a changed slot | Processor: `hasAbSlot`, `captureAbSlot`, `recallAbSlot`, `isAbSlotModified`. The editor's SET A / SET B **capture without asking** | UI-ONLY | Implement the approved rules in the editor (prototype `abClick`, `abStore`) |
| Undo with labels, 40 steps, one step per gesture | Editor: one level (`undo` + `backToMine`). KidPanel: its own 50-step history | **NEW-ENG** | One `juce::UndoManager` for the editor. Coalesce gestures (begin/end gesture). Labels. Decide whether it is per editor session or saved |

### KID and Build My Mix
| Feature | Today | Tag | Work |
|---|---|---|---|
| Push-to-talk on a separate command mic | `Kid/KidSpeech.*`: `CommandMic` (system default input, opened only while held, never reaches `processBlock`) | EXISTS | — |
| Local speech recognition | `Recogniser` (whisper.cpp on a background thread); `SpeechModel` (opt-in download, SHA-1 verified) | EXISTS | UI states as in the prototype |
| Words → intent → plan with memory and follow-ups | `kid::parse`, `Director::respond`, `plan`, plus "a little less" / "too much" / locks / role | EXISTS | — |
| Review before apply: per-row ticks, manual edit kept | `Director::setSelected`, `noteManualEdit`, `selectedChanges`. **But `KidPanel::applyPlan` applies immediately** (`KidPanel.cpp`) | UI-ONLY | Show the plan; apply on the button; keep the 50-step memory semantics |
| Revision numbers and "changed in revision n" tags | — | UI-ONLY | Diff the previous plan's rows |
| New words: *psychedelic / trippy / dreamy*, *more / add character*, *keep the clean sound* | Not in `KidDirector.cpp` | **NEW-ENG (small)** | Add the regexes and their direction plus the lock, with tests. Mapping in `INTENT_PARAMETER_MAP.md` |
| Build My Mix review before apply | `automode::buildMix` returns a proposal with per-change from / to / reason. **The editor applies immediately** (`runBuild` → `applyPreset ("Built: …")`) | UI-ONLY | The same review component as KID. Beat Lock already applies only ticked rows (`applyBeatSelection`), a precedent |
| Audition (Current / Proposal) | — | **NEW-ENG** | See §2 |
| KID never touches Gain / Mix / limiter / Bypass | `Director::isAllowed` | EXISTS | — |

### First launch
| Feature | Today | Tag | Work |
|---|---|---|---|
| 5-step onboarding, skippable, remembered | — | UI-ONLY + **NEW-ENG (small)** | A per-*user* "intro done" flag needs a global settings file (`juce::PropertiesFile` in app data). `setUiFlag` is per *project* and would re-show the intro in every new project |

## 2. Audition and Preview — the one significant engineering item

**Requirement.** Hear the ticked changes (or a Scene) *without* applying them. Leaving the audition restores the exact sound. Nothing reaches the host's automation or undo, and nothing is saved.

| Option | How | Pros | Cons |
|---|---|---|---|
| A. Swap parameters | `applyParameterChanges (target)`, then restore the snapshot | No processor change | The host sees parameter changes (automation, "project modified", the host's undo). A crash mid-audition leaves the sound changed. **Breaks "nothing changes until Apply" in the host's eyes.** |
| **B. Processor preview state (recommended)** | The processor holds an optional *preview* parameter snapshot, read by `processChunk` instead of the parameter tree while active. It is set from the message thread through a lock-free handoff (double buffer plus atomic index; no allocation or locks on the audio thread) | The host sees nothing. Exact restore. It also serves Scene Preview | DSP-adjacent work. Every stage must read from one source of values. Smoothing on entry and exit |

**Constraints to solve for option B:**
- **Latency stages.** Pitch Tune (≈31 ms), Formant (≈21 ms), Harmony and the Output Limiter change the latency the host is told about. An audition that turns one of them on would change the host's latency compensation. Possible approaches:
  - restrict the latency-changing rows during audition, with a label;
  - pre-allocate and report the worst-case latency while auditioning;
  - or accept the change and say so.
  
  **Needs a decision.**
- **Smoothing.** A 20 ms crossfade, as Bypass already does, on entering and leaving.
- **DRY still wins.** Bypass applies on top of a preview.
- **Tests:**
  - the parameter tree is untouched while previewing;
  - the exact sound returns afterwards;
  - no allocation on the audio thread;
  - latency behaviour;
  - automation during a preview.

## 3. Suggested order

| Phase | Content | Risk |
|---|---|---|
| 0 | Decide the open questions (§4, and `DESIGN_REVIEW.md` §5). Port the tokens and type to the JUCE LookAndFeel | Low |
| 1 | Frame, rail, chain strip, workspaces re-housing the existing attachments, Scenes (factory and yours), save flows, A/B rules, unsaved indicator, signal and clipping states, Help | Low (UI-ONLY). Existing tests: "navigation never changes the sound", presets and A/B round-trips |
| 2 | One review component; Build My Mix and KID switched to review-before-apply; unified undo | Medium (behaviour change for current users) |
| 3 | Preview engine (§2) for Audition and Scene Preview | **High:** processor change; needs full DSP tests |
| 4 | First launch (global settings), Booth visual from real taps, reduced motion, keyboard shortcuts, accessibility pass | Medium (**UNVERIFIED** host keyboard focus in FL Studio on both OSes) |
| 5 | KID vocabulary additions and tests; concept Scenes once sound-designed | Low |

**Testing.** Each phase keeps the current suite green. Phase 2 needs these tests:
- opening, closing and cancelling a proposal changes nothing;
- only ticked rows apply;
- a hand-moved row is kept;
- undo after apply restores exactly.

These are the same assertions the prototype's `qa/verify-prototype.js` makes in the browser.

## 4. Existing features the v2 information architecture doesn't place yet (must not be lost)

| Feature in the plug-in today | Suggested home | Status |
|---|---|---|
| **BEAT** (Beat Lock: sidechain analysis, ticked suggestions) | A "Fit to the beat" tool reached from the Booth, or KID ("fit it to my beat"); its review already matches the v2 component | Open question |
| **REFERENCE** (Reference Match) | Same as above: a tool with the shared review | Open question |
| **MASTER** (Mastering Lab, Mastering mode) | A mode switch outside the vocal workspaces (Help / More menu or a rail footer); its screen unchanged | Open question |
| The current instrument's **Mandala** view | Becomes an option of the Booth visual | Open question |
| **SOUNDS** page (profiles + Build My Mix) | Replaced by SCENES and Build My Mix | Covered |

## 5. Out of scope here (by the brief)
- Production AI, LLMs or cloud services.
- New DSP or new parameters.
- Licensing, accounts or activation: not designed. **Unresolved product requirement.** The prototype has none and assumes none.
