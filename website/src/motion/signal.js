// A. THE GLITCHKID SIGNAL — the site's signature animation (ART_DIRECTION.md §7A).
// A fine line enters, oscillates, gets excited, interferes, fractures into layers, separates into R/G/B, reconstructs, and resolves into a clean
// line in the profile's gradient. Canvas 2D; one path of ~900 points per layer. Time-based (it represents a signal; it is not a meter).
import { clamp, ease, fitCanvas, hash, loop, mix, span, currentProfile, spectral } from "./core.js";

export const PHASES = [
    { name: "Enter", from: 0.0, to: 0.8 },
    { name: "Oscillate", from: 0.8, to: 1.6 },
    { name: "Excite", from: 1.6, to: 2.6 },
    { name: "Interfere", from: 2.6, to: 3.4 },
    { name: "Fracture", from: 3.4, to: 4.2 },
    { name: "Separate", from: 4.2, to: 5.0 },
    { name: "Reconstruct", from: 5.0, to: 5.8 },
    { name: "Resolve", from: 5.8, to: 6.6 },
];
export const DURATION = 6.6;

// the state of the signal at time t (seconds). Pure: the same t always draws the same frame (except the idle breath, which keeps its own clock).
export function stateAt (t) {
    const s = (a, b, f) => span (t, a, b, f);
    const amp = t < 0.8 ? 0 : t < 1.6 ? mix (0, 0.06, s (0.8, 1.6, ease.inOutSine)) : t < 2.6 ? mix (0.06, 0.22, s (1.6, 2.6, ease.inOutSine))
              : t < 5.0 ? 0.22 : t < 5.8 ? mix (0.22, 0.035, s (5.0, 5.8, ease.inOutSine)) : t < 6.6 ? mix (0.035, 0.014, s (5.8, 6.6, ease.outCubic)) : 0.014;
    const cycles = t < 0.8 ? 1.5 : t < 1.6 ? mix (1.5, 3, s (0.8, 1.6)) : t < 2.6 ? mix (3, 9, s (1.6, 2.6, ease.inOutSine))
                 : t < 5.0 ? 9 : t < 5.8 ? mix (9, 3, s (5.0, 5.8, ease.inOutSine)) : t < 6.6 ? mix (3, 2, s (5.8, 6.6)) : 2;
    const down = s (5.0, 5.8, ease.inOutSine);
    return {
        enter: s (0, 0.8, ease.outExpo),          // how much of the width the line has reached
        amp, cycles,
        interfere: s (2.6, 3.4) * (1 - down),     // inharmonic partials, beating, horizontal tears
        fracture: s (3.4, 4.2, ease.inOutSine) * (1 - down),
        chroma: s (4.2, 5.0, ease.inOutSine) * (1 - down),
        resolve: s (5.8, 6.6, ease.outCubic),     // neutral silver -> the profile gradient
        phase: PHASES.findIndex ((p) => t >= p.from && t < p.to),
    };
}

function makeGrain (size = 128) {
    const c = document.createElement ("canvas");
    c.width = c.height = size;
    const g = c.getContext ("2d"), img = g.createImageData (size, size);
    for (let i = 0; i < img.data.length; i += 4) {
        const v = (hash (i * 0.37) * 255) | 0;
        img.data[i] = img.data[i + 1] = img.data[i + 2] = v;
        img.data[i + 3] = 255;
    }
    g.putImageData (img, 0, 0);
    return c;
}

export function createSignal (canvas, { y = 0.5, onPhase = () => {}, speed = 1 } = {}) {
    const ctx = canvas.getContext ("2d");
    const grain = makeGrain();
    let lastPhase = -2, startedAt = 0;
    let w = 0, h = 0;

    function path (st, t, layer, dx, dy) {
        const mid = h * y, A = st.amp * h, n = Math.max (120, Math.min (900, Math.round (w / 1.6)));
        const xEnd = w * st.enter, k = layer - 3, spread = st.fracture;
        const tearStep = Math.floor (t * 6); // tears jump 6 times a second: a glitch, not a strobe
        ctx.beginPath();
        for (let i = 0; i <= n; i++) {
            const u = i / n, x = u * w;
            if (x > xEnd) break;
            const taper = Math.pow (Math.sin (Math.PI * clamp (u * 1.02)), 0.55); // calm at the edges: a signal in a window
            const ph = t * 1.8 * speed + k * spread * 0.6;
            let v = Math.sin (2 * Math.PI * st.cycles * u - ph);
            if (st.interfere > 0) {
                v += st.interfere * (0.34 * Math.sin (2 * Math.PI * st.cycles * 2.71 * u + t * 7.3) + 0.18 * Math.sin (2 * Math.PI * st.cycles * 5.13 * u - t * 11));
                v *= 1 + st.interfere * 0.55 * Math.sin (2 * Math.PI * 0.7 * u + t * 2.1); // beating
            }
            let tx = 0, ty = 0;
            const seg = Math.floor (u * 18);
            if (st.interfere > 0.05 && hash (seg * 13.7 + tearStep) > 0.8) { // a horizontal tear
                tx = (hash (seg + tearStep * 3.1) - 0.5) * 70 * st.interfere;
                ty = (hash (seg * 2.3 + tearStep) - 0.5) * 18 * st.interfere;
            }
            const yy = mid + v * A * taper + k * spread * h * 0.034 + ty + dy;
            i === 0 ? ctx.moveTo (x + tx + dx, yy) : ctx.lineTo (x + tx + dx, yy);
        }
    }

    function draw (t) {
        ({ w, h } = fitCanvas (canvas));
        ctx.clearRect (0, 0, w, h);
        const st = stateAt (t), p = currentProfile();
        if (st.phase !== lastPhase) { lastPhase = st.phase; onPhase (st.phase, st); }
        ctx.lineJoin = "round";
        ctx.lineCap = "round";

        // the raw signal is neutral silver; the resolved signal is the profile's gradient
        const brand = ctx.createLinearGradient (0, 0, w, 0);
        brand.addColorStop (0, spectral (p, 0.15, 0.0));
        brand.addColorStop (0.18, spectral (p, 0.35));
        brand.addColorStop (0.6, spectral (p, 0.75));
        brand.addColorStop (1, spectral (p, 1.0));

        // fracture: seven layers, the outer ones fainter
        for (let layer = 0; layer < 7; layer++) {
            const centre = layer === 3;
            if (! centre && st.fracture < 0.01) continue;
            const a = centre ? 1 : st.fracture * 0.5 * (1 - Math.abs (layer - 3) / 4);
            ctx.globalAlpha = a * (1 - st.chroma * (centre ? 0.85 : 0.4));
            path (st, t, layer, 0, 0);
            ctx.strokeStyle = st.resolve > 0 ? brand : `rgba(222,226,234,${0.85})`;
            ctx.lineWidth = centre ? 1.3 + st.resolve * 0.4 : 1;
            ctx.stroke();
            if (centre && st.resolve > 0) { // the resolved line's soft light
                ctx.globalAlpha = 0.14 * st.resolve;
                ctx.lineWidth = 7;
                ctx.stroke();
            }
        }
        // separation: true R / G / B channels, additive (overlaps return to white)
        if (st.chroma > 0.01) {
            ctx.globalCompositeOperation = "lighter";
            const off = st.chroma * 16;
            [["rgba(255,40,70,", -off, -st.chroma * 3], ["rgba(40,255,140,", 0, 0], ["rgba(60,110,255,", off, st.chroma * 3]].forEach (([c, dx, dy]) => {
                ctx.globalAlpha = 1;
                path (st, t, 3, dx, dy);
                ctx.strokeStyle = c + (0.85 * st.chroma) + ")";
                ctx.lineWidth = 1.4;
                ctx.stroke();
            });
            ctx.globalCompositeOperation = "source-over";
        }
        // the head of the entering line
        if (st.enter < 1) {
            const x = w * st.enter;
            ctx.globalAlpha = 1;
            ctx.fillStyle = p.text;
            ctx.fillRect (x - 1, h * y - 1, 2, 2);
        }
        // analog texture, only while the signal is disturbed: scan lines and grain
        const disturbance = Math.max (st.interfere * 0.5, st.chroma);
        if (disturbance > 0.02) {
            ctx.globalAlpha = 0.07 * disturbance;
            ctx.fillStyle = ctx.createPattern (grain, "repeat");
            ctx.save ();
            ctx.translate ((hash (Math.floor (t * 24)) * 128) | 0, (hash (Math.floor (t * 24) + 9) * 128) | 0);
            ctx.fillRect (-128, -128, w + 256, h + 256);
            ctx.restore();
            ctx.globalAlpha = 0.12 * disturbance;
            ctx.fillStyle = "#000";
            for (let yy = 0; yy < h; yy += 3) ctx.fillRect (0, yy, w, 1);
        }
        ctx.globalAlpha = 1;
    }

    // modes: "play" (the sequence), "idle" (after it: the resolved line breathing, on its own slow clock), "frozen" (scrubbed or reduced motion)
    let mode = "idle", frozen = DURATION;
    const runner = loop (canvas, (now) => {
        if (mode === "play") {
            const t = (now - startedAt) / 1000;
            if (t >= DURATION) mode = "idle";
            draw (Math.min (t, DURATION));
        } else draw (mode === "idle" ? DURATION + now * 0.00025 : frozen);
    });
    return {
        play () { startedAt = performance.now(); mode = "play"; lastPhase = -2; },
        seek (t) { mode = "frozen"; frozen = clamp (t, 0, DURATION); },
        final () { this.seek (DURATION); }, // reduced motion: the resolved frame, still
        stats: runner.stats,
        stop: runner.stop,
        stateAt,
    };
}
