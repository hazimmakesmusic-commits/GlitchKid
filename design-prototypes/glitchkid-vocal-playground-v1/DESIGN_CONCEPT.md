# GlitchKid — THE VOCAL PLAYGROUND

> *Your voice. Your universe.*

Concept prototype v1 · design only · not approved for production. Open `index.html` in a desktop browser. It plays no audio, and everything you see moving is illustrative.

---

## 1. The idea in one sentence

**The voice is the interface.** GlitchKid opens on the artist's own voice, drawn as a living voiceprint, not on an EQ. Every creative decision visibly changes that voice. The technical controls are always one step away, but never the first thing you meet.

Most vocal plug-ins show you their processing. GlitchKid shows you **yourself**, and then lets you change who that is.

## 2. The four places

| Place | What it is | What it feels like |
|---|---|---|
| **THE BOOTH** (home) | Your voice, the MAKE IT bar, the FEEL controls, the scene you're in, and THE PATH (the real 17-stage signal chain). | A dark recording booth with one light on: you. |
| **SHAPE** | The technical foundation: CLEAN, TUNE, TONE, DYNAMICS. | An engineer's bench: bone-white on charcoal, a fine measurement grid, thin wide-tracked type, exact numbers, curves you can drag. |
| **CHARACTER** | The Experimental Lab: Formant, Harmony, Mutation, Stutter. | A specimen lab after midnight: electric pink, heavy italic type, tactile pads and ladders instead of rows of knobs. |
| **SPACE** | Doubler, Echo, Dimension, plus the FINISH stage (Gain, Mix, Output Limiter). | A room seen from above: deep ultraviolet, ultralight type with wide letter-spacing, the voice in the centre with its echoes travelling outward. |

The three worlds each have their own **colour, type voice and texture**, so you always know where you are without reading a label:

* **SHAPE** uses lilac and bone, a grid, light weight and tracking of 0.16 em.
* **CHARACTER** uses pink, a warm glow and heavy italic type.
* **SPACE** uses ultraviolet, haze and the lightest weight with tracking of 0.32 em.

They are **groupings of controls, not a new signal chain.** THE PATH under the Booth always shows the true processing order. You can see that the worlds interleave: Doubler (#5, SPACE) sits between the Lab effects, and Stutter (#12, CHARACTER) comes after AIR LAB. That is honest: grouping by meaning never pretends to reorder the sound.

## 3. The signature: THE VOICE (voiceprint)

The centre of the Booth is a mirrored, breathing outline of the incoming vocal, with fine topographic lines inside it. In the prototype it is driven by a synthetic example envelope (labelled **ILLUSTRATIVE · EXAMPLE AUDIO**). In the plug-in it would be drawn from the real input and output display taps.

It is not decoration: **every layer is a real setting**, so the picture teaches the sound.

| What you see | What it means (setting that drives it) |
|---|---|
| The bone-white outline and its brightness | SHAPE. The outline follows the level. Compression makes it steadier (`comp_ratio`). Saturation adds fine grain (`sat_drive`). A brighter EQ lightens the fill (`eq_hi_gain`, `eq_b2_gain`). |
| A smaller or larger inner ring | Formant shift (`formant_shift`). |
| A dashed pink second outline | Harmony (`harm_on`). Its size follows the interval and its strength follows the level. |
| Pink fragments breaking off the outline | Mutation (`mut_on`), with Amount and Blend. |
| Pink slices around the core | Stutter (`stutter_on`). More slices means a faster rate. |
| Violet rings travelling outward | Echo (`echo_on`). Ring count follows feedback, spacing follows time, spread follows width. |
| A soft violet haze | Dimension (`dim_on`). |
| Two faint copies to the left and right | Doubler (`dbl_on`). The offset follows width. |
| A dotted circle | The Output Limiter ceiling (`lim_on`, `lim_ceiling`). |
| Everything grey, layers gone | **DRY**: the untouched voice (`bypass`). |

The legend under the voice (Shape · Character · Space) uses the same three colours as the worlds and THE PATH. That one colour system runs through the whole product.

## 4. MAKE IT — say what you want to hear

A single line under the voice: **MAKE IT** *darker and more aggressive…*

Type a feeling or tap a suggestion: *darker and more aggressive*, *wide melodic rap*, *clean it up, keep the character*, *ghostly ad-lib*, *✦ surprise me*. GlitchKid answers like a collaborator, not a form:

1. **"I heard: DARKER · AGGRESSIVE."** The words it understood are underlined in your sentence. Words it didn't use are shown honestly ("Not used: ad-lib").
2. **"Here is what I would change."** Each proposed change is shown under its world (SHAPE, CHARACTER, SPACE) with:
   * its own checkbox;
   * the current and proposed values;
   * the reason, and the word that caused it ("AGGRESSIVE · Faster attack grabs the front of words · #8").
3. **HOW FAR:** Subtle · Clear · Bold scales the proposal.
4. **◐ HOLD TO PREVIEW** (concept): hold to hear the ticked changes, then release to return.
5. **Apply n selected** or **Cancel — keep my sound.** Undo brings the previous sound back.

It also says what it **won't** do. Ask for "louder" or "autotune" and it explains that level and tuning are yours and where to find them. Say "keep the character" and no Lab effect is touched. Asking for opposites ("darker … brighter") uses the one you said last and tells you so.

**Honesty, stated on the screen:** in this prototype the words are matched against a fixed list. There is no AI, speech recognition or language model, and nothing in the repository provides one. The concept is designed so it *could* ship that way: a deterministic word-to-settings table, unit-testable, with the same review-before-apply rules as Build My Mix.

## 5. SCENES — sounds as places

The preset browser becomes a library of **scenes**, each with its own art and mood:

* **AFTER HOURS:** amber streaks and a crescent moon.
* **GLASS VOICE:** clear refracting facets.
* **FRONT ROW:** a single spotlight over a crowd.
* **NIGHT DRIVE:** a road into an ultraviolet horizon.
* **DISTORTED ANGEL:** a fractured pink halo.
* **UNDERGROUND:** raw concrete crossed by one pink line.
* **FLOATING:** drifting soft light.
* **RAW ENERGY:** a jagged white waveform.

Every card has a short description and a **DRY / PROCESSED audition switch**. The audition is a concept; in the prototype it only changes the picture.

**These are design concepts, not new presets.** Each card is marked **PLACEHOLDER** and loads one of the eight starting profiles that already exist (for example, NIGHT DRIVE → MODERN MELODIC RAP). Loading follows the plug-in's existing rule: your tuning, Gain, Mix, AIR LAB, CLEAN and limiter stay yours.

## 6. Other details

* **FEEL** (Booth, right): the three quick controls the plug-in already has (Tone, Punch, Space), shown as large tactile faders and marked **EXISTS TODAY**.
* **THE PATH**: 17 numbered stations on one line ending in an arrow. Each is coloured by world and lit when active. Click one and its controls rise in a drawer over the Booth, with "Open in SPACE →" for the full view. The stages are buttons for reading and editing, never handles for moving.
* **DRY | PROCESSED** sits top-right as a large switch: the real Bypass parameter, which crossfades in 20 ms.
* **Help line**: point at anything and the footer explains it, including the parameter ID.

## 7. Visual language

* **Surfaces:** near-black with a hint of violet (#09080c → #1a1820), a soft spotlight gradient behind the voice, and fine grain in the scene art.
* **Colour has a job:**
  * lilac/bone = SHAPE;
  * electric pink = CHARACTER;
  * ultraviolet = SPACE;
  * soft grey = FINISH;
  * a single violet-to-pink gradient for the two actions that change your sound (PROCESSED, Apply);
  * amber, used only for honesty labels (ILLUSTRATIVE, EXAMPLE, CONCEPT, PLACEHOLDER);
  * green, used only for "EXISTS TODAY".
* **Type:** one sans family (Inter, which the plug-in already bundles) used in three voices, plus a monospace for every number, so values line up and read as measurements.
* **Motion:** only the voice moves, because it represents sound. Nothing else animates. Motion stops with the system's reduced-motion setting or the toolbar's Motion Off.
* **Avoided on purpose:** neon glows on every edge, chrome, scanlines, gaming HUD frames, random gradients, 8-bit art.

## 8. Inspiration, and where GlitchKid departs from it

The brief points to VocalDose (vocalessential.com/vst) for its philosophy: intent-driven interaction, a vocal-chain experience, simple dry/processed auditioning, and a conversational tone. **The site could not be opened from this design session** (its host is outside the session's allowed network), so this concept works from the brief's description of that philosophy, not from its screens. Nothing here copies its layout or wording.

GlitchKid's own identity comes from three things:

* **the voiceprint** (you see your voice change, layer by layer);
* **the three worlds** with their own visual personalities;
* **THE PATH's honesty** (grouping is visibly separate from signal order).

## 9. Risks this concept takes on purpose

* Opening on a picture instead of controls: some engineers will want the EQ first, so SHAPE is always one click away.
* A text field in a plug-in: hosts sometimes steal keystrokes. This is flagged in IMPLEMENTATION_PLAN.md and needs host testing.
* Scene names that promise a mood the current profiles were not designed for. They stay PLACEHOLDER until sounds are designed and listening-tested.
* "Hold to preview" bends the "nothing changes before Apply" rule. It is flagged for your decision in DESIGN_REVIEW.md.
