# KID — architecture (proposal)

**Labels:**
* **FACT (repo):** checked in the repository at `dba3bd4`.
* **FACT (external):** a well-known property of a third-party project. Verify it again during the spike.
* **ESTIMATE:** not measured.

Nothing here is implemented.

## 1. Principle

KID is a **proposal generator outside the audio path.** It reads text (from typing or from speech) and analysis numbers, and outputs a validated list of parameter changes. The artist approves the list, and the plug-in's existing parameter system applies it. The DSP doesn't know KID exists.

```
 COMMAND MIC (separate input)          GLITCHKID AUDIO INPUT (the vocal)
        │ push-to-talk only                     │
        ▼                                       ▼
 [A] capture (message thread / worker)   audio thread: existing DSP, unchanged
        ▼                                       │  writes display taps and analyser fields (FACT, repo)
 [B] speech-to-text (worker thread, local)      │
        ▼                                       ▼
   text ◄──── typed text / suggestion chips   [F] analysis reader (message thread, read-only)
        ▼                                       │
 [C] intent interpreter ──► IntentFrame         │
        ▼                                       │
 [D] director (memory: direction, constraints, role) ◄┘
        ▼
 [E] validator (allowlist schema, ranges, types, dependencies) ──► Proposal
        ▼
 UI: KID's Take (inspect, untick, audition*, Cancel)
        ▼  explicit Apply
 [G] applier: GlitchKidProcessor::applyParameterChanges (gestures, one Undo step)   (FACT, repo)
        ▼
 APVTS parameters ──► audio thread reads them as today
```

\* Audition is an open engineering item (§6).

## 2. Components

| | Component | Thread | Input → output | Notes |
|---|---|---|---|---|
| A | Command capture | Worker (device callback) | Mic samples → mono buffer (≤ 10 s) | Only while push-to-talk is held. A separate device from the host's audio. See §5 for the platform risks. |
| B | Speech-to-text | Worker, low priority | Buffer → text | Local recogniser (recommended: whisper.cpp, §8). The buffer is discarded after transcription. |
| C | Intent interpreter | Message thread | Text → `IntentFrame` | Deterministic word and phrase grammar (the prototype's `parse`). An optional cloud model may produce the same `IntentFrame`, never parameters. |
| D | Director | Message thread | `IntentFrame` + memory + current `SoundState` + analysis → candidate changes | The prototype's `updateDir` + `decide`. Pure function, unit-tested. |
| E | Validator | Message thread | Candidate changes → `Proposal` | The allowlist (57 IDs) and parameter ranges come from the processor's own `ParameterLayout`. It enforces type and choice range, drops no-ops, and logs refusals. |
| F | Analysis reader | Message thread | `VocalAnalyzer::fetch()`, `getCleanApplied()`, `getTuneDetectedMidi()` (FACT, repo) → `AnalysisSnapshot` | Read-only. It reuses the analyser that Build My Mix already uses. |
| G | Applier | Message thread | The ticked rows of the `Proposal` → `SoundState` → `applyParameterChanges` | Existing function: `beginChangeGesture` / `setValueNotifyingHost` / `endChangeGesture` per parameter (FACT, repo). The editor's `undo` / `backToMine()` gives one-step Undo (FACT, repo). |

**What no KID component does:**
* run on the audio thread;
* allocate there;
* hold a lock the audio thread waits on;
* write `apvts` state or any processor member directly.

## 3. Data contracts

**IntentFrame** (the only thing a cloud model would ever be allowed to produce):

```json
{
  "ops":    { "width": 1.0, "depth": 1.0 },
  "scale":  0.0,
  "more":   false,
  "locks":  ["front", "presence"],
  "flags":  ["mud", "ess", "air", "glitch", "natural"],
  "role":   "lead | adlib | null",
  "improve": false,
  "reset":  false,
  "notes":  ["Level stays yours: …"],
  "heard":  ["wider", "keep the vocal upfront"]
}
```

* Dimensions are fixed: `bright, warm, presence, punch, width, depth, clean, exp`.
* Values are limited to −2…+2.
* Unknown keys are rejected.

**Director memory** (per editor session): `{ base SoundState, direction{8}, locks{}, flags{}, role, lastTouched[], unticked{}, manual{} }`.

**Proposal row:** `{ id, from, to, reason, stage, tag: NEW | UP | DOWN | YOU CHANGED, selected }`. `to` has already passed validation.

## 4. Safety rules (enforced in code, tested)

1. **Allowlist.** KID may propose 57 of 69 parameters. It never proposes `tune_*` (5), `gain`, `mix`, `lim_*` (4) or `bypass`. This list is generated from the layout, not typed by hand.
2. **Range and type.** Each value is clamped to the parameter's own range, and choices and switches are rounded. Non-finite values are refused (as `applyParameterChanges` already does).
3. **Direction-safe moves.** Continuous settings move towards a musical goal only in the asked direction ("wider" can never narrow). Tested in the prototype suite.
4. **Dependencies.** For example: a Doubler amount is only meaningful with `dbl_on`, so the director proposes both. Echo division only applies while `echo_sync` is on. Formant has no switch: 0 is off.
5. **Explicit Apply only.** The applier is reachable only from the Apply button. It writes the ticked rows, with host gestures, as one Undo step.
6. **Manual override wins.** If the artist changes a proposed parameter by hand, that row is unticked and the proposal is rebuilt from the new sound.
7. **Bounded memory.** The conversation is not saved by default, and command audio is never saved.
8. **Failure is silent for the sound.** Any exception, timeout or refusal leaves the parameters untouched and shows one line.

## 5. Capturing the command microphone inside a plug-in (risks)

This is the hardest part. It must be proven by a spike before anything else.

* **macOS (risk):** microphone permission (TCC) is granted to the *host application*, not to a plug-in. FL Studio records audio, so it normally asks for microphone access. A plug-in opening a second Core Audio input inside the host's process should inherit it. **Not verified.**
  * Apple's Speech framework (`SFSpeechRecognizer`) needs usage descriptions in the *app's* Info.plist, which a plug-in can't add. So it probably can't be used inside a DAW. **To verify.**
* **Windows (risk):** many audio interfaces run ASIO drivers that one client holds exclusively. The command mic should therefore be a different device (for example the laptop's built-in mic via WASAPI shared mode). Opening it from inside the host process must not disturb the host's driver. **To verify.**
* **Fallbacks if the spike fails:**
  * typing and suggestion chips (always available);
  * a small companion app that captures speech and sends text to the plug-in over a local socket;
  * the host's own sidechain input. **Rejected:** it would route speech into the plug-in's audio input, which the brief forbids.

## 6. Audition ("HEAR KID")

Writing real parameters to audition would notify the host (automation, "project modified", host undo). A clean audition needs a **processor-side temporary override**:

* the message thread posts a small, fixed-size set of values to a lock-free slot;
* the audio thread reads it *instead of* the parameter values while the slot is armed, using the existing smoothers;
* releasing HEAR disarms the slot.

This is DSP-adjacent work and needs its own design review. Until then the honest alternative is **Apply + Undo**, which is instant. This is a decision for you (DESIGN_REVIEW §4).

## 7. State, automation and undo

* **Parameters:** unchanged. Automation and saved sessions see normal parameter changes after Apply (FACT, repo: `applyParameterChanges` uses gestures).
* **Undo:** the editor's existing one-level `undo` and `backToMine()`. A multi-step KID history would be new work and is not part of the MVP.
* **Plug-in state:** KID adds nothing by default. Optional: save the kept constraints (no text, no audio) in the editor state next to `uiLevel` / `uiCategory`.
* **Presets:** KID never writes presets. The artist can save the result with the existing preset save.

## 8. Local or cloud: comparison

| | Local deterministic engine | Local speech recogniser (whisper.cpp) | Cloud speech recognition | Cloud language model (intent only) |
|---|---|---|---|---|
| What it does | Words → IntentFrame → proposal | Command audio → text | Command audio → text | Free text → IntentFrame |
| Running cost to GlitchKid | 0 | 0 (the user's CPU) | Per minute of audio (ESTIMATE in PRIVACY_AND_COSTS) | Per request (ESTIMATE) |
| Privacy | Nothing leaves the machine | Nothing leaves the machine | Command audio leaves the machine | Command text leaves the machine |
| Internet | Not needed | Not needed after the model download | Required | Required |
| Latency (ESTIMATE) | < 10 ms | Roughly 0.3–2 s for a 5 s command, depending on model and CPU (to be measured) | Roughly 0.5–2 s plus the network | Roughly 0.5–3 s plus the network |
| Capability | Fixed vocabulary; handles combinations, conflicts and follow-ups by rule | Good for short English commands (tiny/base models); accents and noise vary | Usually stronger | Understands open phrasing; can be wrong, so its output is validated |
| Offline fallback | It *is* the fallback | Typing | Typing + local engine | Local engine |
| Mac and Windows | Plain C++ | C/C++, MIT licence, runs on macOS (Apple Silicon) and Windows (FACT, external) | Any | Any |
| Binary or download size | Small | Model files roughly 75 MB (tiny) to 142 MB (base) (FACT, external, whisper.cpp README; verify) | None | None |
| Memory | Small | Roughly 270–390 MB while running tiny/base (FACT, external, whisper.cpp README; verify) | Small | Small |

## 9. Recommended MVP architecture

* **Engine:** local deterministic director (C, D, E, F, G) plus typed text and suggestion chips. It is shippable without speech, without a server and without per-use cost, and it is fully unit-testable.
* **Phase 2:** local push-to-talk speech (A, B) with whisper.cpp, downloaded only when the user opts in, after the capture spike passes on Mac and Windows.
* **Optional later:** a cloud language layer that only outputs `IntentFrame`, behind an explicit opt-in. The local validator still decides everything.
* **Timeline and effort:** see AI_MVP_ROADMAP.md.
