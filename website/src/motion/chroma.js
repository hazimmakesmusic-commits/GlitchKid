// B. CHROMATIC DISTORTION (ART_DIRECTION.md §7B): a picture "tuned in" like a signal. The source is split once into true R, G and B layers; a frame
// draws ~24 horizontal slices per channel, additively, with channel offsets, slice displacement, a rolling bar, scan lines and grain.
// amount 0 = the source drawn untouched (zero cost at rest). Never used on body text.
import { clamp, ease, fitCanvas, hash, reducedMotion, tokens } from "./core.js";

function splitChannels (src) {
    const w = src.naturalWidth || src.width, h = src.naturalHeight || src.height;
    return ["#ff0000", "#00ff00", "#0000ff"].map ((c) => {
        const cv = document.createElement ("canvas");
        cv.width = w; cv.height = h;
        const g = cv.getContext ("2d");
        g.drawImage (src, 0, 0, w, h);
        g.globalCompositeOperation = "multiply"; // keep one channel
        g.fillStyle = c;
        g.fillRect (0, 0, w, h);
        g.globalCompositeOperation = "destination-in"; // and the source's own transparency
        g.drawImage (src, 0, 0, w, h);
        return cv;
    });
}

function makeGrain (size = 160) {
    const c = document.createElement ("canvas");
    c.width = c.height = size;
    const g = c.getContext ("2d"), img = g.createImageData (size, size);
    for (let i = 0; i < img.data.length; i += 4) { const v = (hash (i * 0.71) * 255) | 0; img.data[i] = img.data[i + 1] = img.data[i + 2] = v; img.data[i + 3] = 255; }
    g.putImageData (img, 0, 0);
    return c;
}

export function createChroma (canvas, { fit = "contain" } = {}) {
    const ctx = canvas.getContext ("2d");
    const grain = makeGrain();
    const lines = document.createElement ("canvas"); // one 3 px scan-line tile, filled once per frame instead of a fill per line
    lines.width = 1; lines.height = 3;
    lines.getContext ("2d").fillRect (0, 0, 1, 1);
    let grainFill = null, linesFill = null;
    let source = null, channels = null, amount = 0, burstRaf = 0;
    const stats = { frameMs: 0, scriptMs: 0 };

    function destRect (w, h) {
        const sw = source.naturalWidth || source.width, sh = source.naturalHeight || source.height;
        const k = fit === "cover" ? Math.max (w / sw, h / sh) : Math.min (w / sw, h / sh);
        const dw = sw * k, dh = sh * k;
        return { x: (w - dw) / 2, y: (h - dh) / 2, w: dw, h: dh, k, sw, sh };
    }

    function render (t = performance.now() / 1000) {
        const t0 = performance.now();
        const { w, h } = fitCanvas (canvas);
        ctx.clearRect (0, 0, w, h);
        if (! source) return;
        const r = destRect (w, h);
        if (amount <= 0.001) { ctx.drawImage (source, r.x, r.y, r.w, r.h); return; }
        const step = Math.floor (t * 24); // displacement changes 24 times a second during a burst
        const slices = 24, a = amount;
        ctx.globalCompositeOperation = "lighter";
        let sy = 0;
        for (let j = 0; j < slices && sy < r.sh; j++) {
            const sh = Math.max (2, (r.sh / slices) * (0.4 + 1.2 * hash (j * 7.7 + step * 0.3)));
            const torn = hash (j * 3.1 + step) > 0.62;
            const shift = (hash (j + step * 1.7) - 0.5) * r.w * 0.09 * a * (torn ? 1 : 0.12);
            const off = [-14 * a, 0, 14 * a], vy = [-2 * a, 0, 2 * a];
            for (let c = 0; c < 3; c++)
                ctx.drawImage (channels[c], 0, sy, r.sw, Math.min (sh, r.sh - sy), r.x + shift + off[c], r.y + sy * r.k + vy[c], r.w, Math.min (sh, r.sh - sy) * r.k);
            sy += sh;
        }
        ctx.globalCompositeOperation = "source-over";
        // the rolling bar: a soft bright band travelling down, as on an unlocked video signal
        const barY = r.y + ((t * 0.9) % 1) * r.h;
        ctx.globalAlpha = 0.16 * a;
        ctx.fillStyle = "#fff";
        ctx.fillRect (r.x, barY, r.w, Math.max (2, r.h * 0.012));
        // scan lines and grain, inside the picture only
        ctx.save();
        ctx.beginPath(); ctx.rect (r.x, r.y, r.w, r.h); ctx.clip();
        grainFill ??= ctx.createPattern (grain, "repeat");
        linesFill ??= ctx.createPattern (lines, "repeat");
        ctx.globalAlpha = 0.22 * a;
        ctx.fillStyle = linesFill;
        ctx.fillRect (r.x, r.y, r.w, r.h);
        ctx.globalAlpha = 0.09 * a;
        ctx.translate ((hash (step) * 160) | 0, (hash (step + 5) * 160) | 0);
        ctx.fillStyle = grainFill;
        ctx.fillRect (r.x - 160, r.y - 160, r.w + 320, r.h + 320);
        ctx.restore();
        ctx.globalAlpha = 1;
        const cost = performance.now() - t0;
        stats.scriptMs = stats.scriptMs * 0.9 + cost * 0.1;
    }

    return {
        stats,
        setSource (img) { source = img; channels = splitChannels (img); render(); },
        setAmount (v) { amount = clamp (v); render(); },
        render,
        // a burst: in to full distortion by the peak (35 %), out with expo; `onPeak` swaps the picture at the moment it is least legible
        burst ({ ms = tokens.burst.ms, strength = 1, onPeak } = {}) {
            cancelAnimationFrame (burstRaf);
            if (reducedMotion()) { onPeak?.(); amount = 0; render(); return Promise.resolve(); } // a hard cut
            return new Promise ((done) => {
                const start = performance.now(), peak = tokens.burst.peak;
                let swapped = false, last = 0;
                const tick = (now) => {
                    const p = clamp ((now - start) / ms);
                    if (! swapped && p >= peak) { swapped = true; onPeak?.(); }
                    amount = strength * (p < peak ? Math.pow (p / peak, 2) : 1 - ease.outExpo ((p - peak) / (1 - peak)));
                    render (now / 1000);
                    if (last) stats.frameMs = stats.frameMs * 0.8 + (now - last) * 0.2;
                    last = now;
                    if (p < 1) burstRaf = requestAnimationFrame (tick);
                    else { amount = 0; render(); stats.frameMs = 0; done(); }
                };
                burstRaf = requestAnimationFrame (tick);
            });
        },
    };
}
