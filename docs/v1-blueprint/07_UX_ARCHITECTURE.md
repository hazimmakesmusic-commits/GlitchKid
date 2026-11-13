# Deliverable 7 — UX architecture

## Priority order

One window, built around the journey in Deliverable 2. The brief's priorities, in this order, decide every layout question:

1. Vocal loading
2. Genre selection
3. Mix generation
4. Four macros
5. Five stages
6. Beat Lock status
7. A/B and bypass
8. Saving and recall

## 1. Structure

```
┌────────────────────────────────────────────────────────────────────────────┐
│ TOP BAR  GlitchKid · [R&B ▾ "My Hook" ● edited] · A | B · ↶ ↷ ·            │
│          [ORIGINAL | PROCESSED · matched] · Save · ☰                        │
├───────────────┬────────────────────────────────────────────────────────────┤
│ FLOW COLUMN   │ TABS:  MIX | TUNE | CLEAN | SHAPE | SPACE | FINISH          │
│ 1 Vocal ▮▮▮   ├────────────────────────────────────────────────────────────┤
│   Listening…  │                                                            │
│ 2 Genre       │   MIX tab (default):  TONE   PUNCH   SPACE   PRESENCE       │
│   [7 cards]   │   four large knobs, value in words                         │
│ 3 GENERATE    │                                                            │
│   ↺ Previous  │   Beat Lock card:  ● Locked — 3 adjustments [ON]  Details  │
├───────────────┴────────────────────────────────────────────────────────────┤
│ STATUS LINE  plain-language status · input/output meter · latency (ms)     │
└────────────────────────────────────────────────────────────────────────────┘
```

- **Top bar** (always visible): genre and sound name with ● edited state; A/B; undo/redo; the compare switch with a “matched” label; Save; menu (Settings, Help, About).
- **Flow column** (always visible): steps 1–3 of the journey.
  - It collapses to a slim strip once a mix exists.
  - Clicking the strip expands it again.
- **Tabs:**
  - **MIX** is the home: the four macros and Beat Lock.
  - The five stage tabs hold the detail controls, simple first, with one **Advanced** disclosure per stage (independent per stage; this fixes today's shared switch).
- **Status line:**
  - one plain sentence;
  - an input/output meter (no other technical meters);
  - the reported latency in ms (useful when Tune is on).
- **Removed from today's UI:**
  - the 11-item rail;
  - START's seven dials;
  - CREATIVE / BEAT / REFERENCE / MASTER / KID screens;
  - MANDALA and the psychedelic layer (Deliverable 3).

## 2. Screens and states

| Region | State | Shows |
|---|---|---|
| Flow · Vocal | No signal | “Waiting for your vocal — press play in your DAW” |
| | Signal | Animated meter, “Vocal coming in” |
| | Clipping | Red: “Your recording is clipping before GlitchKid…” (stays until dismissed) |
| | Listening | Ring progress 0–100 %; “Listening… (6 s)” |
| Flow · Genre | None chosen | 7 cards; Generate disabled: “Choose a genre first” |
| | Chosen | Card highlighted with its one-line description |
| Flow · Generate | Ready | Primary button |
| | Listening | Button becomes “Listening…”, with Cancel |
| | Too little audio | Inline: “Couldn't hear enough. Play longer, or Generate as designed.” |
| | Done | “Mix generated for R&B — fitted to your vocal (8 s)” + a **What changed** link |
| | Edits exist, Generate again | Confirm: “Generate again? Your changes will be replaced. (Previous mix keeps them.)” |
| MIX · macros | Centred | A 0 mark, “as generated” |
| | Moved | Ring offset + words (“+2.1 dB presence, a little more de-essing”) |
| MIX · Beat Lock | All six states of Deliverable 6 §6 | ON/OFF switch only when Locked; adjustment list with plain reasons; Re-adapt |
| TUNE | Off · On · Suggestion | Suggestion card: “Suggested: A minor (confident) — **Use** / **Keep mine**” |
| Compare | Processed · Original (matched) · A · B · Beat Lock OFF · Previous mix | One clear “Now hearing: …” label in the status line |
| Save | Dialog | Name; genre tag (pre-filled); overwrite confirmation (existing rules [V]) |
| Session notice | Older project with deferred features | One-time notice (Deliverable 3 §4) |
| Errors | Host gives no tempo; Instrumental input silent; analysis failed | Plain, actionable copy; never blocks vocal-only work |

## 3. Interactions

| Action | Behaviour |
|---|---|
| Macro drag / scroll / arrows | Changes the macro's offset (an automatable parameter). Shift = fine. Double-click = centre. |
| Detail control | Shows the **effective** value; editing writes the baseline. It claims the parameter from Beat Lock (Deliverable 4 §2). |
| Generate | One undo step. Macros reset to centre. Previous mix stored. |
| Genre card | Selects only; nothing changes until Generate. |
| A / B | Empty slot stores; occupied slot recalls; asks before leaving a changed slot or overwriting a different sound (rules from the earlier approved design). |
| ORIGINAL / PROCESSED (key D) | Instant; loudness-matched unless true bypass is chosen in Settings. |
| Previous mix | Swaps the current and previous states (both kept). |
| Undo / redo (Ctrl/Cmd+Z, Shift+Z) | Every gesture and engine action. Shortcuts may be captured by hosts [T]; the buttons always work. |
| Beat Lock ON/OFF | Compares adapted vs unadapted, loudness-matched. |
| Preset menu | Grouped by genre: the 7 genre starting points (they Generate when chosen, after confirmation), then the user's presets under their genre tags. |

## 4. Visual direction

**Premium, dark, tactile, credible.** It should feel like a well-made hardware unit, not a game or a dashboard.

| Element | Direction |
|---|---|
| Surfaces | Near-black background; warm dark-grey panels with a single subtle bevel; no noise or psychedelic layers |
| Colour | One brand accent (for the primary action, active states and the macro rings). Semantic colours only for meaning: green = signal OK, amber = attention, red = clipping. Each accent has one job. |
| Controls | Large machined-style knobs for the 4 macros; compact knobs and sliders in stages; clear switch states (with text, never colour only) |
| Type | Inter (bundled [V]): 12 px minimum at 100 %, tabular numbers for values |
| Motion | Only feedback: meters, listening progress, value changes. No idle animation. Respects the MOTION preference [V]. |
| Meters | Input and output peak meters only, plus the analyser progress. No spectrum or Mandala in V1. |
| Brand art | The Glitch Relic artwork goes on the About screen, if the owner agrees (Deliverable 3) |
| Accessibility | Contrast ≥ 4.5:1 for text; focus ring; every control has an accessible name and description; full keyboard reach; screen-reader pass with VoiceOver and Narrator before launch |

## 5. Window and scaling

- Keep the 1120 × 680 design size with uniform scaling (existing mechanism [V]).
- **Proposed:** raise the minimum scale from 70 % to 80 %, so the 12 px minimum stays ≥ 9.6 px.
- Maximum 150 %.
- Test at 100 %, 125 % and 200 % OS display scaling on Mac and Windows [T].

## 6. Copy rules

- Name things by what they do for the voice: “more presence”, not “+3 dB @ 3.4 kHz”. Numbers appear in readouts and Advanced.
- Say “we measured”, never “we heard” or “AI thinks”. No quality or professional claims in the UI (Deliverable 2 §1).
- Every warning says what to do next.

## 7. Reuse from the current code

| Reuse | From |
|---|---|
| Parameter attachments, knob and switch components, slider styles | `LabKit`, `PlaygroundWidgets`, `NeonKit` (restyled) [V] |
| A/B logic, preset save rules, confirmations | Processor and `PresetController` [V] |
| Beat Lock suggestion list with reasons | Today's BEAT page, reworked for automatic mode |
| Analysis progress and status copy | Today's SOUNDS / Build My Mix page |
| Resize constrainer, UI flags | Editor [V] |

The two earlier HTML prototypes (`design-prototypes/`) are **superseded concepts**. Only their compare and confirmation rules carry over, and those were already approved.
