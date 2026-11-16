# GLITCHKID — Signal in Motion

Art direction and motion system for the GlitchKid website. Phase 1 of the creative-direction override. The three motion prototypes (`src/lab/`) implement this document. The full site waits for their approval.

## 1. Concept

**A signal enters, is transformed, and leaves as something different.** The whole site is one signal path. Each section is a stage of it, named in the interface like a channel strip:

| Stage | Section | What the signal does there |
|---|---|---|
| `IN` | Hero | A fine line enters the dark, oscillates, breaks apart, and resolves into the wordmark |
| `PROC` | Product | The resolved line becomes the plug-in. The real interface is "tuned in" through a burst of chromatic distortion |
| `MON` | Hear it | The frequency field: real audio analysis of the original and processed takes |
| `ROUTE` | Workflow | One continuous line runs through Load → Choose → Build → Refine, changing shape at each stage |
| `OUT` | Beta | The signal is quiet: a single steady line above a plain form |

This order gives the page a structure that comes from the product, not from a template: hero, product, demo, steps, form.

## 2. What we refuse (and what replaces it)

| Generic AI landing page | GlitchKid |
|---|---|
| Rounded cards for every section | Hairline rules, a measured grid, and content set directly on the dark field. Panels appear only where the product itself is a panel (the plug-in screens). |
| Glows and circles behind headings | Light comes only from the signal. Headings are black-on-black typography with one accent word. |
| Particles | The only moving matter is the signal: a line, its harmonics, and its spectrum |
| Gradient everywhere | Colour appears where frequency is: low = deep, mid = primary, high = secondary. Off the signal there is no gradient except the primary CTA. |
| Animate everything | Three cinematic moments (hero, product tune-in, frequency field). Everything else gets one short, precise reveal or none. |
| Stock musicians | The real product, the real waveform and typography. No people imagery until real artists supply it. |

## 3. Grid and composition

- **12 columns,** 1440 max, 24 px gutters, 16 px outer margin on phones.
- **Asymmetry:** text anchors to columns 1–6. Signal graphics bleed off the right edge (columns 5–12 and beyond the frame), so the page never centres a hero.
- **Measurement layer:** 1 px hairlines (BORDER `#293A34`), tick marks every 8 px on the section rules, and mono stage labels (`IN · 00`, `PROC · 01`…) at the top-left of each section. This gives the technical precision of an oscilloscope graticule, used sparingly.
- **Negative space:** at least 18 vh between sections. No section has more than one heading level visible above the fold.

## 4. Typography

| Role | Face | Setting |
|---|---|---|
| Display | **Archivo** variable (width 62–125, weight 100–900) | Uppercase, weight 800, width 72 (condensed), leading 0.86, tracking −0.02 em. Sizes from 15vw (hero, phones) down to 48 px. |
| Body | **Inter** variable | 17/1.6, weight 400, SECONDARY TEXT `#91A69C` on VOID. Measure ≤ 62 ch. |
| Technical | **JetBrains Mono** 400/500 | 11–13 px, uppercase, tracking +0.08 em. Labels, timecodes, values, captions. |

All are self-hosted WOFF2 under the SIL OFL (`src/fonts/`), about 180 KB in total. No third-party font requests.

**Kinetic type** (the type that moves) treats the headline as a signal being compressed. Each line enters wide (width 125, tracking +0.12 em) from behind a mask, and settles condensed (width 72, tracking −0.02 em). The compressor's own gesture becomes the typography's gesture.

## 5. Colour

- **VIRIDIAN is the default identity** (the same tokens as the plug-in, `Source/UI/Theme.cpp`, and `src/styles.css`): VIRIDIAN `#00DDA5`, EMERALD `#00B894`, DEEP TEAL `#08776F`; signature gradient `linear-gradient(110deg, #00DDA5 0%, #00B894 48%, #08776F 100%)`; VOID `#080D0D` (field), SURFACE `#111A18` (plane), ELEVATED `#1B2925`, BORDER `#293A34` (hairline), PRIMARY TEXT `#F1F7F4`, SECONDARY TEXT `#91A69C`. Tertiary text is secondary text mixed 15 % into VOID.
- **Profiles:** the plug-in's five profiles stay selectable. Each has primary, secondary, soft and deep. The other four sit on the original graphite foundation (`#07070a`, `#0d0c11`, `#24222b`, `#f2eff6`, `#b4adbf`, `#6f6979`).
- **Rules:**
  1. Accent colour appears only in the signal graphics, the primary CTA, focus rings and the selected state.
  2. Gradients run in frequency order: deep → primary → secondary as frequency rises, or as the signal resolves (VIRIDIAN: deep teal → emerald → viridian, the signature gradient).
  3. Chromatic separation uses true R/G/B channels: it's a physical effect, not a palette. The brand gradient is what the signal resolves into.

## 6. Motion language

### Tokens

| Token | Duration | Easing | Use |
|---|---|---|---|
| `snap` | 140 ms | `cubic-bezier(.2,.8,.2,1)` | Buttons, toggles, tabs, form focus |
| `move` | 480 ms | `cubic-bezier(.16,1,.3,1)` (expo-out) | Section reveals, crops |
| `type` | 900 ms per line, 110 ms stagger | expo-out | Kinetic headline lines (mask, width, tracking) |
| `burst` | 420 ms (peak at 35 %) | ease-in then expo-out | Chromatic distortion events |
| `signal` | 6.6 s | phase-specific (below) | The hero signal sequence, once per visit |

### Triggers

- **Hero:** on load.
- **Section reveals:** once, when 20 % of the section is visible. Never in reverse, never on every paragraph.
- **Distortion bursts:** only on the product tune-in, a section change in the product tabs, and a hover over the product image.
- **Frequency field:** continuous only while audio really plays. In idle it moves at a tenth of the speed and says it is idle.

### Reduced motion

Every component has a still state:
- the signal draws its resolved final frame;
- type appears without movement;
- distortion is skipped (a hard cut);
- the field shows one still idle frame;
- live audio drawing stays on, since it's a meter, but without trailing history.

## 7. The four motion components

### A. The GlitchKid Signal (`src/motion/signal.js`)

Canvas 2D, about 900 points per frame and a few strokes. One reusable component: `createSignal(canvas, { duration, onPhase })` with `play()`, `seek(t)` and `final()`.

| # | Time (s) | Phase | What changes |
|---|---|---|---|
| 1 | 0.0–0.8 | Enter | A 1 px line draws in from the left edge (expo-out) |
| 2 | 0.8–1.6 | Oscillate | Low amplitude sine, 1.5 → 3 cycles across the frame |
| 3 | 1.6–2.6 | Excite | Amplitude to 22 % of height, frequency chirps to 9 cycles |
| 4 | 2.6–3.4 | Interfere | Inharmonic partials and beating; the line starts to tear horizontally |
| 5 | 3.4–4.2 | Fracture | It splits into 7 layered copies with spread and phase offsets |
| 6 | 4.2–5.0 | Separate | R, G and B channels pull apart (additive), with scan lines and grain |
| 7 | 5.0–5.8 | Reconstruct | Layers and channels converge, amplitude falls, the tears close |
| 8 | 5.8–6.6 | Resolve | A clean line in the profile gradient. The wordmark is revealed along it, and the headline follows. |

After phase 8 the line rests at about 2 % amplitude: the idle breath.

### B. Chromatic distortion (`src/motion/chroma.js`)

Canvas 2D. A source (an image or a canvas) is split once into R, G and B channel layers. Each frame draws about 24 horizontal slices per channel, additively, with:
- channel offsets;
- slice displacement (a deterministic hash, so it never strobes);
- scan lines and grain.

Cost at rest is zero: with amount 0 it draws the source unchanged. Never applied to body text.

### C. Generative frequency field (`src/motion/field.js`)

Canvas 2D. **Spectral strata:**
- 28 log-spaced bands (40 Hz – 16 kHz) from a Web Audio `AnalyserNode`;
- the bands stack into flowing strata over the last 3 seconds, low frequencies deep at the bottom and high frequencies bright at the top;
- band energy sets each stratum's thickness, total spectral energy sets the field's height, and a waveform filament (time domain) runs through it.

**Live** only when an analyser is connected: the demo recordings, a file you choose, or your microphone. **Idle** uses a slow synthetic texture and is labelled `IDLE · NO AUDIO`, so it never pretends to be listening.

### D. Kinetic typography (`src/motion/kinetic.js`)

Masked line reveals with animated width, weight and tracking, using the Web Animations API (no library). Lines are split by markup (`<span data-line>`), never by measuring glyphs, so text stays selectable and readable by screen readers.

## 8. Technical choices

- **No animation library.** Canvas 2D and the Web Animations API cover everything here. WebGL would only add GPU cost and complexity for effects that 2D canvas draws at 60 fps.
- **One render loop per visible canvas,** paused off-screen and while the tab is hidden; device pixel ratio capped at 2.
- **Motion code is separate from content:** `src/motion/*.js` are ES modules with no DOM knowledge beyond their canvas or element.
- **Performance budget:** under 6 ms of script per frame on a mid-range laptop. The lab pages show live frame times.

## 9. Quality gate (asked before each phase is accepted)

1. Could this be mistaken for a template? If yes, fix composition and type, not effects.
2. Does every motion have a reason, a trigger and a still state?
3. Is the product (real screens, real sound) still the hero?
4. Is everything readable and usable with motion off?
5. Is the phone composition designed, not just shrunk?
