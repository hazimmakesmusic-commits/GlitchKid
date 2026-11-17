// PRODUCT REVEAL (PROC · 01): real screens of the current build. Each screen is a normal <img> (sharp, accessible, works without script); the
// chromatic distortion (motion/chroma.js) is a canvas laid over it only while a burst plays:
//   - Home "tunes in" once, the first time it is seen: the resolved signal and the wordmark (where the hero leaves off) break up into the interface;
//   - the Advanced Mixing stages switch through a short burst, the new screen swapped in at the burst's peak;
//   - a pointer entering a screen gets a small burst, at most once a second.
// Reduced motion: no canvas at all; stages switch with a hard cut.
import { createChroma } from "../motion/chroma.js";
import { createSignal } from "../motion/signal.js";
import { reducedMotion } from "../motion/core.js";

const W = 1120, H = 680; // the editor's size; bursts work on a 1x copy (the 2x picture stays in the <img>)
const load = (src) => new Promise ((ok, bad) => { const i = new Image(); i.onload = () => ok (i); i.onerror = bad; i.src = src; });
const copy = (img) => { const c = document.createElement ("canvas"); c.width = W; c.height = H; c.getContext ("2d").drawImage (img, 0, 0, W, H); return c; };

// the picture the hero leaves behind: the resolved signal with the wordmark on it, at the screen's size
async function introPicture () {
    const host = document.createElement ("canvas");
    host.style.cssText = `position:fixed;left:-${W * 2}px;top:0;width:${W}px;height:${H}px`;
    document.body.append (host);
    const sig = createSignal (host, { y: 0.56 });
    sig.final();
    await new Promise ((r) => requestAnimationFrame (() => requestAnimationFrame (r)));
    sig.stop();
    const c = document.createElement ("canvas");
    c.width = W; c.height = H;
    const g = c.getContext ("2d");
    g.fillStyle = "#080d0d";
    g.fillRect (0, 0, W, H);
    g.drawImage (host, 0, 0, W, H);
    host.remove();
    const mark = await load ("assets/brand/glitchkid-wordmark.svg");
    const mw = 560, mh = (mw * mark.height) / mark.width;
    g.drawImage (mark, 90, H * 0.56 - mh - 18, mw, mh);
    return c;
}

function screen (figure) {
    const img = figure.querySelector ("img"), canvas = figure.querySelector (".screen__fx");
    const chroma = createChroma (canvas);
    let busy = false, lastHover = 0;
    const ready = () => (img.complete && img.naturalWidth ? Promise.resolve() : new Promise ((r) => img.addEventListener ("load", r, { once: true })));
    async function burst (opts) {
        busy = true;
        figure.classList.add ("is-fx");
        await chroma.burst (opts);
        figure.classList.remove ("is-fx", "is-pending");
        busy = false;
    }
    if (matchMedia ("(hover: hover)").matches)
        figure.addEventListener ("pointerenter", async () => {
            if (busy || performance.now() - lastHover < 1000 || figure.classList.contains ("is-pending")) return;
            lastHover = performance.now();
            await ready();
            chroma.setSource (copy (img));
            burst ({ ms: 300, strength: 0.28 });
        });
    return { figure, img, chroma, burst, ready, get busy () { return busy; } };
}

export function mountProduct (root) {
    if (! root) return;
    const motion = ! reducedMotion();
    const home = root.querySelector (".screen--home"), advanced = root.querySelector (".screen--advanced");
    const homeScreen = motion && home ? screen (home) : null;
    const advScreen = motion && advanced ? screen (advanced) : null;

    // Home tunes in the first time it is seen
    if (homeScreen) {
        home.classList.add ("is-pending");
        const intro = introPicture();
        new IntersectionObserver (async ([e], io) => {
            if (! e.isIntersecting) return;
            io.disconnect();
            try {
                const [picture] = await Promise.all ([intro, homeScreen.ready()]);
                homeScreen.chroma.setSource (picture);
                home.classList.add ("is-fx");
                await new Promise ((r) => setTimeout (r, 420)); // the resolved line holds for a moment before it breaks up
                await homeScreen.burst ({ ms: 620, onPeak: () => homeScreen.chroma.setSource (copy (homeScreen.img)) });
            } catch { home.classList.remove ("is-fx", "is-pending"); } // never leave the screen hidden
        }, { threshold: 0.35 }).observe (home);
    }

    // Advanced Mixing stages: real screens, switched through a burst (or a hard cut)
    const tabs = [...root.querySelectorAll (".stages [role=tab]")], caption = root.querySelector (".stages__caption"), img = advanced?.querySelector ("img");
    async function select (tab) {
        if (tab.getAttribute ("aria-selected") === "true" || advScreen?.busy) return;
        tabs.forEach ((t) => { t.setAttribute ("aria-selected", String (t === tab)); t.tabIndex = t === tab ? 0 : -1; });
        caption.textContent = tab.dataset.caption;
        const base = `assets/product/${tab.dataset.shot}`;
        const next = await load (`${base}.webp`);
        const swap = () => {
            img.srcset = `${base}.webp 1120w, ${base}@2x.webp 2240w`;
            img.src = `${base}.webp`;
            img.alt = `GlitchKid Advanced Mixing, ${tab.textContent} stage.`;
        };
        if (! advScreen) return swap();
        await advScreen.ready();
        advScreen.chroma.setSource (copy (img));
        advScreen.burst ({ onPeak: () => { swap(); advScreen.chroma.setSource (copy (next)); } });
    }
    tabs.forEach ((tab, i) => {
        tab.tabIndex = tab.getAttribute ("aria-selected") === "true" ? 0 : -1;
        tab.addEventListener ("click", () => select (tab));
        tab.addEventListener ("keydown", (e) => { // arrow keys move between stages (the tablist pattern)
            const d = e.key === "ArrowRight" ? 1 : e.key === "ArrowLeft" ? -1 : 0;
            if (! d) return;
            e.preventDefault();
            const t = tabs[(i + d + tabs.length) % tabs.length];
            t.focus();
            select (t);
        });
    });
    addEventListener ("resize", () => { homeScreen?.chroma.render(); advScreen?.chroma.render(); });
}
