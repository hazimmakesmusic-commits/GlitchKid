# KID — your vocal director

KID is the first workspace in GlitchKid. You tell it what you hear in your head; it proposes changes to GlitchKid's own controls; nothing changes until you press **Apply**.

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

* KID may propose 57 of the 69 parameters.
* **It never touches Pitch Tune, Gain, Mix, the Output Limiter or Bypass.** Asking for "louder" or "autotune" gets a reason, not a change.
* GlitchKid has no reverb. "Reverb" is answered honestly and built from Echo and Dimension.
* Every value is clamped to the parameter's own range and step before it is shown.
* KID understands a fixed vocabulary of musical words: warm, bright, dark, airy, clear, upfront, aggressive, soft, wide, narrow, spacious, distant, dreamy, close, clean, muddy, harsh, experimental, glitchy, melodic, rap, natural, ad-libs, plus "a little", "very", "less", "more" and "keep …". The full map is `design-prototypes/glitchkid-kid-vocal-director-v1/INTENT_PARAMETER_MAP.md`.
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

* A real transcription. The model couldn't be downloaded in the build environment; the code paths around it are tested.
* The command microphone inside FL Studio on macOS and Windows.
* The new 1120 × 680 layout on a real screen.
