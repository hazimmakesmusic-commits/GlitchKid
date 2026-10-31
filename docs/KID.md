# KID — your vocal producer

KID is the first workspace in GlitchKid. You tell it what you hear in your head; it proposes changes to GlitchKid's own controls; nothing changes until you press **Apply**.

## The screen

* **YOUR VOCAL.**
  * **LOAD VOCAL FILE** analyses a recording on a background thread. It reads the first 3 minutes and shows the waveform.
    * The file is only read: it is not played and not copied. You hear GlitchKid by playing your track in the DAW.
  * **LISTEN LIVE** listens to the vocal on this track while your project plays.
  * GlitchKid only hears this track, never your whole project.
* **The display.** The GlitchKid mascot shows what is happening: idle, listening, analysing, building, a take waiting, applied, or an error. Below it is KID's conversation.
* **KID'S TAKE** has three tabs:
  * **TAKE**: every proposed change.
  * **VOCAL READ**: what was measured and what is only estimated, labelled as such.
  * **LIMITS**: what KID can and can't do.
* **Six dials.** Each one moves real controls, and each follows them when you change things elsewhere:

  | Dial | What it moves |
  |---|---|
  | CLEAN | the CLEAN LAB amount (0 = off) |
  | TUNE | Pitch Tune strength (0 = off) |
  | STYLE | the PUNCH macro (compression) |
  | SPACE | the SPACE macro (echo, Dimension, doubles) |
  | PRESENCE | the EQ presence band |
  | MIX | Mix |

* **BEFORE / AFTER.** ORIGINAL is the real bypass. With **LEVEL MATCH** on (the default on this screen), ORIGINAL plays at the loudness GlitchKid measured for PROCESSED, so the louder version never wins.
  * PROCESSED has to have played for a few seconds first.
  * Everywhere else, bypass is the exact untouched signal.
* **SOUND PROFILES.** Four original directions:
  * **Melodic Pop-Rap**: Light tuning;
  * **Atmospheric Rap**: Light tuning, wide and spacious, with the low mids kept in check;
  * **Intimate Rap**: Natural tuning, close and present;
  * **Experimental Vocal**: Hard tuning, Mutation and width.

  Each starts from a factory profile, fitted to your voice by the Build My Mix engine.
  * Naming an artist ("make it sound like ...") picks the nearest profile as a *direction*. KID says plainly that it does not copy anyone's voice or vocal chain.
  * To keep a sound you like, save it as a preset (bottom bar).
* **BUILD MY MIX** reads your vocal (the loaded file first, otherwise the live input) and builds a whole mix from the chosen profile, explained in one sentence.
* **CLEAN MY VOCAL** (Gentle / Balanced / Aggressive) uses CLEAN LAB only: rumble, low-mid build-up, resonances and harsh S's.
  * It is not noise reduction.
  * Aggressive can thin your tone.
* **The links** under BEFORE / AFTER (Tune, Clean, Dynamics, Tone, Space, Creative) open the detailed pages. Press KID in the rail to come back.

## Using KID

1. **Talk or type.**
   * Hold the mic button and speak, then release. You can also tap once to start and tap again to stop.
   * Or type in the box and press Enter.
   * Or tap a suggestion.
2. **Read the take.** KID'S TAKE lists every change:
   * the module and the parameter;
   * the current value → the proposed value;
   * why, and the parameter ID.

   THE PATH glows on the stages KID wants to change.
3. **Shape it by talking.** Follow-ups change the same take:
   * "Too much. Make it subtle" scales it to 40 %.
   * "A little less" scales it to 70 %.
   * "More" scales it to 150 %.
   * "Keep the vocal upfront" caps echo, doubles and Dimension, and protects Presence.
   * "Keep the presence".
   * "Start over".
4. **Untick** anything you don't want. If you move a proposed control by hand, KID unticks that change and starts from your sound.
5. **Apply** writes the ticked changes through the normal parameter path (your host sees normal parameter moves). **UNDO** (top right) brings your previous sound back.

"I don't know what it needs. Improve it" and cleaning requests first listen to your vocal. Press play in your project; KID needs 2–8 seconds of singing or rapping. The VOCAL READ tab then shows what was measured.

## What KID will and won't do

* KID may propose 62 of the 69 parameters.
* **It never touches Gain, Mix, the Output Limiter or Bypass.** Asking for "louder" gets a reason, not a change.
* **Pitch Tune:** KID can switch it on and choose a style (Natural, Light or Hard), for example for "make the tuning harder" or "less robotic".
  * It sets the key and scale only from a *reliable* estimate of a loaded vocal file.
  * Otherwise Pitch Tune keeps the key set on TUNE, and KID says so.
* GlitchKid has no reverb. "Reverb" is answered honestly and built from Echo and Dimension.
* Every value is clamped to the parameter's own range and step before it is shown.
* KID understands a fixed vocabulary of musical words: warm, bright, dark, airy, clear, upfront, aggressive, soft, wide, narrow, spacious, distant, dreamy, close, clean, muddy, harsh, experimental, glitchy, melodic, rap, natural, ad-libs, emotional, expensive, "sit better with the beat", tuning words ("autotune it", "make the tuning harder", "less robotic", "no autotune"), the four profile names and the artist references mapped to them, "clean my vocal (gently / aggressively)" and "build my mix", plus "a little", "very", "less", "more" and "keep …". The full map is `design-prototypes/glitchkid-kid-vocal-director-v1/INTENT_PARAMETER_MAP.md`.
* It is not a chatbot, and there is no AI model.

## Speech

* **Push-to-talk only.** The command microphone is your computer's default input, opened only while you hold the button. It is separate from the track: what you say never enters the vocal chain, the analyser or your project, and it is deleted after it is turned into text.
* **Speech recognition runs on your computer** (whisper.cpp). Nothing is sent anywhere.
* **First use: SET UP SPEECH** downloads the speech model once (about 142 MB from the whisper.cpp project). KID checks it against its published checksum before using it. Typing works without it.
* **macOS:** your host (FL Studio) needs microphone permission (System Settings > Privacy & Security > Microphone).
* **Windows:** the command mic uses the normal Windows audio system, not ASIO, so it doesn't compete with your audio interface's ASIO driver.

## LAB

LAB holds the hands-on controls for the four creative effects, each in its fixed place in the chain:
* Formant (#2);
* Harmony (#3);
* Mutation (#4);
* Stutter (#12).

KID can propose Mutation ("experimental") and Stutter ("glitchy"); this is where you fine-tune them.

## Not verified yet

* The level-matched compare, the producer screen and file loading inside a real host (they are tested in the build environment only).
* Key estimation on real, varied vocals. The tests use synthetic sung lines; rap and short takes are expected to give "no key", by design.
* A real transcription. The model couldn't be downloaded in the build environment; the code paths around it are tested.
* The command microphone inside FL Studio on macOS and Windows.
* The new 1120 × 680 layout on a real screen.
