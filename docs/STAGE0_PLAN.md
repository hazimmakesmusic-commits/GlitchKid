# GlitchKid — Stage 0 plan (historical, refreshed)

This file began as the Stage 0 inspection report (2026-10-14, empty repository). It is kept as a record of the
original plan and has been updated to say what actually happened. For the current state see
`ARCHITECTURE.md`, `TESTING.md`, `ROADMAP.md` and `../CHANGELOG.md`.

## 1. Environment (where work is done)
| Item | Finding |
|---|---|
| Development container | Linux (Ubuntu 24.04, x86_64). It can compile and unit-test the DSP and build a Linux VST3 for `pluginval`. It cannot build or load a macOS plug-in, and cannot run FL Studio. |
| Owner's machine | Apple Silicon Mac, FL Studio. macOS builds, FL Studio loading and listening checks are done there, and results come from the owner's screenshots and reports. |

## 2. Licences (verified vs unverified)
| Component | Terms | Status |
|---|---|---|
| JUCE 9.0.4 | Dual: AGPLv3 **or** the JUCE 9 EULA (commercial tiers). | Dual licence read from JUCE's `LICENSE.md`. **Tier eligibility and prices for JUCE 9 are NOT verified.** The owner must read https://juce.com/legal/juce-9-licence/. |
| VST3 SDK (bundled in JUCE) | MIT (SDK 3.8) per JUCE's own `JUCE.spdx.json`. | Verified from JUCE's files. |

AGPLv3 would require releasing GlitchKid's source under AGPL. A closed-source commercial release needs a JUCE
EULA tier. This is a business fact, not a development blocker. Not legal advice. See `../LICENSES.md`.

Possible later costs (none paid yet; development cost is ₹0): JUCE Indie tier, and the Apple Developer Program
(about US$99/year) for signing and notarising a release that other people install.

## 3. Original milestones and what happened
| Milestone | Planned | Status |
|---|---|---|
| M1 / Stage 1 | Minimal VST3: pass-through, bypass, gain, state, window | Done. Built and loaded in FL Studio on the owner's Mac. |
| M1a | Linux build + unit tests | Done: `juce::UnitTest` console app run by `ctest` (not Catch2). |
| M2 / Stage 2 | EQ, compressor, saturation, dry/wet Mix, each tested alone | Done (stages 2.1 to 2.5). |
| Stage 3.1 | First signature effect | Done: Glitch Stutter. |
| Hardening | NaN/Inf safety, JUCE pin, docs | Done (see `../CHANGELOG.md`, "Unreleased"). |
| Presets, UI polish, release | | Not started. See `ROADMAP.md`. |
| Pitch-correction gate | Separate feasibility research | Not started; nothing about it exists in the code. |

## 4. Risks still open
Starter-tier eligibility; signing cost; no Mac in the container (FL Studio checks are manual); CPU, buffer-size
and sample-rate behaviour inside FL Studio unmeasured; other DAWs untested.
