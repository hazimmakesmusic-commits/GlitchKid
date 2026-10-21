# GlitchKid Vocal Lab: experience differentiation audit

Status: read-only audit. Nothing in the repository was changed except this file. Nothing was committed, pushed, stashed or switched. No build was run for this audit; test statements below refer to the last full run on this exact commit (321 test groups, 0 failures, fresh build outside `build/`, Linux), because the working tree has not changed since (`git status` clean, HEAD `c72e3f1`).

How to read it: **Observed** = seen in code, tests or docs of this repository. **Hypothesis** = my reasoning, not yet tested with any artist. **Recommendation** = what I would do. No competitor research was done (none is possible offline here), so no claim below says anything is unique in the market.

---

## 1. Observed: what is actually in the repository

Status classes: **V** implemented and verified, **I** implemented but not adequately verified, **P** partially implemented, **D** planned or documented only, **M** missing.

| # | Area | Status | Evidence and limits |
|---|---|---|---|
| 1 | Pitch correction (Pitch Tune): strength, speed, key, scale | **V** (DSP, offline) | `PitchCorrection/*`, `PitchTuneTests`, `PitchCorrectionTests`. Verified on synthetic signals; roadmap itself says "reported working in FL Studio on the Mac", no listening tests on more voices. ~31 ms latency while on. |
| 2 | Key/scale selection | **P** | 12 keys, six scales only (chromatic, major, natural minor, harmonic minor, major and minor pentatonic). No key detection (UI says so). No raga or other scale families. |
| 3 | Live pitch feedback | **V** | Real detector output (sung note, cents, target note) shown as text and as the outlined key on TUNE. |
| 4 | EQ | **V** | 4-band RBJ EQ, tested against the real DSP; an EQ response graph computed from the same formulas (test compares it with the real output within 0.15 dB). |
| 5 | Dynamics | **V** (offline) / **I** (by ear) | Broadband compressor with a real gain-reduction meter. No listening validation on real vocals recorded in the repo. |
| 6 | De-essing | **M** | There is no de-esser parameter or DSP. Sibilance is only measured by the analyser (`sibilanceIndex`) and used inside BUILD; AIR LAB ducks its own added highs on sibilance. |
| 7 | Gain management | **P** | Output gain, Mix, per-profile makeup calibration (levels within about +-0.6 dB of the input on a synthetic test signal), AIR LAB trim. No loudness matching of processed vs dry on the user's real audio. |
| 8 | Reverb | **M** | None. "Space" is echo + Dimension width + synthetic doubler. |
| 9 | Delay / echo | **V** (offline) | Tested; tempo sync uses the host BPM (`getPlayHead` is used for BPM only). |
| 10 | Creative effects (formant, harmony, mutation, stutter, Alter Egos) | **V** (code, tests) but hidden | Parameters kept for session compatibility, removed from the UI; BUILD switches them off. |
| 11 | AIR LAB (presence and air) | **V** (synthetic) / **I** (real vocals, hosts) | `Tests/AirLabTests.cpp`; no host, no pluginval, no listening test. |
| 12 | BUILD MY VOCAL MIX | **V** (engine, offline) / **I** (real vocals) | `automode::buildMix` is pure and tested for range, repeatability, no drift, silence, fuzz; the plug-in listens to up to 8 s of live playback, then applies. Sound quality on real rap/trap vocals has never been assessed in this repo. Beat-informed mode: **I** (the FL Studio routing of the optional instrumental input is unverified). |
| 13 | Analysis | **V** (synthetic) | `VocalAnalyzer`: peak, RMS (not LUFS), crest, clipping, dynamic range, 5 band shares, a rough sibilance index. No pitch statistics, no formant or vowel information, no phrase segmentation, no noise-floor estimate. |
| 14 | Explanations | **P** | Every change carries a reason (`MixProposal.changes[*].reason`, `evidence`, `unavailable`). The UI shows the first three fitted values and a WHAT CHANGED dialog; the reasons are not shown beside the controls. |
| 15 | Profiles | **V** (offline) | 8 profiles, tests prove they are distinct, finite, level-matched on a synthetic signal, never clip. Whether they are *useful* to rap/trap/Hindi/Punjabi vocals is untested. |
| 16 | Presets and saved state | **V** | `.gkpreset` files, atomic overwrite, hostile-file tests, state round trip, older state loads with defaults. |
| 17 | A/B workflow | **V** (offline) | Two stored sounds (SET A/B, recall, copy, edited flag), saved with the session. Switching applies parameters one by one from the message thread; a test proves no allocation and finite audio while this happens from another thread. **Whether the switch is click-free by ear is unverified**; the roadmap lists "an atomic parameter swap if partial application is ever audible". |
| 18 | Dry/processed comparison | **V** | Real bypass, delay-matched, 20 ms crossfade (`MixTests`). It is **not loudness-matched**: the processed side can be louder, which biases judgement (hypothesis: this matters for the target users). |
| 19 | Undo of a build or profile load | **V** | BACK TO MY SOUND (one level), tested end to end through the real button. |
| 20 | Phrase-level or time-aware processing | **M** | No phrase detection, no timeline access beyond BPM, no regions, no scene or lane concept. The analyser's activity gate is a 100 ms level frame, not a phrase model. |
| 21 | Plug-in-written automation | **M** | A VST3 does not edit the host's automation lanes; only the user can. All parameters are host-automatable. |
| 22 | UI navigation and feedback | **V** (headless renders) / **I** (hosts) | Three workspaces, a processing-chain strip showing real stage activity and real levels, a build button whose states follow the real analyser state; rendered and clicked headlessly in tests. Never opened in FL Studio since the redesigns. |
| 23 | Audio-thread safety | **V** (design, tests) | No allocation in the audio path (tests), atomics for UI data, analyser publishes with release/acquire ordering, UI on the message thread. No ThreadSanitizer run recorded in this audit. |
| 24 | Latency | **V** | Reported latency follows Pitch Tune; AIR LAB adds none (test). |
| 25 | Host validation | **M** | No pluginval run, no DAW run in any session so far except user screenshots of an older build in FL Studio. |
| 26 | Vocal DNA, character tab, 50-artist catalogue | removed | Rejected concepts are gone from the UI (the 50 presets were deleted; the Alter Egos remain as hidden parameters only). |

Facts that shape every concept below:
1. The plug-in processes **live**, causally. It cannot hear a phrase before it arrives and cannot read the project. "Analysis" means: the user presses BUILD and plays about 8 s.
2. The mix engine is a **pure function** from (profile, analysis, current sound) to a list of parameter changes with reasons. Calling it several times on one stored analysis is cheap and needs no audio thread work.
3. The only compare tools today are DRY/PROCESSED (not level-matched) and two A/B slots (not level-matched).
4. All sound state is a flat list of parameter values (`SoundState`), so any "alternative" is already representable and saveable.
5. Profiles are level-matched against the input in offline tests, which is the one existing piece of loudness discipline.

---

## 2. Concept evaluation

Scale 1-5. For "risk", 5 = low risk. "Fit" = suitability for independent rap, trap, melodic rap and Hindi/Punjabi/Hinglish vocals, judged from the code's capabilities (hypothesis, not user data).

### A. Listen-first mixing (alternatives, matched loudness, choose)
- What the code gives it: the mix engine can produce several alternatives from one analysis; A/B slots and `SoundState` already hold sounds; DRY/PROCESSED and BACK TO MY SOUND exist; profiles are already level-calibrated; the editor already drives all of this.
- What is missing: a way to switch between several candidates quickly during playback, **level matching on the user's own audio** (only static, synthetic-signal calibration exists), a *meaningful* definition of "alternative" (the 8 profiles are not alternatives to one question; they are 8 different styles), a glitch-free switch (unverified), and a decision memory.
- Scores: differentiation **3** (A/B and presets are common mechanisms; the difference would be *what* is offered and *how* it is level-matched, which is not yet designed or proven), quality **3** (it improves decisions, not the DSP; it can lift the floor for artists who cannot judge a louder preset), ease **5**, feasibility **4**, risk **3** (loudness matching and glitch-free switching are real unknowns), fit **4** (rap/trap decisions are often "tighter vs bigger vs brighter"; the scale list is a gap for Hindi/Punjabi but not for this concept).

### B. Living vocal workspace (one view of the vocal, the processing and the reasons)
- Observed: the chain strip, the real input/output meters, the GR meter, the tuning read-out, WHAT CHANGED and per-change reasons all exist.
- Scores: differentiation **2** (it is largely built; a more elaborate version would add polish, not a new way of deciding), quality **2**, ease **4**, feasibility **4**, risk **4**, fit **3**. It is a good *supporting* layer for A, not a direction by itself.

### C. Performance-aware treatment (phrases and moments)
- Observed: nothing (item 20/21). The plug-in is causal and cannot see the project.
- What could exist inside a VST: (1) a causal activity/phrase tracker (hypothesis: detecting verse/hook or ad-lib vs lead from live audio alone would be unreliable); (2) user-driven scenes: two or more stored sounds that the **user** switches per phrase through a host-automatable "scene" control. (2) needs a new automatable parameter and an interpolation rule between sounds (switches, echo division, the 20 ms smoothing, A/B slots currently store whole states and are not parameters), which the roadmap already flags as unsolved.
- Scores: differentiation **4** (rap vocals genuinely differ between verse, hook and ad-libs), quality **4** (potentially the largest quality gain), ease **2**, feasibility **2**, risk **2** (new automatable parameter that touches every saved state, interpolation across discontinuous parameters, behaviour when the host loops or jumps), fit **4**. High value, wrong first step.

### D (new). Move-by-move review: every change of a build is an individually auditionable decision
- Observed basis: `MixProposal.changes` is a list of {parameter, from, to, reason}; `evidence` and `unavailable` strings exist. The profile's base values are the foundation; the engine's analysis-driven moves (compressor threshold, makeup, tone EQ, saturation, AIR) are the part that was derived from *this vocal*.
- Idea: after BUILD the artist can switch each analysis-driven move ("brightness -3 dB because the top end is hot", "compressor threshold -22 dB because your level is high") on or off while the vocal loops, hear it with/without, and read the reason next to it. The unit of listening is a single, named move, which is easier to judge than a whole preset.
- Scores: differentiation **3** (explaining and toggling individual moves is not something I can show competitors lack; it is a coherent way to use the engine's existing evidence), quality **3** (reduces the risk that a wrong automatic move stays in), ease **4**, feasibility **5** (data exists; apply a subset of the proposal), risk **4**, fit **3** (the moves are generic: levels, tone, dynamics).

### Not proposed
No second new concept is supported by evidence: reference-track matching needs loading and analysing user audio (a large feature with no code today), and anything claiming a "mix quality score" would be fabricated by the current analyser.

### Comparison

| Concept | Differentiation | Quality | Ease | Feasibility | Risk (5 = low) | Fit | Total |
|---|---|---|---|---|---|---|---|
| A Listen-first | 3 | 3 | 5 | 4 | 3 | 4 | 22 |
| B Living workspace | 2 | 2 | 4 | 4 | 4 | 3 | 19 |
| C Phrase-aware (as scenes) | 4 | 4 | 2 | 2 | 2 | 4 | 18 |
| D Move-by-move review | 3 | 3 | 4 | 5 | 4 | 3 | 22 |

The totals are not a decision; they say A and D are the only two that can be tested with the code that exists, and C is the most valuable but not yet buildable safely.

---

## 3. Recommendation

**Build A and D as one experience, "Takes"**, in a minimal form first. They share one object (a set of candidate sounds produced by the existing engine from one analysis) and one control (switch, compare, keep). Keep B as the explanation layer. Keep C as the later direction once the artist has proven they use alternatives at all.

### The artist problem
An independent artist mixing their own rap or melodic-rap vocal in FL Studio cannot tell whether an automatic mix is *good*, only whether it is *different or louder*. Today GlitchKid gives one answer (BUILD) and an un-level-matched DRY/PROCESSED switch. (Hypothesis: the artist then either accepts it blindly or discards it, and cannot say which part helped.)

### Core interaction (step by step)
1. TUNE: set key and scale (existing).
2. AUTO: pick a starting profile and press BUILD (existing). The analysis pass stays exactly as today.
3. **New:** the artist chooses one question to answer: TONE (darker / brighter), PUNCH (softer / more upfront) or SPACE (drier / more spacious). These three already exist as bounded combinations of real parameters (`MacroEngine`), so each question has a defined, safe axis.
4. GlitchKid prepares three takes along that axis: one step down, the build as it is, one step up. They are numbered, not named, and not labelled "better".
5. The vocal loops in the DAW. One control switches Take 1/2/3 (and DRY). The artist hears the same words with one difference. The take's level is compensated so loudness is as equal as the compensation can make it (see risks); the control says how it was compensated.
6. The artist keeps one take. The choice is applied as the current sound (undoable with BACK TO MY SOUND; A/B slots remain available). The next question (e.g. PUNCH) can be answered the same way.
7. For the kept take, the engine's analysis-driven moves are listed with their reasons; each can be switched off and heard with/without (D).

What the artist **hears**: three versions of one vocal that differ in one dimension, at nearly equal loudness, switched while the loop plays. What the artist **sees**: the three takes as plain numbered slots, the real chain strip and meters, the reason strings from the engine for the active take, and an honest line about how level was matched.

### What makes it different from a conventional vocal plug-in (observed vs hypothesis)
- Observed: conventional preset/A-B designs in this repo compare whole sounds without level matching and without a defined question.
- Hypothesis: constraining each comparison to *one axis, three steps, matched level* makes decisions easier and better for users without trained ears. This is the idea to test; it is not proven and I have no evidence about other products.

### Smallest testable prototype
P0 (no code change in the plug-in, one day): render the same vocal through the existing engine offline (the tests already do this for every profile) for TONE / PUNCH / SPACE at -1/0/+1 steps, level-match by measured RMS, export 9 WAVs per vocal; run a blind listening session with 5-8 independent artists on their own dry vocals. Measures: can they pick consistently, do they pick the same take twice, does matched level change their choice compared with unmatched?
P1 (small build, only if P0 shows people choose consistently and differently from the loudest): a TAKES strip on AUTO: axis selector, three take buttons, DRY, KEEP. It only applies existing parameters (`applySoundState`), uses `SoundState` for the takes, and uses static level compensation from a calibration table (like the profiles'), no new parameters.

### Reuse vs new
Reuse: `automode::buildMix` and `MixProposal` (candidates and reasons), `MacroEngine` (axes), `SoundState` and `applyPreset` (applying a take), BACK TO MY SOUND (undo), the chain strip and meters, the build button states, the calibration scripts (`tools/calibrate_profiles.py`) for level compensation, the offline test harness for rendering.
New: a take model (three sounds plus the chosen axis, UI-side only), a take-switch control, level compensation for take offsets (a calibration table measured offline, extending what exists for profiles), a per-move apply (for D), and the tests for all of these. Anything that needs a **new saved parameter** (for example a take selector the host can automate, or an internal take trim) is out of scope for P1 because it changes every saved state.

### Risks
- **Sound quality:** the takes are only as good as the engine's moves, which are unverified on real vocals. If all three takes sound mediocre, the interface makes that obvious rather than fixing it.
- **Loudness matching:** static compensation is calibrated on synthetic signals; a real vocal with different spectrum/dynamics can differ by more than the +-0.6 dB measured for profiles. Live measurement would need either storing audio (allocation, memory) or a trim control that the user did not ask for and that would touch Gain, which the engine promises never to touch.
- **Glitch-free switching:** parameters are applied one at a time from the message thread; stages have 20 ms smoothers, but there is no test that proves a take switch is inaudible (echo feedback, compressor state, partially applied sounds). This needs a test and a listening check before P1 ships.
- **Latency:** none expected (takes only change existing parameters; Pitch Tune is never touched by a build).
- **CPU:** none beyond the existing engine; building three proposals is cheap and runs on the message thread.
- **State compatibility:** none if takes live only in the editor; saving the chosen take is the normal sound. Anything stored in the session (last axis, which take was kept) would use the existing UI-preference mechanism (`uiProfile` style).
- **DAW stability:** the plug-in is switching parameters during playback, which hosts handle as automation; the risk is volume of host notifications (about 50 parameters per switch). Unverified in FL Studio.

### Validation with real artists before expanding
1. P0 blind sessions (above) with at least 5 artists covering rap/trap and at least 2 Hindi/Punjabi/Hinglish vocals; their own dry vocals, their own monitors.
2. Record: choice consistency across repeats, whether the chosen take differs between matched and unmatched level, time to a decision compared with their usual workflow, and a free-text reason.
3. If P0 passes, P1 with the same artists in their DAW; add: do they keep the result, do they open MODIFY afterwards, do they want to switch moves off (D)?
4. Only after both: consider C (scene automation) as a separate prototype.

---

## 4. Decision brief

**Build first:** P0, the offline matched-loudness takes test with real artists and their own vocals; then P1 (TAKES strip) if the test passes. Single axis, three steps, level-compensated, undoable, no new parameters.

**Why:** it is the only direction that can be tested before writing plug-in code, it reuses the engine, macros, sounds and undo that exist, and it directly addresses a decision problem the current UI has (un-matched, undefined comparisons). It does not depend on anything that is currently missing (no de-esser, no reverb, no timeline).

**What not to build now:**
- Phrase-aware processing or an automatable scene parameter (high value, but it needs interpolation rules across discontinuous parameters and changes every saved state).
- Any score, meter or message that judges "mix quality" (the analyser cannot support it).
- More profiles, an artist-inspired catalogue, a vocal "DNA" or character concept, or additional decoration of the existing screens.
- A de-esser or reverb *as part of this experience work* (they are legitimate DSP gaps, to be decided separately).
- Reference-track matching.

**Abandon or rethink the idea if:**
- In P0, artists cannot choose consistently between matched-loudness takes (random preference) or their choice does not change when loudness is matched (then level matching is not the lever).
- Artists prefer the un-compensated louder take and say the matched version "sounds worse" (then the premise that they benefit from matching is wrong, at least for this audience).
- The three takes sound nearly the same to them (the axes are too small) or all sound unusable (the engine is not good enough yet; fix the sound first).
- A glitch-free switch cannot be achieved without a new parameter or an audio-thread change that risks the sessions.
- In P1, fewer than about half of the artists use more than one take on their own songs after the first session.

**Evidence still missing for any decision:** listening results on real vocals of any kind, FL Studio and pluginval runs of the current builds, and any data from artists about how they currently choose between versions.
