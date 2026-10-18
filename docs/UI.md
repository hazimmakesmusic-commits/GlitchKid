# GlitchKid interface: Tune -> Auto -> Modify (the "console")

The plug-in is drawn as a small alien-lab console: a dark hull with violet edge lighting, a **glass screen** at the top (step tabs, a painted title, status text, the Kid in his saucer),
a **deck** below it (reactor ANALYZE button, macro dials, DRY | PROCESSED plates) and three **pods** (the Sound Worlds). Buttons are chamfered hull plates (the step tabs are slanted),
every knob is a ring of LED segments around a knurled metal cap. On MODIFY the screen shrinks to a title strip and the category panel takes the space.
**Glitch:** at random moments (every 2-5 s) and on every click the screen glitches for ~1/3 s (title split into pink/blue slices, tear bars, a jolt of the mascot); the title also scrambles into place
when it changes. All of it stops when MOTION is off. Nothing here changes what a control does.

The window is one fixed layout (1060 x 700, scaled to fit; resizable between 60 % and 150 %, aspect ratio kept). Three steps, one page each. A header (brand, the three step buttons,
the mascot) and a slim preset bar (`<` `>`, preset menu, SAVE, DELETE, A/B) are always shown. The last page and MODIFY category are remembered in the session; a first launch opens **TUNE**.

## 1 TUNE (key and scale)

Real controls only: **KEY** and **SCALE** (the Pitch Tune parameters `tune_key`, `tune_scale`), **TUNING** on/off (`tune_on`), and **TUNING STYLE**: NATURAL / LIGHT / HARD.
A style is a fixed setting of Pitch Tune's own strength and speed (NATURAL 50 % / 80 ms, LIGHT 75 % / 35 ms, HARD 100 % / 0 ms) and switches Pitch Tune on (~31 ms of reported latency).
It is read back from the two knobs, so after hand-editing no style is lit. The summary line ("YOUR KEY: C Natural minor | STYLE: HARD") and the live sung-note read-out come from the real parameters.
**GlitchKid does not detect your key**; the page says so. NEXT goes to AUTO.

## 2 AUTO (sound and analysis)

* **Sound Worlds:** ASTRO MODE, HEARTBREAK, DIGITAL ALTER EGO. Each loads the matching built-in preset (the small line names it). The loaded world is lit.
* **ANALYZE VOCAL** then **APPLY SOUND**: the AutoMode engine (ASTRO MODE profile only; see AUTOMODE.md). Analysis and applying are separate; nothing is applied until APPLY SOUND. The status ring shows real states
  (idle / waiting for audio / listening with real progress / ready / could not analyse). INTENSITY sets how strongly APPLY moves the controls.
* **DRY | PROCESSED:** the real Bypass parameter (delay-matched, 20 ms crossfade), not the Mix knob.
* **Macros:** SPACE, CHARACTER, PUNCH (see mappings below) and CHAOS (the Alter Ego Chaos; needs an Alter Ego, picked in MODIFY > CHARACTER).
* **MODIFY >** goes to step 3. The scope strip shows the real recent input level; the mascot reacts to it and to the AutoMode state.

## 3 MODIFY (one category at a time)

Tabs: **PITCH & KEY** (on/off, key, scale, style, strength, speed, FORMANT) | **TONE** (the four EQ bands, saturation drive) | **DYNAMICS** (compressor, output gain, IN / OUT / GR meters) |
**SPACE** (Liquid Echo, Dimension, Doubler, Harmony) | **CHARACTER** (Alter Egos, Chaos, Blend, Surprise Me, Vocal Mutation, Glitch Stutter).
Under the tabs: **QUICK TWEAKS** (DARKER, MORE SPACE, HARDER TUNE, WEIRDER). Footer: **BACK TO AUTO**, **RESET** (loads Factory Default after asking; A/B snapshots are kept), **SAVE PRESET**, DRY | PROCESSED.
Nothing is shown that has no behaviour behind it. GlitchKid has no reverb: "Space" is delay, width and doubling.

## Macros and quick tweaks (exact mappings)

* **SPACE** echo_on/dim_on/dbl_on (on above 2 %), echo_mix 0-60 %, echo_fb 15-45 %, echo_width 20-75 %, dim_mix 0-45 %, dbl_amount 0-40 %. Read back: echo_mix/60.
* **CHARACTER** sat_drive 0-12 dB, eq_b2_gain 0-+2.5 dB. Read back: sat_drive/12.
* **PUNCH** comp_ratio 1-6, comp_thresh -12..-26 dB, attack 15 ms, release 110 ms, knee 6 dB, makeup min(4, 0.8 (ratio-1)) dB. Read back: (ratio-1)/5.
* **DARKER** eq_hi_gain -1.5 dB, eq_b2_gain -0.5 dB, echo_tone x0.85. **MORE SPACE** SPACE +15 points. **HARDER TUNE** tune_on, strength +10 %, speed -8 ms. **WEIRDER** mut_on, +15 % amount, +10 % character.
A macro overwrites its parameters while dragged; use A/B (SET A first) to return. Quick tweaks are fixed, bounded buttons, **not language understanding**: typed or spoken requests are not implemented.
Never touched by macros or tweaks: Gain, Mix, Bypass, key, scale, FORMANT, the Alter Ego selection.

## Design system and safety

Obsidian background; acid lime = primary/active; electric violet = atmosphere; hot pink = chaos/mutation/errors/dry; chrome = highlights. `Source/UI/PlaygroundWidgets.*`: palette, look-and-feel, widgets. No images, fonts or network.
All painting and animation run on the message thread from the editor's 30 Hz timer; a widget repaints only when its value moved; MOTION off makes everything static. The audio thread is unchanged.
Pages are laid out in `GlitchKidEditor::layoutContent()`: every control that is not part of the current page is hidden, so nothing from another page can be clicked.
Parameter IDs, state, presets, A/B and automation are unchanged; the interface only adds saved preferences `uiPage`, `uiCategory`, `uiMotion` (never part of a sound or preset).

## Testing

`Tests/MacroTests.cpp`: macro, tweak and tuning-style logic; parameter application; saved preferences; and an editor test that builds the real editor, renders every page and MODIFY category at several sizes,
drives the real navigation buttons (checking that each page shows only its own controls and that the buttons change real parameters), and renders every AutoMode state.
`GLITCHKID_SNAPSHOT_DIR=/some/folder` writes the renders as PNG files. They show layout; they do not replace looking at the plug-in in the DAW.
