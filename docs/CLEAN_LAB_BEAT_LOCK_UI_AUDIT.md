# CLEAN LAB / BEAT LOCK / Experimental Lab UI: audit

Written before this pass's implementation, from the source at commit `89e9b97` (clean working tree), not from earlier reports. Linux only: nothing here was tested in FL Studio, on macOS or on Windows.

## 1. What the code does today

### Signal chain (`GlitchKidProcessor::processBlock`)
input -> NaN/Inf zeroing -> dry copy (delay-matched) -> Pitch Tune -> Formant -> Harmony -> Mutation -> Doubler -> user EQ (4 bands) -> CLEAN LAB corrective + resonance -> compressor -> CLEAN LAB de-esser -> saturation (4x oversampled, 6 samples latency) -> AIR LAB -> stutter -> echo -> Dimension -> gain -> Mix / Bypass crossfade against the delay-matched dry copy. Latency 6 samples (Pitch Tune adds its own, reported). 65 parameters; IDs are permanent and new ones are only appended.

### Feature status

| Feature | Status | Evidence |
|---|---|---|
| Pitch correction, key and scale | Implemented and tested | `Tests/FormantTuneTests.cpp` and others; UI: key pad, scale chips, styles |
| 4-band parametric EQ and a response graph | Implemented and tested | graph curve compared with the real DSP in `MacroTests` |
| Compressor | Implemented and tested | |
| Corrective EQ (rumble, low-mid), dynamic resonance suppression (up to 3 bells), de-esser | Implemented and tested on synthetic signals (`Tests/CleanLabTests.cpp`); **not validated on real vocals** | zero latency; bounded (3.5 / 6 / 9 dB); learns only while the voice moves |
| CLEAN amount, bypass crossfade, telemetry, EQ-graph overlay | Implemented and tested | |
| Dedicated de-esser | Exists (inside CLEAN LAB) | sibilance/body ratio, 7.5 kHz bell, <= 9 dB x depth |
| Reverb | **Missing** | SPACE = echo, width (Dimension), doubler only |
| AIR LAB, delay, width | Implemented and tested | |
| Formant, harmony, mutation, stutter, Alter Ego | Implemented, hidden from the UI (kept for old sessions; BUILD switches them off) | |
| BUILD MY VOCAL MIX | Implemented and tested (vocal-only and beat-informed) | `MixEngineTests` |
| Instrumental input | Implemented and unit-tested with synthetic audio: an optional aux bus, **never mixed into the output**; status notConnected / connectedSilent / live | `SourceTests` |
| BEAT LOCK engine and workspace | Implemented and tested (engine and editor) | `BeatLockTests`, `LevelUiTests` |
| A/B, undo (BACK TO MY SOUND / UNDO), presets, saved state | Implemented and tested | |
| Real-time waveform / spectrum display | **Partial before this pass**: only a rolling input-peak strip and peak meters existed. No audio data reached the interface except peaks | |
| FL Studio / macOS / Windows validation, pluginval | **Not done / blocked** (this environment is Linux without those tools) | |

### Beat routing (checked in code)
* The plug-in declares an optional second input bus "Instrumental" (off by default, mono or stereo, `isBusesLayoutSupported`). The audio thread measures it (`beatAnalyzer`, a peak, the silent-seconds counter) and never adds it to the output. A single stereo input that already holds vocal and beat mixed is measured as the vocal; GlitchKid cannot separate it, and every screen that mentions a beat says so.
* `getInstrumentalStatus()` is derived from the real bus state and signal. The routing has **never been verified in FL Studio**; if the host does not enable the bus, BEAT LOCK stays at "no beat input".

### UI architecture (before this pass)
* `Source/PluginEditor.{h,cpp}`: one editor, a 960 x 600 design scaled to the window, a hidden-controls-per-page layout (`layoutContent`), a 30 Hz timer, parameter attachments for every control, an undo struct, the BUILD and BEAT LOCK state machines.
* `Source/UI/PlaygroundWidgets.*`: palette tokens, look and feel, dial, chain strip, EQ graph, profile cards, note keys, the BUILD button.
* The previous milestone had restyled **everything as an 8-bit pixel interface with a level-select screen** (a pixel font, stepped corners, segmented meters, "LEVEL 01" cards). That direction is now **rejected** and is removed in this pass; the two files that carried it (`PixelKit.*`) are deleted and the widgets return to sharp, vector, technical drawing.
* UI preferences (`uiLevel`, `uiCategory`, `uiMotion`, `uiProfile`) live in the saved state, not in sounds, presets or A/B.
* Existing design tokens: the locked palette (Void Black, Deep Graphite, Ultraviolet, Cyber Pink, Pale Chrome, Muted Lavender Grey, Divider Violet; red only for errors/clipping); Inter Regular and Bold are the only bundled fonts.

## 2. Gaps this pass addresses
1. The interface must be the Experimental Lab look (no pixel styling, no level select, no mascot, no fake data).
2. A central, real-time waveform/spectrum built from genuine audio data, with a safe audio-thread to UI transfer.
3. A compact persistent navigation between five workspaces (HOME, CLEAN LAB, BEAT LOCK, BUILD MY VOCAL MIX, FINAL TOUCH), no wizard.
4. CLEAN LAB: a large EQ graph with the real vocal spectrum behind it, simple controls first and the three section trims behind an ADVANCED switch.
5. BEAT LOCK: a real spectrum of the vocal and, only while a beat really arrives, of the instrumental.

## 3. Compatibility risks (and how they are handled)
* Parameter IDs / count: no DSP parameters are added (the count stays 65). The display taps are plain audio-thread writes into fixed arrays; they are not parameters.
* Saved state: the UI keys are plain properties. `uiLevel` (0-4) is kept with the same meaning (0 = HOME, 1 = CLEAN LAB, 2 = BEAT LOCK, 3 = BUILD, 4 = FINAL TOUCH); a new `uiCleanAdv` is added; both default when absent. The older `uiPage` is ignored.
* The editor may be opened and closed freely; nothing in the sound depends on it.
* Audio thread: the only additions are three bounded lock-free taps (no allocation, no lock, no I/O). The tests include the existing allocation counter over the processing path.

## 4. Known limitations before this pass
No real-vocal listening, no host test, no pluginval, no reverb, no noise / room / click / breath / plosive reduction (CLEAN LAB does not claim any of them), masking figures are estimates from five coarse bands, and digital levels are not LUFS.
