# Sources: what GlitchKid can actually hear

A plug-in on one track hears only what the host sends to it. This is the truth for the current build.

| Source | How GlitchKid gets it | Status |
|---|---|---|
| **Lead vocal** | The audio on the track GlitchKid is inserted on (the main input) | Works. Analysed by ANALYZE VOCAL. |
| **Instrumental** | An optional second input bus named "Instrumental" (a sidechain-style input). It is OFF by default, so a fresh instance behaves exactly like the old single-input plug-in. The host must send the beat's track to it. | Built and unit-tested with synthetic audio (it is only measured, never mixed into the output). **Not yet tested in FL Studio, on a Mac or on Windows.** |
| **Ad-libs** | Not separately available. One instance hears one input. A second GlitchKid instance on the ad-lib track could be analysed on its own. | Not implemented. The window says "AD-LIBS: NOT AVAILABLE". |

## What the window says
- `INSTRUMENTAL: NOT CONNECTED`: the bus is off (the host gave nothing).
- `CONNECTED, BUT SILENT`: the bus is on but carried silence for more than a second. Hosts send silence for an unrouted sidechain, so "not routed" and "playing silence" look the same.
- `LIVE`: audio is arriving on the bus.
- "The beat has been analysed" is only shown (as a BEAT: finding) after ANALYZE listened to real instrumental audio.

## FL Studio (to test; do not assume)
FL Studio can send a mixer track to a plug-in's sidechain input, but this has NOT been verified with GlitchKid. The intended routing is: put GlitchKid on the vocal track, then in the mixer route the beat's track to it as a sidechain send and pick it as the "Instrumental" input in the plug-in's wrapper settings. Press ANALYZE and play the whole section. If the status stays NOT CONNECTED, the host did not enable the bus.
Fallbacks if it does not work (not built): a rendered-reference file analyser, or a small companion utility.

## What the measurements are
`VocalAnalyzer` measures the active audio: peak, RMS, crest factor, clipping, level percentiles and five coarse energy bands. `automode::compare` (SourceRelationship) turns the lead and instrumental analyses into per-band "masking risk" estimates and plain-English findings. These are estimates from five coarse bands (digital levels, not LUFS, not calibrated spectra). They say nothing about emotion, genre or quality.
