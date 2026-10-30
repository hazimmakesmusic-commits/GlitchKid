# KID — privacy and costs

**Labels:**
* **FACT:** verified in the repository, or a well-known property of a named project. Re-check external facts before relying on them.
* **ASSUMPTION:** a figure chosen to make the arithmetic concrete. Replace it with current published prices before any decision.
* **ESTIMATE:** derived from assumptions.

**No AI or speech service is assumed to be free, or to stay at today's price.**

## 1. Privacy by design

| Data | Local MVP (Phases 1–2) | With the optional cloud layer (Phase 4) |
|---|---|---|
| The vocal audio GlitchKid processes | Never leaves the machine; never used for commands | Never sent |
| Command speech (push-to-talk) | Captured only while held; transcribed locally; deleted at once | Still transcribed locally by default. Sending audio to a cloud recogniser would be a separate, explicit opt-in. |
| Command text | Stays in memory for the editor session | Sent to the backend only when the artist has opted in. No storage beyond what abuse prevention needs (to be defined). |
| Analysis numbers (peak, bands, sibilance …) | Local | Optional: a few numbers may be sent with the text to improve the proposal. Off by default. |
| Project and session | Nothing from KID is saved by default | Same |
| Always-on listening | Never: push-to-talk only | Never |

**Obligations for a cloud layer:**
* a privacy notice;
* consent before the first cloud request;
* a data-processing agreement with any provider;
* a regional data policy (for example GDPR for EU users);
* a way to switch the cloud off entirely while keeping KID working locally.

## 2. Running costs: local

| Item | Cost to GlitchKid | Notes |
|---|---|---|
| Director engine | 0 per use | Runs on the user's CPU in milliseconds (ESTIMATE). |
| whisper.cpp speech | 0 per use | Runs on the user's CPU. Model download about 75 MB (tiny) to 142 MB (base) (FACT, external: whisper.cpp README, verify). |
| Model hosting and download | Small | ASSUMPTION: CDN egress at $0.01–0.09 per GB → about $0.001–0.013 per base-model download (ESTIMATE). It could also be bundled in the installer, which costs nothing per download but makes the installer larger. |
| Engineering | One-off | See AI_MVP_ROADMAP.md. |

**Local KID has no per-user running cost.** This is the main reason it is the recommended MVP.

## 3. Running costs: cloud (optional, per active user)

**Assumptions:**
* a command is about 5 s of speech;
* the language model sees about 1,500 input tokens (instructions, schema, short context) and writes about 300 output tokens;
* an active artist sends 300 commands a month.

| Item | Price ASSUMPTION (check current lists) | Per command (ESTIMATE) | Per active user per month (ESTIMATE) |
|---|---|---|---|
| Cloud speech-to-text | $0.006 per audio minute (the price OpenAI published for its Whisper API in 2023) | ≈ $0.0005 | ≈ $0.15 |
| Small language model | $0.15 per million input tokens and $0.60 per million output tokens (a published small-model price point in 2024) | ≈ $0.0004 | ≈ $0.12 |
| Backend (proxy, authentication, logs) | ASSUMPTION: $20–200 a month fixed at small scale | — | Spread across users |
| **Total variable** | | **≈ $0.001** | **≈ $0.25–0.30 + the fixed share** |

Read this table as orders of magnitude, not quotes:
* **Larger models** can cost 10–50× more per token.
* **Heavier use** scales linearly.
* **A free tier** must have a hard cap per user.
* **The plug-in must never contain a provider key.** Everything goes through GlitchKid's backend.

## 4. Latency (ESTIMATES, to be measured in spikes S1 and S2)

| Path | Typical | Notes |
|---|---|---|
| Typed → local engine → proposal | < 0.1 s | Plus the analysis time (2–8 s of playback) when analysis is needed |
| Push-to-talk → local whisper (tiny/base) → proposal | About 0.3–2 s after release | Depends on the CPU and model; Apple Silicon is likely faster |
| Push-to-talk → cloud speech-to-text + language model → proposal | About 1–4 s after release | Plus the network; slower on poor connections |

## 5. Network failure and degradation

* **No internet:** the local engine and local speech still work. Only the optional cloud layer is unavailable, and KID says "using offline KID".
* **Cloud timeout** (for example 4 s): fall back to the local interpretation of the same text, or report "I didn't get that; try again or type".
* **Invalid cloud output:** rejected by the schema validator; fall back to local. **Nothing ever reaches the parameters without the local validator and the artist's Apply.**

## 6. Platforms (Mac and Windows)

| Topic | macOS | Windows |
|---|---|---|
| Director engine | Same C++ | Same C++ |
| Local speech (whisper.cpp) | Supported, with Apple Silicon acceleration (FACT, external) | Supported on CPU (FACT, external) |
| Microphone permission | Granted to the host app (TCC); the plug-in inherits it (to verify) | The system privacy setting for microphone access applies to the host process (to verify) |
| Second input device in-process | Core Audio; expected to work (to verify) | Risky with exclusive ASIO drivers; use a different device in shared mode (to verify) |
| OS speech APIs | Apple's Speech framework needs app Info.plist keys a plug-in can't add, so it's probably unusable in a DAW (to verify) | Not evaluated; availability differs by Windows edition and language |

## 7. Recommendation

1. **MVP:** local deterministic KID, typed. **$0 running cost**; no data leaves the machine.
2. **Then:** local push-to-talk with whisper.cpp, opt-in download. **$0 running cost**; no data leaves the machine.
3. **Only if needed:** an opt-in cloud language layer that returns intents, never parameters, with per-user cost caps. Budget about $0.25–0.30 per active user per month plus fixed backend costs (ESTIMATE) **before** any pricing decision.
