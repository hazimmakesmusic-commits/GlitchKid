# GLITCHKID beta website

A static site with no dependencies: HTML, CSS, vanilla JS as ES modules, and a small Node build script. It deploys to any static host. The creative source of truth is `ART_DIRECTION.md` ("Signal in Motion").

```
website/
  site.config.json   every deployment setting, in one place (environment variables override it)
  build.mjs          copies src/ to dist/ (without the motion lab), adds the logo files from ../Assets/Brand, fills in the settings, writes robots.txt (+ sitemap.xml)
  ART_DIRECTION.md   concept, grid, type, colour, motion tokens, the four motion components, the quality gate
  src/
    index.html       the page (copy checked against the plug-in source; unknowns say "awaiting confirmation")
    styles.css       tokens (VIRIDIAN on VOID, the plug-in's values), self-hosted fonts, the 12-column grid, stage labels, sections
    main.js          entry module: navigation, section reveals, the A/B player, the waitlist form
    sections/        hero.js (the signal, the wordmark, the headline), product.js (real screens tuned in with the chromatic distortion)
    motion/          the approved motion modules: core, signal (8-phase sequence), chroma (chromatic distortion), field (frequency field), kinetic (type)
    lab/             the three motion prototypes. Source only: NOT in the public build (INCLUDE_LAB=1 node build.mjs copies them for local review)
    fonts/           Archivo (display), Inter (body), JetBrains Mono (labels): WOFF2, SIL OFL, licences beside them
    assets/product/  real plug-in screens, 1x and 2x WebP, and unaltered crops of three modules (see "Product screens" below)
    assets/og-image.png  social preview (1200x630): the real lockup and the real Home screen
    audio/           the demo recordings go here (see audio/README.md)
```

## Run it locally

```bash
cd website
node build.mjs                                   # Node 18 or later
python3 -m http.server 8080 --directory dist     # or any static server (ES modules need http://, not file://), then open http://localhost:8080
```

## Settings (no domain is hard-coded)

| Setting | `site.config.json` | Environment variable | When it's empty |
|---|---|---|---|
| Site URL | `siteUrl` | `SITE_URL` | No canonical URL, no sitemap, and a relative social image (most link previews need the full URL) |
| Waitlist endpoint | `betaFormEndpoint` | `BETA_FORM_ENDPOINT` | The form validates, then says the waitlist isn't open yet. It never claims you were added. |
| Support email | `contactEmail` | `CONTACT_EMAIL` | The facts table says "awaiting confirmation" |
| Privacy policy URL | `privacyPolicyUrl` | `PRIVACY_URL` | The Privacy link goes to the FAQ answer |
| Demo recordings | `demo.original`, `demo.processed`, `demo.credit` | — | The audio section shows a clearly marked placeholder |

Everything in the config is sent to visitors, so never put a secret in it.

## Deploy

Decided: **Cloudflare Pages** (build command `node build.mjs`, output directory `dist`, root directory `website`), with a **Pages Function + D1** for the waitlist. Neither is configured yet.

## The waitlist (backend: a later stage)

Only the email is required. The form sends a `multipart/form-data` POST with `Accept: application/json` to `BETA_FORM_ENDPOINT`, and says "You're on the waitlist" **only after a 2xx response**. The function must store the entry before it answers 2xx.

Fields sent: `email`, `consent=yes`, and, only when filled in, `name`, `os`, `daw`, `genre`, `link`. A hidden `company` field catches bots: if it's filled in, nothing is sent and nothing is claimed. A double click sends once.

## Product screens (real renders, never drawn)

```bash
# 1. render the current build's editor at 2x, driven like a user: a starting sound, BUILD MY MIX on a test signal, TUNE and CLEAN LAB on
GLITCHKID_TEST_ONLY="Website shots" GLITCHKID_WEB_SHOTS_DIR=/tmp/shots ./GlitchKidTests
# 2. convert to WebP (1x + 2x), cut the module crops, rebuild the social image
python3 tools/web_assets.py /tmp/shots
```

The signal playing through the plug-in in those renders is a synthetic test tone, not a recording; the page says "a test signal playing" and never presents it as a vocal. Re-render after every interface change.

## What's real, what isn't

- **Hero:** the GlitchKid Signal is a time-based animation (it represents a signal; it is not a meter). The headline is real text, visible from the first frame.
- **Product:** real screens of the current build. The chromatic distortion only plays over them for a moment; the picture you read is the unaltered render.
- **Audio scope:** draws the real waveform of the playing demo (Web Audio). It only runs when real recordings exist.
- **Features and facts:** taken from the source and `docs/PROCESSING_SYSTEMS.md`. Only verified support is listed: VST3, Mac with Apple Silicon, tested in FL Studio 2025. Windows is planned and not available (there is no Windows build).

## Before launch: outstanding items

| Item | Owner decision / asset |
|---|---|
| Domain | Choose it, then set `SITE_URL` |
| Waitlist backend | Cloudflare Pages Function + D1 (later stage), then set `BETA_FORM_ENDPOINT` |
| Demo recordings | A real vocal take and its GlitchKid-processed version, with publishing rights (`src/audio/README.md`) |
| Privacy policy | Text and URL (`PRIVACY_URL`); approve the short statement in the FAQ and the consent line |
| Support contact | Email (`CONTACT_EMAIL`) |
| Minimum macOS version | The build targets macOS 12; confirm by testing before it's listed |
| Installation | The beta build is unsigned and not notarised; the page says macOS will warn. Signing and notarisation, or tester instructions. |
| Other DAWs | Confirm each host before listing it (only FL Studio 2025 on macOS is verified) |
| Analytics | None included. Add only with a privacy notice. |
