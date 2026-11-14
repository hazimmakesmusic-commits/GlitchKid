// GLITCHKID website behaviour. No dependencies. Every feature degrades: no JS = a readable page; reduced motion = still visuals.
"use strict";
const config = JSON.parse (document.getElementById ("site-config")?.textContent || "{}");
const reduceMotion = matchMedia ("(prefers-reduced-motion: reduce)").matches;
const store = { // browser storage can be unavailable (private windows, blocked cookies): never let it break the page
    get: (k) => { try { return localStorage.getItem (k); } catch { return null; } },
    set: (k, v) => { try { localStorage.setItem (k, v); } catch { /* not kept */ } },
};
const css = (name) => getComputedStyle (document.documentElement).getPropertyValue (name).trim();
const el = (tag, props = {}, text = "") => Object.assign (document.createElement (tag), props, text ? { textContent: text } : {});
document.querySelectorAll ("[data-year]").forEach ((n) => (n.textContent = new Date().getFullYear()));

// ---- colour profiles (the same five as the plug-in; MAGENTA by default) ----
const profiles = ["viridian", "magenta", "sapphire", "amber", "void"];
const profileListeners = [];
function setProfile (name, remember) {
    if (! profiles.includes (name)) name = "magenta";
    document.documentElement.dataset.profile = name;
    document.querySelectorAll (".swatches [data-profile]").forEach ((b) => b.setAttribute ("aria-checked", String (b.dataset.profile === name)));
    if (remember) store.set ("gk-profile", name);
    profileListeners.forEach ((f) => f());
}
setProfile (store.get ("gk-profile") || "magenta", false);
document.querySelectorAll (".swatches [data-profile]").forEach ((b) => b.addEventListener ("click", () => setProfile (b.dataset.profile, true)));

// ---- navigation ----
const toggle = document.querySelector (".nav__toggle"), links = document.getElementById ("nav-links");
toggle?.addEventListener ("click", () => {
    const open = toggle.getAttribute ("aria-expanded") !== "true";
    toggle.setAttribute ("aria-expanded", String (open));
    links.classList.toggle ("is-open", open);
});
links?.addEventListener ("click", (e) => {
    if (e.target.closest ("a")) { toggle.setAttribute ("aria-expanded", "false"); links.classList.remove ("is-open"); }
});

// ---- reveal on scroll: once, gently; skipped entirely with reduced motion ----
if (! reduceMotion && "IntersectionObserver" in window) {
    document.documentElement.classList.add ("motion");
    const io = new IntersectionObserver ((entries) => entries.forEach ((e) => {
        if (e.isIntersecting) { e.target.classList.add ("is-visible"); io.unobserve (e.target); }
    }), { rootMargin: "0px 0px -8% 0px", threshold: 0.12 });
    document.querySelectorAll (".reveal").forEach ((n) => io.observe (n));
}

// ---- Advanced Mixing tabs: real screens of each stage ----
const tabImage = document.querySelector (".tabs__image"), tabCaption = document.querySelector (".tabs__caption");
document.querySelectorAll (".tabs [role=tab]").forEach ((tab, i, all) => {
    tab.addEventListener ("click", () => {
        all.forEach ((t) => t.setAttribute ("aria-selected", String (t === tab)));
        tabImage.style.opacity = "0.2";
        const next = new Image();
        next.onload = () => { tabImage.src = next.src; tabImage.alt = "GlitchKid Advanced Mixing, " + tab.textContent + " stage."; tabImage.style.opacity = "1"; };
        next.src = "assets/product/" + tab.dataset.shot + ".webp";
        tabCaption.textContent = tab.dataset.caption;
    });
    tab.addEventListener ("keydown", (e) => { // arrow keys move between tabs
        const d = e.key === "ArrowRight" ? 1 : e.key === "ArrowLeft" ? -1 : 0;
        if (d) { const t = all[(i + d + all.length) % all.length]; t.focus(); t.click(); }
    });
});

// ---- HERO: a signal study. Time-based animation (NOT live audio): a raw, unsteady signal enters from the left, passes through the instrument
// (the bright plane) and leaves as layered, shaped harmonics in the profile's gradient, breathing with a phrase-like envelope. ----
(function hero() {
    const canvas = document.querySelector (".hero__canvas");
    if (! canvas) return;
    const ctx = canvas.getContext ("2d");
    let w = 0, h = 0, running = true, colours = {};
    const readColours = () => (colours = { a: css ("--secondary"), b: css ("--primary"), c: css ("--soft") });
    profileListeners.push (() => { readColours(); if (reduceMotion) draw (2.2); });
    readColours();
    function resize() {
        const dpr = Math.min (2, window.devicePixelRatio || 1);
        w = canvas.clientWidth; h = canvas.clientHeight;
        canvas.width = Math.round (w * dpr); canvas.height = Math.round (h * dpr);
        ctx.setTransform (dpr, 0, 0, dpr, 0, 0);
    }
    const smooth = (e0, e1, x) => { const t = Math.min (1, Math.max (0, (x - e0) / (e1 - e0))); return t * t * (3 - 2 * t); };
    // a deterministic "rough" signal: detuned partials plus fast jitter (no per-frame randomness, so it never flickers)
    const raw = (x, t, k) => Math.sin (x * 0.021 + t * 1.7 + k) * 0.5 + Math.sin (x * 0.067 - t * 2.3 + k * 2.1) * 0.28 + Math.sin (x * 0.19 + t * 5.1 + k * 0.7) * 0.22 * Math.sin (t * 0.9 + x * 0.004);
    const shaped = (x, t, k) => Math.sin (x * 0.012 - t * 1.1 + k * 0.55) * (0.62 + 0.38 * Math.sin (t * 0.35 + k)) + Math.sin (x * 0.024 - t * 1.6 + k) * 0.18;

    function draw (t) {
        ctx.clearRect (0, 0, w, h);
        const cx = w * (w < 700 ? 0.36 : 0.42), mid = h * (w < 700 ? 0.62 : 0.56), amp = Math.min (h * 0.2, 170);
        const env = 0.55 + 0.45 * Math.pow (Math.sin (t * 0.42) * 0.5 + 0.5, 1.4); // a slow phrase envelope: the "vocal" swells and rests

        // the spectral field behind the shaped side: thin columns whose heights follow the same envelope
        const cols = Math.floor ((w - cx) / 14);
        for (let i = 0; i < cols; i++) {
            const x = cx + 30 + i * 14, f = i / cols;
            const hgt = amp * 1.5 * env * (0.25 + 0.75 * Math.abs (Math.sin (i * 0.37 + 1.3) * Math.cos (i * 0.11))) * (1 - f * 0.55);
            const g = ctx.createLinearGradient (0, mid - hgt, 0, mid + hgt);
            g.addColorStop (0, "transparent"); g.addColorStop (0.5, colours.b); g.addColorStop (1, "transparent");
            ctx.globalAlpha = 0.07 * (1 - f * 0.6);
            ctx.fillStyle = g;
            ctx.fillRect (x, mid - hgt, 2, 2 * hgt);
        }
        ctx.globalAlpha = 1;

        // the instrument plane: a thin bright band, the point of transformation
        const plane = ctx.createLinearGradient (0, mid - amp * 2, 0, mid + amp * 2);
        plane.addColorStop (0, "transparent"); plane.addColorStop (0.5, colours.c); plane.addColorStop (1, "transparent");
        ctx.fillStyle = plane; ctx.globalAlpha = 0.35;
        ctx.fillRect (cx - 0.75, mid - amp * 2, 1.5, amp * 4);
        const halo = ctx.createRadialGradient (cx, mid, 0, cx, mid, amp * 1.6);
        halo.addColorStop (0, colours.b); halo.addColorStop (1, "transparent");
        ctx.globalAlpha = 0.13 * env; ctx.fillStyle = halo;
        ctx.fillRect (cx - amp * 1.6, mid - amp * 1.6, amp * 3.2, amp * 3.2);
        ctx.globalAlpha = 1;

        // the signal lines: raw on the left, shaped on the right, a smooth hand-over through the plane
        const lines = w < 700 ? 5 : 8, step = w < 700 ? 6 : 4;
        for (let k = 0; k < lines; k++) {
            const depth = k / (lines - 1); // back lines thinner and fainter: depth without 3-D
            const grad = ctx.createLinearGradient (0, 0, w, 0);
            grad.addColorStop (0, "rgba(220,224,232,0.0)"); grad.addColorStop (Math.max (0, cx / w - 0.25), "rgba(220,224,232,0.55)");
            grad.addColorStop (cx / w, colours.c); grad.addColorStop (Math.min (1, cx / w + 0.18), colours.a); grad.addColorStop (1, colours.b);
            ctx.strokeStyle = grad;
            ctx.lineWidth = 1 + (1 - depth) * 1.6;
            ctx.globalAlpha = 0.18 + (1 - depth) * 0.62;
            ctx.beginPath();
            for (let x = 0; x <= w; x += step) {
                const m = smooth (cx - 80, cx + 120, x);
                const y = (1 - m) * raw (x, t, k * 0.9) * amp * 0.75 + m * shaped (x, t, k * 0.42) * amp * env * (1 - depth * 0.35);
                const spread = (k - (lines - 1) / 2) * (6 + 10 * m); // the shaped harmonics fan out into layers
                x === 0 ? ctx.moveTo (x, mid + y + spread) : ctx.lineTo (x, mid + y + spread);
            }
            ctx.stroke();
        }
        ctx.globalAlpha = 1;
    }

    resize();
    addEventListener ("resize", () => { resize(); if (reduceMotion) draw (2.2); });
    if (reduceMotion) { draw (2.2); return; } // one still frame
    new IntersectionObserver (([e]) => { running = e.isIntersecting; if (running) requestAnimationFrame (loop); }).observe (canvas);
    document.addEventListener ("visibilitychange", () => { if (! document.hidden && running) requestAnimationFrame (loop); });
    const start = performance.now();
    function loop (now) {
        if (! running || document.hidden) return;
        draw ((now - start) / 1000);
        requestAnimationFrame (loop);
    }
    requestAnimationFrame (loop);
}) ();

// ---- AUDIO: original vs processed. Only real recordings; a clearly marked placeholder otherwise. ----
(function player() {
    const root = document.querySelector (".player");
    if (! root) return;
    const audios = Object.fromEntries ([...root.querySelectorAll ("audio")].map ((a) => [a.dataset.version, a]));
    const playBtn = root.querySelector (".player__play"), playIcon = playBtn.querySelector ("span"), seek = root.querySelector (".player__seek"), time = root.querySelector (".player__time");
    const radios = [...root.querySelectorAll (".ab [data-version]")];
    const scope = root.querySelector (".player__scope"), sctx = scope.getContext ("2d");
    let version = "original", analyser = null, data = null, raf = 0;
    const fmt = (s) => (isFinite (s) ? `${Math.floor (s / 60)}:${String (Math.floor (s % 60)).padStart (2, "0")}` : "0:00");
    const active = () => audios[version];
    const showPlaying = (on) => { playIcon.textContent = on ? "❚❚" : "▶"; playBtn.setAttribute ("aria-label", on ? "Pause" : "Play"); };

    function drawScope (live) { // the real waveform of what is playing (Web Audio analyser); a flat line when nothing plays
        const k = Math.min (2, devicePixelRatio || 1), w = (scope.width = scope.clientWidth * k), h = (scope.height = scope.clientHeight * k);
        sctx.clearRect (0, 0, w, h);
        const g = sctx.createLinearGradient (0, 0, w, 0);
        g.addColorStop (0, css ("--secondary")); g.addColorStop (1, css ("--primary"));
        sctx.strokeStyle = g; sctx.lineWidth = 2 * k;
        sctx.beginPath();
        if (live && analyser) {
            analyser.getByteTimeDomainData (data);
            for (let i = 0; i < data.length; i++) { const x = (i / (data.length - 1)) * w, y = (data[i] / 255) * h; i ? sctx.lineTo (x, y) : sctx.moveTo (x, y); }
        } else { sctx.moveTo (0, h / 2); sctx.lineTo (w, h / 2); }
        sctx.stroke();
    }
    function loop() { drawScope (true); raf = requestAnimationFrame (loop); }
    function unavailable (message) {
        root.dataset.state = "unavailable";
        if (message) root.querySelector (".player__placeholder .mono").textContent = message;
        [playBtn, seek, ...radios].forEach ((n) => (n.disabled = true));
        sctx.clearRect (0, 0, scope.width, scope.height); // blank behind the note: nothing is implied
    }
    profileListeners.push (() => { if (root.dataset.state !== "unavailable") drawScope (false); });
    if (! config.demo?.available) return unavailable();

    let ready = 0;
    for (const [v, a] of Object.entries (audios)) {
        a.addEventListener ("canplaythrough", () => { if (++ready === 2) enable(); }, { once: true });
        a.addEventListener ("error", () => unavailable ("The demo recordings couldn't be loaded"), { once: true });
        a.addEventListener ("timeupdate", () => { if (a === active()) { seek.value = String (Math.round ((a.currentTime / (a.duration || 1)) * 1000)); time.textContent = `${fmt (a.currentTime)} / ${fmt (a.duration)}`; } });
        a.addEventListener ("ended", () => { showPlaying (false); cancelAnimationFrame (raf); drawScope (false); });
    }
    if (config.demo.credit) root.querySelector (".player__credit").textContent = config.demo.credit;
    root.dataset.state = "loading";
    drawScope (false);
    // each recording is fetched once and played from memory: always seekable (a switch lands on the same moment on any host) and instant
    Promise.all (Object.keys (audios).map ((v) => fetch (config.demo[v]).then ((r) => { if (! r.ok) throw new Error (r.status); return r.blob(); })))
        .then ((blobs) => Object.values (audios).forEach ((a, i) => { a.preload = "auto"; a.src = URL.createObjectURL (blobs[i]); }))
        .catch (() => unavailable ("The demo recordings couldn't be loaded"));

    function enable() {
        root.dataset.state = "ready";
        [playBtn, seek, ...radios].forEach ((n) => (n.disabled = false));
        time.textContent = `0:00 / ${fmt (active().duration)}`;
    }
    function ensureAnalyser() { // created on the first press (browsers only allow audio after a user gesture)
        if (analyser) return;
        try {
            const ac = new (window.AudioContext || window.webkitAudioContext)();
            analyser = ac.createAnalyser(); analyser.fftSize = 2048; data = new Uint8Array (analyser.fftSize);
            for (const a of Object.values (audios)) ac.createMediaElementSource (a).connect (analyser);
            analyser.connect (ac.destination);
        } catch { analyser = null; } // the audio still plays without the scope
    }
    playBtn.addEventListener ("click", async () => {
        ensureAnalyser();
        const a = active();
        if (a.paused) {
            try { await a.play(); } catch { return unavailable ("Playback was blocked by the browser"); }
            showPlaying (true);
            loop();
        } else {
            a.pause(); showPlaying (false);
            cancelAnimationFrame (raf); drawScope (false);
        }
    });
    radios.forEach ((r) => r.addEventListener ("click", () => { // switch at the same position: a true A/B
        if (r.dataset.version === version) return;
        const from = active(), playing = ! from.paused;
        version = r.dataset.version;
        const to = active();
        to.currentTime = from.currentTime;
        from.pause();
        if (playing) to.play().catch (() => {});
        radios.forEach ((x) => x.setAttribute ("aria-checked", String (x === r)));
    }));
    seek.addEventListener ("input", () => { const a = active(); if (a.duration) a.currentTime = (Number (seek.value) / 1000) * a.duration; });
}) ();

// ---- FACTS: support and privacy links, only when configured ----
if (config.contactEmail) {
    const dd = document.querySelector ("[data-contact]");
    if (dd) dd.replaceChildren (el ("a", { href: "mailto:" + config.contactEmail }, config.contactEmail));
}
if (/^https?:\/\//.test (config.privacyPolicyUrl || "")) {
    document.querySelectorAll (".privacy-link").forEach ((a) => Object.assign (a, { href: config.privacyPolicyUrl, target: "_blank", rel: "noopener" }));
    const p = document.querySelector ("[data-privacy]");
    if (p) p.replaceChildren ("We use your application only to run the GlitchKid beta and contact you about it. ",
                              el ("a", { href: config.privacyPolicyUrl, target: "_blank", rel: "noopener" }, "Read the privacy policy"), ".");
}

// ---- BETA FORM: validated here, sent only to the configured endpoint, and "received" only when that endpoint says so ----
(function betaForm() {
    const form = document.querySelector (".form");
    if (! form) return;
    const status = form.querySelector (".form__status"), submit = form.querySelector ("[type=submit]");
    const say = (text, kind) => { status.textContent = text; status.dataset.kind = kind || ""; };
    const rules = {
        name: (v) => (v.trim().length >= 2 ? "" : "Please enter your name."),
        email: (v) => (/^[^\s@]+@[^\s@]+\.[^\s@]{2,}$/.test (v.trim()) ? "" : "Please enter a valid email address."),
        daw: (v) => (v ? "" : "Please choose your DAW."),
        os: (v) => (v ? "" : "Please choose your operating system."),
        genre: (v) => (v.trim() ? "" : "Please tell us your genre."),
        experience: (v) => (v ? "" : "Please choose your experience."),
        link: (v) => { if (! v.trim()) return ""; try { const u = new URL (v.trim()); return /^https?:$/.test (u.protocol) ? "" : "Please use a web link (https://…)."; } catch { return "Please use a full link, starting with https://"; } },
        consent: (_, input) => (input.checked ? "" : "Please agree so we can contact you about the beta."),
    };
    const errorFor = { name: "e-name", email: "e-email", daw: "e-daw", os: "e-os", genre: "e-genre", experience: "e-exp", link: "e-link", consent: "e-consent" };
    function check (name) {
        const input = form.elements[name], msg = rules[name] (input.value, input);
        input.setAttribute ("aria-invalid", String (!! msg));
        input.setAttribute ("aria-describedby", errorFor[name]);
        document.getElementById (errorFor[name]).textContent = msg;
        return ! msg;
    }
    Object.keys (rules).forEach ((n) => form.elements[n].addEventListener ("blur", () => form.elements[n].value && check (n)));
    form.elements.os.addEventListener ("change", (e) => { document.getElementById ("h-os").hidden = ! e.target.value || e.target.value.startsWith ("macOS on Apple"); });

    const previous = store.get ("gk-beta-applied");
    if (previous) say (`You already applied from this browser on ${new Date (previous).toLocaleDateString()}. Sending again creates a second application.`);

    let sending = false;
    form.addEventListener ("submit", async (e) => {
        e.preventDefault();
        if (sending) return; // no double submissions
        const bad = Object.keys (rules).filter ((n) => ! check (n));
        if (bad.length) { form.elements[bad[0]].focus(); say ("Please check the highlighted fields.", "error"); return; }
        if (form.elements.company.value) return; // a bot filled the hidden field: send nothing, claim nothing
        if (! config.betaFormEndpoint) { say ("Applications aren't open on this site yet. Please check back soon.", "error"); return; }
        sending = true; submit.disabled = true; submit.textContent = "Sending…"; say ("");
        try {
            const body = new FormData (form);
            body.delete ("company");
            body.set ("consent", "yes");
            const res = await fetch (config.betaFormEndpoint, { method: "POST", body, headers: { Accept: "application/json" } });
            if (! res.ok) throw new Error (String (res.status));
            store.set ("gk-beta-applied", new Date().toISOString());
            const done = el ("div", { className: "form__done", tabIndex: -1 });
            done.append (el ("h3", { className: "grad-text" }, "Application received"),
                         el ("p", {}, "Thank you. We read every application. If there's a place for you in this beta round, we'll email you the next steps."));
            form.replaceChildren (done);
            done.focus();
        } catch {
            sending = false; submit.disabled = false; submit.textContent = "Apply for beta";
            say ("Your application wasn't sent. Please check your connection and try again.", "error");
        }
    });
}) ();
