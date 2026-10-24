# Interface style (usability-first redesign)

Visual only. No DSP, parameter, preset or state change.

## Palette (tokens in `Source/UI/PlaygroundWidgets.h`)
Background #101014, raised panel #1B1921, secondary surface #25222D, primary text #F0ECF5, secondary text #A7A0B2, violet #A83BEE (selection, main actions, value arcs), magenta #E14CBB (real processing activity, highlights), border #3A3644. Red is for clipping and errors. Chrome is used only for the wordmark and the idle emblem. The palette test (`Tests/VisualTests.cpp`) checks the contrast of every text colour on every surface.

## Rules
* One corner shape: a small rounded corner (`chamferPath`, radius up to 6 px). No glow (`drawGlow` is a no-op), no scanlines, no grid, no haze.
* Selected = solid violet fill with white text. Unselected workspace tabs are a plain quiet surface. Hover lightens, disabled dims, a focus ring is drawn on keyboard focus.
* Dials: thin violet value arc, flat cap, short indicator. Sliders and toggles are flat. Panels are flat with a 1 px border.
* Text is Inter at its natural width. Words written in capitals in the code are drawn in Title Case (`displayCase`); statuses and sentences use sentence case; acronyms and user text (preset names) are left alone.
* HOME: header, signal chain, a plain display (grid, axis labels, legend, real traces and waveform), four compact workspace cards, the preset and A/B rail. The chrome artwork appears only as a small emblem in the middle of the display while there is no signal, and disappears when signal arrives. There is no artwork motion.
