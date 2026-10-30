# GlitchKid Vocal Lab — redesign v1: design review (revision 3 polish)

**Revision 3 polish (this version)** is a narrow, prototype-only pass on revision 2; behaviour is unchanged:
* **Build My Mix review:** the drawer is 600 px wide (was 540) and each change is a two-line row: checkbox, setting name with parameter id, Current → Proposed on the first line, and the reason on its own full-width line below. The reason no longer wraps in a narrow column. The footer stays fixed, and the last row scrolls fully into view (checked with the 30-row GRITTY RAP proposal). Proposed values, the per-row checkbox and review-before-apply are unchanged.
* **Readability at 80 %:** knob values, chain positions (#n), A/B slot statuses and workspace tab statuses went from 12.5 to 13 design px (10.4 px on screen at 80 %). Tab statuses are shorter so they fit without ellipsis (e.g. "A minor", "ratio 3.0:1", "0.0 dB · limiter"); this is display text only. Nothing was shrunk; content that does not fit scrolls.
* **Experimental Lab chain strip:** one row of 17 equal, numbered stages on a connector line, ending in an arrow, with the heading "Signal chain · fixed order, left to right — stages cannot be moved" and a legend. The four Lab stages (#2, #3, #4, #12) have a pink outline (filled when on) and their names are shown under their own column. The stages are not buttons and cannot be dragged.
* **Two small fixes found while checking this pass:** the tab status line's descenders were clipped (fixed by trimming the tab padding and the status line-height, not the text size). "Reset prototype" now also clears the status-bar message.

**Revision 2** implements your four approved decisions in the prototype:
1. Real signal order kept; navigation is TUNE · CLEAN · TONE · DYNAMICS · SPACE · FINISH plus a separate **Experimental Lab** tab.
2. Formant, Harmony, Mutation and Stutter live only in the Experimental Lab, which shows their real chain positions (#2, #3, #4, #12) on a 17-stage strip.
3. Build My Mix is review-before-apply with one checkbox per proposed change; Lab effects are proposed off and ticked by default; Cancel, × and switching workspace change nothing.
4. A/B: an empty slot captures the current sound; an occupied slot is recalled; nothing overwrites a slot without an explicit confirmation; leaving a slot with unsaved changes asks first.

Design phase only. Nothing in `Source/`, `Tests/`, `CMakeLists.txt`, presets or docs was changed; the earlier prototype `design-prototypes/mvp-main-screen-v1/` is untouched. The prototype (`index.html`) is a standalone HTML page: **it processes no audio, is not connected to the JUCE plug-in, and every meter, spectrum, telemetry line and analysis result in it is simulated example data** (labelled on the page).

Labels used: **FACT** (checked in the repository), **PROPOSAL** (design decision), **ASSUMPTION** (not established).

---

## 1. Baseline

* Branch `claude/glitchkid-plugin-architecture-3o6w74`, HEAD `c3653aa`. The working tree still holds the uncommitted floating-point fix and the "signal-rail" shell (17 modified files plus untracked tests, `cmake/`, docs) and the earlier prototype folder. All preserved.
* FACT: the *working-tree* editor is the signal-rail shell, 1128 × 600; at HEAD it is the six-tab 960 × 600 layout. Both scale uniformly with a fixed aspect ratio, 70–150 %, and neither remembers the window size.

## 2. Current UI problems (grounded in the repository)

| # | Problem | Evidence |
|---|---|---|
| 1 | Text too small at the smallest window size | About 20 labels are created at 11 design px (`label (…, 11.0f, …)` in `PluginEditor.cpp`), others at 10.5 px; at the 70 % minimum that is 7.4–7.7 px. |
| 2 | Navigation does not match the job or the signal path | HEAD: six top tabs plus four FINAL TOUCH category tabs plus a chain strip repeating four stages. Working tree: a nine-row rail. The output limiter sits on the DYNAMICS page although it is the last stage (after Gain and Mix). |
| 3 | Four working effects have no controls | Formant, Harmony, Mutation and Stutter are in the signal path with parameters and tests, but the interface only warns when an old session has them on. |
| 4 | Build My Mix applies before you can see it | `runBuild()` calls `applyPreset ("Built: …", lastProposal.sound)` immediately; review happens afterwards through WHAT CHANGED and BACK TO MY SOUND. |
| 5 | Presets are names in a popup | The preset bar opens a popup menu; profile descriptions only appear on the BUILD page cards. |
| 6 | Beat Lock and Reference Match feel like separate products | Each is a full page/tab rather than a context tool next to the vocal controls. |
| 7 | Floating tooltips covered controls (HEAD) | Replaced in the working tree by an info line; the redesign keeps the info line. |

## 3. New hierarchy and rationale (PROPOSAL)

```
┌ Top bar: identity · preset (‹ name ● modified ›, browser) · A/B (labels: empty / active / unsaved / saved) · Save to A|B · Undo ··· Beat Lock · Reference Match · Dry|Processed ┐
├ Workspace bar: TUNE · CLEAN · TONE · DYNAMICS · SPACE · FINISH │ LAB (Experimental Lab, dashed, pink) · [Signal order]                                                ┤
├ Central workspace (selected group: module cards with switch, chain position, state, main controls, "Show advanced") │ Studio column  ┤
│                                                                                                                      │ signal + meters│
│                                                                                                                      │ quick controls │
│                                                                                                                      │ Build My Mix   │
│                                                                                                                      │ context status │
└ Status line: help for the control under the pointer · last action · latency                                                           ┘
```

* **Central controls first.** The central workspace takes about 70 % of the width. Each module is a card with a real switch (only where a parameter exists), its position in the 17-stage chain (#1–#17), a plain state ("on", "1:1 — not compressing", "0 = off"), its main controls and an optional "Show advanced".
* **Fast tools stay out of the way.** Tone/Punch/Space, Build My Mix and the context status live in the right column so they never push the precise controls around.
* **Beat Lock and Reference Match are context tools.** Two labelled buttons in the top bar open them over the central workspace while the studio column stays visible; "Back to <workspace>" returns.
* **Honest signal flow.** The workspaces are navigation groups. Every card shows its real chain position, and "Signal order" lists all 17 stages with their workspace. Nothing can be reordered and the design never suggests otherwise.

### Grouping: the brief's proposal vs this prototype

| Workspace | Brief proposed | Prototype | Why |
|---|---|---|---|
| 1 | TUNE: Pitch Tune, Formant, Harmony | **TUNE**: Pitch Tune only (#1) | Revision 2: Formant and Harmony moved to the Experimental Lab (your decision 2). A "Next in the chain" card points to them. |
| 2 | CLEAN | CLEAN | Same. Split in the chain (#7 and #9 around the compressor); the card says so. |
| 3 | TONE: EQ, Saturation, AIR LAB | TONE: same | Same. Not contiguous (#6, #10, #11); the card positions say so. |
| 4 | DYNAMICS: Compressor + Output Limiter | **DYNAMICS: Compressor** | The limiter is stage #17, after Gain and Mix. Showing it next to the compressor would imply the wrong order. |
| 5 | SPACE: Echo, Dimension | **SPACE: Doubler, Echo, Dimension** | Echo (#13) and Dimension (#14) are contiguous; the Doubler is a width/thickness effect (its card states that it runs before the EQ, #5). Stutter (#12) moved to the Experimental Lab; the Dimension card notes where it runs. |
| Lab | — | **EXPERIMENTAL LAB: Formant (#2), Harmony (#3), Mutation (#4), Stutter (#12)** | Your decision 2. A separate tab after a divider keeps the six standard tabs uncluttered; its status shows "n of 4 on" with a pink dot, so an active Lab effect is visible from every workspace. Each card states its neighbours ("after Pitch Tune (#1)", "runs between AIR LAB (#11) and Echo (#13)"). |
| 6 | FINISH: Doubler, Mutation, Stutter, Gain, Mix | **FINISH: Gain, Mix, Output Limiter** | Stages #15–#17, exactly the end of the chain; Doubler, Mutation and Stutter run much earlier. |

Remaining non-contiguity: CLEAN (#7/#9), TONE (#6/#10/#11), the Doubler (#5 in SPACE) and the Lab effects (#2–#4 and #12 in one workspace). All three are labelled with positions; the "Signal order" panel explains them.

## 4. What changed from the existing interface

* One six-workspace bar replaces tabs + categories + chain strip (HEAD) or the nine-row rail (working tree).
* Formant, Harmony, Mutation and Stutter get controls again, in a separate Experimental Lab workspace (marked "Lab", with latency badges where they add latency, and a 17-stage chain strip showing where they run).
* CLEAN (revision 2) uses its space for the three real sections — Corrective EQ (#7), Resonance (#7), De-esser (#9) — each with its real trim (`clean_tone`, `clean_res`, `clean_ess`) and its telemetry fields (example values), plus an ILLUSTRATIVE correction curve. Only the five real `clean_*` parameters appear.
* A/B (revision 2): the slot buttons show their state in words; "Save to A/B" replaces the silent STORE; confirmation dialogs guard overwrites and unsaved changes.
* The output limiter moves to FINISH with Gain and Mix.
* Build My Mix becomes review-first: Direction → Listen → Review (grouped, tickable, stale-aware) → Apply; Cancel changes nothing.
* A real preset browser for the existing presets: Factory Default and the eight profiles with their own descriptions, what each touches, and what a load keeps.
* Beat Lock and Reference Match become top-bar context tools.
* The status line keeps the hover help and adds the reported latency.

## 5. Readability and scaling trade-offs

| Role | Design px | 80 % | 100 % | 150 % |
|---|---|---|---|---|
| Meta: units, chips, legends, hints | 12.5 | 10.0 | 12.5 | 18.8 |
| Knob values, chain positions, A/B and tab statuses (revision 3) | 13 | 10.4 | 13 | 19.5 |
| Control labels, buttons | 13.5 | 10.8 | 13.5 | 20.3 |
| Body text | 14 | 11.2 | 14 | 21 |
| Card titles, workspace names | 16 | 12.8 | 16 | 24 |
| Workspace / panel titles | 22 | 17.6 | 22 | 33 |

* **Measured in the prototype** (headless Chromium, every workspace with all "advanced" sections open, plus Beat Lock, Reference Match, the preset browser and the Build My Mix review, at 80/100/150 %): smallest computed text size is 12.5 design px everywhere, so about 10 px at 80 %. No text outside the frame, no unintended clipping, no overlapping controls. One automatic overlap flag is a false positive: a checkbox scrolled beneath the Build My Mix footer inside a scroll container.
* **Trade-off accepted:** with the advanced controls open, the TONE, SPACE and EXPERIMENTAL LAB workspaces are taller than the panel and scroll vertically; the studio column's CONTEXT section sits just below the fold at 1120 × 680 and scrolls. Text is never shrunk to make things fit.
* Workspace tab status lines are short summaries (revision 3) and were measured not to truncate at any of the three scales. The ellipsis remains only as a safety net; the full state is in the workspace itself.
* **Not claimed:** that the plug-in fits every screen. 100 % (1120 × 680) remains the primary design target; 80 % is the measured minimum.

## 6. FL Studio window-fit concerns (ASSUMPTIONS until tested)

* 100 % = 1120 × 680, 80 % = 896 × 544, 150 % = 1680 × 1020.
* 150 % exceeds a 1080p display and a 13" MacBook Air (1470 × 956 or 1440 × 900 points) once FL Studio's wrapper title bar is added. The production version should cap the largest scale to the display that holds the editor.
* Unknown: whether FL Studio honours the plug-in's resize limits and fixed aspect, whether it remembers the editor size per project, and how its own "plugin scaling" option interacts with JUCE's scaling. The FL checklist from Phase 0 covers these.
* The design does not depend on hover: every hover help string is also available on keyboard focus.

## 7. Simulated vs verified

| Verified (repository) | Simulated in the prototype |
|---|---|
| All 69 parameter IDs, ranges, defaults, choice lists | Every meter value, the spectrum, compressor/limiter gain reduction |
| Signal order and chain positions | Pitch Tune read-out ("Sung A3 −18 cents") |
| Which parameters have on/off switches; latency per effect | CLEAN telemetry |
| The eight profiles' values and texts; what a profile load keeps | The EQ curve (simplified shapes, labelled "approximate") |
| Tone/Punch/Space and tuning-style mappings | Beat Lock analysis and suggestions; Reference Match results |
| Build My Mix target construction, kept parameters, Lab switch-offs, reason format | The two "fitted" Build My Mix changes and their measurements |
| A/B (2 slots), one-level Undo, Dry = real bypass | A/B rules (empty = capture, occupied = recall, confirm before overwrite or losing unsaved changes) are proposals for the editor |
| CLEAN telemetry fields (`CleanLab::Applied`) | CLEAN values and the ILLUSTRATIVE correction curve |
| Reference Match formats and limits | File choosing (disabled) |

## 8. Accessibility and keyboard

* Prototype: confirmation dialogs take focus on their first button (Cancel) and close with Escape (= Cancel); every control is focusable; knobs respond to arrows, Page Up/Down, Home/End and double-click reset; workspace tabs move with Left/Right; Escape closes the browser and the signal-order panel; focus rings are visible (pink outline); switches expose `role="switch"` with state; knobs expose `role="slider"` with value text.
* State never relies on colour alone: off modules are hatched and say "off"; disabled controls are dimmed and the help line says why; stale Build My Mix rows carry text, not only colour.
* Contrast (computed with the WCAG formula): body text #f1edf6 on #08070b 17.4:1; secondary #b3abbf 8.0–9.1:1; muted #8a8297 4.8–5.5:1 on the panel colours (passes AA for normal text); white on violet buttons 4.6:1; warning text #ffbe5c on its banner 11.3:1. Re-measure on the final JUCE colours.
* JUCE equivalents needed: explicit focus order (`setFocusContainerType`, `setExplicitFocusOrder`), `AccessibilityHandler` titles and value text for custom knobs, keyboard handling that does not fight FL Studio's own shortcuts.

## 9. macOS vs Windows (needs separate validation)

* **Fonts:** the plug-in bundles Inter (Regular, Bold) under `Assets/Fonts`; the HTML prototype falls back to system fonts if Inter is not installed, so text widths in the prototype can differ slightly from the plug-in.
* **High-DPI:** macOS Retina doubles pixels; Windows uses per-monitor DPI scaling (100–200 %) that combines with JUCE's own scale and FL Studio's plug-in scaling option. The 80 % floor must be checked on a 100 %-DPI Windows display, where physical text is smallest.
* **Keyboard:** FL Studio's plug-in wrapper may capture keys (space, arrows) on either platform; keyboard control of knobs must be tested in the host.
* **Mouse wheel:** direction and step size differ (macOS natural scrolling, Windows wheel notches).
* **File formats:** Reference Match reads M4A only on macOS (CoreAudio).
* **Build:** `-ffp-contract=off` applies to Clang/GCC; MSVC's floating-point behaviour is not yet verified; Windows has never been built.

## 10. Checks actually performed (revision 3 polish)

Headless Chromium (Playwright) on Linux, 30 October 2026. Scripts are in the session scratchpad, outside the repository. **Not checked:** any real browser on macOS or Windows, FL Studio, a plug-in window, or a real display at 80 %.

* **Interaction test (revision 2 suite, selectors updated for the new rows): 41 checks, 41 passed**, with no page errors. Lab separation, Build My Mix (nothing changes until Apply; unticked rows keep their value; partial apply shows "modified"; Undo), A/B (empty slot captures; occupied slot never overwritten without a dialog), and CLEAN (only the five `clean_*` controls; curve labelled illustrative) behave as in revision 2.
* **Polish checks at 80/100/150 %: 27 checks, 27 passed.** No tab name or status truncated. No A/B label truncated. No Build My Mix name or value clipped. The last change row is reachable above the fixed footer, and the footer does not move. The smallest on-screen text is 10.0 / 12.5 / 18.75 px. The strip is one row of stages 1–17 in order, with the Lab stages at 2, 3, 4 and 12, and the four Lab labels do not overlap.
* **Long proposal (GRITTY RAP, 30 rows):** after scrolling, the last row is fully visible and no cell is clipped, at all three scales.
* **Strip end arrow:** stage 17 and its arrow lie inside the visible workspace at all three scales.
* **Reset prototype** clears the status-bar message (1 check, passed).
* **Layout audit, 36 states** (7 workspaces with "advanced" open, the Build My Mix review, the A/B confirmation, the preset browser, Beat Lock and Reference Match, each at 3 scales): no text under 12.5 design px, nothing outside the frame, no clipping and no overlaps in any of the 36. The audit now also checks **vertical** clipping. Run against the pre-fix tab CSS, it flags the clipped tab statuses (19 px of text in a 14 px box) in all 36 states, which proves the check works.
* **Screenshots:** `vocal-lab-redesign-v1-review-screenshots-rev6/`, 12 states × 3 scales = 36 files, no page errors. `-rev4/` (before the strip-arrow, reset-message and tab-descender fixes) and `-rev5/` (before the tab-descender fix) were exported earlier in this pass. Both are kept, not overwritten, and are superseded.

## 10a. Rendering and interaction checks performed for revision 2 (kept for history)

Headless Chromium (Playwright) on Linux, 28 October 2026. **Not checked:** any real browser on macOS or Windows, FL Studio, or a plug-in window.

* **Interaction test, 41 checks, 41 passed** (`verify_rev2.js` in the session scratchpad, outside the repository):
  * No Lab control appears in TUNE, CLEAN, TONE, DYNAMICS, SPACE or FINISH (with every "advanced" section open). All 11 Lab parameters are in the Lab. The cards show #2, #3, #4 and #12, the strip shows 17 stages with 4 marked, and the Lab tab shows "1 of 4 on".
  * Build My Mix leaves the prototype state byte-identical after opening, computing, switching workspace, closing with × and Cancel. Every proposed change has its own checkbox (9 of 9) with separate Current and Proposed columns. Mutation is proposed off and ticked by default. Ticking and unticking change nothing.
  * After unticking two rows and applying: the unticked settings are unchanged, every ticked change is applied, and nothing else moves. The preset reads "Built: …" with "● modified", and Undo restores the exact previous state. An edit during review marks that row stale and unticks it, with the Rebuild banner.
  * A/B: starts with A active and B empty. An empty B captures the sound unchanged; an edit shows "unsaved". Leaving asks first; Cancel keeps B and the edits. "Switch without saving" recalls A and leaves B's slot untouched; occupied B with no edits is recalled directly. "Save to B" on an occupied slot asks; Cancel leaves the slot untouched; confirming overwrites it and B shows "active".
  * CLEAN shows exactly the five `clean_*` controls and its curve is labelled illustrative.
* **Layout audit at 80/100/150 %** (all seven workspaces with "advanced" open, the Build My Mix review, the A/B confirmation, the preset browser, Beat Lock and Reference Match): smallest computed text 12.5 design px (10 px at 80 %), nothing outside the frame, no clipped text, no page errors. The only overlap flags come from a "Show advanced" link scrolled out of view inside the workspace scroll container (a false positive of the measurement).
* Workspace tab names fit at every scale (checked that none is truncated); long tab status lines end in "…" by design.

## 11. Revision 1 rendering check (kept for history)

Rendered with the pre-installed headless Chromium (Playwright) on Linux at 80/100/150 %, through every workspace, the context tools, the preset browser and the Build My Mix flow. Checked: no page errors; smallest text size; text outside the frame; clipping; overlapping controls. Also checked: a stale row appears after editing a parameter that is in the proposal; a partial apply gives "Built: …" with "● modified"; Undo restores the previous preset name. **Not checked:** any real browser on macOS or Windows, FL Studio, or a real plug-in window.
