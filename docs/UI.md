# GlitchKid interface: the Vocal Lab (TUNE -> AUTO -> MODIFY)

An underground cyberpunk sound laboratory on near-black: Void Black `#08060D`, Deep Graphite `#15111B`, Ultraviolet `#C026FF` (selection, focus, the main action, EQ), Cyber Pink `#FF3DAE` (real processing activity, the tuned-to note, success), Pale Chrome `#E8DDF2`, Muted Lavender Grey `#9B91A5`, Divider Violet `#392044`. Thin technical details (grid, header scanlines, corner brackets, a fringe on the wordmark, a very short workspace glitch that MOTION off removes). No mascot or artwork. See `docs/VOCAL_LAB_UI_AUDIT.md` and `docs/VOCAL_LAB_UI_REDESIGN_AUDIT.md`.

One fixed 960 x 600 layout, scaled to fit (resizable 60 %-150 %, aspect kept). Always shown: the header (GLITCHKID / VOCAL LAB, TUNE | AUTO | MODIFY, DRY | PROCESSED, MOTION), the processing chain and the preset bar (`<` `>`, preset menu, SAVE, DELETE, status, A/B). The last workspace, MODIFY category and selected profile are remembered in the session; a first launch opens TUNE. Every control that is not on the current page is hidden (`GlitchKidEditor::layoutContent()`).

## The chain
INPUT -> TUNE -> TONE -> DYNAMICS -> SPACE -> OUTPUT. INPUT / OUTPUT show the real peak level. The four stages are the real stages of the engine; a pink dot means the stage is doing something right now (from the actual parameters; all off while DRY). Click a stage to open its MODIFY category. A small dot travels the path only while a signal really arrives (MOTION on). A pink note appears if an older session still has a hidden effect on.

## 1 TUNE
KEY pad and SCALE chips (real `tune_key` / `tune_scale`), TUNING on/off (`tune_on`), TUNING STYLE NATURAL (50 % / 80 ms), LIGHT (75 % / 35 ms), HARD (100 % / 0 ms) (a style switches Pitch Tune on: about 31 ms of reported latency; it is read back from the two values). A summary line, the real Pitch Tune read-out (sung note, cents, target) and a real input-level strip. GlitchKid does not detect your key; the page says so.

## 2 AUTO
Eight profiles: CLEAN & NATURAL, MODERN MELODIC RAP, HARD-TUNED RAP, DARK & ATMOSPHERIC, BRIGHT & PRESENT, SMOOTH R&B, GRITTY RAP, SPACIOUS LEAD. Select to read, LOAD PROFILE to hear it as saved, BACK TO MY SOUND to undo.
BUILD MY VOCAL MIX listens while the host plays, then applies the selected profile plus bounded, explained adjustments (works without a beat). The button shows real state: ready, waiting for audio, analysing (progress follows the analysis time), failed. A status line, the input / vocal / instrumental status, MIX INTENSITY, and the RESULT block (what was applied, the first fitted settings with before/after values, WHAT CHANGED for all of it).

## 3 MODIFY
PITCH & KEY (on/off, key, scale, style, strength, speed), TONE (a darker-to-brighter slider, the EQ response graph with draggable LOW / BODY / PRESENCE / AIR points, the selected band's FREQ / GAIN / Q knobs, WARMTH, and the AIR LAB strip: switch, LINK, PRESENCE and AIR, MIX, TRIM), DYNAMICS (a softer-to-more-upfront slider, compressor, output gain, IN / OUT / GR meters), SPACE (a dry-to-spacious slider, echo, width, doubler). BACK TO AUTO, RESET, SAVE PRESET. GlitchKid has no reverb: "space" is delay, width and doubling.

## High-level sliders (exact mappings, `Source/Macros/MacroEngine`)
* TONE: eq_hi_gain -6..+6 dB, eq_b2_gain -1.5..+1.5 dB (0.5 = flat). Read back from eq_hi_gain.
* PUNCH: comp_ratio 1-6, comp_thresh -12..-26 dB, attack 15 ms, release 110 ms, knee 6 dB, makeup min(4, 0.8 (ratio-1)) dB. Read back from the ratio.
* SPACE: echo_on/dim_on/dbl_on (on above 2 %), echo_mix 0-60 %, echo_fb 15-45 %, echo_width 20-75 %, dim_mix 0-45 %, dbl_amount 0-40 %. Read back from echo_mix.
A slider overwrites its parameters while dragged and follows them otherwise. Never touched: Gain, Mix, Bypass, tuning, key, scale.

## Safety and testing
Painting and animation run on the message thread (30 Hz timer); MOTION off makes everything still. Parameter IDs, state, presets, A/B and automation are unchanged; saved preferences are `uiPage`, `uiCategory` (0-3), `uiMotion`, `uiProfile`.
`Tests/MacroTests.cpp` builds the real editor, renders every page and category at several sizes and drives the real controls (see the audit). `GLITCHKID_SNAPSHOT_DIR=/folder` writes PNG renders; they show layout, not sound.
