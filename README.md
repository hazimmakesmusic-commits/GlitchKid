# GlitchKid

*A playground for your sound.* — a local-first VST3 vocal-processing plugin (macOS, FL Studio first).

**Status: Stage 1A.** Clean stereo/mono pass-through with a **Gain** parameter (-60 to +12 dB) and a **Bypass** parameter, plus a small window. There are no effects yet. Both parameters are automatable and saved/restored with the project.

## Build on macOS (Apple Silicon)
Needs Xcode Command Line Tools, CMake >= 3.22 and Git (internet is needed once, to download JUCE).

```sh
git clone https://github.com/hazimmakesmusic-commits/GlitchKid.git
cd GlitchKid
git checkout claude/glitchkid-plugin-architecture-3o6w74   # until merged
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release              # configure (downloads JUCE 9.0.4)
cmake --build build --config Release -j 8                   # build
```
Output: `build/GlitchKid_artefacts/Release/VST3/GlitchKid.vst3`

Check it is Apple Silicon: `lipo -info build/GlitchKid_artefacts/Release/VST3/GlitchKid.vst3/Contents/MacOS/GlitchKid` (should say `arm64`).

## Install and test in FL Studio
```sh
mkdir -p ~/Library/Audio/Plug-Ins/VST3
cp -R build/GlitchKid_artefacts/Release/VST3/GlitchKid.vst3 ~/Library/Audio/Plug-Ins/VST3/
```
In FL Studio: Options > Manage plugins > confirm `~/Library/Audio/Plug-Ins/VST3` is a scan path > Find installed plugins. Add GlitchKid to a mixer insert, play audio, and check: gain knob changes level; Bypass returns to original level; save the project, reopen it, and the settings are restored. (macOS may block an unsigned plugin the first time: System Settings > Privacy & Security > allow.)

Optional extra check with the free [pluginval](https://github.com/Tracktion/pluginval): `pluginval --strictness-level 5 ~/Library/Audio/Plug-Ins/VST3/GlitchKid.vst3`

## Identity codes (never change after release; changing them breaks saved sessions)
- Manufacturer code: `GlKd`  - Plugin code: `Gkid`  - Parameter IDs: `gain`, `bypass`

## Layout
`CMakeLists.txt` build · `Source/` plugin code · `docs/` plans · `LICENSES.md` dependency/licence notes
