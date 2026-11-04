# QA report — The Session That Talks Back

Run on 2026-11-04 in the cloud build container: Linux, headless Chromium through the pre-installed Playwright 1.56.1, with the Inter font installed. Kalam (the handwriting) is bundled in the folder.

**Not checked:**
- macOS or Windows;
- FL Studio;
- any real plug-in window;
- a screen reader;
- real audio;
- browsers other than Chromium.

## 1. How it was checked

`qa/verify.js`:
- starts its own static server and opens the prototype in a 1460 × 1000 viewport;
- drives every journey with real clicks, key presses, mouse holds and the keyboard;
- asserts on the prototype's state (`window.GK.S`) after each step;
- with `--shots`, saves the screenshots.

To run it, from this folder:
```
node qa/verify.js            # checks only
node qa/verify.js --shots    # checks + screenshots/ (1x) and screenshots/hero/ (2x)
```

**Final run: 306 checks, 306 passed, 0 failed. No console errors.**

| Section | Checks | What is asserted |
|---|---|---|
| 0. Facts from the source | 10 | 68 parameters; the 17 stages in order; the 8 profiles; the 22 user-owned parameters; KID's allowlist; spot-checked defaults; the 5 ideas and 6 worlds from the brief exist. Every idea recipe (from two starting sounds) and every world value is a real parameter inside its range. No idea touches gain, mix, limiter or key. Worlds keep the 22 user-owned parameters. |
| 1. First launch | 13 | Three steps; the tape name; honest waiting, then ✓ signal; DRY/PROCESSED drive the real Bypass; ends in the session; layout of each step |
| 2. The session | 8 | Transport and BPM; LIVE; SIMULATED TAKE label; five cards; KID's fresh note; layout. **The waveform follows the real compare:** DRY draws differently, and PROCESSED draws exactly as before (canvas pixels compared). Paused message. |
| 3. Audition | 7 | Hear changes nothing; “HEARING · WIDE DREAM (NOT KEPT)”; the chain lights what you hear; HEARING sticker; the waveform draws what you hear; stop |
| 4. Look inside | 12 | Every change listed and ticked, all real parameters; untick; “With these” auditions only the ticked rows; Keep applies exactly the ticked rows; KEPT; the undo label; KID offers the notebook |
| 5. Put away | 2 | Nothing changes; Bring back |
| 6. Undo / redo | 4 | Buttons and Ctrl/Cmd+Z, Ctrl/Cmd+Shift+Z; card states follow |
| 7. Notebook keep | 6 | Name prefilled; empty name refused; name + note kept; ON NOW |
| 8. Happy accident | 7 | Real controls in range, never gain/mix/limiter/key; nothing changes until Keep; the notebook suggests its name |
| 9. Notebook, SHAPE | 16 | Hear compares without switching; Use without a question when the sound is kept; a knob is one undo step; the Hard tuning style; ON NOW · CHANGED; KID's Chromatic note opens TUNE; a measurement note is labelled SIMULATED MEASUREMENT; rename; remove asks |
| 9b. Hand move during a review | 3 | The row unticks (“YOU MOVED THIS”); Keep keeps your value; two undos go back |
| 10. Unsaved work | 7 | Asks before replacing an unkept sound; Cancel; Keep it first → notebook → then loads; exact `Profiles.inc` values; tuning kept |
| 11. WORLDS | 12 | Hear without loading; FACTORY / DRAFT labels; Look never lists user-owned parameters; Keep disabled with nothing ticked; take two settings only; a draft world loads exactly; Keep my version appears only after a change |
| 12. KID | 41 | Phases in order; three directions; nothing applied; allowlist; “wider” = Doubler + Dimension; honest labelling; revisions 2 and 3; upfront caps; Hear the first; Keep resets KID's memory; voice phases in order; a new set after a keep; darker; new words flagged; rougher/unstable; *keep the words clear* locks presence; distant without cutting presence; reverb honesty; unsupported timing changes nothing; not understood; quick tap; speech not set up (+ simulated download); mic blocked (typing works); hold K; not processing |
| 13. A/B | 12 | Empty captures without asking; changed; B captures and A keeps its own; occupied recalls; asks before leaving a changed slot; Cancel; Switch without storing; asks before storing over a different sound; Replace |
| 14. Save | 12 | Empty, file-system characters and factory names refused; saved; “Saved sound · Saved”; saving unchanged just says so; ● Unsaved; update offers the same name; Replace asks; Cancel goes back; Replace updates |
| 15. TOOLS | 29 | **Beat Lock:** unrouted explanation; routed; simulated labels; only `beatMoves` within their limits; balance advice only; partial keep; undo. **Reference Match:** no file, no rights, no analysis; only Body/Presence/Air; ≤ 3 dB a move, within limits; a band beyond ±6 dB left alone; never a copy, medium confidence; Cancel; lower intensity, smaller moves. **Build My Mix:** listens (simulated); never proposes user-owned parameters; the fitted row labelled; exact partial keep; “Built: … · ● Not kept yet”; undo; no-vocal path; cancel. “Build my mix” to KID opens it. |
| 16. MASTER | 8 | Asks first; Cancel; real defaults on the dials; vocal chain shown not in use; banner and KID off duty; KID refuses; off again |
| 17. States | 8 | No input; clipping (CLIP + KID); file is look-only; compact tabs |
| 18. Close and reopen | 9 | Intro stays done; name, sound, source, notebook, A/B and desk return; the saved sound is on the shelf and recalls exactly; Delete asks, and the current sound is marked “Deleted · ● Not kept yet” |
| 19. Layout | 45 | At compact, standard and large: SESSION; SHAPE's 6 tabs; LAB; WORLDS; TOOLS' 3 tabs; MASTER; the review drawer; Help |
| 20. Help, keyboard, contrast | 35 | Help content; keyboard focus reaches the plug-in with a visible ring; **30 contrast pairs ≥ 4.5:1** (lowest 5.00:1, white on the button gradient's middle); smallest text 12 px; no control, value or status in handwriting; no console errors |

**Layout checks.** There are 67 in all: 45 in section 19 and 22 in other sections. Each one flags:
- text cut off by its own box;
- text clipped by an ancestor, checked per axis: an axis stops being checked only above a container that scrolls on that axis;
- controls clipped or overlapping among what is really visible, using rectangles clipped by their scroll containers (the review drawer is treated as an overlay);
- text under 12 px;
- stray “null”, “undefined” or “NaN”.

Card names, descriptions and intents are clamped on purpose (`data-clamp`): their full text stays in the DOM and in Look inside.

**Negative test of the per-axis clipping check.** I injected a 60 px taller take panel. The check went from 0 clipped card buttons to 12. Earlier runs had also caught real defects: stray “null”, a clipped tape label, and the chain strip cut off at the compact size.

## 2. Problems found and fixed during QA

| Found | Fix |
|---|---|
| The text “null” in the intro and the speech dialog (native `append` with a null child) | A `put()` helper skips empty parts; the stray-text check guards it |
| The handwritten tape name clipped at the top (line height 1) | Line height 1.45 |
| WORLDS didn't redraw when you pressed Hear (no HEARING sticker or Stop) | The shelves redraw on every update |
| TOOLS didn't redraw after each analysis step; the intensity sliders didn't move | Each step redraws the panel; the sliders sync on every update |
| The chain strip overflowed at compact (stages 14–17 cut off) | Short names at compact only (FORM, HARM, MUT, DBL, WARM, STUT, DIM, LIM); full names stay in the label and help |
| The sound's name cut off in the compact take header | Shorter chips at compact (LIVE, 92 BPM, File…) |
| **Found in screenshot review, missed by the checks:** a wrapped card name or a long HEARING chip pushed the take footer onto two lines and hid the cards' Hear/Keep buttons (at compact, entirely) | The footer moved onto the waveform as an overlay; KID cards no longer repeat the quoted request (KID's note and Look inside show it); compact cards use 1-line names and 2-line descriptions. The clipping check was made per-axis so it now catches this (see the negative test). |
| The KID count on the rail: white on pink, 2.97:1 | Dark text, 6.2:1 |
| Toasts sat over the desk's Keep buttons | Moved to the top centre |
| The logo was truncated; KID's ✕ overlapped the note's text; card content overflowed | Wider logo; note padding; line clamps |
| “Keep my version” showed on an unchanged world | Shown only after a change |
| A kept card's “Keep it in the notebook?” hid behind an older KID reply | Keeping clears the old reply |
| KID started a new set for any sentence that had a sound word | Like `KidDirector`'s memory, any sentence refines the ideas on the desk; “start over” begins again; “a little less” with nothing on the desk says so |
| The speech status said only “Unavailable” after a failed mic press | It says “Speech isn't set up yet. Typing works.” / “Microphone blocked. Typing works.” |
| Beat Lock and Reference Match were approximations | Beat Lock follows `beatMoves` (limits; never pulls a value back from beyond its limit). Reference Match follows `ReferenceMatch.h`'s documented bounds. |
| A deleted saved sound still in use was labelled “Built” | “Deleted · ● Not kept yet” |
| False alarms in the overlap check (controls half scrolled out of view; the drawer overlay) | Rectangles clipped by their scroll containers; the drawer is treated as an overlay |

## 3. A finding in production code (not changed)

`ReferenceMatch.cpp` can move a band by more than its documented 3 dB limit. A band that starts beyond ±6 dB on the side opposite the step is pulled to the edge in one move: for example Air shelf −12 → −6 dB, which the Radio Static world produces. `ReferenceMatch.h` says such a band is “left alone rather than pulled back”. No existing test covers it. The details and a proposed fix are in `OPEN_DECISIONS.md` §15. The prototype follows the header.

## 4. Not verified, and limits

- **No audio.** The take, levels, meters, every measurement, speech recognition and KID's wording are simulated and labelled. Idea recipes and draft worlds are valid settings that nobody has listened to.
- **KID's numbers are the prototype's rules,** modelled on `KidDirector`, not its exact output.
- **Not tested** in FL Studio, on macOS or Windows, at real OS display scaling, with VoiceOver or Narrator, or with a host's keyboard focus. The K, D and Ctrl/Cmd+Z shortcuts may be captured by the host.
- **Browsers:** Chromium only, served and opened from disk. Safari and Firefox not checked.
- **Fonts:** screenshots use Inter (installed here) and the bundled Kalam. On a Mac without Inter the stack falls back to SF Pro (similar metrics; unchecked).
- **Not driven by QA:** KID's echo-sync note (a rule on `echo_on` / `echo_sync`), “Notes quiet”, the “Store in A, then switch” choice, and every possible happy-accident combination (one seeded roll is checked).
- **Persistence** uses the browser's local storage. In the plug-in it would be the project state (notebook, desk, name) and the preset folder.

## 5. Screenshots

`screenshots/` holds 29 files at 1x, named `NN_state_size.png` (sizes: compact 900×580, standard 1120×680, large 1360×800). `screenshots/hero/` holds 5 presentation shots at 2x:
- the session with KID's ideas, at standard and large;
- the compact session;
- the review drawer while auditioning;
- WORLDS.

`SCREEN_STATE_INVENTORY.md` lists which state each screenshot shows. The folder is about 28 MB.
