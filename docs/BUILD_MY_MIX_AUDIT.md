# BUILD MY VOCAL MIX: audit (verified in the source)

**State before this work** (branch `claude/glitchkid-plugin-architecture-3o6w74`, clean at `450de89`).

- **Screens:** TUNE (note pad, scale chips, tuning style, ON), AUTO (artist gallery, reactor ANALYZE button, APPLY SOUND, DRY/PROCESSED, MODIFY), MODIFY (five categories).
- **DSP that really exists:** gain, bypass, 4-band EQ, compressor, saturation (4x oversampled), Pitch Tune, Formant, Harmony (fixed interval), Vocal Mutation, Doubler, Glitch Stutter, Liquid Echo, Dimension, Mix. **No reverb.** No key detection (the key and scale are chosen by the user). 54 parameters; IDs unchanged.
- **Analysis:** `VocalAnalyzer` (lead = the main input): peak, RMS over active frames, crest factor, clipped / near-clipped counts, dynamic range from level percentiles, five coarse band shares, a sibilance estimate, silence/too-short/invalid detection. Runs in the audio callback without allocation or locks and publishes through atomics. It does NOT measure pitch.
- **Instrumental:** an optional second input bus (off by default) with its own `VocalAnalyzer` instance: it measures exactly the same things for the beat (level, peak, crest, dynamic range, five band shares). It measures NO tempo, key, arrangement or stereo information. `automode::compare` turns lead + beat into masking-risk estimates per band.
- **Old AutoMode:** ASTRO MODE rules only (`AutoMix`), applied by the APPLY SOUND button. It is superseded by BUILD MY VOCAL MIX on AUTO (the code and its tests remain).
- **Presets:** 50 artist-inspired profiles as overrides on the Factory Default (`FactoryPresets`, generated table). Verified by test: every id exists, in range, none duplicated, none uses an unimplemented effect (they use Pitch Tune, Formant, EQ, compression, saturation, Echo, Dimension, Doubler, Mutation, Harmony and Stutter). No duplicates: the closest two profiles differ in 10 controls; built mixes differ in at least 10.
- **The trigger point:** the AUTO reactor button (`GlitchKidEditor::autoAnalyzeButton`). Parameters are changed only through `GlitchKidProcessor::applyPreset` / `applyParameterChanges`, i.e. the normal `setValueNotifyingHost` calls (automation-compatible, with begin/end change gestures).

**Blockers:** none for vocal-only. The beat-informed path depends on FL Studio enabling the "Instrumental" bus, which is unverified.

## What BUILD MY VOCAL MIX does
1. You press the big button (optionally after choosing an artist sound; with none it starts from the clean default).
2. It listens to the vocal while the host plays (2-8 s of real audio; silence or invalid audio is reported, nothing changes, the button becomes TRY AGAIN).
3. `automode::buildMix` (pure, tested) takes the profile's saved values as the foundation and makes small bounded adjustments from the measurements (compressor threshold follows the recording level, headroom, hot/clipping input, tone from the vocal's own balance, beat findings only if the beat was really analysed). Effect amounts are scaled by MIX INTENSITY. Gain, Mix, Bypass and YOUR key and scale are never touched.
4. The result is applied to the real parameters as a normal editable sound named "Built: <profile>". The old sound is kept: BACK TO MY SOUND undoes it (even after several builds in a row).
5. The window says VOCAL ONLY or BEAT INFORMED and WHAT CHANGED lists each changed setting with its reason, the evidence used and what was not available.

## Verified / not verified
- **Automated (Linux):** engine logic, ranges, fuzz (600 random analyses incl. NaN/Inf), distinct profiles, no drift on repeated builds, undo, state round trip, end-to-end audio for all 50 profiles + clean default (finite, within 3.3 dB of the input level, peak <= 0.98 on a -8 dB peak voice), the real button driven through the editor (listen, build, undo, silent input fails).
- **Not verified:** anything in FL Studio, on macOS or on Windows; real vocals (only synthetic signals); the beat-informed path with a real routed beat; ASan / pluginval.

## Manual FL Studio test (Mac)
1. Pull the branch, rebuild, copy `GlitchKid.vst3` to `~/Library/Audio/Plug-Ins/VST3/`, rescan plugins.
2. Put GlitchKid on a vocal track with a dry vocal clip. Open AUTO and pick an artist card.
3. Press BUILD MY VOCAL MIX, press play, let the vocal run 8 seconds. Expect: the ring fills, then "VOCAL ONLY mix built…", the preset bar shows "Built: …", the sound changes (toggle DRY / PROCESSED).
4. Press WHAT CHANGED, then BACK TO MY SOUND: your previous sound returns.
5. Beat: send the beat's mixer track to GlitchKid's sidechain input (look for it in the plug-in wrapper's settings; unverified). The AUTO line should change from NOT CONNECTED to LIVE; build again and expect BEAT INFORMED with "BEAT:" findings. If it stays NOT CONNECTED, the host did not enable the bus: tell me what FL shows.
