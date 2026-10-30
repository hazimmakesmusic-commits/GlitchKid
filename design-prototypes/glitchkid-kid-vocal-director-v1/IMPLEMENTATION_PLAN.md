# KID — implementation plan (mapping to existing production modules)

**Production implementation is not approved.** This updates the Vocal Playground plan (`../glitchkid-vocal-playground-v1/IMPLEMENTATION_PLAN.md`) for a KID-centred Booth. Phases and effort are in AI_MVP_ROADMAP.md, and the AI design is in AI_ARCHITECTURE.md.

## 1. What must not change

* **Parameters:** the 69 parameter IDs, ranges, defaults and versions (`createLayout`).
* **Audio:** DSP, the 17-stage order, latency reporting and the dry path.
* **Presets and state:** factory profiles, the preset format, A/B slots and the saved editor state.
  * KID adds nothing to saved state by default.
* **Build My Mix:** `automode::buildMix` and its rules. KID is a separate feature.
* **Tests:** the existing tests are not weakened.

## 2. Mapping: KID concept → existing code → new code

| Concept part | Existing code it relies on (FACT, repo at `dba3bd4`) | New code |
|---|---|---|
| Applying KID's take | `GlitchKidProcessor::applyParameterChanges (const SoundState&)`: per-parameter `beginChangeGesture` / `setValueNotifyingHost` / `endChangeGesture`; skips non-finite and unchanged values | Build a `SoundState` from the ticked rows; call the existing function |
| Undo | The editor's `undo` struct and `backToMine()` (used today by Build My Mix, profiles, Beat Lock and Reference) | Capture the undo snapshot before Apply, like Build My Mix does |
| Allowlist and ranges | `apvts` parameter layout (`RangedAudioParameter` ranges) | Generate the allowlist and ranges at start-up from the layout, minus the never-list; unit-test it |
| Vocal read | `VocalAnalyzer` (`start`, `stop`, `fetch`, `completedCount`; fields: peak, RMS, crest, clipped, dynamic range, 5 bands, sibilance); `getCleanApplied()`; `getTuneDetectedMidi()` / `getTuneTargetMidi()` | A read-only `AnalysisSnapshot` for the director; the VOCAL READ tab |
| Director (intent → direction → decisions) | — | **New pure C++ library** (`Source/Kid/`): lexicon, `IntentFrame`, direction memory, rules, validator. No JUCE GUI dependency, so it can be tested in `Tests/`. |
| KID panel (session, Take, tabs, HEAR, Apply) | The editor shell's layout and styling (`Source/UI/*`, `PluginEditor`) | New components; the session list is an accessible list; the Take rows reuse the per-change review idea from Build My Mix |
| THE PATH glow | The rail and chain logic in the shell | Highlight set = the stages of the proposal rows |
| Voiceprint and meters | `getInputTap()`, `getOutputTap()` (`DisplayTap`), `consumeInputPeak()`, `consumeOutputPeak()` | The voiceprint component (see the Vocal Playground plan); "preview" mode draws the proposal |
| Push-to-talk capture | — | **New:** command-device selection and capture on a worker thread. **Blocked on spike S1.** |
| Speech-to-text | — | **New:** whisper.cpp wrapper on a worker thread; model manager (opt-in download, checksum). **Blocked on spike S2.** |
| HEAR · KID (audition) | — | **New and DSP-adjacent:** a processor-side temporary override. **Needs your decision and a design review.** |
| Manual controls | Every existing control | The SHAPE / CHARACTER / SPACE layouts from the Vocal Playground plan |
| Optional cloud | — | Backend proxy plus client; outputs `IntentFrame` only (Phase 4) |

## 3. Threading rules

* KID code runs on the **message thread**, except capture and speech recognition, which use **worker threads**.
* Nothing from KID runs on the audio thread. Nothing allocates there or takes locks the audio thread waits on.
* Analysis uses the analyser's existing atomics and `fetch()` (already lock-free and audio-thread safe; FACT, repo).

## 4. Tests to add (in `Tests/`)

* **Director unit tests:**
  * every lexicon entry;
  * the follow-up sequences (wider → subtle → upfront; darker → a little less → keep presence);
  * conflicts ("wide but upfront");
  * roles;
  * natural;
  * "improve it" from fixed `VocalAnalysis` values;
  * unknown words.
* **Validator tests:**
  * the allowlist equals the layout minus the never-list;
  * every proposed value lies within its range;
  * forbidden IDs are refused;
  * no NaN/Inf.
* **Applier test:** apply → the parameters equal the ticked rows; unticked rows are unchanged; Undo restores the exact state; host gestures are begun and ended (mock listener).
* **UI test:** no parameter changes before Apply (mirrors the prototype's 51-check suite).
* **Speech (Phase 2):** a capture-thread test with a fake device; a recogniser wrapper test with a fixed WAV; failure paths.

## 5. Size and layout

The concept is drawn at 1120 × 680 (80 / 100 / 125 %). The editor today is 1128 × 600 (scaled 70–150 %; FACT, repo). A KID Booth needs the extra height for the session script and the Take panel, so changing the editor size is part of this plan. It needs FL Studio validation on macOS and Windows.

## 6. Order of work

Follow AI_MVP_ROADMAP.md:
* Phase 0: decisions and spikes;
* Phase 1: typed KID (the MVP);
* Phase 2: push-to-talk;
* Phase 3: audition (optional);
* Phase 4: cloud (optional).

Each phase ends with the full suite green on Linux and Mac.
