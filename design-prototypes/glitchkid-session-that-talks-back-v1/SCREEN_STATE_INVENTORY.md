# Screen and state inventory — The Session That Talks Back

Every screen and state in the prototype: how to reach it, what it shows, its screenshot (`screenshots/`, at the size given), and the QA section that drives it (`qa/verify.js`, results in `QA_REPORT.md`).

- **Prototype controls** = the bar above the plug-in window.
- **Screenshot sizes:** `_compact` 900×580, `_standard` 1120×680, `_large` 1360×800, all at 1x. `screenshots/hero/` holds five presentation shots at 2x.

## 1. Areas

| Area | How to get there | Shows | Screenshot | QA |
|---|---|---|---|---|
| SESSION | Rail · start | The take, ideas on the desk, KID's note and bar, the notebook | `03_session-fresh_*` (all 3 sizes) | 2–12, 17, 19 |
| SHAPE › CLEAN / TUNE / TONE / DYNAMICS / SPACE / OUTPUT | Rail · a chain chip · a KID note (Open TUNE) | Every main control with its stage numbers; Reset; Advanced / More | `18_shape-space_*` (all 3 sizes) | 9, 11, 19 |
| LAB | Rail · chain chips 2, 3, 4, 12 | Formant, Harmony, Mutation, Stutter | `19_lab_standard` | 19 |
| WORLDS | Rail · the sound chip | Your saved sounds · factory tapes · draft worlds | `08_worlds_standard`, `_large`; `hero/worlds@2x` | 10, 11, 18, 19 |
| TOOLS › BEAT LOCK / REFERENCE MATCH / BUILD MY MIX | Rail · KID (“build my mix”) · a KID note (Open Beat Lock) | The three tools | `13_beat-lock_standard`, `14_build-my-mix-review_*` | 15, 19 |
| MASTER | Rail (below the line) | Mastering mode, style, dials, meters, load/export | `15_master-on_standard` | 16, 19 |

## 2. States from the brief

| # | State | How to reach it | What the user sees | Screenshot | QA |
|---|---|---|---|---|---|
| 1 | **Initial empty session** | First launch, or after Reset prototype data → Skip | Factory Default · Saved; 5 idea cards; KID's “Fresh session” note with “Roll a happy accident”; empty notebook | `03_session-fresh_*` | 2 |
| 2 | **Vocal loaded / playback active** | Vocal = Live, FL Studio = Playing | LIVE · from FL Studio, FL Studio playing · 92 BPM, moving waveform and meters | `03_…`, `09_…` | 2 |
| | Vocal from a file | Look at a file… (or Take = Loaded file) | FILE · hook_take_03.wav · look only; a static waveform; Back to live input | — | 17 |
| | Paused | FL Studio = Stopped | The waveform freezes and dims; “Paused · Press play in FL Studio…” | — | 2 |
| 3 | **KID idle** | Default | Tell KID · guided assistant; “Ready. Hold to talk or type. Speech stays on this computer.” | `03_…` | 12 |
| | **KID listening** | Hold the mic or K | The mic turns pink; a live level; “Listening… let go to send” | `10_kid-listening_standard` | 12 |
| | **KID processing** | Release or Send | Transcribing on this computer… → Reading your words… → Leaving ideas on the desk… | — | 12 |
| | **KID unavailable** | Speech = Not set up / Mic blocked · Engine = Not processing · Mastering mode on | A pink note that says why and what to do (Set up speech… / where to allow the mic / switch Mastering off). Typing always works when speech is the only problem. | `15_master-on_standard` | 12, 16 |
| 4 | **Suggestion displayed** | Type “Make this hook feel wider.” | KID's note (“Heard: wider. I left 3 ideas on the desk…”, Hear the first); three KID cards; a count on the rail | `09_kid-three-ideas_*`; `hero/session-kid-ideas@2x` | 12 |
| | Revised suggestion | “A little less.” / “Keep the vocal upfront.” | The same cards with REV 2 / REV 3 and new values | `16_no-input_standard` (REV 6) | 12 |
| 5 | **Idea auditioning** | Hear on a card, a notebook entry or a tape; “With these” in a review | Cyan HEARING sticker; the take says “HEARING · WIDE DREAM (NOT KEPT)”; a cyan frame on the waveform; the chain lights what you hear; the sound is unchanged | `04_audition-idea_standard` | 3, 4, 9, 11, 12 |
| 6 | **Proposed changes reviewed** | Look inside · N changes (cards, tapes, tools) | The drawer “What changes”: rows in processing order, grouped by module (SHAPE / LAB), from → to, a reason, ticks, All / None, Listen Current / With these, Cancel / Keep N changes | `05_look-inside_*`; `hero/review-drawer@2x` | 4, 11, 15 |
| | A row moved by hand | Move one of the listed controls while the drawer is open | The row unticks: “YOU MOVED THIS · YOUR VALUE IS KEPT” | — | 9b |
| 7 | **Apply confirmed** | Keep | KEPT sticker; a toast with Undo; KID's note “… is yours now. Keep it in the notebook?” | `06_notebook-kept_standard` | 4, 7 |
| 8 | **Rejected / cancelled** | ✕ on a card · Cancel or × in the drawer · ✕ on a note | The card leaves the desk (toast: Bring back) · nothing changes · the note goes | — | 5, 11, 15 |
| 9 | **Undo and redo** | ↶ / ↷, Ctrl/Cmd+Z, Ctrl/Cmd+Shift+Z | Toasts “Undone: …” (with Redo) / “Redone: …” (with Undo); the cards' KEPT state follows | — | 6, 15 |
| 10 | **Notebook with ideas** | Keep this sound, or Keep in notebook… from a note | Entries with names, notes, “kept just now · from …”, ON NOW / ON NOW · CHANGED; Hear · Use · Rename · ✕ | `06_…`, `09_…` | 7, 9 |
| 11 | **A/B empty** | Start | A empty · B empty | `03_…` | 13 |
| | **A/B captured** | Click an empty slot | “playing”; ⤓ appears; “changed” (amber) after a change | `12_ab-slots_standard` | 13 |
| | **A/B recalled** | Click an occupied slot | The sound switches; asks first if the playing slot has changes (Cancel / Switch without storing / Store in A, then switch) | — | 13 |
| | Storing over a different sound | ⤓ on a slot that holds another sound | “Replace what's in A?” | — | 13 |
| 12 | **Unsaved changes** | Change anything | The sound chip: “● Unsaved”; a Built sound: “● Not kept yet” | `09_…` | 14, 15 |
| | **Confirmation** | Use a tape or a notebook entry while your sound isn't kept anywhere | “Your current sound isn't kept anywhere”: Cancel / Use … anyway / Keep it first | — | 10, 11 |
| 13 | **Build My Mix proposal with selectable changes** | TOOLS › BUILD MY MIX → Start → listen → direction → Review the changes | Every change, ticked; fitted rows labelled SIMULATED MEASUREMENT; Keep N changes → “Built: SMOOTH R&B · ● Not kept yet” | `14_build-my-mix-review_*` | 15 |
| | Build My Mix with no vocal | Vocal = None | “No vocal heard yet” + Skip: use as designed; the review says “not fitted to your take” | — | 15 |
| 14 | **Error / unsupported request** | “Fix my timing.” · “purple monkey dishwasher” · a quick tap on the mic | Pink note: what GlitchKid can't do and what it can; or “I didn't catch a sound word…”; or “Hold the button while you talk”. Nothing changes. | `11_kid-unsupported_standard` | 12 |
| 15 | **Small and larger window layouts** | Window = Compact / Standard / Large | See DESIGN_CONCEPT §6 | every `_compact` / `_large` file; `hero/session-compact@2x`, `hero/session-kid-ideas-large@2x` | 17, 19 |

## 3. Other states

| State | How to reach it | What the user sees | Screenshot | QA |
|---|---|---|---|---|
| First launch · 1 name | First run · Restart intro | “This is your session”: name the tape (any language) | `01_first-launch-name_standard` | 1 |
| First launch · 2 signal | Next | Waiting for sound… / ✓ Vocal coming in; DRY / PROCESSED | `02_first-launch-signal_standard` | 1 |
| First launch · 3 KID | Next | “KID leaves notes”: Notes on / Only when I ask | — | 1 |
| No input | Vocal = None | “No vocal yet” on the waveform; KID's note says what to do | `16_no-input_standard` | 17 |
| Input clipping | Vocal = Clipping | CLIP lights; KID: “clipping before it reaches me… I can't repair clipped audio” | `17_clipping_standard` | 17 |
| Not processing | Engine = Not processing | “Not processing” on the waveform (a design placeholder: no verified trigger); KID unavailable | — | 12 |
| Mastering mode on | MASTER › switch → confirm | Steel banner on every vocal area; chain chips off; KID off duty | `15_master-on_standard` | 16 |
| Happy accident | Happy accident | A pink ACCIDENT card made of 3 random real moves | `07_happy-accident_standard` | 8 |
| Echo not synced (note) | Echo on with Sync off | KID: “Echo isn't locked to the song. FL Studio says 92 BPM.” → Lock echo to tempo | — | (rule; not driven) |
| Tune chromatic (note) | Pitch Tune on with Chromatic | KID: “Pitch Tune is pulling to every note” → Open TUNE | — | 9 |
| Measurement (note) | After keeping an idea, CLEAN off | KID: low-mid build-up near 300 Hz, SIMULATED MEASUREMENT → Ideas with CLEAN | — | 9 |
| Notes quiet | Intro step 3 or Help → KID notes: quiet | “KID · notes are quiet. I'll answer when you ask.” | — | (not driven) |
| Saved sound | Save… | “Saved sound · Saved”; on the WORLDS shelf; ✕ asks before deleting | — | 14, 18 |
| Deleted saved sound in use | Delete the saved sound you are using | “Deleted · ● Not kept yet”; your sound stays | — | 18 |
| Close and reopen | Close & reopen project | The session name, sound, desk, notebook, A/B and saved sounds return; the intro doesn't | — | 18 |
| Reference Match, rights | TOOLS › REFERENCE MATCH | Analyse is disabled until a song is chosen and “I have the right to use this song” is ticked | — | 15 |
| Help | ? | Shortcuts, “Where's play?”, how KID works; KID notes on/quiet; Show the intro again | `20_help_standard` | 20 |

## 4. Dialogs

| Dialog | Asked when | Choices |
|---|---|---|
| Keep this sound / Rename | Keeping to the notebook / renaming an entry | Name (required, ≤ 40 characters), note (optional) · Cancel / Keep (Rename) |
| Remove “…”? | ✕ on a notebook entry | Cancel / Remove |
| Your current sound isn't kept anywhere | Using another sound while yours is only in the live state | Cancel / Use … anyway / Keep it first |
| Save this sound | Save… | Name (no empty name, no `\ / : * ? " < > \|`, no factory names) · Cancel / Save |
| Replace “…”? | Saving over an existing name | Cancel (back to Save) / Replace |
| Delete “…”? | ✕ on a saved sound | Cancel / Delete |
| Switch to B? | Leaving a changed A/B slot | Cancel / Switch without storing / Store in A, then switch |
| Replace what's in A? | ⤓ over a different sound | Cancel / Replace A |
| Turn on Mastering mode? | MASTER switch | Cancel / Turn on Mastering mode |
| Set up speech | Speech not set up | Not now / Download (simulated progress) → Done |
| Help | ? | KID notes on/quiet · Show the intro again · Close |

Escape closes any dialog; Tab stays inside it.
