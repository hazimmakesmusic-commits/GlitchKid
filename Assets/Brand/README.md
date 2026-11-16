# GLITCHKID brand identity

**BUILT TO SOUND DIFFERENT.** Underground in personality. Premium in execution. Professional in sound.

Everything in this folder is generated from one geometric construction by `tools/brand/gen_brand.py`, or rendered by JUCE from these same files. Nothing here is a screenshot or traced artwork.

## 1. Logo system

| Asset | File | Use |
|---|---|---|
| Primary emblem | `glitchkid-emblem.svg` | A forward-leaning **GK**. The G's top is a blade cut to a point, its crossbar runs on into a K, and the K's leg breaks out of the letter as a spike: the broken signal. Brushed chrome. |
| Primary wordmark | `glitchkid-wordmark.svg` | GLITCHKID in a custom geometric letterform: straight strokes, 45° cuts, the same italic lean as the emblem, and one glitch slice through the K. Brushed chrome. |
| Lockup | `glitchkid-lockup.svg` | Emblem, wordmark and the tagline. The tagline is set in Inter (SVG text; the marks are paths). |
| White / black | `*-white.svg`, `*-black.svg` | On photos, merchandise, single-colour print. The plug-in compiles in the white versions and draws their outlines. |
| Single colour | `*-mono.svg` | Viridian `#00DDA5` (the default identity). |
| App icon | `glitchkid-icon.svg`, `exports/icon/glitchkid-icon-{1024…16}.png` | The emblem only, chrome on a VOID tile (rounded 22 %), with a soft viridian light in one corner. |
| Profile tiles | `exports/profiles/*.png` | The five colour profiles as silk-wave tiles (rendered by the plug-in's own picker code). |
| Lockups per profile | `exports/lockup/glitchkid-lockup-<profile>.png` | Product artwork, web, social: chrome marks with each profile's accent edge. |

### Usage

- **Clear space:** at least the height of the wordmark's cap on every side of the lockup; half the emblem's height around the emblem.
- **Minimum size:** emblem 16 px tall; wordmark 12 px tall (the brand test checks every stroke stays separate at 14 px).
- **Backgrounds:** near-black or graphite for chrome. Use the white or black version on photos and light backgrounds. Never put the chrome version on a mid grey.
- **Don't:** recolour the chrome with a rainbow, add glow beyond the profile's fine accent edge, outline, distress, stretch, rotate or re-letter the wordmark, or set "GLITCHKID" in a font in place of the wordmark.

## 2. Colour

**VIRIDIAN is the default identity**, the same tokens in the plug-in (`Source/UI/Theme.cpp`) and on the website (`website/src/styles.css`):

| Token | Value | Use |
|---|---|---|
| VIRIDIAN | `#00DDA5` | the bright accent: active and selected states, value arcs, real activity, accent text |
| EMERALD | `#00B894` | the gradient's centre; hover and focus |
| DEEP TEAL | `#08776F` | fills that carry white text (5.4 : 1): selected buttons, the main action |
| Signature gradient | `linear-gradient(110deg, #00DDA5 0%, #00B894 48%, #08776F 100%)` | used selectively: knob arcs, the selected stage bar, active module edges |
| VOID | `#080D0D` | the window |
| SURFACE | `#111A18` | panels |
| ELEVATED | `#1B2925` | raised sections and controls |
| BORDER | `#293A34` | separation |
| PRIMARY TEXT | `#F1F7F4` | |
| SECONDARY TEXT | `#91A69C` | (hint text is the same colour sunk 45 % into VOID) |

The other four **colour profiles** stay selectable. They sit on the original graphite foundation (below) and set only their accents. The default applies only when no valid saved choice exists, so a user who picked another profile keeps it.

| Profile | Primary (fills, selection) | Secondary (bright accent) | Soft (hover, focus) | Deep (gradient end) | Gradient |
|---|---|---|---|---|---|
| 01 VIRIDIAN (default) | `#08776F` | `#00DDA5` | `#00B894` | `#08776F` | viridian → emerald → deep teal |
| 02 MAGENTA | `#8C4CF7` | `#FF4FB8` | `#C4A6FF` | `#3B1A7A` | hot pink → ultraviolet |
| 03 SAPPHIRE | `#2563EB` | `#4FA8FF` | `#B5D6FF` | `#0B1F4D` | electric blue → midnight blue |
| 04 AMBER | `#A8501C` | `#FF9A3D` | `#FFD4A8` | `#5C2810` | warm orange → burnt copper |
| 05 VOID | `#6A6F7A` | `#C9CED6` | `#EEF0F3` | `#2A2D33` | silver → graphite |

- **Primary gradient:** secondary → centre (48 %) → primary (VIRIDIAN: #00DDA5 → #00B894 → #08776F, the signature gradient). It is used on knob arcs, slider travel, switches, the vocal core ring, the selected stage bar and active module edges.
- **Secondary gradient:** primary → deep. It is used on selected fills (white text stays readable) and glows.
- **Graphite foundation (the other four profiles):** surfaces `#0B0910` / `#101014` / `#1B1921` / `#25222D`, border `#3A3644`, text `#F0ECF5` / `#A7A0B2`.
- **Never profiled (meaning, not brand):** input signal cyan `#5EE6FF`, good `#62E0A6`, warning `#FFB54C`, error `#FF5C6C`, and chrome `#F7F8FA → #C3C8D0 → #8A909A → #DDE1E6 → #6E747E`.
- **Contrast (tested in every profile):**
  - white on primary ≥ 4.5 : 1;
  - primary on every surface ≥ 3 : 1;
  - secondary and soft as text on every surface ≥ 4.5 : 1.
- **Source of truth:** `Source/UI/Theme.cpp`. This table documents it, and `Tests/VisualTests.cpp` pins it.

## 3. Typography

- **Inter** (SIL OFL, bundled in `Assets/Fonts`) for all interface text. The wordmark is never typeset.
- **Scale:** 20 display · 14 section and module · 12.5–13 body and values · 11.5 captions · 10.5 small caps.
- **Capitals with tracking** for section names, switches and the tagline. Capitals written in code are shown in Title Case, except names and units.
- **Tagline:** Inter Bold, all caps, tracked out to the wordmark's width.

## 4. Design tokens in code

| Token | Where |
|---|---|
| Surfaces, text, borders | `Source/UI/PlaygroundWidgets.h` (`ui::obsidian`, `panelBottom`, `panelTop`, `border`, `well`, `textBright`, `textDim`, `disabled`) and HOME's `nk::` names: live, set by the profile's foundation |
| Live accents | `ui::violet` (primary), `ui::pink` (secondary), `ui::neon` (soft): set by the current profile. HOME's `nk::uv`, `nk::pinkNeon`, `nk::uvSoft` are references to them. |
| Profiles, gradients, persistence | `Source/UI/Theme.{h,cpp}` |
| Logo drawing | `Source/UI/Brand.{h,cpp}` |
| Profile selector | `Source/UI/ProfilePicker.{h,cpp}` |
| Knob | `ui::drawDial`: a gradient value arc with a halo and a machined cap. The same knob is used in both workspaces. |
| Buttons, switches, sliders | `PlaygroundLookAndFeel`: selected fill = secondary gradient; switch and slider travel = primary gradient; focus = soft ring |
| Radii | 5 (buttons), 8 (panels), 9 (tiles), 12 (cards and the journey) |
| Motion | 30 Hz live view; 100 ms page transition; MOTION off holds the bloom, drift and core glow still |

## 5. Regenerating

```bash
python3 tools/brand/gen_brand.py                       # rewrites the SVGs from the geometry
GLITCHKID_TEST_ONLY=Brand GLITCHKID_BRAND_DIR=Assets/Brand/exports ./GlitchKidTests   # re-renders the PNG exports with JUCE
```
