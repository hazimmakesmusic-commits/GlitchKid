# Screenshot and review guide — vocal-lab-redesign-v1 (revision 3 polish)

The prototype is a static page: it plays and analyses no audio. Use this guide to review layout, readability and interaction at the three target scales.

## Open it

1. Open `design-prototypes/vocal-lab-redesign-v1/index.html` in a desktop browser (Chrome, Safari, Firefox or Edge). No server or internet is needed.
2. Set the browser zoom to **100 %** (Cmd/Ctrl + 0). The prototype has its own scale buttons; browser zoom would distort the measurement.
3. Use a browser window at least 1760 × 1100 px for the 150 % view (smaller windows scroll; that is fine for review).

## The review toolbar (above the plug-in frame)

* **Scale:** 80 % (896 × 544), 100 % (1120 × 680), 150 % (1680 × 1020).
* **Signal:** "Example frame" (simulated meters and spectrum) or "No signal" (the idle state).
* **Reset prototype:** returns to the example session (Modern Melodic Rap, Light tuning in A natural minor, Mutation left on; A/B slot A holds this sound, slot B is empty).

## Exported sets (headless Chromium, Linux)

* `design-prototypes/vocal-lab-redesign-v1-review-screenshots-rev6/` — **current**: revision 3 polish, 12 states × 3 scales = 36 files, named `<080|100|150>_<nn>_<state>.png`. Shows the two-line Build My Mix rows, 13 px values and statuses, the shorter tab statuses and the new 17-stage strip.
* `…-review-screenshots-rev5/` — superseded: the same 36 states, captured before the fix for clipped tab-status descenders (visible on "shaping").
* `…-review-screenshots-rev4/` — superseded: the same 36 states, captured before that fix and two others: the strip's end arrow touching stage 17, and a stale "Slot B was empty…" message after "Reset prototype" (state 12).
* Both were kept and not overwritten.
* `…-review-screenshots-rev3/` — revision 2 of the prototype, 11 states × 3 scales = 33 files.
* `…-review-screenshots-rev2/` — superseded: the same states captured before three layout fixes (Lab strip squeezed into one column, DYNAMICS tab dot overlapping, CLEAN label clipped). Kept, not overwritten.
* `…-review-screenshots/` and `…-review-screenshots-run2/` — revision 1 of the prototype (Lab effects inside VOICE/SPACE).

## Screens to capture at each scale (80 %, 100 %, 150 %)

These match the exported set (`nn` in the file names):

| nn | Screen | How to reach it |
|---|---|---|
| 01 | TUNE (default workspace) | Load the page |
| 02 | CLEAN on: three sections with trims, example telemetry and the ILLUSTRATIVE curve | CLEAN tab; switch Clean on |
| 03 | SPACE (Doubler, Echo, Dimension) | SPACE tab |
| 04 | Experimental Lab: one-row 17-stage strip in fixed order (Lab stages #2, #3, #4, #12 outlined pink, names under their column), then the Formant, Harmony and Mutation cards | LAB tab (dashed, after the divider), or "Open Experimental Lab →" on TUNE |
| 05 | Build My Mix review: one two-line row per change (checkbox, setting, Current → Proposed; reason below) | Reset prototype; "Build my mix…" → "Next: listen →" → "Simulate listening" |
| 06 | Review scrolled to LAB EFFECTS with one row unticked | Untick "Echo: mix", scroll to the bottom of the panel |
| 07 | Preset browser | Click the preset name |
| 08 | A/B: empty B captured | Click B (it was empty): label changes to "active" |
| 09 | A/B: B with unsaved changes | Change anything (e.g. TUNE → Hard): B shows "unsaved" |
| 10 | A/B: leaving unsaved changes asks first | Click A |
| 11 | A/B: overwrite asks first | Cancel, then "Save to B" |
| 12 | Build My Mix, longest proposal (GRITTY RAP, 30 rows) scrolled to the end: last row fully visible above the fixed footer | Reset prototype; "Build my mix…" → GRITTY RAP → "Next: listen →" → "Simulate listening"; scroll the panel to the bottom |

Also worth trying (not exported): Cancel and × in Build My Mix (nothing changes), "Switch without saving" (A recalled, B untouched), "Overwrite B" (B updated, label "active"), Signal order, Dry, the idle state, Beat Lock, Reference Match.

### Capturing

* **macOS:** Cmd + Shift + 4, then Space, then click the browser window (whole window), or drag over the plug-in frame. The capture tool shows pixel dimensions while dragging, which lets you confirm the frame size (e.g. 896 × 544 at 80 %).
* **Windows:** Win + Shift + S, choose rectangle or window.
* **Chrome (exact element):** open DevTools (Cmd/Ctrl + Opt/Shift + I), select the element `#scaler`, right-click → "Capture node screenshot".
* Name files `<scale>_<screen>.png`, for example `80_voice.png`, and keep them outside the repository (e.g. a folder in Pictures).

## What to check

**Readability (most important at 80 %)**
* Every label, value and status text can be read without leaning in. The smallest text is designed at 12.5 px, which is 10 px on screen at 80 %. Knob values, chain positions, A/B statuses and tab statuses are 13 px (10.4 px at 80 %).
* Workspace tab statuses ("A minor", "ratio 3.0:1", "3 of 3 on") are whole, including letters that dip below the line (p, g).
* Build My Mix rows: the reason is on its own line and wraps at most once or twice; Current and Proposed are never cut off.
* Values in the small boxes under knobs are not cut off (e.g. "4.00 kHz", "−17.8 dB", "1/8 dotted").
* Card titles, chain positions (#1–#17) and "Lab"/latency badges do not collide.

**Clipping and overlap**
* Nothing is cut off at the right or bottom edge of the frame.
* No two buttons or knobs overlap. Tab status lines should not need "…" at any scale; if one does, note which.
* The central workspace and the studio column scroll when content is taller than the panel; check that scrolling reveals everything (scroll inside the panel, not the page).

**Honesty**
* Simulated items are labelled: "EXAMPLE FRAME" over the signal, "example telemetry", "example — real detector output in the plug-in", "APPROXIMATE CURVE", the yellow "SIMULATED" banners in Beat Lock, Reference Match and Build My Mix.
* Off modules look different (hatched, dimmed) and say so; Dry shows the pink banner.
* No control suggests that modules can be reordered. The Lab strip says "fixed order, left to right — stages cannot be moved"; its stages are not buttons.

**Interaction**
* Knobs: drag up/down, mouse wheel, arrow keys (focus with Tab), double-click to reset.
* Build My Mix: Cancel, × and switching workspace leave the preset name and every value unchanged; an unticked row keeps its current value after Apply; a partial apply shows "Built: …" with "● modified"; Undo returns to the previous sound and name.
* A/B: an empty slot captures the current sound; an occupied slot recalls; nothing overwrites a slot without a dialog; leaving a slot with unsaved changes asks first; Escape in a dialog = Cancel.
* Experimental Lab: no Lab control appears in the six standard workspaces; the Lab tab shows "n of 4 on" when a Lab effect is active.

## Optional automated check (Chromium + Playwright, if you have Node)

The design phase used a headless Chromium script to measure the smallest computed text size, text outside the frame, horizontal and vertical clipping, and overlaps at all three scales (results in DESIGN_REVIEW.md §10). To repeat it, install Playwright in a folder **outside** the repository and load the page with `file://…/index.html`; for each scale button and workspace, read `getComputedStyle(el).fontSize` of every visible text element under `#plugin` and compare element rectangles. This is optional; the manual checks above are the reference.

## Recording findings

Write findings in a text file outside the repository (for example `~/GlitchKid-design-review-v1.txt`):

```
Browser / OS / display (resolution, scaling):
Scale 80 %:  readable? (Y/N, which items)  clipped? overlaps?
Scale 100 %: ...
Scale 150 %: fits the screen? ...
Build My Mix flow: clear? anything confusing?
Workspace grouping (TUNE/CLEAN/TONE/DYNAMICS/SPACE/FINISH + Experimental Lab): clear? ...
Anything that looks fake or misleading:
```
