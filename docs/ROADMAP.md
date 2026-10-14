# Roadmap

Only the first section exists today. Everything else is a plan, not a feature, and the order can change.

## Built (see `../CHANGELOG.md`)
Gain/bypass, 4-band EQ, compressor, saturation (4x oversampled), Glitch Stutter, Liquid Echo (tempo-aware delay), Dimension (stereo thickening), MOODS 2.0 (five Alter Egos with Chaos, Blend and Surprise Me), Mix, meters, state save/restore,
NaN/Inf-safe EQ and saturation, unit tests.

## Next (hardening, from the project audit; not yet started unless noted)
- Automated regression gate (CI on a free macOS runner, if the repository's minutes allow) so Apple-Silicon-only
  bugs are caught before the owner finds them.
- Version attribute in saved state, plus a migration test.
- Check CPU, sample rates and buffer sizes inside FL Studio; test mono and other DAWs.
- Make the latency check a runtime guard, not only a debug assertion.
- Decide how the plug-in's Bypass relates to FL's wrapper controls (FL shows its own "MIX LEVEL").

## Later (not started, no code exists)
- Presets (needs a state version first).
- Alter Egos tuned by ear on real vocals; MORPH between two Alter Egos (needs per-parameter interpolation rules, a rule for switches and the echo division, a second stored selection); user presets (save/recall your own; needs a state version first); optionally an Alter Ego that also sets Glitch Stutter.
- Liquid Echo extras if wanted: ping-pong, ducking, modulation, tail reporting to the host.
- Stutter locked to the host beat grid (PPQ phase).
- Pitch Tune is in the plug-in (Stage 4.2). Next: check it in FL Studio on the Mac (latency change when switching, PDC, mono tracks); listening tests on more voices; if it sounds grainy, add pitch-mark alignment to the shifter; consider formant control and a lower-latency mode (higher lowest pitch); optional: a vibrato-preserving mode, a "target note" lock, scale editor.
- Release: signing/notarisation (Apple Developer Program, about US$99/year), installer, licensing decision.

## Needs the owner's decision
- Read the JUCE 9 EULA and confirm Starter eligibility (or choose Indie, or AGPL). Until then: no commercial release.
- Repository visibility (it is public today, so the source is visible).
