# GlitchKid — Stage 0 report (inspection, licensing, plan)

Date: 2026-10-14. Nothing but this file has been created. No code yet.

## 1. Environment findings
| Item | Finding |
|---|---|
| Repo | Empty (no commits, no files). Nothing to protect. |
| Where Claude runs | Cloud Linux container (Ubuntu 24.04, x86_64, 4 cores) — NOT your Mac. |
| Tools here | cmake 3.28.3, g++ 13.3, clang 18.1, ninja 1.11, git, python 3.13 |
| Consequence | Here we can compile and unit-test DSP and build a Linux test plugin. We cannot produce or load a macOS VST3, and cannot run FL Studio. |
| Your Mac | UNKNOWN — macOS version, chip (Apple Silicon/Intel), Xcode, CMake, FL Studio version not inspected. Commands in section 5 collect these. |

## 2. Licence findings (verified vs unverified)
| Component | Terms | Status |
|---|---|---|
| JUCE (latest tag 9.0.4) | Dual: AGPLv3 **or** commercial "JUCE 9 EULA". Read from the repo's LICENSE.md. | Verified (primary) |
| JUCE AGPLv3 route | Requires releasing GlitchKid's full source under AGPL. **Not closed-source.** | Standard AGPL; rejected |
| JUCE Starter tier | Free; revenue/funding cap ~US$20,000/yr; no splash-screen. Indie ~$40/mo or $800 perpetual (cap ~$300k); Pro $175/mo. | **Unverified for JUCE 9** — juce.com is unreachable from this container; figures come from search snippets and JUCE 8 documents. You must read https://juce.com/legal/juce-9-licence/ yourself. |
| VST3 SDK 3.8+ | MIT licence (Steinberg, Oct 2025); closed-source commercial use allowed. Fetched LICENSE.txt shows MIT. | Verified (primary) |
| CLAP SDK | MIT (not yet checked in the repo) | To verify if used |

**Conclusion:** JUCE + CMake + macOS VST3 can support a closed-source product at ₹0 *only* under the Starter tier, and only while GlitchKid's revenue/funding stays under the cap. Past the cap you must buy Indie (~$40/mo) or stop distributing. That is a business-model fact, not a blocker for development. I am not a lawyer; the EULA text governs.

**Fallback if Starter is unsuitable:** iPlug2 (permissive licence, not yet verified) or a bare VST3-SDK wrapper (MIT) around our own DSP.
**Mitigation built into the plan:** keep all DSP in plain C++ with no JUCE dependency (`Source/DSP`), so a framework change only replaces the thin plugin wrapper.

## 3. Costs that touch the ₹0 rule
- Apple code-signing + notarization needs the Apple Developer Program (~US$99/yr). Not needed to develop or test on your own Mac (ad-hoc signing). Needed before you give the plugin to other people without Gatekeeper warnings. **Deferred to pre-launch; needs your decision then.**
- macOS CI: a free GitHub Actions macOS runner can build the Mac binary without you installing anything, but private repos have limited free minutes (macOS minutes count extra). Repo visibility unknown to me.

## 4. Plan
| Milestone | Deliverable | Acceptance |
|---|---|---|
| M1 (Stage 1) | JUCE (pinned tag, fetched by CMake) minimal VST3: passthrough, bypass, 1 gain parameter, state save/restore, plain "GlitchKid" window | Builds on Mac; `pluginval` passes; FL Studio scans + loads it; gain audibly changes; project save/reload restores gain |
| M1a | Same code builds on Linux in this container + DSP unit tests (Catch2/ctest) run | `ctest` green, real output pasted |
| M2 | DSP modules (gain, EQ, compressor, saturation, blend) each tested alone | Numeric tests: gain dB error, EQ response, compressor threshold/ratio, no NaN/denormals |
| M3 | Parameters, presets, validation | Malformed/old preset files rejected safely |
| M4 | GlitchKid UI | Every control bound to a parameter |
| M5 | Test matrix, bug list | Per your section 7 |
| Gate | Pitch correction feasibility | Separate research, after M3 |

Risks: Starter revenue cap; Apple signing cost; no Mac in this container, so FL Studio verification is manual by you; FL Studio Mac VST3 scanning quirks (to be confirmed on your install); `pluginval` availability on macOS (free, GPLv3 tool used for testing only, not shipped).

## 5. Actions needed from you
1. Read the JUCE 9 EULA (link above) and confirm you qualify for Starter. Tell me if not.
2. On your Mac, run in Terminal and paste the output back:
   `sw_vers; uname -m; xcode-select -p; cmake --version; git --version; ls /Applications | grep -i fl`
   If Xcode tools/CMake are missing: `xcode-select --install` and `brew install cmake` (free; install Homebrew from brew.sh first if needed).
3. Say whether the GitHub repo is public or private (affects free macOS CI).
