# User journeys — The Session That Talks Back

The 11 journeys from the brief, as the prototype plays them. Each one says what is real in GlitchKid, what the prototype simulates, and which QA section drives it (`qa/verify.js`; see `QA_REPORT.md`).

The artist throughout is producing in FL Studio, with GlitchKid on the vocal's Mixer insert.

---

## 1. First launch

**Goal:** go from installing it to hearing a difference in under a minute, with nothing to sign up for.

1. GlitchKid opens on **“This is your session”**. The tape already reads *late night session*; the artist renames it *midnight hook*. Any language works; the handwriting font covers Latin and Devanagari, and other scripts fall back to the system font.
2. **“Your vocal comes from FL Studio.”** The IN meter waits (“Waiting for sound…”) until they press play, then says ✓ Vocal coming in. DRY / PROCESSED is right there: DRY is the original, at matched loudness.
3. **“KID leaves notes.”** They choose *Notes on* or *Only when I ask*. The intro says plainly that KID never applies anything, listens only while you hold its button, and transcribes on this computer.
4. **Start the session** lands on SESSION, with five ideas on the desk and one note from KID.

Skip is on every step. The intro never returns unless asked (Help › Show the intro again).
**Real:** Bypass compare, the input level. **Simulated:** the input itself. **QA:** 1, 18.

## 2. Load or receive a vocal

**Goal:** know at a glance that GlitchKid is hearing the vocal.

- **Live (the normal case):** “LIVE · from FL Studio” and “FL Studio playing · 92 BPM” in the take's header; the waveform and meters move. FL Studio owns play and stop, so GlitchKid shows the transport but doesn't fake a play button. With FL Studio stopped, the take says *Paused* and the picture dims.
- **No vocal:** “No vocal yet. Press play in FL Studio, or sing into the armed track.” KID's note says the same, and that the desk still works.
- **Clipping:** CLIP lights, and KID says the take is clipping *before* it reaches GlitchKid, which can't repair it.
- **A file:** *Look at a file…* loads a take for looking and measuring only: “FILE · look only”. GlitchKid can't play files, so it says to play the track in FL Studio to hear changes.

**Real:** input from the host, host BPM, input peaks, file analysis. **Simulated:** the take, the meters, the file, the play state (reading it would be new, OPEN_DECISIONS §11). **QA:** 2, 17.

## 3. Use KID

**Goal:** say a feeling and get options you can hear, without losing control.

1. The artist types **“Make this hook feel wider.”** (or holds the mic: Listening… → Transcribing on this computer… → Reading your words…).
2. KID's note: *“Heard: wider. I left 3 ideas on the desk. Hear them, look inside, keep one or none. Gain, mix and the limiter stay yours.”* Three cards appear: **Wide** (straight from what you said), **Wide, words up front** and **Wide & Distant, further out**.
3. **Hear the first** auditions it; the sound itself doesn't change. **Look inside** shows exactly which controls move (Doubler on, Dimension on, wider…).
4. **“A little less.”** revises the same three cards (REV 2, about 70 %). **“Keep the vocal upfront.”** caps Dimension at 35 % and Echo at 22 % (REV 3).
5. They **Keep** the second card. One undo step; the next sentence starts from this new sound.
6. Other requests work the same way: *“I want it darker and more intimate.”*, *“Give me a rougher, more unstable texture.”* (KID says “rougher” and “unstable” are new words for it), *“Make the vocal feel distant but keep the words clear.”* (presence is locked).
7. Requests it can't do get an honest answer and change nothing: *“Fix my timing.”* → timing is editing in FL Studio. *“Add some reverb.”* → “GlitchKid has no reverb; the space here is Echo + Dimension” (and ideas built from those).

**Real:** `KidDirector` rules and allowlist, `CommandMic` / whisper.cpp. **Simulated:** speech recognition, KID's wording. **New:** three directions, the new words. **QA:** 12.

## 4. Work manually

**Goal:** move from KID's ideas to the controls themselves, and back, without losing anything.

- Any chain chip, KID note (“Open TUNE”) or the rail opens **SHAPE** at the right section: CLEAN · TUNE · TONE · DYNAMICS · SPACE · OUTPUT. The experimental modules are in **LAB**.
- Each control is a real parameter (drag, scroll, arrows, double-click resets). Each module has **Reset**. Every gesture is one undo step.
- If a review is open and you move one of its controls by hand, that row unticks (“YOU MOVED THIS · YOUR VALUE IS KEPT”) and Keep applies the rest.
- The notebook notices: an entry you started from shows **ON NOW · CHANGED**.

**Real:** every control. **QA:** 9, 9b, 19.

## 5. Discover a happy accident

**Goal:** stumble into something good, and not lose it.

1. **Happy accident** on the desk rolls a card, say *Tape Ghost*: “A random mix of real controls: Stutter, Dimension, Harmony. Might be great, might be nothing.”
2. **Hear** it. Only real controls in real ranges are involved, never gain, mix, the limiter or your key.
3. **Keep** → KID's note: *“Tape Ghost is yours now. Keep it in the notebook so you can find it again?”* → **Keep in notebook…** → the name is filled in (*Tape Ghost*); add a note → it's in the notebook, ON NOW.

**Real:** every move is an existing parameter. **New:** the feature. **QA:** 8.

## 6. Compare versions (A/B, the notebook, DRY)

**Goal:** hear alternatives side by side without overwriting anything.

- **A/B:** clicking an empty slot stores the current sound, without a question. Clicking an occupied slot recalls it. If the playing slot has changes, GlitchKid asks (Cancel / Switch without storing / Store in A, then switch). ⤓ stores into a slot, and asks before replacing a different sound.
- **Notebook:** **Hear** compares any entry without switching; **Use** switches (one undo step).
- **DRY / PROCESSED (D):** your original at matched loudness.
- Ideas, tapes and reviews all audition through **Hear** / **With these**.

**Real:** A/B slot APIs, Bypass compare. **New:** the A/B confirmations, the notebook, the audition. **QA:** 9, 13.

## 7. Build My Mix

**Goal:** a complete starting mix, reviewed before it lands.

1. TOOLS › BUILD MY MIX → **Start** → **Start listening** (play the vocal in FL Studio). Level, noise floor, tone and dynamics tick off, labelled SIMULATED MEASUREMENTS.
2. **Next: direction** → one of the 8 factory profiles (e.g. SMOOTH R&B) and an intensity (scales echo, Dimension, Doubler and warmth).
3. **Review the changes** → every change, in processing order, with its reason. The move fitted to the take is labelled. Untick anything you don't want; **With these** to hear it; **Keep N changes**.
4. The sound becomes **“Built: SMOOTH R&B · ● Not kept yet”**. Undo takes the whole build back.

With no vocal, it says so and offers **Skip: use as designed** (the review then says “not fitted to your take”). Your tuning, gain, mix, CLEAN, AIR LAB and limiter are never changed.
**Real:** `MixEngine` (profile, intensity, the 22 kept parameters). **Simulated:** the listening, the fitted move. **New:** review before applying (today it applies at once). **QA:** 15.

## 8. Beat Lock

**Goal:** make the vocal sit with the beat, using conservative moves.

1. TOOLS › BEAT LOCK. If nothing is routed, the panel explains: Beat Lock needs the beat on GlitchKid's second (sidechain) input. FL Studio's exact clicks are unverified.
2. With the beat routed, **Listen to both** → five-band bars (your vocal vs the beat), labelled SIMULATED MEASUREMENTS.
3. **Look at the suggestions** → presence up so the words stay clear (at most +4 dB in total), Body / low shelf a little down when the low-mids are busy (never below −6 dB), existing echo and width eased when the beat is crowded. **BALANCE** is advice only: GlitchKid never moves faders. The beat is never processed.
4. Tick, hear, keep. One undo step.

KID points here when you ask about the beat. **Real:** `BeatLock`, `beatMoves`. **Simulated:** routing and measurements. **QA:** 15.

## 9. Reference Match

**Goal:** get closer to the tone of a song you love, without copying it.

1. TOOLS › REFERENCE MATCH → **Choose a song file…** → tick **“I have the right to use this song”** (Analyse stays disabled until both are done).
2. **Analyse the reference**, then **Analyse my vocal** (play it in FL Studio).
3. **Look at the suggestions** → at most three EQ moves (Body, Presence, Air shelf), each at most 3 dB and half the measured difference × intensity. A band already beyond its limit is left alone, and the review says so. Confidence: medium (a full mix is never rated higher). The copy says it is tonal guidance, never a copy, and compares tone only.
4. **Match intensity** scales the moves down.

**Real:** `ReferenceMatch` bounds as documented in its header. **Simulated:** the file picker and the measured differences. **Finding:** the code can exceed the 3 dB limit in one case (OPEN_DECISIONS §15). **QA:** 15.

## 10. Mastering

**Goal:** master a finished mix, without confusing it with vocal shaping.

1. **MASTER** sits below a line in the rail, in steel blue. The page says it is for a finished stereo mix on FL Studio's master track, and suggests a separate GlitchKid there.
2. **Mastering mode** asks first: this instance stops shaping a vocal and its latency changes.
3. On: style (4 real configurations), Intensity, Loudness, Tone, Width, Output; meters and load/export are simulated in the prototype.
4. Everywhere else, a steel banner says Mastering mode is on. The chain shows the vocal modules as not in use, and KID is off duty. Switching it off brings everything back.

**Real:** `master_on`, `master_style`, the 5 dials. **Simulated:** meters, load, export. **QA:** 16.

## 11. Save and return

**Goal:** keep work safe, and come back to it exactly.

- **Save…** → a preset for every project. Refused: empty names, file-system characters and factory names. Replacing an existing name asks first. Saved sounds appear on the WORLDS shelf *your saved sounds* (Hear / Look / Use / ✕ with confirmation).
- **The notebook** keeps ideas for this session, with the project.
- **Unsaved work:** the sound chip shows ● Unsaved. Before another sound replaces one that isn't kept anywhere (not in the notebook, an A/B slot or your saved sounds), GlitchKid asks: Cancel / Use … anyway / **Keep it first**.
- **Close and reopen the project:** the session name, the sound and where it came from, the desk, the notebook, A/B and saved sounds all return, and the intro doesn't.

**Real:** `PresetController` save, overwrite and factory-name rules; plug-in state with the project; A/B. **New:** the session name, notebook and desk in the project state; the keep-first prompt. In the prototype, saving uses the browser's local storage. **QA:** 10, 14, 18.
