# GlitchKid interface: the Experimental Lab (HOME, CLEAN LAB, BEAT LOCK, BUILD MY VOCAL MIX, FINAL TOUCH)

An underground cyberpunk sound laboratory on near-black, using the locked palette only: Void Black `#08060D`, Deep Graphite `#15111B`, Ultraviolet `#C026FF` (selection, focus, the main action, the vocal), Cyber Pink `#FF3DAE` (real processing activity, the output, the tuned-to note, success), Pale Chrome `#E8DDF2`, Muted Lavender Grey `#9B91A5`, Divider Violet `#392044` (red only for clipping or an error). No pixel or retro-game styling, no level select, no mascot or character, no RPG or XP idea, no blue/cyan accent. Bold, tracked-out capitals for titles and technical labels (Inter Bold), plain Inter for values and descriptions; chamfered, sharp panels; a restrained neon glow; thin header scanlines; a very short horizontal glitch when a workspace opens (MOTION off removes it and every travelling animation; meters and the display still update).

One fixed 960 x 600 layout, scaled to fit (resizable 60 %-150 %, aspect kept). Always shown: the navigation and status bar, the processing chain and the preset bar (`<` `>`, preset menu, SAVE, DELETE, status, A/B). The last workspace and FINAL TOUCH category are remembered in the session (`uiLevel` 0-4, `uiCategory` 0-3, `uiCleanAdv`); a first launch opens HOME. Every control that is not on the current workspace is hidden (`GlitchKidEditor::layoutContent()`). Opening, closing or resizing a workspace never changes a parameter (tested).

## Navigation and status bar
HOME | CLEAN LAB | BEAT LOCK | BUILD MY MIX | FINAL TOUCH (any workspace at any time, no wizard), then DRY | PROCESSED (the real bypass), UNDO (puts back the sound from before the last build, profile load or BEAT LOCK change; disabled when there is nothing to undo) and MOTION. To the right of the chain: VOCAL IN and OUT (real digital peaks; CLIPPING after the output really reached full scale; NO SIGNAL when no vocal arrives).

## The chain
INPUT -> TUNE -> TONE -> DYNAMICS -> SPACE -> OUTPUT. INPUT / OUTPUT show the real peak level. The four stages are the real stages of the engine; a pink dot means the stage is doing something right now (from the actual parameters, including CLEAN LAB and AIR LAB; all off while DRY). Click a stage to open its FINAL TOUCH category.

## HOME / SESSION
The central real-time display: a log-frequency spectrum and a waveform strip of **VOCAL INPUT** (the audio this track sends to the plug-in, before processing, violet) and **OUTPUT** (after processing, pink). An INSTRUMENTAL INPUT trace (grey) appears only while a beat is really arriving on the Instrumental input. Without a signal it says NO SIGNAL and draws nothing. Four workspace tiles (CLEAN LAB, BEAT LOCK, a prominent BUILD MY VOCAL MIX, FINAL TOUCH) whose status line is read from real state: CLEAN LAB off / waiting for audio / learning your voice / cleaning now / nothing to fix found; BEAT LOCK no beat input connected / input silent / ready to analyze / analyzing / analysed n suggestions / applied n changes; BUILD not built yet / listening / built n changes (beat-aware or vocal only) / failed; FINAL TOUCH n of 4 stages active (or DRY). Opening a workspace never marks anything complete.

## CLEAN LAB
The CLEAN LAB switch (a genuine, click-free bypass: switch it off and on to hear before and after; DRY | PROCESSED bypasses everything), the CLEAN amount, RESET CLEAN and an ADVANCED switch that reveals the three section trims (corrective EQ, resonance, de-esser). A large EQ graph shows your EQ (violet curve), CLEAN LAB's applied corrections (pink curve) and, faintly behind them, the real spectrum of the vocal input while audio plays. A summary of what CLEAN LAB is really doing (rumble blend, low-mid cut, up to three resonance cuts, S-sound gain reduction, learning status, net level change) comes only from the DSP's telemetry. See `docs/CLEAN_LAB_IMPLEMENTATION.md`.

## BEAT LOCK
ANALYZE BEAT listens to the vocal and the beat together while the host plays (only if a beat really arrives on the optional Instrumental input), shows the real vocal and beat peaks, a spectrum of the vocal and (only when live) the beat, a five-band comparison and a list of suggestions (BALANCE advice, PRESENCE, CLARITY, SPACE), each with its reason, its real current -> proposed value and a tick you can remove. APPLY SELECTED changes only ticked items, once per analysis; UNDO BEAT LOCK / UNDO puts your sound back. With no beat the page says so, starts no analysis and offers the vocal-only workflow (BUILD MY MIX). See `docs/SOURCES.md`.

## BUILD MY VOCAL MIX
Eight profiles: CLEAN & NATURAL, MODERN MELODIC RAP, HARD-TUNED RAP, DARK & ATMOSPHERIC, BRIGHT & PRESENT, SMOOTH R&B, GRITTY RAP, SPACIOUS LEAD. Select to read, LOAD PROFILE to hear it as saved, BACK TO MY SOUND to undo.
BUILD MY VOCAL MIX listens while the host plays, then applies the selected profile plus bounded, explained adjustments (works without a beat). The button shows real state: ready, waiting for audio, analysing (progress follows the analysis time), failed. A status line, the input / vocal / instrumental status, MIX INTENSITY, and the RESULT block (what was applied, the first fitted settings with before/after values, WHAT CHANGED for all of it, FINAL TOUCH > to continue by hand).

## FINAL TOUCH
PITCH & KEY (the key pad, scale chips, on/off, tuning style, strength and speed; GlitchKid does not detect your key), TONE (a darker-to-brighter slider, the EQ response graph with the vocal spectrum behind it and draggable LOW / BODY / PRESENCE / AIR points, the selected band's FREQ / GAIN / Q knobs, WARMTH, and the AIR LAB strip), DYNAMICS (a softer-to-more-upfront slider, compressor, output gain, IN / OUT / GR meters, and the OUTPUT LIMITER: a switch, CEILING, DRIVE, RELEASE, a real gain-reduction bar and the post-limiter sample peak with CLIPPING; see `docs/OUTPUT_LIMITER_QUALITY_IMPLEMENTATION.md`), SPACE (a dry-to-spacious slider, echo, width, doubler). BACK TO BUILD, RESET, SAVE PRESET. GlitchKid has no reverb: "space" is delay, width and doubling. The older creative effects (formant, harmony, mutation, stutter, Alter Ego) stay in the engine for old sessions but have no controls.

## High-level sliders (exact mappings, `Source/Macros/MacroEngine`)
* TONE: eq_hi_gain -6..+6 dB, eq_b2_gain -1.5..+1.5 dB (0.5 = flat). Read back from eq_hi_gain.
* PUNCH: comp_ratio 1-6, comp_thresh -12..-26 dB, attack 15 ms, release 110 ms, knee 6 dB, makeup min(4, 0.8 (ratio-1)) dB. Read back from the ratio.
* SPACE: echo_on/dim_on/dbl_on (on above 2 %), echo_mix 0-60 %, echo_fb 15-45 %, echo_width 20-75 %, dim_mix 0-45 %, dbl_amount 0-40 %. Read back from echo_mix.
A slider overwrites its parameters while dragged and follows them otherwise. Never touched: Gain, Mix, Bypass, tuning, key, scale.

## Safety and testing
Painting and animation run on the message thread (30 Hz timer); MOTION off makes everything still. Parameter IDs, state, presets, A/B and automation are unchanged; saved preferences are `uiLevel` (0-4; the older `uiPage` is ignored), `uiCategory` (0-3), `uiCleanAdv`, `uiMotion`, `uiProfile`.
`Tests/MacroTests.cpp` builds the real editor, renders every page and category at several sizes and drives the real controls (see the audit). `GLITCHKID_SNAPSHOT_DIR=/folder` writes PNG renders; they show layout, not sound.

## Visual system
Surfaces, controls, typography and the Glitch Relic are described in `docs/VISUAL_TRANSFORMATION_IMPLEMENTATION.md`. The relic is decoration only: it sits behind the HOME spectrum at low strength, never behind text or controls, and carries no data.

## Reference tab
A sixth tab, REFERENCE, after FINAL TOUCH: see `docs/REFERENCE_MATCH.md`. Saved tab numbers 0-4 are unchanged; 5 is REFERENCE.
