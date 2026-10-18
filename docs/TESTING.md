# Testing

## What is verified, and where
| Level | How | Status |
|---|---|---|
| Source logic, DSP maths, safety properties | `GlitchKidTests` (`juce::UnitTest`), run by `ctest` on Linux and on the owner's Mac | 245 test groups, all passing on Linux (Release build, 0 warnings). The Mac `ctest` run was reported passing at Stage 2.x; **it has not been re-run on the Mac since the NaN/Inf hardening and the JUCE pin.** |
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

## Vocal Mutation (Stage 4.5): what the tests prove, and what they cannot
`MutationTests.cpp`, 22 groups. Module: the Mutation/Character mappings; off, MUTATION 0 and BLEND 0 are bit-exact and idle; **single sideband**: with a 0.5-amplitude tone, UP puts 0.5 at f + fc,
DOWN at f - fc, ROBOT puts 0.354 (= 0.25 x sqrt 2) at both, and the unwanted sideband and the original partial are at least 35 dB down (measured worst -41.1 dB) at 44.1/48/88.2/96 kHz x
300/1000/5000 Hz; Mutation moves the sideband to 600 Hz x m^2 and Character moves energy monotonically between DOWN and UP; **level is equal across Character** (worst 0.03 dB on noise) while
the spectrum differs; **different from the saturator**: a 220 Hz harmonic voice keeps 100 % of its power at multiples of 220 Hz through the real `Saturation` at 15 dB drive and 0.0 % through Mutation;
Blend 50 % is exactly the average of 0 and 100 %; continuity just above MUTATION 0; silence in = exact silence out, down to 1e-37 amplitude without blow-up; impulse and step responses finite,
bounded, the tail dies; mono and stereo (identical channels stay identical, a one-sided signal stays one-sided, mono equals the stereo result); **bit-identical output for 1/7/64/333/2048-sample blocks**;
random parameter automation, a Mutation/Character jump and on/off toggling stay click-free (largest step under 0.2); off is exact and no stale audio returns after re-enabling; NaN, +-Inf and an
overflow burst behave as zeros and never contaminate later audio (bit-exact against the same signal with zeros). Processor: 46 parameters with the right defaults and ranges; reported latency
unchanged; default is a bit-exact pass-through, ON changes the sound, Bypass / Mix 0 / BLEND 0 restore the exact input (mono and stereo); no stale audio after a settled Bypass; the whole chain
with Pitch Tune, FORMANT, echo and Dimension on stays finite; random automation (mono/stereo, 3 block sizes, NaN/Inf injected); Alter Egos (every one, full Chaos, Surprise seeds) never change the four parameters; state round-trip and an older state without the parameters; **0 heap
allocations per block** (module and plug-in). Mutation-checked: 15 deliberate breaks (sideband sign, missing quadrature delay, no level compensation, NaN guard, overflow guard, fade-in, linear instead of
squared carrier, inverted Character, wrong blend, memory not cleared when idle, no smoothing, stage not in the chain, Blend wired to the wrong knob, latency wrongly reported, no reset in settled Bypass): each fails at least one test.
Not provable by these tests: that it sounds good on a real voice, how much of each setting is useful musically, FL Studio behaviour, and the feel of the effect while singing.

## MOODS + Mutation, presets and A/B (Stage 4.6): what the tests prove, and what they cannot
`MutationTests.cpp` (MOODS part, 3 groups) and `PresetTests.cpp` (17 groups). **MOODS + Mutation:** each of CELESTIAL, VOIDWALKER, THE PARASITE and ALTERED STATE sets exactly the documented Mutation values at Chaos 0, 0.5 and 1 and stays recognised as active; THE CLONE switches Mutation off and leaves MUTATION/CHARACTER/BLEND
alone; a later Alter Ego fully replaces the Mutation identity; Surprise Me never moves Mutation; every Alter Ego at Chaos 0, 0.5 and 1 is within 0.70 dB of the input level (budget 1 dB); cycling through all five (mono and stereo, NaN/Inf injected) stays finite and bounded; the existing Alter Ego tests (ranges, Chaos linearity, Surprise,
state, host gestures, smoothness) all still pass with 34 owned parameters. **Presets:** the factory default equals every parameter's default and excludes Bypass; every parameter, plus the Alter Ego selection, survives capture/apply and the XML text round trip (4 random states); a real file preserves MOODS, Mutation, Pitch Tune, FORMANT, EQ, compressor, saturation,
echo, Dimension, Stutter, gain and Mix; duplicates (also by case) are refused unless overwriting is explicit and keep one file with the stored spelling; names are cleaned (path traversal, absolute paths, dots, reserved names, length, unicode) and a traversal name can neither write, read nor delete outside the folder; a missing, empty or uncreatable folder is survived; 9 kinds of bad file (garbage, truncated, empty, wrong root,
no/zero version, no parameters, NaN/Inf values, oversized) are refused with a message; an older preset gets defaults for newer parameters; a newer one loads with a note and unknown fields are ignored; wild values are clamped and NaN/Inf become defaults; a damaged file does not hide the others; a refused save leaves the file untouched; the window actions
(save, confirm-overwrite, failed load changes nothing, delete clears the name, factory default). **A/B:** slots start empty and recalling or copying from an empty slot does nothing; A and B capture independently and recall restores the complete sound (including the Alter Ego) six times in a row without changing either stored slot; edits are flagged and never written into a slot; copy works both
ways and keeps the source; loading a preset or the factory default keeps both slots and clears the active marker; slots, active marker and preset name survive a DAW save/reopen; older sessions and damaged slot data load safely. **Real time:** applying sounds from another thread while 400 blocks are processed stays finite and bounded (also under ASan/UBSan); 0 allocations per block after a preset load.
Mutation-checked: 22 deliberate breaks (sanitiser allowing separators, save ignoring overwrite, case-sensitive duplicates, no version check, no version written, bad values accepted, Bypass inside a sound, missing parameters keeping the live value, no NaN guard when applying, recall rewriting a slot, preset load clearing the slots or keeping the marker, copy in the wrong direction, no baseline after saving, stale name after delete, a failed load
applying anyway, input NaN guard removed, inactive Alter Ego values applied, wrong Parasite character, Clone leaving Mutation on, missing makeup compensation, and the traversal guard): all caught except the last one, which is an equivalent mutant (the guard is a second layer behind the name cleaning and cannot be reached through the public functions). The copy-direction break first survived (the test compared B with A after a wrong-way copy) and led to a stronger test.
Not provable by these tests: the editor's dialogs and menus (they are thin glue over the tested controller and were only rendered, not clicked), what a preset folder looks like on the Mac, FL Studio's behaviour with the pop-up menu and text dialogs, that the MOODS mappings sound good on a real voice, and "never on the audio thread" as a property (it holds because the audio code has no reference to the preset classes; the
concurrent test shows it is also safe in practice).

## Vocal Doubler and Harmony (Stage 4.8): what the tests prove, and what they cannot
`DoublingHarmonyTests.cpp`, 26 groups. **Doubler:** off and amount 0 are bit-exact, silence stays silence; **timing** (5, 18, 45 ms at 44.1/48/96 kHz): the two delays are exactly 0.85 x and 1.25 x TIMING with no wander, and no energy arrives before the first double; **detune** (0, 3, 8, 25 cents, 20 s each): the fastest delay change never exceeds the setting, reaches at least 40 % of it, averages to no pitch shift;
the doubled signal is a delayed, positively correlated copy of the source (cross-correlation peaks at the timing lags, not at unrelated lags); **width 100 %: the mono sum equals the dry input to 2e-6**, width 0 keeps L = R; level +0.05 .. +0.73 dB over 12 amount/width combinations, +0.19 dB at the defaults; the 14 kHz copy is quieter than the 400 Hz one (low-pass); mono and stereo at 44.1/48/88.2/96 kHz; **bit-identical output for 1, 7, 64, 333 and 2048-sample blocks**;
random automation, on/off switching and a 20 s extreme-settings run are click-free and bounded; no stale audio after switching off; NaN, +-Inf and an overflow burst behave as zeros (bit-exact against zeros) and never contaminate later audio. **Harmony:** all nine intervals sing the right pitch (worst 0.1 cents) at 130, 220 and 330 Hz, and every name states its direction; the lead is the input delayed by exactly the reported latency at 44.1/48/88.2/96 kHz and equals Pitch Tune's delay;
the level control is exact; unvoiced noise is gated and silence gives silence; after the lead stops nothing remains (no feedback) and the output is bounded; a 200 -> 300 Hz note jump gives no extra large clicks; block-size independence (mono and stereo); NaN/Inf/overflow input equals zeros; feed() then process() has no gap; **Pitch Tune is bit-identical with the new option off**. **In the plug-in:** the 8 new parameters, defaults, ranges and clamping; off = bit-exact and 6 samples;
latency (Doubler 0, Harmony exactly Pitch Tune's, 6 + 2L with Pitch Tune, 3 sample rates); Mix 0 % and Bypass give the exact input delayed by the total latency with the Doubler and Harmony on (mono and stereo; 4 cases); the chain with everything on stays finite under random automation with NaN/Inf injected (mono/stereo, 3 block sizes); end to end, a mono vocal is widened and the harmony appears at the interval and ABOVE the lead, not below;
state round trip, an older state without the parameters, presets and A/B include them; MOODS (3 Alter Egos use the Doubler as designed, 2 switch it off and leave the rest, Harmony and the reported latency are never touched, Surprise leaves it alone, cycling through all five is finite); 0 heap allocations per block for each module and for the plug-in with both on.
Mutation-checked: 24 deliberate breaks (identical delays, no low-pass, wrong side sign, no drift, drift three times too big, too loud, NaN guard removed, stale audio kept, timing ignored, interval flipped or ignored, gate ignored, lead not delayed, level ignored, voice fed back, fixed mode on by default, latency not reported, dry path not delayed, Doubler or Harmony not in the chain, wrong MOODS mappings): all caught. One (interval direction flipped) first survived because the module built its settings in two places and the tests only used one, and the end-to-end test only asked for "some energy"; both were fixed.
Not provable by these tests: that the doubles or the harmony sound good on a real voice, how natural the double is on exposed or vibrato-heavy vocals, harmony quality on real, breathy, fast or very low/high voices, FL Studio's behaviour when the Harmony changes the latency, and the window's controls beyond rendering. `DoubleLab` renders audio so you can judge.

## Manual FL Studio checklist (owner)
Load on an insert; every knob changes sound; Bypass is clean and level-matched; Mix 0 % equals bypass; Glitch
Stutter on/off, each rate, Stutter Mix, host tempo shown and followed; ALTER EGOS: click each of CELESTIAL, THE CLONE, VOIDWALKER, THE PARASITE, ALTERED STATE on a vocal (the lit button moves, the knobs move, the sound changes, no clicks), drag CHAOS from 0 to 100 % (the character intensifies, no pops, level steady), drag BLEND (0 % = original, 100 % = processed), press SURPRISE ME several times (small, usable variations; the label shows the variation number), edit one knob (label turns CUSTOM), re-click the Alter Ego (restores), save/reopen; Pitch Tune: ON with a dry vocal (the read-out shows the sung note and the target, the voice moves to the nearest note of KEY/SCALE; STRENGTH 0 leaves it alone; SPEED 0 is the hard-tune sound; no click or gap when switching ON/OFF during playback; the latency shown by FL changes from 6 to about 1500 samples while ON and tracks stay aligned; Mix 0 % and Bypass stay aligned with it; breaths and "s" sounds are not mangled; a mono track works), and that OFF sounds identical to before; Dimension: ON, WIDTH, DETUNE, DEPTH, MIX on a mono vocal, plus a mono-sum check (pan both channels to centre on a mono mixer insert or use a mono-sum meter); Liquid Echo: ON, SYNC with several divisions at the project tempo, free TIME, FEEDBACK, TONE, WIDTH, ECHO MIX, automation of each, project recall; automation; save, close, reopen the project
and settings return; latency shown as 6 samples. Record the result and FL/macOS versions.
