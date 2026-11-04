// QA for "The Session That Talks Back": drives the prototype in headless Chromium with real clicks, key presses and mouse holds,
// asserts on its state after every step, checks the layout at three window sizes, and (with --shots) saves screenshots.
// Run from the prototype folder:  node qa/verify.js            checks only
//                                 node qa/verify.js --shots    checks + screenshots into ./screenshots (1x) and ./screenshots/hero (2x)
// Needs only Playwright (pre-installed in the build container; elsewhere: npm i -D playwright && npx playwright install chromium).
'use strict';
const http = require('http');
const fs = require('fs');
const path = require('path');
const { chromium } = require('playwright');

const ROOT = path.resolve(__dirname, '..');
const SHOTS = process.argv.includes('--shots');
const SHOT_DIR = path.join(ROOT, 'screenshots');
const TYPES = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.png': 'image/png', '.woff2': 'font/woff2', '.txt': 'text/plain' };

let passed = 0, failed = 0;
const failures = [], notes = [];
function check(ok, what, detail) {
  if (ok) { passed++; console.log('  PASS  ' + what); }
  else { failed++; failures.push(what + (detail ? ' — ' + detail : '')); console.log('  FAIL  ' + what + (detail ? ' — ' + detail : '')); }
}
const section = (t) => console.log('\n## ' + t);

function serve() {
  const srv = http.createServer((req, res) => {
    const rel = decodeURIComponent(new URL(req.url, 'http://x').pathname).replace(/^\/+/, '') || 'index.html';
    const file = path.resolve(ROOT, rel);
    if (!file.startsWith(ROOT + path.sep) || !fs.existsSync(file) || fs.statSync(file).isDirectory()) { res.writeHead(404); res.end(); return; }
    res.writeHead(200, { 'content-type': TYPES[path.extname(file)] || 'application/octet-stream' });
    fs.createReadStream(file).pipe(res);
  });
  return new Promise((r) => srv.listen(0, '127.0.0.1', () => r(srv)));
}

// WCAG 2.x contrast between two hex colours
function contrast(a, b) {
  const lum = (h) => { const c = h.match(/\w\w/g).map((x) => parseInt(x, 16) / 255).map((v) => (v <= 0.03928 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4))); return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]; };
  const [x, y] = [lum(a), lum(b)].sort((p, q) => q - p);
  return (x + 0.05) / (y + 0.05);
}

(async () => {
  const srv = await serve();
  const base = 'http://127.0.0.1:' + srv.address().port + '/index.html';
  const browser = await chromium.launch();
  const ctx = await browser.newContext({ viewport: { width: 1460, height: 1000 }, deviceScaleFactor: 1 });
  const page = await ctx.newPage();
  const consoleErrors = [];
  page.on('console', (m) => { if (m.type() === 'error') consoleErrors.push(m.text()); });
  page.on('pageerror', (e) => consoleErrors.push(e.message));
  if (SHOTS) fs.mkdirSync(path.join(SHOT_DIR, 'hero'), { recursive: true });

  const S = () => page.evaluate(() => JSON.parse(JSON.stringify(GK.S)));
  const sound = () => page.evaluate(() => JSON.parse(JSON.stringify(GK.S.sound)));
  const tid = (t) => page.locator('[data-testid="' + t + '"]');
  const settle = () => page.evaluate(() => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r))));   // the prototype redraws on the next frame
  const clearToasts = () => page.evaluate(() => { const t = document.querySelector('.toast-layer'); if (t) t.replaceChildren(); });
  const click = async (t) => { await clearToasts(); await tid(t).first().click(); await settle(); };
  const sim = async (k) => { await page.locator('[data-sim="' + k + '"]').click(); await settle(); };
  const wait = (ms) => page.waitForTimeout(ms);
  const visible = async (t) => (await tid(t).count()) > 0 && tid(t).first().isVisible();
  const text = async (t) => ((await tid(t).count()) ? (await tid(t).first().textContent()) || '' : '');
  const until = async (fn, what, ms = 8000) => { const t0 = Date.now(); for (;;) { if (await page.evaluate(fn)) return true; if (Date.now() - t0 > ms) { check(false, 'timed out waiting: ' + what); return false; } await wait(25); } };
  const same = (a, b) => !!a && !!b && Object.keys(a).every((k) => Math.abs(a[k] - b[k]) < 1e-6);
  const diffIds = (a, b) => Object.keys(a).filter((k) => Math.abs(a[k] - b[k]) >= 1e-6).sort();
  const note = async () => ({ id: (await tid('kidnote').count()) ? await tid('kidnote').getAttribute('data-note') : '', text: await text('kidnote'), cls: (await tid('kidnote').count()) ? await tid('kidnote').getAttribute('class') : '' });
  const rows = () => page.evaluate(() => (GK.S.review ? GK.S.review.rows.map((r) => ({ id: r.id, to: r.to, sel: r.sel, manual: r.manual, sim: r.sim })) : []));
  const fast = () => page.evaluate(() => { GK.pace.k = 0.05; });
  const press = async (dataKey, key, n) => { await page.locator('[data-key="' + dataKey + '"]').focus(); for (let i = 0; i < n; i++) await page.keyboard.press(key); await wait(800); await settle(); };   // a knob gesture closes 700 ms after the last key
  async function shot(name, sizes = ['standard']) {      // sizes are set without clicking, so a held mouse button or the focus is never disturbed
    if (!SHOTS) return;
    await clearToasts();
    for (const sz of sizes) {
      await page.evaluate((v) => GK.setSize(v), sz); await settle(); await wait(120);
      await page.locator('#frameWrap').screenshot({ path: path.join(SHOT_DIR, name + '_' + sz + '.png') });
    }
    await page.evaluate(() => GK.setSize('standard')); await settle();
  }
  async function kidType(words) {
    await page.evaluate(() => { GK.phaseLog.length = 0; });
    const f = tid('kid-input'); await f.fill(words); await f.press('Enter');
    await until(() => !GK.S.kid.busy && GK.phaseLog.length > 0 && ['done', 'error', 'unavailable'].includes(GK.S.kid.phase), 'KID to answer “' + words + '”');
    await settle();
  }
  async function holdMic(ms) {
    const b = await tid('mic').boundingBox();
    await page.mouse.move(b.x + b.width / 2, b.y + b.height / 2);
    await page.mouse.down(); await wait(ms); await page.mouse.up(); await settle();
  }
  const kidCards = async (state = 'new') => (await S()).desk.filter((c) => c.kind === 'kid' && (!state || c.state === state));

  // Layout: text cut off or clipped, overlapping or clipped controls, text under 12 px and stray null/undefined/NaN, among what is really visible.
  // Card names, descriptions and intents are clamped on purpose ([data-clamp]): the full text stays in the DOM and in "Look inside".
  async function layout(name) {
    const issues = await page.evaluate(() => {
      const fr = document.getElementById('frame'), out = [];
      const tl = fr.querySelector('.toast-layer'); if (tl) tl.replaceChildren();
      const vis = (e) => { const r = e.getBoundingClientRect(), cs = getComputedStyle(e); return r.width > 0 && r.height > 0 && cs.visibility !== 'hidden' && cs.display !== 'none'; };
      const k = fr.getBoundingClientRect().width / 1120;
      const scope = fr.querySelector('#gk-onb') || fr.querySelector('#gk-modal .modal-layer') || fr;
      const label = (e) => (e.dataset.testid || e.dataset.key || e.getAttribute('aria-label') || e.textContent || e.tagName).trim().slice(0, 48);
      const seen = (e) => { const r = e.getBoundingClientRect(); let L = r.left, T = r.top, R = r.right, B = r.bottom, a = e.parentElement;   // the part of e not clipped by its ancestors
        while (a && a !== fr) { const cs = getComputedStyle(a); if (cs.overflowX !== 'visible' || cs.overflowY !== 'visible') { const ar = a.getBoundingClientRect(); L = Math.max(L, ar.left); T = Math.max(T, ar.top); R = Math.min(R, ar.right); B = Math.min(B, ar.bottom); } a = a.parentElement; }
        return { left: L, top: T, right: R, bottom: B, width: R - L, height: B - T }; };
      const onTop = (e) => { const r = seen(e); if (r.width <= 0 || r.height <= 0) return false; const h = document.elementFromPoint(r.left + r.width / 2, r.top + r.height / 2); return h && (h === e || e.contains(h) || h.contains(e)); };
      const clippedBy = (e, stop) => {      // hidden by an ancestor that clips on an axis where nothing between them scrolls
        const r = e.getBoundingClientRect(); let a = e.parentElement, sx = false, sy = false;
        while (a && a !== stop) { const cs = getComputedStyle(a), ar = a.getBoundingClientRect();
          if (!sx && /(hidden|clip)/.test(cs.overflowX) && (r.left < ar.left - 1 || r.right > ar.right + 1)) return true;
          if (!sy && /(hidden|clip)/.test(cs.overflowY) && (r.top < ar.top - 1 || r.bottom > ar.bottom + 1)) return true;
          if (/(auto|scroll)/.test(cs.overflowX)) sx = true; if (/(auto|scroll)/.test(cs.overflowY)) sy = true;
          a = a.parentElement; }
        return false; };
      const inScrolledOut = (e) => { let a = e.parentElement; const r = e.getBoundingClientRect(); while (a && a !== fr) { const cs = getComputedStyle(a); if (/(auto|scroll)/.test(cs.overflowY + cs.overflowX)) { const ar = a.getBoundingClientRect(); if (r.bottom < ar.top + 1 || r.top > ar.bottom - 1 || r.right < ar.left + 1 || r.left > ar.right - 1) return true; } a = a.parentElement; } return false; };
      for (const e of scope.querySelectorAll('*')) {
        if (!vis(e) || inScrolledOut(e)) continue;
        const own = [...e.childNodes].some((n) => n.nodeType === 3 && n.textContent.trim());
        if (!own || e.closest('.hl-text')) continue;          // the help line truncates by design; the full text is the control's description
        if (/\b(null|undefined|NaN)\b/.test([...e.childNodes].filter((n) => n.nodeType === 3).map((n) => n.textContent).join(' '))) out.push('stray null/undefined/NaN: ' + label(e));
        const cs = getComputedStyle(e), px = parseFloat(cs.fontSize);
        if (px < 11.95 && !e.closest('svg')) out.push('text below 12 px (' + px + ' px): ' + label(e));
        if (!e.closest('[data-clamp]') && e.tagName !== 'OPTION' && e.tagName !== 'SELECT' && (e.scrollWidth > e.clientWidth + 1 || e.scrollHeight > e.clientHeight + 2) && (/(hidden|clip)/.test(cs.overflowX + cs.overflowY) || cs.textOverflow === 'ellipsis')) out.push('text cut off: ' + label(e));
        if (clippedBy(e, fr.parentElement)) out.push('text clipped by its container: ' + label(e));
      }
      const ctl = [...scope.querySelectorAll('button, input, select, [role="slider"], [role="switch"], [role="checkbox"]')].filter((e) => vis(e) && !inScrolledOut(e) && onTop(e));
      for (const e of ctl) if (clippedBy(e, fr)) out.push('control clipped: ' + label(e));
      for (let i = 0; i < ctl.length; i++) for (let j = i + 1; j < ctl.length; j++) {
        const a = ctl[i], b = ctl[j]; if (a.contains(b) || b.contains(a)) continue;
        if (!!a.closest('#gk-drawer') !== !!b.closest('#gk-drawer')) continue;   // the review drawer is an overlay on purpose
        const p = seen(a), q = seen(b);
        const ox = Math.min(p.right, q.right) - Math.max(p.left, q.left), oy = Math.min(p.bottom, q.bottom) - Math.max(p.top, q.top);
        if (ox > 2 * k && oy > 2 * k) out.push('controls overlap: ' + label(a) + ' × ' + label(b));
      }
      return [...new Set(out)];
    });
    check(issues.length === 0, 'layout clean: ' + name, issues.join('; '));
  }

  // ------------------------------------------------------------ 0
  section('0. Facts copied from the GlitchKid source, and the designed recipes stay inside them');
  await page.goto(base);
  await page.evaluate(() => localStorage.clear());
  await page.goto(base);
  await wait(300);
  const facts = await page.evaluate(() => {
    const inRange = (id, v) => { const p = GK.PM[id]; if (!p) return false; if (p.type === 'float') return v >= p.min - 1e-9 && v <= p.max + 1e-9; if (p.type === 'bool') return v === 0 || v === 1; return Number.isInteger(v) && v >= 0 && v < p.choices.length; };
    const fd = GK.factoryDefault();
    const ideaBad = [];
    for (const r of GK.IDEAS) for (const from of [fd, GK.profileSound(3, fd)]) { const set = r.make(from); for (const [k, v] of Object.entries(set)) if (!inRange(k, v)) ideaBad.push(r.id + '.' + k + '=' + v); if (Object.keys(set).some((k) => GK.KID_NEVER.includes(k))) ideaBad.push(r.id + ' touches gain/mix/limiter/key'); }
    const worldBad = []; for (const w of GK.WORLDS) for (const [k, v] of Object.entries(w.o)) { if (!inRange(k, v)) worldBad.push(w.id + '.' + k); if (GK.USER_OWNED.includes(k)) worldBad.push(w.id + ' sets user-owned ' + k); }
    const from = { ...fd, tune_on: 1, tune_key: 9, gain: -3, clean_on: 1, lim_on: 1, air_on: 1 };
    const worldOwned = GK.WORLDS.every((w) => { const t = GK.worldSound(w, from); return GK.USER_OWNED.every((k) => t[k] === from[k]); });
    return { n: GK.PARAMS.length, chain: GK.CHAIN.map((c) => c[0]).join(','), prof: GK.PROFILES.map((p) => p.name), owned: GK.USER_OWNED.length, never: GK.KID_NEVER.join(','),
      ideas: GK.IDEAS.map((r) => r.name), worlds: GK.WORLDS.map((w) => w.name), ideaBad, worldBad, worldOwned,
      defaults: Object.fromEntries(['comp_ratio', 'echo_mix', 'dim_mix', 'tune_speed', 'lim_ceiling', 'echo_div'].map((k) => [k, fd[k]])) };
  });
  check(facts.n === 68, '68 sound parameters (69 with Bypass), as in createLayout', String(facts.n));
  check(facts.chain === '1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17', 'the chain strip shows the 17 stages in processing order');
  check(facts.prof.length === 8 && facts.prof.includes('SMOOTH R&B') && facts.prof.includes('DARK & ATMOSPHERIC'), 'the 8 factory profiles are present');
  check(facts.owned === 22, 'a profile load keeps the 22 user-owned parameters');
  check(facts.never === 'gain,mix,lim_on,lim_ceiling,lim_drive,lim_release,tune_key,tune_scale', 'KID’s allowlist: never gain, mix, the limiter, or your key and scale');
  check(facts.defaults.comp_ratio === 1 && facts.defaults.echo_mix === 30 && facts.defaults.dim_mix === 100 && facts.defaults.tune_speed === 20 && facts.defaults.lim_ceiling === -1 && facts.defaults.echo_div === 4, 'spot-checked defaults match the source', JSON.stringify(facts.defaults));
  check(facts.ideas.join('|') === 'Ghost Double|Wide Dream|Broken Radio|Close & Warm|Afterimage', 'the five idea cards from the brief exist');
  check(facts.ideaBad.length === 0, 'every idea recipe sets only real parameters inside their ranges, never gain, mix, limiter or key', facts.ideaBad.join(', '));
  check(facts.worlds.join('|') === 'Dreamscape|Neon City|Radio Static|Underwater|Chrome Room|Afterhours', 'the six worlds from the brief exist');
  check(facts.worldBad.length === 0 && facts.worldOwned, 'every draft world is a valid parameter state and, like a factory profile, keeps the 22 user-owned parameters', facts.worldBad.join(', '));

  // ------------------------------------------------------------ 1
  section('1. First launch: short, skippable, ends inside the session');
  check(await visible('onb-0'), 'first launch opens a 3-step intro (step 1: name the session)');
  check((await tid('onb-name').inputValue()) === 'late night session', 'the session already has a name you can keep or change');
  await tid('onb-name').fill('midnight hook'); await settle();
  await layout('intro 1 · name');
  await shot('01_first-launch-name');
  await click('onb-next');
  check(await visible('onb-1'), 'step 2: your vocal comes from FL Studio');
  await sim('signal-none');
  check((await text('onb-signal')).includes('Waiting for sound'), 'with no input the intro honestly waits (“Waiting for sound…”)');
  await click('onb-sim');
  check((await text('onb-signal')).includes('Vocal coming in'), 'with sound coming in it says so');
  await click('onb-dry');
  check((await S()).bypass === true, 'DRY in the intro is the real compare (Bypass on)');
  await page.locator('[data-key="onb-proc"]').click(); await settle();
  check((await S()).bypass === false, 'PROCESSED switches it back');
  await layout('intro 2 · signal');
  await shot('02_first-launch-signal');
  await click('onb-next');
  check(await visible('onb-2'), 'step 3: KID leaves notes; on, or only when asked');
  await layout('intro 3 · KID');
  await click('onb-next');
  let st = await S();
  check(!st.onb.open && st.onbDone, 'finishing the intro lands in the session');
  check(st.session.name === 'midnight hook' && (await text('tape')).includes('midnight hook'), 'the session name is on the tape label');

  // ------------------------------------------------------------ 2
  section('2. The session: play (in FL Studio) → discover');
  check((await text('transport')).includes('FL Studio playing · 92 BPM'), 'the take shows FL Studio’s transport and tempo (playback belongs to FL Studio)');
  check((await text('take-source')).includes('LIVE'), 'the take is the live input from FL Studio');
  check((await page.locator('.wave-tag').textContent()).includes('SIMULATED TAKE'), 'the waveform is labelled SIMULATED TAKE');
  check((await S()).desk.map((c) => c.name).join('|') === 'Ghost Double|Wide Dream|Broken Radio|Close & Warm|Afterimage', 'five idea cards wait on the desk');
  let n = await note();
  check(n.id === 'fresh' && (await visible('note-roll')), 'KID leaves one small, dismissible note: a fresh session');
  await layout('session · fresh');
  await shot('03_session-fresh', ['compact', 'standard', 'large']);
  await sim('transport-stopped'); await sim('motion-false'); await wait(150);
  const pic = () => page.evaluate(() => document.getElementById('gk-wave').toDataURL());
  const picA = await pic();
  await click('cmp-dry'); await wait(60);
  const picDry = await pic();
  await click('cmp-proc'); await wait(60);
  check(picDry !== picA && (await pic()) === picA, 'the waveform follows the real compare: DRY draws differently, PROCESSED draws the same as before');
  check((await text('wave-msg')).includes('Paused'), 'with FL Studio stopped the take says “Paused”');

  // ------------------------------------------------------------ 3
  section('3. Audition an idea: hear it, nothing changes');
  const s0 = await sound();
  await click('hear-wide');
  st = await S();
  check(st.audition && st.audition.kind === 'card' && st.audition.id === 'wide', 'Hear auditions Wide Dream');
  check(same(st.sound, s0), 'auditioning changes nothing: the sound is exactly as it was');
  check((await text('hear-chip')).includes('WIDE DREAM (NOT KEPT)'), 'the take says what you hear: “HEARING · WIDE DREAM (NOT KEPT)”');
  check((await tid('chain-14').getAttribute('data-on')) === '1' && st.sound.dim_on === 0, 'the chain shows what you hear (Dimension lit; its real value is still off)');
  check(await page.locator('[data-testid="card-wide"] .sticker.hearing').isVisible(), 'the card wears a HEARING sticker');
  check((await pic()) !== picA, 'the waveform draws what you hear (wider while auditioning)');
  await sim('transport-playing'); await sim('motion-true');
  await shot('04_audition-idea');
  await click('hear-wide');
  check((await S()).audition === null, 'pressing it again stops the audition');

  // ------------------------------------------------------------ 4
  section('4. Look inside, untick, hear the ticked changes, keep part of it');
  await click('look-radio');
  check(await visible('review'), 'Look inside opens “What changes”');
  let rv = await rows();
  const radioIds = await page.evaluate(() => GK.cardRows(GK.S.desk.find((c) => c.id === 'radio')).map((r) => r.id));
  check(rv.length === radioIds.length && rv.length >= 8 && rv.every((r) => r.sel), 'every change is listed and ticked (' + rv.length + ' real controls)');
  check(await page.evaluate((ids) => ids.every((id) => !!GK.PM[id]), rv.map((r) => r.id)), 'each row is a real parameter');
  await click('chk-eq_lo_gain'); await click('chk-eq_lo_freq');
  rv = await rows();
  check(rv.filter((r) => r.sel).length === radioIds.length - 2, 'unticking leaves those two out');
  await click('listen-idea');
  st = await S();
  check(st.audition && st.audition.kind === 'review' && same(st.sound, s0), '“With these” auditions only the ticked changes; the sound is unchanged');
  check((await text('hear-chip')).includes('BROKEN RADIO (NOT KEPT)'), 'the take says you are hearing Broken Radio, not kept');
  await layout('review drawer');
  await shot('05_look-inside', ['compact', 'standard']);
  await click('review-keep');
  st = await S();
  const kept = st.sound;
  check(st.sound.eq_lo_gain === s0.eq_lo_gain && st.sound.eq_lo_freq === s0.eq_lo_freq && st.sound.eq_hi_gain === -12 && st.sound.mut_on === 1 && st.sound.sat_drive === 11, 'Keep applies exactly the ticked changes (the low end is left as it was)');
  check(st.desk.find((c) => c.id === 'radio').state === 'kept' && st.review === null && st.audition === null, 'the card is marked KEPT and the review closes');
  check(st.undo[st.undo.length - 1].label === 'Keep part of “Broken Radio”', 'Undo knows what happened: “Keep part of “Broken Radio””');
  n = await note();
  check(n.id === 'kept-radio' && (await visible('note-keep')), 'KID offers to keep it in the notebook');
  await layout('session · after keeping');

  // ------------------------------------------------------------ 5
  section('5. Put an idea away, and bring it back');
  await click('reject-ghost');
  st = await S();
  check(st.desk.find((c) => c.id === 'ghost').state === 'rejected' && (await tid('card-ghost').count()) === 0 && same(st.sound, kept), 'Put away removes Ghost Double from the desk and changes nothing');
  await page.locator('[data-key="toast-act"]').click(); await settle();
  check((await S()).desk.find((c) => c.id === 'ghost').state === 'new' && (await visible('card-ghost')), '“Bring back” puts it back on the desk');

  // ------------------------------------------------------------ 6
  section('6. Undo and redo');
  await click('undo');
  st = await S();
  check(same(st.sound, s0) && st.desk.find((c) => c.id === 'radio').state === 'new', 'Undo restores the sound and the card');
  await click('redo');
  st = await S();
  check(same(st.sound, kept) && st.desk.find((c) => c.id === 'radio').state === 'kept', 'Redo (new in this concept) brings it back');
  await page.keyboard.press('Control+z'); await settle();
  check(same(await sound(), s0), 'Ctrl/Cmd+Z undoes');
  await page.keyboard.press('Control+Shift+z'); await settle();
  check(same(await sound(), kept), 'Ctrl/Cmd+Shift+Z redoes');

  // ------------------------------------------------------------ 7
  section('7. Keep it in the session notebook');
  await click('note-keep');
  check((await visible('modal-keep')) && (await tid('keep-name').inputValue()) === 'Broken Radio', 'the note opens “Keep this sound” with the idea’s name');
  await tid('keep-name').fill(''); await click('m-keep');
  check((await text('keep-error')).includes('Give it a name') && (await visible('modal-keep')), 'an empty name is refused, kindly');
  await layout('modal · keep');
  await tid('keep-name').fill('Radio Hook'); await tid('keep-note').fill('2nd hook, low end kept'); await click('m-keep');
  st = await S();
  check(st.notebook.length === 1 && st.notebook[0].name === 'Radio Hook' && st.notebook[0].note === '2nd hook, low end kept' && same(st.notebook[0].sound, kept), 'the notebook keeps the sound with its name and note');
  check(st.activeNote === st.notebook[0].id && (await page.locator('[data-testid="nb-0"] .sticker.now').isVisible()), 'the entry is marked ON NOW');
  check(st.modal === null && st.kid.event === null, 'the note and the dialog are gone');
  await shot('06_notebook-kept');

  // ------------------------------------------------------------ 8
  section('8. A happy accident: roll, hear, keep, name it');
  await click('roll');
  st = await S();
  const acc = st.desk[0];
  check(acc.kind === 'accident' && acc.state === 'new', 'Happy accident leaves a new card at the front of the desk: “' + acc.name + '”');
  check(await page.evaluate((set) => Object.entries(set).every(([k, v]) => { const p = GK.PM[k]; return p && (p.type !== 'float' || (v >= p.min && v <= p.max)); }) && !Object.keys(set).some((k) => GK.KID_NEVER.includes(k)), acc.set),
    'it is made only of real controls inside their ranges (never gain, mix, the limiter or your key)', Object.keys(acc.set).join(', '));
  check(same(st.sound, kept), 'rolling changes nothing until you keep it');
  await click('hear-' + acc.id);
  check((await S()).audition.id === acc.id, 'Hear auditions the accident');
  await shot('07_happy-accident');
  await click('keep-' + acc.id);
  st = await S();
  const accSound = st.sound;
  check(!same(st.sound, kept) && st.desk.find((c) => c.id === acc.id).state === 'kept', 'Keep makes it yours');
  await click('note-keep');
  check((await tid('keep-name').inputValue()) === acc.name, 'the notebook suggests the accident’s name');
  await click('m-keep');
  st = await S();
  check(st.notebook.length === 2 && st.notebook[0].name === acc.name, 'it is in the notebook, newest first');

  // ------------------------------------------------------------ 9
  section('9. The notebook: compare, use, rename, remove; SHAPE by hand');
  await click('nb-hear-1');
  st = await S();
  check(st.audition && st.audition.kind === 'note' && same(st.sound, accSound) && (await text('hear-chip')).includes('RADIO HOOK (NOT KEPT)'), 'Hear compares a notebook entry without switching to it');
  await click('nb-hear-1');
  await click('nb-use-1');
  st = await S();
  check(same(st.sound, kept) && st.preset.name === 'Radio Hook' && st.preset.type === 'notebook' && st.activeNote === st.notebook[1].id, 'Use switches to Radio Hook without a question (the current sound is already kept)');
  check(st.undo[st.undo.length - 1].label === 'Use “Radio Hook”', 'and Undo can switch back');
  await click('rail-shape'); await click('tab-tone');
  check(await visible('screen-shape'), 'SHAPE opens on its TONE section');
  await press('k-eq_hi_gain', 'ArrowUp', 5);
  st = await S();
  check(st.sound.eq_hi_gain > kept.eq_hi_gain && st.undo[st.undo.length - 1].label.startsWith('Change'), 'a real control (Air shelf) moves by hand, as one undo step');
  await click('tab-tune'); await click('style-Hard');
  st = await S();
  check(st.sound.tune_on === 1 && st.sound.tune_strength === 100 && st.sound.tune_speed === 0, 'the Hard tuning style writes strength 100 %, speed 0 ms and switches Pitch Tune on');
  await layout('SHAPE · TUNE');
  await click('rail-session');
  check(await page.locator('[data-testid="nb-1"] .sticker.changed').isVisible(), 'the notebook shows Radio Hook as ON NOW · CHANGED after the hand moves');
  n = await note();
  check(n.id === 'tunechrom' && n.text.includes('every note'), 'KID notices a real setting: Pitch Tune pulls to every note (Chromatic)');
  await click('note-tune');
  st = await S();
  check(st.view.area === 'shape' && st.view.shapeTab === 'tune', 'its note opens TUNE');
  await click('rail-session'); await click('note-dismiss');
  n = await note();
  check(n.id === 'lowmid' && n.text.includes('300 Hz') && (await page.locator('[data-testid="kidnote"] .badge').textContent()).includes('SIMULATED MEASUREMENT'), 'a measurement-based note is labelled SIMULATED MEASUREMENT');
  await click('nb-ren-1');
  check(await visible('modal-rename'), 'Rename opens');
  await tid('keep-name').fill('Radio Hook v2'); await click('m-keep');
  check((await S()).notebook[1].name === 'Radio Hook v2', 'Rename changes the name');
  await click('nb-del-0');
  check(await visible('modal-remove'), 'Remove asks first');
  await click('m-cancel');
  check((await S()).notebook.length === 2, 'Cancel keeps it');
  const mod = await sound();
  await click('nb-del-0'); await click('m-remove');
  st = await S();
  check(st.notebook.length === 1 && st.notebook[0].name === 'Radio Hook v2' && same(st.sound, mod), 'Remove takes it out; your sound is untouched');

  section('9b. A control moved by hand while a review is open keeps your value');
  await click('rail-session');
  await click('look-close');
  await click('rail-shape'); await click('tab-tone');
  await press('k-eq_hi_gain', 'ArrowUp', 3);
  rv = await rows();
  const handRow = rv.find((r) => r.id === 'eq_hi_gain');
  check(handRow && handRow.manual && !handRow.sel && (await text('row-eq_hi_gain')).includes('YOU MOVED THIS'), 'the row for the control you moved unticks: “YOU MOVED THIS · YOUR VALUE IS KEPT”');
  const handValue = (await sound()).eq_hi_gain;
  await click('review-keep');
  st = await S();
  check(st.sound.eq_hi_gain === handValue && st.sound.comp_attack === 18 && st.sound.comp_release === 160, 'Keep applies the other changes and keeps your hand value');
  await click('undo'); await click('undo');
  check(same(await sound(), mod) && (await S()).desk.find((c) => c.id === 'close').state === 'new', 'two undos go back to before the hand move');

  // ------------------------------------------------------------ 10
  section('10. Unsaved work is never lost silently; a profile keeps your tuning');
  await click('rail-worlds');
  await click('sc-use-DARK & ATMOSPHERIC');
  check(await visible('modal-unsaved'), 'starting from another sound asks first when the current one isn’t kept anywhere');
  await layout('modal · unsaved');
  await click('m-cancel');
  check(same(await sound(), mod), 'Cancel changes nothing');
  await click('sc-use-DARK & ATMOSPHERIC'); await click('m-keepfirst');
  check(await visible('modal-keep'), '“Keep it first” opens the notebook');
  await tid('keep-name').fill('Radio, brighter'); await click('m-keep');
  st = await S();
  const expDark = await page.evaluate((m) => GK.profileSound(3, m), mod);
  check(st.notebook[0].name === 'Radio, brighter' && same(st.notebook[0].sound, mod), 'your sound was kept in the notebook first');
  check(st.preset.name === 'DARK & ATMOSPHERIC' && same(st.sound, expDark) && st.sound.eq_hi_gain === -3.5, 'then DARK & ATMOSPHERIC loads, with its exact Profiles.inc values');
  check(st.sound.tune_on === 1 && st.sound.tune_strength === 100 && st.sound.tune_speed === 0, 'your tuning stays (one of the 22 user-owned parameters)');

  // ------------------------------------------------------------ 11
  section('11. WORLDS: hear without loading, look inside, take part, use a draft world, keep your version');
  await click('sc-hear-SMOOTH R&B');
  st = await S();
  check(st.audition && st.audition.kind === 'scene' && same(st.sound, expDark) && (await page.locator('[data-testid="scene-SMOOTH R&B"] .sticker.hearing').isVisible()), 'Hear auditions a factory tape without loading it');
  await click('sc-hear-SMOOTH R&B');
  check((await text('scene-SMOOTH R&B')).includes('FACTORY · IN THE PLUG-IN') && (await text('scene-Radio Static')).includes('DRAFT · NOT LISTENING-TESTED'), 'factory tapes and draft worlds are labelled for what they are');
  await click('sc-look-Radio Static');
  rv = await rows();
  const owned = await page.evaluate(() => GK.USER_OWNED);
  check(rv.length > 0 && rv.every((r) => !owned.includes(r.id)), 'Look lists what Radio Static changes; your user-owned parameters are never in it');
  check((await page.locator('[data-testid="review"] .sticker').first().textContent()).includes('DRAFT WORLD'), 'the review says DRAFT WORLD');
  await page.locator('[data-key="tick-none"]').click(); await settle();
  check(await tid('review-keep').isDisabled(), 'with nothing ticked, Keep is disabled');
  await click('chk-eq_lo_gain'); await click('chk-eq_hi_gain');
  await click('review-keep');
  st = await S();
  check(diffIds(expDark, st.sound).join() === 'eq_hi_gain,eq_lo_gain' && st.sound.eq_lo_gain === -12 && st.sound.eq_hi_gain === -12 && st.preset.name === 'DARK & ATMOSPHERIC', 'Keep takes only the two ticked settings from the world');
  const partial = st.sound;
  await click('sc-use-Radio Static');
  check(await visible('modal-unsaved'), 'using the whole world asks first (your changes aren’t kept anywhere)');
  await click('m-discard');
  st = await S();
  const expRadio = await page.evaluate((m) => GK.worldSound(GK.WORLDS.find((w) => w.id === 'radio'), m), partial);
  check(st.preset.type === 'world' && st.preset.name === 'Radio Static' && same(st.sound, expRadio), 'Use loads the draft world: a valid state of real parameters');
  check((await tid('sc-keep-Radio Static').count()) === 0, '“Keep my version” waits until you change something');
  await click('rail-shape'); await click('tab-tone');
  await press('k-eq_b2_gain', 'ArrowDown', 4);
  await click('rail-worlds');
  await click('sc-keep-Radio Static');
  check((await tid('keep-name').inputValue()) === 'My Radio Static', '“Keep my version” keeps your customised world in the notebook');
  await click('m-keep');
  check((await S()).notebook[0].name === 'My Radio Static', 'it is in the notebook');
  await layout('WORLDS');
  await shot('08_worlds', ['standard', 'large']);

  // ------------------------------------------------------------ 12
  section('12. KID: a guided assistant that leaves ideas, never applies them');
  await fast();
  await click('rail-session');
  const baseK = await sound();
  await kidType('Make this hook feel wider.');
  st = await S();
  const kid1 = await kidCards();
  check(st.kid.phase === 'done' && (await page.evaluate(() => GK.phaseLog.join('>'))) === 'reading>building>done', 'typed: reading your words → leaving ideas → done');
  check(kid1.length === 3 && kid1.every((c) => c.rev === 1 && c.setNo === kid1[0].setNo), 'KID leaves three directions as cards (straight, words up front, further out)');
  check(same(st.sound, baseK), 'KID changed nothing: the ideas wait on the desk');
  check(kid1.every((c) => !Object.keys(c.set).some((k) => facts.never.split(',').includes(k))), 'no KID idea touches gain, mix, the limiter or your key and scale');
  check(kid1[0].set.dbl_on === 1 && kid1[0].set.dim_on === 1, '“wider” = Doubler and Dimension on (real controls)');
  n = await note();
  check(n.text.includes('Heard:') && n.text.includes('wider') && n.text.includes('3 ideas'), 'KID’s note says what it heard and where the ideas are');
  check((await text('kid-status')).includes('Ideas are on the desk') && (await text('kid-label')).includes('guided assistant') && (await tid('kid-label').getAttribute('aria-description')).includes('not generative AI'), 'KID is presented as a guided creative assistant (its description: not generative AI)');
  await layout('session · KID ideas');
  await shot('09_kid-three-ideas', ['compact', 'standard', 'large']);
  await kidType('A little less.');
  const kid2 = await kidCards();
  check(kid2.length === 3 && kid2.every((c) => c.rev === 2 && c.setNo === kid1[0].setNo), '“A little less.” revises the same ideas (revision 2)');
  check(kid2[0].set.dim_mix < kid1[0].set.dim_mix && (await note()).text.includes('about 70%'), 'less: Dimension mix ' + kid1[0].set.dim_mix + ' → ' + kid2[0].set.dim_mix + ' %');
  await kidType('Keep the vocal upfront.');
  st = await S();
  const kid3 = await kidCards();
  check(kid3.every((c) => c.rev === 3) && st.kid.mem.locks.includes('front'), '“Keep the vocal upfront.” → revision 3, keeping the vocal upfront');
  check(kid3.every((c) => (c.set.dim_mix === undefined || c.set.dim_mix <= 35) && (c.set.echo_mix === undefined || c.set.echo_mix <= 22)), 'upfront caps Dimension at 35 % and Echo at 22 %');
  check(await page.locator('[data-testid="card-' + kid3[0].id + '"] .badge').first().textContent() === 'REV 3', 'the cards show their revision');
  await click('note-hear');
  st = await S();
  check(st.audition && st.audition.kind === 'card' && st.audition.id === kid3[0].id && same(st.sound, baseK), '“Hear the first” auditions KID’s first idea; nothing changes');
  await click('keep-' + kid3[1].id);
  st = await S();
  check(!same(st.sound, baseK) && ['gain', 'mix', 'lim_on', 'lim_ceiling', 'lim_drive', 'lim_release', 'tune_key', 'tune_scale'].every((k) => st.sound[k] === baseK[k]), 'Keep applies one idea; gain, mix, the limiter, key and scale are untouched');
  check(st.kid.mem === null, 'after you keep one, KID’s next ideas start from your new sound');
  await tid('kid-hears').selectOption('1');
  const vBase = await sound();
  await page.evaluate(() => { GK.phaseLog.length = 0; });
  { const b = await tid('mic').boundingBox();
    await page.mouse.move(b.x + b.width / 2, b.y + b.height / 2); await page.mouse.down(); await wait(140);
    check((await S()).kid.phase === 'recording' && (await tid('kid-status').getAttribute('data-phase')) === 'recording', 'holding the mic: “Listening… let go to send”');
    await shot('10_kid-listening');
    await page.mouse.up(); }
  await until(() => !GK.S.kid.busy && GK.S.kid.phase === 'done', 'KID to answer the voice command');
  st = await S();
  check((await page.evaluate(() => GK.phaseLog.join('>'))) === 'recording>transcribing>reading>building>done', 'voice: listening → transcribing on this computer → reading → ideas → done');
  const kid4 = await kidCards();
  check(kid4.length >= 2 && kid4.every((c) => c.setNo === kid3[0].setNo + 1 && c.rev === 1), 'a new set of ideas; the old, unkept ones made room');
  check(st.desk.some((c) => c.id === kid3[1].id && c.state === 'kept'), 'the idea you kept stays on the desk, marked KEPT');
  check(kid4[0].set.eq_hi_gain < vBase.eq_hi_gain && same(st.sound, vBase), '“darker and more intimate” = less air on top; still nothing applied');
  await kidType('Give me a rougher, more unstable texture.');
  const kid5 = await kidCards();
  n = await note();
  check(n.text.includes('new words for me'), 'words KID’s rules don’t know today (“rougher”, “unstable”) are flagged as new');
  { const eff = { ...vBase, ...kid5[0].set };      // the sound this idea would give (Radio Static already has Mutation on)
    check(kid5[0].rev === 2 && eff.sat_drive > vBase.sat_drive && eff.mut_on === 1 && eff.mut_amount > vBase.mut_amount, 'rougher = more saturation; unstable = more Mutation (revision 2 of the same ideas)', 'drive ' + vBase.sat_drive + '→' + eff.sat_drive + ', mutation ' + vBase.mut_amount + '→' + eff.mut_amount); }
  await kidType('Make the vocal feel distant but keep the words clear.');
  st = await S();
  const kid6 = await kidCards();
  check(st.kid.mem.locks.includes('presence'), '“keep the words clear” locks the presence band (a KidDirector rule)');
  check(kid6[0].set.echo_on === 1 && (kid6[0].set.eq_b2_gain === undefined || kid6[0].set.eq_b2_gain >= vBase.eq_b2_gain), 'distant = echo on, but the presence band is never cut');
  await kidType('Add some reverb.');
  check((await note()).text.includes('GlitchKid has no reverb'), '“reverb” is answered honestly: the space here is Echo + Dimension');
  const deskBefore = (await S()).desk.map((c) => c.id + c.state).join();
  const sBefore = await sound();
  await kidType('Fix my timing.');
  st = await S();
  n = await note();
  check(st.kid.phase === 'error' && n.cls.includes('err') && n.text.includes('Timing isn’t something GlitchKid changes'), 'unsupported: “Fix my timing.” gets an honest answer');
  check(st.desk.map((c) => c.id + c.state).join() === deskBefore && same(st.sound, sBefore), 'nothing on the desk or in the sound changed');
  await layout('session · KID unsupported');
  await shot('11_kid-unsupported');
  await kidType('purple monkey dishwasher');
  check((await note()).text.includes('didn’t catch a sound word'), 'not understood: KID says so and suggests words');
  await page.evaluate(() => { GK.pace.k = 1; });
  { const b = await tid('mic').boundingBox(); await page.mouse.move(b.x + b.width / 2, b.y + b.height / 2); await page.mouse.down(); await page.mouse.up(); await settle(); }
  check((await note()).text.includes('Hold the button while you talk'), 'a quick tap explains push-to-talk');
  await fast();
  await sim('speech-missing');
  await holdMic(60);
  n = await note();
  check(n.text.includes('Speech isn’t set up') && (await visible('note-speech')) && (await text('kid-status')).includes('Typing works'), 'speech not set up: KID says so, offers set-up; typing works');
  await click('note-speech');
  const sp = await text('modal-speech');
  check(sp.includes('142 MiB') && sp.includes('SHA-1') && sp.includes('never uploaded'), 'set-up explains the one-time download, the hash check and that your voice stays local');
  await layout('modal · speech');
  await click('m-download');
  await until(() => GK.S.sim.speech === 'ready' && GK.S.modal && GK.S.modal.dl >= 100, 'the simulated model download');
  await settle(); await click('m-download');
  check((await S()).sim.speech === 'ready' && (await S()).modal === null, 'after the download speech is ready');
  await sim('speech-denied');
  await holdMic(60);
  check((await note()).text.includes('Privacy & Security'), 'mic blocked: KID says where to allow it');
  await kidType('Make it darker.');
  check((await S()).kid.phase === 'done', 'typing still works with the mic blocked');
  await sim('speech-ready');
  await page.evaluate(() => { document.activeElement && document.activeElement.blur(); GK.phaseLog.length = 0; });
  await page.keyboard.down('k'); await wait(80); await page.keyboard.up('k');
  await until(() => !GK.S.kid.busy && GK.S.kid.phase === 'done', 'KID to answer the K key');
  check((await page.evaluate(() => GK.phaseLog[0])) === 'recording', 'holding K talks to KID from anywhere');
  await sim('engine-error');
  check((await text('wave-msg')).includes('Not processing') && (await text('kid-status')).includes('Unavailable'), 'not processing: the take and KID say so');
  await kidType('Make it darker.');
  check((await note()).text.includes('isn’t processing') && (await S()).kid.phase === 'unavailable', 'KID refuses honestly while GlitchKid isn’t processing');
  await layout('session · not processing');
  await sim('engine-ok');
  await click('note-dismiss');

  // ------------------------------------------------------------ 13
  section('13. A/B: empty captures, occupied recalls, nothing overwritten silently');
  const a0 = await sound();
  check((await tid('ab-A').getAttribute('data-state')) === 'empty' && (await tid('ab-B').getAttribute('data-state')) === 'empty', 'A and B start empty');
  await click('ab-btn-A');
  st = await S();
  check(same(st.ab.A.sound, a0) && st.ab.active === 'A' && (await tid('ab-A').getAttribute('data-state')) === 'playing' && !(await visible('modal-abswitch')), 'an empty A stores the current sound, without a question');
  await click('keep-after');
  check((await tid('ab-A').getAttribute('data-state')) === 'changed', 'A shows “changed” after a change');
  const a1 = await sound();
  await click('ab-btn-B');
  st = await S();
  check(same(st.ab.B.sound, a1) && st.ab.active === 'B' && same(st.ab.A.sound, a0), 'an empty B stores the current sound; A keeps its own');
  await click('ab-btn-A');
  st = await S();
  check(same(st.sound, a0) && st.ab.active === 'A', 'an occupied A recalls it (B had no unsaved changes)');
  await click('keep-close');
  const a2 = await sound();
  await click('ab-btn-B');
  check(await visible('modal-abswitch'), 'switching away from a changed A asks first');
  await layout('modal · A/B switch');
  await click('m-cancel');
  check(same(await sound(), a2) && (await S()).ab.active === 'A', 'Cancel keeps you on A, with your changes');
  await click('ab-btn-B'); await click('m-switch');
  st = await S();
  check(same(st.sound, a1) && st.ab.active === 'B' && same(st.ab.A.sound, a0), '“Switch without storing” plays B and leaves A as stored');
  await click('ab-store-A');
  check(await visible('modal-abover'), 'storing over a different sound in A asks first');
  await click('m-cancel');
  check(same((await S()).ab.A.sound, a0), 'Cancel leaves A as it was');
  await click('ab-store-A'); await click('m-replace');
  st = await S();
  check(same(st.ab.A.sound, a1) && st.ab.active === 'A', 'Replace stores the current sound in A');
  await shot('12_ab-slots');

  // ------------------------------------------------------------ 14
  section('14. Save a preset (every project), update it, and the rules that protect it');
  await click('save');
  check(await visible('modal-save'), 'Save… opens');
  await tid('save-name').fill(''); await click('m-save');
  check((await text('save-error')).includes('Give your sound a name'), 'an empty name is refused');
  await tid('save-name').fill('night/drive');
  check((await text('save-error')).includes('can’t contain'), 'characters a file name can’t hold are refused');
  await tid('save-name').fill('Smooth R&B');
  check((await text('save-error')).includes('factory name'), 'factory names are protected');
  await tid('save-name').fill('Night Drive Lead'); await click('m-save');
  st = await S();
  check(st.lib.length === 1 && st.lib[0].name === 'Night Drive Lead' && same(st.lib[0].sound, st.sound) && st.preset.type === 'user', 'Save keeps a preset for every project');
  check((await text('sound-meta')).includes('Saved sound · Saved'), 'the top bar says it is saved');
  await click('save');
  check(!(await visible('modal-save')) && (await text('toast')).includes('Nothing has changed since'), 'saving again with no change just says so');
  await click('keep-ghost');
  check((await text('sound-meta')).includes('Unsaved'), 'a change shows ● Unsaved');
  await click('save');
  check((await tid('save-name').inputValue()) === 'Night Drive Lead', 'Save… offers the same name to update it');
  await click('m-save');
  check(await visible('modal-overwrite'), 'replacing a saved sound asks first');
  await click('m-cancel');
  check((await visible('modal-save')) && !same((await S()).lib[0].sound, await sound()), 'Cancel goes back; nothing is replaced');
  await click('m-save'); await click('m-replace');
  st = await S();
  check(same(st.lib[0].sound, st.sound) && !(await text('sound-meta')).includes('Unsaved'), 'Replace updates the saved sound');

  // ------------------------------------------------------------ 15
  section('15. TOOLS: Beat Lock, Reference Match, Build My Mix (existing engines, reviewed before anything changes)');
  await click('rail-tools');
  check((await visible('beat-unrouted')) && (await tid('beat-analyse').isDisabled()), 'Beat Lock without the beat routed explains the sidechain routing and waits');
  await sim('sidechain-beat');
  check(await visible('beat-routed'), 'with the beat on the Instrumental input it is ready');
  await click('beat-analyse');
  await until(() => GK.S.beat.analysed, 'Beat Lock to listen');
  await settle();
  check(await page.locator('[data-testid="tool-beat-panel"] .badge.sim').isVisible(), 'the band comparison is labelled SIMULATED MEASUREMENTS');
  await layout('TOOLS · Beat Lock');
  const bl0 = await sound();
  await click('beat-review');
  rv = await rows();
  check(rv.length > 0 && rv.every((r) => ['eq_b2_gain', 'eq_b1_gain', 'eq_lo_gain', 'echo_mix', 'dim_mix'].includes(r.id)) && rv.every((r) => r.id !== 'eq_b2_gain' || r.to <= 4) && rv.every((r) => !['eq_b1_gain', 'eq_lo_gain'].includes(r.id) || r.to >= -6),
    'Beat Lock proposes only beatMoves: presence (at most +4 dB), Body/low shelf (never below −6 dB), existing echo/width', rv.map((r) => r.id + '→' + r.to).join(', '));
  check((await text('advisory')).includes('BALANCE') && (await text('advisory')).includes('never moves your faders'), 'balance is advice only: GlitchKid never moves your faders');
  await shot('13_beat-lock');
  const untick = rv.find((r) => r.id !== 'eq_b2_gain').id;
  await click('chk-' + untick);
  await click('review-keep');
  st = await S();
  check(st.sound[untick] === bl0[untick] && diffIds(bl0, st.sound).length === rv.length - 1, 'Keep applies only the ticked Beat Lock moves');
  await click('undo');
  check(same(await sound(), bl0), 'Undo takes them back');
  await click('tool-reference');
  check(await tid('ref-analyse').isDisabled(), 'Reference Match: no song chosen, nothing to analyse');
  await click('ref-choose');
  check(await tid('ref-analyse').isDisabled(), 'the rights box must be ticked first');
  await click('ref-rights'); await click('ref-analyse');
  await until(() => GK.S.ref.refDone, 'the reference analysis');
  await settle(); await click('ref-vocal');
  await until(() => GK.S.ref.vocalDone, 'the vocal analysis');
  await settle();
  await layout('TOOLS · Reference Match');
  const rf0 = await sound();
  await click('ref-review');
  rv = await rows();
  check(rv.length > 0 && rv.every((r) => ['eq_b1_gain', 'eq_b2_gain', 'eq_hi_gain'].includes(r.id)), 'Reference Match moves only Body, Presence and Air shelf', rv.map((r) => r.id).join(', '));
  check(rv.every((r) => Math.abs(r.to - rf0[r.id]) <= 3 + 1e-6 && Math.abs(r.to - rf0[r.id]) >= 0.05) && rv.every((r) => (r.id === 'eq_b2_gain' ? r.to <= 4 : Math.abs(r.to) <= 6 + 1e-6)), 'no move over 3 dB; presence at or under +4 dB, Body and Air within ±6 dB (ReferenceMatch.cpp)');
  check((await text('review')).includes('never a copy') && (await text('review')).includes('Confidence: medium'), 'it says it is tonal guidance, never a copy, at medium confidence for a full mix');
  if (rf0.eq_hi_gain < -6) { check(!rv.some((r) => r.id === 'eq_hi_gain') && (await text('review')).includes('Air shelf: already at or beyond its limit'), 'a band already beyond ±6 dB (Air shelf ' + rf0.eq_hi_gain + ' dB) is left alone, as ReferenceMatch.h documents');
    notes.push('ReferenceMatch.cpp itself would pull a band that starts beyond ±6 dB back to the edge in one move (here Air shelf ' + rf0.eq_hi_gain + ' → −6 dB), which contradicts ReferenceMatch.h. The prototype follows the header. See OPEN_DECISIONS.md.'); }
  const full = rv.map((r) => Math.abs(r.to - rf0[r.id]));
  await click('review-cancel');
  check(same(await sound(), rf0), 'Cancel changes nothing');
  await page.locator('[data-key="ref-intensity"]').focus(); for (let i = 0; i < 5; i++) await page.keyboard.press('PageDown'); await settle();
  check((await S()).ref.intensity === 50, 'intensity set to 50 %');
  await click('ref-review');
  const half = (await rows()).map((r) => Math.abs(r.to - rf0[r.id]));
  check(half.every((d, i) => d <= full[i] + 1e-6) && half.reduce((a, b) => a + b, 0) < full.reduce((a, b) => a + b, 0), 'lower intensity asks for smaller moves');
  await click('review-close');
  await click('tool-build');
  await click('build-start');
  await click('build-listen');
  await until(() => GK.S.build && GK.S.build.listened, 'Build My Mix to listen');
  await settle();
  check(await page.locator('[data-testid="tool-build-panel"] .badge.sim').isVisible(), 'what it heard is labelled SIMULATED MEASUREMENTS');
  await click('build-next'); await click('dir-SMOOTH R&B');
  await layout('TOOLS · Build My Mix');
  const bm0 = await sound();
  await click('build-review');
  rv = await rows();
  check(rv.length > 5 && rv.every((r) => !owned.includes(r.id)), 'Build My Mix never proposes your 22 user-owned parameters (' + rv.length + ' changes listed)');
  check(rv.some((r) => r.id === 'comp_thresh' && r.sim), 'the move fitted to your take is labelled SIMULATED MEASUREMENT');
  await shot('14_build-my-mix-review', ['standard', 'compact']);
  await click('chk-' + rv[0].id);
  await click('review-keep');
  st = await S();
  check(st.preset.name === 'Built: SMOOTH R&B' && st.preset.type === 'built' && st.sound[rv[0].id] === bm0[rv[0].id] && diffIds(bm0, st.sound).length === rv.length - 1, 'Keep applies exactly the ticked changes and names the sound “Built: SMOOTH R&B”');
  check((await text('sound-meta')).includes('Not kept yet'), 'the top bar says the built sound isn’t kept anywhere yet');
  await click('undo');
  check(same(await sound(), bm0), 'Undo takes the whole build back');
  await sim('signal-none');
  await click('build-start');
  check((await visible('build-nosignal')) && (await visible('build-skip')), 'with no vocal Build My Mix says so and offers a direction “as designed”');
  await click('build-skip'); await click('build-review');
  check((await rows()).every((r) => !r.sim) && (await text('review')).includes('not fitted to your take'), 'unfitted: no simulated measurement, and it says so');
  await click('review-cancel');
  await click('build-cancel');
  check(same(await sound(), bm0), 'cancelling Build My Mix changes nothing');
  await sim('signal-live');
  await click('rail-session');
  await kidType('Build my mix.');
  check((await S()).view.area === 'tools' && (await S()).view.toolTab === 'build' && (await S()).build !== null, '“Build my mix” to KID opens Build My Mix');
  await click('build-cancel');

  // ------------------------------------------------------------ 16
  section('16. MASTER: the existing Mastering Lab, kept apart from vocal shaping');
  await click('rail-master');
  await click('master-on');
  check(await visible('modal-master-on'), 'turning on Mastering mode asks first (it changes what this whole instance does)');
  await click('m-cancel');
  check(!(await S()).master.on, 'Cancel leaves it off');
  await click('master-on'); await click('m-masteron');
  st = await S();
  check(st.master.on && (await text('mval-master_intensity')) === '60 %' && (await text('mval-master_loudness')) === '+3.0 dB', 'Mastering mode on; the dials show the real defaults (Intensity 60 %, Loudness +3 dB)');
  check(await page.evaluate(() => [...document.querySelectorAll('.cchip')].every((c) => c.dataset.on === '0')), 'the vocal chain shows as not in use');
  await layout('MASTER · on');
  await shot('15_master-on');
  await click('rail-session');
  check((await visible('banner-master')) && (await note()).id === 'master' && (await text('kid-status')).includes('Mastering mode is on'), 'the session and KID say Mastering mode is on');
  await kidType('Make it darker.');
  check((await S()).kid.phase === 'unavailable', 'KID refuses while mastering');
  await click('rail-master'); await click('master-on');
  check(!(await S()).master.on, 'switching it off brings the vocal chain back');

  // ------------------------------------------------------------ 17
  section('17. States: no input, clipping, a loaded file, compact layout tabs');
  await click('rail-session');
  for (let i = 0; i < 4; i++) { const id = (await note()).id; if (id && (/^r[0-9a-z]/.test(id) || id.startsWith('kept-'))) await click('note-dismiss'); else break; }
  await sim('signal-none');
  check((await text('wave-msg')).includes('No vocal yet') && (await note()).id === 'noinput', 'no input: the take and KID say so, and how to fix it');
  await layout('session · no input');
  await shot('16_no-input');
  await sim('signal-clipping');
  await until(() => !document.querySelector('[data-testid="clip"]').hidden, 'the CLIP light', 8000);
  check((await note()).id === 'clip' && (await note()).text.includes('can’t repair'), 'input clipping: CLIP lights and KID says it happens before GlitchKid');
  await layout('session · clipping');
  await shot('17_clipping');
  await sim('signal-live');
  await sim('take-file');
  check((await text('take-source')).includes('look only') && (await text('take-file')).includes('Back to live input'), 'a loaded file is for looking and measuring only (the plug-in can’t play files)');
  await sim('take-live');
  await page.evaluate(() => GK.setSize('compact')); await settle();
  check((await visible('rtab-kid')) && (await visible('rtab-notebook')), 'compact window: KID and the notebook share the right column as tabs');
  await click('rtab-notebook');
  check(await visible('notebook'), 'the NOTEBOOK tab shows the notebook');
  await layout('compact · notebook tab');
  await click('rtab-kid');
  await page.evaluate(() => GK.setSize('standard')); await settle();

  // ------------------------------------------------------------ 18
  section('18. Close and reopen: the session comes back as you left it');
  const pre = await S();
  await page.locator('[data-sim="reopen"]').click();
  await page.waitForLoadState('load'); await wait(400);
  st = await S();
  check(!st.onb.open, 'the intro does not come back');
  check(st.session.name === pre.session.name && same(st.sound, pre.sound) && st.preset.name === pre.preset.name, 'the session name, the sound and its source come back');
  check(st.notebook.length === pre.notebook.length && st.notebook.every((e, i) => e.name === pre.notebook[i].name && same(e.sound, pre.notebook[i].sound)), 'the notebook comes back');
  check(same(st.ab.A.sound, pre.ab.A.sound) && same(st.ab.B.sound, pre.ab.B.sound) && st.ab.active === pre.ab.active, 'the A/B slots come back');
  check(st.desk.map((c) => c.id + ':' + c.state).join() === pre.desk.map((c) => c.id + ':' + c.state).join(), 'the desk comes back as you left it');
  await click('rail-worlds');
  check(await visible('scene-Night Drive Lead'), 'your saved sound is on the WORLDS shelf');
  await click('sc-use-Night Drive Lead');
  if (await visible('modal-unsaved')) await click('m-discard');
  st = await S();
  check(same(st.sound, st.lib[0].sound) && st.preset.type === 'user', 'Use recalls the saved sound exactly');
  await click('sc-del-Night Drive Lead');
  check(await visible('modal-deluser'), 'deleting a saved sound asks first');
  await click('m-delete');
  st = await S();
  check(st.lib.length === 0 && st.preset.type === 'loose' && (await text('sound-meta')).includes('Deleted · ● Not kept yet') && (await visible('my-tapes-empty')), 'Delete removes it; your current sound stays, marked “Deleted · ● Not kept yet”');

  // ------------------------------------------------------------ 19
  section('19. Layout at three window sizes, every area');
  await fast();
  await click('rail-session');
  await kidType('Make it wider.');
  for (const sz of ['compact', 'standard', 'large']) {
    await page.evaluate((v) => GK.setSize(v), sz); await settle();
    await click('rail-session'); await layout(sz + ' · SESSION');
    await click('rail-shape');
    for (const t of ['clean', 'tune', 'tone', 'dynamics', 'space', 'output']) { await click('tab-' + t); await layout(sz + ' · SHAPE › ' + t.toUpperCase()); }
    await click('rail-lab'); await layout(sz + ' · LAB');
    await click('rail-worlds'); await layout(sz + ' · WORLDS');
    await click('rail-tools');
    for (const t of ['beat', 'reference', 'build']) { await click('tool-' + t); await layout(sz + ' · TOOLS › ' + t); }
    await click('rail-master'); await layout(sz + ' · MASTER');
    await click('rail-session');
    const c = (await kidCards())[0];
    if (c) { await click('look-' + c.id); await layout(sz + ' · review drawer'); await click('review-close'); }
    await click('help'); await layout(sz + ' · help'); await click('m-close');
  }
  await page.evaluate(() => GK.setSize('standard')); await settle();
  await click('rail-shape'); await click('tab-space'); await shot('18_shape-space', ['compact', 'standard', 'large']);
  await click('rail-lab'); await shot('19_lab');
  await click('rail-session');

  // ------------------------------------------------------------ 20
  section('20. Help, keyboard, contrast, text size');
  await click('help');
  const help = await text('modal-help');
  check(help.includes('Where’s play?') && help.includes('not generative AI') && help.includes('Ctrl/Cmd+Shift+Z'), 'Help explains where play is, how KID works (not generative AI) and the shortcuts');
  await shot('20_help');
  await page.keyboard.press('Escape'); await settle();
  await page.evaluate(() => document.activeElement && document.activeElement.blur());
  let focusIn = false, ring = 'none';
  for (let i = 0; i < 120 && !focusIn; i++) { await page.keyboard.press('Tab'); ({ focusIn, ring } = await page.evaluate(() => { const a = document.activeElement; const cs = a ? getComputedStyle(a) : null; return { focusIn: !!(a && a.closest('#frame')), ring: cs ? (cs.boxShadow !== 'none' ? cs.boxShadow : cs.outlineStyle !== 'none' ? 'outline ' + cs.outlineWidth : 'none') : 'none' }; })); }
  check(focusIn && ring !== 'none', 'keyboard focus reaches the plug-in and shows a visible focus ring', ring);
  const pairs = [
    ['#f6f1fc', '#160f21', 'text on a panel'], ['#cdc1de', '#160f21', 'secondary text on a panel'], ['#a497b8', '#160f21', 'dim text on a panel'],
    ['#a497b8', '#271d36', 'dim text on a raised button'], ['#a497b8', '#07050b', 'help line'], ['#a497b8', '#221a2f', 'dim text on an idea card'],
    ['#22162d', '#efe5cf', 'tape label and cassette label (paper)'], ['#1f1330', '#e9d6ff', 'KID note'], ['#4d3a62', '#e9d6ff', 'KID note meta line'],
    ['#ffffff', '#2a1740', 'KID note primary button'], ['#1f1330', '#ffd3dc', 'KID error note'], ['#4a3a12', '#f1d6d0', 'badge on a KID note'],
    ['#142004', '#b8ff5c', 'Keep button, KEPT / ON NOW stickers'], ['#04232b', '#5ee3ff', 'HEARING sticker'], ['#2a0619', '#ff4fb8', 'ACCIDENT sticker, KID count'],
    ['#2a1a02', '#ffc163', 'CHANGED sticker'], ['#ffffff', '#6f3df2', 'primary button (gradient start)'], ['#ffffff', '#a63ad6', 'primary button (gradient middle)'],
    ['#ffffff', '#c42f8f', 'primary button (gradient end)'], ['#ffdc94', '#160f21', 'SIMULATED badge'], ['#bdf5dc', '#160f21', 'IN THE PLUG-IN badge'],
    ['#ddd2ee', '#160f21', 'DRAFT badge'], ['#ffc163', '#1d1529', '● Unsaved'], ['#ffb8c3', '#160f21', 'error text'], ['#dff9ff', '#11303a', 'DRY pressed'],
    ['#d8fbff', '#1c2033', 'HEARING chip while auditioning'], ['#2a0610', '#ff6479', 'CLIP'], ['#ffe4b4', '#261b26', 'warning banner'],
    ['#dcecf8', '#201c30', 'Mastering banner'], ['#ffc6cf', '#2b1119', 'danger button'],
  ];
  let low = 99, lowWhat = '';
  for (const [fg, bg, what] of pairs) { const c = contrast(fg, bg); if (c < low) { low = c; lowWhat = what; } check(c >= 4.5, 'contrast ≥ 4.5:1 — ' + what, c.toFixed(2) + ':1'); }
  notes.push('Lowest contrast pair: ' + low.toFixed(2) + ':1 (' + lowWhat + ').');
  const minPx = await page.evaluate(() => Math.min(...[...document.querySelectorAll('#frame *')].filter((e) => [...e.childNodes].some((x) => x.nodeType === 3 && x.textContent.trim())).map((e) => parseFloat(getComputedStyle(e).fontSize))));
  check(minPx >= 12, 'the smallest text in the window is ' + minPx + ' px');
  const handUi = await page.evaluate(() => [...document.querySelectorAll('#frame button, #frame [role="slider"], #frame .k-value, #frame .badge, #frame .kid-status')].filter((e) => getComputedStyle(e).fontFamily.includes('Kalam')).map((e) => e.dataset.testid || e.className).slice(0, 5));
  check(handUi.length === 0, 'handwriting is only for names and notes: no control, value or status uses it', handUi.join(', '));
  check(consoleErrors.length === 0, 'no console errors during the whole run', consoleErrors.join(' | '));

  // ------------------------------------------------------------ hero screenshots at 2x, from the same saved session
  if (SHOTS) {
    const saved = await page.evaluate(() => { GK.persistNow(); return JSON.stringify({ ...localStorage }); });
    const hctx = await browser.newContext({ viewport: { width: 1460, height: 1000 }, deviceScaleFactor: 2 });
    await hctx.addInitScript((data) => { if (!sessionStorage.getItem('gk-seeded')) { const o = JSON.parse(data); for (const [k, v] of Object.entries(o)) localStorage.setItem(k, v); sessionStorage.setItem('gk-seeded', '1'); } }, saved);
    const hp = await hctx.newPage();
    await hp.goto(base); await hp.waitForTimeout(500);
    const ht = (t) => hp.locator('[data-testid="' + t + '"]');
    const hs = async (name, sz = 'standard') => { await hp.evaluate((v) => { GK.setSize(v); const t = document.querySelector('.toast-layer'); if (t) t.replaceChildren(); }, sz); await hp.waitForTimeout(250); await hp.locator('#frameWrap').screenshot({ path: path.join(SHOT_DIR, 'hero', name + '@2x.png') }); };
    await hp.evaluate(() => { GK.pace.k = 0.05; });
    await ht('kid-input').fill('Make the vocal feel distant but keep the words clear.'); await ht('kid-input').press('Enter');
    await hp.waitForFunction(() => !GK.S.kid.busy && GK.S.kid.phase === 'done');
    await hs('session-kid-ideas'); await hs('session-kid-ideas-large', 'large'); await hs('session-compact', 'compact');
    const first = await hp.evaluate(() => GK.S.desk.find((c) => c.kind === 'kid' && c.state === 'new').id);
    await ht('look-' + first).click(); await hp.waitForTimeout(100); await ht('listen-idea').click(); await hs('review-drawer');
    await ht('review-close').click(); await ht('rail-worlds').click(); await hs('worlds');
    await hctx.close();
  }

  console.log('\n' + passed + ' passed, ' + failed + ' failed');
  if (notes.length) console.log('Notes:\n  - ' + notes.join('\n  - '));
  if (failures.length) console.log('Failures:\n  - ' + failures.join('\n  - '));
  await browser.close(); srv.close();
  process.exit(failed ? 1 : 0);
})().catch((e) => { console.error(e); process.exit(2); });
