# Dual-workspace redesign: Home + Advanced Mixing

Status: implemented on branch `claude/glitchkid-plugin-architecture-3o6w74`, uncommitted, on top of `4818599` (2026-11-14).
The source code is the authority; this document records the audit, the decisions and what is deliberately not done.

## 1. Audit (Phase 0)

### What exists

| Area | Where | What it really does |
|---|---|---|
| Home | `Source/UI/MainScreen.*` | Full-window instrument (page 6, `kidView` off). Vocal visual from the display taps, 4 modules, 6 basic dials, ORIGINAL / PROCESSED. |
| Advanced Mixing | `Source/PluginEditor.cpp` `layoutContent` | 10 rail rows over 8 page numbers. FINAL TOUCH (page 4) holds 4 categories: 0 PITCH, 1 TONE, 2 DYNAMICS, 3 SPACE. |
| Navigation state | `uiLevel` (page), `uiCategory`, `uiAdvancedRow`, `uiMainView` in the session | Page numbers are saved in sessions and must keep their meaning. |
| Rail rows (index = test and session meaning) | `railItems[11]` | 0 HOME, 1 SOUNDS, 2 CLEAN, 3 TUNE, 4 TONE, 5 DYNAMICS, 6 SPACE, 7 CREATIVE, 8 BEAT, 9 REFERENCE, 10 MASTER |
| Genres | `FactoryPresets` (8 profiles) | Browsing (`selectProfile`) changes nothing; loading replaces the sound; Build My Mix fits the browsed profile to the analysed vocal. |
| Build My Mix | `autoAnalyzeButton`, `refreshAutoMode`, `runBuild` | Real analysis (2–8 s of vocal), then `MixEngine`; one-step undo (`backToMine`). |
| Undo | `undoButton` → `backToMine` | One step: the sound before the last build or profile load. **No redo exists.** |
| A/B | `abSetA/B`, `abRecallA/B` | Two snapshot slots in the session. |
| Presets | `PresetController` + preset bar | Factory profiles + user presets; save asks before replacing. |
| Beat Lock | page 2 (BEAT) | Needs the instrumental on the sidechain; manual analyse → suggestions → apply / undo. |
| Visualisations | `EqGraph` (EQ response, draggable points, real spectrum, CLEAN's applied cuts), `GrMeter` (compressor), `limGrBar` (limiter), `LevelMeter` (in/out), `tuneReadout` + `scope` (pitch) | All from DSP read-outs. No stereo-width meter exists. |
| Mix (wet/dry) | `mix` parameter | Only on Home; Advanced had no control for it. |
| Design system | two palettes: `ui::` (Advanced, `PlaygroundWidgets.h`) and `nk::` (Home, `NeonKit.h`), two dial drawings | The two workspaces looked like two products. |

### Problems found

1. Two palettes and two dial styles: Home and Advanced did not share visual DNA.
2. Advanced was a dashboard: an 11-row rail mixing stages with tools, a bordered panel around every page, outlined value boxes under every knob, an "Advanced" switch that hid half of TONE and TUNE.
3. The compressor sat with the limiter (DYNAMICS) instead of with the tone tools; output gain and the limiter had no MIX beside them.
4. Home had no genre choice, no Build My Mix, no A/B and an "Ask Kid" entry to a deferred feature.

## 2. Decisions

### Shared design system (Phase 1)

| Token / component | Decision |
|---|---|
| Colours | One palette, defined once in `PlaygroundWidgets.h` (`ui::`). `nk::` names are aliases of it. Ultraviolet `#9B5CFF`, magenta `#FF4FB8`, near-black surfaces, cyan only for the input signal. |
| Dial | One drawing (`ui::drawDial`): neon value arc (ultraviolet → the knob's accent), machined dark cap, white pointer. Used by Home and every Advanced knob. |
| Values | Plain numbers under knobs: no outlined boxes. |
| Panels | `drawPanel` is a quiet surface with a hairline edge, used only to group, never around a whole page. |
| Typography | Inter (`fontOf`): 20 display, 14 section, 12.5 body, 11.5 caption; capitals only for section names and switches. |
| States | Lit (violet edge or neon arc) = really processing; dim = off; disabled = 40 % alpha; focus = soft violet ring. |

### Home (Phase 2)

- Removed: **Ask Kid** (conversational AI is deferred) and the *Choose a sound* shortcut (replaced by the genre selector). Kid's code is untouched; a session saved with Kid open still opens it, with its own way back.
- Added a **journey column** left of the vocal, in the order of the work:
  1. **SOUND**: a selector of the 8 factory profiles (the same `browsedProfile` as SOUNDS; choosing one changes nothing until you build or load).
  2. **BUILD MY MIX**: the same button logic as SOUNDS (listen, build, build again; *build now* while listening) and the same status text.
  3. **BEAT**: the real instrumental-input status; *Fit to beat* opens BEAT in Advanced.
  4. **NEXT**: one line naming the next step, from the real state (no audio → play; no sound chosen → choose; not built → build; built → refine and compare; then add a beat; then save).
- A/B (SET A, SET B, A, B) on Home's bar: the same buttons as in Advanced.

### Advanced Mixing (Phases 3–4)

The brief's five sections match the DSP, with two moves:

| Section | Page / category | Contents (all existing parameters) |
|---|---|---|
| TUNE | page 4 / cat 0 | Key pad, scale, on/off, styles, strength and speed (always shown), the real pitch read-out and scope. Formant stays in CREATIVE (deferred, Lab). |
| CLEAN | page 1 | Unchanged controls: CLEAN switch and amount, EQ graph with CLEAN's real cuts, telemetry, trims. |
| SHAPE | page 4 / cat 1 | TONE macro, EQ graph (draggable), the selected band's knobs, **the compressor (moved from DYNAMICS)** with its gain-reduction meter, PUNCH macro, warmth, AIR LAB. |
| SPACE | page 4 / cat 3 | Echo, width (Dimension), Doubler. **No reverb exists**; none is shown. |
| FINISH | page 4 / cat 2 | Output gain, **MIX** (a new knob on the existing `mix` parameter), input and output meters, the output limiter with its real gain reduction. |

- **Navigation.** The rail shows the five sections first, then a smaller *MORE* group: Sounds, Beat, Creative, Reference, Master. HOME moves to the top bar as a back button. Rail indices, page numbers and `uiCategory` keep their meaning: an old session saved on DYNAMICS (cat 2) opens FINISH, where the limiter and output still are.
- **The "Advanced" switch** for TONE and TUNE is gone: Advanced Mixing *is* the detailed view.
- **Not invented:** redo, reverb, a stereo-width meter, input gain, live key detection. Each is listed as a future item, not drawn.

## 3. Sync and state

Every control on both workspaces is a JUCE attachment (or the existing macro read-back) on the same APVTS parameter. Nothing holds a copy of a parameter value. Navigation never changes the sound (tested). Home ↔ Advanced keeps `uiAdvancedRow`, so ADVANCED MIXING reopens the last section.

## 4. Validation

`Tests/MainScreenTests.cpp` ("one engine, two workspaces", "HOME's journey", the HOME tests) and the updated rail, SHAPE, FINISH and CLEAN tests in `LevelUiTests.cpp`, `MacroTests.cpp` and `ReferenceMatchTests.cpp`: sync both ways, every new control, navigation, session round trip, window sizes, paint cost.
