# Roadmap

Only the first section exists today. Everything else is a plan, not a feature, and the order can change.

## Built (see `../CHANGELOG.md`)
Gain/bypass, 4-band EQ, compressor, saturation (4x oversampled), Glitch Stutter, Liquid Echo (tempo-aware delay), Dimension (stereo thickening), MOODS 2.0 (five Alter Egos with Chaos, Blend and Surprise Me), Pitch Tune, FORMANT, Vocal Mutation (Stage 4.5, single-sideband frequency shifter / ring modulator; not yet checked in FL Studio), Mix, meters, state save/restore,
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
- Pitch Tune is in the plug-in (Stage 4.2). Reported working in FL Studio on the Mac. Next: check mono tracks and the latency change when switching in more detail; listening tests on more voices; if it sounds grainy, add pitch-mark alignment to the shifter; FORMANT is in the plug-in (Stage 4.4; check it in FL Studio, including both effects on at once, and with more voices; improve the envelope estimator for high voices); consider a lower-latency mode (higher lowest pitch); optional: a vibrato-preserving mode, a "target note" lock, scale editor.
- Presets and A/B follow-ups (after the owner has used them): factory preset set beyond the Factory Default, rename, folders/import/export, an A/B mode where edits go into the active slot, an atomic parameter swap if partial application is ever audible, a real-time safe "edited" baseline that survives a session reopen.
- Vocal Mutation follow-ups (after the owner has auditioned it on real vocals): MOODS integration (an Alter Ego that switches it on with a chosen Character; needs a decision on whether Alter Egos may own `mut_*`); optionally lock the carrier to a note of the Pitch Tune key (musical but couples it to the pitch detector); a gentle low-pass before shifting UP if aliasing is ever heard; drive/feedback inside the stage if it is too clean.
- Release: signing/notarisation (Apple Developer Program, about US$99/year), installer, licensing decision.

## Needs the owner's decision
- Read the JUCE 9 EULA and confirm Starter eligibility (or choose Indie, or AGPL). Until then: no commercial release.
- Repository visibility (it is public today, so the source is visible).
