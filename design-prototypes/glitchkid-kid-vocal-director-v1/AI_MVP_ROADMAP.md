# KID — MVP roadmap

**All effort figures are ESTIMATES:**
* in person-weeks for one engineer experienced in JUCE, working on this codebase;
* including tests;
* excluding listening-test time with artists.

Nothing is scheduled or approved.

## Guiding rule

Ship value without speech, servers or per-use cost first. Add speech only after it has been proven to work inside FL Studio on Mac **and** Windows. Add the cloud only if the local vocabulary proves too narrow, and only as an opt-in.

## Phases

| Phase | Scope | Effort (estimate) | Exit criteria |
|---|---|---|---|
| **0 · Decisions and spikes** | Your decisions:<br>• audition yes/no;<br>• CLEAN and AIR LAB in KID;<br>• saving kept constraints;<br>• editor size.<br>**Spike S1:** open a second input device from inside FL Studio (Mac and Windows) and record 5 s on push-to-talk without disturbing the host's audio.<br>**Spike S2:** run whisper.cpp (tiny.en and base.en) in-process. Measure load time, memory, and latency for a 5 s command, and the CPU impact while the host plays a busy project. | 2–3 | Decisions recorded in `docs/DECISIONS.md`. S1 and S2 reports with measured numbers. Go / no-go for speech. |
| **1 · KID, typed (the MVP)** | • **Director engine in C++:** the lexicon, direction memory, constraints, role, rules and validator, as a pure library with tests.<br>• **KID panel in the Booth:** session script, Take with before → after and reasons, untick, Cancel and Apply.<br>• **Apply** via `applyParameterChanges`, plus Undo via the existing `backToMine()`.<br>• Analysis-led "improve it" via `VocalAnalyzer`.<br>• Suggestion chips.<br>• LIMITS tab. | 5–8 | • Unit tests: allowlist, ranges, direction-safe moves, conflicts and the follow-up sequences from AI_PRODUCT_SPEC §3.<br>• UI test: no change before Apply.<br>• The existing suite passes on Linux and Mac.<br>• First listening session with 3–5 artists. |
| **2 · Push-to-talk (local)** | • Command-mic selection.<br>• Push-to-talk capture (≤ 10 s).<br>• whisper.cpp on a worker thread.<br>• Model download on opt-in, with a checksum.<br>• The states from the prototype.<br>• Error and fallback paths.<br>• A privacy notice. | 6–10 | • Spoken command to proposal in under about 2 s on the reference machines.<br>• Zero audio glitches in the host during capture or recognition (measured).<br>• Works in FL Studio on macOS and Windows. |
| **3 · Audition (optional)** | A processor-side temporary override (AI_ARCHITECTURE §6) behind HEAR · KID. | 3–5 | A DSP design review is passed; no host automation is written during audition; smooth switching with no clicks. |
| **4 · Cloud language layer (optional)** | • A backend proxy (the keys never ship in the plug-in).<br>• Authentication and rate limits.<br>• An LLM prompt that outputs only `IntentFrame`, with schema validation on the server **and** in the plug-in.<br>• Opt-in and a privacy policy. | 8–12, plus operations | • Measurably better understanding on a phrase test set.<br>• A cost per active user inside the budget (PRIVACY_AND_COSTS).<br>• The offline fallback still works. |

**Suggested MVP to aim for first: Phases 0 + 1.** That's about 7–11 person-weeks (estimate). It gives a fully working KID with typing and chips, at no running cost. Phase 2 makes it "you speak, it builds".

## Risks

| Risk | Likelihood (estimate) | Mitigation |
|---|---|---|
| A second mic can't be opened safely inside some hosts or drivers | Medium–high on Windows with ASIO | Spike S1 first. Fallbacks: typing, or a companion app that sends text to the plug-in. |
| Speech recognition competes with the DAW for CPU | Medium | Low-priority worker; run only on push-to-talk release; tiny or base models; measure in S2. |
| Proposals that sound wrong for some voices | High at first | Listening tests; conservative goals; Subtle/Bold through words; artists can untick. |
| Expectation gap ("AI should understand anything") | Medium | Say clearly what KID understands; show examples; answer unknown phrases honestly. |
| Model download size and licence | Low | whisper.cpp and the Whisper weights are MIT-licensed (FACT, external; verify). Download on opt-in. |
| Cost growth if the cloud is used | Medium | Phase 4 is optional; cost caps per user; the local engine stays the default. |

## What does not change in any phase

The 69 parameters, DSP, processing order, presets and saved-state format; the existing tests; Build My Mix.
