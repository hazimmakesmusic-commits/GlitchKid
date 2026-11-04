# Control mapping — The Session That Talks Back

Every visible control, idea, world and suggestion in the prototype is listed here with what it really does in GlitchKid. Facts are taken from the source on branch `claude/glitchkid-plugin-architecture-3o6w74` (HEAD `1f5319a`), on 2026-11-04.

How to read the **Status** column:

| Status | Meaning |
|---|---|
| **Exists** | GlitchKid does this today. The prototype changes only its presentation. |
| **Exists, new UI** | The engine exists. The way it is reached, reviewed or shown is new. |
| **NEW** | Not in the plug-in. Production would need new engineering. It never needs new DSP. |
| **SIMULATED** | Faked in the prototype and labelled on screen. |
| **Prototype only** | Outside the plug-in window, for demonstrating states. |

Nothing in this concept adds a DSP feature. Every sound change writes existing parameters, within their real ranges: the 68 sound parameters plus Bypass and the `master_*` parameters, all from `createLayout` in `Source/PluginProcessor.cpp`.

## 1. Top bar

| Control | What it does | Real behaviour / parameter | Status |
|---|---|---|---|
| Tape label (session name) | Click to rename; any language | No session name exists today. It would be a string in the plug-in state, saved with the project. | NEW |
| Sound chip | Shows where the sound came from (Default, Factory, Draft world, Saved sound, Built, Notebook, Deleted) and whether it changed (● Unsaved / ● Not kept yet). Click → WORLDS | The preset name is real (`getPresetName`). The source types and the comparison with the loaded snapshot are new UI. | Exists, new UI |
| A / B | Empty slot: stores the current sound, without asking. Occupied slot: recalls it. Asks before you leave a changed slot, and before storing over a different sound. ⤓ stores. | `hasAbSlot`, `captureAbSlot`, `recallAbSlot`, `isAbSlotModified` (PluginProcessor.h). Today SET A / SET B capture without asking. The confirmations follow the rules approved in the v2 prototype. | Exists, new UI |
| Undo | One history for every change: knobs, keeps, worlds, tools, A/B switches | Today: one step back after a build, profile load or Beat Lock change (`PluginEditor::undo`), and 50 steps inside the KID panel (`KidPanel.cpp`). No project-wide history exists. | NEW (history) |
| Redo | Brings back what Undo took | No redo exists (no `UndoManager` in the source). | NEW |
| Save… | Saves a preset for every project. Asks before replacing. Refuses empty names, file-system characters and factory names. | `PresetController::saveAs`, `needsOverwriteConfirmation`, `isFactoryName` | Exists |
| DRY / PROCESSED (D) | Compare with your original vocal | Bypass, heard at equal loudness through the processor's level-matched compare (`setLevelMatchedCompare`, PluginProcessor.h:254–258) | Exists |
| ? (Help) | Shortcuts, where play is, how KID works | — | UI only |

## 2. Rail, chain strip, help line

| Control | Real behaviour | Status |
|---|---|---|
| SESSION, SHAPE, LAB, WORLDS, TOOLS, MASTER | Areas of one window. SHAPE and LAB group the existing modules. Grouping never changes the processing order. | Exists, new UI |
| KID (rail, with a count of new ideas) | Opens the session and focuses the KID input. Holding **K** anywhere talks to KID. | Exists, new UI |
| Chain strip 1→17 | The fixed processing order (`processChunk`): 1 Tune, 2 Formant, 3 Harmony, 4 Mutation, 5 Doubler, 6 EQ, 7 CLEAN corrective, 8 Compressor, 9 CLEAN de-esser, 10 Warmth, 11 AIR LAB, 12 Stutter, 13 Echo, 14 Dimension, 15 Gain, 16 Mix, 17 Output Limiter. A lit chip = the module is on in what you are hearing. Click opens the module. At the compact size, long names are shortened (FORM, HARM, MUT, DBL, WARM, STUT, DIM, LIM); the full name stays in the chip's label and help. | Exists |
| Help line | Describes whatever is under the pointer | UI only |

## 3. SESSION

### The take

| Element | Real behaviour | Status |
|---|---|---|
| “LIVE · from FL Studio” / “FILE · look only” | The vocal comes from FL Studio on this Mixer insert. A loaded file is for analysis only: GlitchKid can't play files. | Exists |
| “FL Studio playing · 92 BPM” | BPM comes from the host playhead (`getPosition()->getBpm()`, PluginProcessor.cpp:857–859). Playing or stopped is **not read today**; reading `isPlaying` from the same playhead would be a small addition. | Exists (BPM), NEW (playing state) |
| Waveform | Drawn from a simulated take. Its shape follows real settings: colour from the EQ tone, spread from Doubler and Dimension, echoes from Echo, double edges from Mutation, Formant and Harmony, dashes from Stutter. It is cyan when DRY and framed while you audition. In production it would draw the real input history (new UI). | SIMULATED (labelled SIMULATED TAKE) |
| HEARING · PROCESSED / DRY / “IDEA (NOT KEPT)” | What you hear now: Bypass, or an audition | Exists (Bypass), NEW (audition) |
| IN / OUT meters, CLIP | Levels move with the simulated take. The plug-in has real meters (`inputPeak`, `outputPeak`, `LevelMeter`). | SIMULATED here |
| “Look at a file…” | Loads a file for looking and measuring only | Exists (analysis); SIMULATED here |
| Messages on the waveform | No vocal yet · Paused · Not processing · Mastering mode is on | UI only. “Not processing” has no verified trigger in the source: it is a design placeholder. |

### Ideas on the desk (cards)

Every card offers the same actions:

| Action | Real behaviour | Status |
|---|---|---|
| **Hear** | Audition the idea: you hear it, the parameters don't change. The chain and the waveform show what you hear. | **NEW**: a processor-side preview (see OPEN_DECISIONS §1) |
| **Look inside · N changes** | Lists every real control the idea changes, in processing order, with from → to and a reason. You can tick or untick each one, listen to “Current” or “With these”, then Keep or Cancel. If you move one of those controls by hand while the list is open, its row is unticked and your value is kept. | Exists, new UI (Beat Lock already has per-move ticks). Listening uses the NEW preview. |
| **Keep** | Writes the idea's values to the real parameters. One undo step. | Exists (parameter writes) |
| **✕ Put away** | Removes the card from the desk. Nothing changes; the toast offers “Bring back”. | UI only |

The five idea cards. Values marked “rel.” are relative to the sound you have now; “if on” means the value is only written if that module is already on.

| Idea | What it writes (real parameter IDs) |
|---|---|
| **Ghost Double** | `dbl_on` 1 · `dbl_amount` 18 % · `dbl_width` 60 % · `dbl_detune` 12 ct · `dbl_timing` 30 ms |
| **Wide Dream** | `dim_on` 1 · `dim_width` 85 % · `dim_depth` 60 % · `dim_mix` 35 % (or more if already on) · `dbl_on` 1 · `dbl_amount` 22 % (or more if already on) · `dbl_width` 90 % · `echo_width` 70 % (if echo on) · `eq_b2_gain` +1 dB rel. |
| **Broken Radio** | `eq_lo_gain` −12 dB · `eq_lo_freq` 350 Hz · `eq_hi_gain` −12 dB · `eq_hi_freq` 3.5 kHz · `eq_b2_gain` +5 dB · `eq_b2_freq` 1.8 kHz · `sat_drive` at least 11 dB · `mut_on` 1 · `mut_amount` 18 % · `mut_character` 75 % · `mut_blend` 30 % |
| **Close & Warm** | `eq_lo_gain` +1.5 dB rel. · `eq_hi_gain` −1.5 dB rel. · `sat_drive` at least 2.5 dB · `comp_ratio` at least 2.5:1 · `comp_attack` 18 ms · `comp_release` 160 ms · `echo_mix` at most 8 % (if on) · `dim_mix` at most 10 % (if on) · `dbl_amount` at most 10 % (if on) |
| **Afterimage** | `echo_on` 1 · `echo_sync` 1 · `echo_div` 1/4 dotted · `echo_fb` 55 % · `echo_tone` 2.5 kHz · `echo_width` 60 % · `echo_mix` 28 % · `dim_on` 1 · `dim_mix` 25 % (or more if already on) |

None of them touches Gain, Mix, the Output Limiter, Bypass, or your key and scale. QA section 0 checks that every value is a real parameter inside its range, from two starting sounds. The recipes are designed in this prototype and not listening-tested (OPEN_DECISIONS §6).

**Happy accident.** It picks 3 different move families at random (seeded, so the run is repeatable) from 8. All of them are real parameters:

| Family | Values |
|---|---|
| Formant | `formant_shift` −3…+3 st, never 0 |
| Mutation | on · amount 10–45 % · character 0–100 % · blend 20–60 % |
| Harmony | on · interval: octave down, fifth down, minor 3rd up, major 3rd up, fifth up or octave up · level −18…−10 dB |
| Stutter | on · slice 1/8, 1/16 or 1/32 · mix 15–40 % |
| Echo | on · sync · 1/4, 1/4 dotted, 1/8, 1/8 dotted or 1/8 triplet · feedback 20–60 % · mix 10–35 % · tone 1.5–7.5 kHz |
| Dimension | on · mix 10–45 % · depth 30–90 % · detune 3–15 ct |
| Warmth | `sat_drive` 2–8 dB |
| EQ tilt | Air shelf −6…+3 dB · Body −3…+1 dB |

Status: NEW as a feature, built only from existing parameters.

### KID's note (the sticky note)

One note at a time; ✕ dismisses it. The notes come only from things GlitchKid really knows:

| Note | Source | Status |
|---|---|---|
| Fresh session | The preset is Factory Default and unchanged | Exists (state) |
| No vocal yet | The input level | Exists (input peak) |
| Clipping before it reaches GlitchKid | The input level (CLIP) | Exists (input peak) |
| Echo isn't locked to the song (with the host BPM) → “Lock echo to tempo” | `echo_on`, `echo_sync`, host BPM | Exists |
| Pitch Tune pulls to every note (Chromatic) → “Open TUNE” | `tune_on`, `tune_scale` | Exists |
| Low-mid build-up near 300 Hz → “Ideas with CLEAN” | A measurement of your take, labelled **SIMULATED MEASUREMENT**. In production: the existing `VocalDiagnosis` “mud” finding (low-mid build-up, VocalDiagnosis.cpp:78–98). | SIMULATED here |
| “X is yours now. Keep it in the notebook?” | You just kept an idea | NEW (notebook) |
| Mastering mode is on | `master_on` | Exists |
| KID's reply to what you said | See the KID bar | — |

“KID notes: on / only when I ask” is a setting in the intro and in Help (NEW, a UI preference).

### KID bar: “Tell KID · guided assistant”

| Element | Real behaviour | Status |
|---|---|---|
| Mic (hold), or hold **K** | `CommandMic`: a separate input device, open only while held. Audio goes to whisper.cpp on this computer, then is discarded; it never reaches the vocal chain or the saved session (`KidSpeech.h`). | Exists |
| Text field + Send | The same path as speech, without the mic | Exists |
| Status line | Listening… → Transcribing on this computer… → Reading your words… → Leaving ideas on the desk… → Ideas are on the desk. Also: Unavailable · Didn't catch that · Speech isn't set up · Microphone blocked. | Exists, new UI |
| Interpretation | `KidDirector`'s deterministic rules. Words map to directions (bright, warm, presence, punch, width, depth, clean, strange); “keep …” locks protect things (e.g. *keep the words clear* locks the presence band, `KidDirector.cpp` keeps[]). Follow-ups (“a little less”, “keep the vocal upfront”) revise the same ideas while they are on the desk; “start over” begins again. Never writes Gain, Mix, the limiter, Bypass, or your key and scale (`KidDirector::isAllowed`). | Exists. The prototype's rules are modelled on KidDirector, not its exact output. |
| **Three directions** | 1 *straight from what you said* · 2 *same idea, words kept up front* (adds the upfront lock, halves depth, 70 % width) · 3 *pushed further, a little stranger* (more depth, width and Mutation). Each is a card. | NEW (the engine makes one proposal today) |
| New words | *rough, rougher, gritty, dirty, distorted, crunchy* → warmth + punch; *unstable, wobbly, broken, lo-fi* → Mutation. KID says they are new words. | NEW vocabulary (not in KidDirector's lexicon) |
| Honest answers | Timing → that's editing in FL Studio · noise or breaths → no gate here, CLEAN does rumble/harshness/S sounds · copying a voice → never copies, asks what you like about it · mastering/loudness → MASTER · the beat → Beat Lock · reverb → “GlitchKid has no reverb; the space here is Echo + Dimension” | UI copy over existing facts |
| “Build my mix” | Opens Build My Mix in TOOLS | Exists |

The words KID shows (its replies and directions) are written by this prototype: SIMULATED wording.

### Session notebook

| Control | Real behaviour | Status |
|---|---|---|
| Keep this sound | Keeps the current sound with a name (required) and an optional note | **NEW**: per-project state, saved with the project |
| Hear | Audition an entry without switching | NEW (preview) |
| Use | Switch to it (one undo step). It asks first if your current sound isn't kept anywhere. | NEW (state), Exists (parameter writes) |
| Rename / ✕ Remove | Change the name or note / remove (asks first; your sound doesn't change) | NEW |
| ON NOW · ON NOW · CHANGED | The entry is what you hear / it was, and you have changed it since | NEW |

The notebook is separate from presets (Save…, every project) and from A/B (two quick slots).

## 4. SHAPE (stages in brackets)

Every knob, switch and menu writes the parameter named, with its real range, default and skew. Double-click resets a knob; arrow keys, scroll and drag move it. Each module has **Reset**, which is one undo step.

| Tab | Controls → parameters |
|---|---|
| CLEAN (7·9) | On `clean_on` · Amount `clean_amount` · Advanced: Corrective EQ `clean_tone`, Resonance `clean_res`, De-esser `clean_ess` · “What CLEAN is doing” readout: SIMULATED |
| TUNE (1) | On `tune_on` · Key `tune_key` · Scale `tune_scale` · Style Natural / Light / Hard = strength 50/75/100 %, speed 80/35/0 ms (MacroEngine) · Advanced: Strength `tune_strength`, Speed `tune_speed` · pitch readout: SIMULATED |
| TONE (6·10·11) | EQ: Low `eq_lo_gain` / `eq_lo_freq`, Body `eq_b1_gain` / `eq_b1_freq` (`eq_b1_q`), Presence `eq_b2_gain` / `eq_b2_freq` (`eq_b2_q`), Air shelf `eq_hi_gain` / `eq_hi_freq` · response curve: approximate, drawn from the settings · Warmth `sat_drive` · AIR LAB: `air_on`, `air_presence`, `air_air`, `air_link`, Advanced `air_mix`, `air_trim` |
| DYNAMICS (8) | Threshold `comp_thresh` · Ratio `comp_ratio` · Advanced: `comp_attack`, `comp_release`, `comp_knee`, `comp_makeup` · gain reduction readout: SIMULATED |
| SPACE (5·13·14) | Doubler `dbl_on`, `dbl_amount`, `dbl_width`, More: `dbl_detune`, `dbl_timing` · Echo `echo_on`, `echo_mix`, Sync `echo_sync`, Division `echo_div`, More: `echo_fb`, `echo_tone`, `echo_width`, `echo_time` · Dimension `dim_on`, `dim_mix`, `dim_width`, More: `dim_depth`, `dim_detune` |
| OUTPUT (15–17) | Gain `gain` · Mix `mix` · Limiter `lim_on`, Ceiling `lim_ceiling`, More: `lim_drive`, `lim_release` |

## 5. LAB (experimental modules)

| Card | Parameters |
|---|---|
| Formant (2) | `formant_shift` |
| Harmony (3) | `harm_on`, `harm_interval`, `harm_level` |
| Mutation (4) | `mut_on`, `mut_amount`, More: `mut_character`, `mut_blend` |
| Stutter (12) | `stutter_on`, `stutter_rate`, `stutter_mix` |

All exist today.

## 6. WORLDS

Every tape has **Hear** (audition without loading, NEW preview), **Look** (the review list; take all of it, or only what you tick) and **Use** (load it; one undo step; asks first if your current sound isn't kept). The tape in use also shows **Customise in SHAPE** and, once you change something, **Keep my version** (to the notebook).

| Shelf | What it is | Status |
|---|---|---|
| your saved sounds | Your presets (Save…), in every project. ✕ deletes, after asking. | Exists (`PresetController`) |
| factory tapes | Factory Default + the 8 factory profiles with their exact `Profiles.inc` overrides. Loading keeps the 22 user-owned parameters: `tune_*` (5), `gain`, `mix`, `air_*` (6), `clean_*` (5), `lim_*` (4). | Exists |
| draft worlds | Six **valid parameter states** designed in this prototype. Like a profile, each is Factory Default + overrides and keeps the 22 user-owned parameters. Labelled **DRAFT · NOT LISTENING-TESTED**. They are not presets and would need sound design before shipping. | NEW content (no new DSP) |

The draft worlds (every value inside its real range; QA section 0):

| World | Overrides |
|---|---|
| Dreamscape | `eq_hi_gain` 2 · `eq_hi_freq` 9000 · `eq_b1_gain` −1.5 · `eq_b1_freq` 350 · `comp_ratio` 2.2 · `comp_thresh` −20 · `comp_attack` 18 · `comp_release` 180 · `comp_makeup` 2.5 · `sat_drive` 1 · echo on, sync, 1/4 dotted, fb 45, tone 3500, width 70, mix 26 · Dimension on, width 80, depth 70, mix 40 · Doubler on, amount 25, width 85 |
| Neon City | `eq_b2_gain` 2.5 @ 3500 · `eq_hi_gain` 3 @ 10000 · `eq_b1_gain` −1.5 · comp 4:1, −22 dB, 5 ms / 90 ms, makeup 6 · `sat_drive` 3 · echo on, sync, 1/16, fb 15, tone 7000, width 40, mix 12 · Dimension on, mix 18 · Doubler on, amount 15, width 60 |
| Radio Static | `eq_lo_gain` −12 @ 350 · `eq_hi_gain` −12 @ 3500 · `eq_b2_gain` 6 @ 1800, Q 1.2 · `sat_drive` 12 · comp 5:1, −24 dB, 4 ms, makeup 4 · Mutation on, amount 20, character 70, blend 30 |
| Underwater | `eq_hi_gain` −10 @ 2500 · `eq_b1_gain` 2 @ 500 · `formant_shift` −1.5 · Dimension on, width 70, detune 14, depth 85, mix 55 · echo on, sync, 1/8 dotted, fb 40, tone 1500, mix 30 |
| Chrome Room | `eq_b2_gain` 4 @ 5000, Q 3 · `eq_hi_gain` 2 · Mutation on, amount 45, character 85, blend 55 · Dimension on, mix 25 · echo on, sync, 1/16 triplet, fb 30, tone 9000, mix 15 |
| Afterhours | `eq_hi_gain` −3 @ 7000 · `eq_lo_gain` 2 @ 110 · `eq_b1_gain` −1.5 · `sat_drive` 4 · comp 3:1, −20 dB, 20 ms / 170 ms, makeup 4 · echo on, sync, 1/4, fb 30, tone 2500, mix 18 · Dimension on, mix 20 · Doubler on, amount 20, width 60 |

## 7. TOOLS

All three tools exist. What's new is that each one proposes, you review every move, and then you keep (Beat Lock already works this way).

| Tool | Real behaviour | What the prototype simulates |
|---|---|---|
| **Beat Lock** | Compares your vocal with the beat on GlitchKid's second input bus, “Instrumental” (PluginProcessor.cpp:209), in five coarse bands at digital level (not LUFS). Proposes `automode::beatMoves` (SourceRelationship.cpp, shared with Build My Mix): presence up so the words stay clear (at most +4 dB in total) · Body and low shelf a little down when the low-mids are busy (never below −6 dB) · existing echo and width reduced when the beat is crowded. A move never pushes past its limit or pulls back a value already beyond it. Balance is **advice only**: GlitchKid never moves faders. The beat is never processed. | The routing state, the band levels and what triggers each move: labelled SIMULATED |
| **Reference Match** | Tonal guidance from a song you have the right to use, never a copy. Compares tone only (200 Hz–12 kHz). Moves only Body `eq_b1_gain`, Presence `eq_b2_gain`, Air shelf `eq_hi_gain`. Requests half the measured difference (a full mix is not a vocal) × intensity. At most 3 dB a move; under 0.5 dB ignored; presence at most +4 dB, the others within ±6 dB; a band already beyond a limit is left alone (`ReferenceMatch.h`). Confidence is never “high” for a full mix. | The file picker and the measured differences (Body −2, Presence +3, Air +4 dB). See OPEN_DECISIONS §15 for a code/header mismatch. |
| **Build My Mix** | Listens, you pick one of the 8 profiles, and `MixEngine` builds Factory Default + the profile. It never changes the 22 user-owned parameters or Bypass. Intensity scales Echo mix, Dimension mix, Doubler amount and Warmth by k = 0.5 + 0.5 × intensity (`MixEngine.cpp`). Here you review every change before anything applies; today it applies immediately. | What it heard, and one fitted move (compressor threshold −2 dB), both labelled SIMULATED MEASUREMENT |

## 8. MASTER

| Control | Real behaviour | Status |
|---|---|---|
| Mastering mode | `master_on` (not automatable). The whole instance masters a full stereo mix instead of shaping a vocal; latency changes. Asks first. | Exists |
| Style | `master_style`: Clean & Balanced, Warm & Smooth, Loud & Punchy, Wide & Atmospheric | Exists |
| Dials | `master_intensity` 0–100 % (60) · `master_loudness` 0–10 dB (3) · `master_tone` −100…100 (0) · `master_width` 0–200 % (100) · `master_output` −12…0 dB (0) | Exists |
| Meters, Load a mix…, Export master… | The plug-in has a mastering meter and offline 24-bit export (`MasteringEngine`, `MasterRender`) | SIMULATED here |

While it is on, the vocal chain shows as not in use, KID is off duty, and every vocal area shows a banner.

## 9. Prototype only (above the plug-in window)

| Group | States it simulates |
|---|---|
| Vocal | None · Live · Clipping |
| Take | Live input · Loaded file |
| FL Studio | Playing · Stopped |
| Instrumental input | Nothing routed · Beat routed |
| Speech | Ready · Not set up · Mic blocked |
| Engine | Normal · Not processing |
| Window | Compact 900×580 · Standard 1120×680 · Large 1360×800 · Fit screen |
| Motion | On · Off |
| KID hears | What the simulated speech recogniser “hears” when you hold the mic |
| Buttons | Restart intro · Close & reopen project · Reset prototype data |
