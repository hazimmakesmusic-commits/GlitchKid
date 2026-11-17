// GLITCHKID website build: copies src/ (and the logo files from ../Assets/Brand) to dist/, and fills in the deployment settings.
// No dependencies. Settings come from site.config.json; environment variables override them, so a host can set them per deployment:
//   SITE_URL, BETA_FORM_ENDPOINT, CONTACT_EMAIL, PRIVACY_URL
// Usage: node build.mjs            (writes dist/; INCLUDE_LAB=1 also copies the motion lab for local review)
import { cpSync, existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname (fileURLToPath (import.meta.url));
const src = join (here, "src"), dist = join (here, "dist"), brand = join (here, "..", "Assets", "Brand");
const config = JSON.parse (readFileSync (join (here, "site.config.json"), "utf8"));
const env = process.env;
config.siteUrl = (env.SITE_URL ?? config.siteUrl ?? "").replace (/\/+$/, "");
config.betaFormEndpoint = env.BETA_FORM_ENDPOINT ?? config.betaFormEndpoint ?? "";
config.contactEmail = env.CONTACT_EMAIL ?? config.contactEmail ?? "";
config.privacyPolicyUrl = env.PRIVACY_URL ?? config.privacyPolicyUrl ?? "";
// the demo is only offered when both real recordings are present (the page shows a clearly marked placeholder otherwise)
config.demo.available = ["original", "processed"].every ((k) => config.demo[k] && existsSync (join (src, config.demo[k])));

rmSync (dist, { recursive: true, force: true });
mkdirSync (dist, { recursive: true });
// the motion lab (src/lab/) is a design tool, not part of the site: left out of the public build unless INCLUDE_LAB=1 (for local review)
const includeLab = env.INCLUDE_LAB === "1";
cpSync (src, dist, { recursive: true, filter: (p) => includeLab || ! p.startsWith (join (src, "lab")) });
for (const f of ["glitchkid-emblem.svg", "glitchkid-wordmark.svg", "glitchkid-emblem-white.svg", "glitchkid-wordmark-white.svg", "glitchkid-icon.svg"])
    cpSync (join (brand, f), join (dist, "assets", "brand", f));
for (const px of [32, 256, 512]) // favicon, touch icon, manifest-size icon
    cpSync (join (brand, "exports", "icon", `glitchkid-icon-${px}.png`), join (dist, "assets", "brand", `icon-${px}.png`));

const abs = (path) => (config.siteUrl ? `${config.siteUrl}/${path}` : path); // absolute only when a domain is configured
const escapeAttr = (s) => String (s).replace (/&/g, "&amp;").replace (/"/g, "&quot;").replace (/</g, "&lt;");
const urlTags = config.siteUrl
    ? `<link rel="canonical" href="${escapeAttr (config.siteUrl)}/">\n    <meta property="og:url" content="${escapeAttr (config.siteUrl)}/">`
    : "<!-- canonical and og:url: set SITE_URL (see website/README.md) -->";
const publicConfig = { ...config }; // everything here is public by design (it is sent to every visitor)

let html = readFileSync (join (src, "index.html"), "utf8");
html = html
    .replaceAll ("%URL_TAGS%", urlTags)
    .replaceAll ("%OG_IMAGE%", escapeAttr (abs ("assets/og-image.png")))
    .replaceAll ("%CONFIG_JSON%", JSON.stringify (publicConfig).replace (/</g, "\\u003c"));
writeFileSync (join (dist, "index.html"), html);
writeFileSync (join (dist, "robots.txt"), `User-agent: *\nAllow: /\n${config.siteUrl ? `Sitemap: ${config.siteUrl}/sitemap.xml\n` : ""}`);
if (config.siteUrl)
    writeFileSync (join (dist, "sitemap.xml"),
                   `<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9"><url><loc>${config.siteUrl}/</loc></url></urlset>\n`);

const warn = [];
if (! config.siteUrl) warn.push ("SITE_URL is not set: no canonical URL, relative social image");
if (! config.betaFormEndpoint) warn.push ("BETA_FORM_ENDPOINT is not set: the waitlist form validates but says the waitlist isn't open yet");
if (! config.demo.available) warn.push ("demo recordings missing: the audio section shows its placeholder");
console.log ("built dist/" + (warn.length ? "\n  note: " + warn.join ("\n  note: ") : ""));
