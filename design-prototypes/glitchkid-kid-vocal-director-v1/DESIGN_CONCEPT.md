# KID — YOUR AI VOCAL DIRECTOR

> **YOU SPEAK. IT BUILDS.**

Concept prototype v1 · design only · **not approved for production.**
**What's simulated:** no microphone, speech recognition or AI model is used anywhere in this prototype. KID's understanding is a rule table inside the page, and every meter and analysis value is illustrative.

This folder builds on the Vocal Playground concept (`../glitchkid-vocal-playground-v1/`, unchanged). The SHAPE, CHARACTER and SPACE worlds, the scene library and the voiceprint come from there. The Booth is rebuilt around KID.

---

## 1. What KID is

KID is a vocal producer living inside the plug-in. You tell it what you hear in your head ("wider, but keep me upfront", "my ad-libs should sound distant", "I don't know what it needs"). KID answers like a producer, not a chatbot:

* **It says what it heard**, in its own words: "Wider, more space."
* **It proposes a take:** specific changes to GlitchKid's existing controls, each with before and after values and a reason.
* **It remembers the conversation.** "Too much." "Keep the vocal upfront." "A little less." Each follow-up reshapes the same take instead of starting a new mix.
* **It never acts alone.** You hear the take, untick anything you don't want, and press Apply. Undo is always there, and your manual moves always win.

KID is not a general assistant. It talks only about the sound of the vocal and can only reach GlitchKid's own parameters, through a fixed allowlist.

## 2. What changed after "looks generic"

The previous concept was rejected as generic: dark cards, knobs and panels like any plug-in. KID's Booth drops the dashboard look:

* **The session reads like a script, not a chat.** Your lines are large and light ("YOU · SPOKEN"). KID's lines are pink with a single rule down the left ("KID"). No bubbles, avatars or typing dots: it reads like a producer's session notes.
* **The microphone is the hero control:** a single round push-to-talk button with a violet-to-pink ring, and a live waveform beside it that changes form with each state.
* **KID's Take** is a dedicated panel with its own colour world (deep plum, pink edge). It shows the direction KID is following as "musical meters" (WIDER ▮▮▯▯, MORE SPACE ▮▯▯▯, YOU IN FRONT · kept).
* **THE PATH lights up** the stages KID wants to change, with a pink glow, so you see where in the real chain the change will happen.

## 3. The Booth, top to bottom

| Area | What it shows |
|---|---|
| **YOUR VOICE** (left) | The voiceprint from the Vocal Playground, with IN and OUT levels. When you press HEAR · KID, it redraws as KID's take. It's illustrative here; production would draw it from the real display taps. |
| **The session** (centre) | The script of the conversation: your requests, labelled spoken (simulated) or typed, and KID's replies. While KID works, a grey italic line says what it is doing. |
| **KID'S TAKE** (right) | Three tabs:<br>• **TAKE:** the direction meters, the modules being adjusted, and each change with checkbox, before → after and reason. Changes are tagged NEW, ▲/▼ ADJUSTED or "YOU CHANGED · UNTICKED", and a "No longer changing …" line appears after a follow-up removes something.<br>• **VOCAL READ:** what KID measured.<br>• **LIMITS:** what KID may and may not touch.<br>Footer: **HEAR [CURRENT \| KID]**, **Cancel**, **Apply n**. |
| **The command bar** | Push-to-talk mic, state name, live waveform, helper line, a type-instead field, and context-aware suggestions ("Too much. Make it subtle", "Keep the vocal upfront"). |
| **THE PATH** | The real 17-stage chain. KID's target stages glow pink. Click a stage for its manual controls. |

The header keeps world navigation (BOOTH, SHAPE, CHARACTER, SPACE), the scene pill, Undo and DRY | PROCESSED. Manual controls for all 68 sound parameters remain in the three worlds, and the test confirms it.

## 4. The seven states (all shown in the prototype)

| State | Label | Waveform treatment |
|---|---|---|
| Ready | READY · "Hold to talk, or tap. Your command mic is separate from the track." | A thin line, barely breathing |
| Recording a command | LISTENING TO YOU · "Release to send · Esc cancels" | Dense speech bars in the violet-to-pink gradient; the mic ring pulses |
| Processing the request | UNDERSTANDING | The bars collapse into a travelling line of dots |
| Analysing the vocal | ANALYSING YOUR VOCAL | The track's waveform with a playhead and a progress bar (the real analyser listens 2–8 s while the host plays) |
| Preparing changes | PREPARING CHANGES · "…checking every value is allowed" | Three nodes linking up |
| Ready to audition | READY TO AUDITION | Two waves, CURRENT and KID; the one you're hearing is lit |
| Error or unavailable | KID CAN'T HEAR YOU · "Type instead: KID still works offline." | A dashed amber line; the mic is disabled; typing still works |

**Push-to-talk is the only way to speak to KID.** Hold, or tap once to start and again to stop. There is no wake word and no always-on listening, and the ready state says so.

## 5. Speech is separate from your vocal

The command microphone is a different input from the audio GlitchKid processes. In the design it's a device chosen in settings, such as the laptop mic. What you say to KID is turned into text and then discarded. It never enters the vocal chain, the analyser or the saved session. The LIMITS tab states this, and AI_ARCHITECTURE.md explains how.

## 6. How it would work (summary; details in AI_ARCHITECTURE.md)

```
push-to-talk audio ──► speech recognition ──► words
                                              │
typed text / suggestion chips ────────────────┤
                                              ▼
                              intent interpretation      (what did the artist ask for?)
                                              ▼
                    musical decision-making with memory  (direction + kept constraints + role)
                                              ▼
              validation against the allowlisted schema  (57 of 69 parameters, ranges, dependencies)
                                              ▼
                     proposal: before / after / reason   (nothing has changed yet)
                                              ▼
                       audition (open engineering item)
                                              ▼
                         explicit Apply by the artist
                                              ▼
       existing parameter system (host gestures, automation, one Undo step)
                                              ▼
                          existing DSP, unchanged, on the audio thread
```

* **The AI never touches audio-thread memory** and never writes plug-in state directly. Its only output is a *proposal*: a list of `{parameter id, value, reason}` that the validator has already clamped to each parameter's real range.
* **Only the Apply button writes parameters**, through the same path the plug-in uses today (with host gestures), so automation, host undo and saved sessions behave as they do now.
* **Recommended first version:**
  * the understanding is a local, deterministic rule engine, the same idea as this prototype rewritten in C++ and unit-tested;
  * speech is added later with an on-device recogniser.
  * It works offline, costs nothing per use and sends nothing anywhere.
* **A cloud language model is an optional later layer** for unusual phrasing. It would return only the same structured intent, which the local engine validates. See PRIVACY_AND_COSTS.md for cost, privacy, latency and failure handling.
* **Nothing assumes a free AI service.** The minimum product needs none.

## 7. Musical intelligence, in short

KID thinks in eight musical directions, not knobs:

* **bright ↔ dark**
* **warm**
* **presence**
* **punch ↔ soft**
* **width ↔ narrow**
* **space ↔ close**
* **clean**
* **experimental**

It also has:
* **kept constraints:** you in front, presence, character, warmth;
* a **role:** lead or ad-lib;
* a **natural** flag.

Each request moves the direction. Follow-ups scale it ("subtle" = 40 %, "a little less" = 70 %, "too much" = 50 %, "more" = 150 %) or constrain it. KID then turns the direction into settings with rules that:

* **move a value only towards a musical goal and only in the asked direction,** so "wider" can never narrow the doubles;
* **respect conflicts.** "Wide but keep it upfront" widens with the Doubler and Dimension, keeps echo at 12 % or less, keeps the doubles under the lead and never lowers Presence. "Keep the presence" while darkening uses only the top shelf.
* **refuse honestly.** "Louder": level is yours. "Autotune": tuning is yours. "Reverb": GlitchKid has no reverb, so KID uses Echo and Dimension and says so.

The full mapping, with real parameter ranges, is in INTENT_PARAMETER_MAP.md.

## 8. The artist stays in control

* You can inspect every value before Apply, and every row says why it's there.
* You can modify the take: untick any row, or ask KID to adjust it.
* If you move a proposed control by hand, KID unticks that change and marks it "YOU CHANGED".
* You can audition (HEAR CURRENT | KID). This is a concept; see the open question in DESIGN_REVIEW.md.
* Cancel drops the take, Undo restores the previous sound exactly, and Escape cancels a recording.
* If KID fails (no speech, not understood, unsupported request), it says so in one line and nothing changes.
