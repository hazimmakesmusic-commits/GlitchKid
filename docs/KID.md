# KID — your vocal producer

KID is GlitchKid's main screen. You tell Kid the sound you want. Kid listens to your vocal, works out what it needs, and builds the mix from GlitchKid's own effects. You hear it straight away. You can ask Kid why it made any change, give feedback, or undo.

## The screen

It has four parts.

1. **Top bar.** The GLITCHKID wordmark and Kid's status: Ready, Listening, Analyzing or Processing.
2. **Your vocal.**
   * **Load vocal** analyses a recording on a background thread (the first 3 minutes). Without a file, the waveform shows what GlitchKid is sending out right now.
   * **Original / Processed** switches between your untouched vocal (the real bypass) and GlitchKid. The two are played at matched loudness, so the louder version never sounds better just for being louder.
   * There is no play button. A plug-in can't start your DAW's transport, and the loaded file is only analysed, never played, so you hear GlitchKid by playing your track.
3. **Kid.** The conversation: type, or hold the mic (push-to-talk, once speech is set up).
4. **BUILD MY MIX.** After the first mix, this button reads **UPDATE MY MIX**:
   * with a loaded file, it rebuilds the mix from that file's analysis;
   * without one, it listens to the live input again first.

   **Undo** steps back through Kid's changes, one at a time.

The **Advanced** links at the foot (Tune, Clean, Tone, Dynamics, Space) open the detailed pages. KID in their side rail brings you back.

## How Kid decides

Kid's pipeline: analysis → diagnosis → direction → plan → validation → apply → your feedback. Each step:

1. **Analysis.** Kid measures your vocal; nothing in this list is guessed.
   * **From a file:**
     * level, peak and clipping;
     * loud and quiet levels;
     * crest factor and how much the level moves;
     * a third-octave tone shape from 200 Hz to 12.8 kHz;
     * sibilance;
     * stereo correlation;
     * pitch (how much is clearly pitched, the range, how far from the nearest note);
     * a key, only when the evidence is strong;
     * the background level between phrases.
   * **From the live input:** levels, five tone bands and sibilance.
2. **Diagnosis.** Kid turns the measurements into findings. Each finding is marked *measured* or *estimate*, has a plain sentence and the evidence behind it, and names a frequency only where the analysis supports one:
   * clipping;
   * a low recording level;
   * background noise;
   * possible rumble;
   * low-mid build-up;
   * harsh upper mids, either broad or concentrated in one band;
   * sharp S's;
   * a dark or an already-bright top;
   * a jumpy level;
   * an already-compressed take;
   * notes that drift off pitch;
   * the key.
3. **Direction.** Your words (and artist references, as an original direction, never a copy) become a musical direction with memory.
4. **Plan.** The plan has three layers:
   * **Corrections** come from the diagnosis:
     * CLEAN's rumble filter, low-mid bell and de-esser;
     * a wide EQ cut for broad harshness, or a narrower cut plus CLEAN's resonance finder for a sharp spot;
     * a gentle air lift for a dark top;
     * compression set from your measured level, kept light on a take that is already compressed or noisy.
   * **The direction** is then layered on top, adapted to the recording. For example:
     * "brighter" never lifts upper mids that are already sharp;
     * an already-bright vocal gets less top;
     * "warmer" doesn't add low end where the low mids are already built up.
   * **A profile or Build My Mix** starts from the existing Build My Mix engine's foundation.
5. **Validation.** Every value is clamped to the parameter's real range and step. Kid can never write Gain, Mix, the output limiter or Bypass. It sets Pitch Tune's key and scale only from a reliable estimate.
6. **Apply.** The mix goes through the normal parameter path, so your host sees ordinary automation-compatible moves and the session saves as usual.
7. **Feedback.** "Less reverb", "make the tuning harder", "more raw", "keep my vocal natural", "more like that reference", "keep the center strong" all change the *same* mix: the plan is always rebuilt from the sound you started with, so nothing piles up. A control you move by hand is yours: Kid leaves it alone and says so.

## Asking Kid

| Ask | What you get |
|---|---|
| "Why did you reduce the high mids?" | The real changes in that area, with their reasons and measurements. If Kid didn't touch it, it says so. |
| "What did you change?" | A one-line summary and the main reasons. |
| "Analyze my vocal" | Every finding, labelled measured or estimate. |
| "What key is it in?" | The estimate, or "I can't tell reliably", never a guess. |

Questions never change the sound.

## What Kid can't do

GlitchKid has no:
* reverb (space comes from Echo and Dimension);
* noise reduction;
* clipping repair;
* timing correction.

Kid says so instead of pretending. Kid only hears this track (and the beat, if you route it to the instrumental input), never your whole project.

## Speech

* **Push-to-talk only**, on your computer's default input, separate from the track. What you say never enters the vocal chain and is discarded after it is turned into text.
* **Recognition runs on your computer** (whisper.cpp).
* **Set up speech** downloads the model once (about 142 MB) and checks its checksum before use. Typing always works.

## AI

Kid is local, deterministic, rule-based software built on real measurements. There is no language model and no network service.

## Not verified yet

* Any of this inside FL Studio or another host.
* Real vocals. The tests use synthetic sung lines and noise.
* The diagnosis thresholds. They are production heuristics and need tuning on real recordings.
