// D. KINETIC TYPOGRAPHY (ART_DIRECTION.md §7D). Lines are marked up as <span data-line><span>…</span></span>, so text stays real, selectable text.
// A line enters wide (width 125, tracking +0.12em) from behind its mask and settles condensed (width 72, tracking -0.02em): the compressor's gesture.
import { reducedMotion, tokens } from "./core.js";

export function revealLines (root, { delay = 0, from = 125, to = 72, weight = 800 } = {}) {
    const lines = [...root.querySelectorAll ("[data-line] > span")];
    if (reducedMotion()) { lines.forEach ((l) => (l.style.transform = "none")); return Promise.resolve(); }
    return Promise.all (lines.map ((el, i) => el.animate ([
        { transform: "translateY(108%)", fontVariationSettings: `"wdth" ${from}, "wght" ${weight}`, letterSpacing: "0.12em" },
        { transform: "translateY(0)", fontVariationSettings: `"wdth" ${to}, "wght" ${weight}`, letterSpacing: "-0.02em" },
    ], { duration: tokens.type.ms, delay: delay + i * tokens.type.stagger, easing: tokens.type.easing, fill: "both" }).finished));
}

// the wordmark (or any mark) tuned in along the signal: opens from the centre line with a decaying R/B channel split
export function revealMark (el, { delay = 0, ms = 760 } = {}) {
    if (reducedMotion()) { el.style.clipPath = "none"; return Promise.resolve(); }
    return el.animate ([
        { clipPath: "inset(50% 0 50% 0)", filter: "drop-shadow(-7px 0 0 rgba(255,40,70,.85)) drop-shadow(7px 0 0 rgba(60,110,255,.85))", opacity: 0.6 },
        { clipPath: "inset(0 0 0 0)", filter: "drop-shadow(-2px 0 0 rgba(255,40,70,.4)) drop-shadow(2px 0 0 rgba(60,110,255,.4))", opacity: 1, offset: 0.55 },
        { clipPath: "inset(0 0 0 0)", filter: "drop-shadow(0 0 0 rgba(255,40,70,0)) drop-shadow(0 0 0 rgba(60,110,255,0))", opacity: 1 },
    ], { duration: ms, delay, easing: tokens.move.easing, fill: "both" }).finished;
}

// a short, precise fade-up for supporting copy and controls (the "move" token)
export function revealSoft (els, { delay = 0, stagger = 80 } = {}) {
    if (reducedMotion()) return Promise.resolve();
    return Promise.all ([...els].map ((el, i) => el.animate ([{ opacity: 0, transform: "translateY(10px)" }, { opacity: 1, transform: "none" }],
        { duration: tokens.move.ms, delay: delay + i * stagger, easing: tokens.move.easing, fill: "both" }).finished));
}
