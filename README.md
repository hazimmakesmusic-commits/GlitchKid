# GlitchKid

*A playground for your sound.* — a local-first VST3 vocal-processing plugin (macOS, FL Studio first).

**Status: Stage 2.1.** Clean stereo/mono pass-through with a **Gain** parameter (-60 to +12 dB, 0.1 dB steps, ramped over 20 ms) and a **Bypass** parameter, plus a small window with input/output peak meters (-60 to +6 dBFS; red above 0 dBFS). There are no effects yet. Both parameters are exposed to the host for automation (FL Studio automation is not yet confirmed) and saved/restored with the project. Processing is 32-bit float with no limiter, so +12 dB gain on a loud signal can exceed 0 dBFS; the OUT meter turns red when it does.

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

## Automated tests (unit tests of the real processor)
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGLITCHKID_BUILD_TESTS=ON
cmake --build build --config Release -j 8
ctest --test-dir build --output-on-failure
```
Expect `100% tests passed`. They check bit-exact pass-through at 44.1/48/96 kHz and several block sizes, exact gain levels, bypass, ramped changes, mono, silence, meters and state save/restore (including garbage state). They do not test sound quality or the editor.

Optional extra check with the free [pluginval](https://github.com/Tracktion/pluginval): `pluginval --strictness-level 5 ~/Library/Audio/Plug-Ins/VST3/GlitchKid.vst3`

## Identity codes (never change after release; changing them breaks saved sessions)
- Manufacturer code: `GlKd`  - Plugin code: `Gkid`  - Parameter IDs: `gain`, `bypass`

## Layout
`CMakeLists.txt` build · `Source/` plugin code · `docs/` plans · `LICENSES.md` dependency/licence notes
