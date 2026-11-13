# Deliverable 6 — Beat Lock technical specification

Beat Lock adapts the vocal to the instrumental: automatically, conservatively, and without fighting the artist. It builds on code that exists today [V]. Everything new is marked, and anything not yet known to be reliable is marked [R].

## 1. Principles

1. **It only measures what can really be measured,** and says what it does not know (the existing `unavailable` lists [V]).
2. **It changes only the vocal's own controls.** It never processes the beat, never moves faders, never invents reverb that isn't in the mix [V].
3. **It adapts once per beat,** as a separate, switchable layer. **It never runs continuously.**
4. **Manual edits always win.**
5. **Every adaptation is bounded,** and explained in plain words.

## 2. What exists today [V]

| Part | Behaviour |
|---|---|
| Input | Optional “Instrumental” bus (mono/stereo), disabled by default. Measured, never mixed (`PluginProcessor.cpp:204–210`, `:849–900`). Status: *not connected / connected but silent / live*; hosts send silence for an unrouted sidechain, so the first two can't be told apart. |
| Analysis | Two `VocalAnalyzer`s (lead and beat) run together while the host plays. Each gives digital peak/RMS (not LUFS), 5 band shares and dynamics. They stop after 2–8 active seconds. |
| Relationship (`SourceRelationship.cpp:27`) | Vocal-minus-beat level; per-band masking risk (bands 1–3 rated); worst band; beat low-end heavy (> 50 % below 120 Hz); beat very loud (peak within 1 dB of full scale); the lead's sibilance, presence and air shares; a confidence from active audio heard. |
| Moves (`beatMoves`, `:91–128`) | **PRESENCE:** bell + risk × 2 (≤ 2.5 dB), total ≤ +4 dB. *Withheld* (advice only) when the lead's sibilance > 0.35 or air > 0.12. **CLARITY:** Body −1 and low shelf −1 dB when low-mids are at risk; low shelf −1.5 dB when the beat is low-end heavy; never below −6 dB in total. **SPACE:** existing echo/width amounts × clutter (0.85, × 0.9 if very loud). A value beyond its limit is never pulled back. |
| Balance | Advice only (beyond ±6 dB). GlitchKid never moves a fader. |
| Flow | Manual: ANALYZE BEAT, then ticks per suggestion, then APPLY SELECTED; single-step undo (`PluginEditor.cpp:1554–1618`). Shared with Build My Mix. |
| Tests | `BeatLockTests` (6 groups), `SourceTests` (6), parts of `MixEngineTests` and `AutoModeTests` |

## 3. What can be measured

| Characteristic | Status | How | Use in V1 |
|---|---|---|---|
| Beat presence (signal on the bus) | **Measured** [V] | Bus enabled + peak; silence timer | Activation |
| Level and headroom | **Measured** (digital RMS/peak) [V] → **LUFS short-term + true peak, new** | Reuse the BS.1770 meter from Mastering | Balance advice; “beat very loud” |
| Spectral distribution | **Measured, coarse** (5 bands) [V] → **19 third-octave bands, new** | Reuse `SpectralProfile` | Masking estimate |
| Frequency masking | **Inferred** [V] from energy shares → **improved, new:** per band, the beat-to-vocal energy ratio **during vocal-active frames only** | Both analysers share frame timing | Presence and clarity moves |
| Low-frequency density | **Measured** [V] → finer split (sub < 60 / low 60–120 Hz), new | — | Low-shelf move |
| Stereo characteristics of the beat | **Not measured** (mono sum [V]) → **new:** per-band mid/side ratio and correlation | Small addition | Width/SPACE scaling (a wide beat → narrower vocal width) [A] |
| Transient / rhythmic density | **Not measured** → onset rate per band [R] | New detector | Ducking depth; delay division suggestion [R] |
| Tempo | Host BPM [V] | Playhead | Delay sync, reverb pre-delay |
| Key / harmony of the beat | **Not reliably detectable** for polyphonic music within this scope [R] | — | **Not used.** TUNE suggests the key from the vocal only. |
| Genre, mood, “quality” | **Not detectable; never claimed** | — | — |

## 4. Adaptation rules (V1)

The existing moves are kept. New moves are bounded the same way and pass the same review.

| Category | Rule | Bound | Status |
|---|---|---|---|
| PRESENCE | Existing (§2) | ≤ +4 dB total; withheld if harsh | Existing [V] |
| CLARITY | Existing (§2) | ≥ −6 dB total | Existing [V] |
| SPACE: echo, width | Existing × clutter | — | Existing [V] |
| SPACE: reverb | `rev_mix` × clutter (genre-limited, e.g. ATMOSPHERIC ≥ × 0.9) | — | New |
| SPACE: ducking | +10 points on a crowded or dense beat | ≤ 60 | New [R for “dense”] |
| De-ess coupling | If PRESENCE raised the bell and the lead's sibilance is within 0.05 of the threshold: `clean_ess` +10 | ≤ 100 | New [A] |
| Width | Wide beat (side/mid > threshold [A]) → `dim_mix` × 0.85 | — | New [R] |
| Genre scaling | Each genre scales or limits these moves (Deliverable 5) | — | New |

- **Confidence gate:** apply only when the relationship confidence is ≥ 0.6 (about 5 active seconds of both [A]). Below that, keep listening; after 30 s, report “not enough of both”.
- **Expected result:** most beat/vocal pairs produce 0–4 moves, all small.

## 5. Layer, ownership and compare

Beat Lock deltas live in their own layer (Deliverable 4 §2), stored in the session:

`effective = detail + macro offset + Beat Lock delta`, unless the artist has claimed that parameter.

| Event | Effect on the Beat Lock layer |
|---|---|
| Beat Lock adapts | Deltas written for unclaimed parameters; status “Beat Lock: N adjustments” with their reasons |
| Artist edits a detail control | That parameter is **claimed**: its delta is dropped; the new value is what you hear |
| Artist moves a macro | No effect on the layer (macros add on top) |
| Generate / genre change | Layer and claims cleared; Beat Lock re-adapts if the beat analysis is still valid |
| Beat Lock switch OFF | Deltas not applied (stored) → instant before/after compare, loudness-matched |
| Undo | Restores the previous layer |

## 6. Activation state machine

| State | Enter when | UI |
|---|---|---|
| **No beat** | Bus disabled | “Optional: route your beat to the Instrumental input” + how-to |
| **Waiting** | Bus enabled, silent > 1 s | “Instrumental input connected, no sound yet” |
| **Beat detected** | Beat RMS > −50 dBFS for ≥ 1 s | “Beat detected” |
| **Listening** | Beat present **and** vocal active | Progress (both analysers) |
| **Locked** | Analysis ready and confidence passes the gate → deltas applied **once**, automatically | “Beat Lock: 3 adjustments” (expandable list) + ON/OFF switch |
| **Can't tell** | 30 s without both active | “Play a part where the vocal and the beat play together” |
| **New beat?** | While Locked: the beat signature moves beyond the threshold for > 5 s [A] | **Asks**: “This sounds like a different beat. Adapt again?” Never re-adapts silently. |

A **Re-adapt** button is always available in the Beat Lock details.

## 7. Threading and safety

- **Sample capture:** on the audio thread, lock-free (existing pattern [V]).
- **Analysis and decisions:** on the message or worker thread; the functions stay pure.
- **Deltas** go to the audio thread through atomics and reach the DSP through the existing parameter smoothing. No clicks.
- **No continuous control loop exists,** by design.

## 8. Validation plan

| Level | What | Pass condition |
|---|---|---|
| Unit | Each rule, bound, withheld case, confidence gate, ownership and state transition; signature distance | Mutation-checked tests (house rule [V]) |
| Synthetic | Constructed vocal/beat pairs with known masking (e.g. a beat loud at 2–5 kHz) | The expected move, sign and bounds |
| Corpus | ≥ 30 vocal × beat pairs across the 7 genres (Deliverable 8 §2.1) | No move exceeds bounds. Vocal loudness changes ≤ 0.5 LU. Never makes the measured masking ratio worse. |
| Objective | Masking ratio in the presence band before/after; presence-to-beat ratio; integrated loudness of the vocal | Improvement or no change on ≥ 80 % of pairs [A] |
| Listening | Blind, loudness-matched: Beat Lock ON vs OFF in the full mix (vocal + beat); rate intelligibility and preference | ON preferred or tied on ≥ 70 % of pairs, never worse on intelligibility by panel majority [A] (Deliverable 8 §2.4) |
| Host | Sidechain routing to the Instrumental input in FL Studio, Ableton Live, Reaper (VST3) and Logic Pro (AU), Mac and Windows | Bus status correct; analysis runs; no audio leaks to the output [T] |

## 9. Stated limitations (shown in product help)

- GlitchKid hears the beat only on its Instrumental input. A track that already mixes vocal and beat is measured as “the vocal” [V].
- Measurements are coarse estimates of where the beat may cover the voice; they are not a judgement of the mix.
- Beat Lock adapts once; it does not follow changes in the song section by section.
- Routing steps differ per DAW. They are verified per host before a support claim (FL Studio routing is unverified today [D]).
