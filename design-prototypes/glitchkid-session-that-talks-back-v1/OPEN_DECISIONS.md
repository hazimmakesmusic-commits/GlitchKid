# Open decisions — The Session That Talks Back

What this concept leaves for you to decide, or for engineering to settle, before any of it is built. Each entry gives the question, the options, a recommendation, and what it blocks. Facts are from the source at `1f5319a`.

## Engineering (no new DSP in any of them)

### 1. The audition engine (“Hear”, “With these”)
**Question.** Hearing an idea without applying it needs the processor to play a *different parameter set* than the one stored.
**Options.**
- (a) A preview snapshot. The audio thread reads a second `SoundState` while auditioning; parameters and the host never see it.
- (b) Write the parameters, then restore them afterwards. Simple, but the host records automation, and a crash leaves the preview applied.
- (c) A second chain. CPU heavy.

**Recommendation.** (a), with a short crossfade, and with the preview ending on any parameter gesture from the host.
**Open sub-question.** Ideas that change latency (Pitch Tune ~31 ms, Formant ~21 ms, Harmony) would shift timing mid-audition. Either report the larger latency while auditioning, or mark those ideas “latency changes when kept”.
**Blocks.** Hear on cards, tapes and the notebook; “With these” in every review.

### 2. One undo history, and redo
**Today.** One step back after a build, profile load or Beat Lock (`PluginEditor::undo`), and 50 steps inside the KID panel (`KidPanel.cpp:424`). No redo; knob moves are not in any history.
**Recommendation.** One `juce::UndoManager` for every sound change. Each knob gesture, Keep, Use, A/B switch and tool is one transaction; notebook and preset edits are not (they ask first instead).
**Open sub-question.** FL Studio has its own undo for parameter changes, so check that the two don't fight.

### 3. Session name, notebook and desk in the project state
**Question.** These are new per-project data, so the saved-state format changes. This prototype doesn't change it, as the brief required.
**Recommendation.** A versioned child tree in the plug-in state. Older projects load with an empty notebook and the default name. Limit the notebook (e.g. 50 entries) and drop KID cards you haven't kept on reload.
**Decide.** Is the desk saved with the project (the prototype does), or only the notebook?

### 4. KID's three directions
**Today.** `KidDirector` returns one validated plan.
**Option.** Call it three times with derived intents (as the prototype's “lenses” do): *as asked*, *+ keep upfront (depth × 0.5, width × 0.7)*, *further (+depth, +width, +strangeness)*. Drop duplicates.
**Decide.** Three cards, or one card plus “closer / further” buttons? Three is more playful; one is calmer. Artist testing should choose.

### 5. KID's new words
*rough, rougher, gritty, dirty, distorted, crunchy* → warmth + punch; *unstable, wobbly, broken, lo-fi* → Mutation. These are not in `KidDirector`'s lexicon. The prototype flags them as new words in KID's reply.
**Decide.** Add them to the lexicon (with tests, as for the existing words), after a listening check of the weights.

### 6. Idea recipes and draft worlds: sound design and listening tests
The five idea cards and six draft worlds are valid parameter states designed here. Nobody has listened to them.
**Decide.**
- Who designs and listening-tests them.
- Whether worlds stay a separate shelf, or some become factory presets. That would change the factory preset set, which the brief excluded for now.
- A trademark check on the names (Neon City, Chrome Room…).

### 7. Happy accident bounds
Random moves are kept inside conservative ranges, but some add latency (Formant, Harmony) or can be loud (Stutter at 40 %, Echo at 60 % feedback).
**Decide.**
- Exclude latency-changing moves, or label them.
- Seed per session (repeatable, as in the prototype) or truly random.
- Whether a “roll again” keeps the previous card.

### 8. Which KID notes ship
The prototype's notes come only from known facts: settings, the input level, host BPM, or a labelled measurement (`VocalDiagnosis` “mud”).
**Decide.**
- The rule list.
- How often a note may appear.
- Whether the default is *Notes on* or *Only when I ask*.

Recommendation: at most one note at a time, never during recording, and no note that repeats once dismissed in that session.

### 9. Reflowing layouts vs uniform scaling
**Today.** The editor scales uniformly between 70 % and 150 % with a fixed aspect ratio (`setResizeLimits`, PluginEditor.cpp:674–676).
**Proposal.** Three layouts: compact 900×580, standard 1120×680, large 1360×800. Text stays ≥ 12 px instead of shrinking.
**Decide.** Free resizing between breakpoints, or snapping to the three sizes; and the minimum size. Reflowing JUCE layouts is real UI work.

### 10. The handwriting font in the plug-in
Kalam (SIL OFL 1.1, Indian Type Foundry) is bundled here for names and notes. Shipping it means:
- embedding it as `BinaryData`;
- adding it to `LICENSES.md` (OFL allows embedding; the font can't be sold on its own);
- testing it on the software renderer.

Kalam covers Latin and Devanagari; other scripts fall back to the system font. Test those with real session names.

### 11. FL Studio's play state
The processor reads only the BPM from the playhead (PluginProcessor.cpp:857–859). Showing “playing / stopped” needs `isPlaying` from the same `getPosition()`. This is small and low-risk.

### 12. The real waveform
The prototype draws a simulated take. In production: a short history of the input (or output) level from the existing analysis/display path, drawn at display rate.
**Budget.** As for the Mandala view, under 16 ms per full repaint on the software renderer. Measure it.

## Product

### 13. Three ways to keep a sound
- **Notebook:** this session's ideas, with the project.
- **Saved sounds:** presets, in every project.
- **A/B:** two quick slots.

Each is clear on its own, but together they may be one too many. Watch whether artists understand “Keep this sound” vs “Save…”. If not, merge the notebook and A/B (A/B as two pinned notebook entries).

### 14. Beat Lock routing in FL Studio
The prototype explains the sidechain routing in general terms and marks FL Studio's exact clicks as unverified. Verify them in FL Studio (Mixer › route to this track › sidechain; plug-in input settings) and write the exact steps. Detecting “nothing routed” automatically would also help.

### 15. Reference Match: the code does not match its documented bound (finding)
`ReferenceMatch.h` says *“a single move is at most 3 dB … a band already beyond a limit is left alone rather than pulled back”*. `ReferenceMatch.cpp` only skips a band that is beyond the limit **in the step's direction**:

```cpp
if ((step > 0.0f && cur >= limit) || (step < 0.0f && cur <= -maxTotalDb))
    continue;
const float target0 = juce::jlimit (-maxTotalDb, limit, cur + step);
```

So a band below −6 dB with a brightening step is pulled to −6 dB in one move. For example, Air shelf at −12 dB, which the Radio Static world or Broken Radio produce, would become −6 dB: a 6 dB move. The same happens above the presence limit with a darkening step. The existing test (`ReferenceMatchTests.cpp:249`) covers a band at its limit in the step's direction, not this case.

**Decide.** Either:
- fix the code to match the header (skip when `cur > limit || cur < -maxTotalDb`), or
- clamp the move itself to ±3 dB and update the header.

Add a test for a band at −12 dB either way. **This prototype follows the header.** No production code was changed.

### 16. Where MASTER lives
It is in the rail, below a line, in steel blue, with a confirmation before it switches on.
**Alternative.** Hide it behind a setting or the plug-in menu, since it is used once per song on a different instance.
**Decide.** By how often artists use it.

### 17. “Not processing”
The prototype shows a *Not processing* state (and KID unavailable), as v2 did. There is no verified trigger for it in the source. Either define one (e.g. an engine error or a sample-rate mismatch) or drop the state.

### 18. Accessibility and host keyboard
Not tested:
- VoiceOver or Narrator;
- the host's keyboard focus;
- whether FL Studio captures **K**, **D** or Ctrl/Cmd+Z before the plug-in.

Every control has a label and a description, focus is visible, contrast is ≥ 4.5:1 and text ≥ 12 px (QA_REPORT). If the host captures the keys, the mic and DRY buttons remain.

### 19. KID's conversation
The prototype keeps only KID's latest reply, in memory, and never saves what you said. Recommendation: keep it that way (privacy), and keep the desk cards as the record of what KID suggested.

### 20. Licensing, accounts, model licence
Unresolved. The prototype assumes no account, no activation and no online step. `LICENSES.md` says the whisper `ggml-base.en` model's licence should be verified before any commercial release.

### 21. Which concept to pursue
This concept and `glitchkid-vocal-playground-v2` share the same facts (parameters, KID rules, A/B and save rules) but differ in feel:
- **v2:** a booth with a big KID drawer and proposals.
- **This one:** a session, ideas on a desk, notes, a notebook.

**Decide.** One, or a merge. For example, this concept's desk and notebook, with v2's drawer-based KID conversation for artists who want to talk more.
