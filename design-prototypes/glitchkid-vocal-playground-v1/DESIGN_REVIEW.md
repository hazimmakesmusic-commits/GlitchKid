# THE VOCAL PLAYGROUND — design review (concept prototype v1)

**Status:** design concept for review. **Not approved for production.**

**Scope:**
* Nothing in `Source/`, `Tests/`, `CMakeLists.txt`, presets, DSP, parameters or build configuration was changed.
* The Vocal Lab prototype (`design-prototypes/vocal-lab-redesign-v1/`) and every earlier screenshot folder are untouched.

**Baseline:** repository at `dba3bd4`. The parameter layout, presets and macros are unchanged since `c3653aa`, where the parameter map was verified.

Labels: **REAL** = exists in the plug-in today · **CONCEPT** = proposed behaviour, not implemented · **ILLUSTRATIVE** = simulated picture or numbers.

## 1. What the prototype is built on (checked in the repository)

| Fact | Source |
|---|---|
| 69 parameters: IDs, ranges, defaults, skew centres and choice lists. The page's data is generated from the verified parameter map, so it cannot drift by hand. | `PluginProcessor.cpp` `createLayout`, via `vocal-lab-redesign-v1/PARAMETER_MAP.md` |
| 17-stage processing order, shown in THE PATH. | `PluginProcessor` (see PARAMETER_MAP header) |
| The 8 starting profiles, with every override value used when a Scene loads. | `Source/Presets/Profiles.inc` (parsed directly) |
| Loading a profile keeps 22 parameters (tuning ×5, `gain`, `mix`, `air_*` ×6, `clean_*` ×5, `lim_*` ×4) and sets everything else to its default plus the profile's overrides. | `PresetController::loadFactory`, `factoryDefaultState` |
| FEEL Tone, Punch and Space mappings. | `MacroEngine.cpp` (PARAMETER_MAP §2) |
| Real data sources for the Booth: input and output display taps, input/output peak, compressor and limiter gain reduction, Pitch Tune detected and target note (only while Tune is on), CLEAN telemetry, latency. | `PluginProcessor.h`, `DisplayTap.h`, `CleanLab::Applied` |
| Current editor design size: 1128 × 600, scaled 70–150 %. | `PluginEditor.h` (`designW`, `designH`) |

## 2. Real, concept or illustrative — every part of the prototype

| Part | Status |
|---|---|
| Every knob, switch and choice (68 sound parameters) and DRY / PROCESSED (`bypass`). | **REAL** parameters. The prototype changes only a JavaScript copy. |
| FEEL Tone / Punch / Space. | **REAL** (existing quick controls). |
| THE PATH order, numbering, lit or unlit state, the CLEAN split (#7 and #9). | **REAL** order. The lit state is computed from the settings. |
| THE VOICE picture. | **ILLUSTRATIVE.** The shape comes from a synthetic envelope and the layers from real settings. Production would use the real taps (§6). |
| IN/OUT meters, gain reduction, CLEAN "what it is doing", Tune read-out. | **ILLUSTRATIVE** values. The data sources are REAL. |
| EQ curve. | **ILLUSTRATIVE** approximation (not the plug-in's filter maths). |
| Compressor curve. | Computed from the settings with a standard soft-knee formula. The plug-in's exact curve may differ, and the screen says so. |
| THE ROOM (SPACE). | **ILLUSTRATIVE**, drawn from the settings, not from audio. |
| Latency read-out. | Approximation (same rule as the Vocal Lab prototype). |
| MAKE IT (word matching, proposal, review, apply). | **CONCEPT.** Nothing like it exists in the plug-in. |
| Hold to preview. | **CONCEPT** with an open engineering question (§4). |
| Scenes: names, art, moods, per-card audition. | **CONCEPT / PLACEHOLDER.** Loading a scene uses a REAL starting profile. |
| Undo (one level). | **REAL** (one level, like the plug-in today). |

## 3. Key decisions

1. **The Booth comes first, not the EQ.** The first screen answers "what does my voice sound like and how do I change it?" The four technical areas are one click away (SHAPE), and every stage is one click away on THE PATH.
2. **Three worlds, as you specified:**
   * SHAPE = CLEAN, TUNE, TONE, DYNAMICS;
   * CHARACTER = Formant, Harmony, Mutation, Stutter;
   * SPACE = Doubler, Echo, Dimension plus FINISH (Gain, Mix, Limiter).

   Where each stage lives:
   * **Saturation and AIR LAB** sit in SHAPE → TONE as tone shapers.
   * **Pitch Tune** is stage #1 but lives in SHAPE.
   * **Doubler** (#5) sits between Lab stages in the signal but lives in SPACE.
   * **Stutter** (#12) is late in the chain but lives in CHARACTER.

   THE PATH shows these interleavings openly, and each world shows a mini-map of its fixed positions.
3. **The Experimental Lab is kept separate** (your approved decision 2). CHARACTER is the only place its 11 parameters appear (tested).
4. **No reordering is implied.** The path stages are numbered, joined by one line ending in an arrow, and headed "Fixed order … they never move a stage". They open controls, nothing more. There is no drag handle (tested).
5. **Colour carries meaning, not decoration.** See DESIGN_CONCEPT §7. Amber is reserved for honesty labels, so simulated content is easy to spot.
6. **MAKE IT never touches what is yours.** It never proposes tuning (`tune_*`), `gain`, `mix`, AIR LAB (`air_*`), the limiter (`lim_*`) or `bypass` (tested across every word, intensity and five "surprise" picks). It can touch 32 parameters: EQ gains, compressor ratio/attack/threshold, saturation, CLEAN on/amount, Doubler, Dimension, Echo, and the four Lab effects.
   * **Difference from Build My Mix:** "clean" can switch CLEAN on and raise its amount, because the artist asked for it in words. Build My Mix never touches `clean_*`. **Your decision:** keep this, or keep CLEAN out of MAKE IT too.
7. **The approved Build My Mix safety rules hold for MAKE IT** (all tested):
   * nothing changes until Apply;
   * each change can be unticked, and an unticked change keeps its value;
   * Cancel, × and Escape change nothing;
   * Undo restores the exact previous state;
   * a partial apply shows "● modified".
8. **Scenes don't create presets.** The 8 scene names map onto the 8 existing starting profiles, and every card says PLACEHOLDER:

   | Scene | Starting profile |
   |---|---|
   | AFTER HOURS | SMOOTH R&B |
   | GLASS VOICE | CLEAN & NATURAL |
   | FRONT ROW | BRIGHT & PRESENT |
   | NIGHT DRIVE | MODERN MELODIC RAP |
   | DISTORTED ANGEL | DARK & ATMOSPHERIC |
   | UNDERGROUND | GRITTY RAP |
   | FLOATING | SPACIOUS LEAD |
   | RAW ENERGY | HARD-TUNED RAP |

   Whether the profiles are listening-tested isn't recorded in the repository, so they are treated as untested.
9. **A/B is not shown in this concept** because it wasn't in the brief. The approved A/B rules (an empty slot captures; nothing is overwritten silently) still apply and would sit next to Undo in the header.

## 4. Open questions for you

1. **Hold to preview.** It momentarily plays the proposal while you hold the button and reverts on release. It never changes the saved state, and the test confirms the state is unchanged while holding. However, it does change the **sound** before Apply, which bends the approved rule.

   Engineering is unresolved. Setting real parameters would be visible to the host (automation, the host's undo, "project changed"). A clean implementation needs a processor-side temporary override that bypasses the parameter tree, which is real DSP-adjacent work.

   **Options:**
   * (a) keep it as a later feature;
   * (b) drop it and rely on Apply + Undo (Undo is instant);
   * (c) allow it only for parameters that are safe to override.
2. **CLEAN in MAKE IT** (see §3.6).
3. **Typing inside a plug-in.** Some hosts (FL Studio among them) can take keyboard focus away from plug-in text fields. Chips work without typing, but the text field needs host testing before anyone relies on it.
4. **Scene names.** They are moods for review. Approving them doesn't approve new sounds. Designing real scene sounds would be separate work with listening tests.
5. **Size.** The concept is designed at 1120 × 680, the size agreed earlier. The editor today is 1128 × 600, so adopting this concept means changing the editor size.

## 5. Readability and screen sizes

* **Type scale (design px):** 12.5 for meta, values and badges; 13.5 for labels; 14.5 for body; 16–18 for titles; 30 for world names.
* **Measured smallest text:** 12.5 design px, so 10 px on screen at 80 %, 12.5 px at 100 % and 15.6 px at 125 %. No text was shrunk to make things fit.
* **Sizes:**

  | Scale | Size | Notes |
  |---|---|---|
  | 80 % | 896 × 544 | The measured minimum. |
  | 100 % | 1120 × 680 | The primary target. |
  | 125 % | 1400 × 850 | Should fit a 13-inch MacBook Air (1470 × 956 points) inside FL Studio's plug-in window, but this is **not verified**. |

  **No claim is made that it fits every screen.**
* **Where content scrolls (measured; the same at every scale because the whole frame scales uniformly):**
  * SHAPE → TONE, by 284 px (Warmth and AIR LAB are below the EQ);
  * the Scene library, by 137 px (the second row of cards);
  * the "ghostly" MAKE IT proposal, by 263 px (the proposal list scrolls on its own, separately from the conversation).

  The Booth, CLEAN, TUNE, DYNAMICS, CHARACTER and SPACE fit without scrolling.
* **Windows and macOS:** layouts use system fonts as a fallback (Inter is not bundled in the prototype). Real browsers on macOS and Windows weren't tested.

## 6. Limitations

* No audio. The voice, meters and analysis are simulated, and the motion is a synthetic envelope.
* The voiceprint in production needs a new component that reads the display taps at about 30 Hz. Its CPU cost is unmeasured.
* The EQ curve is approximate. The compressor curve is a standard formula, not the plug-in's.
* The latency figure is an approximation (31 ms Pitch Tune or Harmony, 21 ms Formant, 1.5 ms limiter, plus 6 samples).
* The VocalDose page couldn't be opened from this session (its host is outside the allowed network), so inspiration came from the brief's description only.
* Accessibility gaps:
  * The voice canvas is described only by its status line ("PROCESSED · 10 of 17 stages active") and the help text.
  * Colour-coded worlds are always paired with words and the legend.
  * Screen-reader testing wasn't done.
* Not tested: FL Studio, a real plug-in window, real macOS or Windows browsers, touch screens.

## 7. Checks actually performed

Headless Chromium (Playwright) on Linux, 30 October 2026. The scripts are in the session scratchpad, outside the repository.

* **Interaction and safety test: 46 checks, 46 passed, no page errors.**
  * **Parameter coverage:**
    * every one of the 68 sound parameters has a control in SHAPE, CHARACTER or SPACE;
    * `bypass` is the DRY / PROCESSED switch;
    * the 11 Lab parameters appear only in CHARACTER;
    * SHAPE and SPACE hold their stated parameters.
  * **THE PATH:** 17 stages numbered 1–17 in the real order with the real names; exactly #2, #3, #4 and #12 are CHARACTER; nothing is draggable.
  * **MAKE IT safety:**
    * opening, typing, unticking and holding Preview change nothing;
    * releasing Preview, Cancel and Escape change nothing;
    * Apply sets only the ticked rows, an unticked row keeps its value, and nothing outside the proposal moves;
    * "● modified" appears after Apply, and Undo restores the exact previous state.
  * **MAKE IT language handling:**
    * unrecognised words propose nothing, show as "Not used", and leave Apply disabled;
    * "keep the character" excludes every CHARACTER parameter;
    * typed HTML is shown as text;
    * "surprise me" is a repeatable fixed pick;
    * a sweep of every word at every intensity never proposes tuning, Gain, Mix, AIR LAB, limiter or Bypass, and changes nothing.
  * **Scenes:**
    * there are 8, all marked PLACEHOLDER;
    * loading UNDERGROUND gives exactly the defaults plus the GRITTY RAP overrides and keeps the 22 parameters that are yours;
    * a freshly loaded scene isn't "modified", and Undo restores.
  * **Audition and labels:** DRY sets `bypass` and the Booth says DRY. ILLUSTRATIVE, APPROXIMATE CURVE, GR EXAMPLE, the CLEAN and Tune example labels, DRAWN FROM SETTINGS, and the "no AI, speech or language model" statement are all present.
  * **Other controls:** FEEL Tone at the top sets Air shelf +6 dB and Presence +1.5 dB (the MacroEngine mapping). The Mutation pad works with the arrow keys. With no signal the meters read −∞.
* **Layout audit: 14 screens × 3 scales, each checked as shown and again with every panel scrolled to its end. 84 of 84 clean.**
  * The 14 screens are: the Booth, the stage drawer, the four SHAPE areas, CHARACTER, SPACE, three MAKE IT states, Scenes, DRY, and the Booth with Lab effects on.
  * Results: no text under 12.5 design px, nothing outside the frame, no horizontal or vertical clipping, no overlapping controls.
  * Decorative text inside the scene artwork (SVG) is excluded.
* **The Booth fits** without scrolling at 80, 100 and 125 % (each column was measured).
* **Scaling:** the frame measures 896 × 544, 1120 × 680 and 1400 × 850, and a knob drag behaves the same at every scale.
* Four bugs were found during the build and fixed:
  * world views drew over the Booth;
  * the Mutation pad had no keyboard access;
  * Escape didn't close MAKE IT when focus was outside the frame;
  * screenshots were scaled twice.

## 8. Screenshots

`screenshots/`: 15 states × 3 scales = 45 files, named `<080|100|125>_<nn>_<state>.png`:

| nn | State |
|---|---|
| 01 | Booth |
| 02 | Booth with the stage drawer for Echo (#13) |
| 03 | MAKE IT: "darker and more aggressive" |
| 04 | MAKE IT: "ghostly ad-lib", one row unticked |
| 05 | MAKE IT: holding Preview (the Booth shows the proposal) |
| 06 | Booth after applying "ghostly" and "glitchy alien" (CHARACTER layers visible) |
| 07 | Booth, DRY |
| 08 | Scene library |
| 09 | SHAPE · TONE |
| 10 | SHAPE · DYNAMICS |
| 11 | SHAPE · TUNE |
| 12 | SHAPE · CLEAN |
| 13 | CHARACTER |
| 14 | SPACE |
| 15 | Booth, no signal |

To review by hand, open `index.html`, set the browser zoom to 100 % (Cmd/Ctrl + 0), and use the toolbar: Scale (80/100/125 %), Signal (Example / None), Motion (On/Off), and Reset prototype.
