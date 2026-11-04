# Intent → parameter map (v2)

How KID turns words into changes to GlitchKid's **own** parameters. Every parameter named here exists in `Source/PluginProcessor.cpp` (`createLayout`) with the range shown in §5.

- **Existing vocabulary** is taken from `Source/Kid/KidDirector.cpp` (`parse`, the `OPS` / `Keep` / scale tables).
- **New vocabulary** is marked **NEW**. It is used by the prototype and needs adding to `KidDirector` (`IMPLEMENTATION_PLAN.md`).
- **Numbers.** The values below are the **prototype's** rule set, modelled on `KidDirector::decide`. They are similar in direction and order of magnitude, not identical. Production values must come from the real engine and its tests.

## 1. Directions (dimensions)

| Dimension | Words (existing unless marked) | What the prototype changes (stage) | Notes |
|---|---|---|---|
| **bright** (−1 = darker) | darker, dark, moody, murky · brighter, bright, airy, crisp, shiny | Air shelf `eq_hi_gain` ±3 dB per unit (6) · Presence `eq_b2_gain` ±0.8 dB (6) · Echo tone `echo_tone` ×0.8 / ×1.2 if echo is on (13) | With *keep it upfront*, darker does not cut presence |
| **warm** | warmer, warm, fuller, thicker | Low `eq_lo_gain` +1.5 dB (6) · Warmth `sat_drive` +1.5 dB (10) | No saturation under *keep the clean sound* |
| **presence** | clear, clearer, clarity, upfront, up front, in front, forward, present | Presence `eq_b2_gain` +1.8 dB (6) | — |
| **punch** | aggressive, punchy, punch, energy, harder, hype · softer, gentler, smoother (−0.6) | Ratio `comp_ratio` (start at 1.5 if off) +2 · Threshold `comp_thresh` −5 dB · Attack `comp_attack` → ≤ 6 ms · Makeup `comp_makeup` +1.8 dB (8) · Warmth `sat_drive` +2 dB (10) | Grit is skipped under *keep the clean sound* |
| **width** | wider, wide, width, stereo, bigger, huge · narrower, mono, tighter | Doubler `dbl_on` on, `dbl_amount` starts at 25 % (5) · `dbl_width` +14 (5) · Dimension `dim_on` on, `dim_mix` starts at 20 %, +16 (+8 when upfront) (14) · `dim_width` +14 (14) · Echo width `echo_width` +20 if echo is on (13) | Effects switched on start low, so it's a touch, not a wash |
| **depth** | distant (also −0.5 bright, −1 presence), far, background · more space, spacious, echo, ambient, atmospheric, reverb · closer, intimate, drier (−1) | Echo `echo_on` on, `echo_mix` starts at 12 %, +10 per unit (+14 for ad-libs), `echo_sync` on, `echo_div` 1/8 (1/4 for ad-libs), `echo_fb` +8 (13) · Dimension on, `dim_mix` +10 (14) | GlitchKid has no reverb; space is echo + Dimension (the engine says so) |
| **clean** | clean it up, clean up, cleaner, clean my vocal | CLEAN `clean_on` on, `clean_amount` 50 + 15 per unit (7 & 9) | — |
| **exp** | experimental, weird, alien, strange, robotic, crazy, wild | Mutation `mut_on` on, `mut_amount` 15 + 30 per unit, `mut_blend` 30 + 30 per unit (4) · Dimension depth `dim_depth` +15 if ≥ 0.6 and Dimension is on (14) | — |
| **NEW** psychedelic | psychedelic, trippy, dreamy | exp +0.6, width +0.6, depth +0.8 | Not in `KidDirector` today |
| **NEW** character | more character, add (some) character | exp +0.5, warm +0.4 | Not in `KidDirector` today; it only has *keep the character* as a lock |

Dimensions are clamped to −2 … +2. Every value is clamped to the parameter's real range; a row at a limit is tagged *AT ITS LIMIT*.

## 2. Modifiers

| Kind | Words (existing) | Effect |
|---|---|---|
| Amount | a little, slightly, a bit, a touch | × 0.5 on this sentence's directions |
| Amount | very, much, way more, a lot | × 1.5 |
| Refinement | a little less, bit less | × 0.7 on the named dimensions, or all of them if none is named |
| Refinement | too much, less, tone it down, pull it back, not so much, too far, too strong | × 0.5 |
| Refinement | subtle, subtler | × 0.4 |
| Role | ad-libs, backing | Longer throws (1/4), more echo mix |

## 3. Constraints ("keep …")

| Words | Lock | Effect in the proposal |
|---|---|---|
| keep it / the vocal upfront · but keep it upfront · keep the presence | front + presence | No presence cut; echo mix capped at 22 %; Dimension mix capped at 35 % |
| keep the character | char | Saturation never reduced; the clean-up says it leaves the character alone |
| keep the warmth / body | warm | No low-end cut |
| **NEW** keep the clean sound | clean | CLEAN stays as it is (including a clean-up already proposed); no saturation is added |

Locks persist across follow-ups and after Apply. Cancel forgets them.

## 4. Never touched (allowlist, `KidDirector::isAllowed`)

`gain`, `mix`, `lim_on`, `lim_ceiling`, `lim_drive`, `lim_release`, `bypass` — and Pitch Tune's `tune_key` / `tune_scale` except from a reliable key estimate. The prototype never writes the key or scale. KID's reply says *Kept as yours: gain, mix and the output limiter*.

## 4a. Worked examples (recorded from the prototype, starting from Factory Default)

Each block lists every row of the proposal: parameter (id), current → proposed. Follow-ups are revisions of the same proposal. Recorded by driving the prototype in headless Chromium.

**Make this vocal darker and more aggressive.  [revision 1]**

```
Presence (eq_b2_gain): 0.0 dB → -0.8 dB
Air shelf (eq_hi_gain): 0.0 dB → -3.0 dB
Threshold (comp_thresh): -18.0 dB → -23.0 dB
Ratio (comp_ratio): 1.0:1 → 3.5:1
Attack (comp_attack): 10 ms → 6.0 ms
Makeup (comp_makeup): 0.0 dB → +1.8 dB
Warmth (sat_drive): 0.0 dB → +2.0 dB
```

**A little less.  [revision 2]**

```
Presence (eq_b2_gain): 0.0 dB → -0.6 dB
Air shelf (eq_hi_gain): 0.0 dB → -2.1 dB
Threshold (comp_thresh): -18.0 dB → -21.5 dB
Ratio (comp_ratio): 1.0:1 → 2.9:1
Attack (comp_attack): 10 ms → 6.0 ms
Makeup (comp_makeup): 0.0 dB → +1.3 dB
Warmth (sat_drive): 0.0 dB → +1.4 dB
```

**Keep the vocal upfront.  [revision 3]**

```
Air shelf (eq_hi_gain): 0.0 dB → -2.1 dB
Threshold (comp_thresh): -18.0 dB → -21.5 dB
Ratio (comp_ratio): 1.0:1 → 2.9:1
Attack (comp_attack): 10 ms → 6.0 ms
Makeup (comp_makeup): 0.0 dB → +1.3 dB
Warmth (sat_drive): 0.0 dB → +1.4 dB
```

**Make it darker.  [revision 4]**

```
Air shelf (eq_hi_gain): 0.0 dB → -5.1 dB
Threshold (comp_thresh): -18.0 dB → -21.5 dB
Ratio (comp_ratio): 1.0:1 → 2.9:1
Attack (comp_attack): 10 ms → 6.0 ms
Makeup (comp_makeup): 0.0 dB → +1.3 dB
Warmth (sat_drive): 0.0 dB → +1.4 dB
```

**Make it wider but keep it upfront.  [revision 1]**

```
Doubler (dbl_on): Off → On
Doubler amount (dbl_amount): 50% → 25%
Doubler width (dbl_width): 70% → 84%
Dimension (dim_on): Off → On
Dimension width (dim_width): 60% → 74%
Dimension mix (dim_mix): 100% → 28%
```

**That’s too much width.  [revision 2]**

```
Doubler (dbl_on): Off → On
Doubler amount (dbl_amount): 50% → 25%
Doubler width (dbl_width): 70% → 77%
Dimension (dim_on): Off → On
Dimension width (dim_width): 60% → 67%
Dimension mix (dim_mix): 100% → 24%
```

**Clean it up but keep the character.  [revision 1]**

```
CLEAN (clean_on): Off → On
Clean amount (clean_amount): 50% → 65%
```

**Keep the clean sound but add more character.  [revision 2]**

```
Mutation (mut_on): Off → On
Amount (mut_amount): 40% → 30%
Blend (mut_blend): 100% → 45%
Low (eq_lo_gain): 0.0 dB → +0.6 dB
CLEAN (clean_on): Off → On
Clean amount (clean_amount): 50% → 65%
```

**Give my ad-libs a distant, psychedelic sound.  [revision 1]**

```
Mutation (mut_on): Off → On
Amount (mut_amount): 40% → 33%
Blend (mut_blend): 100% → 48%
Doubler (dbl_on): Off → On
Doubler amount (dbl_amount): 50% → 25%
Doubler width (dbl_width): 70% → 78%
Presence (eq_b2_gain): 0.0 dB → -2.2 dB
Air shelf (eq_hi_gain): 0.0 dB → -1.5 dB
Echo (echo_on): Off → On
Division (echo_div): 1/8 → 1/4
Feedback (echo_fb): 35% → 51%
Echo mix (echo_mix): 30% → 40%
Dimension (dim_on): Off → On
Dimension width (dim_width): 60% → 68%
Dimension depth (dim_depth): 50% → 59%
Dimension mix (dim_mix): 100% → 50%
```

**I don’t know what it needs. Help me improve it.  [revision 1]**

```
Presence (eq_b2_gain): 0.0 dB → +0.9 dB
CLEAN (clean_on): Off → On
Clean amount (clean_amount): 50% → 65%
Threshold (comp_thresh): -18.0 dB → -20.5 dB
Ratio (comp_ratio): 1.0:1 → 2.5:1
Attack (comp_attack): 10 ms → 6.0 ms
Makeup (comp_makeup): 0.0 dB → +0.9 dB
Warmth (sat_drive): 0.0 dB → +1.0 dB
```

## 5. Parameter reference (all 68 sound parameters)

Generated from the prototype's parameter table, which was checked against `createLayout`.
- **Bypass** is the DRY switch.
- **The 7 `master_*` parameters** belong to the Mastering Lab and are out of scope here.
- **Stage** is the real processing position.
- **KID** = whether KID may change it.
- **Profile load** = whether a factory profile *sets* it or *keeps* the user's value.

| ID | Name in v2 | Range | Unit | Default | Stage | Workspace | KID | Profile load |
|---|---|---|---|---|---|---|---|---|
| `gain` | Gain | -60 … 12 | dB | 0.0 dB | 15 | SPACE › Output | never | kept |
| `eq_lo_freq` | Low freq | 20 … 500 (skew centre 100) | Hz | 100 Hz | 6 | SHAPE › TONE | allowed | set |
| `eq_lo_gain` | Low | -15 … 15 | dB | 0.0 dB | 6 | SHAPE › TONE | allowed | set |
| `eq_b1_freq` | Body freq | 100 … 5000 (skew centre 700) | Hz | 400 Hz | 6 | SHAPE › TONE | allowed | set |
| `eq_b1_gain` | Body | -15 … 15 | dB | 0.0 dB | 6 | SHAPE › TONE | allowed | set |
| `eq_b1_q` | Body Q | 0.3 … 10 (skew centre 1.5) | — | Q 1.00 | 6 | SHAPE › TONE | allowed | set |
| `eq_b2_freq` | Presence freq | 500 … 16000 (skew centre 3000) | Hz | 3 kHz | 6 | SHAPE › TONE | allowed | set |
| `eq_b2_gain` | Presence | -15 … 15 | dB | 0.0 dB | 6 | SHAPE › TONE | allowed | set |
| `eq_b2_q` | Presence Q | 0.3 … 10 (skew centre 1.5) | — | Q 1.00 | 6 | SHAPE › TONE | allowed | set |
| `eq_hi_freq` | Air shelf freq | 1000 … 18000 (skew centre 6000) | Hz | 8 kHz | 6 | SHAPE › TONE | allowed | set |
| `eq_hi_gain` | Air shelf | -15 … 15 | dB | 0.0 dB | 6 | SHAPE › TONE | allowed | set |
| `comp_thresh` | Threshold | -60 … 0 | dB | -18.0 dB | 8 | SHAPE › DYNAMICS | allowed | set |
| `comp_ratio` | Ratio | 1 … 20 (skew centre 4) | :1 | 1.0:1 | 8 | SHAPE › DYNAMICS | allowed | set |
| `comp_attack` | Attack | 0.1 … 100 (skew centre 10) | ms | 10 ms | 8 | SHAPE › DYNAMICS | allowed | set |
| `comp_release` | Release | 10 … 1000 (skew centre 100) | ms | 100 ms | 8 | SHAPE › DYNAMICS | allowed | set |
| `comp_knee` | Knee | 0 … 24 | dB | +6.0 dB | 8 | SHAPE › DYNAMICS | allowed | set |
| `comp_makeup` | Makeup | 0 … 24 | dB | 0.0 dB | 8 | SHAPE › DYNAMICS | allowed | set |
| `sat_drive` | Warmth | 0 … 18 | dB | 0.0 dB | 10 | SHAPE › TONE | allowed | set |
| `stutter_on` | Stutter | off / on | — | Off | 12 | CHARACTER | allowed | set |
| `stutter_rate` | Slice | 0=1/4, 1=1/8, 2=1/16, 3=1/32 | — | 1/8 | 12 | CHARACTER | allowed | set |
| `stutter_mix` | Stutter mix | 0 … 100 | % | 100% | 12 | CHARACTER | allowed | set |
| `echo_on` | Echo | off / on | — | Off | 13 | SPACE | allowed | set |
| `echo_sync` | Sync to tempo | off / on | — | On | 13 | SPACE | allowed | set |
| `echo_time` | Echo time | 1 … 2000 (skew centre 300) | ms | 350 ms | 13 | SPACE | allowed | set |
| `echo_div` | Division | 0=1/2, 1=1/4, 2=1/4 dotted, 3=1/4 triplet, 4=1/8, 5=1/8 dotted, 6=1/8 triplet, 7=1/16, 8=1/16 dotted, 9=1/16 triplet, 10=1/32 | — | 1/8 | 13 | SPACE | allowed | set |
| `echo_fb` | Feedback | 0 … 90 | % | 35% | 13 | SPACE | allowed | set |
| `echo_tone` | Echo tone | 400 … 20000 (skew centre 3000) | Hz | 6 kHz | 13 | SPACE | allowed | set |
| `echo_width` | Echo width | 0 … 100 | % | 0% | 13 | SPACE | allowed | set |
| `echo_mix` | Echo mix | 0 … 100 | % | 30% | 13 | SPACE | allowed | set |
| `tune_on` | Pitch Tune | off / on | — | Off | 1 | SHAPE › TUNE | allowed | kept |
| `tune_key` | Key | 0=C, 1=C#, 2=D, 3=D#, 4=E, 5=F, 6=F#, 7=G, 8=G#, 9=A, 10=A#, 11=B | — | C | 1 | SHAPE › TUNE | never | kept |
| `tune_scale` | Scale | 0=Chromatic, 1=Major, 2=Natural minor, 3=Harmonic minor, 4=Major pentatonic, 5=Minor pentatonic | — | Chromatic | 1 | SHAPE › TUNE | never | kept |
| `tune_strength` | Strength | 0 … 100 | % | 100% | 1 | SHAPE › TUNE | allowed | kept |
| `tune_speed` | Speed | 0 … 400 (skew centre 40) | ms | 20 ms | 1 | SHAPE › TUNE | allowed | kept |
| `formant_shift` | Formant shift | -12 … 12 | st | 0.0 st | 2 | CHARACTER | allowed | set |
| `mut_on` | Mutation | off / on | — | Off | 4 | CHARACTER | allowed | set |
| `mut_amount` | Amount | 0 … 100 | % | 40% | 4 | CHARACTER | allowed | set |
| `mut_character` | Character | 0 … 100 | % | 50% | 4 | CHARACTER | allowed | set |
| `mut_blend` | Blend | 0 … 100 | % | 100% | 4 | CHARACTER | allowed | set |
| `harm_on` | Harmony | off / on | — | Off | 3 | CHARACTER | allowed | set |
| `harm_interval` | Interval | 0=Octave DOWN (-12), 1=Fifth DOWN (-7), 2=Major 3rd DOWN (-4), 3=Minor 3rd DOWN (-3), 4=Minor 3rd UP (+3), 5=Major 3rd UP (+4), 6=Fourth UP (+5), 7=Fifth UP (+7), 8=Octave UP (+12) | — | Fifth UP (+7) | 3 | CHARACTER | allowed | set |
| `harm_level` | Harmony level | -24 … 0 | dB | -9.0 dB | 3 | CHARACTER | allowed | set |
| `dbl_on` | Doubler | off / on | — | Off | 5 | SPACE | allowed | set |
| `dbl_amount` | Doubler amount | 0 … 100 | % | 50% | 5 | SPACE | allowed | set |
| `dbl_width` | Doubler width | 0 … 100 | % | 70% | 5 | SPACE | allowed | set |
| `dbl_detune` | Doubler detune | 0 … 25 | ct | 8.0 ct | 5 | SPACE | allowed | set |
| `dbl_timing` | Doubler timing | 5 … 45 | ms | 18 ms | 5 | SPACE | allowed | set |
| `dim_on` | Dimension | off / on | — | Off | 14 | SPACE | allowed | set |
| `dim_width` | Dimension width | 0 … 100 | % | 60% | 14 | SPACE | allowed | set |
| `dim_detune` | Dimension detune | 0 … 20 | ct | 6.0 ct | 14 | SPACE | allowed | set |
| `dim_depth` | Dimension depth | 0 … 100 | % | 50% | 14 | SPACE | allowed | set |
| `dim_mix` | Dimension mix | 0 … 100 | % | 100% | 14 | SPACE | allowed | set |
| `mix` | Mix | 0 … 100 | % | 100% | 16 | SPACE › Output | never | kept |
| `air_on` | AIR LAB | off / on | — | Off | 11 | SHAPE › TONE | allowed | kept |
| `air_presence` | Presence lift | 0 … 100 | % | 30% | 11 | SHAPE › TONE | allowed | kept |
| `air_air` | Air lift | 0 … 100 | % | 30% | 11 | SHAPE › TONE | allowed | kept |
| `air_link` | Link | off / on | — | On | 11 | SHAPE › TONE | allowed | kept |
| `air_mix` | AIR mix | 0 … 100 | % | 100% | 11 | SHAPE › TONE | allowed | kept |
| `air_trim` | AIR trim | -12 … 12 | dB | 0.0 dB | 11 | SHAPE › TONE | allowed | kept |
| `clean_on` | CLEAN | off / on | — | Off | 7 & 9 | SHAPE › CLEAN | allowed | kept |
| `clean_amount` | Clean amount | 0 … 100 | % | 50% | 7 & 9 | SHAPE › CLEAN | allowed | kept |
| `clean_tone` | Corrective EQ | 0 … 100 | % | 100% | 7 & 9 | SHAPE › CLEAN | allowed | kept |
| `clean_res` | Resonance | 0 … 100 | % | 100% | 7 & 9 | SHAPE › CLEAN | allowed | kept |
| `clean_ess` | De-esser | 0 … 100 | % | 100% | 9 | SHAPE › CLEAN | allowed | kept |
| `lim_on` | Output Limiter | off / on | — | Off | 17 | SPACE › Output | never | kept |
| `lim_ceiling` | Ceiling | -12 … -0.1 | dBFS | -1.0 dBFS | 17 | SPACE › Output | never | kept |
| `lim_drive` | Limiter drive | 0 … 24 | dB | 0.0 dB | 17 | SPACE › Output | never | kept |
| `lim_release` | Limiter release | 10 … 500 | ms | 80 ms | 17 | SPACE › Output | never | kept |
