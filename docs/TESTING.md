# Testing

## What is verified, and where
| Level | How | Status |
|---|---|---|
| Source logic, DSP maths, safety properties | `GlitchKidTests` (`juce::UnitTest`), run by `ctest` on Linux and on the owner's Mac | 178 test groups, all passing on Linux (Release build, 0 warnings). The Mac `ctest` run was reported passing at Stage 2.x; **it has not been re-run on the Mac since the NaN/Inf hardening and the JUCE pin.** |
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
`StutterTests.cpp` 17 · `RobustnessTests.cpp` 6 (NaN/Inf/overflow) · `EchoTests.cpp` 15 (Liquid Echo) · `DimensionTests.cpp` 15 (Dimension) · `AlterEgoTests.cpp` 16 (Alter Egos, Chaos, Blend, Surprise) · `PitchCorrectionTests.cpp` 14 (the pitch-correction module on its own) · `PitchTuneTests.cpp` 14 (Pitch Tune inside the plug-in) · `FormantShiftTests.cpp` 16 (the formant shifter module) · `FormantTuneTests.cpp` 11 (Formant inside the plug-in).

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

## Alter Egos and the Mood Engine: what the tests prove, and what they cannot
(The earlier five Moods were replaced by the Alter Egos; every invariant of the old `MoodTests` was ported, none dropped.)
Proved (deterministic): five unique, stable IDs with names, descriptions and documented required effects; availability follows the required effects
(The Clone needs Dimension, Celestial and Voidwalker need the echo, The Parasite needs only EQ/compressor/saturation, nothing is available without its
effects); every referenced parameter exists, is unique, legal (floats in range, choice indices valid), the same 30 in every Alter Ego, never includes
Output Gain, Bypass, Mix or Stutter, and respects the safe limits at every Chaos and 25 Surprise seeds; documented base and full values of 20 defining
parameters; Chaos is deterministic, linear, bounded between its end values, clamped, moves at least 10 parameters per Alter Ego, never moves a switch;
through the processor Chaos 0 equals the documented base, sets only the owned parameters (unrelated ones checked with non-default values) and stores
the selection; Blend 0 % is exactly the input delayed by the reported latency (6 samples), 100 % the processed sound, 50 % the average of the two
(error < 1e-4); Surprise changes only allowlisted parameters, within each jitter and hard limit, repeatably per seed, never feedback above 75 %,
skips an effect that is off, keeps the Alter Ego identifiable (switches, division, drive, makeup, ratio unchanged) and stays within 2.5 dB of the input
level; finite bounded audio for every Alter Ego at Chaos 0/0.5/1, with and without a variation, mono and stereo at 44.1/48/96 kHz; level within
1 dB of the input at five Chaos settings (worst measured 0.63 dB); switching Alter Egos, jumping Chaos and pressing Surprise during playback are
finite with no step beyond 8x a 30 Hz tone's natural step (worst measured 7.41x, a deliberate worst case: a Chaos jump from 0 to 100 % in one
instant; a dragged slider moves in small steps); saved state round-trips the parameters and the selection, restoring never re-applies the
Alter Ego or overwrites an edit, a default state and an older state (no selection notes, no Dimension parameters) load; all 36 original parameter
IDs are present and none was added; host gestures are reported; missing optional parameters are skipped safely.
Mutation-checked (each caught): reversed Chaos, a switch that moves with Chaos, an out-of-range table value, unsafe feedback at full Chaos, Surprise
without hard limits, Surprise that randomises the drive, an Alter Ego that sets Blend, a selection not stored in the state, a far-too-loose
"active" match, availability that ignores required effects, a mis-tuned makeup (level).
Not provable by these tests: whether the Alter Egos sound good on a real voice (they are measured starting points, not tuned by ear), how loud your
voice gets (makeup is matched to speech-like noise), and behaviour inside FL Studio on the Mac.

## Formant in the plug-in (Stage 4.4): what the tests prove, and what they cannot
`FormantTuneTests.cpp`, 11 groups: the parameter (-12..+12, default 0, 0.1 step) and 6 reported samples at 0; with Formant at 0 the output is the input delayed by exactly 6
samples, bit-exact (3 rates x 3 block sizes, fresh and after use); latency 6 + Formant (+ Pitch Tune) at 44.1/48/96 kHz; **the plug-in output equals the standalone module's
output delayed by 6 samples, bit for bit** (mono/stereo, 2 rates, 2 shifts, block sizes 64 to 4096); notes preserved and formants moved against an ideal vowel; Mix 0 and Bypass
give the dry input delayed by the total latency, bit-exact, with Formant alone and together with Pitch Tune; moving Formant off and on during playback (finite, bounded, no dip
on engage, 0 exact again); NaN/Inf/huge equal zeros always-on and during the crossfade; Formant + Pitch Tune together (note pulled onto the note, spectral centroid moves up);
Alter Egos never touch it, state round-trips, an older state loads at 0; 0 heap allocations per block with both on. Mutation-checked (see CHANGELOG/DECISIONS 23).
Not provable by these tests: FL Studio behaviour with two effects changing the latency, real-voice quality beyond the owner's single test, live-monitoring feel at about 53 ms.

## Formant shifter module (Stage 4.3): what the tests prove, and what they cannot
`FormantShiftTests.cpp`, 16 groups, synthetic signals only (vowels with known formants, noise, bursts): latency reported and measured; neutral exact (3e-8) at 7 rates;
fundamental within 0.09 cents over 60 cases; formants move toward an ideal moved-formant vowel (2.9 dB mean error vs 6.7 dB for doing nothing); almost no energy between the harmonics
(mean -42.6 dB, worst -28.4 dB); loudness held (within 1.7 dB); silence/low-level/DC; voiced-unvoiced transitions; band-limited "s" noise; abrupt pitch change; mono = stereo and shared gain;
7 rates x 9 block sizes bit-identical; parameter changes while running (identical for any block split, thread-safe setter); NaN/Inf/huge bit-identical to zeros; reset and boundaries;
0 heap allocations in `process()` (a global operator-new counter). Mutation-checked. A sine wave is never used as evidence. What they cannot show: how a real voice sounds
(breath, sibilants, vibrato, consonants), which needs the listening test in `docs/FORMANT_SHIFTING_FEASIBILITY.md`; behaviour above about 250 Hz is documented as weak.

## Pitch Tune (Stages 4.1 and 4.2): what the tests prove, and what they cannot
`PitchCorrectionTests.cpp` (module, synthetic voices; numbers in `docs/PITCH_CORRECTION_FEASIBILITY.md`) and `PitchTuneTests.cpp` (in the plug-in):
the five parameters exist with off/neutral defaults and the 36 earlier ones are intact; with Tune off the output is the input delayed by exactly 6 samples
(bit-exact, 8 rate/block combinations); with Tune on the reported latency is 6 plus the Pitch Tune delay (44.1, 48, 96 kHz) and returns to 6 when off;
a +35 cent note is pulled onto the note within 10 cents in stereo and mono, at 44.1/48 kHz and block sizes 64 to 4096 (including blocks larger than the
internal chunk); strength 0 keeps pitch (3 cents), level (10 %) and timing (8 samples) of the input; Mix 0 and Bypass give the dry input delayed by the TOTAL
latency, bit-exact; switching on and off during playback is finite, bounded, has no silent gap on engage, and "off" is bit-exact again; a dry-only switch has
no click; NaN/Inf/huge input equals zeros bit-exactly, always-on and while the switch crossfade is running; identical channels give identical outputs; the
read-out shows the sung and target note; key/scale choices are honoured; Alter Egos never touch the Tune controls; state round-trips and an older state loads with Tune off.
Mutation-checked (each caught): dry path not delayed, latency not reported, idle not recording history, NaN not zeroed before the blend, blend never engaging,
no re-anchor at voice onset, plus the module-level mutations listed in the feasibility doc.
Not provable by these tests: how it sounds on real voices beyond the owner's single listening test, polyphonic or mixed material, host behaviour when the
reported latency changes (FL Studio, Mac), and live-monitoring feel at 31 ms.

## Manual FL Studio checklist (owner)
Load on an insert; every knob changes sound; Bypass is clean and level-matched; Mix 0 % equals bypass; Glitch
Stutter on/off, each rate, Stutter Mix, host tempo shown and followed; ALTER EGOS: click each of CELESTIAL, THE CLONE, VOIDWALKER, THE PARASITE, ALTERED STATE on a vocal (the lit button moves, the knobs move, the sound changes, no clicks), drag CHAOS from 0 to 100 % (the character intensifies, no pops, level steady), drag BLEND (0 % = original, 100 % = processed), press SURPRISE ME several times (small, usable variations; the label shows the variation number), edit one knob (label turns CUSTOM), re-click the Alter Ego (restores), save/reopen; Pitch Tune: ON with a dry vocal (the read-out shows the sung note and the target, the voice moves to the nearest note of KEY/SCALE; STRENGTH 0 leaves it alone; SPEED 0 is the hard-tune sound; no click or gap when switching ON/OFF during playback; the latency shown by FL changes from 6 to about 1500 samples while ON and tracks stay aligned; Mix 0 % and Bypass stay aligned with it; breaths and "s" sounds are not mangled; a mono track works), and that OFF sounds identical to before; Dimension: ON, WIDTH, DETUNE, DEPTH, MIX on a mono vocal, plus a mono-sum check (pan both channels to centre on a mono mixer insert or use a mono-sum meter); Liquid Echo: ON, SYNC with several divisions at the project tempo, free TIME, FEEDBACK, TONE, WIDTH, ECHO MIX, automation of each, project recall; automation; save, close, reopen the project
and settings return; latency shown as 6 samples. Record the result and FL/macOS versions.
