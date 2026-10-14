# GlitchKid

*A playground for your sound.* — a local-first VST3 vocal-processing plugin (macOS, FL Studio first).

**Status: Stage 2.3.** Signal path: input -> 4-band parametric EQ -> compressor -> output gain -> output, with peak meters on input and output and a gain-reduction meter.

- **EQ:** low shelf (20-500 Hz), bell 1 (100-5000 Hz, Q 0.3-10), bell 2 (500-16000 Hz, Q 0.3-10), high shelf (1-18 kHz); each band +/-15 dB in 0.1 dB steps. RBJ cookbook biquads in double precision; a band within 0.05 dB of 0 dB is skipped, so a flat EQ is bit-exact. Shelf slopes are fixed (Q 0.7071). Settings are smoothed over ~20 ms.
- **Compressor:** feed-forward, stereo-linked peak detector, soft knee, separate attack/release smoothing of the gain reduction (after Giannoulis, Massberg & Reiss, JAES 2012). Threshold -60..0 dB (default -18), ratio 1..20:1 (default 1.0 = off, so a fresh instance does not change the sound), knee 0..24 dB, attack 0.1..100 ms and release 10..1000 ms (both are one-pole time constants: about 63% of a level step for attack, falling to about 37% for release), makeup 0..24 dB. No lookahead, so no latency, but a very fast transient can overshoot for about one attack time. Idle (ratio 1, no makeup, release finished) it is skipped and bit-exact. The GR meter shows gain reduction, 0..24 dB.
- **Gain:** -60 to +12 dB in 0.1 dB steps, ramped over 20 ms (applied after the EQ).
- **Bypass:** 20 ms crossfade to the untouched input (EQ, compressor and gain all bypassed); when settled it is an exact copy and costs no CPU.
- **Meters:** IN/OUT peak, -60 to +6 dBFS, red above 0 dBFS. Processing is 32-bit float with no limiter, so EQ boost plus gain can exceed 0 dBFS; watch the OUT meter.
- All parameters are exposed to the host for automation (FL Studio automation not yet confirmed) and saved/restored with the project. Parameter IDs: `gain`, `bypass`, `eq_lo_freq/gain`, `eq_b1_freq/gain/q`, `eq_b2_freq/gain/q`, `eq_hi_freq/gain`, `comp_thresh`, `comp_ratio`, `comp_attack`, `comp_release`, `comp_knee`, `comp_makeup`.
- Not built yet: saturation, dry/wet blend, presets, pitch correction. Latency: 0 samples. CPU not yet measured.

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
Expect `100% tests passed`. They check bit-exact pass-through at 44.1/48/96 kHz and several block sizes, exact gain levels, bypass, ramped changes, mono, silence, meters, state save/restore (including garbage state), the EQ's measured frequency response, the compressor's measured static curve (hard and soft knee), attack/release time constants at three sample rates, stereo linking, makeup gain and idle/bit-exact behaviour (bell, low and high shelf, narrow vs wide Q, stability under extreme and randomly moving settings). They do not test how it sounds or the editor window.

Optional extra check with the free [pluginval](https://github.com/Tracktion/pluginval): `pluginval --strictness-level 5 ~/Library/Audio/Plug-Ins/VST3/GlitchKid.vst3`

## Identity codes (never change after release; changing them breaks saved sessions)
- Manufacturer code: `GlKd`  - Plugin code: `Gkid`  - Parameter IDs: `gain`, `bypass`

## Layout
`CMakeLists.txt` build · `Source/` plugin code · `docs/` plans · `LICENSES.md` dependency/licence notes
