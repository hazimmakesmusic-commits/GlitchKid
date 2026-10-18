# Interactive playground UI

The window has two parts: the **HOME** screen (always shown) and the **LAB** (the previous detailed controls, shown with the LAB button).
Closed, the window is the home screen only; open, it grows to show everything. Both keep their aspect ratio and scale with the window.

## HOME (top to bottom)

| Area | What it is | Connected to |
|---|---|---|
| The Kid + scope | original vector mascot; pupils grow with the real input peak, eyes look toward the loaded Sound World, mood follows the AutoMode state; the strip is a rolling view of recent input peaks (~4 s) | `consumeInputPeak()` (the same atomic the IN meter reads) |
| Preset bar | `<` `>` step, preset menu, SAVE, DELETE, A/B | existing preset system (unchanged) |
| Sound Worlds | ASTRO MODE, HEARTBREAK, DIGITAL ALTER EGO cards; click loads the built-in preset (the small line names it); the loaded world lights up | `PresetController::loadFactory` |
| Main action | status ring, ANALYZE VOCAL / STOP, APPLY SOUND, INTENSITY, status text | AutoMode (ASTRO MODE only): `VocalAnalyzer`, `automode::compute` |
| Macros | SPACE, CHARACTER, PUNCH, CHAOS dials | `macros::apply/read`, `setChaos` (see below) |
| Talk to the Kid | DARKER, MORE SPACE, HARDER TUNE, WEIRDER | `macros::apply(Command)` |
| Alter Egos | the five MOODS buttons | existing |
| DRY / PROCESSED | big switch + "HEARING:" caption | the real `bypass` parameter |
| MOTION, LAB | animations on/off, show/hide the detailed controls | UI preferences saved in the session |

Status ring states are real: grey dot = idle; spinning arc = waiting for audio (ANALYZE pressed, nothing has reached the plug-in yet); filling arc = listening
(fill = vocal heard / 8 s); green check = analysis ready; pink X = could not analyse (too short / too quiet / invalid). Nothing is simulated.

## Macros (exact mappings; ranges are the real parameter ranges)

Dragging a macro writes the parameters below (only those that changed, with host gestures); its position is read back from one primary parameter, so it follows the detailed controls.
A macro overwrites those parameters while it is moved; use A/B (SET A first) to get back.

* **SPACE**: echo_on/dim_on/dbl_on (on above 2 %), echo_mix 0-60 %, echo_fb 15-45 %, echo_width 20-75 %, dim_mix 0-45 %, dbl_amount 0-40 %. Primary: echo_mix/60. *There is no reverb in GlitchKid; this is delay + width.*
* **CHARACTER**: sat_drive 0-12 dB, eq_b2_gain 0-+2.5 dB. Primary: sat_drive/12.
* **PUNCH**: comp_ratio 1-6, comp_thresh -12..-26 dB, attack 15 ms, release 110 ms, knee 6 dB, makeup min(4, 0.8 (ratio-1)) dB. Primary: (ratio-1)/5.
* **CHAOS**: the existing Alter Ego Chaos (same as the LAB slider). Disabled until an Alter Ego is selected.

Never touched by macros or commands: Gain, Mix, Bypass, key, scale, FORMANT, Mutation (except WEIRDER), Harmony, Stutter, the Alter Ego selection.

## Quick commands (not language understanding)

Four fixed buttons, each one bounded change: **DARKER** (top shelf -1.5 dB, presence -0.5 dB, echo tone x0.85), **MORE SPACE** (SPACE +15 points),
**HARDER TUNE** (Pitch Tune on, strength +10 %, speed -8 ms; adds ~31 ms of reported latency), **WEIRDER** (Mutation on, +15 % amount, +10 % character).
Typed or spoken requests ("make it darker") are **not implemented**; the layout leaves a panel for them. A request that has no DSP behind it (for example reverb size, doubling intervals beyond the existing Doubler, key-aware harmony) must be reported as unsupported rather than approximated.

## Design system

Obsidian background; acid lime = primary/active; electric violet = atmosphere; hot pink = chaos, mutation, errors, dry; chrome = highlights and caps.
`Source/UI/PlaygroundWidgets.*` holds the palette, the look-and-feel (glossy buttons, chrome-capped knobs, slim sliders, tooltips) and the widgets. No images, fonts, assets or network.

## Real-time safety

All painting and animation run on the message thread from the editor's single 30 Hz timer; nothing in the audio callback changed for the UI. The audio thread only raises the existing input-peak atomic;
the analyzer's atomics were added in the AutoMode stage. A widget repaints only when its value moved. MOTION off makes every widget jump to its end state and stops idle animation (blinking, antenna sway, spinning ring, hop).
Macro and command application uses ordinary parameter calls on the message thread, each with a host gesture, only for parameters that changed.

## Testing

`Tests/MacroTests.cpp`: macro/command ranges, determinism, limits, read-back, only-listed-parameters, parameter application, UI preferences, and an editor smoke test that builds the real editor,
runs its timer and renders it in every AutoMode state, both LAB states, at several sizes. Set `GLITCHKID_SNAPSHOT_DIR=/some/folder` to write the renders as PNG files.
The renders show layout; they do not replace looking at the plug-in on a real screen in the host.
