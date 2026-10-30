# GlitchKid Vocal Lab — redesign v1: implementation plan (revision 2)

Revision 2 reflects your approved decisions: a separate Experimental Lab workspace; Build My Mix review with one checkbox per change and Lab effects proposed off by default; A/B where an empty slot captures, an occupied slot recalls, and nothing is overwritten without confirmation.

Plan only; nothing here is implemented. No DSP, parameter ID, preset value or state-format change is planned except where a step says "appended UI key". Each phase ends at an approval gate; nothing proceeds without your explicit go-ahead.

## 1. Starting point and preconditions

* Base: HEAD `c3653aa` plus the uncommitted working tree (floating-point fix + signal-rail shell). **Before any implementation, decide what the redesign builds on** (Phase 0 checklist: Mac validation, then commit or keep). The redesign replaces the rail shell's navigation, so building on HEAD + the determinism fix alone is also possible; `Tests/MacroTests.cpp`, `CMakeLists.txt` and `CHANGELOG.md` contain edits from both efforts and would need a manual split.
* Engine facts the plan relies on (verified): `automode::buildMix` is pure and returns `sound` plus `changes` (id, from, to, reason); `GlitchKidProcessor::applyParameterChanges`, `applyPreset`, `notePresetSaved (name, state)` (sets preset name and comparison baseline **without** applying), `isPresetModified`, `captureAbSlot` / `recallAbSlot` / `copyAbSlot` / `isAbSlotModified`.

## 2. UI architecture (proposal)

```
GlitchKidEditor (owns layout, timers, scale, persistence)
├── TopBar            preset field + browser button, A/B (state labels) + Save to A|B, Undo, Beat Lock, Reference Match, Dry|Processed
├── WorkspaceBar      six WorkspaceTab buttons + a separate Experimental Lab tab (status from real parameters) + Signal order popover
├── WorkspaceView     one of seven panels (TUNE, CLEAN, TONE, DYNAMICS, SPACE, FINISH, EXPERIMENTAL LAB), each a vertical scroll of ModuleCards
├── ConfirmDialog     in-window confirmation (A/B overwrite, leaving unsaved changes); Escape = Cancel
│   └── ModuleCard    switch (only if a parameter exists), title, chain position, state text, main controls, advanced controls
├── StudioColumn      SpectrumView (existing) + meter row + MacroStrip (existing macros) + BuildMyMixCard + ContextStatus
├── ContextView       hosts the existing Beat Lock UI and ReferencePanel over the workspace
├── BuildReviewPanel  drawer: Direction → Listen → Review → Applied
├── PresetBrowser     overlay: Factory Default, profiles (groups), user presets; "Save current sound as preset…"
└── StatusLine        hover/focus help, last action, reported latency
```

* Keep the existing design-size + uniform `AffineTransform` scaling approach (it is proven in this codebase); new design size 1120 × 680 (a target until FL Studio confirms it).
* Reuse: `ParamKnob`, APVTS attachments, `PlaygroundLookAndFeel` (with new type-scale tokens), `SpectrumView`, `EqGraph` (real response), `LevelBar`/`ReductionBar`, `NoteKey`, `ReferencePanel`, the Beat Lock rows and logic, `refreshState` / `refreshStatus` logic for honest states, the info-line hover help.
* Retire after migration: rail rows, Final Touch page layout code, HOME tiles (their status logic moves into tab states and the studio column).

## 3. JUCE components likely to change

| File | Change | Risk | Tests |
|---|---|---|---|
| `Source/PluginEditor.{h,cpp}` | New layout (top bar, workspace bar, workspace view, studio column, status line); new design size; scale persistence; move every existing control into its card; add controls for Formant, Harmony, Mutation, Stutter (attachments to existing parameters) | Large file; losing a control or attachment; saved UI keys | Control-inventory test (every parameter reachable and attached to the right ID), navigation tests rewritten, overlap/readability tests |
| **New** `Source/UI/WorkspaceView.{h,cpp}`, `ModuleCard.{h,cpp}` | Card container with switch/position/state/advanced toggle | Layout overflow | Readability + overlap at 80/100/150 % |
| **New** `Source/UI/BuildReviewPanel.{h,cpp}` | Review-first Build My Mix | Stale proposals, partial apply | `BuildReviewTests` (section 6) |
| **New** `Source/UI/PresetBrowser.{h,cpp}` | Overlay for existing presets | Must not load on selection, only on Load/double-click | Browser tests |
| `Source/UI/PlaygroundWidgets.{h,cpp}` | Type-scale tokens (12.5 / 13.5 / 14 / 16 / 22), minimum font enforced, card and tab looks | Visual regressions; contrast | `VisualTests` contrast, readability test |
| `Source/UI/LabKit.{h,cpp}` | `SpectrumView` at the new size; meter row | Repaint cost | `UiPerfTests` |
| `Source/UI/ReferencePanel.{h,cpp}` | Lay out inside the context view | Narrower width | `ReferenceMatchTests` UI parts |
| `Source/AutoMode/MixEngine.{h,cpp}` | **Pure helpers only**: `groupChanges (proposal)` (display grouping) and `applySelection (proposal, selectedChangeIds, live, ranges)`; `buildMix` unchanged | Range clamping | `MixEngineTests` additions |
| `CMakeLists.txt` | Add new files to the plug-in **and** the separate `GlitchKidTests` source list | Missing one list | Both targets build |
| `Source/PluginProcessor.*`, DSP, `SoundState`, `PresetStore`, `Profiles.inc` | **No change planned** | — | Existing suites |

## 4. Parameter / APVTS integration

* Every control binds through existing APVTS attachments (`SliderAttachment`, `ButtonAttachment`, `ComboBoxAttachment`) or `ParamKnob`; automation and host undo behave as today.
* The macros keep calling `macros::apply` → `applyParameterChanges` (one gesture per drag, as today).
* Tuning-style chips keep `macros::apply (TuneStyle)`.
* The Lab effects get standard attachments to `formant_shift`, `harm_on/interval/level`, `mut_on/amount/character/blend`, `stutter_on/rate/mix`, shown only in the Experimental Lab workspace. Latency changes when Formant/Harmony switch are already reported by the processor. The Lab tab's status ("n of 4 on") is computed from these parameters so an active Lab effect is visible from every workspace.
* No new parameters. Parameter count stays 69 (a test asserts it and the ID list).

## 5. State persistence

* Keep reading `uiLevel` / `uiCategory` so old sessions open on a sensible workspace; map old values: HOME→TUNE, CLEAN LAB→CLEAN, BEAT LOCK→(TUNE + Beat Lock context open), BUILD→TUNE, FINAL TOUCH category 0/1/2/3→TUNE/TONE/DYNAMICS/SPACE, REFERENCE→(TUNE + Reference context open). `uiWorkspace` gets 0–6 (6 = Experimental Lab).
* **Appended UI keys:** `uiWorkspace` (0–5), `uiScale` (80–150), `uiAdv` (bit mask of open "advanced" sections). Old builds ignore unknown keys.
* A/B slots, preset name and Undo stay as they are in the state (A/B and the preset name are saved; Undo is editor-only and is not saved, as today).
* **A/B rules (editor-side, no processor change):** clicking an empty slot calls `captureAbSlot`; clicking an occupied slot calls `recallAbSlot` — but if the active slot `isAbSlotModified()`, a dialog first offers Cancel / Switch without saving / Save to <slot> and switch. "Save to A/B" on an occupied slot (i.e. always, once captured) asks "Overwrite slot X?" before `captureAbSlot`. The existing SET A / SET B buttons, which capture without asking, are removed.

## 6. Build My Mix review logic

1. **Compute without applying:** call `buildMix` with the live state; keep `proposal` and `snapshot = captureSoundState()`.
2. **Group for reading:** EQ per band (`eq_lo`, `eq_b1`, `eq_b2`, `eq_hi`), then by module prefix (`comp`, `sat`, `echo`, `dim`, `dbl`, `mut`, `harm`, `stutter`, `formant`). Categories: *Fitted to your vocal* (any reason not starting with "From the "), *From the direction*, *Lab effects* (`mut`, `harm`, `stutter`, `formant`), the last proposed off and ticked by default. If no Lab effect is on, the Lab category says so.
3. **Show every change:** one row per change with its own checkbox (ticked by default), a plain description ("Switch Mutation off", "Echo: mix"), the parameter ID, **Current** (live value) and **Proposed** columns, and the engine's reason. A group checkbox only ticks/unticks its rows. Unticking an effect switch while keeping an amount is allowed (the amount is stored but inaudible while the effect is off); the panel says so.
4. **Stale detection** (30 Hz timer): if a proposed parameter differs from `snapshot`, mark that row stale, untick it and show the live value with "was …"; show a Rebuild banner. Edits to parameters outside the proposal are kept.
4b. **Opening, computing, closing (×), Cancel and switching workspace never change the sound.**
5. **Preset load, A/B recall, Undo, Reset while open:** close the panel; message "Sound changed: the proposal was discarded."
6. **Apply:** `undo = {captureSoundState(), name}`; `target = live` with the ticked changes' `to` values (a stale row applies only if the user ticks it again); `applyParameterChanges (target)`; then `notePresetSaved ("Built: " + direction, live-with-every-proposed-change)` so a partial apply shows "● modified" while a full apply does not. **No processor change needed.** (Check during implementation whether to also clear the active A/B slot as `applyPreset` does.)
7. **Cancel / close:** nothing changes.
8. **Undo:** restores the exact pre-apply state and preset name (existing `backToMine`).
9. Never touched (engine facts, also asserted at UI level): `tune_*`, `gain`, `mix`, `bypass`, `air_*`, `clean_*`, `lim_*`.

## 7. Preset-browser improvements

* Phase A (UI only): browser overlay for Factory Default + the eight profiles (existing texts) + user presets from `PresetController::allPresetNames`; select ≠ load; Load or double-click loads; Undo works; "what a load keeps" is stated.
* Phase B (needs listening): run the listening protocol on the eight profiles (at least three real vocals each: melodic rap, Hindi/Hinglish or Punjabi sung, a quiet close take; level within ±1 dB; peaks below −1 dBFS; told apart from the nearest neighbour in 4 of 5 blind trials; description true). Only then consider new presets, appended, generated by `tools/gen_profiles.py`, toward 15–20 **only if** each passes. Fewer is better than weak.
* Categories stay a proposal until the listening results exist.

## 8. Testing requirements

| Area | Tests |
|---|---|
| Compatibility | Parameter count 69 + ID list snapshot; state round-trip with the new UI keys; old `uiLevel`/`uiCategory` sessions open sensibly; presets load identically; Undo exactness |
| A/B | Empty slot captures without changing the sound; occupied slot recalls; leaving a modified slot asks (Cancel keeps everything; switch without saving leaves the old slot untouched; save-and-switch stores first); Save to an occupied slot asks; cancelled overwrite leaves the slot byte-identical; no code path captures into an occupied slot without the dialog |
| Controls | Inventory: every parameter has exactly one primary control in a workspace, attached to the right ID; macros write the documented parameters; disabled states (Echo Time while Sync on, Division while Sync off) |
| Readability | Every visible text ≥ 12.5 design px (≥ 10 px at 80 %); no text outside its component; no overlapping interactive components; at 80/100/150 % for all seven workspaces, both context tools, the browser, the review panel and the confirmation dialog |
| Build My Mix | Open/compute/close/Cancel/workspace switch change nothing; one checkbox per change; only ticked changes are applied; unticked settings unchanged; nothing else moves; user edit marks the row stale and is kept; preset load/A-B/Undo close the panel; partial apply = "Built: …" + modified; full apply = not modified; Undo restores exactly; protected parameters untouched; values in range |
| Performance | Workspace switch repaint cost; spectrum repaint; no allocations on the audio thread (existing tests) |
| Determinism | The `Numeric determinism` group still passes |
| Host (manual) | FL Studio macOS at 80/100/150 %: fit, readability, resize behaviour, size memory, automation, Dry/Processed latency alignment, Build My Mix cancel null test; Windows separately |

## 9. Phases (each ends at an approval gate)

| Phase | Scope | Acceptance |
|---|---|---|
| 0 | Mac validation of the current working tree; decide the base | Phase 0 checklist passes; your decision |
| 1 | Readability foundation: type tokens, minimum font, 80–150 % limits with a display-aware maximum, `uiScale`, readability/overlap tests | No text below 10 px at 80 %; no overlaps; full suite passes |
| 2 | Shell: top bar, workspace bar, status line, empty workspace containers; old pages hosted inside temporarily; context buttons | Every existing control still reachable; navigation tests pass; old sessions open sensibly |
| 3 | Workspaces: module cards for the six standard groups and the Experimental Lab (with its 17-stage strip); "Signal order" | Control-inventory test passes (no Lab control outside the Lab); parameters 69 |
| 4 | Studio column: spectrum, meters, macros, Build My Mix card, context status | Real data only; perf tests pass |
| 5 | Build My Mix review-first | All section-8 Build My Mix tests pass |
| 6 | Preset browser (existing presets) + listening pass | Browser tests; listening sheet for all 8 |
| 7 | New presets that pass (optional) | Each passes the protocol |
| 8 | FL Studio macOS validation; Windows build and validation (separate) | Checklists recorded |

## 10. Risks and rollback

* **Risk:** `PluginEditor.cpp` is large; a big-bang rewrite would be hard to review. **Mitigation:** phases 2–5 each keep the plug-in working; land them as separate commits only after your approval.
* **Risk:** test churn hides weakened assertions. **Mitigation:** the control-inventory and parameter-ID tests are written first and never relaxed.
* **Risk:** host behaviour (size memory, scaling) differs from assumptions. **Mitigation:** Phase 1 measures it in FL Studio before the layout work.
* **Risk:** bringing back Lab effects changes product scope (latency, CPU). **Mitigation:** separate Experimental Lab workspace; off by default; latency badges; Build My Mix proposes switching them off (your decision), but the user can untick that row.
* **Rollback:** every phase is a separate set of commits on the branch (only when you say "commit and push"); reverting a phase's commits restores the previous UI. No state-format change is irreversible: new UI keys are appended and ignored by older builds; parameters and presets are unchanged.
