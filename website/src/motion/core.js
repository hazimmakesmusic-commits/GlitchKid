// GLITCHKID motion core: tokens, easings, profile colours and one well-behaved render loop. No DOM knowledge beyond what is passed in.
export const reducedMotion = () => matchMedia ("(prefers-reduced-motion: reduce)").matches;

export const tokens = { // ART_DIRECTION.md §6
    snap: { ms: 140, easing: "cubic-bezier(.2,.8,.2,1)" },
    move: { ms: 480, easing: "cubic-bezier(.16,1,.3,1)" },
    type: { ms: 900, stagger: 110, easing: "cubic-bezier(.16,1,.3,1)" },
    burst: { ms: 420, peak: 0.35 },
};

export const clamp = (x, a = 0, b = 1) => Math.min (b, Math.max (a, x));
export const mix = (a, b, t) => a + (b - a) * t;
export const ease = {
    outExpo: (t) => (t >= 1 ? 1 : 1 - Math.pow (2, -10 * t)),
    outCubic: (t) => 1 - Math.pow (1 - t, 3),
    inOutSine: (t) => -(Math.cos (Math.PI * t) - 1) / 2,
};
// progress of t through [a, b], eased
export const span = (t, a, b, f = (x) => x) => f (clamp ((t - a) / (b - a)));

// the five profiles (identical to the plug-in, Source/UI/Theme.cpp). VIRIDIAN is the default identity: its spectral middle is emerald, so the
// signal resolves along the signature gradient (deep teal -> emerald -> viridian); field and text are each profile's foundation.
const graphite = { field: "#07070a", text: "#f2eff6" };
export const profiles = {
    viridian: { primary: "#08776f", secondary: "#00dda5", soft: "#00b894", deep: "#08776f", mid: "#00b894", field: "#080d0d", text: "#f1f7f4" },
    magenta:  { primary: "#8c4cf7", secondary: "#ff4fb8", soft: "#c4a6ff", deep: "#3b1a7a", ...graphite },
    sapphire: { primary: "#2563eb", secondary: "#4fa8ff", soft: "#b5d6ff", deep: "#0b1f4d", ...graphite },
    amber:    { primary: "#a8501c", secondary: "#ff9a3d", soft: "#ffd4a8", deep: "#5c2810", ...graphite },
    void:     { primary: "#6a6f7a", secondary: "#c9ced6", soft: "#eef0f3", deep: "#2a2d33", ...graphite },
};
export const currentProfile = () => profiles[document.documentElement.dataset.profile] || profiles.viridian;

export function hexToRgb (hex) {
    const n = parseInt (hex.slice (1), 16);
    return [(n >> 16) & 255, (n >> 8) & 255, n & 255];
}
// colour along the frequency order deep -> primary (VIRIDIAN: emerald) -> secondary (t in 0..1)
export function spectral (p, t, alpha = 1) {
    const stops = [p.deep, p.mid || p.primary, p.secondary].map (hexToRgb);
    const x = clamp (t) * 2, i = Math.min (1, Math.floor (x)), f = x - i;
    const c = stops[i].map ((v, k) => Math.round (mix (v, stops[i + 1][k], f)));
    return `rgba(${c[0]},${c[1]},${c[2]},${alpha})`;
}

// size a canvas to its CSS box at a capped device pixel ratio; returns the drawing size in CSS pixels
export function fitCanvas (canvas, maxDpr = 2) {
    const dpr = Math.min (maxDpr, window.devicePixelRatio || 1);
    const w = canvas.clientWidth, h = canvas.clientHeight;
    if (canvas.width !== Math.round (w * dpr) || canvas.height !== Math.round (h * dpr)) {
        canvas.width = Math.round (w * dpr);
        canvas.height = Math.round (h * dpr);
    }
    canvas.getContext ("2d").setTransform (dpr, 0, 0, dpr, 0, 0);
    return { w, h, dpr };
}

// one render loop: runs only while the element is on screen and the tab is visible; reports frame cost for the lab's meter
export function loop (element, frame) {
    let visible = true, raf = 0, last = 0, running = true;
    const stats = { frameMs: 0, scriptMs: 0 };
    const tick = (now) => {
        raf = 0;
        if (! running || ! visible || document.hidden) return;
        const t0 = performance.now();
        frame (now, last ? now - last : 16.7);
        const cost = performance.now() - t0;
        stats.scriptMs = stats.scriptMs * 0.95 + cost * 0.05;
        if (last) stats.frameMs = stats.frameMs * 0.95 + (now - last) * 0.05;
        last = now;
        raf = requestAnimationFrame (tick);
    };
    const kick = () => { if (! raf && running && visible && ! document.hidden) { last = 0; raf = requestAnimationFrame (tick); } };
    const io = new IntersectionObserver (([e]) => { visible = e.isIntersecting; kick(); });
    io.observe (element);
    document.addEventListener ("visibilitychange", kick);
    kick();
    return { stats, stop () { running = false; stats.frameMs = 0; io.disconnect(); cancelAnimationFrame (raf); }, start () { running = true; kick(); } };
}

// deterministic hash in 0..1 (for tears and slices that never strobe)
export const hash = (n) => { const s = Math.sin (n * 127.1 + 311.7) * 43758.5453; return s - Math.floor (s); };
