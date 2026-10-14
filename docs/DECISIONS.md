# Decisions log

Each entry: what was decided, why, and what was measured. Newest last.

1. **Framework: JUCE 9.0.4 + CMake + VST3 (macOS arm64 first).** Free to build with; closed-source commercial use needs the JUCE EULA (free Starter tier has a revenue/funding cap; terms for JUCE 9 were not verifiable from the build environment). Re-read https://juce.com/legal/juce-9-licence/ and confirm eligibility before any commercial release. JUCE is fetched by CMake at a pinned tag, never vendored. See LICENSES.md.
2. **No feature may be a decoration.** Every control maps to a parameter that changes audio; parameter IDs are permanent once released.
3. **"Off" means bit-exact.** The 0.1 dB parameter grid left ~9e-7 residue at "0 dB" on Apple Silicon (found by the Mac test run); gains/bands/makeup/mix within half a step of neutral are snapped to exactly neutral, and idle stages are skipped.
4. **Parameters smoothed over 20 ms** (gain, wet mix, EQ settings, drive); EQ coefficients refresh every 32 samples while moving.
5. **EQ:** own RBJ biquads in double precision (no allocation on the audio thread, no extra dependency).
6. **Compressor:** feed-forward, stereo-linked peak detector, soft knee, attack/release on the gain reduction (Giannoulis et al. 2012); default ratio 1:1 so a fresh instance is transparent. Attack/release are time constants (63% / 37%).
7. **Saturation anti-aliasing: 4x oversampling with JUCE's polyphase IIR half-band filters, not ADAA.** Measured against an ideal band-limited reference (aliasing, 10 kHz / 6 dB drive): plain tanh -27 dB, first-order ADAA -39 dB (and it dulls treble by up to 1.7 dB at 10 kHz), 4x IIR oversampling about -90 dB with flat response. A "linear part exact + ADAA on the residual" variant was rejected after it overshot badly (up to 3.7x full scale at 10 kHz).
8. **Constant 6-sample latency (0.125 ms @ 48 kHz), reported to the host.** The oversampler's latency is always present; the dry/bypass path and an idle saturation stage are delayed by the same amount so everything stays time-aligned. A linear-phase FIR oversampler would be phase-exact but costs 31-49 samples; rejected for now. Known consequence: blending dry with saturated audio (Mix < 100 %) can slightly colour the top octave (phase error 19 deg at 14 kHz, 81 deg at 18 kHz).
9. **Pitch correction is not started.** Separate feasibility gate per the master prompt.
10. **Process rule:** a Linux build and tests prove the code logic only; macOS behaviour is proven by the owner's Mac build, `ctest` and FL Studio. Tests are mutation-checked (deliberately broken code must fail them).
