# GlitchKid interface: the 8-bit level select (CLEAN LAB, BEAT LOCK, BUILD MY VOCAL MIX, FINAL TOUCH)

An 8-bit cyberpunk interface on near-black, using the locked palette only: Void Black `#08060D`, Deep Graphite `#15111B`, Ultraviolet `#C026FF` (selection, focus, the main action), Cyber Pink `#FF3DAE` (real processing activity, the tuned-to note, success), Pale Chrome `#E8DDF2`, Muted Lavender Grey `#9B91A5`, Divider Violet `#392044` (red only for clipping or an error). No lime, no cyan, no mascot, character, RPG or XP idea. Pixel headings (a 5x7 pixel font, `Source/UI/PixelKit.*`), stepped-corner boxes, chunky buttons with a hard shadow, dotted dials, segmented meters, a faint dither and subtle scanlines. Numbers and body text stay in Inter so they are readable. MOTION off removes the page fade and every travelling animation (meters still update).

One fixed 960 x 600 layout, scaled to fit (resizable 60 %-150 %, aspect kept). Always shown: the persistent bar, the processing chain and the preset bar (`<` `>`, preset menu, SAVE, DELETE, status, A/B). The last level and FINAL TOUCH category are remembered in the session (`uiLevel` 0-4, `uiCategory` 0-3); a first launch opens the level-select screen. Every control that is not on the current page is hidden (`GlitchKidEditor::layoutContent()`).

## The persistent bar
`< LEVELS` (back to the level select), the current stage, VOCAL IN and OUT levels (real digital peaks; `CLIPPING` after the output really reached full scale; `NO SIGNAL` when no vocal arrives), DRY | PROCESSED (the real bypass), UNDO (puts back the sound from before the last build, profile load or BEAT LOCK change; disabled when there is nothing to undo) and MOTION. The preset name and restore/A-B stay in the bottom bar.

## The chain
INPUT -> TUNE -> TONE -> DYNAMICS -> SPACE -> OUTPUT. INPUT / OUTPUT show the real peak level. The four stages are the real stages of the engine; a pink dot means the stage is doing something right now (from the actual parameters, including CLEAN LAB and AIR LAB; all off while DRY). Click a stage to open its FINAL TOUCH category. A small dot travels the path only while a signal really arrives (MOTION on).

## Level select
Four cards, always open (nothing is locked, any order). The line at the bottom of a card is read from real state, never from a click:
* LEVEL 01 CLEAN LAB: OFF / ON - WAITING FOR AUDIO / ON - LEARNING YOUR VOICE / ON - CLEANING NOW / ON - NOTHING TO FIX FOUND (from the DSP's telemetry).
* LEVEL 02 BEAT LOCK: NO BEAT INPUT CONNECTED / BEAT INPUT IS SILENT / BEAT ARRIVING - READY TO ANALYZE / ANALYZING... / ANALYSED - n SUGGESTIONS / APPLIED - n CHANGES.
* LEVEL 03 BUILD MY VOCAL MIX: NOT BUILT YET / LISTENING TO YOUR VOCAL / BUILT - n CHANGES, BEAT-AWARE or VOCAL ONLY / BUILD FAILED.
* LEVEL 04 FINAL TOUCH: n OF 4 STAGES ACTIVE (from the chain), or DRY.
Two pixel meters show the real vocal-in and output peaks.

## Level 1: CLEAN LAB
The CLEAN LAB switch (a genuine, click-free bypass: switch it off and on to hear before and after), the CLEAN amount, three section trims (corrective EQ, resonance, de-esser) and RESET CLEAN. The read-outs (rumble blend, low-mid cut, up to three resonance cuts, S-sound gain reduction, net level change, learning status) come only from the DSP's telemetry. The real EQ graph below shows your EQ (violet) and CLEAN LAB's applied corrections (pink); both are calculated from real settings, there is no spectrum analyser. See `docs/CLEAN_LAB_IMPLEMENTATION.md`.

## Level 2: BEAT LOCK
ANALYZE BEAT listens to the vocal and the beat together while the host plays (only if a beat really arrives on the optional Instrumental input), shows the real vocal and beat peaks, a five-band comparison and a list of suggestions (BALANCE advice, PRESENCE, CLARITY, SPACE), each with its reason, its real current -> proposed value and a tick you can remove. APPLY SELECTED changes only ticked items, once per analysis; UNDO BEAT LOCK / UNDO puts your sound back. With no beat the page says so, starts no analysis and offers the vocal-only workflow (BUILD MY VOCAL MIX). See `docs/CLEAN_LAB_BEAT_LOCK_IMPLEMENTATION.md`.

## Level 3: BUILD MY VOCAL MIX
Eight profiles: CLEAN & NATURAL, MODERN MELODIC RAP, HARD-TUNED RAP, DARK & ATMOSPHERIC, BRIGHT & PRESENT, SMOOTH R&B, GRITTY RAP, SPACIOUS LEAD. Select to read, LOAD PROFILE to hear it as saved, BACK TO MY SOUND to undo.
BUILD MY VOCAL MIX listens while the host plays, then applies the selected profile plus bounded, explained adjustments (works without a beat). The button shows real state: ready, waiting for audio, analysing (progress follows the analysis time), failed. A status line, the input / vocal / instrumental status, MIX INTENSITY, and the RESULT block (what was applied, the first fitted settings with before/after values, WHAT CHANGED for all of it).

## Level 4: FINAL TOUCH
PITCH & KEY (the key pad, scale chips, on/off, tuning style, strength and speed; GlitchKid does not detect your key), TONE (a darker-to-brighter slider, the EQ response graph with draggable LOW / BODY / PRESENCE / AIR points, the selected band's FREQ / GAIN / Q knobs, WARMTH, and the AIR LAB strip: switch, LINK, PRESENCE and AIR, MIX, TRIM), DYNAMICS (a softer-to-more-upfront slider, compressor, output gain, IN / OUT / GR meters), SPACE (a dry-to-spacious slider, echo, width, doubler). BACK TO BUILD, RESET, SAVE PRESET. GlitchKid has no reverb: "space" is delay, width and doubling.

## High-level sliders (exact mappings, `Source/Macros/MacroEngine`)
* TONE: eq_hi_gain -6..+6 dB, eq_b2_gain -1.5..+1.5 dB (0.5 = flat). Read back from eq_hi_gain.
* PUNCH: comp_ratio 1-6, comp_thresh -12..-26 dB, attack 15 ms, release 110 ms, knee 6 dB, makeup min(4, 0.8 (ratio-1)) dB. Read back from the ratio.
* SPACE: echo_on/dim_on/dbl_on (on above 2 %), echo_mix 0-60 %, echo_fb 15-45 %, echo_width 20-75 %, dim_mix 0-45 %, dbl_amount 0-40 %. Read back from echo_mix.
A slider overwrites its parameters while dragged and follows them otherwise. Never touched: Gain, Mix, Bypass, tuning, key, scale.

## Safety and testing
Painting and animation run on the message thread (30 Hz timer); MOTION off makes everything still. Parameter IDs, state, presets, A/B and automation are unchanged; saved preferences are `uiLevel` (0-4; the older `uiPage` is ignored), `uiCategory` (0-3), `uiMotion`, `uiProfile`.
`Tests/MacroTests.cpp` builds the real editor, renders every page and category at several sizes and drives the real controls (see the audit). `GLITCHKID_SNAPSHOT_DIR=/folder` writes PNG renders; they show layout, not sound.
