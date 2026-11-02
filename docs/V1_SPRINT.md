# GlitchKid V1 finish sprint

## Audit: the top 5 issues

The audit was done at `9f1c863`, on a clean tree, from every page rendered by the test suite.

1. **Navigation overlapped.**
   * Three entry points (MAIN, START, BUILD) with different jobs.
   * Two frames: full-window screens without the rail (Main, Kid, Mastering) and rail pages.
   * Mastering could only be reached from Main.
2. **START was a dashboard.** A spectrum display, an empty box and four status tiles, duplicating the instrument's live view.
3. **BEAT showed an empty "Suggested Changes" panel.** It also put a developer doc path (`docs/SOURCES.md`) in the UI.
4. **The Creative page (LAB) was visually heavy.**
   * All four cards were pink whether or not the effect was on.
   * Mutation showed three dials of equal weight.
5. **The same action had different names.**
   * Dry / Original for the compare.
   * "Build My Vocal Mix" / "Vocal-Only: Build My Mix" / "Build My Mix" for the build.
   * Main / Start / Kid for the first page.

## Navigation (V1)

One frame on every page: the rail, ORIGINAL / PROCESSED, UNDO and MOTION at the top, presets and A/B at the bottom. The only exception is Kid, which is its own full-window view with its own way back.

| Group | Rail item | What it is |
|---|---|---|
| START | START | The instrument: the live in/out view, seven main dials, "Choose a sound", Ask Kid |
| START | SOUNDS | The starting sounds and BUILD MY MIX (was BUILD) |
| SHAPE THE VOCAL | CLEAN, TUNE, TONE, DYNAMICS, SPACE | In working order, numbered steps 1-4 in the header |
| CREATIVE | CREATIVE | Formant, Harmony, Mutation, Stutter (was LAB) |
| FIT TO TRACK | BEAT, REFERENCE | |
| FINISH | MASTER | The Mastering Lab (real and tested) |

Saved page numbers keep their meaning. 0, the old dashboard, now opens START. The dashboard is no longer on the rail; its code and its tests remain.

## What changed (UI only: no DSP, parameter, preset, state or routing change)

* **Rail:** 11 rows in 5 groups. Rows were renamed, MASTER added, and the old START dashboard dropped. Header subtitles give the step order.
* **START:**
  * the instrument inside the frame;
  * its own preset selector, compare and Advanced buttons removed (the frame has them);
  * one next step, "Choose a sound" (opens SOUNDS), and Ask Kid.
* **MASTER:** inside the frame. The header's ORIGINAL / PROCESSED is the level-matched compare, and the Mastering mode switch is explained next to it.
* **Compare:** "DRY" renamed ORIGINAL everywhere.
* **Build:** "BUILD MY VOCAL MIX" renamed BUILD MY MIX everywhere. The beat page's button is now "NO BEAT? BUILD MY MIX >".
* **BEAT:**
  * says what input it needs in plain words, with no developer path;
  * the right panel explains what will appear and when, until an analysis exists.
* **CREATIVE:**
  * cards are neutral until their effect is really on, then lit, with an ON/OFF label;
  * shorter notes;
  * Mutation shows Amount, and Character and Blend sit behind MORE.

## Verified (build environment, Linux, VST3 build + unit tests)

* All 466 test groups pass, including:
  * default settings pass audio bit-exactly;
  * bypass / ORIGINAL;
  * presets and A/B;
  * state round trips;
  * every dial moving its real parameters;
  * navigation never changing the sound;
  * every page opening and painting;
  * no developer paths on screen;
  * BEAT and REFERENCE showing no results before analysis;
  * CREATIVE cards following their effects.
* The VST3 builds.

## Not verified

* Loading and using the plug-in in FL Studio (or any host).
* The window at the smallest allowed size on a real screen.
* Real vocals and beats.

## Deferred

* An expandable "advanced" section on TUNE and TONE: they are already compact, and the risk outweighed the benefit for V1.
* Collapsing REFERENCE's technical intro text.
* Removing the old dashboard code.
