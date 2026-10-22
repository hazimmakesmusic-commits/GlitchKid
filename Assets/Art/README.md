# Artwork

`glitch_relic.png`: the Glitch Relic, a liquid-chrome loop shown around the HOME display (decorative only; it never stands in for the spectrum or waveform).

* Source: supplied by the project owner in `GlitchKid_Visual_Assets.zip` as `artwork/glitch_relic_master.png` (1774 x 887, RGBA, transparent background; `glitch_relic_2048.png` in that pack is a byte-identical copy).
* Provenance and licence: described by the pack as generated concept artwork. The project owner approved its use in the plug-in; the exact tool and licence terms of its generation are not recorded here, so confirm them before any public release.
* Use: compiled in through `juce_add_binary_data` (`BinaryData::glitch_relic_png`), decoded and cropped once on a background thread by `GlitchRelicLoader`. If decoding ever failed, the procedural relic in `Source/UI/GlitchRelic.cpp` is used instead.
