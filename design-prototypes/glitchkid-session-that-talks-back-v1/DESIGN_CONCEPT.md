# GlitchKid Vocal Lab — The Session That Talks Back

A concept for the whole plug-in as an artist's late-night studio: a place you play, not a panel you operate. Your take is in the middle. Ideas wait on the desk. KID, a guided assistant, leaves the odd note. The notebook remembers what you liked. The loop is:

> **Play your vocal → discover a possibility → audition it → make it yours.**

This folder is a **standalone design prototype**. No audio runs. Nothing in it changes the plug-in, its DSP, parameters, presets or saved-state format.

## 0. Opening it

- **Open:** `index.html` in a browser, served or straight from disk. Both were checked in Chromium; Safari and Firefox were not tested. The bar above the window simulates FL Studio and the computer (input, transport, speech, window size). It is not part of the plug-in.
- **Check:** from this folder, run `node qa/verify.js` (or `--shots` to save screenshots). It needs only Playwright. Results are in `QA_REPORT.md`.

| File | What it is |
|---|---|
| `CONTROL_MAPPING.md` | Every visible control and idea, mapped to the real parameter it writes, or marked as simulated or new |
| `SCREEN_STATE_INVENTORY.md` | Every screen and state, how to reach it, and its screenshot |
| `USER_JOURNEYS.md` | The 11 journeys from the brief |
| `OPEN_DECISIONS.md` | Product and technical decisions still open |

## 1. What stays true in this concept

| Principle | How it shows up |
|---|---|
| **Artist-led, AI-assisted** | KID never applies anything. Its ideas wait on the desk like everything else, and you hear them, look inside and keep or put away. |
| **Real behaviour only** | Every control, idea, world and suggestion writes existing parameters inside their real ranges (68 sound parameters, Bypass and the Mastering Lab's). Measurements, the take and speech are simulated and say so on screen. No reverb, noise gate or timing fix is implied, because GlitchKid has none. |
| **Fast** | No account, no mandatory chat, no tutorial game. The intro is 3 short, skippable steps and ends inside the session. Every idea is two clicks from your ears: Hear, then Keep. |
| **Playful, never confusing** | The playfulness lives in materials (tape, stickers, cassettes, a notebook, handwriting). Controls stay plain, labelled and consistent. *The studio can be messy; the controls cannot be.* |
| **Reversible** | Hear without changing anything. Look inside and untick. Keep or Cancel. Put away (with “Bring back”). Undo **and Redo**. A/B. The notebook. Before anything replaces a sound that isn't kept anywhere, GlitchKid asks. |
| **Expressive, restrained** | Motion comes only from the take and from your settings. There is no idle animation, no glow for its own sake, no game mechanics: no XP, coins, unlocks, levels, leaderboards or achievements. |

## 2. Information architecture

One window, one rail. Nothing reorders the processing chain; the chain strip at the bottom always shows the fixed order 1→17.

| Area | What lives there | Compared with today's plug-in |
|---|---|---|
| **SESSION** (home) | The take (live input or a loaded file, the waveform, what you are hearing, meters), ideas on the desk, KID's note and bar, the session notebook | New home: a session, not a control page |
| **SHAPE** | Every main control, grouped by what it does: CLEAN · TUNE · TONE · DYNAMICS · SPACE · OUTPUT, each showing its stage numbers | Same controls, regrouped |
| **LAB** | The experimental modules: Formant, Harmony, Mutation, Stutter | Kept apart so the main path stays simple |
| **WORLDS** | Your saved sounds · the 8 factory profiles + Factory Default (factory tapes) · six draft worlds | Presets become tapes you can hear before loading |
| **TOOLS** | Beat Lock · Reference Match · Build My Mix | All three exist. Each now proposes and you review. |
| **MASTER** | The Mastering Lab, separated by a line and a steel colour | Unchanged in behaviour, kept apart from vocal shaping |
| **KID** (rail, bottom) | Jumps to the KID bar. Holding **K** talks to KID from anywhere. | — |

The **top bar** is constant: the session's tape label · the sound chip (where the sound came from, and ● Unsaved / ● Not kept yet) · A / B · Undo · Redo · Save… · DRY / PROCESSED · Help. The **help line** at the bottom says what is under the pointer.

### The SESSION screen

| Region | Contents |
|---|---|
| **The take** | “the take”, LIVE · from FL Studio (or FILE · look only), FL Studio playing · 92 BPM, Look at a file… The waveform is labelled SIMULATED TAKE. Overlaid on it: what you hear (HEARING · PROCESSED / DRY / “WIDE DREAM (NOT KEPT)”) and the IN/OUT meters with CLIP. Messages (No vocal yet · Paused · Not processing · Mastering mode is on) dim the picture behind them. |
| **Ideas on the desk** | A row of cards: 5 ideas (Ghost Double, Wide Dream, Broken Radio, Close & Warm, Afterimage), KID's ideas and happy accidents. Each card shows a sticker (IDEA / KID / ACCIDENT, plus REV n, HEARING, KEPT), a handwritten name, what it does, “Look inside · N changes”, and Hear · Keep · ✕. **Happy accident** rolls a new card. The row scrolls sideways (mouse wheel too), with a fade that says there is more. |
| **KID's note** | A lilac sticky note with KID's face: one note at a time, dismissible. It comes only from what GlitchKid knows: your settings, the input level, FL Studio's tempo, or a measurement labelled SIMULATED MEASUREMENT. |
| **Tell KID · guided assistant** | Hold-to-talk mic (a separate command mic) or type. The status line says honestly what is happening: Listening… · Transcribing on this computer… · Reading your words… · Leaving ideas on the desk… · Ideas are on the desk. Errors are said plainly too. |
| **Session notebook** | Ruled paper with spiral holes. Keep this sound → name + note. Each entry has Hear (compare) · Use · Rename · ✕, and the stickers ON NOW or ON NOW · CHANGED. |

## 3. KID: a guided creative assistant

KID is presented as what it is: GlitchKid's **rule engine** (`KidDirector`) turning your words into ideas made of GlitchKid's own controls. It is not generative AI. The bar's label, the intro and Help all say so.

- **You say a feeling, KID leaves up to three directions:** *straight from what you said*, *the same idea with the words kept up front*, and *pushed further, a little stranger*. Each is a card with a musical one-liner (“A doubled edge, fewer doubles, a wider room”) and its real changes inside.
- **It refines while its ideas are on the desk.** “A little less.”, “Keep the vocal upfront.”, “Make it darker.” revise the same cards (REV 2, REV 3…). Keeping one makes the next sentence start from your new sound.
- **It is honest about limits.** Timing is editing in FL Studio. There is no noise gate (CLEAN does rumble, harshness and S sounds). It never copies a voice. Loudness is MASTER. The beat is Beat Lock. “GlitchKid has no reverb; the space here is Echo + Dimension.” Words it doesn't know today (“rougher”, “unstable”) are flagged as new.
- **It never touches** Gain, Mix, the Output Limiter, Bypass, or your key and scale.
- **Privacy:** the mic is a separate device, open only while held. Speech is transcribed on this computer and discarded (`KidSpeech`). Typing always works.

## 4. Visual language

**Palette.** Near-black ink (`#0a0711`), ink-purple panels (`#160f21`, `#1d1529`), ultraviolet `#8f5bff` and hot pink `#ff4fb8` for energy, cyan `#5ee3ff` only for **hearing** (audition, DRY), acid green `#b8ff5c` only for **keep** (Keep buttons, KEPT, ON NOW), steel `#8fb6d8` only for **MASTER**. Paper `#efe5cf` and lilac sticky `#e9d6ff` are the analogue materials. Each accent colour has one job, so colour carries meaning without carrying it alone: every state is also a word or a shape.

**Type.** Handwriting (Kalam, SIL OFL 1.1, bundled in `assets/fonts/`, with Latin and Devanagari) is used **only for names and notes**: the session tape, card and notebook names, area titles, KID's notes. Every control, value, status and label uses Inter / the system font. Nothing in the window is smaller than 12 px; QA checks that no control, value or status uses the handwriting font.

**Materials and what they mean.**

| Material | Meaning |
|---|---|
| Masking tape (top bar) | The session's name: yours, editable |
| Taped cards on the desk | Ideas you haven't decided about |
| Cassette tapes (WORLDS) | Sounds to start from; the label stripe is the sound's colour |
| Lilac sticky note with KID's face | KID talking. Pink means a problem. |
| Ruled notebook with spiral holes | What you kept, for this session |
| Stickers | IDEA (paper) · KID (lilac) · ACCIDENT (pink) · HEARING (cyan) · KEPT / ON NOW (acid) · CHANGED (amber) · REV n · DRAFT · FACTORY · SIMULATED |

**Motion.** The waveform scrolls with the (simulated) take and redraws when a setting changes: tone shifts its colour, Doubler and Dimension spread it, Echo trails it, Mutation, Formant and Harmony split its edge, Stutter dashes it. Nothing moves on its own otherwise. Motion can be switched off; the waveform then freezes. A grain overlay at 7 % gives the dark a paper-like tooth without lowering contrast.

**Contrast.** 30 text/background pairs checked, all ≥ 4.5:1; the lowest is 5.00:1, white on the middle of the button gradient. Focus is always visible (a lilac ring).

## 5. Honesty labels

| Label | Where |
|---|---|
| SIMULATED TAKE | On the waveform |
| SIMULATED / SIMULATED MEASUREMENT(S) | CLEAN, Tune and compressor readouts; KID's measurement note; Beat Lock bands; Build My Mix listening; fitted rows in reviews; Mastering meters |
| DRAFT · NOT LISTENING-TESTED | Draft worlds (they are valid settings, not presets) |
| FACTORY · IN THE PLUG-IN / IN THE PLUG-IN | Factory tapes; Beat Lock, Reference Match, Build My Mix, Mastering mode |
| guided assistant · not generative AI | KID's bar, intro and Help |
| PROTOTYPE · simulated take | The help line, always |
| “Prototype: …” toasts | Simulated file pickers, routing and exports |

## 6. Window sizes

The prototype has three layouts: **compact 900×580**, **standard 1120×680** (today's design size) and **large 1360×800**. The layout *reflows* rather than scaling:

- **Compact:** KID and the notebook share the right column as tabs; the take's chips shorten (LIVE, 92 BPM, File…); cards show one-line names and two-line descriptions; the chain strip uses short names (FORM, HARM, DIM…); the logo and the sound chip step aside (the sound's name moves into the take's header).
- **Large:** a taller waveform, wider cards and a wider right column.

Today's plug-in scales uniformly between 70 % and 150 % with a fixed aspect ratio (`setResizeLimits`). Reflowing is a production decision (OPEN_DECISIONS §9).

## 7. Built on what exists vs. new engineering

| Uses what exists | Needs new engineering (no new DSP) |
|---|---|
| All 68 parameters, Bypass, `master_*`; the chain order; the factory profiles and the 22 parameters they keep | **Audition** (Hear, “With these”): a processor-side preview of a parameter set |
| `KidDirector` (rules, allowlist, memory), `KidSpeech` / `CommandMic` | **Three directions** from one sentence, and the new words |
| A/B slot APIs; `PresetController` save / overwrite / factory-name rules | **Redo** and one undo history for every change |
| Beat Lock (`beatMoves`), Reference Match, Build My Mix (`MixEngine`), the Mastering Lab | **Session name**, **notebook**, **desk** (per-project state) |
| Host BPM; input/output peaks; `VocalDiagnosis` findings | Reading FL Studio's play state; a real waveform from the input history |
| | Reflowing compact and large layouts; the handwriting font in the plug-in |

## 8. Relationship to earlier prototypes

This is a new folder. Nothing earlier was changed. It reuses the source-checked parts of `glitchkid-vocal-playground-v2`: the parameter table, factory profiles, KID's rule set, the A/B and save rules, and the knob, switch and module components. The session metaphor, desk, idea cards, happy accidents, notebook, worlds, three KID directions, redo, the review-before-keep tools and the layouts are new here.
