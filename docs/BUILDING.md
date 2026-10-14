# Building

## Dependencies
| Dependency | Version | Notes |
|---|---|---|
| JUCE | 9.0.4, commit `3fb5cbc0340758596dac39f7c8de47d562b4896e` | Downloaded by CMake `FetchContent` at configure time (internet needed once); pinned by commit in `CMakeLists.txt`; never stored in the repo. Dual-licensed AGPLv3 / JUCE 9 EULA; see `../LICENSES.md`. |
| CMake | 3.22 or newer | Tested with 3.28.3 (Linux). |
| C++ | C++17 | macOS: Xcode Command Line Tools (Apple clang). Linux checks: GCC 13.3, clang 18. |
| Git | any recent | |
| Ninja | optional | Only used in the Linux container. |
| pluginval | optional | Free validator, used for testing only, not shipped. |

On Apple hosts CMake sets `arm64` and macOS deployment target 12.0. Only VST3 is built.
Plug-in cost: ₹0 to build; no paid or hosted service is used.

## macOS (Apple Silicon)
Keep the build folder outside the source folder if you prefer; `build/` inside the repo is git-ignored.
```sh
git clone https://github.com/hazimmakesmusic-commits/GlitchKid.git
cd GlitchKid
git checkout claude/glitchkid-plugin-architecture-3o6w74
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j 8
```
Result: `build/GlitchKid_artefacts/Release/VST3/GlitchKid.vst3`.
Check it is native: `lipo -info build/GlitchKid_artefacts/Release/VST3/GlitchKid.vst3/Contents/MacOS/GlitchKid` should say `arm64`.

Install for FL Studio:
```sh
mkdir -p ~/Library/Audio/Plug-Ins/VST3
cp -R build/GlitchKid_artefacts/Release/VST3/GlitchKid.vst3 ~/Library/Audio/Plug-Ins/VST3/
```
Then in FL Studio: Options > Manage plugins > scan `~/Library/Audio/Plug-Ins/VST3`. macOS may block an unsigned
plug-in the first time (System Settings > Privacy & Security).

## Linux (development / test only)
The Linux build is for tests and `pluginval`; it is not a deliverable. Use a scratch directory outside the repo:
```sh
cmake -S . -B /path/to/scratch/rel -G Ninja -DCMAKE_BUILD_TYPE=Release -DGLITCHKID_BUILD_TESTS=ON
cmake --build /path/to/scratch/rel -j4
```
## Notes
- Changing the JUCE version means changing the commit in `CMakeLists.txt` and the line in `LICENSES.md`, then re-running all tests.
- The fetched JUCE source (`_deps/juce-src`) ships its own `CLAUDE.md`/`AGENTS.md`; do not open an AI assistant inside that folder.
- Signing/notarisation for distribution needs the Apple Developer Program (about US$99/year); not done, not needed for your own machine.
