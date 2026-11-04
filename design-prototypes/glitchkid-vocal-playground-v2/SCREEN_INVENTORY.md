# Screen and state inventory — GlitchKid Vocal Playground v2

For each screen:
- **Purpose · Primary action · Secondary actions · Status indicators**
- **States** (empty / active / disabled / error)
- **Leaving it** (what happens when you navigate away)

Unless noted, "leaving" never changes the sound: navigation is display-only, as in today's plug-in.

The frame is the same on every screen:
- **Top bar:** the loaded sound, ‹ ›, A │ B, Save, Undo, DRY │ PROCESSED, and Help.
- **Rail:** BOOTH, SHAPE, CHARACTER, SPACE, SCENES and KID.
- **Meters:** IN, OUT and CLIP.
- **Chain strip:** stages 1 → 17.
- **Help line.**

---

### Frame elements (always visible)
| Element | Behaviour and states |
|---|---|
| **Loaded sound** (top bar) | Name, plus type: *Default*, *Factory*, *Yours* or *Built*. Then save state: *Saved*, *● Unsaved* or *● Not saved*. Clicking it opens SCENES. ‹ › steps through the factory profiles, and Undo goes back. |
| **A │ B** | Each slot shows *empty*, *stored*, *playing* or *changed* as text; the active slot is also outlined. **⤓** appears on occupied slots. The rules are in `USER_JOURNEYS.md` journey 4. |
| **Save** | The menu or dialog depends on what is loaded; see "Save flows". Ctrl/Cmd+S. |
| **Undo** | Shows *Undo: <what>* in its description. Marked `aria-disabled` with *Nothing to undo yet* when the stack is empty. Ctrl/Cmd+Z. |
| **DRY │ PROCESSED** | Level-matched compare: the existing `bypass` parameter plus `levelMatch`. DRY is drawn cyan; PROCESSED violet. Press D to toggle. |
| **Meters** | Peak dBFS for IN (cyan) and OUT (ultraviolet → pink). **CLIP** shows as text for 1.5 s when the input reaches full scale. Simulated in the prototype. |
| **Chain strip** | 17 chips in processing order. *On* = a dot, a fill and bold text (not colour alone). The current workspace's chips are outlined. A chip opens its module. It reflects what you *hear*, so it changes during audition or preview. |
| **Help line** | What the pointed or focused control does, plus its range and default. Long texts are truncated here; the full text is also the control's accessible description. |
| **KID (rail)** | *Ask* / *Open* / *Waiting*, with a badge "1" when a proposal is waiting. Hold **K** anywhere to talk. |

### 1. First launch (5 steps)
- **Purpose:** what GlitchKid is, confirm the signal, show the compare, choose a path, and explain the workspaces and KID's privacy. Ends inside the plug-in.
- **Primary:** Next / Finish. **Secondary:** Back, **Skip intro** (every step), and *Prototype: simulate playback* (prototype only).
- **Status:** step dots (5); the live IN bar on step 2.
- **States:** signal waiting / ✓ coming in; DRY / PROCESSED pressed; path selected.
- **Leaving:** Skip or Finish marks it done (remembered; *Help › Show the intro again*). Finish with *build* opens Build My Mix; with *pro* opens SHAPE.

### 2. Booth (home)
- **Purpose:** at a glance:
  - Is a vocal coming in? (signal card and meters)
  - Is processing active? (the hear-chip, the chain strip, DRY)
  - What is loaded? (name, type, save state, A/B)
  - What next? (the Next step card)
- **Primary:** the **Next step** button, one of:
  - Browse Scenes;
  - Hear DRY;
  - Save;
  - Open SHAPE;
  - How to route it.
  
  **Secondary:** the three big moves (Tone / Punch / Space); HOLD TO TALK; type to KID; examples; **Help me build my vocal**.
- **Status:** the ILLUSTRATIVE visual; HEARING · PROCESSED / DRY (ORIGINAL) / PROPOSAL, NOT APPLIED / PREVIEW · name; the signal card; *A proposal is waiting*.
- **States:**
  - **Empty:** waiting for a vocal — a seed visual and routing help.
  - **Active.**
  - **Disabled:** KID's mic, when speech is unavailable.
  - **Error:** clipping; Mastering mode; not processing.
- **With the drawer open:** the KID column hides; the visual shrinks to 250 px and stays visible beside the drawer.

### 3. SHAPE › CLEAN (stages 7 and 9)
- **Purpose:** remove rumble, build-up and harshness, and tame S sounds.
- **Primary:** On/Off and the Amount knob. **Secondary:** *Advanced: section trims* (Corrective EQ, Resonance, De-esser), Reset.
- **Status:** `Stage 7 & 9 of 17`, neighbours, and *What CLEAN is doing* (SIMULATED: learned seconds, rumble, low-mid, resonance, de-ess).
- **States:** off (dimmed body; telemetry explains); on without signal (*nothing learned without sound*); on with signal.

### 4. SHAPE › TUNE (stage 1)
- **Purpose:** pull pitch to the beat's key and scale.
- **Primary:** On/Off, Key (12 note chips), Scale (6), Style (Natural / Light / Hard). **Secondary:** *Advanced: strength and speed*, Reset.
- **Status:** latency note (about 31 ms, compensated by the host); SIMULATED read-out; "GlitchKid does not detect your key".
- **States:** off / on; the style chip shows ✓ only when strength and speed match it (within 3 % and 5 ms, the existing rule).

### 5. SHAPE › TONE (stages 6, 10, 11)
- **Purpose:** tonal balance.
- **Primary:** EQ band gains (Low, Body, Presence, Air shelf), Warmth drive, AIR LAB On/Off with Presence lift and Air lift.
- **Secondary:** band frequencies; *Advanced: bell widths (Q)*; AIR LAB *mix and trim*; Link; Reset per module.
- **Status:** the response curve (labelled *approximate*); module On/Off badges.
- **States:** a flat curve at defaults; AIR LAB knobs dim when it is off, but stay usable (today's plug-in behaviour).

### 6. SHAPE › DYNAMICS (stage 8)
- **Purpose:** even out the words.
- **Primary:** Threshold and Ratio. **Secondary:** *Advanced* (Attack, Release, Knee, Makeup), Reset.
- **Status:** SIMULATED gain reduction; "Ratio 1:1 = no compression".

### 7. CHARACTER — Experimental Lab (stages 2, 3, 4, 12)
- **Purpose:** a distinctive vocal identity.
- **Primary:** per card, the on/off state and one main control:
  - **Formant:** Shift; it has no switch, and is off at 0.
  - **Harmony:** Interval and Level.
  - **Mutation:** Amount.
  - **Stutter:** Slice and Mix.
- **Secondary:** **More** (Mutation's Character and Blend); **Reset** (only that card, with Undo).
- **Status:** the card lights when the effect is really on; stage badge; latency notes.
- **Leaving:** nothing changes.

### 8. SPACE (stages 5, 13, 14, 15–17)
- **Purpose:** put the vocal in its world.
- **Primary:**
  - **Doubler:** Amount and Width. Its "runs early" note is prominent.
  - **Echo:** Mix, Sync/Free and Division.
  - **Dimension:** Mix and Width.
  - **Output:** Gain, Mix, Limiter On and Ceiling.
- **Secondary:** **More** on each card. Echo: Feedback, Tone, Width and Time (used when Free). Dimension: Depth, Detune. Doubler: Detune, Timing. Limiter: Drive, Release.
- **Status:** KID never changes the Output card; it says so.

### 9. SCENES
- **Purpose:** choose or recall a starting sound.
- **Primary:** **Load** (Undo goes back). **Secondary:** **Preview**; filter (All / Factory / Yours / Concepts); **Delete…** on your own sounds.
- **States:**
  - **Factory:** 8 profiles plus Factory Default, thumbnails drawn from their real values.
  - **Yours:** empty state ("Nothing saved yet…"), or cards.
  - **Concepts:** 8 placeholders with dashed cards, *No sound defined*, and disabled *Not a preset yet*.
  - **Loaded:** the loaded card is outlined and marked LOADED.
- **Preview bar:** *You are hearing X. Nothing is loaded; your sound is safe.* Actions: **Back to my sound** or **Load it**. Leaving SCENES ends the preview without loading.

### 10. KID — voice command states (drawer)
| State | Trigger | UI | Exit |
|---|---|---|---|
| Ready | Opening KID | *Tell me what you want to hear…* | Hold the mic or type |
| Recording | Holding the mic (pointer, Space/Enter on the mic, or K) | Pink pulsing mic, command-mic level bars, *Listening… let go to send* | Release. A tap under 300 ms → *Hold the button while you speak* |
| Transcribing | Release | *Transcribing on this computer…* | Automatic |
| Heard you | Transcript ready | *You said (transcribed on this computer)* bubble | Automatic |
| Interpreting | — | *Reading your words…* | Automatic |
| Preparing | — | *Building a proposal…* | Automatic |
| Proposal ready | — | KID's interpretation, plus the proposal | Apply / Cancel / × / follow-up |
| Auditioning | Listen › Proposal | Pill *Auditioning the proposal*; the Booth chip *PROPOSAL, NOT APPLIED* | Listen › Current, Apply, Cancel; × resets to Current |
| Applied | Apply | *Applied n changes* with **Undo** and **Done** | Done / a new command |
| Didn't catch that | No sound words | Examples; nothing changed | A new command |
| Unavailable | Speech not set up, mic blocked, Mastering mode, or not processing | The reason and what to do (set up speech, privacy settings, MASTER page) | Fix the cause; typing works except in Mastering / not-processing |

Offline is not an error: everything runs locally; only the one-time model download needs internet.

### 11. KID — proposal review and application
- **Primary:** **Apply n changes** (disabled at 0 ticked, or while busy). **Secondary:**
  - **Cancel** (discard);
  - **×** (keep for later);
  - **Listen:** Current / Proposal;
  - **All / None**;
  - per-row ticks;
  - follow-up: mic, text and 5 chips.
- **Status:**
  - **Header:** REVISION n, *n changes · n ticked · grouped by stage, in processing order*.
  - **Groups:** `stage · MODULE` plus its workspace.
  - **Row tags:** *changed / new in revision n*, *YOU MOVED THIS · YOUR VALUE IS KEPT*, *AT ITS LIMIT*, *SIMULATED MEASUREMENT*.
- **Rules:**
  - Opening or closing changes nothing.
  - Only ticked rows apply.
  - A hand-moved control is unticked, disabled, and its value kept.
  - KID never proposes Gain, Mix, the limiter or Bypass.
- **Leaving:** × keeps the proposal (rail badge, Booth notice); Cancel discards; audition always returns to Current.

### 12. Build My Mix (drawer, 3 steps)
- **1 Listen:**
  - **Start listening** shows progress and a checklist (SIMULATED MEASUREMENTS).
  - Without signal: a banner and **Skip: use as designed**.
  - Clipping: a banner.
- **2 Direction:** 8 profile cards (radio) plus Intensity (100 % = as designed; 0 % halves echo, Dimension, Doubler and saturation).
- **3 Review:** the shared review component. It names the profile and whether it was fitted to the vocal (*fitted* / *as designed (not fitted)*).
- **Exit:** Cancel or × at any step changes nothing. Apply → *Built: PROFILE* with ● Not saved, and Undo.

### 13. Save flows
| Situation | Flow |
|---|---|
| Factory, default or built sound | **Save** → *Save this sound* dialog with a name field and live checks (empty; more than 40 characters; `\ / : * ? " < > \|`; factory names) → Save |
| Your sound, unchanged | Toast: *“X” is saved. Nothing has changed since.* |
| Your sound, changed | Menu: **Update “X”…** (→ *Update “X”?* confirmation: Cancel / Save as new… / **Replace**) or **Save as new…** |
| Name already exists | Saving shows a hint while you type, then *Replace “X”?* (Cancel goes back to the name, / **Replace**) |
| Delete (SCENES › Yours) | *Delete “X”?* (Cancel / **Delete**). It can't be undone, and says so. |

### 14. Undo and cancellation
- **Undo** covers:
  - loading a scene;
  - applying a proposal;
  - switching A/B;
  - module resets;
  - Reset everything;
  - each control gesture (a drag, or a burst of key presses, counts as one step).

  It holds 40 steps, labelled ("Undo: Change Echo mix").
- **Cancel** exists wherever something is proposed: KID, Build My Mix, Preview, and every dialog. Cancel never changes the sound.
- **Confirmations appear only where something can't be undone:**
  - overwriting a saved sound or an A/B slot;
  - deleting;
  - switching away from a changed A/B slot;
  - *Reset everything* (it can be undone, but it is large).

### 15. No input · clipping · unavailable · error
See `USER_JOURNEYS.md` journey 5. Each has text, an icon and a next action; none relies on colour alone.

### 16. Help and progressive disclosure
- **Help (?):**
  - shortcuts;
  - "No sound?" routing;
  - how KID works (local speech, rule engine, nothing sent);
  - the fixed order;
  - **Reset everything…**;
  - **Show the intro again**.
- **Tooltips:** every control has an accessible description, which the help line also shows on hover or focus. Knobs add their range and default.
- **Advanced / More:** each disclosure is a button with `aria-expanded` and a ▸ / ▾ arrow. The section stays open while you work, but it is not remembered between sessions; see the open questions.

### 17. Window sizes and scrolling
- **One 1120 × 680 design scaled as a whole,** the same model as today's editor, which allows 70–150 % at a fixed aspect ratio.
- **Shown at 80 % (896 × 544), 100 % and 125 % (1400 × 850).** The prototype page also has *Fit* for phones.
- **Scrolling** where content is long, rather than shrinking text:
  - SHAPE, CHARACTER and SPACE scroll when Advanced or More is open;
  - SCENES scrolls;
  - the KID and Build My Mix proposal list scrolls between a fixed header and a fixed action bar;
  - the follow-up chips scroll sideways, with an edge fade.
