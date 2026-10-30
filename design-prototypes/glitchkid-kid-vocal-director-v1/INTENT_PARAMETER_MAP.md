# KID — intent → parameter map

**Source of truth:**
* the rules are the prototype's `parse` / `updateDir` / `decide` functions (`index.html`);
* the parameter IDs, ranges and defaults come from `createLayout` (verified at `dba3bd4`, via `../vocal-lab-redesign-v1/PARAMETER_MAP.md`).

Every parameter below exists. No processor or parameter has been invented.

**Expected audible results are design intent, not listening-tested.** Values must be tuned by ear before release.

## 1. How a request becomes settings

1. **Words → intents** (the lexicon in §2). Intensity words scale each intent: "a little / slightly / a bit" = ×0.5; "much / really / very / a lot" = ×1.5.
2. **Intents → direction:** eight values between −2 and +2, with memory.
   * New intents add to the direction.
   * "too much / less / pull it back" ×0.5, "a little less" ×0.7 and "subtle" ×0.4 scale the dimensions changed last.
   * "more / further" scales the last dimensions ×1.5.
   * "start over" clears everything.
3. **Constraints and role** (§4) are added and remembered.
4. **Direction → settings** (§3). Each rule moves a value **towards a goal by k = min(1, |direction| ÷ 1.5)**, and only if the goal lies in the asked direction. Additive rules (EQ dB) add a fixed amount per unit.
5. **Validation:**
   * Allowlist: 57 IDs. Never `tune_*`, `gain`, `mix`, `lim_*` or `bypass`.
   * Clamping to the range, and rounding for choices and switches.
   * No-op rows (value unchanged) are dropped.

## 2. Lexicon (prototype)

| Intent | Words (examples) | Direction change |
|---|---|---|
| BRIGHTER | bright, brighter, crisp, shiny, sparkle | bright +1 |
| AIRY | airy, air, breathy | bright +1 and flag `air` (allows AIR LAB) |
| DARKER | dark, darker, moody, murky | bright −1 |
| WARMER | warm, warmer, smooth, analog, silky | warm +1 |
| MORE PRESENT | clear, clarity, intelligible, upfront, forward | presence +1 |
| HARDER | aggressive, punchy, energy, hard, angry, hype | punch +1 |
| SOFTER | soft, softer, gentle, relaxed | punch −1 |
| WIDER | wide, wider, width, stereo, big, huge | width +1 |
| NARROWER | narrow, mono, centred, focused | width −1 |
| MORE SPACE | atmospheric, atmosphere, spacious, space, ambient | depth +1 |
| DISTANT | distant, far, far away, background | depth +1.5, presence −1, bright −0.5 |
| DREAMY | dreamy, floaty, floating, ethereal | depth +1, width +0.5, bright +0.5, warm +0.5 |
| CLOSER | dry, drier, close, closer, intimate | depth −1, width −0.5 |
| CLEANER | clean, clean up, tidy, muddy, mud, boxy | clean +1 (and flag `mud` for mud, muddy, boxy) |
| LESS HARSH | harsh, sibilant, essy, sibilance | clean +1 and flag `ess` |
| STRANGER | experimental, weird, alien, robotic, mutate, wild | exp +1 → Mutation |
| GLITCHY | glitch, glitchy, chop, choppy, stutter | exp +1 and flag `glitch` → Stutter |
| (style) melodic | melodic | depth +0.5, width +0.5, bright +0.5 |
| (style) rap | rap, rapper, bars | punch +0.5, presence +0.5 |
| NATURAL | natural, transparent, realistic | flag `natural` (§4) |
| IMPROVE | improve, better, what it needs, don't know, fix it | Run the analysis, then §5 |
| Declined | louder, quieter, volume / autotune, tune, pitch, in key | A note only: level and tuning are yours |
| Mapped with a note | reverb | depth +1 and the note "GlitchKid has no reverb …" |

## 3. Direction → modules → parameters

Goals and caps are the prototype's starting values. **Range** is the parameter's real range (default in brackets).

| Direction | Module (stage) | Parameter · range (default) | Rule | Expected result |
|---|---|---|---|---|
| bright ± | 4-band EQ (#6) | `eq_hi_gain` · −15…15 dB (0) | +3 dB per unit | More or less top end |
| bright ± | 4-band EQ (#6) | `eq_b2_gain` · −15…15 dB (0) | +1 dB per unit (not lowered if *presence* is kept) | Presence follows the top |
| bright + with `air` | AIR LAB (#11) | `air_on` · off/on (off); `air_air` · 0…100 % (30) | On; towards 80 % | Breathy, open top |
| warm + | Saturation (#10), EQ (#6) | `sat_drive` · 0…18 dB (0); `eq_lo_gain` · −15…15 dB (0) | +1.5 dB per unit; +1 dB per unit | Density, low body |
| presence ± | 4-band EQ (#6) | `eq_b2_gain` | +1.5 dB per unit (never negative if *presence* is kept) | Words cut through, or sit back |
| punch + | Compressor (#8), Saturation (#10) | `comp_ratio` · 1…20 :1 (1); `comp_attack` · 0.1…100 ms (10); `comp_thresh` · −60…0 dB (−18); `sat_drive` | Ratio towards 6:1; attack towards 2 ms; threshold −3 dB per unit; drive +1.5 dB per unit | Steadier, harder, more grit |
| punch − | Compressor (#8) | `comp_ratio`, `comp_attack` | Ratio towards 1:1; attack towards 40 ms | Softer front, more dynamics |
| width + | Doubler (#5) | `dbl_on` (off); `dbl_width` · 0…100 % (70); `dbl_amount` · 0…100 % (50) | On; width towards 92 %; amount towards 45 % (capped at 28 % if *front* is kept) | Copies left and right |
| width + | Dimension (#14) | `dim_on` (off); `dim_width` · 0…100 % (60); `dim_mix` · 0…100 % (100) | On; width towards 88 %; mix towards 55 % (capped at 30 % if *front* is kept) | Width and movement |
| width − | Doubler, Dimension | `dbl_on`, `dbl_amount`, `dim_on` | At −0.75 or below: off; otherwise doubles quieter | Narrower, centred |
| depth + | Echo (#13) | `echo_on` (off); `echo_sync` (on); `echo_div` · 11 choices (1/8); `echo_mix` · 0…100 % (30); `echo_fb` · 0…90 % (35); `echo_tone` · 400…20000 Hz (6000) | On, synced; 1/8 if echo was off (1/4 for ad-libs); mix towards 32 % (40 % for ad-libs; capped at 12 % if *front*); feedback towards 55 %; tone towards 2.2 kHz | Space behind the voice; darker repeats sit further back |
| depth + (not *front*) | Dimension (#14) | `dim_on`, `dim_mix` | On; mix towards 55 % | Haze |
| depth + (not *presence*) | 4-band EQ (#6) | `eq_b2_gain` | −0.75 dB per unit | The voice steps back |
| depth − | Echo, Dimension | `echo_on`, `dim_on` | Off (Dimension stays if width is also asked) | Close and direct |
| clean + | CLEAN (#7 corrective, #9 de-esser) | `clean_on` (off); `clean_amount` · 0…100 % (50) | On; towards 85 % | Rumble, mud, resonances and S handled by CLEAN's own detector |
| clean + with `mud` | 4-band EQ (#6), CLEAN | `eq_b1_freq` · 100…5000 Hz (400); `eq_b1_gain` (0); `clean_tone` · 0…100 % (100) | Body band to 300 Hz; cut by 2 dB per unit; corrective EQ at 100 % | Less mud |
| clean + with `ess` | CLEAN de-esser (#9) | `clean_ess` · 0…100 % (100) | 100 % | Softer S |
| exp + | Mutation (#4) | `mut_on` (off); `mut_amount` (40); `mut_character` (50); `mut_blend` (100) | On; amount towards 80 %; character towards 75 %; blend 40 % + 20 % per unit if it was off | The voice bends into something new |
| exp + with `glitch` | Stutter (#12) | `stutter_on` (off); `stutter_rate` · 1/4…1/32 (1/8); `stutter_mix` (100) | On; 1/16; mix 30 % + 20 % per unit if it was off | Tempo-locked slices |

Not used by KID's rules today: Formant (#2) and Harmony (#3). Harmony is only switched *off* by NATURAL. They stay manual in CHARACTER because their results are too personal to propose by default. This can be revisited.

## 4. Constraints, role and natural

| Constraint (words) | Effect |
|---|---|
| **Front** ("keep the vocal upfront", "… but keep it upfront") | Echo mix ≤ 12 %, Doubler amount ≤ 28 %, Dimension mix ≤ 30 %. Also sets *presence*. |
| **Presence** ("keep the presence", "keep the clarity") | `eq_b2_gain` is never lowered. A negative presence direction is reset to 0. |
| **Character** ("keep the character") | No Lab parameter is proposed. |
| **Warmth** ("keep the warmth / body / low end") | `eq_lo_gain` is not lowered by warmth rules. |
| **Role ad-lib** ("ad-libs", "backing") | Echo 1/4 throws, echo mix goal 40 %, `eq_lo_gain` −3 dB, `eq_hi_gain` −1.5 dB |
| **Natural** | `sat_drive` ≤ 2 dB, echo mix ≤ 12 %, Lab effects that are on are switched off (unless *character* is kept) |

**Conflicts are resolved by constraint, not by order.** "Wide but keep it upfront" widens with the Doubler and Dimension inside the front caps. It adds no echo beyond 12 % and never lowers Presence (tested).

## 5. Analysis-led requests ("improve it", cleaning)

KID reads only fields that exist (FACT, repo: `VocalAnalysis`, `CleanLab::Applied`):

| Measured field | Rule (prototype thresholds, to be tuned) | Direction |
|---|---|---|
| `sibilanceIndex` > 0.25 | Harsh S | clean +1, flag `ess` |
| `bandFraction[1]` (120–500 Hz) > 0.35 | Mud | clean +1, flag `mud` |
| `dynamicRangeDb` > 12 dB | The level jumps about | punch +0.5 |
| `bandFraction[3] + [4]` (> 2 kHz) < 0.25 | Dull top | bright +0.5 |
| `clippedSamples` > 0 | The recording clips | A note only: "lower the recording level". KID never fixes clipping with Gain. |

**Not measured today, so never used:** stereo width or correlation (the analyser and display taps are mono), pitch range and stability (Pitch Tune reports only the current note while it is on), LUFS loudness.

## 6. Tests that guard this map (prototype suite)

The prototype suite has 51 checks, all passing. Among them:
* the allowlist size and exclusions;
* no forbidden parameter proposed across 21 phrases;
* "wider" raises widths;
* "subtle" reduces the same rows;
* "keep upfront" caps and keeps Presence;
* "a little less" stays darker but less so;
* "keep the presence" drops the Presence cut;
* "wide but keep it upfront";
* ad-lib throws;
* analysis-led "improve it";
* manual override;
* unknown words.
