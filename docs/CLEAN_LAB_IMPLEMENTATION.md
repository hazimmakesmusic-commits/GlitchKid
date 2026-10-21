# CLEAN LAB: implementation report

CLEAN LAB is implemented as a bounded first version. It is **not** a claim that a vocal comes out professionally mixed: everything below was verified on synthetic signals; whether it sounds good on real vocals has **not** been tested by a human (see section 8).
Audit that preceded the work: `docs/CLEAN_LAB_AUDIT.md`.

## 1. What it does
One module (`Source/DSP/CleanLab.{h,cpp}`), two call points, five new parameters, zero latency, off by default.

| Part | What it does | Limits |
|---|---|---|
| **Rumble** | A 70 Hz 2nd-order high-pass blended in only as far as the measured energy in the lowest FFT bin (about 23 Hz) calls for, relative to the 150-500 Hz band. | Only the lowest bin is used, because the analysis window's leakage from a low voice's fundamental reaches the next bins. A rumble at 50-60 Hz is therefore not detected (conservative). |
| **Low-mid build-up** | One wide bell (Q 0.9) at the frequency (150-400 Hz) where the vocal's long-term spectrum most exceeds a straight line fitted to its own 700 Hz - 5 kHz region and extended downward. Starts at 6.5 dB of excess; cut = 0.7 x (excess - 6.5), at most 3.5 dB. | A heuristic, validated only on synthetic signals. Real voices with a strong natural F1 or chest resonance may or may not trigger it. |
| **Resonances** | Up to 3 narrow bells (Q 6), each at most 6 dB, found in 1-8 kHz as bins that stay above their +-1/3 octave surroundings (after a +-1/12 octave power smoothing that removes the comb of harmonics) by at least 4 dB and stand out by 3 dB from the +-1/4 octave neighbours. Evidence is accumulated only while the voice **moves** (the shape of the excess changes), so a held note is not mistaken for a resonance. Slots keep their frequency, gains and frequencies are smoothed (0.25 s / 0.5 s). | Slow by design: needs about 1.6 s of voiced audio plus movement, and a few seconds before a cut reaches full depth. Below 1 kHz harmonics are too far apart to tell from resonances, so that range is not searched (the low-mid bell covers 150-400 Hz). |
| **De-esser** | Sibilance = energy above 4.5 kHz relative to the body (about 1 kHz) on the mono sum. Above a threshold a wide bell (7.5 kHz, Q 0.9, about 4.5-12.5 kHz) is lowered by up to 9 dB x depth (ratio 4, 3 dB knee, 0.5 ms attack, 40 ms release). Both channels get the same gain. With no reduction the audio passes bit for bit. | No look-ahead (adding latency was ruled out), so the first fraction of a millisecond of a very fast consonant can pass. It lowers a broad band while it works, which also softens some "SH" energy. |
| **CLEAN amount** | `amount` sets the depth of every part; the three section trims (`tone`, `res`, `ess`) multiply it for that section only. Depth maps to limits, not to a blind gain multiplier: rumble blend, bell depth, resonance depth (each capped), and for the de-esser both a more sensitive threshold (1.9 - 0.9 x depth) and a larger maximum (9 dB x depth). Amount 0 = bit-exact. | |

Switching on and off is a 20 ms crossfade; off is an exact pass-through at no CPU cost. While off, the filters are cleared but **what was learned is kept**, so comparing on/off does not relearn. While on, CLEAN LAB learns; it says how much it has heard.

## 2. Signal-chain position (reasoning in the audit)
`... user EQ -> CLEAN LAB (rumble, low-mid, resonances) -> compressor -> CLEAN LAB (de-esser) -> saturation -> AIR LAB -> stutter -> echo -> Dimension -> gain -> mix`
- Corrective stage after the user's EQ, before the compressor: a manual cut is seen by the detectors and not repeated; resonances and rumble do not drive the compressor.
- De-esser after the compressor (compression raises relative sibilance), before saturation and AIR LAB.
- No existing stage was moved or changed. Latency is unchanged (tested).

## 3. Parameters (appended after `air_trim`, host indexes 60-64; IDs are permanent from now on)
`clean_on` (bool, off), `clean_amount` (0-100 %, 50), `clean_tone` (0-100 %, 100), `clean_res` (0-100 %, 100), `clean_ess` (0-100 %, 100).
Compatibility: older sessions and presets lack them and load with CLEAN LAB off (tested, including a state with the parameters removed). Existing parameters keep their index. BUILD MY VOCAL MIX and LOAD PROFILE never change them, and while CLEAN LAB is on (amount above 5 %) BUILD **skips its own low-shelf, low-mid and sibilance EQ corrections** and says so in its evidence (tested), so the same fault is not corrected twice. `Reset CLEAN` restores the five defaults (an explicit button on the CLEAN page); there is no other new undo path.

## 4. UI (MODIFY > CLEAN, plus the TONE graph)
- A fifth MODIFY category, CLEAN: switch, CLEAN amount, RESET CLEAN, three section trims, and live read-outs that come only from the DSP's telemetry (rumble blend, low-mid cut and frequency, up to three resonance cuts with frequency, de-esser gain reduction as a bar and in dB, net level change, learning status). Nothing is drawn or written that the DSP did not report: while learning it says so and shows no correction.
- The EQ graph on TONE draws CLEAN LAB's real applied corrections as a second, pink curve (labelled CLEAN LAB), computed with the same formulas as the DSP; a test compares it with the measured response.
- The chain strip's TONE / DYNAMICS dots also light while CLEAN LAB is applying something / enabled.
- Loudness bias in comparisons: CLEAN LAB's own on/off switch is the click-free A/B, and the net level change it causes is displayed so the artist can compensate with TRIM/Gain. Automatic loudness compensation was **not** added (it would need a new parameter or a measurement of the user's audio).

## 5. Files
New: `Source/DSP/CleanLab.{h,cpp}`, `Tests/CleanLabTests.cpp`, `docs/CLEAN_LAB_AUDIT.md`, this file.
Changed: `CMakeLists.txt`; `Source/PluginProcessor.{h,cpp}` (parameters, two calls, park on bypass, telemetry getter); `Source/AutoMode/MixEngine.cpp` (keep-list, skip rules, evidence); `Source/Presets/PresetController.cpp` (carry-over and reserved ids); `Source/PluginEditor.{h,cpp}` (CLEAN category, read-outs, chain dots, overlay feed); `Source/UI/PlaygroundWidgets.{h,cpp}` (overlay curve, `ReductionBar`); tests `MacroTests`, `FactoryPresetTests`, `AlterEgoTests`, `MutationTests`, `PitchTuneTests`, `AirLabTests` (parameter counts and lists); `CHANGELOG.md`, `docs/UI.md`.

## 6. Tests (`Tests/CleanLabTests.cpp`, all synthetic)
Neutral and bit-exact behaviour; de-esser (S bursts lowered, vowels untouched, S still present, consonant envelope correlation, amount law, stereo identical, silence / -90 dBFS / NaN / full-scale); resonances (known resonance found within 300 Hz and lowered, clean voices and a held note left alone, at most three bells and 6 dB each, no wobble over 4 s, 5 s of silence and a 12 dB level change do not move it, trims at 0); corrective EQ (rumble found and lowered, muddy build-up found and lowered, normal voices nearly untouched, tone trim 0); no double correction with the user's EQ; graph overlay against the real DSP on a noise probe (within 0.7 dB); monotonic amount law; no click when switching or moving the amount; CLEAN LAB with EQ + compressor; five sample rates x four block sizes; processor integration (65 parameters, order and defaults, state round trip, older state, latency unchanged, no allocation over 400 blocks, no clipping, BUILD interplay); the editor (the CLEAN page, attachments, truthful read-outs, RESET CLEAN).

## 7. Build, test and measurements (Linux only; synthetic signals)
- Build: fresh temporary directory outside `build/`: `cmake -S . -B <dir> -DCMAKE_BUILD_TYPE=Release -DGLITCHKID_BUILD_TESTS=ON && cmake --build <dir> -j8`: exit 0, no compiler warnings, `GlitchKid.vst3` and `GlitchKidTests` built.
- Tests: `<dir>/GlitchKidTests_artefacts/Release/GlitchKidTests`: 336 test groups, **0 failures** (the previous milestone had 321; the new groups are CLEAN LAB's).
- Sanitizers: the same suite built with `-fsanitize=address,undefined` (RelWithDebInfo) and run with `ASAN_OPTIONS=detect_leaks=0:alloc_dealloc_mismatch=0` (the mismatch option is needed because an older test replaces the global `operator new`): 0 failures, 0 sanitizer reports. That run predates a final one-line UI text change.
- pluginval: not installed, not downloaded: **not run**. FL Studio / macOS / Windows: **not run**.
- Git: only the files listed above changed; the untracked `docs/GLITCHKID_EXPERIENCE_DIFFERENTIATION_AUDIT.md` from the previous task is untouched; nothing committed.
- Latency: 0 samples added (test compares `getLatencySamples()`).
- CPU, stereo, 480-sample blocks, everything on: about 1.3-1.8 % of one core over the runs made (CLEAN LAB alone, offline, indicative; the FFT runs once per 512 samples). Not measured inside a host.
- Behaviour on the synthetic test vocal: a +14 dB / Q 12 resonance at 2.9 kHz is cut by about 3.8 dB (found at 3.01 kHz, resonance band lowered by about 2.8 dB, neighbours -0.3 dB); a +12 dB wide build-up at 260 Hz gets a 1.8-3.5 dB cut near 210-320 Hz; 38 Hz / 25 Hz hum is detected and lowered by more than 6 dB; S bursts are lowered by about 8 dB in 6-10 kHz while the body stays within 0.45 dB.

## 8. Known limitations and what is NOT validated
- **No real vocal has been processed and listened to.** Thresholds (4 dB resonance floor, 3 dB prominence, 6.5 dB low-mid start, 1.9 sibilance ratio) were set from synthetic signals only and may be too timid or too eager on real recordings.
- The detectors learn slowly by design; a vocal that never moves (a monotone drone) teaches nothing and receives no resonance or low-mid correction.
- The resonance range starts at 1 kHz; narrow boxiness at 300-1000 Hz is not separated from harmonics.
- No look-ahead in the de-esser (zero latency chosen).
- pluginval was not available; no FL Studio, macOS or Windows run was possible here.
- CPU was measured offline for the module only.
- No loudness compensation: CLEAN LAB can make the vocal quieter (the display shows by how much).

## 9. Listening-test plan (a person must run it; nothing below has been done)
Dry vocals, each looped 20 s through GlitchKid with CLEAN LAB off and on, loudness matched by ear using TRIM/Gain (the net level change is on the CLEAN page), several times, ideally blind (a friend toggles):
1. a muddy close-mic recording; 2. a boxy or resonant take; 3. a bright, sibilant take; 4. an already clean take (it should receive almost nothing: check the read-outs say "none found"); 5. melodic rap and 6. fast, consonant-heavy rap.
For each, listen for: less unwanted resonance, better clarity, warmth and body kept, consonants still intelligible, and no lisping, dullness, ringing, pumping or watery artifacts, and no correction of the already-good take. Try CLEAN at 25 %, 50 %, 100 % and each section trim alone. Write down the three worst moments and send them back with the settings and the read-outs.

## 10. Manual FL Studio steps (macOS)
1. `cd ~/GlitchKid`, `git pull origin claude/glitchkid-plugin-architecture-3o6w74`, `cmake --build build -j8`.
2. `cp -R build/GlitchKid_artefacts/Release/VST3/GlitchKid.vst3 ~/Library/Audio/Plug-Ins/VST3/`, rescan in FL Studio, load it on the vocal insert.
3. MODIFY > CLEAN: switch CLEAN LAB on, play the vocal for 10-20 s (the voice must move: sing or rap normally), and watch "Learning your voice" turn into read-outs.
4. Open MODIFY > TONE: the pink curve shows the corrections. Compare with the switch off and on; match loudness with TRIM or Gain.
5. Automate CLEAN (the amount) in FL, save, reopen: the setting returns. Open an old project: CLEAN LAB must be off and the sound identical.
Do not report FL Studio compatibility as verified until step 5 has been done on a real machine.

## 11. Remaining work before a commercial release
Listening validation on a library of real vocals (and threshold tuning from it), a pluginval run on macOS and Windows, a host run in FL Studio, a whole-plug-in CPU profile, a decision on automatic loudness compensation and on look-ahead, and (if wanted) a low-mid / boxiness detector that works below 1 kHz with a pitch-aware analysis.
