# Deliverable 10 — Launch readiness checklist

GlitchKid V1 is ready for commercial release only when **every** item below is checked, with evidence attached: a log, a report or a signed sheet with date and build hash.

- An item that cannot be verified **blocks** the release, or is explicitly waived by the owner in writing, with the reason recorded.
- “Owner's ear” or “reported working” is not evidence for any item except the owner's own sign-offs.

## 1. Sound quality (the primary gate)

- [ ] Corpus complete and rights-cleared (≥ 36 vocals covering every category, ≥ 21 instrumentals; hash manifest frozen) — Deliverable 8 §2.1
- [ ] Blind, loudness-matched panel run with ≥ 5 listeners (≥ 2 engineers, ≥ 2 target artists); raw ratings archived
- [ ] Generate vs engineer reference: mean overall ≥ reference − 10 (proposed gate; final number approved in Phase 0)
- [ ] Generate vs raw: ≥ raw + 15 on ≥ 90 % of items
- [ ] No item rated below 60 for artifacts/naturalness (Generate and Generate + macros)
- [ ] Harshness/sibilance not worse than the reference by > 10, overall and per genre
- [ ] Beat Lock ON preferred or tied on ≥ 70 % of pairs, never worse on intelligibility by majority
- [ ] Pitch correction (Natural/Light) ≥ 70 for artifacts on the sung material
- [ ] Objective metrics within corridors for every genre; no regression against the previous release candidate
- [ ] Every compare inside the product is loudness-matched by default (test evidence)
- [ ] Reverb passes its measurement tests (RT60, ringing, mono fold-down, CPU)

## 2. Functional completeness (V1 specification)

- [ ] Journey steps 1–7 work as specified, including every edge case in Deliverable 2 §3
- [ ] Five stages with exactly the specified controls; nothing fake or placeholder (UI review against Deliverable 2 §4)
- [ ] Four macros automatable, bounded, monotonic; neutral = bit-exact
- [ ] Beat Lock state machine complete; never re-adapts silently; manual edits always win
- [ ] TUNE suggestion never changes key or scale by itself
- [ ] A/B, bypass, Beat Lock ON/OFF and Previous mix change only what you hear or the live sound, never the stored ones
- [ ] Presets and sessions restore every field (round-trip tests); migration of older owner projects tested on fixtures
- [ ] Deferred features absent from the V1 build (Lab flag OFF); no network access at runtime (verified, e.g. with a network monitor)

## 3. Stability, safety, performance

- [ ] All unit tests pass in CI on macOS universal, Windows x64 and Linux, in both configurations
- [ ] pluginval strictness 10 passes on every platform; `auval` passes; Steinberg VST3 validator passes
- [ ] Sanitizer run clean; no allocation or locks on the audio thread (test evidence)
- [ ] 8-hour stability run with random automation: no crash, no memory growth, no NaN output
- [ ] CPU within budget: ≤ 5 % of an M1 performance core and ≤ 8 % on the reference Windows machine at 48 kHz / 128 samples (or the approved figures)
- [ ] Reported latency equals measured latency in every state (Tune on/off); offline bounce = real-time render
- [ ] Block-size independence and sample-rate changes verified (44.1–96 kHz; 192 kHz works)

## 4. Cross-DAW (claim only what passed)

- [ ] FL Studio: Mac (arm64, Intel) and Windows: all matrix cases pass (Deliverable 8 §4)
- [ ] Ableton Live 12: Mac and Windows: all cases pass
- [ ] Reaper: Mac and Windows: all cases pass
- [ ] Logic Pro (AU): all cases pass
- [ ] Sidechain (Instrumental input) routing steps verified and documented per host
- [ ] Published support matrix lists only the passed host/OS/format combinations

## 5. User experience

- [ ] First-run guidance tested with ≥ 5 target artists. Each reaches a generated mix without help in ≤ 2 minutes (recorded sessions) [A target]
- [ ] Accessibility: text ≥ 12 px at 100 %; contrast ≥ 4.5:1; keyboard reach; VoiceOver and Narrator pass
- [ ] UI scaling checked at the minimum scale, 100 %, 150 %; Retina and Windows 125 %/200 %
- [ ] Every error state has actionable copy (review against Deliverable 7 §2)
- [ ] Claims review: no “professional because louder”, no artist names, genres described as starting points (Deliverable 2 §1)

## 6. Legal and licensing

- [ ] JUCE licence tier confirmed and in force for closed-source commercial distribution
- [ ] Third-party notices complete (`LICENSES.md`): JUCE, Inter (OFL), any reverb or other code sources; whisper.cpp not in the V1 build
- [ ] Product EULA and privacy statement published (no data collection)
- [ ] Trademark check on the product name and genre names; plug-in manufacturer and plug-in codes (`GlKd` / `Gkid`) final and never to change [V: `CMakeLists.txt`]
- [ ] Repository visibility decided (it is public today [D])

## 7. Distribution and support

- [ ] macOS installer signed and notarised (Apple Developer Program); Windows installer signed
- [ ] Clean-machine install, uninstall and upgrade tested on each OS
- [ ] Version numbering set (CMake project version ≠ 0.1.0); `CHANGELOG.md` release entry
- [ ] Manual and help: journey, stages, macros, Beat Lock routing per host, limitations (Deliverable 6 §9)
- [ ] Support channel and crash-report process defined (manual reports; no telemetry)
- [ ] Rollback plan: the previous installer kept; preset and session compatibility promise written down

## 8. Final sign-off

- [ ] Every item above checked, with evidence links
- [ ] Owner go/no-go signed and dated, with the build hash of the release candidate
