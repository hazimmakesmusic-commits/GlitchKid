# UI concepts (mockups only)

Static images at the plugin's 960x600 design size (rendered 2x), using the palette from `UI_STYLE.md`. Nothing here is implemented, so the values in the images are placeholders.

1. `concept1_home.png`: **Refined Home.** This is the current layout, tightened up: a signal-chain strip with an activity dot per stage, an EQ/spectrum display, IN/GR/OUT meters, four workspace cards with live status, and a preset/A-B rail.
2. `concept2_alter_egos.png`: **Alter Ego Stage.** The five moods are big selectable cards around the Glitch Relic orb, with three macros (Intensity, Human<->Mutant, Mix).
3. `concept3_fx_rack.png`: **FX Rack + Stutter Pads.** Each effect is a module with an ON footswitch, and Glitch Stutter becomes hold-to-repeat pads with a view of the captured slice. The second row of pads (tape stop, reverse, gate, scatter) contains new effect ideas that are not built.

Sources are in `src/`. To re-render, run Playwright on each HTML file at a 960x600 viewport.
