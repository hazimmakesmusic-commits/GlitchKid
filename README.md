# GlitchKid

*A playground for your sound.* — a local-first VST3 vocal-processing plugin (macOS, FL Studio first).

**Status: Stage 3.1 (first signature effect: Glitch Stutter).** Signal path: input -> 4-band parametric EQ -> compressor -> saturation -> **Glitch Stutter** -> output gain -> Mix (blend with the latency-aligned dry input) -> output, with peak meters on input and output and a gain-reduction meter.

- **EQ:** low shelf (20-500 Hz), bell 1 (100-5000 Hz, Q 0.3-10), bell 2 (500-16000 Hz, Q 0.3-10), high shelf (1-18 kHz); each band +/-15 dB in 0.1 dB steps. RBJ cookbook biquads in double precision; a band within 0.05 dB of 0 dB is skipped, so a flat EQ is bit-exact. Shelf slopes are fixed (Q 0.7071). Settings are smoothed over ~20 ms.
- **Compressor:** feed-forward, stereo-linked peak detector, soft knee, separate attack/release smoothing of the gain reduction (after Giannoulis, Massberg & Reiss, JAES 2012). Threshold -60..0 dB (default -18), ratio 1..20:1 (default 1.0 = off, so a fresh instance does not change the sound), knee 0..24 dB, attack 0.1..100 ms and release 10..1000 ms (both are one-pole time constants: about 63% of a level step for attack, falling to about 37% for release), makeup 0..24 dB. No lookahead, so no latency, but a very fast transient can overshoot for about one attack time. Idle (ratio 1, no makeup, release finished) it is skipped and bit-exact. The GR meter shows gain reduction, 0..24 dB.
- **Saturation:** a tanh soft-saturation waveshaper with one control, **Drive** 0..18 dB (default 0 = off). It adds odd harmonics only. The shaper runs 4x oversampled (JUCE's polyphase IIR half-band oversampler) so the harmonics it creates above the audible range are filtered out instead of folding back as inharmonic aliasing: measured with a 7 kHz tone at 18 dB drive, folded-back energy is about -107 dB, versus -13.5 dB for a plain tanh. Quiet material gets louder as you add drive (about +17 dB of small-signal gain at 18 dB), while a full-scale input stays near full scale (the oversampling filters can overshoot it by up to about 2 dB on hard-driven, square-ish waves), so trim with Output Gain and watch the OUT meter.
- **Latency: 6 samples (0.125 ms at 48 kHz), constant**, reported to the host, so the host compensates automatically. It comes from the saturation oversampler. The bypass path and an idle saturation stage are delayed by exactly the same amount, so Bypass stays time-aligned and the default signal is a bit-exact copy of the input, 6 samples later.
- **Gain:** -60 to +12 dB in 0.1 dB steps, ramped over 20 ms (applied after the EQ).
- **Glitch Stutter:** while **ON**, the stage repeats the slice of audio that played just before it was switched on, over and over (a beat repeat). Controls: **ON** (automatable, default off), **RATE** (1/4, 1/8, 1/16, 1/32 note; default 1/8), **STUTTER MIX** 0..100 % (default 100; 0 % is the exact dry signal). It uses only the real incoming audio (a continuously recorded history of that stage's input), never generated sound, and the loop never feeds back into the history, so it cannot build up or run away: the output is never louder than the input. It sits after saturation, so it repeats your processed sound.
  - **Tempo:** slice length = rate at the host's tempo (read from the host's play head every block). If the host gives no tempo, or an invalid one (zero, negative, NaN, absurd), it uses **120 BPM**, and the window says so ("NO HOST TEMPO: 120 BPM" vs "HOST TEMPO 128.0 BPM"). Valid host tempos are clamped to 40..300 BPM. Rate or tempo changes (more than 2 %) while on re-capture the slice through short fades.
  - **What "synchronised" means here:** the repeat is exactly one rhythmic step long and follows tempo changes, but the loop **starts when you switch it on**; it is not locked to the host's beat grid. Switch it on (or automate it) on the beat and the repeats land on the grid.
  - **Clicks:** switching on or off, and re-capturing, are 3 ms fades between live audio and the loop; the loop seam is a crossfade (up to 4 ms) into the audio that preceded the slice. The behaviour is computed per sample, so it does not depend on the host's block size. It adds no latency. Parameter changes take effect at the next audio block (host automation granularity).
  - **Limits:** the longest slice is 1.5 s (1/4 note at 40 BPM). The slice is the audio before the switch-on, so a loop can contain a transition if you switch on late. Not yet tested in FL Studio or any DAW.
- **Mix:** 0..100 % (default 100), a dry/processed blend (linear amplitude crossfade, smoothed over 20 ms). The dry side is the same latency-aligned copy that Bypass uses, so the blend does not comb-filter. 0 % is an exact copy and costs almost no CPU. Mix makes parallel compression and parallel saturation possible. Limit: the saturation oversampler has linear phase only up to about 10 kHz (phase error about 19 deg at 14 kHz, 81 deg at 18 kHz), so with Mix below 100 % and saturation engaged the extreme top octave can be slightly coloured.
- **Bypass:** 20 ms crossfade to the untouched (latency-aligned) input; EQ, compressor, saturation and gain are all bypassed. When settled it is an exact copy and costs almost no CPU.
- **Meters:** IN/OUT peak, -60 to +6 dBFS, red above 0 dBFS. Processing is 32-bit float with no limiter, so EQ boost plus gain can exceed 0 dBFS; watch the OUT meter.
- All parameters are exposed to the host for automation (FL Studio automation not yet confirmed) and saved/restored with the project. Parameter IDs: `gain`, `bypass`, `eq_lo_freq/gain`, `eq_b1_freq/gain/q`, `eq_b2_freq/gain/q`, `eq_hi_freq/gain`, `comp_thresh`, `comp_ratio`, `comp_attack`, `comp_release`, `comp_knee`, `comp_makeup`, `sat_drive`, `mix`, `stutter_on`, `stutter_rate`, `stutter_mix`.
- Pitch Tune (vocal pitch correction for one voice: key, scale, strength, speed; adds about 31 ms of latency only while it is on) is built, tested on synthetic voices and one real recording, and the owner reported it working in FL Studio on the Mac (Stage 4.2; not independently measured).
- FORMANT (vocal size: moves the voice's resonances without changing the notes; adds about 21 ms only while away from 0) is built and tested on synthetic voices; the prototype passed one owner listening test; not yet checked inside FL Studio.
- Vocal Mutation (Stage 4.5; a frequency shifter / ring modulator for robotic and alien voices: MUTATION = how far every partial is moved in Hz, CHARACTER = DOWN - ROBOT - UP, BLEND; adds no latency; default off) is built and tested on synthetic signals only (200 test groups); not yet auditioned on a real vocal or checked in FL Studio. IDs: `mut_on`, `mut_amount`, `mut_character`, `mut_blend`.
- MOODS now also drive Vocal Mutation (Stage 4.6): CELESTIAL a slow shimmer, VOIDWALKER a hollow undertone, THE PARASITE robotic ring modulation, ALTERED STATE an alien shift; THE CLONE switches it off. User presets (PRESET menu, SAVE, DELETE, "Factory Default"; files in `~/Library/Application Support/GlitchKid/Presets/User/`) and A/B comparison (SET A / SET B, A, B, A>B, B>A) are built and tested on Linux (219 test groups); not yet checked on the Mac or in FL Studio.
- Not built yet: factory presets beyond the default, preset folders and import/export. 

## Verification status (what has actually been checked)
- Verified on a real Apple Silicon Mac with FL Studio 2025: builds with the Command Line Tools only, `arm64` binary, loads as `VST3 (Apple - native)`, `ctest` passes, window renders and fits, IN/OUT/GR meters move, FL Studio shows the plugin-reported latency (0.14 ms = 6 samples at 44.1 kHz).
- Reported working by the project owner (not independently measured): EQ, compressor and saturation sound as expected, Bypass is clean, and FL Studio automation of GlitchKid parameters works.
- Reported working by the project owner on the Mac in FL Studio (not independently measured): the Stage 2.5 Mix control and the Stage 3.1 Glitch Stutter (on/off, rate, Stutter Mix, host-tempo following, automation and project recall as per the manual test list).
- Verified on the owner's Apple Silicon Mac after the NaN/Inf hardening and JUCE commit pin (screenshot, 2026-10-14): Release build with tests succeeds and `ctest` reports 100% tests passed (1/1). FL Studio reload after this change not yet reported.
- Verified on the owner's Apple Silicon Mac after adding Liquid Echo (screenshot, 2026-10-14): Release build with tests succeeds and `ctest` reports 100% tests passed (1/1). Liquid Echo has not yet been reported as checked inside FL Studio.
- Not yet checked: CPU load in FL Studio; other sample rates and buffer sizes inside FL Studio; other DAWs; a formal listening-test protocol across different voices.
- No claim of professional sound quality is made. The unit tests prove the maths and safety properties, not taste.

## Build on macOS (Apple Silicon)
Needs Xcode Command Line Tools, CMake >= 3.22 and Git (internet is needed once, to download JUCE).

```sh
git clone https://github.com/hazimmakesmusic-commits/GlitchKid.git
cd GlitchKid
git checkout claude/glitchkid-plugin-architecture-3o6w74   # until merged
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release              # configure (downloads JUCE 9.0.4)
cmake --build build --config Release -j 8                   # build
```
Output: `build/GlitchKid_artefacts/Release/VST3/GlitchKid.vst3`

Check it is Apple Silicon: `lipo -info build/GlitchKid_artefacts/Release/VST3/GlitchKid.vst3/Contents/MacOS/GlitchKid` (should say `arm64`).

## Install and test in FL Studio
```sh
mkdir -p ~/Library/Audio/Plug-Ins/VST3
cp -R build/GlitchKid_artefacts/Release/VST3/GlitchKid.vst3 ~/Library/Audio/Plug-Ins/VST3/
```
In FL Studio: Options > Manage plugins > confirm `~/Library/Audio/Plug-Ins/VST3` is a scan path > Find installed plugins. Add GlitchKid to a mixer insert, play audio, and check: gain knob changes level; Bypass returns to original level; save the project, reopen it, and the settings are restored. (macOS may block an unsigned plugin the first time: System Settings > Privacy & Security > allow.)

## Automated tests (unit tests of the real processor)
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGLITCHKID_BUILD_TESTS=ON
cmake --build build --config Release -j 8
ctest --test-dir build --output-on-failure
```
Expect `100% tests passed`. The tests check: bit-exact (latency-delayed) pass-through at 44.1/48/96 kHz and several block sizes, exact gain levels, bypass, ramped changes, mono, silence, meters, and state save/restore (including garbage state); the EQ's measured frequency response (bell, low and high shelf, narrow vs wide Q) and its stability under extreme and moving settings; the compressor's measured static curve (hard and soft knee), attack/release time constants at three sample rates, stereo linking, makeup gain and idle behaviour; and the saturation's small-signal gain at low and high frequency, harmonic content, anti-aliasing against a plain tanh, latency, block-size independence, fades and idle behaviour. They do not test how it sounds or the editor window.

Optional extra check with the free [pluginval](https://github.com/Tracktion/pluginval): `pluginval --strictness-level 5 ~/Library/Audio/Plug-Ins/VST3/GlitchKid.vst3`

## Identity codes (never change after release; changing them breaks saved sessions)
- Manufacturer code: `GlKd`  - Plugin code: `Gkid`  - Parameter IDs: `gain`, `bypass`

## Layout
`CMakeLists.txt` build · `Source/` plugin code · `docs/` plans · `LICENSES.md` dependency/licence notes

## CPU (indicative)
Measured in a Linux container (not Apple Silicon), 48 kHz stereo, block size 512, as a share of one core (Stage 3.1): all stages idle 0.12%; EQ + compressor 0.3%; saturation at 12 dB drive 1.6%; Glitch Stutter on 0.10%; everything on 1.4%. Real numbers on your Mac and inside FL Studio have not been measured.
