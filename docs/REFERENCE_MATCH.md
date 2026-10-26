# Reference Match: Reference-Inspired EQ

Status: implemented and tested on Linux with synthetic signals and generated files only. Nothing here is tested on a Mac or in FL Studio, and it has not been compared with real songs or real vocals.

## What it is, and is not
A tool that compares the broad tone of an audio file you are allowed to use with the tone of your own vocal, and proposes a small, bounded, reviewable change to the vocal's own EQ. The result is called **Reference-Inspired EQ**. It is not an exact vocal match: a finished song is a full mix (vocal, instruments, mastering), not an isolated vocal. Confidence for a full-mix reference is never "high".

Not done, on purpose: no Spotify or YouTube link analysis (the plug-in has no network and no authorised route to the audio), no automatic matching of dynamics, stereo width, ambience or loudness, no changes to Gain, Mix, Pitch Tune or any parameter outside the EQ band gains, and nothing is applied without review.

## Staged plan
1. **Analysis** (`Source/Analysis/ReferenceAnalyzer.*`): read a local file off the audio thread (cancellable, with progress and clear errors), mix to a mono sum (the same signal definition as the plug-in's own display tap), pick or accept a section, and reduce it to a 19-band third-octave tonal shape from 200 Hz to 12.8 kHz, level-independent. The user's vocal is reduced the same way from the existing input tap on the message thread (no audio-thread change). Only profiles with the same basis are compared.
2. **Matching engine** (`Source/AutoMode/ReferenceMatch.*`): a pure function from two profiles, the current EQ and an intensity to a list of bounded EQ-gain changes with reasons and a confidence. At most 3 dB per move, 6 dB total per band, the presence bell at most +4 dB total, a band already beyond a limit is left alone, half of the measured difference is requested for a full mix, small differences are ignored.
3. **Interface**: a sixth tab, Reference (appended after the existing five, so saved tab numbers keep their meaning).
4. Tests at every stage (`Tests/ReferenceMatchTests.cpp`) plus the existing suite, including the real-time allocation test.

## Limits to remember
Below 200 Hz is not compared (a master's bass and drums dominate it). A full mix's tone includes its instruments. The vocal profile needs the host to play audio through the plug-in. The macOS file dialog, CoreAudio file support and FL Studio behaviour are unverified. MP3 is off in this build.

## What was built
* `Source/Analysis/ReferenceAnalyzer.{h,cpp}`: `SpectralAccumulator` (4096-point Hann frames, hop 2048, third-octave band energies, activity gate, level-independent shape, a variability figure), `analyseReferenceFile` (reads on the calling thread, picks the loudest 20 s or a chosen section, cancellable, progress, clear errors) and `ReferenceJob` (its own low-priority thread, result delivered on the message thread, `stopJob()` guarantees no callback afterwards).
* `Source/AutoMode/ReferenceMatch.{h,cpp}`: `proposeReferenceInspiredEq` and `applyMatchChanges`.
* `Source/UI/ReferencePanel.{h,cpp}`: the REFERENCE workspace. It is part of the window only while its tab is open; leave the tab open while measuring.
* The editor change: a sixth tab (the tab bar was re-spaced to fit), `Page::reference = 5`, and the saved `uiLevel` restore (0-4 as before, 5 is REFERENCE, other values behave as before).

## Compatible signals
Both profiles are measured as the mono sum ((L+R)/2) of the first two channels, which is also what the plug-in's input tap holds. The engine refuses profiles with a different basis or fewer than 12 comparable bands. A stereo file is never compared channel by channel (a test checks that an out-of-phase stereo file, whose mono sum is silent, is reported as silent).

## Confidence
Never high for a full-mix reference. It is lowered to low when the reference section is under 8 s of sound, the reference tone changes by more than 6 dB between frames, under 6 s of the vocal was measured, or the vocal's tone changes by more than 7 dB; a low confidence also reduces the proposed amounts.

## Verification (Linux)
See the final report in the session: full suite, the Reference Match groups, the existing real-time allocation tests, an AddressSanitizer + UndefinedBehaviorSanitizer run, and a fresh Release build. Not verified: macOS (file dialog, CoreAudio), FL Studio, real songs, listening.
