# Deliverable 5 — Genre profile specification

## What a genre is

A genre is a **starting point that adapts to the voice**. It is not a fixed chain and not a promise of a result.

- **Mechanism (unchanged).** Factory Default + the genre's targets, then bounded adjustments from the vocal's measurements (`MixEngine`'s existing rules [V], extended in §3).
- **Ownership.** Pitch Tune, Gain and Mix are never set by a genre [V].
- **Naming.** Genre names describe sound and style; no artist names.

## 1. The launch library (7)

| Genre | Built from (existing profile [V]) | For |
|---|---|---|
| **POP** | BRIGHT & PRESENT | Sung pop and pop-leaning vocals |
| **R&B** | SMOOTH R&B | Soulful, sung R&B |
| **MELODIC RAP** | MODERN MELODIC RAP | Sung or rap-sung hooks and verses |
| **TRAP** | HARD-TUNED RAP | Modern trap, tight and upfront |
| **RAP** | GRITTY RAP | Raw, dense, mostly dry rap |
| **ATMOSPHERIC** | DARK & ATMOSPHERIC + SPACIOUS LEAD (merged) | Moody, wide, ambient vocals |
| **NATURAL** | CLEAN & NATURAL | Acoustic, singer-songwriter, transparent polish |

**Why seven:**

- They cover the target customer's main styles with the profiles that already exist.
- Merging the two “space” profiles keeps the list short.
- If the listening budget is tight, **ATMOSPHERIC** and **NATURAL** are the first candidates to ship later, leaving 5.

## 2. Profile definitions

Values marked [V] are today's exact `Profiles.inc` numbers (the starting point). Everything else, including all reverb, ducking and CLEAN values, is a **proposed** target [A] to be tuned by listening (§4).

> **Status:** nothing in this section has been listened to in the V1 form. Treat every number as the first draft of a tuning session.

### POP — “Bright, upfront, polished”

| Aspect | Target |
|---|---|
| Vocal character | Clear, forward, glossy; words always intelligible |
| Tonal direction | Low shelf −1.5 dB @ 120 Hz; Body −1.5 dB @ 400 Hz; Presence +3 dB @ 4 kHz; Air +3 dB @ 11 kHz [V] |
| Dynamics | 3.5:1 @ −20 dB, attack 6 ms, release 90 ms, knee 5 dB, makeup 6.5 dB [V]: consistent level, consonants intact |
| Presence | High; AIR LAB presence 35 % [A] |
| Space | Plate 1.6 s, pre-delay 40 ms, mix 12 % [A]; Echo 1/16, fb 10 %, tone 7 kHz, width 20, mix 7 % [V]; Doubler 12 %, width 50 [V]; ducking 30 [A] |
| Warmth | 1.5 dB [V] |
| CLEAN | On, 55 % [A] |
| Pitch correction | Suggest **Light** when the artist enables Tune |
| Beat Lock | Full presence moves allowed; space reduced when crowded (× 0.85) |

### R&B — “Warm, soft, gently wide”

| Aspect | Target |
|---|---|
| Vocal character | Smooth, intimate warmth, silky top |
| Tonal direction | Low +1.5 dB @ 120 Hz; Body −1 dB @ 300 Hz; Presence +1 dB @ 3.5 kHz; Air +1.5 dB @ 9.5 kHz [V] |
| Dynamics | 2.5:1 @ −17 dB, attack 18 ms, release 180 ms, knee 12 dB, makeup 4 dB [V]: gentle, breathing |
| Presence | Moderate; AIR LAB presence 25 % [A] |
| Space | Plate/Hall 2.2 s, pre-delay 30 ms, mix 18 % [A]; Echo 1/4, fb 28 %, tone 3.5 kHz, width 45, mix 18 % [V]; Dimension width 45, depth 40, mix 22 % [V]; Doubler 25 %, width 75 [V]; ducking 25 [A] |
| Warmth | 1.2 dB [V] |
| CLEAN | On, 45 % [A] |
| Pitch correction | Suggest **Natural** |
| Beat Lock | Presence moves allowed; keep the space character (reduce at most × 0.85) |

### MELODIC RAP — “Glossy, smooth, a little space”

| Aspect | Target |
|---|---|
| Vocal character | Polished melodic flow, upfront hooks |
| Tonal direction | Low +1 dB @ 100 Hz; Body −1.5 dB @ 350 Hz; Presence +2 dB @ 4 kHz; Air +2 dB @ 10 kHz [V] |
| Dynamics | 3:1 @ −19 dB, attack 10 ms, release 110 ms, knee 8 dB, makeup 6.5 dB [V] |
| Presence | High; AIR LAB presence 30 % [A] |
| Space | Plate/Room 1.4 s, pre-delay 30 ms, mix 12 % [A]; Echo 1/8 dotted, fb 22 %, tone 4.5 kHz, width 40, mix 14 % [V]; Dimension width 50, mix 22 % [V]; Doubler 20 %, width 70 [V]; ducking 35 [A] |
| Warmth | 2.5 dB [V] |
| CLEAN | On, 50 % [A] |
| Pitch correction | Suggest **Light** (Hard as an alternative) |
| Beat Lock | Full presence moves; space × 0.85 when crowded |

### TRAP — “Tight, forward, built for hard tuning”

| Aspect | Target |
|---|---|
| Vocal character | Tight, dry-ish, aggressive and forward |
| Tonal direction | Low −1 dB @ 100 Hz; Body −2 dB @ 300 Hz; Presence +2.5 dB @ 3.2 kHz; Air +2 dB @ 9.5 kHz [V] |
| Dynamics | 4:1 @ −21 dB, attack 5 ms, release 80 ms, knee 4 dB, makeup 7.5 dB [V]: firm control |
| Presence | High; AIR LAB presence 30 % [A] |
| Space | Room 0.9 s, mix 6 % [A]; Echo 1/16, fb 12 %, tone 6 kHz, width 20, mix 8 % [V]; Doubler 15 %, width 60 [V]; ducking 40 [A] |
| Warmth | 3 dB [V] |
| CLEAN | On, 55 % [A] |
| Pitch correction | Suggest **Hard** (strength 100 %, speed 0 ms). The genre's character depends on it, so the TUNE stage says so. **Generate never switches Tune on.** |
| Beat Lock | Full presence and clarity moves; ducking deepened on dense beats [R] |

### RAP — “Raw, dense, in your face”

| Aspect | Target |
|---|---|
| Vocal character | Raw, dense, very upfront, mostly dry |
| Tonal direction | Low +2 dB @ 100 Hz; Body −1 dB @ 450 Hz; Presence +2.5 dB @ 2.8 kHz; Air +0.5 dB @ 8 kHz [V] |
| Dynamics | 5:1 @ −24 dB, attack 4 ms, release 70 ms, knee 3 dB, makeup 6.5 dB [V]: heavy, consistent |
| Presence | High, from the bell, not air; AIR LAB presence 20 % [A] |
| Space | Room 0.6 s, mix 4 % [A]; no Echo [V]; Doubler 25 %, width 70 [V]; ducking 20 [A] |
| Warmth | 7 dB [V]: grit is part of the sound (Build My Mix halves it on bright or clipping vocals [V]) |
| CLEAN | On, 50 % [A] |
| Pitch correction | Suggest none (Tune off) |
| Beat Lock | Presence and clarity moves; space moves rarely apply (little space) |

### ATMOSPHERIC — “Deep, wide, moody”

| Aspect | Target |
|---|---|
| Vocal character | Dark, wide, floating; space is part of the identity |
| Tonal direction | Low +2 dB @ 90 Hz; Body −2.5 dB @ 300 Hz; Presence −0.5 dB @ 2.5 kHz; Air −3.5 dB @ 6.5 kHz [V, from DARK & ATMOSPHERIC]. Spacious Lead's brighter top is the “+TONE” direction. |
| Dynamics | 3:1 @ −20 dB, attack 14 ms, release 160 ms, knee 8 dB, makeup 9 dB [V] |
| Presence | Lower by design; PRESENCE macro is the user's lever; AIR LAB presence 20 % [A] |
| Space | Hall 3.5 s, pre-delay 60 ms, mix 25 % [A]; Echo 1/4 dotted, fb 40 %, tone 2.2 kHz, width 75, mix 28 % [V]; Dimension width 65, depth 55, mix 35 % [V]; Doubler 35 %, width 85 [V]; ducking 20 [A] |
| Warmth | 3.5 dB [V] |
| CLEAN | On, 45 % [A] |
| Pitch correction | Suggest **Light** |
| Beat Lock | Space reduction limited (× 0.9) so the character survives; presence moves allowed |

### NATURAL — “Transparent, honest, lightly polished”

| Aspect | Target |
|---|---|
| Vocal character | The singer as recorded, cleaned and balanced |
| Tonal direction | Body −1 dB @ 300 Hz; Presence +1 dB @ 3.5 kHz; Air +1 dB @ 11 kHz [V] |
| Dynamics | 2.5:1 @ −18 dB, attack 15 ms, release 140 ms, knee 10 dB, makeup 3.5 dB [V] |
| Presence | Natural; AIR LAB presence 20 % [A] |
| Space | Room 1.2 s, pre-delay 20 ms, mix 10 % [A]; no Echo; no Doubler [V]; ducking 15 [A] |
| Warmth | 0 [V] |
| CLEAN | On, 40 % [A] |
| Pitch correction | Suggest **Natural** |
| Beat Lock | Gentle: presence moves at half size |

**FINISH for every genre:**

- Limiter on, ceiling −1 dBFS.
- Gain and Mix untouched [V].
- Match level as the user set it.

## 3. Adapting to the voice

These rules run after the genre targets. All are bounded and deterministic.

### Existing `MixEngine` rules [V]

- Compressor threshold follows the vocal's RMS, so the compression amount matches the genre's intent.
- Ratio: +0.5 for wide dynamics; × 0.75 for already-compressed takes.
- Makeup reduced when peaks are hot.
- Clipping input: saturation halved, makeup −2 dB.
- Strong low end: low shelf −2 dB.
- Heavy low-mids: Body −1.5 dB.
- Weak presence: +1 dB.
- Strong sibilance: high shelf down (or left to CLEAN).
- Dark top: +1 dB.
- Already bright: saturation halved.
- When CLEAN is on, the corrections it covers are skipped.

### New rules [A] (each needs a unit test and a listening check)

1. **CLEAN amount** follows the measured problems: +10 points for strong low-mid or sibilance, −10 for a clean take.
2. **Reverb pre-delay** follows tempo when the host BPM is known: about a 1/64 note, clamped 15–80 ms, so tails start after the consonant.
3. **AIR LAB air** is reduced on vocals already bright above 5 kHz (air share > 0.12, the same threshold as today's saturation rule).
4. **Ducking:** +10 for fast, dense delivery (high onset rate [R]).
5. **Register:** a deep voice (low median pitch, from live pitch evidence) gets the low shelf 0.5 dB lower; a high voice gets Air 0.5 dB lower [R].

## 4. Tuning process and acceptance

1. **Corpus.** Per genre, at least 6 vocals covering the brief's list: clean studio, noisy home, bright, dark, deep, high, soft, aggressive. Plus 3 instrumentals per genre (Deliverable 8 §2.1).
2. **References.** For each vocal, a mix by an experienced engineer using standard tools, and the untreated vocal.
3. **Iterate** targets and adaptation bounds by listening, level-matched at all times.
4. **Accept a genre** when the blind, level-matched panel results meet the launch gate (Deliverable 8 §2.4) for that genre's corpus.
5. **Freeze.** Record the final values in `Profiles.inc` (generated by `tools/gen_profiles.py` [V]) with a changelog of what changed and why.

## 5. Migration from today's profiles

| Today | V1 |
|---|---|
| BRIGHT & PRESENT | POP |
| SMOOTH R&B | R&B |
| MODERN MELODIC RAP | MELODIC RAP |
| HARD-TUNED RAP | TRAP |
| GRITTY RAP | RAP |
| DARK & ATMOSPHERIC | ATMOSPHERIC |
| SPACIOUS LEAD | ATMOSPHERIC; preset name kept as “(older profile)” |
| CLEAN & NATURAL | NATURAL |

A session or preset naming an old profile shows the new genre name; its stored values are unchanged.
