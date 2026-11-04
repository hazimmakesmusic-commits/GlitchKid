# Privacy and costs — GlitchKid Vocal Playground v2

The design keeps today's local-first approach: **nothing the artist records or says leaves their computer.** This page lists what data exists, where it goes, and what running KID costs. Where the repository doesn't settle a fact, it says *unverified*.

## 1. Data inventory

| Data | Source | Where it goes | Kept | Status |
|---|---|---|---|---|
| The vocal audio | The host's audio buffer | Processed in memory by the DSP chain. The analysers read it for meters and measurements | Not stored by GlitchKid, except where the artist explicitly loads or exports a file (Mastering export, a loaded vocal file) | Exists |
| Voice-command audio | `CommandMic`: a **separate** input device, open only while the mic button or K is held | A private in-memory buffer → whisper.cpp → discarded. Never reaches `processBlock`, the analysers, the display taps or the saved session (`KidSpeech.h`) | Not stored | Exists |
| Transcript text | whisper.cpp | Shown in the KID conversation; read by `kid::parse` | In memory for the session. **Open question:** whether the conversation should be saved with the project (the prototype doesn't save it) | Exists (memory) |
| Parameter values, presets, A/B slots | The artist | The project (plug-in state) and the user preset folder | Until deleted | Exists |
| UI preferences (intro done, motion, Advanced open) | The artist | v2 proposes a global settings file (`juce::PropertiesFile`) for "intro done"; others per project | Until reset | NEW (small) |
| Speech model `ggml-base.en.bin` | Downloaded once on request from `huggingface.co/ggerganov/whisper.cpp`, checked against SHA-1 `137c40403d78fd54d454da0f9bd998f78703390c` before loading | The user's application-data folder | Until removed | Exists |
| Telemetry or analytics | — | **None exists, and none is proposed.** Any future measurement (see `AI_PRODUCT_SPEC.md` §7) must be opt-in | — | Not present |
| Accounts, licensing, activation | — | **Not designed.** Unresolved product requirement; the prototype assumes none | — | Unresolved |

**Microphone permission.** On macOS a plug-in runs inside the host's process, so the microphone permission belongs to the host (FL Studio). This is OS behaviour, not verified in FL Studio in this phase. The v2 copy tells the artist where to allow it on macOS and Windows; typing to KID always works.

## 2. Costs of the local design (today and v2)

| Cost | Size | Notes |
|---|---|---|
| Server or API cost | **0** | No network service is used. KID's interpretation is local rules; speech is local whisper.cpp |
| Download | About **142 MiB**, once | The size listed for `base.en` in whisper.cpp's model list; not re-measured here. Only when the artist chooses *Set up speech* |
| Disk | The model file plus presets | — |
| CPU on a command | One whisper.cpp transcription per release of the mic, on a background thread | **Unverified:** time per command on target machines (e.g. MacBook Air M-series, mid-range Windows). Measure before release; it shapes how long *Transcribing on this computer* shows |
| CPU while idle | None for KID (the command mic is closed) | The Booth visual repaints at display rate while open. Budget below 16 ms per full repaint on the software renderer, as for today's Mandala view |
| Binary size | The whisper.cpp static library is linked when `GLITCHKID_SPEECH=ON` (default) | **Unverified:** the added size; measure on release builds |

## 3. If a cloud interpreter were ever added (not proposed)

This is only a framework for that decision, so it isn't made by accident.
- **What would be sent:** the transcript text only (never audio), plus the minimum schema prompt. **Opt-in per user.** Off by default. The offline fallback is the local rules.
- **Cost model:** `cost per command ≈ (input tokens × input price) + (output tokens × output price)`.
  - The input is the schema plus the transcript, a few hundred tokens.
  - The output is one `IntentFrame` in JSON, under a hundred tokens.
  - Multiply by commands per user per month. Prices must be taken from the provider at decision time; none are assumed here.
- **Obligations:**
  - a privacy notice;
  - a data-processing agreement with the provider;
  - a retention policy;
  - regional rules (GDPR in the EU/UK);
  - a store-review description of what is sent.
- **Safety:** the cloud output is just another `IntentFrame` and passes the same validator. It can never set parameter values directly (`AI_ARCHITECTURE.md` §5).

## 4. Licences touching KID (from `LICENSES.md`)
- **whisper.cpp 1.9.5:** MIT. Fetched at configure time, pinned by commit.
- **The `ggml-base.en` model:** MIT (OpenAI Whisper weights). `LICENSES.md` itself notes *"verify before any commercial release"*. Not shipped in the plug-in; downloaded on request.

## 5. Privacy promises the v2 UI makes, and where they are kept
| Promise in the UI | Kept by |
|---|---|
| "KID listens only while you hold its button." | `CommandMic` opened and closed with the button (exists) |
| "Your voice command comes from a separate microphone, never your vocal track." | `CommandMic` uses the system default input, not the host's audio (exists) |
| "Transcribed on this computer, then discarded." | Local whisper.cpp; the buffer is discarded (exists) |
| "Nothing is uploaded." | No network calls except the opt-in model download (exists) |
| "Nothing changes until you apply." | v2 review-before-apply (UI change) plus the preview engine (new) |
