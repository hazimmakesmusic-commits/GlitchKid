# GLITCHKID beta website

A static site with no dependencies: HTML, CSS, vanilla JS and a small Node build script. It's fast, it deploys to any static host, and there's nothing to keep updated.

```
website/
  site.config.json   every deployment setting, in one place (environment variables override it)
  build.mjs          copies src/ to dist/, adds the logo files from ../Assets/Brand, fills in the settings, writes robots.txt (+ sitemap.xml)
  src/
    index.html       the page (all copy checked against the plug-in source; unknowns say "awaiting confirmation")
    styles.css       design tokens: surfaces + the five colour profiles (same values as Source/UI/Theme.cpp)
    main.js          hero signal study, profiles, reveals, Advanced tabs, the A/B player, the beta form
    assets/product/  real plug-in screens (rendered by the plug-in's own test suite from the current build)
    assets/og-image.png  social preview (1200x630), made from the real lockup and a real screen
    audio/           the demo recordings go here (see audio/README.md)
```

## Run it locally

```bash
cd website
node build.mjs                                   # Node 18 or later
python3 -m http.server 8080 --directory dist     # or any static server, then open http://localhost:8080
```

## Settings (no domain is hard-coded)

| Setting | `site.config.json` | Environment variable | When it's empty |
|---|---|---|---|
| Site URL | `siteUrl` | `SITE_URL` | No canonical URL, no sitemap, and a relative social image (most link previews need the full URL) |
| Beta form endpoint | `betaFormEndpoint` | `BETA_FORM_ENDPOINT` | The form validates, then says applications aren't open yet. It never shows "received". |
| Support email | `contactEmail` | `CONTACT_EMAIL` | The facts table says "awaiting confirmation" |
| Privacy policy URL | `privacyPolicyUrl` | `PRIVACY_URL` | The Privacy link goes to the FAQ answer |
| Demo recordings | `demo.original`, `demo.processed`, `demo.credit` | — | The audio section shows a clearly marked placeholder |

Everything in the config is sent to visitors, so never put a secret in it.

## Deploy (any static host)

Build command: `node build.mjs`. Publish directory: `dist`. Base directory: `website`. Set the environment variables in the host's dashboard.

- **Netlify / Cloudflare Pages / Vercel:** connect the repository and use the settings above. All three serve audio with byte-range support.
- **GitHub Pages:** run `node build.mjs` in a GitHub Actions workflow and publish `website/dist`.
- **Domain:** add it in the host's dashboard, then set `SITE_URL=https://your-domain` and redeploy.

No paid service is needed to host the site itself.

## The beta form backend (needs your decision)

There's no backend in this repository. The form sends a standard `multipart/form-data` POST with `Accept: application/json` to `BETA_FORM_ENDPOINT`, and shows "Application received" **only after a 2xx response**. Options, simplest first:

1. **A form service** (Formspree, Basin, Getform…): create a form, paste its endpoint URL into `BETA_FORM_ENDPOINT`, and turn on its spam filter. Most have a free tier with limits. Check their privacy terms, because they store the applications.
2. **Netlify Forms:** possible, but it needs a hidden static form for Netlify to detect. Ask and I'll add it.
3. **Your own function** (Cloudflare Worker, Vercel or Netlify function) that writes to a sheet or database. It must answer with a 2xx status only after it has stored the application.

Fields sent: `name`, `email`, `daw`, `os`, `genre`, `experience`, `link` (optional), `consent=yes`. A hidden `company` field catches bots; if it's filled in, nothing is sent and nothing is claimed.

## What's real, what isn't

- **Product screens:** real renders of the current build (`GLITCHKID_SNAPSHOT_DIR=… GlitchKidTests`, then converted to WebP). Re-render after interface changes.
- **Hero:** a **time-based animation** (a "signal study"), not live audio. The page says so under the hero.
- **Audio scope:** draws the **real waveform** of the playing demo (Web Audio). It only runs when real recordings exist.
- **Features and facts:** taken from the source (`CMakeLists.txt`, `Source/`, `README.md`). There's no reverb, no key detection and no Windows build, and the page says so.

## Before launch: outstanding items

| Item | Owner decision / asset |
|---|---|
| Domain | Choose it, then set `SITE_URL` |
| Beta form endpoint | Pick an option above, then set `BETA_FORM_ENDPOINT` |
| Demo recordings | A real vocal take and its GlitchKid-processed version, with publishing rights (`src/audio/README.md`) |
| Privacy policy | Text and URL (`PRIVACY_URL`). Approve the short statement in the FAQ and form consent. |
| Support contact | Email (`CONTACT_EMAIL`) |
| Beta terms | Free or paid, duration, number of places, NDA. All currently "awaiting confirmation". |
| Installation | How testers get the build (installer, signing and notarisation). Currently "awaiting confirmation". |
| Other DAWs | Confirm each host before listing it (only FL Studio 2025 on macOS is verified) |
| Analytics | None included. Add only with a privacy notice. |
| Fonts | System font stack for speed. If you want Inter on the web, add WOFF2 files (the bundled OTFs are 600 KB each). |

## Checks that were run

Playwright (Chromium), desktop 1440×900 and iPhone 13:
- **Navigation:** the mobile menu works and there's no horizontal scroll.
- **Profiles:** the profile switch applies and is remembered.
- **Tabs:** the Advanced tabs show the right screens.
- **Audio:** the placeholder shows (and nothing plays) without recordings. With two test files, the player switches versions at the same position and plays one at a time.
- **Form:** field errors appear. There's an honest message with no endpoint, "not sent" on a server error, and "received" only on a 2xx. A double click sends once, the honeypot isn't sent, and a returning visitor sees the duplicate warning.
- **Reduced motion:** no reveal animations.
- **Console:** no errors.

The test tones used for the player check were deleted afterwards and are not part of the site.
