# GlitchKid interface: TUNE -> AUTO -> MODIFY

A calm alien-lab console (dark hull, violet edge light, lime for active things, the Kid in his saucer). One fixed 1160 x 720 layout, scaled to fit (resizable 60 %-150 %, aspect kept).
A header (three step tabs, MOTION, mascot) and a preset bar (`<` `>`, preset menu, SAVE, DELETE, A/B) are always shown. The last page and MODIFY category are remembered in the session; a first launch opens TUNE.
Every control that is not on the current page is hidden (`GlitchKidEditor::layoutContent()`), so nothing from another page can be clicked.

## 1 TUNE
KEY note pad (twelve keys) and SCALE chips (real `tune_key` / `tune_scale`), TUNING on/off, and TUNING STYLE NATURAL (50 % / 80 ms), LIGHT (75 % / 35 ms), HARD (100 % / 0 ms). A style switches Pitch Tune on (about 31 ms of reported latency) and is read back from the two values. GlitchKid does not detect your key; the page says so.

## 2 AUTO
* Eight starting profiles as cards: CLEAN & NATURAL, MODERN MELODIC RAP, HARD-TUNED RAP, DARK & ATMOSPHERIC, BRIGHT & PRESENT, SMOOTH R&B, GRITTY RAP, SPACIOUS LEAD. Selecting a card only reads about it. LOAD PROFILE applies it; BACK TO MY SOUND undoes the last load or build.
* BUILD MY VOCAL MIX: listens to the vocal while the host plays (idle / listening / analysing / ready states on the button and the mascot), then applies the selected profile plus bounded, explained adjustments. Works without a beat. WHAT CHANGED lists every change and its reason. MIX INTENSITY scales the effect amounts.
* DRY | PROCESSED is the real Bypass (delay-matched). The source line says whether the vocal and the optional Instrumental input have been analysed.

## 3 MODIFY
Four categories: PITCH & KEY (on/off, key, scale, style, strength, speed), TONE (four EQ bands, warmth), DYNAMICS (compressor, output gain, IN / OUT / GR meters), SPACE (echo, width, doubler). QUICK TWEAKS: DARKER, MORE SPACE, HARDER TUNE. Footer: BACK TO AUTO, RESET, SAVE PRESET, DRY | PROCESSED. GlitchKid has no reverb: "space" is delay, width and doubling.
Removed from the interface: the Character tab, SURPRISE ME, Alter Egos, Mutation, Stutter, Harmony, FORMANT and the 50-artist gallery. Their parameters still exist (hidden) so saved sessions load; BUILD switches them off and says so.

## Macros and quick tweaks
* SPACE: echo_on/dim_on/dbl_on (on above 2 %), echo_mix 0-60 %, echo_fb 15-45 %, echo_width 20-75 %, dim_mix 0-45 %, dbl_amount 0-40 %.
* PUNCH: comp_ratio 1-6, comp_thresh -12..-26 dB, attack 15 ms, release 110 ms, knee 6 dB, makeup min(4, 0.8 (ratio-1)) dB.
* DARKER eq_hi_gain -1.5 dB, eq_b2_gain -0.5 dB, echo_tone x0.85. MORE SPACE: SPACE +15 points. HARDER TUNE: tune_on, strength +10 %, speed -8 ms.
Fixed, bounded buttons, not language understanding. Never touched: Gain, Mix, Bypass, key, scale.

## Safety and testing
Painting and animation run on the message thread (30 Hz timer); MOTION off makes everything static. Parameter IDs, state, presets, A/B and automation are unchanged; saved preferences are `uiPage`, `uiCategory` (0-3), `uiMotion`.
`Tests/MacroTests.cpp` builds the real editor, renders every page and category at several sizes, drives the real buttons (profile select/load/back, BUILD end to end including a silent failure, DRY/PROCESSED, categories) and checks that the Character tab is gone. `GLITCHKID_SNAPSHOT_DIR=/folder` writes PNG renders; they show layout, not sound.
