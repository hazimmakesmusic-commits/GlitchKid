// GLITCHKID website behaviour (ES module, no dependencies). Every feature degrades: no JS = a readable page; reduced motion = still visuals.
import { mountHero } from "./sections/hero.js";
import { mountProduct } from "./sections/product.js";
import { reducedMotion } from "./motion/core.js";

const config = JSON.parse (document.getElementById ("site-config")?.textContent || "{}");
const store = { // browser storage can be unavailable (private windows, blocked cookies): never let it break the page
    get: (k) => { try { return localStorage.getItem (k); } catch { return null; } },
    set: (k, v) => { try { localStorage.setItem (k, v); } catch { /* not kept */ } },
};
const el = (tag, props = {}, text = "") => Object.assign (document.createElement (tag), props, text ? { textContent: text } : {});
document.querySelectorAll ("[data-year]").forEach ((n) => (n.textContent = new Date().getFullYear()));

// ---- navigation ----
const toggle = document.querySelector (".nav__toggle"), links = document.getElementById ("nav-links");
const closeMenu = () => { toggle.setAttribute ("aria-expanded", "false"); links.classList.remove ("is-open"); };
toggle?.addEventListener ("click", () => {
    const open = toggle.getAttribute ("aria-expanded") !== "true";
    toggle.setAttribute ("aria-expanded", String (open));
    links.classList.toggle ("is-open", open);
});
links?.addEventListener ("click", (e) => { if (e.target.closest ("a")) closeMenu(); });
addEventListener ("keydown", (e) => { if (e.key === "Escape" && links.classList.contains ("is-open")) { closeMenu(); toggle.focus(); } });

// ---- the signature moments ----
mountHero (document.querySelector (".hero"));
mountProduct (document.getElementById ("instrument"));

// ---- section reveals: section headings, once, when they come into view; skipped with reduced motion ----
if (! reducedMotion() && "IntersectionObserver" in window) {
    document.documentElement.classList.add ("motion");
    const targets = document.querySelectorAll (".section .display, .section .section__lede, .home-index, .advanced__copy");
    const io = new IntersectionObserver ((entries) => entries.forEach ((e) => {
        if (e.isIntersecting) { e.target.classList.add ("is-in"); io.unobserve (e.target); }
    }), { rootMargin: "0px 0px -10% 0px", threshold: 0.2 });
    targets.forEach ((n) => { n.classList.add ("js-reveal"); io.observe (n); });
}

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
        g.addColorStop (0, "#08776f"); g.addColorStop (0.48, "#00b894"); g.addColorStop (1, "#00dda5");
        sctx.strokeStyle = g; sctx.lineWidth = 1.5 * k;
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
    if (! config.demo?.available) return unavailable();

    let ready = 0;
    for (const a of Object.values (audios)) {
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
    if (p) p.replaceChildren ("We use them only to run the GlitchKid beta waitlist and contact you about the beta. ",
                              el ("a", { href: config.privacyPolicyUrl, target: "_blank", rel: "noopener" }, "Read the privacy policy"), ".");
}

// ---- WAITLIST: only the email is required. Validated here, sent only to the configured endpoint, and "on the waitlist" only when that endpoint
// says so (a 2xx). The backend (Cloudflare Pages Function + D1) is a later stage: with no endpoint the form says the waitlist isn't open yet. ----
(function waitlist() {
    const form = document.querySelector (".form");
    if (! form) return;
    const status = form.querySelector (".form__status"), submit = form.querySelector ("[type=submit]");
    const say = (text, kind) => { status.textContent = text; status.dataset.kind = kind || ""; };
    const rules = {
        email: (v) => (/^[^\s@]+@[^\s@]+\.[^\s@]{2,}$/.test (v.trim()) ? "" : "Please enter a valid email address."),
        name: () => "",
        os: () => "",
        daw: () => "",
        genre: () => "",
        link: (v) => { if (! v.trim()) return ""; try { const u = new URL (v.trim()); return /^https?:$/.test (u.protocol) ? "" : "Please use a web link (https://…)."; } catch { return "Please use a full link, starting with https://"; } },
        consent: (_, input) => (input.checked ? "" : "Please tick this so we can email you about the beta."),
    };
    const errorFor = { email: "e-email", name: "e-name", os: "e-os", daw: "e-daw", genre: "e-genre", link: "e-link", consent: "e-consent" };
    function check (name) {
        const input = form.elements[name], msg = rules[name] (input.value, input);
        input.setAttribute ("aria-invalid", String (!! msg));
        input.setAttribute ("aria-describedby", errorFor[name]);
        document.getElementById (errorFor[name]).textContent = msg;
        return ! msg;
    }
    Object.keys (rules).forEach ((n) => form.elements[n].addEventListener ("blur", () => form.elements[n].value && check (n)));
    form.elements.os.addEventListener ("change", (e) => { document.getElementById ("h-os").hidden = ! e.target.value || e.target.value === "macOS on Apple Silicon"; });

    const previous = store.get ("gk-waitlist-joined");
    if (previous) say (`You already joined from this browser on ${new Date (previous).toLocaleDateString()}.`);

    let sending = false;
    form.addEventListener ("submit", async (e) => {
        e.preventDefault();
        if (sending) return; // no double submissions
        const bad = Object.keys (rules).filter ((n) => ! check (n));
        if (bad.length) {
            if (bad.some ((n) => ["name", "os", "daw", "genre", "link"].includes (n))) form.querySelector (".form__more").open = true;
            form.elements[bad[0]].focus();
            say ("Please check the highlighted fields.", "error");
            return;
        }
        if (form.elements.company.value) return; // a bot filled the hidden field: send nothing, claim nothing
        if (! config.betaFormEndpoint) { say ("The waitlist isn't open on this site yet. Please check back soon.", "error"); return; }
        sending = true; submit.disabled = true; submit.textContent = "Sending…"; say ("");
        try {
            const body = new FormData (form);
            body.delete ("company");
            body.set ("consent", "yes");
            for (const [k, v] of [...body.entries()]) if (typeof v === "string" && ! v.trim()) body.delete (k); // optional fields left empty are not sent
            const res = await fetch (config.betaFormEndpoint, { method: "POST", body, headers: { Accept: "application/json" } });
            if (! res.ok) throw new Error (String (res.status));
            store.set ("gk-waitlist-joined", new Date().toISOString());
            const done = el ("div", { className: "form__done", tabIndex: -1 });
            done.append (el ("h3", {}, "You're on the waitlist"),
                         el ("p", {}, "Thank you. The beta is free; we'll email you when places open. Joining doesn't guarantee a place."));
            form.replaceChildren (done);
            done.focus();
        } catch {
            sending = false; submit.disabled = false; submit.textContent = "Join the waitlist";
            say ("You weren't added. Please check your connection and try again.", "error");
        }
    });
}) ();
