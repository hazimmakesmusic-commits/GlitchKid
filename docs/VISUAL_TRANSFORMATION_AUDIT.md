# Visual transformation: audit and plan (EXPENSIVE x UNHINGED)

Written before any source edit of this pass, from the code at commit `966ef98` and from the headless renders the test suite produces. A visual judgement here is mine, from images on a software renderer on Linux, not from a real display or a host window.

## 1. Architecture and reusable parts
* `Source/PluginEditor.{h,cpp}`: one editor, a 960 x 600 design scaled uniformly, five workspaces, a persistent bar, a chain strip, a preset bar. All drawing of the shell is in `GlitchKidEditor::paint` (background grid, scanline header, wordmark with a pink/violet fringe, panels) and `paintOverChildren` (the transition line).
* `Source/UI/PlaygroundWidgets.{h,cpp}`: the palette tokens, `PlaygroundLookAndFeel` (buttons, toggles, rotary dial through `drawDial`, linear slider), `ChainStrip`, `EqGraph`, `ProfileCard`, `NoteKey`, `BuildButton`, `ScopeStrip`, `ReductionBar`, `drawPanel`, `chamferPath`, `drawGlow`.
* `Source/UI/LabKit.{h,cpp}`: `labFont`, `LabHeading`, `LevelBar`, `BandCompare`, `WorkspaceTile`, `SpectrumView` (with cached geometry and change-driven redraw).
* Only Inter Regular and Bold are bundled (SIL OFL). There is no other typeface.
* Every control already reads its look from the look-and-feel or from one of these widgets, so a visual change can be made in a handful of drawing functions with no change to a parameter, attachment, range, default or state.
* Performance facts that constrain the work (measured in `docs/UI_RESPONSIVENESS_IMPLEMENTATION.md`): the 30 Hz refresh is about 0.1 ms; the HOME visualizer repaint is about 0.5 ms and is redrawn at up to 60 Hz while audio plays; a full-window software repaint is 3-9 ms. Anything added to the visualizer or to every control must be cached or cheap.

## 2. What is already strong (keep)
* The near-black foundation and the violet/pink discipline; the chamfered corner motif; tracked-out capitals for titles; the real spectrum/waveform hero with a clear legend; the tile status lines read from real state; the thin top rule with a pink segment; the focus rings; the sharp, uncluttered workspace structure.

## 3. Weaknesses that make it look generic or unfinished (from the renders)
1. **Flat rectangles.** Panels, buttons, tiles and the chain strip are single-colour fills with a 1 px border: no surface depth, no light direction, no material. This is the "collection of glowing rectangles" the brief warns about.
2. **Thin vector knobs.** `drawDial` is a hairline arc round a flat disc with a line pointer: it reads as a stock JUCE knob. No bezel, no material, no highlight, weak value indicator.
3. **Sliders and toggles are web-UI parts** (a grey circle on a 4 px line; a pill switch).
4. **No hero object.** HOME's centre is a spectrum plot on an empty black rectangle: informative but anonymous. Nothing is unmistakably GlitchKid.
5. **The wordmark is small and plain** (18 pt, a faint fringe); the header has no brand presence.
6. **One typeface, one weight family.** Everything is Inter; headings are tracked bold caps but not condensed or technical.
7. **Navigation tabs are identical grey boxes** with a violet fill when selected: no selected-state light, no hierarchy.
8. **States are weak.** DRY and PROCESSED differ only by fill; "bypassed" (limiter off, CLEAN LAB off, AIR LAB off) is a small dimming.
9. **Flat background**: a faint dot-line grid and nothing else; no atmosphere.
10. **Meters** are flat gradient bars without scale marks or light.

## 4. Plan (visual only; no DSP, parameter, preset or state change)
Stage A, the surface system: panels with a top-lit gradient, an inner highlight edge and an outer shadow; one chamfer language; buttons with a lit top edge, a pressed state and a glow for selected; DRY/PROCESSED and bypassed states made unmistakable; a background vignette with a faint violet haze.
Stage B, controls: a precision rotary (dark bezel with a rim light, a glowing value arc, a brushed-chrome cap with a specular crescent, a pink notch indicator, hover/focus/disabled states); chrome-capsule slider thumbs with a lit fill; a hardware-style toggle; meters with scale ticks and a light.
Stage C, typography and brand: a condensed, tracked heading face (Inter Bold compressed horizontally, since no other face may be bundled), a chrome-gradient GLITCHKID wordmark with a restrained fringe on the brand only, larger heading hierarchy.
Stage D, the signature: **the Glitch Relic**, an abstract liquid-chrome form (interlocking ribbons with a chrome gradient, a dark core, ultraviolet and cyber-pink rim light and a few specular lines) drawn procedurally once into a cached image, composed behind the real spectrum on HOME at low strength, never behind text or controls, and exported as a PNG by a test/tool for future marketing. No real-time 3D, no new dependency, no animation of its own (so it costs nothing per frame).
Stage E, verification: contrast checks (the relic cannot raise the background behind the spectrum above a set luminance), renders at the minimum, normal and large sizes, the UI cost harness before and after, the full suite and the sanitizer run.

## 5. Risks
* Software renderer images are not what a Mac shows (anti-aliasing, gamma, text smoothing); every look claim is "from the headless render".
* A cached image is rasterised at a fixed size and scaled with the window: drawn at 2x the design size to stay crisp up to the maximum window size.
* More layered drawing costs CPU in the software path; control draws must stay cheap (no per-paint blur or large gradients); the relic is a single image blit.
* Text contrast must not drop: the relic is clipped to the display plate and dimmed; labels stay on flat dark plates.
