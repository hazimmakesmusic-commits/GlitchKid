# User journeys — GlitchKid Vocal Playground v2

Each step names the prototype screen and its screenshot in `screenshots/` (the `100_` prefix means 100 %; many also have `080_` and `125_` versions). It also names the check in `qa/verify-prototype.js` that drives the step. Every step below was performed by that script in headless Chromium, and every check passed (135 of 135; see `DESIGN_REVIEW.md`).

## Journey 1 — Raw vocal to a saved, recallable sound (the primary journey)

| # | The artist… | They see | Safety rule shown | Screenshot |
|---|---|---|---|---|
| 1 | Inserts GlitchKid on the vocal's Mixer insert and opens it for the first time | **Welcome**: one insert for the whole vocal; nothing changes until you choose. *Skip intro* is on every step. | — | `100_01_onboarding_welcome` |
| 2 | Presses play in FL Studio | **Let's hear your vocal**: a live IN bar. It says *Waiting for sound…* until sound arrives, then *✓ Vocal coming in*. With no sound it explains the Mixer-insert routing. | Honest empty state | `100_02_onboarding_signal_waiting` |
| 3 | Flips DRY / PROCESSED | **Hear the difference**: the real compare switch, the same one as in the top bar. It is level-matched. | DRY is always one click away | `100_03_onboarding_dry_processed` |
| 4 | Chooses a path | **Help me build my vocal** or **I know what I'm doing** | — | `100_04_onboarding_path` |
| 5 | Reads the last step and finishes | **Three workspaces, one director**, and how KID handles privacy: push-to-talk, a separate mic, local transcription, nothing uploaded, nothing applied without you | — | `100_05_onboarding_privacy` |
| 6 | Lands in the plug-in with **Build My Mix** open. Plays 10–20 s, then presses **Start listening** | Progress, plus a checklist of measured facts. These are labelled **SIMULATED MEASUREMENTS** here. | — | `100_06_build_listen`, `100_07_build_listened` |
| 7 | Picks a direction (8 factory profiles) and an intensity | Profile cards with their real taglines | — | `100_08_build_direction` |
| 8 | **Reviews** the proposal | The change list (31 for SMOOTH R&B from Factory Default, 2 of them "fitted"), grouped by processing stage. Each row shows current → proposed and a reason; the fitted rows are labelled simulated. | Opening the review changes nothing (checked) | `100_09_build_review_audition` |
| 9 | **Auditions** it: Listen › Proposal | The Booth says *HEARING · PROPOSAL, NOT APPLIED*. The chain strip lights what the proposal would turn on. | The sound is unchanged until Apply (checked) | `100_09_build_review_audition` |
| 10 | Unticks a change they don't want (Echo mix), then **Apply** | Apply *n* changes. The result is named **Built: SMOOTH R&B** and shows ● *Not saved*. | Only ticked changes are applied (checked); Undo restores everything (checked) | — |
| 11 | Refines by voice: holds **HOLD TO TALK**, says *“Make this vocal darker and more aggressive.”* | Listening → Transcribing on this computer → Heard you → Reading your words → Building a proposal → Proposal ready | Push-to-talk only; command mic separate from the vocal | `100_10_kid_recording` … `100_14_kid_proposal` |
| 12 | Follows up: *“A little less.”* | **Revision 2** of the *same* proposal at about 70 %, with changed rows marked | Follow-ups reshape, never pile up | `100_16_kid_followup_less` |
| 13 | Tweaks the TONE big move by hand while the proposal waits, then reopens KID | The Air shelf row is unticked and tagged **YOU MOVED THIS · YOUR VALUE IS KEPT** | A hand-moved control is never overwritten (checked) | `100_17_booth_proposal_waiting`, `100_18_kid_manual_edit_kept` |
| 14 | **Apply** → hears it → happy | *Applied 5 changes* with Undo and Done | Undo restores the exact pre-Apply sound (checked) | `100_19_kid_applied` |
| 15 | Compares with the original: DRY | The Booth visual becomes a single cyan line; the chip says *HEARING · DRY (ORIGINAL)* | — | — |
| 16 | Explores CHARACTER: turns on Mutation and opens **More** | Card lights up; `Stage 4 of 17` | Reset touches only that module (checked) | `100_38_character` |
| 17 | **Save** → names it *Night Drive Lead* | Name checks: empty, invalid characters, factory names. Then *Saved*, and ● Unsaved clears. | — | `100_41_save_name_error` |
| 18 | Changes it again → **Save** → *Update “Night Drive Lead”…* → **Replace** | A menu (Update / Save as new), then a confirmation | Overwriting asks first (checked) | `100_42_save_menu`, `100_43_save_update_confirm` |
| 19 | Closes the project and comes back later (*Close & reopen project* on the prototype page) | Same sound, same name, same A/B slots. The intro does not return. *Night Drive Lead* is in SCENES › Yours and recalls exactly. | Exact recall (checked) | `100_48_scenes_yours` |

## Journey 2 — “I know what I'm doing” (experienced producer)

1. At the onboarding path step, chooses **I know what I'm doing**, which lands on **SHAPE**.
2. Clicks chain chip **13 ECHO**, which opens **SPACE** with Echo's card. Switches it on; the chip lights with a dot.
3. Opens **TUNE**:
   - clicks **Hard**, which writes strength 100 % and speed 0 ms and switches Pitch Tune on;
   - opens **Advanced: strength and speed** to fine-tune.
   (`100_34_shape_tune`, `100_35_shape_tune_advanced`)
4. Opens **TONE › Advanced** to reveal the bell widths (Q). (`100_36_shape_tone_advanced`)
5. Every knob also supports:
   - drag, with Shift for fine control;
   - mouse wheel;
   - arrow keys and PageUp/PageDown;
   - Home and End;
   - Delete to reset;
   - double-click to reset.

   Each value is shown with its unit, and the help line gives the range and default.
6. Never needs KID. The proposal flows are optional.

## Journey 3 — Talking it through with KID (conversation)

These sequences come from the prototype's own output; the exact rows are in `INTENT_PARAMETER_MAP.md`.

| Say | KID does | Revision |
|---|---|---|
| *“Make this vocal darker and more aggressive.”* | Air shelf −3 dB, Presence −0.8 dB, compression 3.5:1 / −23 dB, attack 6 ms, makeup +1.8 dB, warmth +2 dB | 1 |
| *“A little less.”* | The same rows at about 70 % | 2 |
| *“Keep the vocal upfront.”* | Removes the presence cut; caps echo and Dimension if they are on | 3 |
| *“Make it darker.”* | Air shelf goes on to −5.1 dB | 4 |
| *“Make it wider but keep it upfront.”* | Doubler and Dimension on, starting low; Dimension capped for "upfront" | 1 |
| *“That’s too much width.”* | The width moves halve | 2 |
| *“Clean it up but keep the character.”* | CLEAN on at 65 %; saturation and effects untouched | 1 |
| *“Keep the clean sound but add more character.”* | Keeps CLEAN, adds a blended Mutation and a little low end, and adds **no** saturation. Flagged **NEW WORDS** | 2 |
| *“Give my ad-libs a distant, psychedelic sound.”* | 1/4 echo throws, Dimension movement, mirrored width, a blended Mutation, presence pulled back. "psychedelic" flagged as **NEW WORDS** | 1 |
| *“I don’t know what it needs. Help me improve it.”* | Clean-up, presence and gentle compression, with rows tagged **SIMULATED MEASUREMENT** | 1 |
| *“make it sound like purple”* | *Didn't catch that*: no proposal, the sound unchanged | — |

Throughout, the artist can do any of the following, and every one was exercised by the QA script:
- Tick or untick any row, or use **All** / **None**.
- **Listen: Current / Proposal**, so audition never applies anything.
- **×**: closes KID; the proposal waits and the Booth says so.
- **Cancel**: discards the proposal; the sound is unchanged.
- **Apply**: applies only the ticked rows.
- **Undo**: after Apply, in the drawer or the top bar.

## Journey 4 — A/B without fear

1. Clicks **A** while it is empty, which stores the current sound. A's state reads *playing*.
2. Moves a big move. A now reads *changed*: the slot hasn't been overwritten, and it says so.
3. Clicks **B** while it is empty, which stores the new sound. Now A reads *stored* and B *playing*.
4. Clicks **A**. Because B is unchanged, A plays at once.
5. Changes something, then clicks **B**. A dialog appears (`100_46_ab_switch_confirm`):

   > *Switch to B? You've changed the sound since you stored it in A.*

   with Cancel / Switch without storing / **Store in A, then switch**.
6. Wants A to hold the new version, so clicks **⤓** on A. A dialog appears (`100_47_ab_overwrite_confirm`):

   > *Replace what's in A?*

   Cancel keeps A untouched (checked); **Replace A** stores it.

## Journey 5 — Recovering when something is wrong

| Situation | What the artist sees | Screenshot |
|---|---|---|
| No vocal coming in | The signal card reads *No vocal coming in* and points to the Mixer insert. The visual is a quiet seed reading *Waiting for your vocal*. Next step: *Play your vocal*. | `100_31_no_input` |
| No vocal, but asks for Build My Mix | *No vocal heard yet*, with **Skip: use as designed**; the review then says "not fitted" | `100_32_build_no_input` |
| Input clipping | **CLIP** on the IN meter, plus *Your vocal clips before GlitchKid*. It suggests lowering the recording level, and says honestly that clipping already in the audio can't be repaired. | `100_33_input_clipping` |
| Speech not set up | KID says so, and offers **Set up speech** (a one-time, verified model download that then works offline). Typing works meanwhile. | `100_24_speech_not_set_up` |
| Microphone blocked | Where to allow it on macOS and Windows; typing still works | `100_25_mic_blocked` |
| Offline | *Offline is fine: it all runs here.* Only the one-time model download is unavailable, and it says so. | `100_26_offline`, `100_27_offline_speech_setup` |
| Mastering mode is on (a real plug-in mode) | KID is unavailable and says why; workspaces say the vocal controls are not in use | `100_28_mastering_mode_kid`, `100_29_mastering_mode_shape` |
| Not processing (design placeholder) | *Audio passes through unchanged.* There is no verified production trigger for this state yet. | `100_30_processing_error` |
| Didn't understand the words | *Didn't catch that*, with examples; nothing proposed | `100_23_kid_not_understood` |
