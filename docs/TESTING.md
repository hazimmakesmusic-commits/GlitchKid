# Testing

## What is verified, and where
| Level | How | Status |
|---|---|---|
| Source logic, DSP maths, safety properties | `GlitchKidTests` (`juce::UnitTest`), run by `ctest` on Linux and on the owner's Mac | 107 test groups, all passing on Linux (Release build, 0 warnings). The Mac `ctest` run was reported passing at Stage 2.x; **it has not been re-run on the Mac since the NaN/Inf hardening and the JUCE pin.** |
| Memory/UB errors | GCC AddressSanitizer + UBSan build of the same tests | Clean (exit 0, no reports) |
| Plug-in conformance | `pluginval --strictness-level 5` on the Linux VST3 | SUCCESS |
| Loads and runs in FL Studio (macOS arm64) | Owner's manual checks | Reported working for every implemented stage (see README "Verification status"). Not independently measured. |

**Not verified:** CPU load inside FL Studio, other sample rates and buffer sizes inside FL Studio, mono tracks in FL,
other DAWs, a listening-test protocol across voices, and anything about sound quality. The unit tests prove the
maths and safety properties, not taste.

## Run the tests
```sh
cmake -S . -B /path/to/scratch/rel -G Ninja -DCMAKE_BUILD_TYPE=Release -DGLITCHKID_BUILD_TESTS=ON
cmake --build /path/to/scratch/rel -j4
ctest --test-dir /path/to/scratch/rel --output-on-failure
```
(On the Mac, `-G Ninja` can be omitted.) Run the program directly for per-group detail; the binary is under
`GlitchKidTests_artefacts/Release/GlitchKidTests` in the build folder. Expect `100% tests passed`.

## Sanitizer build (Linux, GCC)
```sh
cmake -S . -B /path/to/scratch/asan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DGLITCHKID_BUILD_TESTS=ON \
  -DCMAKE_CXX_COMPILER=g++ -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=undefined" \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
```
## pluginval
```sh
pluginval --strictness-level 5 --validate-in-process path/to/GlitchKid.vst3
```
Headless Linux needs a display, e.g. prefix with `xvfb-run -a`.

## Test files (groups)
`main.cpp` 18 (processor, EQ) · `CompressorTests.cpp` 12 · `SaturationTests.cpp` 12 · `MixTests.cpp` 8 ·
`StutterTests.cpp` 17 · `RobustnessTests.cpp` 6 (NaN/Inf/overflow) · `EchoTests.cpp` 15 (Liquid Echo) · `DimensionTests.cpp` 15 (Dimension).

## Conventions
- A test that cannot fail is not a test: new tests are mutation-checked (deliberately break the code; the test must fail).
- Build folders live outside the repository.
- A Linux pass proves code logic only. Apple Silicon differences (FMA contraction) once hid a bug that only the Mac
  showed; reproduce with `-mfma -ffp-contract=fast` on Linux when relevant.
- No CI exists yet (see `ROADMAP.md`).

## Liquid Echo: what the tests prove, and what they cannot
Proved (deterministic): silence stays silent; an impulse returns after exactly the set time at 44.1 to 192 kHz and block sizes 1 to 2048;
tempo-synced lengths for every division, host tempo, missing/NaN/zero/negative tempo falling back to 120 BPM, tempo changes followed;
repeats decay; maximum feedback (and an out-of-range value) is stable with no DC build-up; Mix 0 is bit-exact dry, Mix 100 is wet only;
Tone darkens repeats without touching the dry signal; Width offsets the right echo by the documented amount at equal level; mono works;
results are bit-identical for any block size; time changes and on/off switching are click-free; off leaves no stale echo; NaN/Inf/huge input
recovers; state round-trips and an old state without echo parameters loads with echo defaults; parameters changed every block stay finite;
latency stays 6 samples. Each safety feature was mutation-checked.
Not provable by these tests: how it sounds (musicality, the pitch bend while time moves), FL Studio's handling of tail length, and
behaviour on the Mac inside FL Studio.

## Dimension: what the tests prove, and what they cannot
Proved (deterministic): silence stays silent; mono layout works and ignores Width; Mix 0 and Depth 0 are bit-exact dry; no panning
(a centred voice stays equally loud in L and R, a hard-left voice stays mostly left); Width measurably reduces L/R correlation
(1.000, 0.964, 0.877, 0.789, 0.753 at Width 0/25/50/75/100 for a mono noise voice, Depth 100, Detune 12); at Width 100 the mono sum
equals the dry signal; level stays within 1 dB of dry (about +0.55 dB at Depth 100, independent of Width); bass (30 Hz) changes by
about 0.56 dB at most; Depth and Detune change the signal, and with Detune 0 the output is exactly time-invariant; voice timing
(9 ms positive, 13 ms negative) at 44.1/48/96 kHz; extreme settings stay finite and bounded (< 1.6 for full-scale noise); output is
bit-identical for block sizes 1 to 2048 (so the LFO never resets per block); NaN/Inf/overflow recovery; parameter changes every
block and on/off switching are click-free; off is exact and leaves no stale audio; state round-trip and old-state loading;
latency stays 6 samples. Each safety/behaviour feature was mutation-checked (8 deliberate breakages, each caught).
Mono-sum worst case (swept sines 250 Hz to 10 kHz, Detune 0): Width 0 / Depth 100: -6.80 to +4.04 dB; Width 60 / Depth 50 (defaults):
-1.53 to +1.39 dB; Width 100: 0.00 dB.
CPU (Linux container, not Apple Silicon, 48 kHz stereo, block 512, share of one core): all stages idle 0.11 %; Dimension on at maximum
0.52 %, at defaults 0.44 %; everything on 1.9 %.
Not provable by these tests: how it sounds (whether the thickening is pleasant, the right amount of movement), and behaviour on the
Mac inside FL Studio.

## Manual FL Studio checklist (owner)
Load on an insert; every knob changes sound; Bypass is clean and level-matched; Mix 0 % equals bypass; Glitch
Stutter on/off, each rate, Stutter Mix, host tempo shown and followed; Dimension: ON, WIDTH, DETUNE, DEPTH, MIX on a mono vocal, plus a mono-sum check (pan both channels to centre on a mono mixer insert or use a mono-sum meter); Liquid Echo: ON, SYNC with several divisions at the project tempo, free TIME, FEEDBACK, TONE, WIDTH, ECHO MIX, automation of each, project recall; automation; save, close, reopen the project
and settings return; latency shown as 6 samples. Record the result and FL/macOS versions.
