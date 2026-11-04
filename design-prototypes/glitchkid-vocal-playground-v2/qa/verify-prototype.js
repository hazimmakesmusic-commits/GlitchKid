// Drives the prototype in headless Chromium and checks the whole journey, the safety rules, the layout and the text size; saves screenshots.
// Run from this folder's parent:  node qa/verify-prototype.js            (checks only)
//                                 node qa/verify-prototype.js --shots    (checks + screenshots into ../screenshots)
// Needs only Playwright (pre-installed in the build container; elsewhere: npm i -D playwright && npx playwright install chromium).
'use strict';
const http = require('http');
const fs = require('fs');
const path = require('path');
const { chromium } = require('playwright');

const ROOT = path.resolve(__dirname, '..');
const SHOTS = process.argv.includes('--shots');
const SHOT_DIR = path.join(ROOT, 'screenshots');
const TYPES = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.png': 'image/png' };

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
  const ctx = await browser.newContext({ viewport: { width: 1460, height: 1080 }, deviceScaleFactor: 1 });
  const page = await ctx.newPage();
  const consoleErrors = [];
  page.on('console', (m) => { if (m.type() === 'error') consoleErrors.push(m.text()); });
  page.on('pageerror', (e) => consoleErrors.push(e.message));
  if (SHOTS) fs.mkdirSync(SHOT_DIR, { recursive: true });

  const S = () => page.evaluate(() => JSON.parse(JSON.stringify(GK.S)));
  const sound = () => page.evaluate(() => JSON.parse(JSON.stringify(GK.S.sound)));
  const tid = (t) => page.locator('[data-testid="' + t + '"]');
  const settle = () => page.evaluate(() => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r))));   // the prototype redraws on the next frame
  const click = async (t) => { await tid(t).first().click(); await settle(); };
  const sim = async (k) => { await page.locator('[data-sim="' + k + '"]').click(); await settle(); };
  const wait = (ms) => page.waitForTimeout(ms);
  const until = async (fn, what, ms = 6000) => { const t0 = Date.now(); for (;;) { if (await page.evaluate(fn)) return true; if (Date.now() - t0 > ms) { check(false, 'timed out waiting: ' + what); return false; } await wait(25); } };
  const same = (a, b) => Object.keys(a).every((k) => Math.abs(a[k] - b[k]) < 1e-6);
  const diffIds = (a, b) => Object.keys(a).filter((k) => Math.abs(a[k] - b[k]) >= 1e-6);
  async function shot(name, scales = [100]) {           // sizes are set without clicking, so a held mouse button or the focus is never disturbed
    if (!SHOTS) return;
    await page.evaluate(() => { const t = document.querySelector('.toast-layer'); if (t) t.replaceChildren(); });
    for (const sc of scales) {
      await page.evaluate((v) => GK.setSize(v), String(sc)); await wait(80);
      await page.locator('#frameWrap').screenshot({ path: path.join(SHOT_DIR, String(sc).padStart(3, '0') + '_' + name + '.png') });
    }
    await page.evaluate(() => GK.setSize('100'));
  }
  const fast = () => page.evaluate(() => { GK.pace.k = 0.05; });
  async function talk() { await holdMic((await page.evaluate(() => GK.S.drawer)) === 'kid' ? 'mic-small' : 'mic-big'); }
  async function holdMic(testid, ms = 220) {
    const b = await tid(testid).boundingBox();
    await page.mouse.move(b.x + b.width / 2, b.y + b.height / 2);
    await page.mouse.down(); await wait(ms); await page.mouse.up(); await settle();
  }
  async function say(chipText) { await page.locator('[data-testid="chip-' + chipText + '"]').click(); await until(() => GK.S.kid.phase === 'proposal' || GK.S.kid.phase === 'ready' || GK.S.kid.phase === 'error', 'KID to answer “' + chipText + '”'); }
  async function typeKid(text) { const f = tid('kid-follow'); await f.fill(text); await f.press('Enter'); await until(() => !GK.S.kid.busy && ['proposal', 'ready', 'error'].includes(GK.S.kid.phase), 'KID to answer “' + text + '”'); }
  const rows = () => page.evaluate(() => (GK.S.kid.prop ? GK.S.kid.prop.rows.map((r) => ({ ...r })) : []));
  const row = (rs, id) => rs.find((r) => r.id === id);

  // Layout: text overflow, overlapping or clipped controls, and text smaller than 12 px, among what is really visible
  async function layout(name) {
    const issues = await page.evaluate(() => {
      const fr = document.getElementById('frame'), out = [];
      const tl = fr.querySelector('.toast-layer'); if (tl) tl.replaceChildren();   // toasts are transient
      const vis = (e) => { const r = e.getBoundingClientRect(), cs = getComputedStyle(e); return r.width > 0 && r.height > 0 && cs.visibility !== 'hidden' && cs.display !== 'none'; };
      const k = fr.getBoundingClientRect().width / 1120;
      const topLayer = fr.querySelector('#gk-onb') || fr.querySelector('#gk-modal .modal-layer') || fr.querySelector('#gk-modal > div');
      const scope = topLayer || fr;
      const label = (e) => (e.dataset.testid || e.dataset.key || e.getAttribute('aria-label') || e.textContent || e.tagName).trim().slice(0, 40);
      const onTop = (e) => { const r = e.getBoundingClientRect(); const pts = [[r.left + r.width / 2, r.top + r.height / 2]]; return pts.some(([x, y]) => { const h = document.elementFromPoint(x, y); return h && (h === e || e.contains(h) || h.contains(e)); }); };
      const inScrolledOut = (e) => { let a = e.parentElement; const r = e.getBoundingClientRect(); while (a && a !== fr) { const cs = getComputedStyle(a); if (/(auto|scroll)/.test(cs.overflowY) || /(auto|scroll)/.test(cs.overflowX)) { const ar = a.getBoundingClientRect(); if (r.bottom < ar.top + 1 || r.top > ar.bottom - 1 || r.right < ar.left + 1 || r.left > ar.right - 1) return true; } a = a.parentElement; } return false; };
      // text: overflow and size
      for (const e of scope.querySelectorAll('*')) {
        if (!vis(e) || inScrolledOut(e)) continue;
        const own = [...e.childNodes].some((n) => n.nodeType === 3 && n.textContent.trim());
        if (!own || e.closest('.hl-text')) continue;   // the help line truncates by design; the full text is the control's description
        if (/\b(null|undefined|NaN)\b/.test([...e.childNodes].filter((n) => n.nodeType === 3).map((n) => n.textContent).join(' '))) out.push('stray “null/undefined/NaN” text: ' + label(e));
        const cs = getComputedStyle(e);
        const px = parseFloat(cs.fontSize);
        if (px < 11.95 && !e.closest('svg')) out.push('text below 12 px (' + px + ' px): ' + label(e));
        if (e.tagName !== 'OPTION' && e.tagName !== 'SELECT' && e.scrollWidth > e.clientWidth + 1 && (cs.overflow === 'hidden' || cs.overflowX === 'hidden' || cs.textOverflow === 'ellipsis')) out.push('text cut off: ' + label(e));
        { const r = e.getBoundingClientRect(); let a = e.parentElement;            // text hidden by a container that does not scroll
          while (a && a !== fr.parentElement) { const ac = getComputedStyle(a); if (/(auto|scroll)/.test(ac.overflowX + ac.overflowY)) break;
            if (/(hidden|clip)/.test(ac.overflowX + ac.overflowY)) { const ar = a.getBoundingClientRect(); if (r.bottom > ar.bottom + 1 || r.right > ar.right + 1 || r.top < ar.top - 1 || r.left < ar.left - 1) { out.push('text clipped by its container: ' + label(e)); break; } }
            a = a.parentElement; } }
      }
      // controls: overlap with each other, and clipped by a non-scrolling ancestor
      const ctl = [...scope.querySelectorAll('button, input, select, [role="slider"], [role="switch"], [role="checkbox"]')].filter((e) => vis(e) && !inScrolledOut(e) && onTop(e));
      for (const e of ctl) {
        const r = e.getBoundingClientRect();
        let a = e.parentElement;
        while (a && a !== fr) { const cs = getComputedStyle(a); if (/(auto|scroll)/.test(cs.overflowX + cs.overflowY)) break; if (/(hidden|clip)/.test(cs.overflowX + cs.overflowY)) { const ar = a.getBoundingClientRect(); if (r.left < ar.left - 1 || r.right > ar.right + 1 || r.top < ar.top - 1 || r.bottom > ar.bottom + 1) { out.push('control clipped: ' + label(e)); break; } } a = a.parentElement; }
      }
      for (let i = 0; i < ctl.length; i++) for (let j = i + 1; j < ctl.length; j++) {
        const a = ctl[i], b = ctl[j]; if (a.contains(b) || b.contains(a)) continue;
        const p = a.getBoundingClientRect(), q = b.getBoundingClientRect();
        const ox = Math.min(p.right, q.right) - Math.max(p.left, q.left), oy = Math.min(p.bottom, q.bottom) - Math.max(p.top, q.top);
        if (ox > 2 * k && oy > 2 * k) out.push('controls overlap: ' + label(a) + ' × ' + label(b));
      }
      return [...new Set(out)];
    });
    check(issues.length === 0, 'layout clean: ' + name, issues.join('; '));
  }

  // ------------------------------------------------------------ 0. facts copied from the source
  section('0. Facts copied from the GlitchKid source');
  await page.goto(base);
  await page.evaluate(() => localStorage.clear());
  await page.goto(base);
  await wait(300);
  const facts = await page.evaluate(() => ({ n: GK.PARAMS.length, chain: GK.CHAIN.map((c) => c[0]).join(','), prof: GK.PROFILES.map((p) => p.name), owned: GK.USER_OWNED.length,
    defaults: Object.fromEntries(['comp_ratio', 'echo_mix', 'dim_mix', 'tune_speed', 'lim_ceiling', 'harm_interval', 'echo_div'].map((k) => [k, GK.factoryDefault()[k]])) }));
  check(facts.n === 68, '68 sound parameters (69 with Bypass), as in createLayout', String(facts.n));
  check(facts.chain === '1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17', 'chain strip shows the 17 stages in processing order');
  check(facts.prof.length === 8 && facts.prof.includes('SMOOTH R&B') && facts.prof.includes('DARK & ATMOSPHERIC'), 'the 8 factory profiles are present');
  check(facts.owned === 22, 'a profile load keeps the 22 user-owned parameters');
  check(JSON.stringify(facts.defaults) === JSON.stringify({ comp_ratio: 1, echo_mix: 30, dim_mix: 100, tune_speed: 20, lim_ceiling: -1, harm_interval: 7, echo_div: 4 }), 'spot-checked defaults match createLayout', JSON.stringify(facts.defaults));

  // ------------------------------------------------------------ 1. first launch
  section('1. First launch (onboarding)');
  await fast();
  check(await tid('onb-0').isVisible(), 'a fresh install opens on the welcome step');
  await shot('01_onboarding_welcome', [80, 100, 125]);
  await click('onb-next');
  await sim('signal-none'); await wait(80);
  check((await tid('onb-signal').textContent()).includes('Waiting'), 'signal step waits honestly when nothing plays');
  await shot('02_onboarding_signal_waiting');
  await click('onb-sim-play'); await wait(80);
  check((await tid('onb-signal').textContent()).includes('Vocal coming in'), 'signal step confirms the vocal once it arrives');
  await click('onb-next');
  await click('onb-dry'); check((await S()).bypass === true, 'onboarding DRY switches the real compare (Bypass)');
  await shot('03_onboarding_dry_processed');
  await click('onb-proc'); check((await S()).bypass === false, 'onboarding PROCESSED switches back');
  await click('onb-next');
  check((await tid('onb-path-build').getAttribute('aria-checked')) === 'true', 'path choice defaults to “Help me build my vocal”');
  await shot('04_onboarding_path', [80, 100, 125]);
  await click('onb-next');
  check((await tid('onb-privacy').textContent()).includes('only while you hold'), 'privacy step states push-to-talk and local transcription');
  await shot('05_onboarding_privacy');
  await layout('onboarding, last step');
  await click('onb-next');
  let st = await S();
  check(!st.onb.open && st.onbDone && st.drawer === 'build', 'finishing lands inside the plug-in with Build My Mix open');

  // ------------------------------------------------------------ 2. Build My Mix
  section('2. Build My Mix: listen, direction, review, audition, cancel, apply, undo');
  const s0 = await sound();
  await shot('06_build_listen');
  await click('build-listen');
  await until(() => GK.S.build && GK.S.build.listened, 'listening to finish');
  await shot('07_build_listened');
  await click('build-next');
  await click('dir-SMOOTH R&B');
  await shot('08_build_direction', [80, 100, 125]);
  await click('build-review');
  st = await S();
  check(st.build.step === 3 && st.build.prop.rows.length > 10, 'review lists every proposed change', st.build.prop.rows.length + ' rows');
  check(same(await sound(), s0), 'opening the review changes nothing');
  const bRows = st.build.prop.rows;
  const owned = await page.evaluate(() => GK.USER_OWNED);
  check(bRows.every((r) => !owned.includes(r.id)), 'Build My Mix never proposes the 22 user-owned parameters (tuning, gain, mix, CLEAN, AIR LAB, limiter)');
  check(bRows.some((r) => r.sim), 'the “fitted” changes are labelled as simulated measurements');
  await click('listen-proposal');
  check(!same(await page.evaluate(() => GK.heard()), s0) && same(await sound(), s0), 'audition plays the proposal without changing the sound');
  check((await tid('hear-chip').textContent()).includes('PROPOSAL'), 'the Booth says the proposal is being auditioned, not applied');
  await shot('09_build_review_audition', [80, 100, 125]);
  await click('listen-current');
  await click('chk-echo_mix');
  check((await tid('prop-count').textContent()).includes((bRows.length - 1) + ' ticked'), 'a change can be unticked on its own');
  await layout('Build My Mix review');
  await click('drawer-close');
  check((await S()).build === null && same(await sound(), s0), 'closing the review (×) changes nothing');
  await click('rail-booth');
  await click('build-start');
  await click('build-listen'); await until(() => GK.S.build && GK.S.build.listened, 'listening again');
  await click('build-next'); await click('dir-SMOOTH R&B'); await click('build-review');
  await click('chk-echo_mix');
  const target = await page.evaluate(() => GK.proposalTarget(GK.S.build.prop));
  await click('build-apply');
  const s1 = await sound();
  check(same(s1, target), 'Apply applies exactly the ticked changes');
  check(Math.abs(s1.echo_mix - s0.echo_mix) < 1e-6, 'the unticked change (Echo mix) was left as it was');
  st = await S();
  check(st.preset.name === 'Built: SMOOTH R&B' && st.preset.type === 'built', 'the result is named “Built: SMOOTH R&B” and marked not saved');
  check((await tid('scene-meta').textContent()).includes('Not saved'), 'the top bar says it is not saved');
  await click('undo');
  check(same(await sound(), s0), 'Undo restores the sound from before Build My Mix');
  check((await tid('undo').getAttribute('aria-disabled')) === 'true', 'Undo shows when there is nothing left to undo');
  await click('rail-booth');

  // ------------------------------------------------------------ 3. KID voice states and the proposal
  section('3. KID: push-to-talk states, proposal, audition, follow-ups, manual edit, apply, undo, cancel');
  await page.evaluate(() => { GK.pace.k = 1; GK.phaseLog.length = 0; });
  await page.locator('[data-testid="phrase-big"]').selectOption('0');   // the simulated speech lives in the prototype controls, outside the plug-in
  const before = await sound();
  const mic = await tid('mic-big').boundingBox();
  await page.mouse.move(mic.x + mic.width / 2, mic.y + mic.height / 2);
  await page.mouse.down(); await wait(450);
  check((await S()).kid.phase === 'recording' && await tid('recording').isVisible(), 'holding the mic opens the command mic (Listening… state)');
  await shot('10_kid_recording', [80, 100, 125]);
  await page.mouse.up();
  for (const [ph, nm] of [['transcribing', '11_kid_transcribing'], ['interpreting', '12_kid_interpreting'], ['preparing', '13_kid_preparing']]) {
    const seen = await page.waitForFunction((p) => GK.S.kid.phase === p, ph, { timeout: 5000 }).then(() => true, () => false);
    check(seen, 'the “' + ph + '” state is shown');
    if (seen) await shot(nm);
  }
  await until(() => GK.S.kid.phase === 'proposal', 'the proposal');
  const log = await page.evaluate(() => GK.phaseLog.join('>'));
  check(/recording>transcribing>heard>interpreting>preparing>proposal/.test(log), 'states run in order: recording → transcribing → heard → interpreting → preparing → proposal', log);
  check(same(await sound(), before), 'a ready proposal has changed nothing');
  let r1 = await rows();
  check(row(r1, 'eq_hi_gain') && row(r1, 'eq_hi_gain').to < before.eq_hi_gain && row(r1, 'comp_ratio') && row(r1, 'comp_ratio').to > 1, '“darker and more aggressive” → less air shelf, more compression');
  check(r1.every((r) => !['gain', 'mix', 'lim_on', 'lim_ceiling', 'lim_drive', 'lim_release'].includes(r.id)), 'KID never proposes Gain, Mix or the limiter (allowlist)');
  check((await tid('drawer-body').textContent()).includes('You said'), 'the recognised command is shown');
  await shot('14_kid_proposal', [80, 100, 125]);
  await layout('KID proposal');
  await page.evaluate(() => { GK.pace.k = 0.05; });
  await click('listen-proposal');
  check((await tid('kid-phase').textContent()).includes('Auditioning') && same(await sound(), before), 'audition state: hearing the proposal, sound unchanged');
  await shot('15_kid_auditioning');
  await click('listen-current');
  const d1 = row(r1, 'eq_hi_gain').to - before.eq_hi_gain;
  await say('A little less.');
  let r2 = await rows();
  check((await S()).kid.prop.rev === 2 && Math.abs((row(r2, 'eq_hi_gain').to - before.eq_hi_gain) - 0.7 * d1) < 0.11, '“A little less.” reshapes the same proposal (revision 2, about 70 %)');
  check(r2.some((r) => /changed in revision 2/.test(r.tag)), 'rows changed by the follow-up are marked');
  await shot('16_kid_followup_less');
  await say('Keep the vocal upfront.');
  let r3 = await rows();
  check((await S()).kid.prop.rev === 3 && !row(r3, 'eq_b2_gain'), '“Keep the vocal upfront.” keeps presence (no presence cut in revision 3)');
  await say('Make it darker.');
  let r4 = await rows();
  check(row(r4, 'eq_hi_gain').to < row(r3, 'eq_hi_gain').to, '“Make it darker.” goes darker on the same proposal (revision 4)');
  // manual edit while the proposal waits
  await click('drawer-close');
  st = await S();
  check(st.kid.prop && st.drawer === null && same(await sound(), before), 'closing KID keeps the proposal waiting and changes nothing');
  check(await tid('proposal-waiting').isVisible(), 'the Booth shows that a proposal is waiting');
  await shot('17_booth_proposal_waiting');
  await tid('slider-macro-tone').focus();
  for (let i = 0; i < 3; i++) await page.keyboard.press('ArrowRight');
  await wait(800); await settle();
  const manual = (await sound()).eq_hi_gain;
  await click('review-waiting');
  let r5 = await rows();
  check(row(r5, 'eq_hi_gain').manual && !row(r5, 'eq_hi_gain').sel, 'a control moved by hand is unticked and marked “your value is kept”');
  await shot('18_kid_manual_edit_kept', [80, 100, 125]);
  const pre = await sound();
  const want = await page.evaluate(() => GK.proposalTarget(GK.S.kid.prop));
  await click('prop-apply');
  const after = await sound();
  check(same(after, want) && Math.abs(after.eq_hi_gain - manual) < 1e-6, 'Apply keeps the hand-moved value and applies the ticked rows');
  check((await S()).kid.phase === 'applied', 'applied state, with Undo offered');
  await shot('19_kid_applied');
  await click('kid-undo');
  check(same(await sound(), pre), 'Undo restores the sound from just before Apply');
  // width, then too much width, then cancel
  const b2 = await sound();
  await typeKid('Make it wider but keep it upfront.');
  const w1 = await rows();
  await say('That’s too much width.');
  const w2 = await rows();
  const dw = (rs) => row(rs, 'dbl_width').to - b2.dbl_width;
  check(row(w1, 'dbl_on') && Math.abs(dw(w2) - 0.5 * dw(w1)) < 0.11, '“That’s too much width.” halves the width changes', dw(w1) + ' → ' + dw(w2));
  check(row(w1, 'dim_mix') && row(w1, 'dim_mix').to <= 35, 'keeping it upfront caps Dimension');
  await click('prop-cancel');
  check(!(await S()).kid.prop && same(await sound(), b2), 'Cancel discards the proposal; the sound is unchanged');
  // clean, then character
  await typeKid('Clean it up but keep the character.');
  const c1 = await rows();
  check(row(c1, 'clean_on') && !(row(c1, 'sat_drive') && row(c1, 'sat_drive').to < b2.sat_drive), '“Clean it up but keep the character.” → CLEAN on, character untouched');
  await say('Keep the clean sound but add more character.');
  const c2 = await rows();
  check(row(c2, 'clean_on') && row(c2, 'mut_on') && !row(c2, 'sat_drive'), '“…add more character” keeps the clean-up, adds Mutation, no grit');
  check((await tid('drawer-body').textContent()).includes('NEW WORDS'), 'words KID does not know today are flagged as new');
  await shot('20_kid_clean_then_character');
  await click('prop-cancel');
  await typeKid('Give my ad-libs a distant, psychedelic sound.');
  const a1 = await rows();
  check(row(a1, 'echo_on') && row(a1, 'echo_div') && row(a1, 'echo_div').to === 1 && row(a1, 'mut_on'), 'ad-libs, distant, psychedelic → long echo throws, Mutation, width');
  await shot('21_kid_adlibs_psychedelic', [80, 100, 125]);
  await click('prop-cancel');
  await typeKid('I don’t know what it needs. Help me improve it.');
  const i1 = await rows();
  check(i1.some((r) => r.sim) && row(i1, 'clean_on'), '“Help me improve it” is built from (simulated) measurements and says so');
  await shot('22_kid_improve');
  await click('prop-cancel');
  await typeKid('make it sound like purple');
  check((await S()).kid.phase === 'error' && !(await S()).kid.prop, 'words it does not understand: “Didn’t catch that”, nothing proposed');
  await shot('23_kid_not_understood');
  await page.keyboard.press('Escape');
  check(same(await sound(), b2), 'after all of that, the sound is still exactly as it was');

  // ------------------------------------------------------------ 4. unavailable, offline and error states
  section('4. Unavailable, offline, no-signal, clipping and error states');
  await click('rail-booth');
  await sim('speech-missing');
  await talk();
  check((await S()).kid.phase === 'unavailable' && await tid('modal-speech').isVisible(), 'speech not set up: KID says so and offers set-up (typing still works)');
  await shot('24_speech_not_set_up');
  await click('m-download');
  await until(() => GK.S.sim.speech === 'ready', 'the simulated model download');
  await click('m-download'); // Done
  check((await S()).sim.speech === 'ready', 'after set-up, speech is ready');
  await sim('speech-denied');
  await talk();
  check((await S()).kid.phase === 'unavailable' && (await tid('drawer-body').textContent()).includes('Privacy & Security'), 'microphone blocked: says where to allow it');
  await shot('25_mic_blocked');
  await sim('speech-ready'); await page.keyboard.press('Escape');
  await sim('net-offline');
  check((await tid('kid-status').textContent()).includes('Offline is fine'), 'offline: KID still works (everything is local)');
  await shot('26_offline');
  await sim('speech-missing'); await talk();
  check(await tid('speech-offline').isVisible(), 'offline + speech not set up: the download is honestly unavailable');
  await shot('27_offline_speech_setup');
  await click('m-cancel'); await sim('speech-ready'); await sim('net-online'); await page.keyboard.press('Escape');
  await sim('engine-mastering');
  await talk();
  check((await S()).kid.phase === 'unavailable', 'Mastering mode on: KID is unavailable and says why');
  await shot('28_mastering_mode_kid');
  await page.keyboard.press('Escape');
  await click('rail-shape');
  check(await tid('banner-engine').isVisible(), 'Mastering mode on: workspaces say the vocal controls are not in use');
  await shot('29_mastering_mode_shape');
  await click('rail-booth'); await layout('Booth, Mastering mode'); await click('rail-shape');
  await sim('engine-error'); await click('rail-booth'); await wait(200);
  check((await tid('signal-card').textContent()).includes('Not processing'), 'processing error state is shown (design placeholder)');
  await shot('30_processing_error');
  await sim('engine-ok');
  await sim('signal-none'); await wait(300);
  check((await tid('signal-card').textContent()).includes('No vocal coming in') && (await tid('orb').textContent()).includes('Waiting'), 'no input: honest empty state, the visual waits');
  await shot('31_no_input', [80, 100, 125]);
  await layout('Booth, no input');
  await click('build-start');
  check(await tid('build-nosignal').isVisible() && await tid('build-skip').isVisible(), 'Build My Mix without input: explains, offers “use as designed”');
  await shot('32_build_no_input');
  await click('build-cancel');
  await sim('signal-clipping');
  await until(() => document.querySelector('[data-testid="clip-in"]').textContent === 'CLIP', 'the CLIP indicator', 8000);
  check((await tid('signal-card').textContent()).includes('Input is clipping'), 'clipping input: warns and says what to do');
  await shot('33_input_clipping');
  await layout('Booth, input clipping');
  await sim('signal-vocal');

  // ------------------------------------------------------------ 5. workspaces
  section('5. Workspaces and progressive disclosure');
  await click('rail-shape');
  for (const t of ['clean', 'tune', 'tone', 'dynamics']) {
    await click('tab-' + t);
    await shot('34_shape_' + t, t === 'tune' ? [80, 100, 125] : [100]);
    await layout('SHAPE › ' + t.toUpperCase());
  }
  await click('tab-tune');
  await click('adv-tune');
  check(await page.locator('[data-param="tune_strength"]').isVisible(), 'TUNE › Advanced reveals strength and speed');
  await click('style-Hard');
  let sn = await sound();
  check(sn.tune_on === 1 && sn.tune_strength === 100 && sn.tune_speed === 0, 'tuning style “Hard” writes the verified values (100 %, 0 ms) and switches Tune on');
  await shot('35_shape_tune_advanced');
  await click('tab-tone'); await click('adv-eq');
  check(await page.locator('[data-param="eq_b1_q"]').isVisible(), 'TONE › Advanced reveals the bell widths (Q)');
  await shot('36_shape_tone_advanced');
  await layout('SHAPE › TONE (advanced)');
  await click('chain-13');
  check((await S()).view.ws === 'space', 'clicking a chain chip opens that module (13 Echo → SPACE)');
  await click('sw-echo_on');
  check((await page.locator('[data-testid="chain-13"]').getAttribute('data-on')) === '1', 'the chain strip shows Echo as on');
  await click('adv-echo');
  await shot('37_space', [80, 100, 125]);
  await layout('SPACE (Echo more open)');
  await click('rail-character');
  await click('sw-mut_on'); await click('adv-mutation');
  await shot('38_character', [80, 100, 125]);
  await layout('CHARACTER (Mutation more open)');
  const bm = await sound();
  await click('reset-mutation');
  sn = await sound();
  check(sn.mut_on === 0 && diffIds(bm, sn).every((k) => k.startsWith('mut_')), 'a module Reset touches only that module');
  await click('undo');
  check((await sound()).mut_on === 1, 'Undo brings the module back');
  await page.keyboard.press('d'); await settle();
  check((await S()).bypass === true, 'D switches to DRY'); await page.keyboard.press('d');

  // ------------------------------------------------------------ 6. scenes
  section('6. Scenes: preview, load, placeholders');
  await click('rail-shape'); await click('tab-tune');
  if ((await sound()).tune_on !== 1) await click('sw-tune_on');
  await click('rail-scenes');
  await shot('39_scenes', [80, 100, 125]);
  await layout('SCENES');
  const sp = await sound();
  await click('preview-SMOOTH R&B');
  check(await tid('preview-bar').isVisible() && same(await sound(), sp) && !same(await page.evaluate(() => GK.heard()), sp), 'Preview plays a scene without loading it');
  await shot('40_scenes_preview');
  await click('preview-back');
  check(same(await sound(), sp) && !(await S()).preview, '“Back to my sound” leaves the sound unchanged');
  await click('load-DARK & ATMOSPHERIC');
  sn = await sound();
  const expect = await page.evaluate((from) => GK.profileSound(3, from), sp);
  check(same(sn, expect) && sn.tune_on === 1, 'Load sets the real profile and keeps your tuning (user-owned parameters)');
  const concepts = await page.locator('.scard.concept').count();
  const disabled = await page.locator('.scard.concept button[disabled]').count();
  check(concepts === 8 && disabled === 8, 'the 8 concept scenes are labelled placeholders and cannot be loaded');

  // ------------------------------------------------------------ 7. save, save as, overwrite
  section('7. Saving: name checks, save, update, overwrite confirmation');
  await click('rail-booth');
  await tid('slider-macro-space').focus(); await page.keyboard.press('ArrowRight'); await wait(800); await settle();
  check((await tid('scene-meta').textContent()).includes('Unsaved'), 'a change shows “● Unsaved” in the top bar (text, not colour alone)');
  await click('save');
  await click('m-save');
  check((await tid('save-error').textContent()).includes('Give your sound a name'), 'empty name is refused with a reason');
  await tid('save-name').fill('Bad/Name'); check((await tid('save-error').textContent()).includes('can’t contain'), 'invalid characters are refused');
  await tid('save-name').fill('SMOOTH R&B'); check((await tid('save-error').textContent()).includes('factory name'), 'factory names are protected');
  await shot('41_save_name_error');
  await tid('save-name').fill('Night Drive Lead');
  await click('m-save');
  st = await S();
  check(st.lib.some((p) => p.name === 'Night Drive Lead') && st.preset.type === 'user' && (await tid('scene-meta').textContent()).includes('Saved'), 'Save stores the sound and clears “Unsaved changes”');
  const savedSound = await sound();
  await tid('slider-macro-tone').focus(); await page.keyboard.press('ArrowLeft'); await wait(800); await settle();
  await click('save');
  check(await tid('save-menu').isVisible(), 'saving a changed sound of yours asks: Update or Save as new');
  await shot('42_save_menu');
  await click('menu-update');
  check(await tid('modal-update').isVisible(), 'Update asks before replacing the saved sound');
  await shot('43_save_update_confirm');
  await click('m-replace');
  st = await S();
  check(!same(st.lib.find((p) => p.name === 'Night Drive Lead').sound, savedSound) && same(st.lib.find((p) => p.name === 'Night Drive Lead').sound, st.sound), 'Replace overwrites only after confirmation');
  await tid('slider-macro-tone').focus(); await page.keyboard.press('ArrowLeft'); await wait(800); await settle();
  await click('save'); await click('menu-saveas');
  await tid('save-name').fill('night drive lead');
  check((await tid('save-error').textContent()).includes('will ask before replacing'), 'an existing name is flagged while typing');
  const libBefore = (await S()).lib.find((p) => p.name === 'Night Drive Lead').sound;
  await click('m-save');
  check(await tid('modal-overwrite').isVisible(), 'saving over an existing name asks first');
  await shot('44_save_overwrite_confirm', [80, 100, 125]);
  await click('m-cancel');
  check(same((await S()).lib.find((p) => p.name === 'Night Drive Lead').sound, libBefore), 'Cancel leaves the saved sound untouched');
  await page.keyboard.press('Escape');

  // ------------------------------------------------------------ 8. A / B
  section('8. A/B: empty captures, occupied recalls, nothing overwritten silently');
  check((await tid('ab-A').getAttribute('data-state')) === 'empty' && (await tid('ab-B').getAttribute('data-state')) === 'empty', 'both slots start empty');
  const abs0 = await sound();
  await click('ab-btn-A');
  st = await S();
  check(same(st.ab.A.sound, abs0) && st.ab.active === 'A' && same(await sound(), abs0), 'clicking empty A stores the current sound (and changes nothing you hear)');
  await tid('slider-macro-space').focus(); await page.keyboard.press('ArrowRight'); await wait(800); await settle();
  check((await tid('ab-A').getAttribute('data-state')) === 'changed', 'A shows “changed” when you move away from what it holds');
  const abs1 = await sound();
  await click('ab-btn-B');
  st = await S();
  check(same(st.ab.B.sound, abs1) && st.ab.active === 'B' && same(st.ab.A.sound, abs0), 'clicking empty B stores the new sound; A keeps its own');
  await shot('45_ab_both_stored');
  await click('ab-btn-A');
  check(same(await sound(), abs0) && (await S()).ab.active === 'A', 'clicking occupied A recalls it');
  await tid('slider-macro-tone').focus(); await page.keyboard.press('ArrowRight'); await wait(800); await settle();
  const abs2 = await sound();
  await click('ab-btn-B');
  check(await tid('modal-abswitch').isVisible(), 'switching away from a changed slot asks first');
  await shot('46_ab_switch_confirm', [80, 100, 125]);
  await click('m-cancel');
  check(same(await sound(), abs2) && (await S()).ab.active === 'A', 'Cancel keeps you on A with your changes');
  await click('ab-btn-B'); await click('m-switch');
  st = await S();
  check(same(await sound(), abs1) && same(st.ab.A.sound, abs0), '“Switch without storing” recalls B and leaves A untouched');
  await tid('slider-macro-punch').focus(); await page.keyboard.press('ArrowRight'); await wait(800); await settle();
  await click('ab-store-A');
  check(await tid('modal-abover').isVisible(), 'storing into an occupied, different slot asks first');
  await shot('47_ab_overwrite_confirm');
  await click('m-cancel');
  check(same((await S()).ab.A.sound, abs0), 'Cancel: A is not overwritten');
  await click('ab-store-A'); await click('m-replace');
  st = await S();
  check(same(st.ab.A.sound, st.sound) && st.ab.active === 'A', 'Replace stores the current sound in A');

  // ------------------------------------------------------------ 9. come back later
  section('9. Close and reopen: the sound, the slots and the saved preset come back');
  await click('rail-scenes');
  await click('load-BRIGHT & PRESENT');
  const pre2 = await S();
  await page.locator('[data-sim="reopen"]').click();
  await page.waitForLoadState('load'); await wait(400);
  st = await S();
  check(!st.onb.open, 'the intro does not come back after it was finished');
  check(same(st.sound, pre2.sound) && st.preset.name === pre2.preset.name && same(st.ab.A.sound, pre2.ab.A.sound) && same(st.ab.B.sound, pre2.ab.B.sound), 'the project reopens with its sound, name and A/B slots');
  await click('rail-scenes');
  await click('filter-yours');
  await click('load-Night Drive Lead');
  const lib = st.lib.find((p) => p.name === 'Night Drive Lead');
  check(same(await sound(), lib.sound) && (await S()).preset.type === 'user', 'the saved preset recalls exactly');
  await shot('48_scenes_yours');
  await click('delete-Night Drive Lead');
  check(await tid('modal-delete').isVisible(), 'deleting a saved sound asks first');
  await click('m-cancel');

  // ------------------------------------------------------------ 10. help, keyboard, contrast, small sizes
  section('10. Help, keyboard, contrast, small window');
  await click('rail-booth');
  await page.evaluate(() => { GK.pace.k = 0.05; GK.phaseLog.length = 0; });
  await page.keyboard.down('k'); await wait(150); await page.keyboard.up('k');
  await until(() => GK.S.kid.phase === 'proposal', 'a proposal from holding K');
  check(true, 'holding K talks to KID from anywhere (keyboard push-to-talk)');
  await click('prop-cancel'); await page.keyboard.press('Escape');
  await page.evaluate(() => document.activeElement && document.activeElement.blur());
  let focusIn = false, ring = 'none';
  for (let i = 0; i < 40 && !focusIn; i++) { await page.keyboard.press('Tab'); ({ focusIn, ring } = await page.evaluate(() => { const a = document.activeElement; return { focusIn: !!(a && a.closest('#frame')), ring: a ? getComputedStyle(a).boxShadow : 'none' }; })); }
  check(focusIn && ring !== 'none', 'keyboard focus reaches the plug-in and shows a visible focus ring', ring);
  await click('help');
  check(await tid('modal-help').isVisible(), 'Help explains routing, shortcuts and how KID works');
  await shot('49_help');
  await page.keyboard.press('Escape');
  const pairs = [['#f4f0fb', '#121019', 'text on panel'], ['#c7bed6', '#121019', 'secondary text on panel'], ['#9d93b0', '#121019', 'dim text on panel'], ['#9d93b0', '#211c2c', 'dim text on raised panel'],
    ['#9d93b0', '#07060b', 'dim text on background'], ['#ffbf5f', '#121019', 'warning text'], ['#ffd98e', '#121019', 'SIMULATED badge'], ['#b9f3d9', '#121019', 'ON badge'], ['#ffb3bf', '#24111a', 'error text'],
    ['#ffffff', '#6f3df2', 'primary button text (gradient start)'], ['#ffffff', '#a63ad6', 'primary button text (gradient middle)'], ['#ffffff', '#c42f8f', 'primary button text (gradient end)'], ['#dff9ff', '#13303a', 'DRY pressed']];
  for (const [fg, bg, what] of pairs) { const c = contrast(fg, bg); check(c >= 4.5, 'contrast ≥ 4.5:1 — ' + what, c.toFixed(2) + ':1'); }
  const minPx = await page.evaluate(() => Math.min(...[...document.querySelectorAll('#frame *')].filter((e) => [...e.childNodes].some((n) => n.nodeType === 3 && n.textContent.trim())).map((e) => parseFloat(getComputedStyle(e).fontSize))));
  check(minPx >= 12, 'smallest text in the window is ' + minPx + ' px at 100 % (' + (minPx * 0.8).toFixed(1) + ' px at 80 %)');
  notes.push('At 80 % the smallest text renders at ' + (minPx * 0.8).toFixed(1) + ' px; the current plug-in allows 70 % (' + (minPx * 0.7).toFixed(1) + ' px).');
  await shot('50_booth_final', [80, 100, 125]);
  await layout('Booth');
  check(consoleErrors.length === 0, 'no console errors during the whole run', consoleErrors.join(' | '));

  console.log('\n' + passed + ' passed, ' + failed + ' failed');
  if (notes.length) console.log('Notes:\n  - ' + notes.join('\n  - '));
  if (failures.length) console.log('Failures:\n  - ' + failures.join('\n  - '));
  await browser.close(); srv.close();
  process.exit(failed ? 1 : 0);
})().catch((e) => { console.error(e); process.exit(2); });
