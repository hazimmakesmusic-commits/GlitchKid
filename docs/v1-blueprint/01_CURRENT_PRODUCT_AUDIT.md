# Deliverable 1 — Current product audit

What exists in GlitchKid today: what works, what is incomplete, and what is broken. The source code is the authority.

**Evidence tags**

| Tag | Meaning |
|---|---|
| **[V]** | Verified by reading the source (file:line given) |
| **[D]** | Stated in the repository's docs, not re-verified |
| **[A]** | Assumption or inference |
| **[T]** | Needs testing |
| **[R]** | Needs research |

## 1. Snapshot

| Item | Finding |
|---|---|
| Repository | `hazimmakesmusic-commits/GlitchKid`, branch `claude/glitchkid-plugin-architecture-3o6w74`, HEAD `4a16b44`, same as `origin`. Working tree clean. [V] |
| Size | Source about 20,000 lines (DSP 4,236 · UI 5,073 + editor 2,632 · Kid 1,704 · Analysis 1,479 · AutoMode 1,118 · Mastering 919 · Presets 627 · Macros 204 · processor 1,704). Tests 16,357 lines in 39 files. 36 docs. [V] |
| Release status | *“Version 0.1.0 … nothing has been released to the public”* (`CHANGELOG.md:3`). [V] Compatibility obligations therefore cover only the owner's own projects and presets. |
| Design prototypes | `design-prototypes/` holds 494 files of HTML concepts. They are not part of the plug-in. Two of them (`glitchkid-vocal-playground-v2`, `glitchkid-session-that-talks-back-v1`) explore directions this brief now supersedes: KID-first, creative ideas. |

## 2. Build, formats and platforms

| Item | Finding |
|---|---|
| Framework | JUCE 9.0.4, pinned by commit and fetched at configure time (`CMakeLists.txt:18`). [V] |
| Plug-in formats | **VST3 only** (`CMakeLists.txt:44`). No AU, so **Logic Pro cannot load it**. No AAX, no Standalone. [V] |
| macOS | **Apple Silicon only** (`CMAKE_OSX_ARCHITECTURES "arm64"`, line 5); macOS 12.0+ (line 6). No Intel/universal binary. [V] |
| Windows | Not configured or verified. `docs/BUILDING.md` mentions Windows paths and an AVX2 requirement. [D] [T] |
| Linux | Builds for tests and pluginval only; “not a deliverable” (`docs/BUILDING.md:47`). [D] |
| Optional dependency | whisper.cpp 1.9.5 for KID speech, **on by default** (`CMakeLists.txt:24`). The model is downloaded at the user's request. [V] |
| Assets | Inter font (OFL) and the owner-supplied “Glitch Relic” artwork, compiled in. [V] |
| Determinism | `-ffp-contract=off` so the same source gives the same bits on every platform. [V] |
| Signing, notarisation, installer | Not done (`docs/BUILDING.md:55`, `docs/ROADMAP.md`). [D] |
| **Licensing** | *“JUCE eligibility is NOT confirmed”* (`LICENSES.md:10`); *“Until then: no commercial release”* (`docs/ROADMAP.md`). The whisper model licence is “verify before any commercial release”. The repository is public. [D] **Commercial blocker.** |

## 3. Architecture

- **Processor** (`Source/PluginProcessor.*`). One `AudioProcessorValueTreeState` with **76 parameters**: 68 sound parameters, Bypass, and 7 `master_*` (`createLayout`, `PluginProcessor.cpp:12–203`). [V]
- **Buses** (`PluginProcessor.cpp:204–210`, `:725`). Stereo or mono main in/out, plus an optional **“Instrumental”** input (disabled by default, mono or stereo) that is only measured, never mixed. [V]
- **Message-thread engines.** Build My Mix, Beat Lock, Reference Match, AutoMix, KID, macros and Alter Egos are pure functions over a `SoundState`. They write parameters through normal host gestures (`applySoundState`, `applyParameterChanges`). [V]
- **Editor.** Design size 1120 × 680; resizable 70–150 % with a fixed aspect ratio (`PluginEditor.h:96`, `PluginEditor.cpp:675–676`). An 11-item rail (`PluginEditor.cpp:118–136`). [V]

## 4. Signal chain (`processChunk`, `PluginProcessor.cpp:948–1272`) [V]

| # | Stage | Latency | Notes |
|---|---|---|---|
| 1 | Pitch Tune | ~31 ms while on | YIN + PSOLA |
| 2 | Formant | ~21 ms while ≠ 0 | STFT envelope warp |
| 3 | Harmony | ~31 ms while on | Fixed interval; *“prototype quality”* (`PluginProcessor.h:129`) |
| 4 | Mutation | 0 | Frequency shifter / ring mod |
| 5 | Doubler | 0 | Synthetic ADT-style double |
| 6 | 4-band EQ | 0 | RBJ biquads, double precision, 20 ms smoothing |
| 7 | CLEAN corrective | 0 | Rumble, low-mid, resonance (FFT side-chain detector, IIR audio path) |
| 8 | Compressor | 0 | Feed-forward peak detector, soft knee, stereo-linked |
| 9 | CLEAN de-esser | 0 | |
| 10 | Saturation | 6 samples, always | tanh, 4× oversampled |
| 11 | AIR LAB | 0 | Parallel presence/air lift, sibilance-aware |
| 12 | Glitch Stutter | 0 | Beat repeat |
| 13 | Liquid Echo | 0 | Tempo-synced delay |
| 14 | Dimension | 0 | Dual chorus widener |
| 15 | Gain | 0 | |
| 16 | Mix | 0 | Dry/wet against the latency-aligned dry path |
| 17 | Output Limiter | ~1.5 ms while on | **Sample-peak** look-ahead (not true-peak) |

- **Total reported latency** is 6 samples plus Tune, Formant, Harmony and Limiter while each is on (`PluginProcessor.cpp:333–340`). A 10 Hz timer updates it, **so the latency changes at runtime** when any of those is switched. [V] How each host handles that is [T].
- **No reverb exists.** SPACE is delay (Echo), chorus width (Dimension) and the Doubler. The source says so itself (`MacroEngine.cpp`, macro description). [V]
- **Bypass** crossfades to the latency-aligned dry signal (`PluginProcessor.cpp:1072–1093`). **The Output Limiter still runs on the bypassed signal when it is on** (`:1091`). [V]
- **Level-matched compare.** It plays the dry signal at the processed loudness, using 3 s running averages (`:1050–1067`, `:1253–1269`). It is only switched on while the START/KID screen is visible (`KidPanel.cpp:215–220`); on every other page, bypass is not loudness-matched. [V]
- **Host bypass.** The source has no `getBypassParameter()` or `processBlockBypassed()` override, so the host's own bypass button is not linked to GlitchKid's latency-aligned Bypass. [V] What each host then plays (timing and level) is [T].
- **Mastering mode** (`master_on`) replaces the whole vocal chain with a separate mastering chain. [V]

## 5. Parameters (`createLayout`) [V]

All IDs are at version 1. Layout order is the host automation order: gain, bypass, EQ (10), compressor (6), saturation, stutter (3), echo (8), tune (5), formant, mutation (4), harmony (3), doubler (5), Dimension (5), mix, AIR LAB (6), CLEAN (5), limiter (4), master (7). Ranges and defaults match the table in `design-prototypes/glitchkid-vocal-playground-v2/INTENT_PARAMETER_MAP.md` (copied from this source on 2026-11-04, and no parameter changed since). Defaults are neutral: a fresh instance does not change the sound.

## 6. Macros and quick controls (`Source/Macros/MacroEngine.*`) [V]

| Macro | Writes | Read back from |
|---|---|---|
| SPACE | `echo_on`, `dim_on`, `dbl_on` (on above 2 %), `echo_mix` 0–60, `echo_fb` 15–45, `echo_width` 20–75, `dim_mix` 0–45, `dbl_amount` 0–40 | `echo_mix` / 60 |
| PUNCH | `comp_ratio` 1–6, `comp_thresh` −12…−26, **attack fixed 15 ms, release fixed 110 ms, knee fixed 6 dB**, makeup formula | `comp_ratio` |
| TONE | `eq_hi_gain` ±6, `eq_b2_gain` ±1.5 | `eq_hi_gain` |

Also: the quick commands DARKER, MORE SPACE and HARDER TUNE; the tune styles NATURAL / LIGHT / HARD (strength 50/75/100 %, speed 80/35/0 ms).

**Problems for V1:**
1. **There is no PRESENCE macro.**
2. **Every macro writes absolute values.** Touching PUNCH replaces a generated mix's attack, release, knee and threshold with fixed values. Touching TONE replaces the generated high shelf. Touching SPACE rewrites Dimension and Doubler amounts from `echo_mix` alone, so a generated mix with Echo 14 % and Dimension 22 % jumps on the first touch.
3. **SPACE switches the Doubler on**, which is texture rather than space.
4. **Macros are not host parameters**, so they cannot be automated. Their gestures write up to 8 underlying parameters.
5. **No loudness compensation.** PUNCH adds up to 4 dB of makeup gain, so a macro move can “win” by being louder.

## 7. Presets and session state

| Item | Finding |
|---|---|
| User presets | One XML file per preset, `formatVersion="1"`, `.gkpreset`, in `~/Library/Application Support/GlitchKid/Presets/User` (Windows `%APPDATA%`). Writes are atomic (temp file then swap); names are sanitised; never overwrites silently; corrupt files are reported (`PresetStore.h`). [V] |
| What a preset holds | Every sound parameter plus the Alter Ego selection. Not Bypass, not `master_*` (`SoundState.h`). Missing parameters get defaults; unknown ones are ignored. [V] |
| Factory profiles | 8, as override tables on top of Factory Default (`FactoryPresets.h`, `Profiles.inc`): CLEAN & NATURAL, MODERN MELODIC RAP, HARD-TUNED RAP, DARK & ATMOSPHERIC, BRIGHT & PRESENT, SMOOTH R&B, GRITTY RAP, SPACIOUS LEAD. They use only EQ, compressor, saturation, Echo, Dimension and Doubler. 7 of the 8 switch the Doubler on. [V] |
| Profile load | Keeps 22 user-owned parameters: `tune_*` (5), `gain`, `mix`, `air_*` (6), `clean_*` (5), `lim_*` (4) (`PresetController.cpp:11–21`). Each profile is validated before loading. [V] |
| Session state | `getStateInformation` writes the APVTS tree as XML (`PluginProcessor.cpp:1278`). It holds every parameter plus properties: preset name, A/B slots and active slot, Alter Ego selection, UI flags, auto intensity. [V] |
| **State versioning** | **None.** No version attribute in the session state; `setStateInformation` simply replaces the state (`:1284–1295`). [V] ROADMAP lists *“Version attribute in saved state, plus a migration test”* as not started. [D] |
| A/B | Two snapshots in the session; recall applies one; `isAbSlotModified`. [V] |
| Undo | One step only, “BACK TO MY SOUND”, after a build, profile load or Beat Lock (`PluginEditor.h:174`). KID keeps a separate 50-step history (`KidPanel.cpp:424`). No redo. Knob moves are not undoable inside the plug-in. [V] |

## 8. Engines

| Engine | Status | Notes |
|---|---|---|
| **Build My Mix** (`MixEngine.*`) | Works; called by the SOUNDS page (`PluginEditor.cpp:1108`) and KID [V] | Foundation = profile + bounded adjustments from the vocal analysis [V]: <ul><li>compressor threshold from RMS</li><li>ratio from dynamic range</li><li>makeup from peaks</li><li>saturation halved if the input clips or the voice is bright</li><li>low shelf / Body / presence / high shelf from band shares and sibilance</li></ul> It skips corrections CLEAN already does, and adds beat moves when the instrumental was analysed. Deterministic: repeated builds give the same result. Tune, Gain and Mix are never touched; Formant, Harmony, Mutation and Stutter are switched off. **Applies immediately** (single-step undo). |
| **Beat Lock** (`BeatLock.*`, `SourceRelationship.*`) | Works, **manual**: ANALYZE BEAT, then APPLY SELECTED (`PluginEditor.cpp:1554–1618`) [V] | <ul><li>Measures from 5 coarse bands, digital levels, not LUFS.</li><li>Moves: presence ≤ +4 dB total, Body/low shelf ≥ −6 dB, existing echo/width × clutter factor.</li><li>A value already beyond a limit is never pulled back.</li><li>BALANCE is advice only.</li><li>Shares its rules with Build My Mix.</li><li>Not automatic; does not detect a beat change.</li><li>FL Studio sidechain routing unverified (`docs/SOURCES.md:20`) [D].</li></ul> |
| **Reference Match** (`ReferenceMatch.*`, `ReferenceAnalyzer.*`, `ReferencePanel`) | Works, with **one bug** [V] | The code pulls a band that starts beyond ±6 dB to the edge in one move (e.g. −12 → −6 dB), contradicting `ReferenceMatch.h:16` (“left alone … at most 3 dB”). Not covered by a test. |
| **AutoMix / “ASTRO MODE”** (`AutoMix.*`, `applyAutoMode`) | **Orphaned**: no UI calls it [V] | Superseded by Build My Mix; still exposed by the processor and tested. |
| **Alter Egos + Mood Engine** (`AlterEgos.*`) | **Orphaned in the UI**: the editor never calls `applyAlterEgo` [V] | Still part of `SoundState`, presets and tests (16 groups). |
| **KID** (`Kid/*`, `KidPanel`) | Works | Rule-based director, whisper.cpp speech, `VocalDiagnosis` findings. It lives on START (`PluginEditor.cpp:120`). |
| **Mastering Lab** (`Mastering/*`, `MasteringScreen`) | Works | Separate chain: BS.1770 meter, true-peak limiter, width, offline WAV export. |

## 9. Analysis

| Analyser | Measures | Limits |
|---|---|---|
| `VocalAnalyzer` (live; lead and beat) | Peak, RMS, crest, clipping, dynamics percentiles, 5 band shares (<120, 120–500, 500–2k, 2–5k, >5k Hz), a rough sibilance index | Listens 2–8 active seconds (gives up at 30 s), only while the host plays [V] |
| `VocalFileAnalysis` (offline, a loaded vocal file) | Peaks, clipping, noise floor (estimate), stereo correlation, 19-band tonal shape, pitch range. **Key estimate only when reliable** (major/minor pair). | Used only by KID and its panel [V]. **No live key detection; the TUNE page never suggests a key.** [V] |
| `ReferenceAnalyzer` | 19 third-octave bands, 200 Hz–12.8 kHz | Reusable for finer Beat Lock analysis [A] |
| Display taps / spectrum | For UI views | — |

## 10. Pitch correction [V]

- **Design.** YIN detector into a scale quantiser, then one PSOLA shifter per channel. Range 80–800 Hz. Strength 0–100 %, speed 0–400 ms. 12 keys × 6 scales (Chromatic, Major, Natural minor, Harmonic minor, Major/Minor pentatonic). About 31 ms latency while on.
- **Status.** Tested on synthetic voices and one real recording [D].
- **Limits.**
  - Key and scale are entirely manual on the TUNE page.
  - The roadmap notes “if it sounds grainy, add pitch-mark alignment” [D], so artifact quality on varied real vocals is unproven [T].

## 11. DSP module status

| Module | Production-ready? | Evidence |
|---|---|---|
| EQ, Compressor, Saturation, Gain/Mix, Output Limiter, CLEAN LAB, AIR LAB, Liquid Echo, Dimension | Mature implementation with unit tests (12–17 groups each) [V] | Sound-quality listening across varied vocals not documented [D] [T] |
| Pitch Tune, Formant | “Productised prototype” (headers) [V] | One real-voice listening test each [D] |
| Doubler | Implemented, tested | Own header: “on long held notes or very exposed vocals it sounds like what it is” [V] |
| Harmony | **Prototype quality** (`PluginProcessor.h:129`) [V] | Fixed interval, not key-aware |
| Mutation, Stutter | Implemented, tested | Creative; not vocal-mixing tools |
| **Reverb** | **Does not exist** | Required by the brief's SPACE stage |

## 12. User interface (today) [V]

- **Rail:** START, SOUNDS | CLEAN, TUNE, TONE, DYNAMICS, SPACE | CREATIVE | BEAT, REFERENCE | MASTER, under five groups (START, SHAPE THE VOCAL, CREATIVE, FIT TO TRACK, FINISH).
- **START:** seven dials (TUNE, CLEAN; TONE, PUNCH, SPACE; MIX, OUTPUT), a live in/out view and KID.
- **Visual layer:** a psychedelic paint pass and a MANDALA view (commit `6c3c0b1`).
- **Known UI issues** (from the last sprint report):
  - TUNE and TONE share one Advanced switch.
  - EQ curve points can be dragged while their knobs are hidden.
  - REFERENCE has empty gaps with Details off.
  - The Details switch isn't remembered.
  - CREATIVE Reset can't be undone in the plug-in.

## 13. Tests

- **Inventory.** 463 `beginTest` groups in 39 files [V], covering DSP maths, safety (NaN/Inf, bit-exact neutral defaults), presets, A/B, engines, UI behaviour and paint budgets.
- **Last run.** On 2026-11-05 in this project's cloud session, 467 groups passed with 0 failures, on a Linux x86_64 build of exactly the current source. No source has changed since (`git diff 6c3c0b1 HEAD` touches only `design-prototypes/`). **Not re-run for this audit:** the build folder no longer exists and the brief forbids building. [V]
- **Stale or missing.**
  - `docs/TESTING.md` still says “245 test groups” and reports a pluginval strictness-5 pass of unknown date. [D]
  - **No CI.** No macOS or Windows automated build.
  - No host tests.
  - No golden-render audio regression.
  - No listening-test protocol.

## 14. Host compatibility

| Host | Status |
|---|---|
| FL Studio 2025, Apple Silicon | Loads as VST3; the owner reported each stage working by ear. Not independently measured. [D] |
| FL Studio, untested areas | CPU, other sample rates and buffer sizes, mono tracks, sidechain routing (`docs/TESTING.md`, `docs/SOURCES.md`) [D] |
| Ableton Live, Reaper | **Never tested** [D] |
| Logic Pro | **Impossible**: no AU build [V] |
| Windows | **Never built** [D] |

**Host-behaviour risks:**
- runtime latency changes;
- the host bypass not linked to the plug-in's Bypass;
- an unrouted sidechain is indistinguishable from a silent one (`PluginProcessor.h` SourceStatus comment);
- the K, D and Ctrl/Cmd+Z shortcuts may be captured by hosts.

## 15. Known bugs and limitations

1. Reference Match can exceed its 3 dB per-move bound (§8). [V]
2. Bypass is not loudness-matched outside START. [V]
3. The Output Limiter processes the bypassed signal. [V]
4. Macros overwrite generated settings with fixed values (§6). [V]
5. No session-state version, so no migration path. [V]
6. Runtime latency changes when Tune, Formant, Harmony or the limiter switch. [V] Host behaviour [T].
7. The VST3 host bypass is unmapped. [V]
8. Key detection is offline only and not offered on TUNE. [V]
9. The live `VocalAnalyzer` sums to mono, so the beat's stereo image is not measured. [V]
10. Beat Lock bands are coarse (5 bands, not LUFS). [V]
11. The UI issues listed in §12.
12. Single-level undo outside KID. [V]

## 16. Documentation versus implementation

| Document says | Code shows |
|---|---|
| `README.md`: “Status: Stage 3.1 … Signal path: EQ → compressor → saturation → Glitch Stutter → gain → Mix”; “Latency: 6 samples, constant” | 17 stages; latency varies with Tune, Formant, Harmony and Limiter |
| `docs/TESTING.md`: “245 test groups” | 463 defined; 467 run on 2026-11-05 |
| `docs/ROADMAP.md`: presets and user presets under “Later”; pitch correction “in the plug-in” | Presets, user presets and A/B exist; the roadmap is partly stale |
| `docs/VOCAL_MIXING_MILESTONE_AUDIT.md`: “No key detection” | An offline key estimate exists (`VocalFileAnalysis`), used only by KID |
| `ReferenceMatch.h:16`: at most 3 dB a move; a band beyond a limit is left alone | `ReferenceMatch.cpp:117–126` can move 6 dB |
| `MixEngine.h` comment: example profile “TRAVIS SCOTT STYLE”, “artist-inspired profile” | Factory profiles are genre-style names; KID keeps artist-word mapping (`KidDirector.cpp`) |
| `AlterEgos.h`, `docs/ARCHITECTURE.md`: Alter Egos and Mood Engine are a product feature | Not reachable from the current UI |

## 17. Summary

| Works and fits V1 | Incomplete | Broken or misfit |
|---|---|---|
| EQ, compressor, saturation, CLEAN, AIR, Echo, Dimension, Gain/Mix, limiter; Pitch Tune (manual); 8 profiles + Build My Mix; Beat Lock (manual); A/B; user presets; session save; level-matched compare engine; analysers; strong unit-test discipline | No reverb; no PRESENCE macro; Beat Lock not automatic; no live key suggestion; no state versioning; no AU, Windows, Intel; no CI or host tests; no listening protocol; licensing unconfirmed | Macros overwrite generated mixes; Reference Match bound bug; host bypass unmapped; compare not loudness-matched outside START; limiter on the dry path; orphaned AutoMix and Alter Egos; stale README and TESTING docs |
