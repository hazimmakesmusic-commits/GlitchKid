# KID — product spec (v2 prototype)

**KID — your vocal director.** *You speak. It builds.*

## 1. What KID is, honestly

| Part | What it is today | Status |
|---|---|---|
| Listening for a command | A push-to-talk **command microphone**: the system's default input, opened only while the button is held, separate from the vocal track (`Kid/KidSpeech.h`, `CommandMic`) | Exists |
| Turning speech into text | **whisper.cpp** with the `ggml-base.en` model, run on the user's computer on a background thread. The model is an opt-in, one-time download checked by SHA-1 | Exists |
| Understanding the words | **A transparent rule engine** (`Kid/KidDirector.cpp`, `kid::parse`): words → intent → a musical direction with memory. Not a large language model; no network | Exists |
| Deciding the changes | `kid::Director`: direction + (optional) analysis → changes to GlitchKid's own parameters, each with a plain reason. Validated against each parameter's real range and an allowlist | Exists |
| Review before apply | The engine supports per-row selection and keeping hand-moved controls (`setSelected`, `noteManualEdit`, `selectedChanges`). **Today's KID screen applies immediately, with Undo.** The v2 design changes that | Engine exists; UI is new |
| Audition | Hearing a proposal before applying it | **New engineering** (`AI_ARCHITECTURE.md` §3) |

**Claims guidance.**
- **"AI" is accurate for the speech recognition only.** KID's musical decisions come from rules that can be read and tested.
- **The UI says** "your vocal director".
- **Marketing should not say:**
  - "AI mixes your vocal";
  - "industry-level results";
  - "one-click professional vocals".
- **Whether to say "AI vocal director" at all is an open question** for the product owner.

## 2. Jobs KID does

1. **"Make it…"**: a direction, e.g. *darker and more aggressive*.
2. **"…but keep…"**: constraints: *keep it upfront*, *keep the character*, *keep the warmth*, *keep the clean sound* (the last is new).
3. **"Less / more / too much…"**: refine the same proposal, never pile on a new one.
4. **"For my ad-libs…"**: a role that changes how far back and how long the echo throws are.
5. **"I don't know what it needs"**: proposes from measurements of the vocal. It needs audio; without audio KID says so.
6. **"Build my mix"**: hands over to Build My Mix, which uses the same review.
7. **Questions** ("why…?", "what changed?"): answered from the real rows. These exist in the engine as `Ask` and are not shown in this prototype.

## 3. Interaction states

| State | Entry | What the artist sees | Exits |
|---|---|---|---|
| Ready | Open KID | The invitation, an example, and the privacy line | Hold to talk, type, or a chip |
| Recording | Mic held (mouse, touch, Space/Enter on the mic, or **K** anywhere) | Pulsing pink mic, command-mic level, *Listening… let go to send* | Release (a tap under 300 ms → *Hold the button while you speak*) |
| Transcribing | Release | *Transcribing on this computer…* | Automatic |
| Heard you | Text ready | The transcript, marked *transcribed on this computer* | Automatic |
| Interpreting | — | *Reading your words…* | Automatic |
| Preparing | — | *Building a proposal…* | Automatic |
| Proposal ready | — | Interpretation, *kept as yours* list, the proposal (revision n) | Apply, Cancel, ×, follow-up, audition |
| Auditioning | Listen › Proposal | *Auditioning the proposal*; Booth: *PROPOSAL, NOT APPLIED* | Current / Apply / Cancel; × always returns to Current |
| Applied | Apply | *Applied n changes*, **Undo**, **Done** | A new command / Done |
| Didn't catch that | No sound words | Examples; nothing changed | A new command |
| Unavailable | Speech not set up · mic blocked · Mastering mode · not processing | The reason and the fix; typing offered where it works | The cause is fixed |

**Offline is not an error state.** Everything runs locally. Only the one-time model download needs internet, and the set-up dialog says so.

## 4. The proposal contract

Every proposal, whether from KID or Build My Mix, follows these rules:
1. **One row per parameter**, grouped by processing stage in chain order, labelled with its workspace.
2. **Each row shows** the *current* (live) value → the *proposed* value with units, and a one-line reason in plain words.
3. **Each row has its own tick.** All / None are shortcuts; there are no group ticks.
4. **Opening, closing (×) or cancelling never changes the sound.** × keeps the proposal for later; Cancel discards it.
5. **Apply applies only the ticked rows,** through the plug-in's normal parameter path (host gestures, automation, saved state), as one undo step.
6. **If the artist moves a proposed control by hand,** that row is unticked, disabled and tagged *YOUR VALUE IS KEPT*. KID says so in the conversation.
7. **Follow-ups reshape the same proposal** (revision n + 1) relative to the sound the proposal started from. Changed or new rows are tagged; removed rows are named.
8. **KID never proposes** Gain, Mix, the Output Limiter or Bypass. Pitch Tune's key and scale come only from a reliable estimate. The UI states *kept as yours*.
9. **Values are always inside the parameter's real range.** A row at a range limit is tagged *AT ITS LIMIT*.
10. **Anything that depends on measuring audio is labelled** (in the prototype, *SIMULATED MEASUREMENT*).

## 5. Vocabulary

| Kind | Exists in `KidDirector` | Added by this design (new) |
|---|---|---|
| Directions | brighter / darker, warmer, clear / upfront / forward, aggressive / punchy, softer, wider / narrower, distant, space / ambient, closer / intimate, clean it up, experimental / weird / robotic… | **psychedelic / trippy / dreamy**; **more character / add character** |
| Amount | *a little / slightly / a bit* (half); *very / much* (1.5×) | — |
| Refinement | *a little less* (70 %), *too much / less* (50 %), *subtle* (40 %) | — |
| Constraints | keep it upfront, keep the presence, keep the character, keep the warmth | **keep the clean sound** |
| Role | ad-libs / backing | — |
| Whole jobs | improve it / help me / what it needs, build my mix, tuning words, producer profiles, artist references (direction only, never a copy) | — |

The exact mappings are in `INTENT_PARAMETER_MAP.md`.

## 6. Failure and edge cases
- **Speech model missing or corrupt.** Offer set-up; verify SHA-1 before loading (exists).
- **Mic permission denied.** On macOS the permission belongs to the host app (FL Studio). Show where to change it; typing works.
- **Ambiguous or empty transcript.** *Didn't catch that*, with examples. Never guess a change.
- **Conflicting words** ("darker but brighter"). The rule engine sums directions, so they can cancel out. The design shows KID's interpretation line ("Heard: …"), so the artist sees what was understood. `IntentFrame::notes` already carries honest notes such as "there is no reverb"; whether it flags contradictions is **not verified**.
- **Proposal waiting while the artist loads a Scene or recalls A/B.** The proposal's "current" values are always read live. Rows whose current value already equals the proposed one should drop out. Implement and test this in phase 2.
- **Host automation moving a proposed parameter.** Treat it as a hand edit: unticked, value kept.
- **Mastering mode on.** KID is unavailable, with the reason.

## 7. Success measures (to be defined with the product owner; nothing is collected today)
- Time from first open to the first saved sound.
- Share of proposals applied, partially applied (some rows unticked) or cancelled.
- Undo within 30 s of Apply. A high rate means proposals miss.
- Follow-ups per proposal. Too many means the first proposal is far off.
- Commands not understood, as a list of missing words for the vocabulary.

Any telemetry needs explicit opt-in. See `PRIVACY_AND_COSTS.md`.

## 8. Out of scope for this phase
- Implementing any of the above in the plug-in.
- Continuous or always-on listening (never; push-to-talk only).
- Cloud models.
- Voice cloning, artist imitation, or matching a copyrighted reference beyond tonal guidance (Reference Match already limits itself to that).
