# Deliverable 2 — Definitive V1 specification

> **GlitchKid turns a raw vocal into a professional starting mix as quickly and effortlessly as possible — without taking control away from the artist.**

**Status.** This is a proposal for owner review. The locked decisions from the brief are treated as requirements. Where this document makes a choice the brief left open, it is marked **Proposed**.

Evidence tags as in Deliverable 1:

| Tag | Meaning |
|---|---|
| [V] | Verified in source |
| [A] | Assumption |
| [T] | Needs testing |
| [R] | Needs research |

## 1. Product definition

| | |
|---|---|
| Category | Professional vocal-mixing plug-in (an insert on the vocal track) |
| Customer | Intermediate independent recording artists who record and mix their own vocals |
| Promise | Professional-quality vocals, faster and easier |
| Differentiator | One-click, genre-based starting mix from analysis of the actual vocal, plus **Beat Lock**: automatic adaptation to the instrumental |
| What it is not | A mastering suite, a creative sound-design toy, a chatbot, or a console with dozens of modules |

### Claims policy (binding on UI copy and marketing)

- Never call a result “professional” because it is louder. Every comparison GlitchKid offers is loudness-matched (§8).
- A genre is “a starting point for this sound”, never a guarantee.
- No artist names; no “AI mixes like a pro engineer”. Analysis is described as measurement (“we measured”), never as hearing or judging.

## 2. Scope

| In V1 | Deferred (implementation kept; see Deliverable 3) |
|---|---|
| Five stages: TUNE, CLEAN, SHAPE, SPACE, FINISH | KID assistant (text and voice) |
| Genre selection (7) and Generate Mix | Reference Match |
| Four macros: Tone, Punch, Space, Presence | Formant, Harmony, Mutation, Stutter, Doubler as user-facing effects |
| Beat Lock, automatic | Alter Egos / Mood Engine; AutoMix (“ASTRO MODE”) |
| Pitch correction with key/scale suggestion | Mastering Lab |
| A/B, instant bypass, loudness-matched compare | Vocal-file drop analysis UI (the engine stays for TUNE suggestions) |
| User presets (organised by genre) and full session recall | Psychedelic visuals, MANDALA, Glitch Relic art (pending a visual-direction decision) |
| VST3 (macOS, Windows) and AU (macOS) | AAX / Pro Tools (not requested) |

## 3. The core journey

### Step 1 — Load the vocal

1. The artist inserts GlitchKid on the vocal track; the plug-in processes that track's audio.
2. The window opens on the **Session view** (Deliverable 7). Status: **“Waiting for your vocal — press play in your DAW.”**
3. The **input meter** shows when audio arrives: the vocal is detected above −50 dBFS RMS in 100 ms frames, which is the existing `VocalAnalyzer` gate [V].
4. If the input clips (samples ≥ 0.999 full scale) a warning reads: *“Your recording is clipping before GlitchKid. Lower the input level and re-record if you can.”*

### Step 2 — Select a genre

1. Seven cards: **POP · R&B · MELODIC RAP · TRAP · RAP · ATMOSPHERIC · NATURAL**. Deliverable 5 defines them.
2. Each card has a one-line sound description, e.g. “Bright, upfront, polished”.
3. **No genre is preselected** on a fresh instance. The **Generate** button is disabled until a genre is chosen (the button explains why).
4. Changing genre does nothing on its own; nothing changes until Generate (no surprise processing).

### Step 3 — Generate the mix

1. One primary button: **GENERATE MIX**.
2. **Proposed — audio already heard.** If GlitchKid has already heard ≥ 2 s of active vocal since the window opened or the last analysis, Generate uses that audio immediately.
3. **Proposed — not enough audio yet.** Otherwise Generate starts listening: *“Listening… play 8 seconds of your vocal.”* There is a progress ring (2–8 active seconds; existing analyser limits [V]), and the mix is generated as soon as listening ends.
4. Result: a complete starting mix (all five stages set) from the genre profile adapted to the measured vocal. The engine is Build My Mix's (Deliverable 4).
5. The previous sound is kept as **Previous mix**, one click away, so “compare with what I had” always works.
6. The artist's **Pitch Tune settings, Gain and Mix are never changed** by Generate (existing rule [V]). Generate never switches Tune on.
7. **Edge cases:**

| Situation | Behaviour |
|---|---|
| Too little vocal (< 2 active seconds after 30 s) | *“I couldn't hear enough of your vocal. Play a longer part, or Generate as designed (not fitted to your voice).”* |
| Input clipping | Generate still runs, using the existing clipping rules (saturation halved, makeup reduced [V]), and repeats the warning |
| Re-generate after manual edits | Confirmation: *“Generate again? Your changes will be replaced. (Previous mix keeps them.)”* |
| Generate with the same genre twice | Same input gives the same result (deterministic engine [V]) |

### Step 4 — Refine with macros

- Four large controls: **TONE · PUNCH · SPACE · PRESENCE**.
- Centre = the generated mix. Each one moves a musically defined dimension (§5).
- Double-click returns a macro to centre.
- The stage detail views (§4) are available but never required.

### Step 5 — Add the beat (optional, any time)

- When audio arrives on the Instrumental input, **Beat Lock** activates on its own:
  1. **detected** (“Beat detected”);
  2. **listening** (both vocal and beat);
  3. **adapted** (“Beat Lock: 3 adjustments”).
- A **Beat Lock** switch compares the adapted mix with the mix before adaptation.
- Manual edits always win (Deliverable 6).
- No beat routed: every other feature works normally. The Beat Lock area shows *“Optional: route your beat to GlitchKid's Instrumental input”*, with a host-specific how-to link.

### Step 6 — Compare

| Control | Behaviour |
|---|---|
| **A / B** | Two snapshots. An empty slot stores the current sound; an occupied slot recalls it. GlitchKid asks before leaving a changed slot or overwriting a different sound (rules approved in the earlier design round). Recall never changes the stored snapshots. |
| **Bypass** (top bar, key D) | Instant, click-free, latency-aligned. **Loudness-matched by default** (§8). |
| **Beat Lock on/off** | Compares adapted vs. unadapted. |
| **Previous mix** | Swaps to the sound before the last Generate or genre change. One undo history step. |

None of these changes the underlying mix; they only change what you hear or which stored sound is live.

### Step 7 — Save

| Kind | Behaviour |
|---|---|
| **Preset** | Every project. Name plus genre tag; stored as the flattened sound plus the generation context (genre, macro positions) (Deliverable 3 §4, “Preset migration”). Existing safe-save rules [V]. |
| **Session** | Automatic with the DAW project. Everything comes back exactly: genre, generated baseline, macros, Beat Lock layer and its on/off, A/B, preset name, stage views. A **state version** is written (new). |
| Recall | Loading a preset never changes Tune key/scale, Gain or Mix. **Proposed:** a preset's own Tune strength/style is offered, not forced. |

## 4. The five stages

Stage order in the UI follows the artist's mental model. The processing order is fixed (Deliverable 4).

### TUNE — pitch correction

| Control | Behaviour |
|---|---|
| On/off | Default off. Switching on adds ~31 ms latency, reported to the host [V]. |
| **Suggested key** | After listening (Generate, or a “Detect key” action): *“Suggested: A minor (confident)”*, with **Use** and **Keep mine**. **Never applied silently.** Hidden when the evidence is weak. |
| Key, Scale | 12 keys; Major, Minor, Chromatic shown first. Harmonic minor and the pentatonics live under More. |
| Strength | 0–100 %. |
| Style | Natural / Light / Hard: sets speed 80 / 35 / 0 ms and suggests strength [V]. Speed itself sits under Advanced. |
| Readout | Detected note → target note. |

Genres may *suggest* a style (TRAP → Hard). Tune is still switched on only by the artist.

### CLEAN — remove problems

| Control | Maps to |
|---|---|
| Clean amount | `clean_amount` (switches `clean_on`) [V] |
| De-ess | `clean_ess` trim [V] |
| Advanced | Corrective EQ trim `clean_tone`, Resonance trim `clean_res` [V] |
| Telemetry | What CLEAN is really doing: `CleanLab::Applied` [V]. Shown only when it changes something. |

Out of scope, stated in the UI copy: **no noise reduction or breath removal** in V1 [R].

### SHAPE — core tone

| Control | Maps to |
|---|---|
| Low · Body · Presence · Air | Gains of the four EQ bands [V] |
| Compression | One “amount” control over threshold and ratio. Attack, release and knee keep the generated values. |
| Warmth | `sat_drive` [V] |
| Advanced | Band frequencies and Q; compressor attack, release, knee, makeup; AIR LAB presence/air/mix |

### SPACE — position in the mix

| Control | Maps to |
|---|---|
| Reverb | Amount, Size, Decay (**new DSP**, Deliverable 4 §5) |
| Delay | Amount, Time (synced division or free), Feedback (`echo_*`) [V] |
| Width | `dim_mix` / `dim_width` [V] |
| Advanced | Reverb pre-delay and tone, delay tone and width, **ducking** (new: the repeats dip under the dry vocal) |
| Internal (no V1 control) | The Doubler is a texture component that a genre may use |

### FINISH — final vocal output

| Control | Maps to |
|---|---|
| Output | `gain` [V] |
| Mix | `mix` (dry/wet) [V] |
| Limiter | On by default for generated mixes; Ceiling (−1 dBFS default) [V] |
| Match level | **Proposed, new.** Trims the output so the processed vocal sits at the same loudness as the input (uses the existing loudness measurement [V]). Keeps gain-staging predictable in the DAW mix. |

FINISH is a vocal output stage, **not mastering**. No loudness targets, no stereo-bus processing.

## 5. Global macros

All four are **automatable host parameters** (new IDs `macro_tone`, `macro_punch`, `macro_space`, `macro_presence`, range −100…+100, default 0 = the generated mix). Each one applies a **bounded offset to the generated values**; it never overwrites them (Deliverable 4 §2).

| Macro | Musical meaning | Moves (offset at ±100) | Guard rails |
|---|---|---|---|
| **TONE** | Darker ↔ brighter overall character | Tilt: high shelf ±4 dB, low shelf ∓2 dB | Presence untouched (that is PRESENCE's job) |
| **PUNCH** | Softer ↔ more impact and consistency | Threshold −8…+6 dB, ratio × 0.7…× 1.6, slightly slower attack at +100 so consonants cut | **Loudness-compensated** so PUNCH never wins by level |
| **SPACE** | Dry and intimate ↔ deep and wide | Reverb send × 0…2, delay mix × 0…2, width × 0.5…1.5, all relative to the genre | At −100: fully dry. Ducking deepens as SPACE rises, to protect the words. |
| **PRESENCE** | Further back ↔ forward and intelligible | Presence bell ±3 dB, AIR LAB presence ±25 points; de-ess sensitivity follows (more presence → slightly more de-essing) | Capped so presence never exceeds +6 dB in total |

- **Smoothing.** Changes reach the DSP through the existing 20 ms parameter smoothing; compressor changes go through its own ballistics [V].
- **Independence.** Each macro owns a distinct dimension, so the order of moves doesn't matter (offsets add).
- **Display.** Readout in plain words (“+2.4 dB presence”) and a ring showing the offset from centre.

## 6. Beat Lock (summary; full specification in Deliverable 6)

1. Activates automatically when a beat is detected.
2. Measures what can be measured: levels, spectral overlap, low-end density, headroom; stereo image is new.
3. Applies bounded adjustments **once per beat**, as a separate layer.
4. Never runs continuously.
5. Re-analyses only when the artist asks, or after a clear beat change (and then asks first).
6. Manual edits to a parameter remove Beat Lock's claim on it.
7. Compare: Beat Lock on/off.

## 7. Pitch correction workflow

1. Detection runs during listening. Key evidence comes from the pitch-class histogram of the voiced vocal: the existing method in `VocalFileAnalysis` [V], fed live (new).
2. A suggestion appears only when reliable (existing `keyReliable` rule [V]).
3. **Use** sets key and scale (one undo step). **Keep mine** dismisses it until the next detection.
4. Detection never changes key or scale on its own; the artist's selection is never overridden.
5. Strength and style are adjusted by the artist; genre suggestions are labelled as suggestions.

## 8. Compare and loudness

- **Bypass is loudness-matched by default everywhere** (today only on START [V]).
- A small label says **“Matched loudness”**. Advanced users can switch to *true bypass* in settings.
- **Proposed:** the Output Limiter is removed from the bypassed path. To keep a level-matched dry signal from clipping, a transparent safety limiter is used **only** while matching raises the dry signal above 0 dBFS. This needs a decision (Deliverable 4 §6).
- A/B, Beat Lock on/off and Previous mix are heard at equal loudness: the comparison gain is shown, never hidden.

## 9. Saving, recall and states

| Must come back after save / reopen | Notes |
|---|---|
| Every parameter (including macros) | Existing [V] plus the new macro parameters |
| Genre, generated baseline, generation evidence summary | New |
| Beat Lock layer (deltas, claimed parameters, on/off, beat signature) | New |
| A/B slots and active slot, preset name, Previous mix | Existing (A/B, preset) [V]; Previous mix new |
| Stage view state, “Advanced” open/closed | Existing UI flags [V] |
| **State version** | New. Loading an older version runs a tested migration (Deliverable 3 §3). |

## 10. Non-functional requirements

| Area | Requirement |
|---|---|
| **Sound quality** | Primary launch gate (Deliverable 8 §2). |
| Latency | ≤ 2 ms reported with Tune off (6 samples + limiter); ~31 ms with Tune on. Latency changes only when the artist switches Tune. **Proposed:** the limiter's latency is always reported in V1, so it does not change at runtime. |
| CPU | **Proposed target [A]:** full V1 chain including reverb at ≤ 5 % of one Apple M1 performance core, 48 kHz / 128 samples, and ≤ 8 % on a reference Windows x64 machine. To be measured. |
| Sample rates, buffers | 44.1, 48, 88.2, 96 kHz (192 kHz must work [T]); buffers 16–4096; mono and stereo tracks. Output must be independent of buffer size (existing design principle [V]). |
| Formats and hosts | VST3 macOS (arm64 + x86_64) and Windows x64; AU macOS. Validated in FL Studio, Ableton Live, Reaper (all OS) and Logic Pro (AU) before any support claim (Deliverable 8 §4). |
| Real-time safety | No allocation, locks or I/O on the audio thread (existing rule [V]). |
| Privacy | No network access at runtime in V1 (KID speech deferred). |
| Accessibility | Text ≥ 12 px at 100 %, contrast ≥ 4.5:1, every control keyboard-reachable, with accessible names. |
| Defaults | A fresh instance before Generate is transparent (existing principle [V]). |

## 11. Out of scope for V1

- KID, Reference Match, Mastering Lab and creative effects (Deliverable 3).
- Noise reduction; breath control.
- Stem separation: a beat already mixed into the vocal track is measured as “vocal” [V].
- Cloud services.
- AAX.
- Large preset libraries: 7 genres and the user's own presets only.
