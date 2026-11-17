// HERO (IN · 00): the GlitchKid Signal plays once per visit (motion/signal.js, the approved 8-phase sequence), the wordmark tunes in on the resolved
// line (revealMark), and the headline — real text, readable from the first frame — gets only the compression gesture (revealLines, unmasked).
// Then the signal freezes on its resolved frame: a deliberate still state. Reduced motion or a second visit in the session: the still state at once.
import { createSignal, PHASES, DURATION } from "../motion/signal.js";
import { revealLines, revealMark } from "../motion/kinetic.js";

export function mountHero (root) {
    if (! root) return;
    const canvas = root.querySelector (".hero__signal"), frame = root.querySelector (".hero__frame"), mark = root.querySelector (".hero__mark");
    const lines = root.querySelector ("[data-kinetic]"), readout = root.querySelector (".hero__readout");
    const play = document.documentElement.classList.contains ("hero-play"); // decided before first paint (index.html head script)

    // the signal runs along the foot of the wordmark, wherever the layout puts it (measured, so every screen size and height lines up)
    let lineY = 0.45;
    const measure = () => {
        const c = canvas.getBoundingClientRect(), m = mark.getBoundingClientRect(), f = frame.getBoundingClientRect();
        if (! c.height) return;
        lineY = (m.bottom - c.top + 10) / c.height; // relative to the canvas (on phones it is wider and shorter than the hero)
        readout.style.top = `${m.bottom - f.top - 12}px`;
    };
    measure();
    document.fonts?.ready.then (measure); // the display face changes the headline's height, and so the mark's position

    let resolved = false;
    const resolve = (animate) => {
        if (resolved) return;
        resolved = true;
        if (animate) revealMark (mark).then (() => mark.style.removeProperty ("clip-path"));
        else mark.style.clipPath = "none";
        mark.style.animation = "none"; // the CSS failsafe is no longer needed
    };
    const signal = createSignal (canvas, {
        y: () => lineY,
        onPhase: (i) => {
            readout.textContent = i >= 0 ? `Phase ${String (i + 1).padStart (2, "0")} · ${PHASES[i].name}` : "Resolved";
            if (i === 7) resolve (true);
        },
    });

    // still state: the resolved frame, drawn, then the loop stops (nothing redraws a picture that does not change)
    let still = false;
    const settle = () => {
        still = true;
        signal.seek (DURATION);
        requestAnimationFrame (() => requestAnimationFrame (() => signal.stop()));
    };
    addEventListener ("resize", () => {
        measure();
        if (still) { signal.start(); requestAnimationFrame (() => requestAnimationFrame (() => signal.stop())); }
    });

    if (! play) {
        resolve (false);
        settle();
        return;
    }
    try { sessionStorage.setItem ("gk-signal", "1"); } catch { /* private mode: it simply plays again next time */ }
    signal.play();
    if (! matchMedia ("(max-width: 760px)").matches) // phones: the headline wraps, and a width animation would re-wrap it (and move what is below)
        revealLines (lines, { mask: false, from: 112, delay: 0 });
    setTimeout (() => { resolve (true); settle(); }, DURATION * 1000 + 120); // also covers a tab that was hidden during the sequence
}
