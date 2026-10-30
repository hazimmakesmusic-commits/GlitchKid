# KID — design review (concept prototype v1)

**Status:** concept for review. **Not approved for production.**

**What changed in the repository:**
* Nothing in `Source/`, `Tests/`, CMake, presets, DSP or parameters changed.
* `../glitchkid-vocal-playground-v1/`, `../vocal-lab-redesign-v1/` and every earlier screenshot folder are untouched.
* This folder is new.

## 1. What is real, simulated or a concept

| Part | Status |
|---|---|
| The push-to-talk mic and its seven states | **SIMULATED.** No microphone, recorder or speech API is called (a test traps all of them). Releasing the mic "says" the typed text or the first suggestion, and the line is labelled "SPOKEN (SIMULATED)". |
| KID's understanding (lexicon, memory, constraints, decisions) | **CONCEPT, working in the page.** Deterministic rules, documented in INTENT_PARAMETER_MAP.md. No AI model. |
| Parameter IDs, ranges and defaults in every proposal | **REAL** (generated from the verified parameter map) |
| Validator (allowlist 57/69; ranges) | **CONCEPT, working in the page.** Mirrors the production design. |
| Apply / Undo / Cancel / untick / manual override | **CONCEPT, working in the page.** The production path would use `applyParameterChanges` and `backToMine()` (REAL). |
| HEAR CURRENT \| KID | **CONCEPT.** It only changes the picture; the engineering is open (§4). |
| Vocal read numbers | **ILLUSTRATIVE.** The fields exist in `VocalAnalyzer` and `CleanLab::Applied`. Stereo and pitch range are marked "not measured". |
| Voiceprint, IN/OUT levels | **ILLUSTRATIVE** |
| SHAPE / CHARACTER / SPACE, scenes | From the Vocal Playground concept (manual controls for all 68 sound parameters) |

## 2. Key decisions

1. **KID is the Booth.** The mic, the session script and KID's Take fill the first screen, and the manual worlds are one click away.
2. **A script, not a chat.** It addresses the "looks generic" feedback and makes KID read like a producer's notes.
3. **Direction with memory.** Follow-ups reshape the same take, and the rows show ▲ / ▼ ADJUSTED and "No longer changing …". This was the core requirement of the brief and is tested.
4. **Goal-directed, direction-safe moves.** The first build of the rules produced "wider" proposals that *lowered* Doubler width, because absolute targets sat below the current value. It was fixed so every continuous move heads towards a goal only in the asked direction, and the suite now checks it.
5. **Constraints beat order.** "Wide but keep it upfront" is resolved by caps, not by whichever word came last.
6. **Honest refusals:** level, tuning, and reverb (there is no reverb module).
7. **Local-first recommendation.** See PRIVACY_AND_COSTS.md: $0 running cost and no data leaving the machine for the MVP.

## 3. Checks actually performed

Headless Chromium (Playwright) on Linux, 30 October 2026. The scripts are in the session scratchpad, outside the repository.

* **KID interaction, memory, safety and honesty: 51 checks, 51 passed, no page errors, no network requests.**
  * **Booth layout:** the Booth holds the mic, script and Take; manual controls remain for all 68 sound parameters.
  * **Push-to-talk (simulated):**
    * holding the mic shows LISTENING TO YOU and changes nothing;
    * Escape cancels and nothing is sent;
    * each of the seven states shows its label;
    * "speech unavailable" disables the mic, and typed requests still work;
    * a released push-to-talk line is labelled spoken (simulated);
    * **no microphone, MediaRecorder or SpeechRecognition API, and no network, was ever called.**
  * **Safety:**
    * proposals, HEAR KID and unticking change nothing;
    * Apply sets exactly the ticked rows;
    * "● modified" appears; Undo restores exactly; Cancel changes nothing.
  * **Memory:**
    * wider → "too much, make it subtle" reduces the same rows (tagged ▼ ADJUSTED) and adds no new ones;
    * "keep the vocal upfront" caps echo (≤ 12 %) and doubles (≤ 28 %), never lowers Presence, and keeps the width;
    * darker → "a little less" → "keep the presence" behaves as specified, and removed changes are listed;
    * "start over" clears the kept constraints.
  * **Conflicts and roles:** "wide but keep it upfront"; ad-libs distant (1/4 throws, thinner low end).
  * **Analysis:**
    * "improve it" passes through processing → analysing → preparing → ready to audition;
    * it proposes cleaning, a mud cut and steadier compression;
    * VOCAL READ is labelled illustrative and marks stereo and pitch range as not measured;
    * LIMITS says the command mic never enters the vocal chain.
  * **Boundaries:**
    * 21 phrases never propose tuning, Gain, Mix, the limiter or Bypass;
    * the allowlist is 57 of 69;
    * "louder" is declined; "reverb" gets an honest answer;
    * unknown words leave Apply disabled;
    * typed HTML is shown as text.
  * **Manual override and path:** moving a proposed control by hand unticks it; THE PATH highlights only the stages KID will change.
* **Layout audit:**
  * **Scope:** 21 states (all KID states and tabs, offline, ad-libs, drawer, scenes, the 4 SHAPE areas, CHARACTER, SPACE) × 3 scales, each checked as shown and with every panel scrolled to its end.
  * **Result: 126 of 126 clean:**
    * no text under 12.5 design px (10 px at 80 %);
    * nothing outside the frame;
    * no clipping;
    * no overlapping controls.
* **The Booth fits without page scrolling** at 80, 100 and 125 %, in both the ready state (longest command bar) and the take state.
* **Bugs found and fixed while building:**
  * proposals moving in the wrong direction (§2.4);
  * truncated state text and the SIMULATED badge in the command bar;
  * smooth scrolling that made captures cut off the newest line;
  * a too-short Take panel.

## 4. Open questions for you

1. **Audition (HEAR KID):**
   * (a) build the processor-side override (DSP-adjacent);
   * (b) drop it and rely on Apply + Undo;
   * (c) defer it.
2. **CLEAN and AIR LAB in KID.** KID may switch CLEAN on and raise AIR LAB when asked ("clean it up", "airy"). Build My Mix never touches them. Keep this?
3. **Formant and Harmony are not used by KID** (only manual). Keep it that way?
4. **Remembered preferences:** should "keep me upfront" persist between sessions (constraints only, no text or audio)?
5. **Speech platform risk:** approve spikes S1 and S2 (AI_MVP_ROADMAP) before any speech work?
6. **Earlier request: three visual options.** You asked for three different directions before the KID brief arrived. KID's Booth is one strong direction. Say if you still want two alternatives drawn.

## 5. Limitations

* No audio, microphone, speech recognition or AI model. Everything KID "understands" comes from the rule tables in the page.
* Proposed values are design starting points and have not been listening-tested.
* The vocal read numbers are illustrative.
* Cost and latency figures are estimates (PRIVACY_AND_COSTS.md).
* The VocalDose website couldn't be opened from this session (outside the allowed network).
* Not tested: FL Studio, a real plug-in window, macOS or Windows browsers, screen readers, touch.

## 6. Screenshots

`screenshots/`: 20 states × 3 scales (80 / 100 / 125 %) = 60 files, named `<080|100|125>_<nn>_<state>.png`.

| nn | State |
|---|---|
| 01 | Booth, ready |
| 02 | Push-to-talk, recording |
| 03 | Processing the request |
| 04 | Analysing the vocal |
| 05 | Preparing changes |
| 06 | Take: "Make my vocal wider and more atmospheric" |
| 07 | Follow-up: "Too much. Make it subtle" |
| 08 | Follow-up: "Keep the vocal upfront" |
| 09 | HEAR · KID |
| 10 | "Make it darker" → "A little less" → "Keep the presence" |
| 11 | "I don't know what it needs. Improve it" |
| 12 | VOCAL READ |
| 13 | LIMITS |
| 14 | Speech unavailable |
| 15 | "Make my ad-libs sound distant" |
| 16 | THE PATH stage drawer (Dimension) with KID's glow |
| 17 | SHAPE · TONE (manual) |
| 18 | CHARACTER (manual) |
| 19 | SPACE (manual) |
| 20 | Scene library |

**To try it yourself:**
1. Open `index.html` and set the browser zoom to 100 % (Cmd/Ctrl + 0).
2. Click a suggestion, or type and press SEND, or hold the mic.
3. In the toolbar, use **Speech: Available / Unavailable** to see the error state, and **Scale** for 80 / 100 / 125 %.
