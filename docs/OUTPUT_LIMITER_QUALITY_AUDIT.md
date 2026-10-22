# Output limiter and mix-quality audit

Written **before** any DSP change in this pass, from the source at commit `e3d45cf` (clean tree). Linux only; nothing here was tested in FL Studio, on a Mac or on Windows, and nothing was listened to.

## 1. Where clipping can occur (signal chain, in order)

input -> NaN/Inf zeroing -> Pitch Tune -> Formant -> Harmony -> Mutation -> Doubler -> user EQ (+-15 dB per band) -> CLEAN LAB corrective -> compressor (makeup 0..24 dB) -> CLEAN LAB de-esser -> saturation (tanh, drive 0..18 dB, no level compensation: "quiet material is boosted by about +17 dB at 18 dB drive; trim with Output Gain") -> AIR LAB (additive presence and harmonics, trim +-12 dB) -> stutter -> echo (feedback) -> Dimension (adds a widened copy) -> output Gain (-60..+12 dB) -> Mix/Bypass crossfade -> (end).

Nothing after the saturation limits the level. Every stage after it can only add energy or gain: AIR LAB (additive), echo feedback and Dimension (sums), the +12 dB output Gain, and the saturation itself returns slightly more than 1.0 for inputs above 1.0. The compressor's makeup gain (up to +24 dB) and the profile makeup values (3.5 to 9 dB) are applied before the saturation, so a hot vocal can arrive at it well above 0 dBFS. **The output can therefore exceed 0 dBFS in ordinary use** (a loud vocal, a profile's makeup, a +6 dB EQ boost and a few dB of Gain are enough). The `OUT: CLIPPING` warning in the screenshots is consistent with this.

## 2. Is there an output limiter?
**No.** There is no limiter or clipper anywhere in `processBlock`. The only protections are the non-finite-sample clean-up at the input and the bounded saturation curve, and neither keeps the output below full scale. The saturation (tanh) is a creative waveshaper placed mid-chain, not an output stage.

## 3. Meter placement and accuracy
* `outputPeak` is raised at the very end of the chain (after the Mix crossfade) and in the DRY early-return path, so it is *post everything*, which is the right place for a post-limiter meter once a limiter exists.
* **Defect found:** both `inputPeak` and `outputPeak` use `buffer.getMagnitude (0, ...)`, i.e. **channel 0 (left) only**. A hot right channel does not show on the meters, on the chain strip, or on the CLIPPING indicator.
* The meters are sample peaks (digital), not true peaks and not loudness.
* The CLIPPING text appears when the held output peak reaches 0.999 (full scale).

## 4. Does the CLEAN LAB graph match the real EQ?
* The violet curve is the combined response of the four user-EQ biquads, calculated with the same RBJ formulas as `ParametricEq` from the live parameter values. `MacroTests` compares it with the real DSP output on a noise probe and requires agreement within 0.15 dB at the test frequencies. **It matches the real transfer function.**
* The pink curve (CLEAN LAB corrections) uses the DSP's own smoothed telemetry and `CleanLab::cleanResponseDb` with the same filter formulas; the test compares it with the real DSP on a noise probe within 0.7 dB. CLEAN LAB's moving, level-dependent de-esser is not drawn in that curve (it is shown as a gain-reduction bar), which the page says.
* The spectrum behind the graph is the real FFT of the vocal input.

## 5. Why Beat Lock showed "PRESENCE BELL (3 kHz): 10.2 -> 11.5 dB"
* **Units and mapping are correct.** The row title shows the real current value of `eq_b2_gain` (dB) and the value after the move, computed by `applyBeatLock` through the real range lookup, so the numbers are in dB of the Bell 2 gain and the parameter range is +-15 dB.
* **The 10.2 dB was already in the user's EQ** (the profiles never exceed +3 dB; a build clamps to +-6 dB). BEAT LOCK then added +1.3 dB (the rule is `clamp (2 x maskRisk, 0, 2.5)` dB).
* **The logic gap:** the rule is a *relative* delta with **no ceiling on the resulting gain**, so a vocal that already has a very large presence boost can be pushed further toward the +15 dB parameter limit. The rule also reads only the beat relationship, not the vocal itself, so it can recommend more presence for a vocal that is already bright or sibilant. (BUILD's own presence/brightness rules do check the vocal and clamp to +-6 dB; the beat-informed moves did not.)
* Also: the beat moves apply a **relative** change on top of whatever is there, so applying a second analysis compounds (the interface permits one apply per analysis; re-analysing and applying again can stack).

## 6. Dry / processed loudness
**Not matched.** DRY | PROCESSED switches the real bypass (delay-matched, 20 ms crossfade) and nothing compensates for level. CLEAN LAB shows its net level change and says to match loudness with Gain or AIR LAB trim by hand. A louder processed signal will tend to sound better; this is a known limitation, not fixed here.

## 7. Latency, CPU, thread safety, tests
* Latency: 6 samples (saturation oversampler), plus Pitch Tune / Formant / Harmony delay while each is on; reported to the host through a 10 Hz message-thread update. The dry path is delay-matched for each.
* CPU: 0.3-0.6 % of one core at default settings and 1.5-3 % with CLEAN LAB, AIR LAB, a live Instrumental input and an analysis (synthetic audio, one core, Release).
* Audio thread: no allocation (tested over 400 blocks and in the doubling/harmony suites), no locks or I/O.
* Tests: 349 groups pass; there are none for an output limiter (none exists), none for meter channel coverage, and none that bound the BEAT LOCK result on an already boosted EQ.

## 8. Plan
1. A real, optional, bypassable, post-everything output limiter (sample-peak, lookahead), four appended parameters, a real gain-reduction meter, a post-limiter two-channel peak, latency reported only while on.
2. Fix the channel-0-only meters.
3. Constrain BEAT LOCK / beat-informed BUILD moves: a ceiling on the resulting gain, no presence boost for a vocal that is already bright or sibilant, a reason shown when a move is withheld.
4. Limiter controls in FINAL TOUCH > DYNAMICS.
5. Tests, fresh-directory build, sanitizers, honest status table.
