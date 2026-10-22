# Visual transformation: implementation

Presentation only. No DSP, parameter (69, unchanged), preset, saved-state or behaviour change. Judged from headless renders on a Linux software renderer; **not seen on a Mac, on a Retina display or in FL Studio**. The audit and plan are in `docs/VISUAL_TRANSFORMATION_AUDIT.md`.

## What changed
* **Surfaces** (`drawPanel`, `drawButtonBackground`, `WorkspaceTile`, `ChainStrip`): chamfered shapes with a top-lit gradient, an inner highlight edge and an outer shadow; selected controls glow, idle ones do not; nav tabs have a lit underline when selected.
* **Controls** (`drawDial`, `drawLinearSlider`, `drawToggleButton`): a dark bezel with a rim light, a glowing value arc, a chrome cap with a specular crescent and a pink notch; chrome-capsule slider thumbs on a lit fill; a hardware-style toggle. All still bound to the same attachments.
* **Typography**: a condensed technical face made from Inter Bold (no new font bundled, so no licence change); a chrome-gradient GLITCHKID wordmark with a restrained fringe on the brand only.
* **Glitch Relic** (`Source/UI/GlitchRelic.*`): a procedural liquid-chrome ribbon knot with ultraviolet and pink rim light, deterministic for a given size, transparent edges. Rendered once at 1824 x 472 on a low-priority thread (about 360 ms), delivered to the message thread, cached for the process (a second editor gets it in about 2.7 ms), stopped in the editor destructor. Shown at strength 0.6 behind the real spectrum on HOME only. `VisualTests` can export it as PNG when `GLITCHKID_SNAPSHOT_DIR` is set.
* **Atmosphere**: violet and pink haze plus a vignette, drawn once per window size into a cached image (three full-window gradients were costing about 15 ms per repaint in software).
* **Visualisation**: traces get a dark under-stroke so they stay readable over the relic; NO SIGNAL sits on a dark pill. All data is still the real FFT / waveform taps.
* **Motion**: no new animation. MOTION off still removes the workspace wipe.

## Files
New: `Source/UI/GlitchRelic.{h,cpp}`, `Tests/VisualTests.cpp`, the two docs. Modified: `Source/PluginEditor.{h,cpp}`, `Source/UI/PlaygroundWidgets.cpp`, `Source/UI/LabKit.{h,cpp}`, `Tests/LevelUiTests.cpp`, `Tests/MacroTests.cpp`, `CMakeLists.txt`, `CHANGELOG.md`, `docs/UI.md`.

## Results (Linux, fresh build directory outside `build/`)
* Release plug-in builds with no compiler warnings (a Release-only error in a test-only helper was found by the fresh build and fixed). Test target warnings are in older test files plus one `-Wshadow` in `VisualTests.cpp` that matches the existing test pattern.
* Full suite: 0 failures (parameter count 69, state and preset restoration, sound tests, overlap and clipping checks at the supported window sizes included).
* UI cost (software renderer, one core): full-window repaint 4.4-6.4 ms per workspace (21 ms before the atmosphere cache, 3.3-9 ms before this pass); HOME visualizer repaint 0.47 ms; 30 Hz refresh 0.014-0.21 ms. The first HOME paint after opening is slower once (relic draw and cache build).
* Contrast: dim-label contrast in the spectrum zone 6.49:1 (needs 4.5:1).

## Not verified
Real display colour and gamma, Retina scaling, font rendering on macOS, FL Studio's window wrapper, GPU/CoreGraphics paint cost, any listening.

## HOME pass (sculpture and motion)
* `SpectrumView::setSculpture`: the plot, grid, legend and wave strip live on a glass plate inset 56 x 8 px in the display; the relic fills the frame behind it, so the chrome ribbons are visible at the two sides and through the glass (88 % dark). The relic geometry was re-composed (wider, thicker ribbon, ends at mid-height) to fit.
* Motion: `SpectrumView::setActivity (dt, activity, motionAllowed)`. `activity` is the mean level of the displayed input or output spectrum that currently has signal (the same real data the traces use, produced on the message thread from the lock-free taps). It eases in and out; the artwork drifts at most 9 x 3 px and brightens by 12 %; at zero activity, or with MOTION off, it holds still and requests no repaint. No audio-thread change.
* Tiles: `WorkspaceTile::setGlyph` draws four fixed schematic pictograms.
* Not changed: layout positions of the title row, tiles and bottom bar; controls; navigation.
* Test and render evidence: `Tests/VisualTests.cpp` (motion, contrast under glass), full suite 0 failures, headless renders `home_signal.png` / `home_nosignal.png`. HOME at the minimum and maximum window sizes was covered by the existing overlap tests, not looked at by eye in this pass.
