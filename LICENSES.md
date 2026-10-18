# Licences and dependencies

| Dependency | Version | How used | Licence |
|---|---|---|---|
| JUCE | 9.0.4 (commit `3fb5cbc0340758596dac39f7c8de47d562b4896e`) | Downloaded by CMake FetchContent at configure time, pinned by commit in `CMakeLists.txt` (tag `9.0.4` resolved to this commit on the upstream remote); not stored in this repo | Dual: AGPLv3 or JUCE 9 EULA (commercial tiers) |
| Inter (Regular, Bold) | 4.x, from the Debian `fonts-inter` package | Bundled in `Assets/Fonts`, compiled into the plug-in with `juce_add_binary_data` for the interface text | SIL Open Font License 1.1 (`Assets/Fonts/OFL.txt`; fonts must keep their licence and may not be sold on their own) |

**JUCE eligibility is NOT confirmed.** GlitchKid is intended as closed-source commercial software, which needs the JUCE EULA (free "Starter" tier has a revenue/funding cap; figures unverified for JUCE 9). This repository is public, so its source is visible to everyone. **Re-read https://juce.com/legal/juce-9-licence/ and confirm eligibility before any commercial release.** Using AGPLv3 instead would require open-sourcing GlitchKid.

Pitch Tune (Stages 4.1 and 4.2) and the experimental formant shifter (Stage 4.3) add **no dependency** (the code is our own, including the FFT); the candidates checked are listed in `docs/PITCH_CORRECTION_FEASIBILITY.md` (Rubber Band GPL-2 and aubio GPL-3 are not usable in a closed-source product; SoundTouch's licence is unverified).

GlitchKid itself has no open-source licence yet (all rights reserved by default). The VST3 interface used by JUCE is MIT-licensed (Steinberg VST3 SDK 3.8+). Optional test tool pluginval is not shipped.
