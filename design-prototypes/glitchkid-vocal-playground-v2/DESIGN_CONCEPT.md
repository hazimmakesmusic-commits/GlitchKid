# GlitchKid — The Vocal Playground (v2)

## Your voice. Your universe.

Design concept for a complete commercial redesign of GlitchKid Vocal Lab. This is a prototype and design phase only: nothing in `Source/` was changed.

> **Folder name.** The brief asked for `design-prototypes/glitchkid-vocal-playground-v1/`. That folder already exists: it holds an earlier, smaller Vocal Playground concept, committed in `b5a685a`. The brief also says never to modify or overwrite existing prototypes, so this work lives in **`glitchkid-vocal-playground-v2/`**, and v1 is untouched.

---

## 1. The idea in one sentence

GlitchKid opens on **the Booth**: one place that shows what you're hearing, how it's built, and what to do next. From there you can:

- **speak** to a vocal director that proposes changes you review before anything moves;
- **step into** three workspaces that hold every real control in the order the sound actually flows.

## 2. Who it is for, and what it promises

| Artist | What they need | How the design answers |
|---|---|---|
| **Newer artist** | Confidence: where's my vocal, is it working, what do I do next? | A 5-step first launch that ends inside the plug-in. A **Next step** card that always names one useful action. **KID** for "I don't know what it needs". **Build My Mix**. **Scenes** with Preview. |
| **Experienced producer** | Speed, precision, predictability, no hidden magic | Every one of the 68 sound parameters is reachable with its real range and unit. A **chain strip** shows the fixed 1 → 17 processing order and what is on. Knobs support drag, scroll, keyboard and double-click reset. Proposals list exact current → proposed values. |

Promises the interface makes, and the honest limit of each:

- **Faster to a finished vocal.** Scenes, KID and Build My Mix produce a complete starting point in seconds. Whether it sounds professional depends on the recording. No "one-click pro vocals" claim.
- **Understandable without mixing knowledge.** Every control has a plain one-line description in the help line and its accessible description. Workspaces are named by intent: SHAPE, CHARACTER, SPACE.
- **Distinctive identities.** CHARACTER (the Experimental Lab) and the psychedelic visual identity.
- **Control over every decision.** Nothing is applied without an explicit Apply. Every proposed change can be unticked, auditioned, cancelled or undone.
- **Repeatable.** Save, Scenes › Yours, and A/B slots. Recall is exact, and the prototype tests it across a close-and-reopen.

## 3. Experience architecture

```
┌ TOP BAR ─ logo · loaded sound (name, type, ● Unsaved) ‹ › · A │ B · Save · Undo · DRY │ PROCESSED · ? ┐
│ RAIL           │  MAIN AREA                                                        │ METERS        │
│ BOOTH          │   Booth  /  SHAPE (Clean · Tune · Tone · Dynamics)                │ IN  OUT       │
│ SHAPE          │          /  CHARACTER (Experimental Lab)  /  SPACE  /  SCENES     │ (dBFS, CLIP)  │
│ CHARACTER      │   + the director drawer (KID or Build My Mix), over the right side │               │
│ SPACE          │                                                                   │               │
│ SCENES         │                                                                   │               │
│ [KID]          │                                                                   │               │
├ CHAIN 1→17 ─ TUNE FORMANT HARMONY MUTATE DOUBLE EQ CLEAN COMP DE-ESS WARMTH AIR STUTTER ECHO DIMENSION GAIN MIX LIMIT ┤
└ HELP LINE ─ what the pointed or focused control does ───────────────────────────────── PROTOTYPE tag ┘
```

**The frame is the same on every page.** That keeps the compare, A/B, Save and Undo in one place. It also means moving around never changes the sound, a rule carried over from the current plug-in, where it is tested.

- **Booth (home).** Left column: what's loaded, three **big moves** (Tone, Punch, Space; the real macros from `MacroEngine`) and the input status. Centre: the signature visual and **Next step**. Right: **KID**, with push-to-talk, typing, examples and **Help me build my vocal**.
- **SHAPE: make the vocal work.** CLEAN (stages 7 and 9), TUNE (1), TONE (EQ 6, Warmth 10, AIR LAB 11) and DYNAMICS (8).
- **CHARACTER: make it distinctive.** The Experimental Lab: Formant (2), Harmony (3), Mutation (4) and Stutter (12).
- **SPACE: put it in its world.** Doubler (5, and labelled as running early), Echo (13), Dimension (14), and Output: Gain 15, Mix 16, Limiter 17.
- **SCENES: sounds as places.**
  - Factory: the 8 real profiles plus Factory Default.
  - Yours: saved presets.
  - Concepts: eight named placeholders with no sound defined; they can't be loaded.
- **The director drawer.**
  - **KID:** conversation, a proposal with revisions, follow-ups, audition, and Apply / Cancel / Undo.
  - **Build My Mix:** Listen → Direction → Review.
  - **One review component for both.** KID and Build My Mix share it, so the safety rules are learned once.

Workspaces are a grouping. **They never imply the chain can be reordered:**

- Every module shows a `Stage n of 17` badge.
- Its neighbours are shown as `previous → this → next`.
- The chain strip is fixed.

## 4. Signature elements

1. **The Booth visual: "a picture of your sound".**
   - **What drives it.** A flower-like form whose shape is derived from the settings:
     - Brightness tints it from ultraviolet toward pink.
     - Punch sharpens the petals.
     - Pitch Tune adds petals.
     - Echo adds drifting rings.
     - Width adds mirrored reflections.
     - Character effects add a chromatic split.
     - Stutter dashes the outline.
     - DRY shows a single plain cyan line, the original.
   - **In the prototype** its size follows a *simulated* level, and it is labelled **ILLUSTRATIVE**.
   - **In production** it would follow the plug-in's real display taps, which already exist (`DisplayTap`, used by today's ribbon and Mandala view).
   - **When nothing plays,** it rests as a quiet dashed seed with "Waiting for your vocal".
2. **The chain strip.** Seventeen chips in true processing order. A lit chip has a dot, a fill and bold text, so its state is never shown by colour alone. Clicking a chip opens its module, and the current workspace's chips are outlined.
3. **The director drawer.** Every proposal reads the same way:
   - **Grouped by stage** in processing order.
   - **Each row shows** the current → proposed value and a one-line reason.
   - **Each row is ticked separately.**
   - **Tags** mark rows "changed in revision 2", "your value is kept" or "simulated measurement".
4. **Scenes as places.** Every factory card's thumbnail is drawn from that profile's real parameter values, so the picture *is* the sound's settings. Concept names are visibly placeholders: a dashed card, "No sound defined", and a disabled button.

## 5. Visual language

| Role | Token | Use |
|---|---|---|
| Base | `#07060b` near-black, panels `#121019` / `#181521` / `#211c2c` | Depth from layered charcoal, not from shadows alone |
| Primary accent | Ultraviolet `#8f5bff` → electric pink `#ff4fb8` | Value arcs, active states, the visual. White text on the primary button gradient (`#6f3df2 → #a63ad6 → #c42f8f`) stays at or above 4.5:1 |
| Workspace marks | SHAPE violet `#b47cff` · CHARACTER pink `#ff4fb8` · SPACE indigo `#7d8bff` | The rail, workspace headers and chain chips. Always paired with a name |
| Input only | Cyan `#5ee3ff` | IN meter, the DRY visual. Used sparingly, so cyan *means* "your original" |
| Status | Good `#5fe3a5`, warning `#ffbf5f`, error `#ff6378` | Always with text (`● ON`, `● Unsaved`, `CLIP`) |
| Text | `#f4f0fb`, `#c7bed6`, `#9d93b0` | All three are at least 6:1 on the panels (checked) |

- **Type.** Inter / SF Pro.
  - Headings use the display cut: 26–22 px for workspace titles, 22 px for the loaded sound.
  - Micro-labels are 12 px uppercase with tracking.
  - **Nothing in the window is smaller than 12 px at 100 %.** That is 9.6 px at 80 %; see the design review.
- **Controls.**
  - **Knobs:** a 270° arc from ultraviolet to pink, with a white pointer. Bipolar knobs (EQ gains, formant, trim) fill from the centre.
  - **Switches:** always carry the word ON or OFF.
  - **Choices:** checked chips carry a ✓.
- **Motion.** Purposeful only:
  - **The visual** follows the level, with a slow rotation and echo-ring drift.
  - **The mic** pulses while recording.
  - **Respects settings:** the MOTION switch and `prefers-reduced-motion` stop decorative movement, while the level still moves (it is information).
- **Avoided on purpose.** A chat-window look, gamer neon, decorative fake analysis, and walls of knobs. Advanced controls sit behind **Advanced / More**.

## 6. Honesty rules built into the design

| Label | Meaning | Where |
|---|---|---|
| **ILLUSTRATIVE** | A picture, not a measurement | The Booth visual |
| **SIMULATED** / **SIMULATED MEASUREMENT** | A value the plug-in would measure from audio; made up in the prototype | CLEAN telemetry, Tune read-out, gain reduction, the Build My Mix "fitted" rows, "Help me improve it" |
| **CONCEPT** | The KID drawer: speech and interpretation are simulated with rules modelled on `KidDirector` | Drawer header |
| **NEW WORDS** | KID's reply flags words that today's `KidDirector` does not know | KID replies |
| **PLACEHOLDERS** | Scene names with no designed sound | Scenes › Concepts |

Outside the plug-in window, the prototype page carries the controls that simulate host and system conditions:

- vocal input, speech, network, the engine, window size and motion;
- what KID "hears" when you hold the mic;
- close and reopen the project, and reset the prototype data.

These are deliberately **not** part of the plug-in UI.

## 7. Where v2 departs from the earlier prototypes

| Earlier work | What v2 keeps | What v2 changes |
|---|---|---|
| `vocal-lab-redesign-v1` (6 review rounds) | The verified parameter map, the approved A/B rules, and review-before-apply with per-row ticks | One frame for everything, workspaces by intent, and the chain strip |
| `glitchkid-vocal-playground-v1` (four places, voiceprint, "hold to preview") | The idea of a signature visual and Scenes as places | **Audition** is an explicit Current / Proposal toggle, not hold-to-preview. Its engineering is spelled out in `AI_ARCHITECTURE.md`. The visual is clearly illustrative |
| `glitchkid-kid-vocal-director-v1` | KID as a director with reasons, and local-first privacy | Push-to-talk states, revisions that reshape the same proposal, and a hand-moved control that is kept, all as *working* interactions |

## 8. Risks taken on purpose

- **The Booth is not a wall of knobs.** Producers must click once (a rail item or chain chip) to reach a module. The chain strip makes that one click direct.
- **Audition is the centrepiece of trust,** and in production it **needs new engineering:** a processor-side preview that doesn't touch the host's parameters.
- **One frame at 1120 × 680, scaled as a whole,** matching today's editor. Text scales with the window, so the plan recommends raising the minimum from 70 % to 80 %.
- **"AI vocal director".** The speech part is machine learning (whisper.cpp). KID's decisions are transparent rules. The UI says "your vocal director", and how "AI" is used in marketing is left as an open question for you (see `DESIGN_REVIEW.md`).
