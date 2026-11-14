// C. GENERATIVE FREQUENCY FIELD (ART_DIRECTION.md §7C): spectral strata. 28 log-spaced bands (40 Hz–16 kHz) from a real AnalyserNode, kept as
// ~160 columns of history and stacked as a centred stream: low bands deep, mids primary, highs secondary. A time-domain filament runs through it.
// With no audio it shows a quiet synthetic texture and says so (IDLE · NO AUDIO): it never pretends to listen.
import { clamp, currentProfile, fitCanvas, hash, loop, reducedMotion, spectral } from "./core.js";

export const BANDS = 28, LO = 40, HI = 16000, COLUMNS = 160;
const edges = Array.from ({ length: BANDS + 1 }, (_, i) => LO * Math.pow (HI / LO, i / BANDS));

export function createField (canvas, { onReadout } = {}) {
    const ctx = canvas.getContext ("2d");
    const history = Array.from ({ length: COLUMNS }, () => new Float32Array (BANDS));
    let head = 0, analyser = null, freq = null, wave = null, label = "IDLE · NO AUDIO", lastPush = 0, lastReadout = 0;
    const live = () => !! analyser;

    function measure (now) { // one column of band energies in 0..1, real (analyser) or synthetic (idle)
        const col = new Float32Array (BANDS);
        if (live()) {
            analyser.getFloatFrequencyData (freq);
            const hzPerBin = analyser.context.sampleRate / analyser.fftSize;
            for (let b = 0; b < BANDS; b++) {
                const a = Math.max (1, Math.floor (edges[b] / hzPerBin)), z = Math.max (a + 1, Math.ceil (edges[b + 1] / hzPerBin));
                let p = 0;
                for (let k = a; k < z && k < freq.length; k++) p += Math.pow (10, freq[k] / 10);
                const db = 10 * Math.log10 (p / (z - a) + 1e-12);
                col[b] = Math.pow (clamp ((db + 85) / 65), 1.4); // -85..-20 dBFS per bin -> 0..1, curved so loud bands stand out from the floor
            }
        } else {
            const s = now * 0.00018; // idle: slow, low, clearly not music
            for (let b = 0; b < BANDS; b++) col[b] = 0.06 + 0.035 * Math.sin (s * 3 + b * 0.45) * Math.sin (s * 1.3 + b * 0.11) + 0.015 * hash (b + Math.floor (now / 400));
        }
        return col;
    }

    function readout () {
        if (! live()) return { label, rms: null, centroid: null };
        analyser.getFloatTimeDomainData (wave);
        let sq = 0; for (const v of wave) sq += v * v;
        const rms = Math.sqrt (sq / wave.length);
        let num = 0, den = 0; const hzPerBin = analyser.context.sampleRate / analyser.fftSize;
        for (let k = 1; k < freq.length; k++) { const m = Math.pow (10, freq[k] / 20); num += m * k * hzPerBin; den += m; }
        return { label, rms: 20 * Math.log10 (rms + 1e-9), centroid: den > 0 ? num / den : 0 };
    }

    function draw (now) {
        const { w, h } = fitCanvas (canvas);
        const still = reducedMotion();
        // the field advances 30 columns a second (4 a second under reduced motion: a slow, legible chart, not motion)
        if (now - lastPush > (still ? 250 : 33)) { history[head] = measure (now); head = (head + 1) % COLUMNS; lastPush = now; }
        ctx.clearRect (0, 0, w, h);
        const p = currentProfile(), mid = h * 0.5, dx = w / (COLUMNS - 1), scale = h * 0.86 / (BANDS * 0.5); // a loud, full-band column fills most of the height
        // totals per column, so the stack is centred on the line (the quiet signal grows outward from it)
        const cols = Array.from ({ length: COLUMNS }, (_, i) => history[(head + i) % COLUMNS]);
        const totals = cols.map ((c) => c.reduce ((a, v) => a + v, 0));
        let lower = cols.map ((_, i) => mid - totals[i] * scale * 0.5);
        for (let b = 0; b < BANDS; b++) {
            const upper = lower.map ((y, i) => y + cols[i][b] * scale);
            const t = b / (BANDS - 1);
            ctx.beginPath();
            ctx.moveTo (0, lower[0]);
            for (let i = 1; i < COLUMNS; i++) ctx.lineTo (i * dx, lower[i]);
            for (let i = COLUMNS - 1; i >= 0; i--) ctx.lineTo (i * dx, upper[i]);
            ctx.closePath();
            // older columns fade: the field is a record that dissolves to the left
            const g = ctx.createLinearGradient (0, 0, w, 0);
            g.addColorStop (0, spectral (p, t, 0));
            g.addColorStop (0.55, spectral (p, t, live() ? 0.55 : 0.28));
            g.addColorStop (1, spectral (p, t, live() ? 0.9 : 0.42));
            ctx.fillStyle = g;
            ctx.fill();
            // hairline between strata: the measurement, not decoration
            ctx.strokeStyle = "rgba(7,7,10,0.85)";
            ctx.lineWidth = 0.75;
            ctx.beginPath();
            ctx.moveTo (0, upper[0]);
            for (let i = 1; i < COLUMNS; i++) ctx.lineTo (i * dx, upper[i]);
            ctx.stroke();
            lower = upper;
        }
        // the filament: the actual waveform, drawn across the field's newest third
        if (live()) {
            analyser.getFloatTimeDomainData (wave);
            const x0 = w * 0.62, n = wave.length;
            ctx.beginPath();
            for (let k = 0; k < n; k += 2) { const x = x0 + (k / n) * (w - x0), y = mid + wave[k] * h * 0.22; k ? ctx.lineTo (x, y) : ctx.moveTo (x, y); }
            ctx.strokeStyle = "rgba(242,239,246,0.85)";
            ctx.lineWidth = 1.2;
            ctx.stroke();
        } else {
            ctx.fillStyle = "rgba(242,239,246,0.5)";
            ctx.fillRect (0, mid - 0.5, w, 1); // idle: a flat line, the honest picture of silence
        }
        if (onReadout && now - lastReadout > 200) { lastReadout = now; onReadout (readout()); }
    }

    for (let i = 0; i < COLUMNS; i++) history[i] = measure (performance.now() - (COLUMNS - i) * 33); // rest from the first frame
    const runner = loop (canvas, draw);
    return {
        stats: runner.stats,
        stop: runner.stop,
        // attach any AudioNode: the analyser listens, it does not change or route the sound
        attach (node, name) {
            analyser = node.context.createAnalyser();
            analyser.fftSize = 4096;
            analyser.smoothingTimeConstant = 0.55;
            freq = new Float32Array (analyser.frequencyBinCount);
            wave = new Float32Array (analyser.fftSize);
            node.connect (analyser);
            label = "LIVE · " + name;
        },
        detach () { analyser?.disconnect(); analyser = null; label = "IDLE · NO AUDIO"; },
        get label () { return label; },
    };
}
