# KID — product specification (concept)

Status: specification for discussion. Nothing here is implemented in the plug-in. "Prototype" refers to `index.html` in this folder, which simulates KID with rule tables.

## 1. Who and why

* **Artist (primary):** records their own vocals and knows how the vocal should *feel* but not which knob does it. They want results in their own words, quickly, without losing control.
* **Producer (secondary):** knows the controls. Uses KID as a fast first pass, then fine-tunes by hand. Needs full transparency and manual override.

**Job to be done:** "Turn what I hear in my head into GlitchKid settings, show me exactly what you changed, and let me say *no*."

## 2. Scope of KID

| KID does | KID does not |
|---|---|
| Interpret requests about the vocal's sound (tone, dynamics, space, cleanliness, character, role) | Answer general questions, write lyrics or chat about anything else |
| Propose changes to GlitchKid's existing parameters (allowlist: 57 of 69) | Touch Pitch Tune, Gain, Mix, the Output Limiter or Bypass |
| Remember the current conversation's direction, constraints and role | Remember across sessions by default (§7) |
| Read GlitchKid's existing analyser (VocalAnalyzer, CLEAN telemetry, the Tune read-out) | Claim measurements the engine can't make (stereo, pitch range) |
| Explain each change in one line | Change anything without an explicit Apply |
| Decline unsupported requests honestly | Invent processors (for example "reverb") or parameters |

## 3. Commands KID must handle (MVP)

The design examples from the brief, as the prototype handles them:

| Artist says | KID's interpretation (direction) | Result in the prototype |
|---|---|---|
| "Make my vocal wider and more atmospheric." | WIDER, MORE SPACE | Doubler width and amount up, Dimension width and mix up, Echo mix, feedback and darker tone, Presence eased slightly |
| "Clean up the muddy frequencies." | CLEANER, plus mud | Analyses first, then CLEAN on/amount, corrective EQ at full strength, body band moved to 300 Hz and cut |
| "Make it darker and more aggressive." | DARKER, HARDER | Air shelf and Presence down; compressor firmer and faster; threshold down; more saturation |
| "Give me a natural melodic rap vocal." | Natural flag, plus light space, width, brightness, punch, presence | Moderate changes; saturation capped at 2 dB; echo capped at 12 %; Lab effects off |
| "Make my ad-libs sound distant." | Role AD-LIBS; MORE SPACE, FURTHER BACK | 1/4 echo throws, more echo, darker repeats, thinner low end, softer top |
| "I don't know what it needs. Improve it." | Analysis-led | Analyses, then cleaner, steadier and slightly brighter, with reasons taken from the analysis |
| "Too much. Make it subtle." | Scales the last direction to 40 % | The same rows, values closer to the current sound, tagged ▼ ADJUSTED |
| "Keep the vocal upfront." | Adds the constraint "you in front" | Echo ≤ 12 %, doubles ≤ 28 %, Dimension mix ≤ 30 %, Presence never lowered |
| "A little less." | Scales the last direction to 70 % | The same rows, smaller |
| "Keep the presence." | Adds the constraint "presence" | Presence changes drop out; "No longer changing: Presence" |
| "Start over." | Clears direction, constraints and role | Empty take |
| "Make it louder" / "autotune it" / "add reverb" | Declined (level and tuning are yours); reverb mapped to Echo and Dimension, with a note | No change, or the Echo/Dimension version |

## 4. States and behaviour

The seven states (ready, recording, processing, analysing, preparing, ready to audition, error) are described in DESIGN_CONCEPT §4. The rules:

* **Push-to-talk only.** Hold, or tap to start and tap again to stop. Escape cancels and nothing is sent. No wake word and no background listening.
* **Analysis runs only when needed:** "improve", or any cleaning request, and only while the host plays audio. The analyser reports "too short" or "too quiet" honestly. KID never invents numbers.
* **KID never changes the sound before Apply.** Ticking or unticking rows, audition and follow-ups change only the proposal.
* **Apply** writes the ticked rows in one step. **Undo** reverses that step exactly. **Cancel** drops the proposal.
* **Manual override:** if the artist moves a proposed parameter by hand, that row is unticked and tagged "YOU CHANGED", and the proposal is rebuilt from the new sound.
* **Kept constraints persist for the conversation** ("Kept in mind for your next request") until "start over".

## 5. KID's voice

* **Short, concrete and musical:** "Wider, more space." "You stay in front: the width comes from the doubles, echo stays low and Presence isn't touched."
* **It reports measurements as measurements** ("It gets muddy around 250 Hz"). In production these sentences must be built from real analyser fields only.
* **No flattery, no claims of taste it can't back up, no exclamation marks.**
* **Every reply ends with what happens next:** "8 changes ready. Hear it, then Apply, or tell me what to change."

## 6. Failure and recovery

| Failure | What the artist sees | What happens |
|---|---|---|
| No command mic, or no permission | KID CAN'T HEAR YOU; mic disabled | Typing and suggestions still work |
| Speech not understood | "I didn't hear a sound in that. Try …" | No proposal; Apply stays disabled |
| Cloud layer offline (later phase) | "Using offline KID" note | The local engine answers |
| Analysis too short or quiet | "I need a few seconds of you singing; press play." | No analysis-based rows |
| A value outside a range, or a forbidden parameter | Nothing (the validator clamps or refuses) | Logged in LIMITS ("refused by the validator") |
| The host is busy or the audio stops | Analysis pauses | Resumes or times out (the analyser already gives up after 30 s) |

## 7. Data and memory

* The conversation exists only while the editor is open.
* **The default saves no transcript in the project.** An optional "remember my preferences" could save the kept constraints only (for example "keep me upfront"), never audio or text. This is a decision for you.
* Command audio is deleted as soon as it has been transcribed. See PRIVACY_AND_COSTS.md.

## 8. Accessibility

* The mic works with Space or Enter (press to start, press again to stop).
* Typing is always available as an alternative to speaking.
* Every state has a text label as well as the waveform.
* The session script is an ARIA live region.
* All direction meters also carry a word label.

## 9. Success criteria (for a real build)

* **Correctness:** for a fixed test set of about 100 phrases, 100 % of proposals stay inside the allowlist and the ranges (automated). At least 90 % move the sound in the asked direction (automated, from the direction rules).
* **Usefulness:** in listening tests, artists keep (apply) at least 60 % of proposed changes without hand edits. This is a target to measure, not a promise.
* **Responsiveness:** typed request to proposal in under 300 ms (local engine). Spoken request to proposal in under about 2 s on a 2020+ laptop (to be measured in the speech spike).
* **Trust:** zero sound changes before Apply (automated UI test, already in the prototype suite).
