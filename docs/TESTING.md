# Testing

## What is verified, and where
| Level | How | Status |
|---|---|---|
| Source logic, DSP maths, safety properties | `GlitchKidTests` (`juce::UnitTest`), run by `ctest` on Linux and on the owner's Mac | 77 test groups, all passing on Linux (Release build, 0 warnings). The Mac `ctest` run was reported passing at Stage 2.x; **it has not been re-run on the Mac since the NaN/Inf hardening and the JUCE pin.** |
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
`StutterTests.cpp` 17 · `RobustnessTests.cpp` 6 (NaN/Inf/overflow).

## Conventions
- A test that cannot fail is not a test: new tests are mutation-checked (deliberately break the code; the test must fail).
- Build folders live outside the repository.
- A Linux pass proves code logic only. Apple Silicon differences (FMA contraction) once hid a bug that only the Mac
  showed; reproduce with `-mfma -ffp-contract=fast` on Linux when relevant.
- No CI exists yet (see `ROADMAP.md`).

## Manual FL Studio checklist (owner)
Load on an insert; every knob changes sound; Bypass is clean and level-matched; Mix 0 % equals bypass; Glitch
Stutter on/off, each rate, Stutter Mix, host tempo shown and followed; automation; save, close, reopen the project
and settings return; latency shown as 6 samples. Record the result and FL/macOS versions.
