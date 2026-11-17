# Building

## Dependencies
| Dependency | Version | Notes |
|---|---|---|
| JUCE | 9.0.4, commit `3fb5cbc0340758596dac39f7c8de47d562b4896e` | Downloaded by CMake `FetchContent` at configure time (internet needed once); pinned by commit in `CMakeLists.txt`; never stored in the repo. Dual-licensed AGPLv3 / JUCE 9 EULA; see `../LICENSES.md`. |
| CMake | 3.22 or newer | Tested with 3.28.3 (Linux). |
| C++ | C++17 | macOS: Xcode Command Line Tools (Apple clang). Windows: Visual Studio 2022 (MSVC). Linux checks: GCC 13.3, clang 18. |
| Git | any recent | |
| Ninja | optional | Only used in the Linux container. |
| pluginval | optional | Free validator, used for testing only, not shipped. |

On Apple hosts CMake sets `arm64` and macOS deployment target 12.0. On Windows it builds x64. Only VST3 is built.
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

## GLITCHKID BEAT LOCK (the second plug-in)
The same build produces it: `build/GlitchKidBeatLock_artefacts/Release/VST3/GlitchKid Beat Lock.vst3` (on macOS, check it with
`lipo -info "build/GlitchKidBeatLock_artefacts/Release/VST3/GlitchKid Beat Lock.vst3/Contents/MacOS/GlitchKid Beat Lock"`). Install it beside GlitchKid:
```sh
cp -R "build/GlitchKidBeatLock_artefacts/Release/VST3/GlitchKid Beat Lock.vst3" ~/Library/Audio/Plug-Ins/VST3/
```
It goes on the instrumental, with the vocal sent to its sidechain input. See `docs/BEAT_LOCK.md`.

## GLITCHKID MASTER (the third plug-in)
The same build produces it: `build/GlitchKidMaster_artefacts/Release/VST3/GlitchKid Master.vst3` (on macOS, check it with
`lipo -info "build/GlitchKidMaster_artefacts/Release/VST3/GlitchKid Master.vst3/Contents/MacOS/GlitchKid Master"`). Install it the same way:
```sh
cp -R "build/GlitchKidMaster_artefacts/Release/VST3/GlitchKid Master.vst3" ~/Library/Audio/Plug-Ins/VST3/
```
It goes last on the master bus, on a finished stereo mix. See `docs/MASTER.md`.

## Windows (x64) — in progress: not yet tested in a DAW
Built and tested on every push by GitHub Actions (`.github/workflows/build.yml`, Windows and macOS runners, free for a public repository).
Each run's page offers `GlitchKid-VST3-Windows`, `GlitchKid-Beat-Lock-VST3-Windows`, `GlitchKid-Master-VST3-Windows` (the plug-ins) and `tests-Windows`
(the test log) as downloads.

To build it yourself: Visual Studio 2022 (the free Community edition, with "Desktop development with C++"), CMake and Git.
```bat
git clone https://github.com/hazimmakesmusic-commits/GlitchKid.git
cd GlitchKid
git checkout claude/glitchkid-plugin-architecture-3o6w74
cmake -S . -B build -DGLITCHKID_BUILD_TESTS=ON
cmake --build build --config Release --parallel
build\GlitchKidTests_artefacts\Release\GlitchKidTests.exe
```
Result: `build\GlitchKid_artefacts\Release\VST3\GlitchKid.vst3` (a folder). Install it by copying that folder to
`C:\Program Files\Common Files\VST3\`, then rescan plug-ins in the DAW. The build is not code-signed: DAWs load it, but a browser or
Windows may warn about the download. KID's speech recognition needs a CPU with AVX2 (2013 or later).

## KID's speech (whisper.cpp)
KID's push-to-talk speech is built in by default: the first CMake configure also downloads whisper.cpp 1.9.5 (pinned by commit; about 15 MB of source) and builds it
as a static library inside the plug-in (it adds roughly 2-3 minutes to a clean build). `-DGLITCHKID_SPEECH=OFF` builds GlitchKid without speech (KID still works by typing).
- The speech **model is not part of the build**: in the plug-in, KID > SET UP SPEECH downloads `ggml-base.en.bin` (about 142 MB) once, checks its SHA-1 and keeps it in
  `~/Library/Application Support/GlitchKid/KID/` (macOS) or `%APPDATA%\GlitchKid\KID\` (Windows). After that, speech works offline.
- The command microphone is the system's default input, opened only while the mic button is held. On macOS the host app (FL Studio) must be allowed to use the microphone
  (System Settings > Privacy & Security > Microphone).
- ggml is built for portability (`GGML_NATIVE=OFF`, no OpenMP, no Metal, no BLAS backend): Apple Silicon uses its NEON code paths and Accelerate's vDSP; x86-64 builds
  (Windows, Intel Macs) need a CPU with AVX2 (2013 or newer).


The Linux build is for tests and `pluginval`; it is not a deliverable. Use a scratch directory outside the repo:
```sh
cmake -S . -B /path/to/scratch/rel -G Ninja -DCMAKE_BUILD_TYPE=Release -DGLITCHKID_BUILD_TESTS=ON
cmake --build /path/to/scratch/rel -j4
```
## Notes
- Changing the JUCE version means changing the commit in `CMakeLists.txt` and the line in `LICENSES.md`, then re-running all tests.
- The fetched JUCE source (`_deps/juce-src`) ships its own `CLAUDE.md`/`AGENTS.md`; do not open an AI assistant inside that folder.
- Signing/notarisation for distribution needs the Apple Developer Program (about US$99/year); not done, not needed for your own machine.
