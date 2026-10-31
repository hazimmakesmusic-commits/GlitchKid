# Vocal Producer — audit and plan (this build)

## Audit (repository at `830cbbd`)

| Area | What exists | Status |
|---|---|---|
| Framework | JUCE 9.0.4, VST3 only, CMake + FetchContent; the editor is JUCE components with custom drawing (no WebView) | Real |
| Parameters | 69 APVTS parameters with stable IDs; state = APVTS XML; UI preferences in the state (`uiLevel`, `uiCategory`, ...) | Real, unchanged by this work |
| Signal order | Pitch Tune, Formant, Harmony, Mutation, Doubler, 4-band EQ, CLEAN corrective, Compressor, CLEAN de-esser, Saturation, AIR LAB, Stutter, Echo, Dimension, Gain, Mix, Output Limiter | Real |
| Pitch correction | YIN detector, scale quantiser, PSOLA shifter (`DSP/PitchCorrection`) | Real |
| Cleanup | CleanLab: rumble, low-mid (mud), up to 3 resonances, de-esser; learns the voice; telemetry | Real. **No noise reduction exists.** |
| Space | LiquidEcho (delay), Dimension (chorus-style width), Doubler. **No reverb exists.** | Real |
| Loudness compare | Bypass = exact, time-aligned dry signal (20 ms crossfade). **No level matching.** | Real |
| Analysis | VocalAnalyzer (live input, 2-8 s): peak, RMS, crest, clipping, dynamic range, 5 bands, sibilance | Real |
| Profiles / presets | 8 factory starting profiles, user presets (save/load/delete), A/B slots, Build My Mix engine | Real |
| KID | Rule-based director (no model), review then Apply, push-to-talk speech via whisper.cpp (local) | Real; speech unverified in a host |
| Reference image | The VocalDose reference image was **not found** in the repository or the session's uploads; the design follows the brief's description | — |

## What this build adds

1. **Level-matched A/B (DSP).** Opt-in loudness matching for ORIGINAL vs PROCESSED:
   * While processed audio plays, the processor measures the long-term loudness of the processed output against the dry input.
   * While ORIGINAL plays (with matching on), the dry signal is played at that loudness.
   * It is real-time safe: no allocation and no locks. It is off by default, so bypass stays exact.
2. **Vocal file analysis.** Load a vocal file and analyse it on a background thread:
   * level and clipping;
   * an estimated noise floor;
   * dynamics, brightness, low-mid build-up and sibilance;
   * pitch: voiced share, range, stability, and an estimated key **only when the evidence is strong**;
   * stereo or mono;
   * a waveform overview.

   Measurements and estimates are labelled as such.
3. **The KID engine grows:**
   * four original **sound profiles**: Melodic Pop-Rap, Atmospheric Rap, Intimate Rap, Experimental Vocal;
   * **artist references** mapped to a profile *direction*, with a stated disclaimer;
   * **tuning words** (harder, natural, emotional, less robotic), so KID may now propose Pitch Tune on, strength and speed, and the key and scale only from a reliable estimate;
   * **Build My Mix**: profile + analysis → a mix that is applied with Undo;
   * **Clean My Vocal**: Gentle, Balanced or Aggressive, using CleanLab only.

   It still never touches Gain, Mix, the limiter or Bypass.
4. **The Vocal Producer screen** (the KID page, redesigned as an instrument):
   * a central display with an original GlitchKid mascot that reacts to states;
   * a source strip (live input or loaded file, with waveform and analysis);
   * the command bar (type or push-to-talk);
   * six large controls bound to real parameters: Clean, Tune, Style, Space, Presence, Mix;
   * a big ORIGINAL / PROCESSED switch with level matching;
   * profile buttons, Build My Mix and Clean My Vocal.
5. **The advanced workspace** is the existing pages (CLEAN, TUNE, TONE, DYNAMICS, SPACE, LAB), reached from the rail. KID is the way back.

## Not done here, and why

* **Playing a loaded file through GlitchKid.** A VST3 effect processes the audio the host gives it. An internal file player would replace your track's audio, which is a different product decision. Analysis of a file works; playback stays in the DAW.
* **Noise reduction, reverb, clipping repair, timing correction.** No such DSP exists. KID says so instead of pretending.
* **An external AI service.** None is connected. The deterministic local engine is the product, and AI_ARCHITECTURE.md (design folder) describes the optional interface.

## Risks

* The 1120 × 680 layout and the mascot animation must stay light. They are drawn on the message thread only while visible.
* Key estimation is unreliable on short or rap-heavy takes. It is reported only above a confidence threshold.

## Status after this build

The audit and plan above were written before the work. Status now:

| Item | Status |
|---|---|
| 1. Level-matched A/B | Done and tested (+6 dB and -6 dB gain: measured within 0.3 dB, ORIGINAL within 0.5 dB of PROCESSED; matching off is bit-exact) |
| 2. Vocal file analysis | Done and tested on synthetic files (key, noise floor, clipping, identical stereo channels, errors, cancel) |
| 3. KID profiles, artist references, tuning, Build My Mix, Clean My Vocal | Done and tested |
| 4. The Vocal Producer screen | Done; tested in the editor (dials, compare, file, build, clean, error) and checked on rendered snapshots |
| 5. Advanced workspace | The existing pages, linked from the screen |

Still to verify:

* in FL Studio and other hosts;
* on real vocals.
