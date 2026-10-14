# GlitchKid

*A playground for your sound.* — a local-first VST3 vocal-processing plugin (macOS, FL Studio first).

**Status: Stage 2.2.** Signal path: input -> 4-band parametric EQ -> output gain -> output, with peak meters on input and output.

- **EQ:** low shelf (20-500 Hz), bell 1 (100-5000 Hz, Q 0.3-10), bell 2 (500-16000 Hz, Q 0.3-10), high shelf (1-18 kHz); each band +/-15 dB in 0.1 dB steps. RBJ cookbook biquads in double precision; a band within 0.05 dB of 0 dB is skipped, so a flat EQ is bit-exact. Shelf slopes are fixed (Q 0.7071). Settings are smoothed over ~20 ms.
- **Gain:** -60 to +12 dB in 0.1 dB steps, ramped over 20 ms (applied after the EQ).
- **Bypass:** 20 ms crossfade to the untouched input (EQ and gain both bypassed); when settled it is an exact copy and costs no CPU.
- **Meters:** IN/OUT peak, -60 to +6 dBFS, red above 0 dBFS. Processing is 32-bit float with no limiter, so EQ boost plus gain can exceed 0 dBFS; watch the OUT meter.
- All parameters are exposed to the host for automation (FL Studio automation not yet confirmed) and saved/restored with the project. Parameter IDs: `gain`, `bypass`, `eq_lo_freq/gain`, `eq_b1_freq/gain/q`, `eq_b2_freq/gain/q`, `eq_hi_freq/gain`.
- Not built yet: compressor, saturation, dry/wet blend, presets, pitch correction. Latency: 0 samples. CPU not yet measured.

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
Expect `100% tests passed`. They check bit-exact pass-through at 44.1/48/96 kHz and several block sizes, exact gain levels, bypass, ramped changes, mono, silence, meters, state save/restore (including garbage state), and the EQ's measured frequency response (bell, low and high shelf, narrow vs wide Q, stability under extreme and randomly moving settings). They do not test how it sounds or the editor window.

Optional extra check with the free [pluginval](https://github.com/Tracktion/pluginval): `pluginval --strictness-level 5 ~/Library/Audio/Plug-Ins/VST3/GlitchKid.vst3`

## Identity codes (never change after release; changing them breaks saved sessions)
- Manufacturer code: `GlKd`  - Plugin code: `Gkid`  - Parameter IDs: `gain`, `bypass`

## Layout
`CMakeLists.txt` build · `Source/` plugin code · `docs/` plans · `LICENSES.md` dependency/licence notes
