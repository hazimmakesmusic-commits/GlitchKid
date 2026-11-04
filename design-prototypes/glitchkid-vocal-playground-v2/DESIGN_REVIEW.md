# Design review — GlitchKid Vocal Playground v2

Reviewed on 2026-11-04 in the cloud build container: Linux, headless Chromium through the pre-installed Playwright, with the Inter font installed.

**Not reviewed:**
- macOS or Windows;
- FL Studio;
- any real plug-in window;
- a screen reader;
- real audio.

## 1. How it was checked

`qa/verify-prototype.js`:
- starts its own static server and opens the prototype in headless Chromium at a 1460 × 1080 viewport;
- drives the whole product with real clicks, key presses and mouse holds;
- asserts on the prototype's state after each step;
- `--shots` also saves the screenshots.

The final run: **139 checks, 139 passed, 0 failed**, with no console errors.

| Section | Checks | What is asserted |
|---|---|---|
| 0. Facts from the source | 5 | 68 sound parameters; the 17 stages in processing order; the 8 factory profiles; 22 user-owned parameters kept on a profile load; spot-checked defaults (`comp_ratio` 1, `echo_mix` 30, `dim_mix` 100, `tune_speed` 20, `lim_ceiling` −1, `harm_interval` 7, `echo_div` 4) |
| 1. First launch | 9 | Welcome; the honest waiting state; signal confirmed; DRY/PROCESSED switches the real compare; default path; privacy copy; layout; finishing lands inside the plug-in with Build My Mix |
| 2. Build My Mix | 15 | Opening the review changes nothing; never proposes the 22 user-owned parameters; "fitted" rows are labelled simulated; audition plays the proposal while the sound is unchanged; per-row untick; × changes nothing; Apply applies exactly the ticked rows; the unticked row is left; *Built:* name and *Not saved*; Undo restores exactly; Undo shows when empty |
| 3. KID | 31 | Holding opens the command mic; transcribing, interpreting and preparing are each shown; states run in order (recording → transcribing → heard → interpreting → preparing → proposal); a ready proposal changes nothing; "darker + aggressive" → less air, more compression; the allowlist; the transcript is shown; audition state; "a little less" = revision 2 at about 70 %; changed rows tagged; "keep it upfront" removes the presence cut; "darker" again; × keeps the proposal waiting; the Booth shows *waiting*; a hand-moved control is unticked and kept; Apply keeps the hand value; applied state; Undo restores the pre-Apply sound; "too much width" halves width; upfront caps Dimension; Cancel changes nothing; clean + character sequence; NEW WORDS flag; ad-libs psychedelic; "improve" uses labelled simulated measurements; not understood; the sound is unchanged after everything |
| 4. States | 14 | Speech not set up (and set-up); mic blocked; offline (KID works); offline + set-up; Mastering mode (KID unavailable; workspace banner; Booth layout); not processing; no input (and layout); Build My Mix without input; input clipping (and layout) |
| 5. Workspaces | 15 | Layout of SHAPE › CLEAN / TUNE / TONE / DYNAMICS, TONE advanced, SPACE and CHARACTER; Advanced reveals the right controls; the *Hard* tuning style writes 100 % / 0 ms and switches Tune on; a chain chip opens its module; the chip shows on; Reset touches only its module; Undo; the D shortcut |
| 6. Scenes | 5 | Layout; Preview plays without loading; Back leaves the sound unchanged; Load sets the real profile and keeps the user's tuning; the 8 concept placeholders cannot be loaded |
| 7. Saving | 11 | *Unsaved* shown as text; empty name refused; invalid characters refused; factory names protected; Save clears *Unsaved*; Update/Save-as menu; Update asks; Replace overwrites only after confirming; existing name flagged while typing; overwrite asks; Cancel leaves the saved sound untouched |
| 8. A/B | 11 | Both start empty; empty A stores without changing the sound; A shows *changed*; empty B stores and A keeps its own; occupied A recalls; leaving a changed slot asks; Cancel keeps you; *Switch without storing* leaves A untouched; storing over a different slot asks; Cancel keeps A; Replace stores |
| 9. Close and reopen | 4 | The intro doesn't return; sound, name and A/B slots return; the saved preset recalls exactly; Delete asks |
| 10. Help, keyboard, contrast | 19 | Holding **K** talks to KID; keyboard focus reaches the plug-in with a visible ring; Help; **13 contrast pairs ≥ 4.5:1**, the lowest being 5.00:1 (white on the gradient's middle, `#a63ad6`); smallest text 12 px; final Booth layout; no console errors |

**Layout checks** run on 15 screens and states. They flag:
- text cut off by its own box;
- **text clipped by any non-scrolling container** (added during this review; a negative test confirmed it catches the defect it was written for);
- controls clipped or overlapping among what is really visible on top;
- text under 12 px;
- stray "null", "undefined" or "NaN".

**Screenshots:** `screenshots/`, 85 files.
- 53 screens and states at 100 % (`100_…`).
- 16 of them also at 80 % (`080_…`, 896 × 544) and 125 % (`125_…`, 1400 × 850).
- File numbers follow the checks above (e.g. `100_18_kid_manual_edit_kept.png`).

## 2. Problems found during this review and fixed

| Found | Fix |
|---|---|
| The chain strip overflowed (GAIN / MIX / LIMIT cut off); the meters' OUT column was clipped; the Booth's left column ran past the window | Tighter chips (a dot only when on), a two-column meter grid, a compact left column |
| The top bar would overflow with both A/B slots occupied | Narrower logo and scene pill; 30 px step buttons |
| "Factory default · ● Unsaved changes" truncated in the top bar | "Default · ● Unsaved" (the full words remain in the Booth badge) |
| The literal text "null" in CHARACTER cards (native `append()` with a null child) | Filtered; the QA now fails on stray null/undefined/NaN |
| The KID drawer covered half the Booth visual, including the *PROPOSAL, NOT APPLIED* chip | While the drawer is open the Booth hides the KID column and shrinks the visual to 250 px, so it stays visible |
| The drawer opened at the top of the history, hiding the newest command and proposal | Scrolls to the newest exchange (your words, then KID's answer) |
| The prototype-only "what you say" picker took space inside the plug-in | Moved to the prototype controls outside the window |
| White text on the primary gradient's pink end was about 3:1 | Gradient darkened (`#6f3df2 → #a63ad6 → #c42f8f`): all three stops above 4.5:1 |
| The input-clipping explanation was cut off at the bottom of the Booth | Shorter copy; new "text clipped by its container" check |
| The simulated clipping input rarely reached full scale, so CLIP almost never lit | Simulation corrected (prototype only) |
| A security hook rejected `innerHTML` | No `innerHTML` anywhere: static SVG goes through `DOMParser`; KID messages are built from text nodes |

## 3. Limitations and things not verified
- **No audio.** Levels, meters, CLEAN telemetry, gain reduction, the Tune read-out, Build My Mix measurements and "improve it" are simulated and labelled. The visual is labelled ILLUSTRATIVE.
- **KID's numbers are the prototype's rule set,** modelled on `KidDirector`, not its exact output.
- **Not tested in FL Studio,** on macOS or Windows, at real OS display scaling, with VoiceOver or Narrator, or with a host's keyboard focus. Keyboard shortcuts (K, D, Ctrl/Cmd+Z/S) may be captured by the host; **unverified**.
- **Fonts.** Screenshots use Inter (installed here). On a Mac without Inter the stack falls back to SF Pro: similar metrics, but not checked.
- **Smallest text.** 12 px at 100 % is 9.6 px at 80 % and 8.4 px at today's 70 % minimum. The plan recommends an 80 % minimum; 9.6 px is readable on Retina but small on a 1080p screen at 100 % OS scaling.
- **Help line.** Long help texts truncate there by design (excluded from the overflow check); the full text is the control's accessible description.
- **Follow-up chips** scroll sideways. An edge fade hints at more, but it is still less discoverable than wrapping.
- **Advanced / More** open state is not remembered across sessions in the prototype.
- **Proposal lists are long** for Build My Mix from Factory Default (31 rows for SMOOTH R&B). They are grouped by stage, but a "collapse unchanged groups" or "show only big changes" option may help; untested with artists.
- **Not designed here:** BEAT (Beat Lock), REFERENCE (Reference Match), MASTER (Mastering Lab) and the instrument's Mandala view. They exist in the plug-in and must be placed (`IMPLEMENTATION_PLAN.md` §4).
- **Persistence** in the prototype uses the browser's local storage. In the plug-in, presets live in the preset folder and A/B slots in the project; this is how today's plug-in already works.

## 4. Brief checklist

| Brief section | Where it is answered | Notes |
|---|---|---|
| 1. Product and commercial goals | `DESIGN_CONCEPT.md` §2 | No unsupported claims; "AI" use is an open question |
| 2. Creative concept | `DESIGN_CONCEPT.md` §4–5 | Tokens, type and motion rules; no gamer neon or chat-window look |
| 3. First launch | Prototype onboarding (5 steps) | Skippable; ends inside the plug-in; no account or online step |
| 4. The Booth | Prototype Booth | Signal, processing, what's loaded and what next are answered at a glance; the visual is labelled illustrative |
| 5. Raw vocal → saved sound | `USER_JOURNEYS.md` journey 1 | All steps driven by the QA |
| 6. Three workspaces | SHAPE / CHARACTER / SPACE | Stage badges and the fixed chain strip; nothing implies reordering |
| 7. KID | Drawer, all states; `AI_*` docs | Push-to-talk default; separate mic; revisions; honest prototype labels |
| 8. Scenes | SCENES | Real factory profiles; concepts as placeholders; no preset files touched |
| 9. Manual control | Every parameter, real ranges | `INTENT_PARAMETER_MAP.md` §5 lists all 68 |
| 10. Build My Mix review | Drawer step 3 | Every rule tested, including a manual edit during a proposal (tested on the KID proposal, which shares the component) |
| 11. A/B, presets, undo, state | Top bar, dialogs | Approved A/B rules implemented and tested |
| 12. Screen inventory | `SCREEN_INVENTORY.md` | — |
| 13. Responsive design | 80 / 100 / 125 % screenshots; scrolling states | Recommends an 80 % minimum; universal fit not claimed |
| 14. Accessibility and polish | Contrast, focus ring, text + shape states, confirmations only for irreversible actions | Screen readers not tested |
| 15. Deliverables | This folder | v2 folder name explained in `DESIGN_CONCEPT.md` |
| 16. Review before stopping | This document | — |
| 17. Safety boundaries | No production file touched; nothing committed | See the final report |

## 5. Open questions for you
1. **Folder.** OK to keep this as `glitchkid-vocal-playground-v2`, leaving the earlier v1 concept as it is?
2. **"AI" in the name.** Speech is machine learning; KID's decisions are transparent rules. Keep "AI vocal director" in marketing, or use "vocal director"?
3. **Audition engine** (`IMPLEMENTATION_PLAN.md` §2). How should latency-changing rows (Tune, Formant, Harmony, Limiter) behave during an audition?
4. **Where do BEAT, REFERENCE and MASTER live** in the new information architecture?
5. **Minimum window size.** Raise it from 70 % to 80 %?
6. **Should the KID conversation be saved with the project,** or forgotten when it closes?
7. **Should Advanced / More stay open per module across sessions?**
8. **Concept Scenes.** Who designs and listening-tests them, and should the names be checked for trademark conflicts?
9. **Licensing, accounts and activation.** Unresolved; the prototype assumes none.
