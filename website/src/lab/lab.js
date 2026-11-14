// The lab bar shared by the prototypes: profile chips (the five profiles) and a live frame-time meter (performance validation, ART_DIRECTION.md §8).
import { profiles } from "../motion/core.js";

export function mountLabBar (...statsList) {
    const chips = document.querySelector (".lab-bar .chips");
    for (const name of Object.keys (profiles)) {
        const b = document.createElement ("button");
        b.dataset.p = name;
        b.title = name;
        b.setAttribute ("aria-label", name + " profile");
        b.setAttribute ("aria-pressed", String (document.documentElement.dataset.profile === name));
        b.onclick = () => {
            document.documentElement.dataset.profile = name;
            chips.querySelectorAll ("button").forEach ((x) => x.setAttribute ("aria-pressed", String (x === b)));
        };
        chips.append (b);
    }
    const meter = document.getElementById ("meter");
    setInterval (() => {
        const live = statsList.filter (Boolean).filter ((s) => s.frameMs > 0);
        if (! live.length) { meter.textContent = "paused (off-screen or still)"; return; }
        const frame = Math.max (...live.map ((s) => s.frameMs)), script = live.reduce ((a, s) => a + s.scriptMs, 0);
        meter.textContent = `${(1000 / frame).toFixed (0)} fps · script ${script.toFixed (2)} ms/frame`;
    }, 500);
}
