# THE VOCAL PLAYGROUND — implementation plan (mapping to existing production modules)

**This is a plan for discussion. Production implementation is not approved.** It maps every part of the concept to code that exists at `dba3bd4`, names the new code each part would need, and lists what must not change.

## 1. What must not change

* **The 69 parameters:** IDs, ranges, defaults and versions (`createLayout`). Saved sessions and automation depend on them.
* **DSP and the processing order** (the 17 stages), latency reporting, and the dry path.
* **Presets and state:** `FactoryPresets` / `Profiles.inc`, the user preset format (`PresetStore`), the A/B slots and the saved editor state.
  * The concept adds **no** factory preset. Scenes are a presentation layer over the existing 8 profiles.
* **The Build My Mix engine** (`automode::buildMix`) and its rules.
* **The tests:** existing tests stay and are never weakened. New behaviour gets new tests.

## 2. Mapping: concept → existing code

| Concept part | Existing production code it uses | New work needed |
|---|---|---|
| Header: DRY / PROCESSED | `bypass` parameter (20 ms crossfade) | Styling only. |
| Header: Undo | Editor `undo` / `backToMine()` (one level) | Wire MAKE IT apply and scene load into the same Undo. |
| Header: scene pill, "● modified" | `PresetController` (`loadFactory`, names), `isPresetModified()` | A scene-metadata table (name, art, mood) next to `FactoryPresets`, display only, plus the artwork. |
| THE PATH (17 stages, lit state) | The current shell's rail and chain logic (`Source/UI/*`, `PluginEditor`), and the parameter values | New strip component: 17 stations with world colours and a stage drawer. The lit-state rules come from the parameters (same as the prototype's `STAGES[].on`). |
| THE VOICE (voiceprint) | `getInputTap()`, `getOutputTap()` (`DisplayTap`: 8192 samples, about 170 ms), `consumeInputPeak()`, `consumeOutputPeak()`; parameter values for the layers | **New component.** A timer (about 30 Hz) on the message thread reads the taps and builds an envelope history. It draws the mirrored outline plus layers from the parameters, and falls back to "No signal" when `writeCount()` stops moving. Needs a CPU budget and a test, like the existing `UiPerfTests`. |
| IN / OUT meters, compressor and limiter GR | `consumeInputPeak`, `consumeOutputPeak`, `consumeGainReductionDb`, `consumeLimiterGainReductionDb` | Restyle the existing meters. |
| FEEL Tone / Punch / Space | `MacroEngine` | Restyle only. |
| SHAPE · CLEAN | `clean_*` parameters, `getCleanApplied()` telemetry | Layout: one switch, two path positions (#7 and #9). |
| SHAPE · TUNE | `tune_*` parameters, Natural/Light/Hard style mapping (`MacroEngine.h`), `getTuneDetectedMidi` / `getTuneTargetMidi` | Key keyboard, scale segments, read-out. |
| SHAPE · TONE | `eq_*`, `sat_drive`, `air_*`; the existing EQ graph (`EqGraph::responseDb`) and spectrum (`DisplaySpectrum`) | Draggable band nodes on the existing graph, the per-band layout, and a Warmth/AIR LAB card. Keep the existing AIR LAB link behaviour (`linkAir`). |
| SHAPE · DYNAMICS | `comp_*`, compressor GR | A transfer curve drawn with **the compressor's own gain computer** (not the prototype's generic formula), plus a GR bar. |
| CHARACTER (Formant, Harmony, Mutation, Stutter) | `formant_shift`, `harm_*`, `mut_*`, `stutter_*` | Specimen cards, interval ladder, XY pad (writes two parameters; needs keyboard access and the host's gesture begin/end around drags), slice selector. |
| SPACE + FINISH | `dbl_*`, `echo_*`, `dim_*`, `gain`, `mix`, `lim_*` | A ROOM view drawn from the parameters (no audio), cards, and the FINISH strip. |
| Scene library | `FactoryPresets::info`, `PresetController::loadFactory` (keeps the 22 user-owned parameters) | Grid, artwork assets, a PLACEHOLDER flag until the sounds are designed and listening-tested, and the audition switch (see §4). |
| MAKE IT | Nothing exists. Closest relatives are `automode::buildMix` (pure proposal plus review) and the Build My Mix review UI rules | **New pure engine** (§3) and a review sheet that reuses the Build My Mix per-change review. |
| Hold to preview | Nothing exists | **Unresolved** (§4). |
| Latency read-out | `getLatencySamples()` / `getWantedLatencySamples()` | Display only. |
| Editor size | Today 1128 × 600, scaled 70–150 % (`PluginEditor.h`) | Move to 1120 × 680 as agreed earlier, keeping a 80 % minimum (the concept's smallest text is 12.5 design px, which is 10 px at 80 %). Needs FL Studio validation on Mac and Windows. |

## 3. MAKE IT engine (proposal)

* **Shape:** `automode::interpretIntent (const juce::String& words, const SoundState& current, float intensity) -> IntentProposal`. It is pure and deterministic, with no allocation on the audio thread (it never runs there).
* **Vocabulary:** a fixed table of intents. Each intent has its words and a list of `{parameter id, op (set / add / at least), value, reason}`.
  * The prototype's table (14 intents, 32 parameters) is a starting point for review. It isn't listening-tested.
* **Rules, enforced in the engine and covered by tests:**
  * It never proposes `tune_*`, `gain`, `mix`, `air_*`, `lim_*` or `bypass`.
  * Whether `clean_*` is allowed is your decision (DESIGN_REVIEW §4).
  * Opposite intents: the last one said wins, and a note says so.
  * "Keep the character" excludes the Lab parameters.
  * Unknown words come back as "not used".
  * Values are clamped to each parameter's range.
  * "Surprise me" is a seeded, repeatable pick.
* **Apply:** set only the ticked changes, inside one host gesture per parameter, and record one Undo step. This is the same path Build My Mix uses today.
* **Tests:**
  * table validity (every ID exists; every value lies within its range);
  * the forbidden list;
  * opposite handling;
  * a "nothing changes before Apply" UI test, mirroring the prototype's test.
* **Not part of this:** speech input, AI or language models. Nothing in the repository supports them, and the concept does not need them.

## 4. Unresolved engineering questions

1. **Hold to preview.** Real parameter changes are seen by the host as automation and edits.
   * A safe preview needs a processor-side temporary override (a set of values the audio thread uses instead of the parameter tree while held), with smoothing and a guaranteed release.
   * That is DSP-adjacent work and needs its own design review, or the feature can be dropped (Undo already gives instant comparison).
2. **Scene audition** (hearing a scene on your vocal before loading) needs the same mechanism, so it shares the decision above.
3. **Text input in hosts.** FL Studio and other hosts may route keystrokes to the host. Test `TextEditor` focus in FL Studio (Mac and Windows) before relying on typing. The chips work without it.
4. **Voiceprint cost.** Rendering a filled path, three inner contours and up to six echo rings at 30 Hz must stay within the existing UI performance budget. Measure it with the existing perf tests and offer a "reduce motion" setting (it exists in the shell as MOTION).
5. **Fonts.** The plug-in bundles Inter Regular and Bold. The concept also uses light, ultralight, heavy italic and a monospace face. Either bundle these (check their licences) or redesign the world type voices with the two weights already bundled.

## 5. Suggested phases (each phase ends green, with new tests)

| Phase | Content | Done when |
|---|---|---|
| 0 | Decisions: preview yes/no, CLEAN in MAKE IT, editor size, fonts | Answers recorded in `docs/DECISIONS.md` |
| 1 | New editor frame at 1120 × 680, header (DRY / PROCESSED, Undo, scene pill), world navigation | The full existing suite passes on Linux and Mac; navigation tests updated (not weakened) |
| 2 | THE PATH with stage drawers | Test: 17 stations in the real order; the drawer edits the right parameters |
| 3 | SHAPE, CHARACTER and SPACE views | Control-inventory test: every one of the 68 sound parameters is reachable; the Lab parameters appear only in CHARACTER |
| 4 | THE VOICE component | Perf test within budget; shows "No signal" with no input; reduced motion respected |
| 5 | MAKE IT engine and review sheet | Engine tests (§3) and the UI test that nothing changes before Apply |
| 6 | Scene library over the existing profiles | Load test: defaults plus overrides, the 22 parameters kept; PLACEHOLDER shown until the sounds are signed off |
| 7 | FL Studio validation on macOS and on Windows at 80 / 100 / 125 % | Your checklist signed off |

## 6. Risks

* **The scope is a full editor rewrite.** The current signal-rail shell would be replaced. Plan it as a branch with the existing tests as the safety net.
* **Visual polish that depends on gradients and blur** (scene art, haze) must be drawn efficiently in JUCE or delivered as pre-rendered images.
* **MAKE IT wording sets expectations.** Results must be listening-tested so the words mean what they say.
