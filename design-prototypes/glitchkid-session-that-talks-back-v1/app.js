/* GlitchKid Vocal Lab — The Session That Talks Back, concept prototype v1 (design only; no audio, nothing here runs in the plug-in).
   Facts copied from the GlitchKid source and checked on 2026-11-04 (reused from the v2 prototype, which was checked the same day):
     - every parameter ID, range, default, skew centre and choice list (Source/PluginProcessor.cpp, createLayout): 68 sound parameters + Bypass,
       and the Mastering Lab's master_* parameters;
     - the fixed processing order (processChunk): 1 Tune, 2 Formant, 3 Harmony, 4 Mutation, 5 Doubler, 6 EQ, 7 CLEAN corrective, 8 Compressor,
       9 CLEAN de-esser, 10 Saturation, 11 AIR LAB, 12 Stutter, 13 Echo, 14 Dimension, 15 Gain, 16 Mix, 17 Output Limiter;
     - the 8 factory profiles and their exact overrides (Source/Presets/Profiles.inc) and the 22 parameters a profile load keeps;
     - KID's allowlist (KidDirector::isAllowed: never Gain, Mix, the Output Limiter or Bypass) and its vocabulary (Source/Kid/KidDirector.cpp);
     - Beat Lock (Source/AutoMode/BeatLock.h) and Reference Match (Source/AutoMode/ReferenceMatch.h) limits.
   Designed here and labelled: the five idea recipes, the six draft worlds, happy accidents (seeded random moves on real controls).
   Simulated and labelled in the UI: the take, every level, meter and measurement, speech recognition, KID's wording.
   New engineering in production (not in the plug-in today): redo, the session name, the notebook, auditioning an idea, KID's three directions.
   No innerHTML: static SVG is parsed with DOMParser, everything else is built from DOM nodes and text. */
'use strict';
(() => {
// ---------------------------------------------------------------- utilities
const $ = (sel, root = document) => root.querySelector(sel);
const clamp = (v, a, b) => Math.min(b, Math.max(a, v));
const copy = (o) => JSON.parse(JSON.stringify(o));
const pace = { k: 1 };                                       // QA runs the simulated waits faster
const later = (ms) => new Promise((r) => setTimeout(r, ms * pace.k));
function el(tag, props, ...kids) {
  const n = document.createElement(tag);
  if (props) for (const [k, v] of Object.entries(props)) {
    if (v == null || v === false) continue;
    if (k === 'class') n.className = v;
    else if (k === 'text') n.textContent = v;
    else if (k === 'style' && typeof v === 'object') Object.assign(n.style, v);
    else if (k.startsWith('on') && typeof v === 'function') n.addEventListener(k.slice(2), v);
    else n.setAttribute(k, v === true ? '' : String(v));
  }
  for (const c of kids.flat(Infinity)) if (c != null && c !== false) n.append(c.nodeType ? c : document.createTextNode(String(c)));
  return n;
}
const svgFrom = (markup) => document.importNode(new DOMParser().parseFromString(markup, 'image/svg+xml').documentElement, true); // static markup only
const ICON = {
  booth: '<circle cx="12" cy="12" r="8.5"/><circle cx="12" cy="12" r="3.5"/><path d="M12 3.5v2M12 18.5v2M3.5 12h2M18.5 12h2"/>',
  shape: '<path d="M3 18c3.5-10 5.5-12.5 9-12.5S17.5 8 21 18"/><path d="M3 18h18"/>',
  character: '<path d="M12 3l2.1 5.4L20 9l-4.4 3.7L17 19l-5-3.1L7 19l1.4-6.3L4 9l5.9-.6z"/>',
  space: '<circle cx="12" cy="12" r="2.2"/><circle cx="12" cy="12" r="6"/><circle cx="12" cy="12" r="9.6" stroke-dasharray="2 3"/>',
  scenes: '<rect x="3" y="4" width="8" height="7" rx="2"/><rect x="13" y="4" width="8" height="7" rx="2"/><rect x="3" y="13" width="8" height="7" rx="2"/><rect x="13" y="13" width="8" height="7" rx="2"/>',
  mic: '<rect x="9" y="3" width="6" height="11" rx="3"/><path d="M5.5 11a6.5 6.5 0 0 0 13 0"/><path d="M12 17.5V21"/>',
  undo: '<path d="M9 14L4 9l5-5"/><path d="M4 9h10a6 6 0 0 1 0 12h-3"/>',
  help: '<circle cx="12" cy="12" r="9"/><path d="M9.6 9.3a2.5 2.5 0 1 1 3.4 2.3c-.7.3-1 .8-1 1.6v.6"/><circle cx="12" cy="17" r=".7" fill="currentColor"/>',
  save: '<path d="M5 4h11l3 3v13H5z"/><path d="M8 4v5h7V4"/><rect x="8" y="13" width="8" height="5" rx="1"/>',
  left: '<path d="M15 6l-6 6 6 6"/>', right: '<path d="M9 6l6 6-6 6"/>', down: '<path d="M6 9l6 6 6-6"/>',
  close: '<path d="M6 6l12 12M18 6L6 18"/>',
};
function icon(name, size) {
  const s = svgFrom('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">' + ICON[name] + '</svg>');
  if (size) { s.setAttribute('width', size); s.setAttribute('height', size); }
  return s;
}

// ---------------------------------------------------------------- the real parameters (68 sound parameters; Bypass is the DRY switch)
const DIV = ['1/2', '1/4', '1/4 dotted', '1/4 triplet', '1/8', '1/8 dotted', '1/8 triplet', '1/16', '1/16 dotted', '1/16 triplet', '1/32'];
const KEYS = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
const SCALES = ['Chromatic', 'Major', 'Natural minor', 'Harmonic minor', 'Major pentatonic', 'Minor pentatonic'];
const INTERVALS = ['Octave DOWN (-12)', 'Fifth DOWN (-7)', 'Major 3rd DOWN (-4)', 'Minor 3rd DOWN (-3)', 'Minor 3rd UP (+3)', 'Major 3rd UP (+4)', 'Fourth UP (+5)', 'Fifth UP (+7)', 'Octave UP (+12)'];
const F = (id, name, mod, min, max, def, unit, x = {}) => ({ id, name, mod, min, max, def, unit, type: 'float', step: 0.1, ...x });
const B = (id, name, mod, def) => ({ id, name, mod, def: def ? 1 : 0, type: 'bool' });
const C = (id, name, mod, choices, def) => ({ id, name, mod, choices, def, type: 'choice' });
const PARAMS = [
  F('gain', 'Gain', 'gain', -60, 12, 0, 'dB'),
  F('eq_lo_freq', 'Low freq', 'eq', 20, 500, 100, 'Hz', { centre: 100, step: 1 }),
  F('eq_lo_gain', 'Low', 'eq', -15, 15, 0, 'dB', { bipolar: true }),
  F('eq_b1_freq', 'Body freq', 'eq', 100, 5000, 400, 'Hz', { centre: 700, step: 1 }),
  F('eq_b1_gain', 'Body', 'eq', -15, 15, 0, 'dB', { bipolar: true }),
  F('eq_b1_q', 'Body Q', 'eq', 0.3, 10, 1, 'q', { centre: 1.5, step: 0.01 }),
  F('eq_b2_freq', 'Presence freq', 'eq', 500, 16000, 3000, 'Hz', { centre: 3000, step: 1 }),
  F('eq_b2_gain', 'Presence', 'eq', -15, 15, 0, 'dB', { bipolar: true }),
  F('eq_b2_q', 'Presence Q', 'eq', 0.3, 10, 1, 'q', { centre: 1.5, step: 0.01 }),
  F('eq_hi_freq', 'Air shelf freq', 'eq', 1000, 18000, 8000, 'Hz', { centre: 6000, step: 1 }),
  F('eq_hi_gain', 'Air shelf', 'eq', -15, 15, 0, 'dB', { bipolar: true }),
  F('comp_thresh', 'Threshold', 'comp', -60, 0, -18, 'dB'),
  F('comp_ratio', 'Ratio', 'comp', 1, 20, 1, ':1', { centre: 4 }),
  F('comp_attack', 'Attack', 'comp', 0.1, 100, 10, 'ms', { centre: 10 }),
  F('comp_release', 'Release', 'comp', 10, 1000, 100, 'ms', { centre: 100, step: 1 }),
  F('comp_knee', 'Knee', 'comp', 0, 24, 6, 'dB'),
  F('comp_makeup', 'Makeup', 'comp', 0, 24, 0, 'dB'),
  F('sat_drive', 'Warmth', 'warmth', 0, 18, 0, 'dB'),
  B('stutter_on', 'Stutter', 'stutter', false),
  C('stutter_rate', 'Slice', 'stutter', ['1/4', '1/8', '1/16', '1/32'], 1),
  F('stutter_mix', 'Stutter mix', 'stutter', 0, 100, 100, '%'),
  B('echo_on', 'Echo', 'echo', false),
  B('echo_sync', 'Sync to tempo', 'echo', true),
  F('echo_time', 'Echo time', 'echo', 1, 2000, 350, 'ms', { centre: 300, step: 1 }),
  C('echo_div', 'Division', 'echo', DIV, 4),
  F('echo_fb', 'Feedback', 'echo', 0, 90, 35, '%'),
  F('echo_tone', 'Echo tone', 'echo', 400, 20000, 6000, 'Hz', { centre: 3000, step: 1 }),
  F('echo_width', 'Echo width', 'echo', 0, 100, 0, '%'),
  F('echo_mix', 'Echo mix', 'echo', 0, 100, 30, '%'),
  B('tune_on', 'Pitch Tune', 'tune', false),
  C('tune_key', 'Key', 'tune', KEYS, 0),
  C('tune_scale', 'Scale', 'tune', SCALES, 0),
  F('tune_strength', 'Strength', 'tune', 0, 100, 100, '%'),
  F('tune_speed', 'Speed', 'tune', 0, 400, 20, 'ms', { centre: 40, step: 1 }),
  F('formant_shift', 'Formant shift', 'formant', -12, 12, 0, 'st', { bipolar: true }),
  B('mut_on', 'Mutation', 'mutation', false),
  F('mut_amount', 'Amount', 'mutation', 0, 100, 40, '%'),
  F('mut_character', 'Character', 'mutation', 0, 100, 50, '%'),
  F('mut_blend', 'Blend', 'mutation', 0, 100, 100, '%'),
  B('harm_on', 'Harmony', 'harmony', false),
  C('harm_interval', 'Interval', 'harmony', INTERVALS, 7),
  F('harm_level', 'Harmony level', 'harmony', -24, 0, -9, 'dB'),
  B('dbl_on', 'Doubler', 'doubler', false),
  F('dbl_amount', 'Doubler amount', 'doubler', 0, 100, 50, '%'),
  F('dbl_width', 'Doubler width', 'doubler', 0, 100, 70, '%'),
  F('dbl_detune', 'Doubler detune', 'doubler', 0, 25, 8, 'ct'),
  F('dbl_timing', 'Doubler timing', 'doubler', 5, 45, 18, 'ms'),
  B('dim_on', 'Dimension', 'dimension', false),
  F('dim_width', 'Dimension width', 'dimension', 0, 100, 60, '%'),
  F('dim_detune', 'Dimension detune', 'dimension', 0, 20, 6, 'ct'),
  F('dim_depth', 'Dimension depth', 'dimension', 0, 100, 50, '%'),
  F('dim_mix', 'Dimension mix', 'dimension', 0, 100, 100, '%'),
  F('mix', 'Mix', 'mix', 0, 100, 100, '%'),
  B('air_on', 'AIR LAB', 'air', false),
  F('air_presence', 'Presence lift', 'air', 0, 100, 30, '%'),
  F('air_air', 'Air lift', 'air', 0, 100, 30, '%'),
  B('air_link', 'Link', 'air', true),
  F('air_mix', 'AIR mix', 'air', 0, 100, 100, '%'),
  F('air_trim', 'AIR trim', 'air', -12, 12, 0, 'dB', { bipolar: true }),
  B('clean_on', 'CLEAN', 'clean', false),
  F('clean_amount', 'Clean amount', 'clean', 0, 100, 50, '%'),
  F('clean_tone', 'Corrective EQ', 'clean', 0, 100, 100, '%'),
  F('clean_res', 'Resonance', 'clean', 0, 100, 100, '%'),
  F('clean_ess', 'De-esser', 'clean', 0, 100, 100, '%'),
  B('lim_on', 'Output Limiter', 'limiter', false),
  F('lim_ceiling', 'Ceiling', 'limiter', -12, -0.1, -1, 'dBFS'),
  F('lim_drive', 'Limiter drive', 'limiter', 0, 24, 0, 'dB'),
  F('lim_release', 'Limiter release', 'limiter', 10, 500, 80, 'ms', { step: 1 }),
];
const PM = Object.fromEntries(PARAMS.map((p) => [p.id, p]));
const IDS = PARAMS.map((p) => p.id);
// A factory profile load keeps these 22 (PresetController / MixEngine); KID never writes Gain, Mix or the limiter (KidDirector::isAllowed).
const USER_OWNED = ['tune_on', 'tune_strength', 'tune_speed', 'tune_key', 'tune_scale', 'gain', 'mix', 'air_on', 'air_presence', 'air_air', 'air_link', 'air_mix', 'air_trim',
  'clean_on', 'clean_amount', 'clean_tone', 'clean_res', 'clean_ess', 'lim_on', 'lim_ceiling', 'lim_drive', 'lim_release'];
const KID_NEVER = ['gain', 'mix', 'lim_on', 'lim_ceiling', 'lim_drive', 'lim_release', 'tune_key', 'tune_scale'];

// workspaces are a UI grouping only; `stage` is the real position in the fixed chain
const WS = { shape: { name: 'SHAPE', line: 'Make the vocal work', color: 'var(--shape)' }, character: { name: 'CHARACTER', line: 'Make it distinctive', color: 'var(--character)' }, space: { name: 'SPACE', line: 'Put it in its world', color: 'var(--space)' } };
const MOD = {
  tune: { name: 'Pitch Tune', ws: 'shape', tab: 'tune', stage: [1] },
  formant: { name: 'Formant', ws: 'character', stage: [2] },
  harmony: { name: 'Harmony', ws: 'character', stage: [3] },
  mutation: { name: 'Mutation', ws: 'character', stage: [4] },
  doubler: { name: 'Doubler', ws: 'space', stage: [5] },
  eq: { name: 'EQ', ws: 'shape', tab: 'tone', stage: [6] },
  clean: { name: 'CLEAN', ws: 'shape', tab: 'clean', stage: [7, 9] },
  comp: { name: 'Compressor', ws: 'shape', tab: 'dynamics', stage: [8] },
  warmth: { name: 'Warmth', ws: 'shape', tab: 'tone', stage: [10] },
  air: { name: 'AIR LAB', ws: 'shape', tab: 'tone', stage: [11] },
  stutter: { name: 'Stutter', ws: 'character', stage: [12] },
  echo: { name: 'Echo', ws: 'space', stage: [13] },
  dimension: { name: 'Dimension', ws: 'space', stage: [14] },
  gain: { name: 'Gain', ws: 'space', stage: [15] },
  mix: { name: 'Mix', ws: 'space', stage: [16] },
  limiter: { name: 'Output Limiter', ws: 'space', stage: [17] },
};
const CHAIN = [[1, 'tune', 'TUNE'], [2, 'formant', 'FORMANT'], [3, 'harmony', 'HARMONY'], [4, 'mutation', 'MUTATE'], [5, 'doubler', 'DOUBLE'], [6, 'eq', 'EQ'],
  [7, 'clean', 'CLEAN'], [8, 'comp', 'COMP'], [9, 'clean', 'DE-ESS'], [10, 'warmth', 'WARMTH'], [11, 'air', 'AIR'], [12, 'stutter', 'STUTTER'], [13, 'echo', 'ECHO'],
  [14, 'dimension', 'DIMENSION'], [15, 'gain', 'GAIN'], [16, 'mix', 'MIX'], [17, 'limiter', 'LIMIT']];
const STAGE_NAME = { 1: 'Pitch Tune', 2: 'Formant', 3: 'Harmony', 4: 'Mutation', 5: 'Doubler', 6: 'EQ', 7: 'CLEAN corrective', 8: 'Compressor', 9: 'CLEAN de-esser', 10: 'Warmth', 11: 'AIR LAB', 12: 'Stutter', 13: 'Echo', 14: 'Dimension', 15: 'Gain', 16: 'Mix', 17: 'Output Limiter' };
function modOn(m, s) {
  switch (m) {
    case 'tune': return s.tune_on >= 0.5;
    case 'formant': return Math.abs(s.formant_shift) >= 0.05;
    case 'harmony': return s.harm_on >= 0.5;
    case 'mutation': return s.mut_on >= 0.5;
    case 'doubler': return s.dbl_on >= 0.5;
    case 'eq': return ['eq_lo_gain', 'eq_b1_gain', 'eq_b2_gain', 'eq_hi_gain'].some((k) => Math.abs(s[k]) >= 0.05);
    case 'clean': return s.clean_on >= 0.5;
    case 'comp': return s.comp_ratio > 1.001;
    case 'warmth': return s.sat_drive >= 0.05;
    case 'air': return s.air_on >= 0.5;
    case 'stutter': return s.stutter_on >= 0.5;
    case 'echo': return s.echo_on >= 0.5;
    case 'dimension': return s.dim_on >= 0.5;
    case 'gain': return Math.abs(s.gain) >= 0.05;
    case 'mix': return s.mix < 99.95;
    case 'limiter': return s.lim_on >= 0.5;
    default: return false;
  }
}
function factoryDefault() { const s = {}; for (const p of PARAMS) s[p.id] = p.def; return s; }
function clampP(id, v) {
  const p = PM[id];
  if (!p) return 0;
  if (!Number.isFinite(v)) return p.def;
  if (p.type === 'bool') return v >= 0.5 ? 1 : 0;
  if (p.type === 'choice') return clamp(Math.round(v), 0, p.choices.length - 1);
  const st = p.step || 0.1;
  return +clamp(Math.round(clamp(v, p.min, p.max) / st) * st, p.min, p.max).toFixed(st < 0.1 ? 2 : st < 1 ? 1 : 0);
}
const skewOf = (p) => (p.centre ? Math.log(0.5) / Math.log((p.centre - p.min) / (p.max - p.min)) : 1);
const toNorm = (p, v) => Math.pow(clamp((v - p.min) / (p.max - p.min), 0, 1), skewOf(p));
const fromNorm = (p, n) => p.min + (p.max - p.min) * Math.pow(clamp(n, 0, 1), 1 / skewOf(p));
function fmt(id, v) {
  const p = PM[id];
  if (p.type === 'bool') return v >= 0.5 ? 'On' : 'Off';
  if (p.type === 'choice') return p.choices[Math.round(v)];
  const z = Math.abs(v) < 0.05 ? 0 : v;
  switch (p.unit) {
    case 'Hz': return v >= 1000 ? (v / 1000).toFixed(v >= 10000 ? 1 : 2).replace(/\.?0+$/, '') + ' kHz' : Math.round(v) + ' Hz';
    case 'dB': return (z > 0 ? '+' : '') + z.toFixed(1) + ' dB';
    case 'dBFS': return z.toFixed(1) + ' dBFS';
    case '%': return Math.round(v) + '%';
    case 'ms': return (v < 10 ? v.toFixed(1) : Math.round(v)) + ' ms';
    case ':1': return v.toFixed(1) + ':1';
    case 'st': return (z > 0 ? '+' : '') + z.toFixed(1) + ' st';
    case 'ct': return v.toFixed(1) + ' ct';
    case 'q': return 'Q ' + v.toFixed(2);
    default: return String(v);
  }
}
const soundEq = (a, b) => IDS.every((k) => Math.abs((a[k] ?? NaN) - (b[k] ?? NaN)) < 1e-6);
function sanitizeSound(x) {                       // saved data is untrusted: every id present, finite and in range
  if (!x || typeof x !== 'object') return null;
  const s = {};
  for (const p of PARAMS) { const v = Number(x[p.id]); if (!Number.isFinite(v)) return null; s[p.id] = clampP(p.id, v); }
  return s;
}

// ---------------------------------------------------------------- factory profiles (exact overrides from Profiles.inc) and concept scenes
const PROFILES = [
  ['CLEAN & NATURAL', 'Transparent, honest, lightly polished', 'eq_b1_freq=300 eq_b1_gain=-1 eq_b2_freq=3500 eq_b2_gain=1 eq_hi_freq=11000 eq_hi_gain=1 comp_thresh=-18 comp_ratio=2.5 comp_attack=15 comp_release=140 comp_knee=10 comp_makeup=3.5'],
  ['MODERN MELODIC RAP', 'Glossy, smooth, a little space', 'eq_lo_gain=1 eq_lo_freq=100 eq_b1_freq=350 eq_b1_gain=-1.5 eq_b2_freq=4000 eq_b2_gain=2 eq_hi_freq=10000 eq_hi_gain=2 comp_thresh=-19 comp_ratio=3 comp_attack=10 comp_release=110 comp_knee=8 comp_makeup=6.5 sat_drive=2.5 echo_on=1 echo_sync=1 echo_div=5 echo_fb=22 echo_tone=4500 echo_width=40 echo_mix=14 dim_on=1 dim_width=50 dim_detune=5 dim_depth=45 dim_mix=22 dbl_on=1 dbl_amount=20 dbl_width=70 dbl_detune=6 dbl_timing=14'],
  ['HARD-TUNED RAP', 'Tight, forward, built for hard tuning', 'eq_lo_gain=-1 eq_lo_freq=100 eq_b1_freq=300 eq_b1_gain=-2 eq_b2_freq=3200 eq_b2_gain=2.5 eq_hi_freq=9500 eq_hi_gain=2 comp_thresh=-21 comp_ratio=4 comp_attack=5 comp_release=80 comp_knee=4 comp_makeup=7.5 sat_drive=3 echo_on=1 echo_sync=1 echo_div=7 echo_fb=12 echo_tone=6000 echo_width=20 echo_mix=8 dbl_on=1 dbl_amount=15 dbl_width=60 dbl_detune=4 dbl_timing=10'],
  ['DARK & ATMOSPHERIC', 'Deep, wide, moody', 'eq_lo_gain=2 eq_lo_freq=90 eq_b1_freq=300 eq_b1_gain=-2.5 eq_b2_freq=2500 eq_b2_gain=-0.5 eq_hi_freq=6500 eq_hi_gain=-3.5 comp_thresh=-20 comp_ratio=3 comp_attack=14 comp_release=160 comp_knee=8 comp_makeup=9 sat_drive=3.5 echo_on=1 echo_sync=1 echo_div=2 echo_fb=40 echo_tone=2200 echo_width=75 echo_mix=28 dim_on=1 dim_width=65 dim_detune=8 dim_depth=55 dim_mix=35 dbl_on=1 dbl_amount=35 dbl_width=85 dbl_detune=8 dbl_timing=20'],
  ['BRIGHT & PRESENT', 'Up front, clear words, airy top', 'eq_lo_gain=-1.5 eq_lo_freq=120 eq_b1_freq=400 eq_b1_gain=-1.5 eq_b2_freq=4000 eq_b2_gain=3 eq_hi_freq=11000 eq_hi_gain=3 comp_thresh=-20 comp_ratio=3.5 comp_attack=6 comp_release=90 comp_knee=5 comp_makeup=6.5 sat_drive=1.5 echo_on=1 echo_sync=1 echo_div=7 echo_fb=10 echo_tone=7000 echo_width=20 echo_mix=7 dbl_on=1 dbl_amount=12 dbl_width=50 dbl_detune=4 dbl_timing=12'],
  ['SMOOTH R&B', 'Warm, soft, gently wide', 'eq_lo_gain=1.5 eq_lo_freq=120 eq_b1_freq=300 eq_b1_gain=-1 eq_b2_freq=3500 eq_b2_gain=1 eq_hi_freq=9500 eq_hi_gain=1.5 comp_thresh=-17 comp_ratio=2.5 comp_attack=18 comp_release=180 comp_knee=12 comp_makeup=4 sat_drive=1.2 echo_on=1 echo_sync=1 echo_div=1 echo_fb=28 echo_tone=3500 echo_width=45 echo_mix=18 dim_on=1 dim_width=45 dim_detune=5 dim_depth=40 dim_mix=22 dbl_on=1 dbl_amount=25 dbl_width=75 dbl_detune=6 dbl_timing=16'],
  ['GRITTY RAP', 'Raw, dense, in your face', 'eq_lo_gain=2 eq_lo_freq=100 eq_b1_freq=450 eq_b1_gain=-1 eq_b2_freq=2800 eq_b2_gain=2.5 eq_hi_freq=8000 eq_hi_gain=0.5 comp_thresh=-24 comp_ratio=5 comp_attack=4 comp_release=70 comp_knee=3 comp_makeup=6.5 sat_drive=7 dbl_on=1 dbl_amount=25 dbl_width=70 dbl_detune=9 dbl_timing=10'],
  ['SPACIOUS LEAD', 'Big, open, a lead that floats', 'eq_lo_gain=0.5 eq_lo_freq=100 eq_b1_freq=350 eq_b1_gain=-1.5 eq_b2_freq=3500 eq_b2_gain=1.5 eq_hi_freq=9500 eq_hi_gain=1.5 comp_thresh=-18 comp_ratio=2.8 comp_attack=12 comp_release=150 comp_knee=9 comp_makeup=7.5 sat_drive=1.8 echo_on=1 echo_sync=1 echo_div=4 echo_fb=32 echo_tone=4000 echo_width=60 echo_mix=24 dim_on=1 dim_width=70 dim_detune=6 dim_depth=60 dim_mix=38 dbl_on=1 dbl_amount=30 dbl_width=90 dbl_detune=7 dbl_timing=18'],
].map(([name, tag, o]) => ({ name, tag, o: Object.fromEntries(o.split(' ').map((kv) => { const [k, v] = kv.split('='); return [k, +v]; })) }));
const CONCEPTS = [['AFTER HOURS', 'Late-night, close, a little smoky'], ['GLASS VOICE', 'Fragile, bright, crystalline edges'], ['FRONT ROW', 'Right in your face, no room'],
  ['NIGHT DRIVE', 'Dark, wide, moving'], ['DISTORTED ANGEL', 'A sweet line over real grit'], ['UNDERGROUND', 'Raw, dense, basement-loud'], ['FLOATING', 'Weightless, washed, wide'], ['RAW ENERGY', 'Unpolished, loud, urgent']];
function profileSound(pi, from) {                  // Factory Default + the profile's overrides; the 22 user-owned parameters stay as they are
  const t = factoryDefault();
  for (const [k, v] of Object.entries(PROFILES[pi].o)) t[k] = clampP(k, v);
  for (const k of USER_OWNED) t[k] = from[k];
  return t;
}
const MACROS = {                                   // MacroEngine.cpp
  tone: { name: 'Tone', lo: 'Darker', hi: 'Brighter', ids: 'Air shelf and Presence (EQ, stage 6)',
    get: (s) => clamp((s.eq_hi_gain + 6) / 12, 0, 1),
    set: (v) => ({ eq_hi_gain: -6 + 12 * v, eq_b2_gain: -1.5 + 3 * v }),
    text: (s) => (Math.abs(s.eq_hi_gain) < 0.05 && Math.abs(s.eq_b2_gain) < 0.05 ? 'Flat' : fmt('eq_hi_gain', s.eq_hi_gain) + ' air') },
  punch: { name: 'Punch', lo: 'Off', hi: 'More control', ids: 'the compressor’s ratio, threshold, attack, release, knee and makeup (stage 8)',
    get: (s) => clamp((s.comp_ratio - 1) / 5, 0, 1),
    set: (v) => { const r = 1 + 5 * v; return { comp_ratio: r, comp_thresh: -12 - 14 * v, comp_attack: 15, comp_release: 110, comp_knee: 6, comp_makeup: Math.min(4, 0.8 * (r - 1)) }; },
    text: (s) => (s.comp_ratio <= 1.001 ? 'Off' : fmt('comp_ratio', s.comp_ratio)) },
  space: { name: 'Space', lo: 'Dry', hi: 'Spacious', ids: 'Echo (13), Dimension (14) and Doubler (5)',
    get: (s) => (s.echo_on >= 0.5 ? clamp(s.echo_mix / 60, 0, 1) : 0),
    set: (v) => { const on = v >= 0.02 ? 1 : 0; return { echo_on: on, dim_on: on, dbl_on: on, echo_mix: 60 * v, echo_fb: 15 + 30 * v, echo_width: 20 + 55 * v, dim_mix: 45 * v, dbl_amount: 40 * v }; },
    text: (s) => (s.echo_on >= 0.5 ? 'Echo ' + Math.round(s.echo_mix) + '%' : 'Dry') },
};
const TUNE_STYLES = [['Natural', 50, 80], ['Light', 75, 35], ['Hard', 100, 0]];

// ---------------------------------------------------------------- reused from the v2 prototype (A/B rules, saving, proposal rows, KID's rules, controls, module cards)

function abClick(k) {
  if (!S.ab[k]) { S.ab[k] = { sound: copy(S.sound), name: S.preset.name }; S.ab.active = k; toast('Stored the current sound in ' + k + '.'); changed(); return; }
  if (S.ab.active === k) { toast(abModified() ? k + ' is playing, with changes that are not stored in ' + k + ' yet. ⤓ stores them.' : k + ' is already playing.'); return; }
  if (abModified()) { openModal({ kind: 'abswitch', from: S.ab.active, to: k }); return; }
  abRecall(k);
}

function abRecall(k) { const before = snap(); S.sound = copy(S.ab[k].sound); S.ab.active = k; stopAudition(true); pushUndo('Switch to ' + k, before); toast('Now playing ' + k + '.'); changed(true); }

function abStore(k, confirmed) {
  if (S.ab[k] && !confirmed && !soundEq(S.ab[k].sound, S.sound)) { openModal({ kind: 'abover', slot: k }); return; }
  S.ab[k] = { sound: copy(S.sound), name: S.preset.name }; S.ab.active = k; toast('Stored the current sound in ' + k + '.'); changed();
}

const BAD_CHARS = /[\\/:*?"<>|]/;

function nameProblem(name) {
  const n = name.trim();
  if (!n) return 'Give your sound a name.';
  if (n.length > 40) return 'Use 40 characters or fewer.';
  if (BAD_CHARS.test(n)) return 'Names can’t contain \\ / : * ? " < > |';
  if (n.toLowerCase() === 'factory default' || PROFILES.some((p) => p.name.toLowerCase() === n.toLowerCase())) return 'That’s a factory name. Choose another, so the factory sound stays as it is.';
  return '';
}

function saveAs(name, overwrite) {
  const n = name.trim();
  const i = S.lib.findIndex((p) => p.name.toLowerCase() === n.toLowerCase());
  if (i >= 0 && !overwrite) return 'exists';
  const entry = { name: n, sound: copy(S.sound) };
  if (i >= 0) S.lib[i] = entry; else S.lib.push(entry);
  S.preset = { name: n, type: 'user', snap: copy(S.sound) };
  persistNow();
  toast('Saved “' + n + '”. It’s in WORLDS › your saved sounds, in every project.');
  changed(true);
  return 'ok';
}

function proposalTarget(pr, live = S.sound) { const t = copy(live); for (const r of pr.rows) if (r.sel && !r.manual) t[r.id] = r.to; return t; }

const selectedCount = (pr) => pr.rows.filter((r) => r.sel && !r.manual).length;

function stageOfId(id) { return id === 'clean_ess' ? 9 : MOD[PM[id].mod].stage[0]; }

function rowsFrom(base, target, why, prev, rev, simIds) {
  const rows = [];
  for (const id of IDS) {
    if (Math.abs(target[id] - base[id]) < 1e-6) continue;
    const p = PM[id], old = prev && prev.find((r) => r.id === id);
    const limited = p.type === 'float' && (Math.abs(target[id] - p.min) < 1e-6 || Math.abs(target[id] - p.max) < 1e-6);
    rows.push({ id, to: target[id], why: why[id] || 'Adjusted to match the direction', sel: old ? old.sel : true, manual: old ? old.manual : false,
      tag: !prev ? '' : !old ? 'new in revision ' + rev : Math.abs(old.to - target[id]) > 1e-6 ? 'changed in revision ' + rev : (old.tag || ''), limited, sim: !!(simIds && simIds.includes(id)) });
  }
  rows.sort((a, b) => stageOfId(a.id) - stageOfId(b.id));
  return rows;
}

const DIMS = ['bright', 'warm', 'presence', 'punch', 'width', 'depth', 'clean', 'exp'];
const zeroDir = () => Object.fromEntries(DIMS.map((d) => [d, 0]));
const OPS = [                                      // vocabulary modelled on KidDirector.cpp; entries marked 'new' are not in KID today
  [/\b(darker|dark|moody|murky)\b/, { bright: -1 }],
  [/\b(brighter|bright|airy|crisp|shiny)\b/, { bright: 1 }],
  [/\b(warmer|warm|fuller|thicker)\b/, { warm: 1 }],
  [/\b(clear|clearer|clarity|upfront|up front|in front|forward|present)\b/, { presence: 1 }],
  [/\b(aggressive|punchy|punch|energy|harder|hype)\b/, { punch: 1 }],
  [/\b(softer|gentler|smoother)\b/, { punch: -0.6 }],
  [/\b(wider|wide|width|stereo|bigger|huge)\b/, { width: 1 }],
  [/\b(narrower|mono|tighter)\b/, { width: -1 }],
  [/\b(distant|far away|far|background)\b/, { bright: -0.5, presence: -1, depth: 1.5 }],
  [/\b(more space|spacious|echo|ambient|atmospheric|reverb)\b/, { depth: 1 }],
  [/\b(drier|closer|intimate)\b/, { depth: -1 }],
  [/\b(clean it up|clean up|cleaner|clean my vocal)\b/, { clean: 1 }],
  [/\b(experimental|weird|alien|strange|robotic|crazy|wild)\b/, { exp: 1 }],
  [/\b(psychedelic|trippy|dreamy)\b/, { exp: 0.6, width: 0.6, depth: 0.8 }, 'new'],
  [/\b(?:more|add) (?:some |more )?character\b/, { exp: 0.5, warm: 0.4 }, 'new'],
];
const LOCKS = [
  [/\bkeep (?:the |my |it |its )?(?:vocal |voice )?(?:upfront|up front|in front|forward)\b|\b(?:but|still|and) (?:keep )?(?:it |the vocal )?(?:upfront|up front|in front)\b/, ['front', 'presence']],
  [/\bkeep (?:the |my )?character\b/, ['char']],
  [/\bkeep (?:the |my |it )?(?:warmth|warm|body)\b/, ['warm']],
  [/\bkeep (?:the |my |it )?clean(?: sound)?\b/, ['clean'], 'new'],
];
function parseCommand(text) {
  let t = ' ' + String(text).toLowerCase().replace(/[’']/g, "'").replace(/[^a-z0-9' -]/g, ' ') + ' ';
  const f = { ops: {}, locks: [], role: '', scale: 0, mag: 1, heard: [], newWords: [], improve: false, build: false };
  for (const [re, ls, isNew] of LOCKS) { const m = t.match(re); if (m) { f.locks.push(...ls); f.heard.push(m[0].trim()); if (isNew) f.newWords.push(m[0].trim()); t = t.replace(re, ' '); } }
  if (/\b(improve|better|what it needs|don't know|dont know|help me|fix it)\b/.test(t)) f.improve = true;
  if (/\bbuild my (?:vocal )?mix\b/.test(t)) f.build = true;
  if (/\bad-? ?libs?\b|\bbacking\b/.test(t)) { f.role = 'adlib'; f.heard.push('ad-libs'); }
  const sm = t.match(/\b(too much|a little less|bit less|less|tone it down|pull it back|not so much|too far|too strong|subtler|subtle)\b/);
  if (sm) { f.scale = /subtle/.test(sm[0]) ? 0.4 : /(little less|bit less)/.test(sm[0]) ? 0.7 : 0.5; f.heard.push(sm[0]); t = t.replace(sm[0], ' '); }
  else if (/\b(a little|slightly|a bit|a touch)\b/.test(t)) f.mag = 0.5;
  if (/\b(very|much|way more|a lot)\b/.test(t)) f.mag = 1.5;
  for (const [re, ops, isNew] of OPS) {
    const m = t.match(re);
    if (m) { for (const [d, v] of Object.entries(ops)) f.ops[d] = (f.ops[d] || 0) + v; f.heard.push(m[0].trim()); if (isNew) f.newWords.push(m[0].trim()); t = t.replace(re, ' '); }
  }
  f.understood = Object.keys(f.ops).length > 0 || f.scale > 0 || f.locks.length > 0 || f.improve || f.build || !!f.role;
  return f;
}
function decide(mem) {
  const b = mem.base, d = mem.dir, L = new Set(mem.locks), adlib = mem.role === 'adlib';
  const t = copy(b), why = {}, notes = [];
  const set = (id, v, w) => { t[id] = clampP(id, v); why[id] = w; };
  const add = (id, dv, w) => set(id, t[id] + dv, w);
  const turnOn = (onId, levelId, start, w) => { if (t[onId] < 0.5) { set(onId, 1, w); set(levelId, start, 'Starts low, so it’s a touch, not a wash'); } };
  const front = L.has('front'), keepPres = L.has('presence'), keepChar = L.has('char'), keepClean = L.has('clean'), keepWarm = L.has('warm');
  if (d.bright) {
    add('eq_hi_gain', 3 * d.bright, d.bright < 0 ? 'Darker: less top end on the air shelf' : 'Brighter: more air on top');
    if (!(d.bright < 0 && keepPres)) add('eq_b2_gain', 0.8 * d.bright, d.bright < 0 ? 'Upper mids eased, so it gets darker rather than duller' : 'A little more presence so the words cut');
    else notes.push('Darker, but the presence band is left where it was, because you asked to keep the vocal upfront.');
    if (t.echo_on >= 0.5) set('echo_tone', b.echo_tone * Math.pow(d.bright < 0 ? 0.8 : 1.2, Math.abs(d.bright)), 'The echo repeats follow the new tone');
  }
  if (d.warm) {
    if (!(d.warm < 0 && keepWarm)) add('eq_lo_gain', 1.5 * d.warm, d.warm > 0 ? 'Warmer: a little more low end' : 'Cooler: less low end');
    if (d.warm > 0 && !keepClean) add('sat_drive', 1.5 * d.warm, 'Warmth: gentle saturation adds harmonics, not volume');
  }
  if (d.presence && !(d.presence < 0 && keepPres)) add('eq_b2_gain', 1.8 * d.presence, d.presence > 0 ? 'More presence: the words come forward' : 'Presence eased: the voice steps back');
  if (d.punch) {
    const r0 = b.comp_ratio > 1.001 ? b.comp_ratio : 1.5;
    set('comp_ratio', r0 + 2 * d.punch, d.punch > 0 ? 'More compression: every word lands with the same strength' : 'Lighter compression: more natural dynamics');
    add('comp_thresh', -5 * d.punch, d.punch > 0 ? 'Lower threshold, so the compressor works on more of the voice' : 'Higher threshold: it only catches the peaks');
    if (d.punch > 0) set('comp_attack', Math.min(b.comp_attack, 6), 'Faster attack: consonants hit harder');
    add('comp_makeup', 1.8 * d.punch, 'Makeup gain, so it stays as loud as before');
    if (d.punch > 0 && !keepClean) add('sat_drive', 2 * d.punch, 'A little grit for aggression');
  }
  if (d.width) {
    if (d.width > 0) { turnOn('dbl_on', 'dbl_amount', 25, 'Doubler on: width from a second, slightly different you'); turnOn('dim_on', 'dim_mix', 20, 'Dimension on: stereo width around the voice'); }
    add('dbl_width', 14 * d.width, d.width > 0 ? 'The doubles spread wider' : 'The doubles pulled in');
    add('dim_width', 14 * d.width, d.width > 0 ? 'Wider Dimension' : 'Narrower Dimension');
    add('dim_mix', (front ? 8 : 16) * d.width, d.width > 0 ? 'More Dimension in the mix' : 'Less Dimension in the mix');
    if (t.echo_on >= 0.5) add('echo_width', 20 * d.width, d.width > 0 ? 'Echo repeats spread across the stereo field' : 'Echo repeats pulled toward the centre');
  }
  if (d.depth) {
    if (d.depth > 0 && t.echo_on < 0.5) { turnOn('echo_on', 'echo_mix', 12, 'Echo on: space around the voice'); set('echo_sync', 1, 'Echo locked to the song’s tempo'); set('echo_div', adlib ? 1 : 4, adlib ? '1/4: long throws for ad-libs' : '1/8: repeats that move with the beat'); }
    add('echo_mix', (adlib ? 14 : 10) * d.depth, d.depth > 0 ? 'More echo: space around you' : 'Less echo: closer and drier');
    add('echo_fb', 8 * d.depth, d.depth > 0 ? 'Longer tail on the echo' : 'Shorter echo tail');
    if (d.depth > 0) { turnOn('dim_on', 'dim_mix', 15, 'Dimension on: a wider room'); add('dim_mix', 10 * d.depth, 'More Dimension: a wider room'); }
  }
  if (d.clean > 0) {
    if (t.clean_on < 0.5) set('clean_on', 1, 'CLEAN on: it learns your voice, then removes rumble, build-up and harshness and tames S sounds');
    set('clean_amount', clamp(50 + 15 * d.clean, 0, 100), 'Balanced clean-up');
    if (keepChar) notes.push('Character kept: the clean-up doesn’t touch saturation or the creative effects.');
  }
  if (d.exp > 0) {
    if (t.mut_on < 0.5) set('mut_on', 1, 'Mutation on: bends the voice into something new');
    set('mut_amount', clamp(15 + 30 * d.exp, 0, 100), 'Mutation amount: noticeable, still a voice');
    set('mut_blend', clamp(30 + 30 * d.exp, 0, 100), 'Blend: part mutated, part you');
    if (d.exp >= 0.6 && t.dim_on >= 0.5) add('dim_depth', 15 * d.exp, 'Deeper Dimension movement: a slow, swirling width');
  }
  if (front) {
    if (t.echo_on >= 0.5 && t.echo_mix > 22) set('echo_mix', 22, 'Echo capped at 22% so the vocal stays upfront');
    if (t.dim_on >= 0.5 && t.dim_mix > 35) set('dim_mix', 35, 'Dimension capped at 35% so the vocal stays upfront');
    if (t.eq_b2_gain < b.eq_b2_gain) { t.eq_b2_gain = b.eq_b2_gain; delete why.eq_b2_gain; }
  }
  if (keepClean && t.sat_drive > b.sat_drive) { t.sat_drive = b.sat_drive; delete why.sat_drive; }
  if (keepClean) notes.push('Clean sound kept: CLEAN stays as it is and no grit is added.');
  if (keepChar && t.sat_drive < b.sat_drive) t.sat_drive = b.sat_drive;
  for (const k of KID_NEVER) t[k] = b[k];          // the allowlist
  return { t, why, notes };
}

function keepFocus(fn) {
  const a = document.activeElement;
  const key = a && a.dataset ? a.dataset.key : null;
  const inp = a && a.tagName === 'INPUT' ? { v: a.value, s: a.selectionStart, e: a.selectionEnd } : null;
  fn();
  if (key && document.activeElement !== a) {
    const n = frame.querySelector('[data-key="' + CSS.escape(key) + '"]');
    if (n) { n.focus({ preventScroll: true }); if (inp && n.tagName === 'INPUT') { n.value = inp.v; try { n.setSelectionRange(inp.s, inp.e); } catch { /* not a text input */ } } }
  }
}

function tip(node, text) {
  node.setAttribute('aria-description', text);
  node.addEventListener('pointerenter', () => setHelp(text));
  node.addEventListener('pointerleave', () => setHelp(''));
  node.addEventListener('focus', () => setHelp(text));
  node.addEventListener('blur', () => setHelp(''));
  return node;
}

function toast(msg, actLabel, act) {
  if (!toastEl) return;
  toastEl.replaceChildren(el('div', { class: 'toast', role: 'status', 'data-testid': 'toast' }, el('span', null, msg), actLabel ? el('button', { 'data-key': 'toast-act', onclick: () => { toastEl.replaceChildren(); act(); } }, actLabel) : null));
  clearTimeout(toastTimer); toastTimer = setTimeout(() => toastEl.replaceChildren(), 5200);
}

function arcPath(cx, cy, r, a0, a1) {
  const pol = (a) => { const t = (a - 90) * Math.PI / 180; return [cx + r * Math.cos(t), cy + r * Math.sin(t)]; };
  const [x0, y0] = pol(a0), [x1, y1] = pol(a1);
  return 'M' + x0.toFixed(2) + ' ' + y0.toFixed(2) + ' A' + r + ' ' + r + ' 0 ' + (Math.abs(a1 - a0) > 180 ? 1 : 0) + ' 1 ' + x1.toFixed(2) + ' ' + y1.toFixed(2);
}

const rangeText = (p) => (p.type === 'float' ? fmt(p.id, p.min) + ' to ' + fmt(p.id, p.max) : '');

function knob(id, o = {}) {
  const p = PM[id], size = o.size || 58;
  const dial = el('div', { class: 'k-dial', role: 'slider', tabindex: 0, 'data-key': 'k-' + id, 'data-param': id, 'aria-label': o.label || p.name, 'aria-valuemin': p.min, 'aria-valuemax': p.max });
  const svg = svgFrom('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="' + size + '" height="' + size + '"><path class="k-track" d="' + arcPath(32, 32, 27, -135, 135) + '"/><path class="k-val" d=""/><circle class="k-cap" cx="32" cy="32" r="19.5"/><line class="k-ptr" x1="32" y1="32" x2="32" y2="17"/></svg>');
  dial.append(svg);
  const val = el('div', { class: 'k-value', 'data-testid': 'val-' + id });
  const wrap = el('div', { class: 'knob', style: { width: Math.max(86, size + 20) + 'px' } }, el('div', { class: 'k-label' }, o.label || p.name), dial, val);
  const arc = svg.querySelector('.k-val'), ptr = svg.querySelector('.k-ptr');
  const sync = () => {
    const v = S.sound[id], a = -135 + 270 * toNorm(p, v);
    const a0 = p.bipolar ? -135 + 270 * toNorm(p, 0) : -135;
    arc.setAttribute('d', Math.abs(a - a0) < 0.5 ? '' : arcPath(32, 32, 27, Math.min(a0, a), Math.max(a0, a)));
    ptr.setAttribute('transform', 'rotate(' + a.toFixed(1) + ' 32 32)');
    val.textContent = fmt(id, v);
    dial.setAttribute('aria-valuenow', v); dial.setAttribute('aria-valuetext', fmt(id, v));
    wrap.classList.toggle('off', !!o.offWhen && o.offWhen());
  };
  const label = 'Change ' + (o.label || p.name);
  dial.addEventListener('pointerdown', (e) => {
    e.preventDefault(); dial.focus(); dial.setPointerCapture(e.pointerId); gestureStart(label);
    const n0 = toNorm(p, S.sound[id]), y0 = e.clientY;
    const mv = (ev) => setParam(id, fromNorm(p, n0 + (y0 - ev.clientY) / (ev.shiftKey ? 700 : 170)));
    const up = () => { dial.removeEventListener('pointermove', mv); dial.removeEventListener('pointerup', up); dial.removeEventListener('pointercancel', up); gestureEnd(0); };
    dial.addEventListener('pointermove', mv); dial.addEventListener('pointerup', up); dial.addEventListener('pointercancel', up);
  });
  dial.addEventListener('dblclick', () => { gestureStart('Reset ' + p.name); setParam(id, p.def); gestureEnd(0); });
  dial.addEventListener('keydown', (e) => {
    const n = toNorm(p, S.sound[id]), st = e.shiftKey ? 0.002 : 0.01;
    const next = { ArrowUp: fromNorm(p, n + st), ArrowRight: fromNorm(p, n + st), ArrowDown: fromNorm(p, n - st), ArrowLeft: fromNorm(p, n - st), PageUp: fromNorm(p, n + 0.1), PageDown: fromNorm(p, n - 0.1), Home: p.min, End: p.max, Delete: p.def, Backspace: p.def }[e.key];
    if (next === undefined) return;
    e.preventDefault(); gestureStart(label); setParam(id, next); gestureEnd(700);
  });
  dial.addEventListener('wheel', (e) => { e.preventDefault(); gestureStart(label); setParam(id, fromNorm(p, toNorm(p, S.sound[id]) - Math.sign(e.deltaY) * 0.01)); gestureEnd(700); }, { passive: false });
  tip(dial, (o.help || p.name) + ' · ' + rangeText(p) + ' · default ' + fmt(id, p.def) + '. Drag, scroll or use the arrow keys; double-click resets.');
  registerCtrl(id, sync); sync();
  return wrap;
}

function hslider(o) {                              // o: { key, name, get():0..1, set(v), text(), lo, hi, help, gesture, local }
  const fill = el('div', { class: 'hs-fill' }), thumb = el('div', { class: 'hs-thumb' });
  const track = el('div', { class: 'hs-track', role: 'slider', tabindex: 0, 'data-key': o.key, 'data-testid': 'slider-' + o.key, 'aria-label': o.name, 'aria-valuemin': 0, 'aria-valuemax': 100 }, el('div', { class: 'hs-rail' }), fill, thumb);
  const val = el('div', { class: 'hs-val', 'data-testid': 'val-' + o.key });
  const wrap = el('div', { class: 'hs' }, el('div', { class: 'hs-top' }, el('span', { class: 'hs-name' }, o.name), val), track, el('div', { class: 'hs-ends' }, el('span', null, o.lo), el('span', null, o.hi)));
  const sync = () => { const v = o.get(); fill.style.left = '0'; fill.style.width = (v * 100) + '%'; thumb.style.left = (v * 100) + '%'; val.textContent = o.text(); track.setAttribute('aria-valuenow', Math.round(v * 100)); track.setAttribute('aria-valuetext', o.text()); };
  const fromX = (x) => { const r = track.getBoundingClientRect(); return clamp((x - r.left) / r.width, 0, 1); };
  track.addEventListener('pointerdown', (e) => {
    e.preventDefault(); track.focus(); track.setPointerCapture(e.pointerId); if (o.gesture) gestureStart(o.gesture); o.set(fromX(e.clientX));
    const mv = (ev) => o.set(fromX(ev.clientX)); const up = () => { track.removeEventListener('pointermove', mv); track.removeEventListener('pointerup', up); if (o.gesture) gestureEnd(0); };
    track.addEventListener('pointermove', mv); track.addEventListener('pointerup', up);
  });
  track.addEventListener('keydown', (e) => { const d = { ArrowRight: 0.02, ArrowUp: 0.02, ArrowLeft: -0.02, ArrowDown: -0.02, PageUp: 0.1, PageDown: -0.1 }[e.key]; if (d == null) return; e.preventDefault(); if (o.gesture) gestureStart(o.gesture); o.set(clamp(o.get() + d, 0, 1)); if (o.gesture) gestureEnd(700); });
  tip(track, o.help);
  if (!o.local) binders.push(sync);
  sync();
  return wrap;
}

function sw(id, o = {}) {
  const p = PM[id];
  const b = el('button', { class: 'sw', role: 'switch', 'data-key': 'sw-' + id, 'data-testid': 'sw-' + id, 'aria-label': o.label || p.name }, el('span', { class: 'sw-track' }, el('span', { class: 'sw-knob' })), el('span', { class: 'sw-txt' }));
  b.addEventListener('click', () => { gestureStart((S.sound[id] >= 0.5 ? 'Turn off ' : 'Turn on ') + p.name); setParam(id, S.sound[id] >= 0.5 ? 0 : 1); gestureEnd(0); });
  const txt = b.querySelector('.sw-txt');
  const sync = () => { const on = S.sound[id] >= 0.5; b.setAttribute('aria-checked', on ? 'true' : 'false'); txt.textContent = (o.text || ((x) => (x ? 'ON' : 'OFF')))(on); };
  tip(b, o.help || p.name + ' on or off.');
  registerCtrl(id, sync); sync();
  return b;
}

function seg(id, o = {}) {
  const p = PM[id];
  const box = el('div', { class: 'seg' + (o.cls ? ' ' + o.cls : ''), role: 'radiogroup', 'aria-label': o.label || p.name });
  p.choices.forEach((c, i) => box.append(el('button', { role: 'radio', 'data-key': 'seg-' + id + '-' + i, 'aria-label': (o.label || p.name) + ' ' + c, onclick: () => { gestureStart('Set ' + p.name); setParam(id, i); gestureEnd(0); } }, c)));
  const sync = () => [...box.children].forEach((b, i) => b.setAttribute('aria-checked', Math.round(S.sound[id]) === i ? 'true' : 'false'));
  tip(box, o.help || p.name);
  registerCtrl(id, sync); sync();
  return box;
}

function sel(id, o = {}) {
  const p = PM[id];
  const s = el('select', { class: 'gk-select', 'data-key': 'sel-' + id, 'aria-label': o.label || p.name }, p.choices.map((c, i) => el('option', { value: i }, c)));
  s.addEventListener('change', () => { gestureStart('Set ' + p.name); setParam(id, +s.value); gestureEnd(0); });
  const sync = () => { s.value = String(Math.round(S.sound[id])); };
  tip(s, o.help || p.name);
  registerCtrl(id, sync); sync();
  return s;
}

function advanced(key, label, body) {
  const open = !!S.view.adv[key];
  return el('div', { class: 'adv' }, el('button', { class: 'adv-btn', 'aria-expanded': open ? 'true' : 'false', 'data-key': 'adv-' + key, 'data-testid': 'adv-' + key, onclick: () => { S.view.adv[key] = !open; changed(true); } }, label || 'Advanced'), open ? el('div', { class: 'adv-body' }, body()) : null);
}

function stageBadge(m) { return tip(el('span', { class: 'badge stage' }, 'Stage ' + MOD[m].stage.join(' & ') + ' of 17'), 'Its fixed position in the processing order. Grouping never changes the order.'); }

function neighbors(stage) {
  const prev = STAGE_NAME[stage - 1], next = STAGE_NAME[stage + 1];
  return el('span', { class: 'neighbors' }, prev ? prev + ' → ' : 'input → ', el('b', null, STAGE_NAME[stage]), next ? ' → ' + next : ' → your mixer');
}

function onBadge(m) { const b = el('span', { class: 'badge' }); bind(() => { const on = modOn(m, S.sound); b.className = 'badge ' + (on ? 'on' : 'off'); b.textContent = on ? '● ON' : '○ OFF'; }); return b; }

function resetModule(m, ids, name) {
  const nm = name || MOD[m].name;
  return tip(el('button', { class: 'btn small ghost', 'data-key': 'reset-' + m, 'data-testid': 'reset-' + m, onclick: () => { gestureStart('Reset ' + nm); setMany(Object.fromEntries(ids.map((k) => [k, PM[k].def]))); gestureEnd(0); toast(nm + ' is back to its defaults.', 'Undo', undo); } }, 'Reset'), 'Put ' + nm + ' back to its defaults. Undo brings your settings back.');
}

function modCard(m, opts, ...kids) {
  const card = el('section', { class: 'card mod', 'aria-label': MOD[m].name, 'data-testid': 'mod-' + m });
  bind(() => card.classList.toggle('is-off', !modOn(m, S.sound)));
  card.append(...[el('div', { class: 'mod-head' }, el('span', { class: 'mod-name' }, opts.title || MOD[m].name), stageBadge(m), opts.sw || onBadge(m), el('span', { class: 'mod-sp' }), opts.neighbors !== false ? neighbors(MOD[m].stage[0]) : null, opts.reset || null),
    opts.desc ? el('p', { class: 'mod-desc' }, opts.desc) : null, el('div', { class: 'mod-body' }, ...kids)].filter(Boolean));
  return card;
}

function teleBox(title, lines) { return [el('div', null, el('span', { class: 'badge sim' }, 'SIMULATED'), ' ', el('b', null, title)), ...lines.map((l) => el('div', null, l))]; }

function shapeClean() {
  const ids = ['clean_on', 'clean_amount', 'clean_tone', 'clean_res', 'clean_ess'];
  const tele = el('div', { class: 'tele', 'data-testid': 'clean-tele' });
  bind(() => tele.replaceChildren(...teleBox('What CLEAN is doing', S.sound.clean_on < 0.5 ? ['Off. Switch it on and CLEAN learns your voice for a few seconds before it corrects anything.'] : S.sim.signal === 'none' ? ['Waiting for your vocal: nothing is learned or corrected without sound.'] : ['Learned 12 s of your voice', 'Rumble: none found', 'Low-mid build-up: 290 Hz, −2.1 dB', 'Resonance: 2.4 kHz, −1.8 dB', 'De-esser: up to −4 dB on S sounds'])));
  return modCard('clean', { sw: sw('clean_on', { help: 'CLEAN on or off. It learns your voice, then corrects.' }), reset: resetModule('clean', ids),
    desc: 'Learns your voice, then removes rumble, low-mid build-up and harsh resonances (stage 7) and tames S sounds (stage 9). One amount; the section trims are under Advanced.' },
  el('div', { class: 'mod-row' }, knob('clean_amount', { size: 84, label: 'Amount', help: 'How much CLEAN corrects', offWhen: () => S.sound.clean_on < 0.5 }), el('div', { style: { flex: 1, minWidth: '260px' } }, tele)),
  advanced('clean', 'Advanced: section trims', () => el('div', { class: 'mod-row' }, knob('clean_tone', { label: 'Corrective EQ' }), knob('clean_res', { label: 'Resonance' }), knob('clean_ess', { label: 'De-esser' }))));
}

function shapeTune() {
  const ids = ['tune_on', 'tune_key', 'tune_scale', 'tune_strength', 'tune_speed'];
  const styles = el('div', { class: 'seg', role: 'radiogroup', 'aria-label': 'Tuning style' }, TUNE_STYLES.map(([n, st, sp]) => {
    const b = tip(el('button', { role: 'radio', 'data-key': 'style-' + n, 'data-testid': 'style-' + n, onclick: () => { gestureStart('Tuning style ' + n); setMany({ tune_strength: st, tune_speed: sp, tune_on: 1 }); gestureEnd(0); } }, n), n + ': strength ' + st + '%, speed ' + sp + ' ms. Switches Pitch Tune on.');
    bind(() => b.setAttribute('aria-checked', Math.abs(S.sound.tune_strength - st) <= 3 && Math.abs(S.sound.tune_speed - sp) <= 5 ? 'true' : 'false'));
    return b;
  }));
  const read = el('div', { class: 'tele' });
  bind(() => read.replaceChildren(...teleBox('What you sing and where it goes', [S.sound.tune_on < 0.5 ? 'Tuning is off: your voice is not pulled to any key.' : S.sim.signal === 'none' ? 'Waiting for your vocal.' : 'Singing A3, 18 cents sharp → pulled to A3 (' + KEYS[S.sound.tune_key] + ' ' + SCALES[S.sound.tune_scale] + ').', 'GlitchKid does not detect your key: choose the key and scale of your beat.'])));
  return modCard('tune', { title: 'Pitch Tune', sw: sw('tune_on'), reset: resetModule('tune', ids), desc: 'Pulls your pitch toward the key and scale you choose. Adds about 31 ms of latency while on (reported to FL Studio, which compensates).' },
    el('div', { class: 'micro', style: { margin: '2px 0 6px' } }, 'Key'), seg('tune_key', { cls: 'keys' }),
    el('div', { class: 'mod-row', style: { marginTop: '12px' } }, el('div', null, el('div', { class: 'micro', style: { marginBottom: '6px' } }, 'Scale'), seg('tune_scale')), el('div', null, el('div', { class: 'micro', style: { marginBottom: '6px' } }, 'Style'), styles)),
    el('div', { style: { marginTop: '12px' } }, read),
    advanced('tune', 'Advanced: strength and speed', () => el('div', { class: 'mod-row' }, knob('tune_strength', { help: 'How far each note is pulled. 100% = all the way' }), knob('tune_speed', { help: 'How fast it glides to the note. 0 ms = the hard-tune sound' }))));
}

function eqCanvas() {
  const cv = el('canvas', { class: 'eq-canvas', width: 1720, height: 220, 'aria-hidden': 'true' });
  const draw = () => {
    const ctx = cv.getContext('2d'), W = 860, H = 110, s = S.sound;
    ctx.setTransform(2, 0, 0, 2, 0, 0); ctx.clearRect(0, 0, W, H);
    const xf = (f) => Math.log(f / 20) / Math.log(1000) * W, yd = (db) => H / 2 - db / 15 * (H / 2 - 8);
    ctx.strokeStyle = '#221d2e'; ctx.lineWidth = 1;
    for (const f of [50, 100, 200, 500, 1000, 2000, 5000, 10000]) { ctx.beginPath(); ctx.moveTo(xf(f), 0); ctx.lineTo(xf(f), H); ctx.stroke(); }
    ctx.beginPath(); ctx.moveTo(0, yd(0)); ctx.lineTo(W, yd(0)); ctx.stroke();
    const resp = (f) => {                            // approximate shapes, not the plug-in's filter maths
      const bell = (g, f0, q) => g * Math.exp(-Math.pow(Math.log2(f / f0), 2) * q * 1.6);
      return s.eq_lo_gain / (1 + Math.pow(f / s.eq_lo_freq, 2)) + s.eq_hi_gain / (1 + Math.pow(s.eq_hi_freq / f, 2)) + bell(s.eq_b1_gain, s.eq_b1_freq, s.eq_b1_q) + bell(s.eq_b2_gain, s.eq_b2_freq, s.eq_b2_q);
    };
    ctx.beginPath();
    for (let x = 0; x <= W; x += 3) { const y = yd(resp(20 * Math.pow(1000, x / W))); if (x) ctx.lineTo(x, y); else ctx.moveTo(x, y); }
    const g = ctx.createLinearGradient(0, 0, W, 0); g.addColorStop(0, '#8f5bff'); g.addColorStop(1, '#ff4fb8');
    ctx.strokeStyle = g; ctx.lineWidth = 2.2; ctx.stroke();
    ctx.fillStyle = '#9d93b0'; ctx.font = '12px Inter, sans-serif';
    for (const [f, t] of [[100, '100 Hz'], [1000, '1 kHz'], [10000, '10 kHz']]) ctx.fillText(t, xf(f) + 3, H - 5);
  };
  binders.push(draw); draw();
  return el('div', null, el('div', { class: 'micro', style: { marginBottom: '6px' } }, 'Response · approximate, drawn from your settings'), cv);
}

function shapeTone() {
  const band = (title, g, f, q) => el('div', { class: 'band' }, el('span', { class: 'bn' }, title), knob(g, { label: 'Gain', size: 52 }), knob(f, { label: 'Freq', size: 44 }), q && S.view.adv.eq ? knob(q, { label: 'Width (Q)', size: 44 }) : null);
  const eqIds = ['eq_lo_freq', 'eq_lo_gain', 'eq_b1_freq', 'eq_b1_gain', 'eq_b1_q', 'eq_b2_freq', 'eq_b2_gain', 'eq_b2_q', 'eq_hi_freq', 'eq_hi_gain'];
  const eq = modCard('eq', { reset: resetModule('eq', eqIds), desc: 'Four bands. At 0 dB a band leaves the voice untouched.' },
    eqCanvas(),
    el('div', { class: 'mod-row', style: { marginTop: '10px', justifyContent: 'space-between' } }, band('LOW', 'eq_lo_gain', 'eq_lo_freq'), band('BODY', 'eq_b1_gain', 'eq_b1_freq', 'eq_b1_q'), band('PRESENCE', 'eq_b2_gain', 'eq_b2_freq', 'eq_b2_q'), band('AIR SHELF', 'eq_hi_gain', 'eq_hi_freq')),
    advanced('eq', 'Advanced: bell widths (Q)', () => el('div', { class: 'dim' }, 'Width (Q) knobs now show on BODY and PRESENCE.')));
  const warm = modCard('warmth', { reset: resetModule('warmth', ['sat_drive']), neighbors: false, desc: 'Gentle saturation (oversampled). 0 dB = clean.' }, knob('sat_drive', { label: 'Drive', size: 72 }));
  const airIds = ['air_on', 'air_presence', 'air_air', 'air_link', 'air_mix', 'air_trim'];
  const air = modCard('air', { sw: sw('air_on'), reset: resetModule('air', airIds), neighbors: false, desc: 'Presence and air that ease off on S sounds. Zero latency.' },
    el('div', { class: 'mod-row' }, knob('air_presence', { offWhen: () => S.sound.air_on < 0.5 }), knob('air_air', { offWhen: () => S.sound.air_on < 0.5 }), el('div', null, el('div', { class: 'micro', style: { marginBottom: '6px' } }, 'Link'), sw('air_link', { text: (x) => (x ? 'LINKED' : 'SEPARATE'), help: 'Link moves Presence lift and Air lift together (the plug-in keeps its existing link behaviour).' }))),
    advanced('air', 'Advanced: mix and trim', () => el('div', { class: 'mod-row' }, knob('air_mix'), knob('air_trim'))));
  return el('div', { style: { display: 'flex', flexDirection: 'column', gap: '12px' } }, eq, el('div', { class: 'grid2' }, warm, air));
}

function shapeDynamics() {
  const ids = ['comp_thresh', 'comp_ratio', 'comp_attack', 'comp_release', 'comp_knee', 'comp_makeup'];
  const gr = el('div', { class: 'tele' });
  bind(() => gr.replaceChildren(...teleBox('Gain reduction', [S.sound.comp_ratio <= 1.001 ? 'Ratio 1:1 — the compressor is not compressing.' : S.sim.signal === 'none' ? 'Waiting for your vocal.' : 'About −' + Math.min(12, (S.sound.comp_ratio - 1) * 1.6 + Math.max(0, -S.sound.comp_thresh - 12) / 4).toFixed(1) + ' dB on loud words.'])));
  return modCard('comp', { reset: resetModule('comp', ids), desc: 'Evens out loud and quiet words. Ratio 1:1 = no compression (the default).' },
    el('div', { class: 'mod-row' }, knob('comp_thresh', { size: 84 }), knob('comp_ratio', { size: 84 }), el('div', { style: { flex: 1, minWidth: '240px' } }, gr)),
    advanced('comp', 'Advanced: attack, release, knee, makeup', () => el('div', { class: 'mod-row' }, knob('comp_attack'), knob('comp_release'), knob('comp_knee'), knob('comp_makeup'))));
}

function labCard(m, cls, opts, primary, more) {
  const card = el('section', { class: 'card lab-card ' + cls, 'aria-label': MOD[m].name, 'data-testid': 'mod-' + m });
  bind(() => card.classList.toggle('lit', modOn(m, S.sound)));
  card.append(...[el('div', { class: 'mod-head' }, el('span', { class: 'mod-name' }, MOD[m].name), stageBadge(m), opts.sw || onBadge(m), el('span', { class: 'mod-sp' }), opts.reset),
    el('p', { class: 'mod-desc' }, opts.desc), el('div', { class: 'mod-row' }, ...primary), more ? advanced(m, 'More', more) : null].filter(Boolean));
  return card;
}

function outputCard() {
  return el('section', { class: 'card lab-card space-card', 'aria-label': 'Output', 'data-testid': 'mod-output' },
    el('div', { class: 'mod-head' }, el('span', { class: 'mod-name' }, 'Output'), el('span', { class: 'badge stage' }, 'Stages 15 · 16 · 17'), el('span', { class: 'mod-sp' }), resetModule('limiter', ['gain', 'mix', 'lim_on', 'lim_ceiling', 'lim_drive', 'lim_release'], 'Output')),
    el('p', { class: 'mod-desc' }, 'Gain (15), dry/wet Mix (16), then the Output Limiter (17): the last thing before your mixer. KID never changes these.'),
    el('div', { class: 'mod-row' }, knob('gain'), knob('mix', { help: 'Dry/wet blend. 100% = fully processed' }), el('div', { style: { display: 'flex', flexDirection: 'column', gap: '8px' } }, el('div', { class: 'micro' }, 'Limiter'), sw('lim_on')), knob('lim_ceiling', { offWhen: () => S.sound.lim_on < 0.5 })),
    advanced('limiter', 'More', () => el('div', { class: 'mod-row' }, knob('lim_drive'), knob('lim_release'))));
}

// ================================================================ THE SESSION THAT TALKS BACK — new for this concept
// ---------------------------------------------------------------- concept-level grouping (the chain and the parameters above are unchanged)
WS.lab = { name: 'LAB', line: 'Experimental', color: 'var(--pink)' };
const TAB_OF = { tune: 'tune', eq: 'tone', warmth: 'tone', air: 'tone', clean: 'clean', comp: 'dynamics', doubler: 'space', echo: 'space', dimension: 'space', gain: 'output', mix: 'output', limiter: 'output' };
for (const m of ['formant', 'harmony', 'mutation', 'stutter']) MOD[m].ws = 'lab';
for (const m of Object.keys(TAB_OF)) MOD[m].tab = TAB_OF[m];
Object.assign(ICON, {
  session: '<rect x="3.5" y="4.5" width="17" height="12" rx="2"/><path d="M8 20.5h8M12 16.5v4"/><path d="M6.5 11h2l1.5-3 2 6 1.5-3h4"/>',
  lab: '<path d="M9 3h6M10 3v6L5 19a1.5 1.5 0 0 0 1.3 2h11.4A1.5 1.5 0 0 0 19 19l-5-10V3"/><path d="M7.5 15h9"/>',
  worlds: '<rect x="3" y="6" width="18" height="12" rx="2"/><circle cx="8.5" cy="12" r="2"/><circle cx="15.5" cy="12" r="2"/><path d="M8.5 14h7"/>',
  tools: '<path d="M14.5 6.5a4 4 0 0 0-5.3 5.3L4 17l3 3 5.2-5.2a4 4 0 0 0 5.3-5.3l-2.3 2.3-2.4-.6-.6-2.4z"/>',
  master: '<path d="M6 4v16M12 4v16M18 4v16"/><rect x="4" y="13" width="4" height="3" rx="1"/><rect x="10" y="7" width="4" height="3" rx="1"/><rect x="16" y="10" width="4" height="3" rx="1"/>',
  redo: '<path d="M15 14l5-5-5-5"/><path d="M20 9H10a6 6 0 0 0 0 12h3"/>',
  dice: '<rect x="4" y="4" width="16" height="16" rx="3"/><circle cx="9" cy="9" r="1.2" fill="currentColor"/><circle cx="15" cy="15" r="1.2" fill="currentColor"/><circle cx="15" cy="9" r="1.2" fill="currentColor"/><circle cx="9" cy="15" r="1.2" fill="currentColor"/><circle cx="12" cy="12" r="1.2" fill="currentColor"/>',
  kidface: '<circle cx="12" cy="13" r="6.5"/><path d="M5 12a7 7 0 0 1 14 0"/><circle cx="10" cy="12.5" r=".9" fill="currentColor"/><circle cx="14" cy="12.5" r=".9" fill="currentColor"/>',
  play: '<path d="M8 5.5v13l10-6.5z" fill="currentColor"/>',
  stop: '<rect x="7" y="7" width="10" height="10" rx="1.5" fill="currentColor"/>',
  file: '<path d="M6 3.5h8l4 4v13H6z"/><path d="M14 3.5v4h4"/><path d="M9 14.5v2M11.5 12v4.5M14 13.5v3"/>',
});
// Words this concept adds to KID's rules ('new' = not in KidDirector's lexicon today), and a lock KidDirector already has (Source/Kid/KidDirector.cpp, keeps[])
OPS.push([/\b(rough|rougher|gritty|grittier|dirty|dirtier|distorted|crunchy)\b/, { warm: 0.6, punch: 0.6 }, 'new'], [/\b(unstable|wobbly|wobble|broken|lo-?fi)\b/, { exp: 0.7 }, 'new']);
LOCKS.push([/\bkeep (?:the |my |its |it )?(?:presence|clarity|words clear)\b/, ['presence']]);
const KID_FACE = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 36 36" class="kid-face" aria-hidden="true"><path d="M7 17a11 11 0 0 1 22 0" fill="none" stroke="#2a1740" stroke-width="2.4" stroke-linecap="round"/><rect x="4" y="16" width="6" height="9" rx="3" fill="#2a1740"/><rect x="26" y="16" width="6" height="9" rx="3" fill="#2a1740"/><circle cx="18" cy="21" r="9" fill="#fff" stroke="#2a1740" stroke-width="2"/><circle cx="15" cy="20" r="1.5" fill="#2a1740"/><circle cx="21" cy="20" r="1.5" fill="#2a1740"/><path d="M15 24.5c1.8 1.4 4.2 1.4 6 0" fill="none" stroke="#2a1740" stroke-width="1.6" stroke-linecap="round"/></svg>';

// Mastering Lab parameters (createLayout: master_on is not automatable; the vocal chain is not in use while it is on)
const MASTER_STYLES = ['Clean & Balanced', 'Warm & Smooth', 'Loud & Punchy', 'Wide & Atmospheric'];
const MP = {
  master_intensity: { name: 'Intensity', min: 0, max: 100, def: 60, unit: '%', step: 1 },
  master_loudness: { name: 'Loudness', min: 0, max: 10, def: 3, unit: 'dB', step: 0.1 },
  master_tone: { name: 'Tone', min: -100, max: 100, def: 0, unit: '', step: 1 },
  master_width: { name: 'Width', min: 0, max: 200, def: 100, unit: '%', step: 1 },
  master_output: { name: 'Output', min: -12, max: 0, def: 0, unit: 'dB', step: 0.1 },
};

// Draft worlds: VALID parameter states designed in this prototype (every value inside its real range), NOT factory presets, NOT listening-tested.
// Like a factory profile they set Factory Default + these values and keep the 22 user-owned parameters.
const WORLDS = [
  ['dreamscape', 'Dreamscape', '#9d7bff', 'Soft, floating, wide. The words drift.', 'eq_hi_gain=2 eq_hi_freq=9000 eq_b1_gain=-1.5 eq_b1_freq=350 comp_ratio=2.2 comp_thresh=-20 comp_attack=18 comp_release=180 comp_makeup=2.5 sat_drive=1 echo_on=1 echo_sync=1 echo_div=2 echo_fb=45 echo_tone=3500 echo_width=70 echo_mix=26 dim_on=1 dim_width=80 dim_depth=70 dim_mix=40 dbl_on=1 dbl_amount=25 dbl_width=85'],
  ['neon', 'Neon City', '#ff4fb8', 'Bright, punchy, a tight slap of echo.', 'eq_b2_gain=2.5 eq_b2_freq=3500 eq_hi_gain=3 eq_hi_freq=10000 eq_b1_gain=-1.5 comp_ratio=4 comp_thresh=-22 comp_attack=5 comp_release=90 comp_makeup=6 sat_drive=3 echo_on=1 echo_sync=1 echo_div=7 echo_fb=15 echo_tone=7000 echo_width=40 echo_mix=12 dim_on=1 dim_mix=18 dbl_on=1 dbl_amount=15 dbl_width=60'],
  ['radio', 'Radio Static', '#ffc163', 'Band-limited, gritty, a little unstable.', 'eq_lo_gain=-12 eq_lo_freq=350 eq_hi_gain=-12 eq_hi_freq=3500 eq_b2_gain=6 eq_b2_freq=1800 eq_b2_q=1.2 sat_drive=12 comp_ratio=5 comp_thresh=-24 comp_attack=4 comp_makeup=4 mut_on=1 mut_amount=20 mut_character=70 mut_blend=30'],
  ['underwater', 'Underwater', '#5ee3ff', 'Muffled, swimming, slow movement.', 'eq_hi_gain=-10 eq_hi_freq=2500 eq_b1_gain=2 eq_b1_freq=500 formant_shift=-1.5 dim_on=1 dim_width=70 dim_detune=14 dim_depth=85 dim_mix=55 echo_on=1 echo_sync=1 echo_div=5 echo_fb=40 echo_tone=1500 echo_mix=30'],
  ['chrome', 'Chrome Room', '#cfd8e6', 'Metallic shine, robotic edges.', 'eq_b2_gain=4 eq_b2_freq=5000 eq_b2_q=3 eq_hi_gain=2 mut_on=1 mut_amount=45 mut_character=85 mut_blend=55 dim_on=1 dim_mix=25 echo_on=1 echo_sync=1 echo_div=9 echo_fb=30 echo_tone=9000 echo_mix=15'],
  ['afterhours', 'Afterhours', '#b47cff', 'Dark, close, smoky late night.', 'eq_hi_gain=-3 eq_hi_freq=7000 eq_lo_gain=2 eq_lo_freq=110 eq_b1_gain=-1.5 sat_drive=4 comp_ratio=3 comp_thresh=-20 comp_attack=20 comp_release=170 comp_makeup=4 echo_on=1 echo_sync=1 echo_div=1 echo_fb=30 echo_tone=2500 echo_mix=18 dim_on=1 dim_mix=20 dbl_on=1 dbl_amount=20 dbl_width=60'],
].map(([id, name, hue, tag, o]) => ({ id, name, hue, tag, o: Object.fromEntries(o.split(' ').map((kv) => { const [k, v] = kv.split('='); return [k, +v]; })) }));
function worldSound(w, from) { const t = factoryDefault(); for (const [k, v] of Object.entries(w.o)) t[k] = clampP(k, v); for (const k of USER_OWNED) t[k] = from[k]; return t; }

// Idea recipes: each is a set of REAL parameter values, relative to the sound you have now
const on = (s, k) => s[k] >= 0.5;
const IDEAS = [
  { id: 'ghost', name: 'Ghost Double', desc: 'A faint second you, a little late and slightly off, just behind the lead.', intent: 'Thicker, without sounding doubled',
    make: () => ({ dbl_on: 1, dbl_amount: 18, dbl_width: 60, dbl_detune: 12, dbl_timing: 30 }),
    why: { dbl_on: 'A second voice on (Doubler)', dbl_amount: 'Kept faint, so it feels like a ghost', dbl_width: 'Just off-centre', dbl_detune: 'Slightly off-pitch', dbl_timing: 'A little late' } },
  { id: 'wide', name: 'Wide Dream', desc: 'The edges drift outward; the lead stays in the middle and clear.', intent: 'Wider, lead stays present',
    make: (s) => ({ dim_on: 1, dim_width: 85, dim_depth: 60, dim_mix: on(s, 'dim_on') ? Math.max(s.dim_mix, 35) : 35, dbl_on: 1, dbl_amount: on(s, 'dbl_on') ? Math.max(s.dbl_amount, 22) : 22, dbl_width: 90, ...(on(s, 'echo_on') ? { echo_width: 70 } : {}), eq_b2_gain: s.eq_b2_gain + 1 }),
    why: { dim_on: 'Dimension on: width around the voice', dim_width: 'Wide', dim_depth: 'Slow movement', dim_mix: 'Enough to feel, not a wash', dbl_on: 'Doubles at the edges', dbl_amount: 'Light doubles', dbl_width: 'Pushed to the sides', echo_width: 'Echo repeats spread out', eq_b2_gain: 'A little presence, so the lead stays in front' } },
  { id: 'radio', name: 'Broken Radio', desc: 'Squeezed through a small speaker, gritty and a bit unstable.', intent: 'Rough, lo-fi texture',
    make: (s) => ({ eq_lo_gain: -12, eq_lo_freq: 350, eq_hi_gain: -12, eq_hi_freq: 3500, eq_b2_gain: 5, eq_b2_freq: 1800, sat_drive: Math.max(s.sat_drive, 11), mut_on: 1, mut_amount: 18, mut_character: 75, mut_blend: 30 }),
    why: { eq_lo_gain: 'Low end cut: small-speaker sound', eq_lo_freq: 'Cut starts at 350 Hz', eq_hi_gain: 'Top end cut', eq_hi_freq: 'Above 3.5 kHz', eq_b2_gain: 'A honky mid bump', eq_b2_freq: 'Around 1.8 kHz', sat_drive: 'Real grit', mut_on: 'Mutation on: instability', mut_amount: 'Just a wobble', mut_character: 'Toward the robotic side', mut_blend: 'Mostly you' } },
  { id: 'close', name: 'Close & Warm', desc: 'Right next to the mic: warmer, softer top, less room.', intent: 'Intimate',
    make: (s) => ({ eq_lo_gain: s.eq_lo_gain + 1.5, eq_hi_gain: s.eq_hi_gain - 1.5, sat_drive: Math.max(s.sat_drive, 2.5), comp_ratio: Math.max(s.comp_ratio, 2.5), comp_attack: 18, comp_release: 160, ...(on(s, 'echo_on') ? { echo_mix: Math.min(s.echo_mix, 8) } : {}), ...(on(s, 'dim_on') ? { dim_mix: Math.min(s.dim_mix, 10) } : {}), ...(on(s, 'dbl_on') ? { dbl_amount: Math.min(s.dbl_amount, 10) } : {}) }),
    why: { eq_lo_gain: 'A little more body', eq_hi_gain: 'A softer top', sat_drive: 'Warmth', comp_ratio: 'Gentle, even level', comp_attack: 'Slow attack keeps it natural', comp_release: 'Smooth release', echo_mix: 'Less room', dim_mix: 'Less width', dbl_amount: 'Fewer doubles' } },
  { id: 'after', name: 'Afterimage', desc: 'The last words hang in the air after you stop.', intent: 'Lingering, distant space',
    make: (s) => ({ echo_on: 1, echo_sync: 1, echo_div: 2, echo_fb: 55, echo_tone: 2500, echo_width: 60, echo_mix: 28, dim_on: 1, dim_mix: on(s, 'dim_on') ? Math.max(s.dim_mix, 25) : 25 }),
    why: { echo_on: 'Echo on (GlitchKid has no reverb; this is its space)', echo_sync: 'Locked to the song tempo', echo_div: '1/4 dotted: long, lazy repeats', echo_fb: 'A long tail', echo_tone: 'Darker repeats sit behind you', echo_width: 'Repeats spread wide', echo_mix: 'Present, not in front', dim_on: 'A little room around it', dim_mix: 'Light' } },
];
const IDEA = Object.fromEntries(IDEAS.map((r) => [r.id, r]));

// Happy accidents: a seeded random mix of real controls (deterministic per session, so it can be replayed and tested)
let seed = 20261104;
const rnd = () => { seed = (seed * 1664525 + 1013904223) >>> 0; return seed / 4294967296; };
const pick = (a) => a[Math.floor(rnd() * a.length)];
const ACCIDENT_NAMES = ['Tape Ghost', 'Basement Choir', 'Wrong Station', 'Glass Ceiling', 'Midnight Smear', 'Paper Moon', 'Static Lullaby', 'Broken Halo', 'Pink Fog', 'Sleepwalk', 'Night Bus', 'Warm Static'];
const MOVES = [
  ['Formant', () => ({ formant_shift: Math.round((rnd() * 6 - 3) * 10) / 10 || 1.5 })],
  ['Mutation', () => ({ mut_on: 1, mut_amount: 10 + rnd() * 35, mut_character: rnd() * 100, mut_blend: 20 + rnd() * 40 })],
  ['Harmony', () => ({ harm_on: 1, harm_interval: pick([0, 1, 4, 5, 7, 8]), harm_level: -18 + rnd() * 8 })],
  ['Stutter', () => ({ stutter_on: 1, stutter_rate: pick([1, 2, 3]), stutter_mix: 15 + rnd() * 25 })],
  ['Echo', () => ({ echo_on: 1, echo_sync: 1, echo_div: pick([1, 2, 4, 5, 6]), echo_fb: 20 + rnd() * 40, echo_mix: 10 + rnd() * 25, echo_tone: 1500 + rnd() * 6000 })],
  ['Dimension', () => ({ dim_on: 1, dim_mix: 10 + rnd() * 35, dim_depth: 30 + rnd() * 60, dim_detune: 3 + rnd() * 12 })],
  ['Warmth', () => ({ sat_drive: 2 + rnd() * 6 })],
  ['EQ tilt', () => ({ eq_hi_gain: -6 + rnd() * 9, eq_b1_gain: -3 + rnd() * 4 })],
];
function rollAccident() {
  const moves = [], used = new Set();
  while (moves.length < 3) { const m = pick(MOVES); if (!used.has(m[0])) { used.add(m[0]); moves.push(m); } }
  const set = {}, why = {};
  for (const [label, f] of moves) for (const [k, v] of Object.entries(f())) { set[k] = clampP(k, v); why[k] = 'A random move on ' + label; }
  const taken = new Set(S.desk.map((c) => c.name));
  const name = ACCIDENT_NAMES.find((n) => !taken.has(n) && rnd() < 0.6) || ACCIDENT_NAMES.find((n) => !taken.has(n)) || 'Accident ' + (S.desk.length + 1);
  const card = { id: 'a' + Date.now().toString(36) + Math.floor(rnd() * 1e4), kind: 'accident', name, desc: 'A random mix of real controls: ' + moves.map((m) => m[0]).join(', ') + '. Might be great, might be nothing.', intent: 'Happy accident', set, why, state: 'new' };
  S.desk.unshift(card);
  toast('Rolled “' + name + '”. Hear it before you decide.');
  changed(true);
  return card;
}

// ---------------------------------------------------------------- state and persistence
const LS = 'gk-stb-session', LS_LIB = 'gk-stb-library';
const store = {
  get(k) { try { return JSON.parse(localStorage.getItem(k)); } catch { return null; } },
  set(k, v) { try { localStorage.setItem(k, JSON.stringify(v)); } catch { /* private window: the prototype still works, it forgets on reload */ } },
  del(k) { try { localStorage.removeItem(k); } catch { /* ignore */ } },
};
const prefersReduced = !!(window.matchMedia && matchMedia('(prefers-reduced-motion: reduce)').matches);
const freshDesk = () => IDEAS.map((r) => ({ id: r.id, kind: 'idea', name: r.name, desc: r.desc, intent: r.intent, state: 'new' }));
const S = {
  sound: factoryDefault(), bypass: false,
  preset: { name: 'Factory Default', type: 'default', snap: factoryDefault() },
  session: { name: 'late night session' },
  ab: { A: null, B: null, active: null },
  undo: [], redo: [],
  notebook: [], activeNote: null,
  desk: freshDesk(),
  audition: null, review: null,
  view: { area: 'session', shapeTab: 'clean', toolTab: 'beat', rtab: 'kid', adv: {}, editName: false },
  kid: { phase: 'ready', reply: null, event: null, dismissed: [], mem: null, busy: false, holdStart: 0, quiet: false, setNo: 0, lastWords: '' },
  build: null,
  beat: { busy: false, progress: 0, analysed: false },
  ref: { file: '', rights: false, refDone: false, vocalDone: false, busy: '', intensity: 100 },
  master: { on: false, master_style: 0, master_intensity: 60, master_loudness: 3, master_tone: 0, master_width: 100, master_output: 0, mix: '' },
  sim: { signal: 'live', take: 'live', transport: 'playing', sidechain: 'none', speech: 'ready', engine: 'ok', size: 'standard', motion: !prefersReduced, bpm: 92 },
  onb: { open: false, step: 0, quiet: false }, onbDone: false,
  lib: [], modal: null, flags: { compared: false },
};
const phaseLog = [];
(function restore() {
  const lib = store.get(LS_LIB);
  if (Array.isArray(lib)) S.lib = lib.map((p) => ({ name: String((p && p.name) || '').slice(0, 40), sound: sanitizeSound(p && p.sound) })).filter((p) => p.name && p.sound);
  const ss = store.get(LS);
  if (!ss || typeof ss !== 'object') { S.onb.open = true; return; }
  const snd = sanitizeSound(ss.sound); if (snd) S.sound = snd;
  if (ss.preset && typeof ss.preset.name === 'string') S.preset = { name: ss.preset.name.slice(0, 48), type: ['default', 'factory', 'world', 'user', 'built', 'notebook', 'loose'].includes(ss.preset.type) ? ss.preset.type : 'built', snap: sanitizeSound(ss.preset.snap) || copy(S.sound) };
  if (ss.session && typeof ss.session.name === 'string' && ss.session.name.trim()) S.session.name = ss.session.name.slice(0, 40);
  for (const k of ['A', 'B']) { const sl = ss.ab && ss.ab[k]; const s2 = sl && sanitizeSound(sl.sound); S.ab[k] = s2 ? { sound: s2, name: String(sl.name || '').slice(0, 48) } : null; }
  S.ab.active = ss.ab && (ss.ab.active === 'A' || ss.ab.active === 'B') && S.ab[ss.ab.active] ? ss.ab.active : null;
  if (Array.isArray(ss.notebook)) S.notebook = ss.notebook.map((e) => ({ id: String(e.id || '').slice(0, 40), name: String(e.name || '').slice(0, 40), note: String(e.note || '').slice(0, 80), from: String(e.from || '').slice(0, 48), at: Number(e.at) || Date.now(), sound: sanitizeSound(e.sound) })).filter((e) => e.id && e.name && e.sound);
  S.activeNote = S.notebook.some((e) => e.id === ss.activeNote) ? ss.activeNote : null;
  if (Array.isArray(ss.desk)) {
    const cards = ss.desk.map((c) => {
      if (!c || typeof c !== 'object') return null;
      const base = { id: String(c.id || '').slice(0, 40), kind: ['idea', 'kid', 'accident'].includes(c.kind) ? c.kind : null, name: String(c.name || '').slice(0, 48), desc: String(c.desc || '').slice(0, 200), intent: String(c.intent || '').slice(0, 120), state: ['new', 'kept', 'rejected'].includes(c.state) ? c.state : 'new', setNo: Number(c.setNo) || 0, rev: Number(c.rev) || 1 };
      if (!base.id || !base.kind) return null;
      if (base.kind === 'idea') return IDEA[base.id] ? base : null;
      const set = {}, why = {};
      for (const [k, v] of Object.entries(c.set || {})) if (PM[k] && Number.isFinite(+v)) { set[k] = clampP(k, +v); why[k] = String((c.why || {})[k] || '').slice(0, 120); }
      return Object.keys(set).length ? { ...base, set, why } : null;
    }).filter(Boolean);
    if (cards.length) S.desk = cards;
  }
  S.bypass = !!ss.bypass; S.onbDone = !!ss.onbDone; S.kid.quiet = !!ss.quiet; S.kid.setNo = Number(ss.setNo) || 0;
  if (ss.sim && typeof ss.sim.motion === 'boolean') S.sim.motion = ss.sim.motion;
  S.onb.open = !S.onbDone;
})();
let persistTimer = 0;
function persistNow() {
  store.set(LS, { sound: S.sound, bypass: S.bypass, preset: S.preset, session: S.session, ab: S.ab, notebook: S.notebook, activeNote: S.activeNote,
    desk: S.desk.map((c) => ({ id: c.id, kind: c.kind, name: c.name, desc: c.desc, intent: c.intent, state: c.state, setNo: c.setNo, rev: c.rev, set: c.set, why: c.why })),
    onbDone: S.onbDone, quiet: S.kid.quiet, setNo: S.kid.setNo, sim: { motion: S.sim.motion } });
  store.set(LS_LIB, S.lib);
}
function persist() { clearTimeout(persistTimer); persistTimer = setTimeout(persistNow, 150); }

// ---------------------------------------------------------------- core: change, undo, redo
const snap = () => ({ sound: copy(S.sound), preset: copy(S.preset), abActive: S.ab.active, desk: S.desk.map((c) => [c.id, c.state]) });
function pushUndo(label, before) { S.undo.push({ label, ...before }); if (S.undo.length > 60) S.undo.shift(); S.redo = []; }
function restoreSnap(e) {
  S.sound = copy(e.sound); S.preset = copy(e.preset); S.ab.active = e.abActive;
  const st = new Map(e.desk || []); for (const c of S.desk) if (st.has(c.id)) c.state = st.get(c.id);
  stopAudition(true);
}
function undo() {
  if (gest) gestureEnd(0);
  const e = S.undo.pop();
  if (!e) { toast('Nothing to undo.'); return; }
  S.redo.push({ label: e.label, ...snap() }); restoreSnap(e);
  toast('Undone: ' + e.label, 'Redo', redo); changed(true);
}
function redo() {
  const e = S.redo.pop();
  if (!e) { toast('Nothing to redo.'); return; }
  S.undo.push({ label: e.label, ...snap() }); restoreSnap(e);
  toast('Redone: ' + e.label, 'Undo', undo); changed(true);
}
const isModified = () => !soundEq(S.sound, S.preset.snap);
const abModified = () => !!(S.ab.active && S.ab[S.ab.active] && !soundEq(S.sound, S.ab[S.ab.active].sound));
function setParam(id, v, src = 'manual') { v = clampP(id, v); if (S.sound[id] === v) return; S.sound[id] = v; if (src === 'manual') manualEdit([id]); changed(); }
function setMany(obj, src = 'manual') {
  const ids = [];
  for (const [k, v] of Object.entries(obj)) { const c = clampP(k, v); if (S.sound[k] !== c) { S.sound[k] = c; ids.push(k); } }
  if (ids.length && src === 'manual') manualEdit(ids);
  if (ids.length) changed();
}
let gest = null, gestTimer = 0;
function gestureStart(label) { clearTimeout(gestTimer); if (!gest) gest = { label, before: snap() }; }
function gestureEnd(delay = 0) {
  clearTimeout(gestTimer);
  const finish = () => { if (!gest) return; const g = gest; gest = null; if (!soundEq(g.before.sound, S.sound)) { pushUndo(g.label, g.before); changed(); } };
  if (delay) gestTimer = setTimeout(finish, delay); else finish();
}
function loadSound(next, label, preset) { const before = snap(); S.sound = copy(next); S.preset = preset; stopAudition(true); pushUndo(label, before); changed(true); }
function setBypass(v) { if (S.bypass === v) return; S.bypass = v; if (v) S.flags.compared = true; changed(); }

// ---------------------------------------------------------------- audition: hear something without changing the sound (production: a processor-side preview, NEW engineering)
function heard() { return S.audition ? S.audition.get() : S.sound; }
function hearing(kind, id) { return !!S.audition && S.audition.kind === kind && S.audition.id === id; }
function toggleAudition(a) { if (S.audition && S.audition.kind === a.kind && S.audition.id === a.id) stopAudition(); else { S.audition = a; if (S.review && a.kind !== 'review') S.review.listen = 'current'; changed(); } }
function stopAudition(silent) { const was = !!S.audition; S.audition = null; if (S.review) S.review.listen = 'current'; if (was && !silent) changed(); }

// ---------------------------------------------------------------- idea cards
function cardSet(c) { return c.kind === 'idea' ? IDEA[c.id].make(S.sound) : c.set; }
function cardWhy(c) { return c.kind === 'idea' ? IDEA[c.id].why : c.why; }
function cardTarget(c) { const t = copy(S.sound); for (const [k, v] of Object.entries(cardSet(c))) t[k] = clampP(k, v); return t; }
function cardRows(c) { return rowsFrom(S.sound, cardTarget(c), Object.fromEntries(Object.entries(cardWhy(c)).map(([k, w]) => [k, w || 'Part of “' + c.name + '”'])), null, 1); }
function keepCard(c, target, partial) {
  const before = snap();
  const n = IDS.filter((k) => Math.abs(target[k] - S.sound[k]) > 1e-6).length;
  if (!n) { toast('That’s already your sound.'); return; }
  S.sound = target; c.state = 'kept'; stopAudition(true);
  if (c.kind === 'kid') S.kid.mem = null;          // the next sentence starts from the sound you just kept
  S.kid.reply = null;
  pushUndo((partial ? 'Keep part of “' : 'Keep “') + c.name + '”', before);
  S.kid.event = { id: 'kept-' + c.id, parts: [{ b: c.name }, ' is yours now. Keep it in the notebook so you can find it again?'], actions: [['Keep in notebook…', () => openModal({ kind: 'keep', name: c.name, from: c.name }), 'primary', 'note-keep']] };
  toast('Kept “' + c.name + '”: ' + n + ' change' + (n > 1 ? 's' : '') + '.', 'Undo', undo);
  changed(true);
}
function rejectCard(c) {
  if (hearing('card', c.id)) stopAudition(true);
  if (S.review && S.review.cardId === c.id) S.review = null;
  c.state = 'rejected';
  toast('Put “' + c.name + '” away. Nothing changed.', 'Bring back', () => { c.state = 'new'; changed(true); });
  changed(true);
}
function lookCard(c) {
  openReview({ kind: 'card', cardId: c.id, title: c.name, sticker: c.kind, sub: c.desc, intent: c.intent, rows: cardRows(c),
    keep: (target, n, all) => keepCard(c, target, !all) });
}
function openReview(r) { r.listen = 'current'; S.review = r; if (S.audition && S.audition.kind !== 'review') S.audition = null; changed(true); }
function closeReview() { if (S.audition && S.audition.kind === 'review') S.audition = null; S.review = null; changed(true); }
function manualEdit(ids) {
  const r = S.review; if (!r) return;
  for (const id of ids) { const row = r.rows.find((x) => x.id === id); if (row && !row.manual) { row.manual = true; row.sel = false; } }
}
function keepReview() {
  const r = S.review; if (!r) return;
  const n = selectedCount(r);
  if (!n) { toast('Nothing is ticked. Tick at least one change.'); return; }
  const target = proposalTarget(r), all = n === r.rows.length;
  S.audition = null; S.review = null;
  r.keep(target, n, all);
  changed(true);
}

// ---------------------------------------------------------------- KID: a guided creative assistant (rules, not generative AI)
const UNSUPPORTED = [
  [/\b(timing|quantize|on beat|in time|offbeat|off beat)\b/, 'timing'],
  [/\b(breaths?|noise gate|background noise|hiss|room noise|denoise)\b/, 'noise'],
  [/\b(exactly like|clone|copy (?:his|her|their|the)|identical to)\b/, 'copy'],
  [/\b(master|mastering|lufs|loudness|make it louder)\b/, 'master'],
  [/\b(the beat|instrumental|bpm|tempo)\b/, 'beat'],
];
const LENSES = [
  { key: 'asked', line: 'Straight from what you said.' },
  { key: 'close', line: 'Same idea, words kept up front.' },
  { key: 'further', line: 'Pushed further, a little stranger.' },
];
function lensMem(mem, key) {
  const m = copy(mem);
  if (key === 'close') { m.locks = [...new Set([...m.locks, 'front', 'presence'])]; m.dir.depth *= 0.5; m.dir.width *= 0.7; }
  if (key === 'further') { m.dir.depth += 0.6; m.dir.width += 0.4; m.dir.exp += 0.5; }
  for (const d of DIMS) m.dir[d] = clamp(m.dir[d], -2, 2);
  return m;
}
function titleFor(dir) {
  const W = { bright: ['Bright', 'Dark'], warm: ['Warm', 'Cool'], presence: ['Up front', 'Set back'], punch: ['Hard', 'Soft'], width: ['Wide', 'Narrow'], depth: ['Distant', 'Close'], clean: ['Clean', 'Clean'], exp: ['Strange', 'Plain'] };
  const top = DIMS.filter((d) => Math.abs(dir[d]) > 0.05).sort((a, b) => Math.abs(dir[b]) - Math.abs(dir[a])).slice(0, 2);
  return top.length ? top.map((d) => W[d][dir[d] > 0 ? 0 : 1]).join(' & ') : 'Your idea';
}
function phraseFor(id, from, to) {
  const up = to > from;
  switch (id) {
    case 'eq_hi_gain': return up ? 'more air on top' : 'less air on top';
    case 'eq_b2_gain': return up ? 'words pushed forward' : 'softer presence';
    case 'eq_lo_gain': return up ? 'a warmer low end' : 'a lighter low end';
    case 'sat_drive': return up ? 'a little grit' : 'less grit';
    case 'comp_ratio': return up ? 'firmer compression' : 'looser dynamics';
    case 'echo_on': case 'echo_mix': return up ? 'more echo' : 'less echo';
    case 'dim_on': case 'dim_mix': case 'dim_width': return up ? 'a wider room' : 'a narrower room';
    case 'dbl_on': case 'dbl_amount': case 'dbl_width': return up ? 'a doubled edge' : 'fewer doubles';
    case 'mut_on': case 'mut_amount': return 'a mutated layer';
    case 'clean_on': case 'clean_amount': return 'cleaned up';
    default: return null;
  }
}
function describeSet(set, base) {
  const out = [];
  for (const id of Object.keys(set).sort((a, b) => stageOfId(a) - stageOfId(b))) { const p = phraseFor(id, base[id], set[id]); if (p && !out.includes(p)) out.push(p); }
  const s = out.slice(0, 3).join(', ');
  return s ? s[0].toUpperCase() + s.slice(1) + '.' : '';
}
function kidAvailable() {
  if (S.master.on) return { ok: false, why: 'Mastering mode is on in this instance, so I’m off duty: I only shape vocals. Switch it off in MASTER.' };
  if (S.sim.engine === 'error') return { ok: false, why: 'GlitchKid isn’t processing right now, so I can’t suggest anything you could hear.' };
  return { ok: true };
}
function speechAvailable() {
  if (S.sim.speech === 'missing') return { ok: false, kind: 'missing', why: 'Speech isn’t set up on this computer yet. Type to me, or set up speech once (it works offline after that).' };
  if (S.sim.speech === 'denied') return { ok: false, kind: 'denied', why: 'I’m not allowed to use a microphone. macOS: System Settings › Privacy & Security › Microphone, allow FL Studio. Windows: Settings › Privacy › Microphone. Typing works.' };
  return { ok: true };
}
const PHASE_TEXT = { ready: 'Ready. Hold to talk or type. Speech stays on this computer.', recording: 'Listening… let go to send', transcribing: 'Transcribing on this computer…', reading: 'Reading your words…', building: 'Leaving ideas on the desk…', done: 'Ideas are on the desk.', unavailable: 'Unavailable', error: 'Didn’t catch that.' };
function setPhase(p) { S.kid.phase = p; phaseLog.push(p); changed(); }
function reply(parts, opts = {}) { S.kid.reply = { id: 'r' + Date.now().toString(36) + Math.floor(Math.random() * 1e4), parts: Array.isArray(parts) ? parts : [parts], kind: opts.kind || 'reply', actions: opts.actions || [], badge: opts.badge }; }
const PHRASES = {
  first: ['Make this hook feel wider.', 'I want it darker and more intimate.', 'Give me a rougher, more unstable texture.', 'Make the vocal feel distant but keep the words clear.', 'Add some reverb.', 'Fix my timing.'],
  follow: ['A little less.', 'Keep the vocal upfront.', 'Make it darker.', 'That’s too much width.'],
};
const ALL_PHRASES = [...PHRASES.first, ...PHRASES.follow];
async function kidCommand(text, via) {
  text = String(text || '').trim().slice(0, 160);
  if (!text) return;
  if (S.view.area !== 'session') { S.view.area = 'session'; changed(true); }
  const av = kidAvailable();
  if (!av.ok) { reply(av.why, { kind: 'err' }); setPhase('unavailable'); return; }
  if (S.kid.busy) return;
  S.kid.busy = true; S.kid.heard = { text, via };
  try {
    if (via === 'voice') { setPhase('transcribing'); await later(650); }
    setPhase('reading'); await later(450);
    const low = text.toLowerCase().replace(/[’']/g, "'");
    const uns = UNSUPPORTED.find(([re]) => re.test(low));
    if (uns) {
      const lines = { timing: ['Timing isn’t something GlitchKid changes; that’s editing in FL Studio. I can change how it ', { b: 'sounds' }, ', not when.'],
        noise: ['GlitchKid has no noise gate or breath remover. ', { b: 'CLEAN' }, ' removes rumble and harshness and tames S sounds. Want ideas with CLEAN?'],
        copy: ['I can point you toward a direction, ', { b: 'never copy anyone’s voice or chain' }, '. Tell me what you like about it: darker? wider? rougher?'],
        master: ['Loudness for a finished mix lives in ', { b: 'MASTER' }, ', on FL Studio’s master track. On a vocal I can shape tone, space and character.'],
        beat: ['I only shape your vocal; the beat is never touched. ', { b: 'Beat Lock' }, ' shows how your vocal sits against it.'] }[uns[1]];
      const acts = { noise: [['Ideas with CLEAN', () => kidCommand('Clean it up but keep the character.', 'typed'), 'primary', 'note-clean']], master: [['Open MASTER', () => go('master'), 'primary', 'note-master']], beat: [['Open Beat Lock', () => { S.view.toolTab = 'beat'; go('tools'); }, 'primary', 'note-beat']] }[uns[1]] || [];
      reply(lines, { kind: 'err', actions: acts });
      setPhase('error'); return;
    }
    const f = parseCommand(text);
    if (f.build) { S.kid.busy = false; S.view.toolTab = 'build'; startBuild(); go('tools'); reply('Opened Build My Mix in TOOLS. It listens first, then you review every change.'); setPhase('done'); return; }
    if (!f.understood) { reply('I didn’t catch a sound word in that. Try “darker”, “wider”, “more space”, “rougher” or “a little less”. Nothing changed.', { kind: 'err' }); setPhase('error'); return; }
    // like KidDirector's memory: while my latest ideas are still on the desk, a new sentence refines them; "start over" begins again
    const liveKid = S.desk.some((c) => c.kind === 'kid' && c.setNo === S.kid.setNo && c.state === 'new');
    const follow = !!S.kid.mem && liveKid && !/\b(start over|reset|forget that|forget it|from scratch)\b/.test(low);
    if (f.scale && !follow) { reply('There’s nothing from me on the desk to scale back. Ask for a change, or use Undo to step back.'); setPhase('done'); return; }
    setPhase('building'); await later(500);
    if (!follow) S.kid.mem = { dir: zeroDir(), locks: [], role: '', base: copy(S.sound) };
    const mem = S.kid.mem, said = [];
    if (f.scale) { const dims = Object.keys(f.ops); for (const d of (dims.length ? dims : DIMS)) mem.dir[d] *= f.scale; said.push('Less of that: about ' + Math.round(f.scale * 100) + '%. '); }
    else for (const [d, v] of Object.entries(f.ops)) mem.dir[d] = clamp(mem.dir[d] + v * f.mag, -2, 2);
    for (const l of f.locks) if (!mem.locks.includes(l)) mem.locks.push(l);
    if (f.role) mem.role = f.role;
    if (/\breverb\b/.test(low)) said.push('GlitchKid has no reverb; the space here is Echo + Dimension. ');
    if (f.newWords.length) said.push('(“' + f.newWords.join('”, “') + '” are new words for me.) ');
    const prevRev = follow ? Math.max(...S.desk.filter((c) => c.kind === 'kid' && c.setNo === S.kid.setNo).map((c) => c.rev || 1)) : 0;
    const sets = [];
    for (const lens of LENSES) {
      const m = lensMem(mem, lens.key), { t, why } = decide(m), set = {};
      for (const id of IDS) if (Math.abs(t[id] - m.base[id]) > 1e-6) set[id] = t[id];
      if (!Object.keys(set).length || sets.some((x) => JSON.stringify(x.set) === JSON.stringify(set))) continue;
      sets.push({ lens, set, why, title: titleFor(m.dir) });
    }
    if (!sets.length) { reply('That leaves nothing to change: your sound is already there.'); setPhase('done'); return; }
    S.desk = S.desk.filter((c) => !(c.kind === 'kid' && c.state === 'new'));     // earlier, untouched KID ideas make room for the new ones
    if (!follow) S.kid.setNo += 1;
    const rev = follow ? prevRev + 1 : 1, setNo = S.kid.setNo;
    const cards = sets.map((x, i) => ({ id: 'k' + setNo + '-' + rev + '-' + i, kind: 'kid', setNo, rev, name: x.lens.key === 'asked' ? x.title : x.title + (x.lens.key === 'close' ? ', words up front' : ', further out'),
      desc: x.lens.line + ' ' + describeSet(x.set, mem.base), intent: '“' + text + '”', set: x.set, why: x.why, state: 'new' }));
    S.desk.unshift(...cards);
    const words = f.heard.filter((h) => !/keep/.test(h));
    reply([...(words.length ? ['Heard: ', { b: words.join(', ') }, '. '] : []), ...said, 'I left ', { b: cards.length + ' idea' + (cards.length > 1 ? 's' : '') }, rev > 1 ? ' (revision ' + rev + ')' : '', ' on the desk. Hear them, look inside, keep one or none. Gain, mix and the limiter stay yours.'],
      { actions: [['Hear the first', () => { const c = S.desk.find((x) => x.id === cards[0].id); if (c) toggleAudition({ kind: 'card', id: c.id, name: c.name, get: () => cardTarget(c) }); }, 'primary', 'note-hear']] });
    setPhase('done');
  } finally { S.kid.busy = false; changed(true); }
}
function micDown() {
  const av = kidAvailable(), sp = speechAvailable();
  if (S.view.area !== 'session') { S.view.area = 'session'; changed(true); }
  if (!av.ok) { reply(av.why, { kind: 'err' }); setPhase('unavailable'); return; }
  if (!sp.ok) { reply(sp.why, { kind: 'err', actions: sp.kind === 'missing' ? [['Set up speech…', () => openModal({ kind: 'speech' }), 'primary', 'note-speech']] : [] }); setPhase('unavailable'); return; }
  if (S.kid.busy) return;
  S.kid.holdStart = performance.now(); setPhase('recording');
}
function micUp() {
  if (S.kid.phase !== 'recording') return;
  if (performance.now() - S.kid.holdStart < 300 * pace.k) { reply('Hold the button while you talk, then let go.'); setPhase('ready'); return; }
  kidCommand(ALL_PHRASES[S.kid.phrase] || ALL_PHRASES[0], 'voice');
}
// KID's notes: only from things GlitchKid really knows (your settings, the input level, FL Studio's tempo); measurements are labelled
function noteNow() {
  const k = S.kid, d = (id) => k.dismissed.includes(id);
  if (S.master.on) return { id: 'master', kind: 'err', parts: ['Mastering mode is on in this instance. Vocal shaping and I are off duty until you switch it off in ', { b: 'MASTER' }, '.'] };
  if (S.sim.signal === 'clipping' && !d('clip')) return { id: 'clip', kind: 'err', meta: 'From the input level', parts: ['Your take is ', { b: 'clipping before it reaches me' }, '. Lower the recording level in FL Studio; I can’t repair clipped audio.'] };
  if (k.reply) return k.reply;
  if (k.event && !d(k.event.id)) return k.event;
  if (k.quiet) return null;
  const s = S.sound;
  if (S.sim.signal === 'none' && !d('noinput')) return { id: 'noinput', parts: ['Nothing’s coming in yet. Press play in FL Studio and I’ll be here. You can still try ideas on the desk.'] };
  if (S.preset.type === 'default' && !isModified() && !S.desk.some((c) => c.state === 'kept') && !d('fresh')) return { id: 'fresh', parts: ['Fresh session. Tell me a feeling (“darker”, “wider”, “rougher”), or roll a ', { b: 'happy accident' }, '. I’ll leave ideas on the desk; you decide.'], actions: [['Roll a happy accident', rollAccident, 'primary', 'note-roll']] };
  if (on(s, 'echo_on') && !on(s, 'echo_sync') && !d('echosync')) return { id: 'echosync', meta: 'From your settings and FL Studio’s tempo', parts: ['Echo isn’t locked to the song. FL Studio says ', { b: S.sim.bpm + ' BPM' }, '. Lock the repeats to it?'], actions: [['Lock echo to tempo', () => { gestureStart('Lock echo to tempo'); setParam('echo_sync', 1); gestureEnd(0); toast('Echo locked to the tempo.', 'Undo', undo); }, 'primary', 'note-sync']] };
  if (on(s, 'tune_on') && s.tune_scale === 0 && !d('tunechrom')) return { id: 'tunechrom', meta: 'From your settings', parts: ['Pitch Tune is pulling to ', { b: 'every note' }, ' (Chromatic). If you know the beat’s key, set it for a cleaner pull.'], actions: [['Open TUNE', () => goModule('tune'), '', 'note-tune']] };
  if (S.sim.signal !== 'none' && !on(s, 'clean_on') && S.desk.some((c) => c.state === 'kept') && !d('lowmid')) return { id: 'lowmid', badge: 'SIMULATED MEASUREMENT', meta: 'Measured on your take', parts: ['There’s some ', { b: 'low-mid build-up near 300 Hz' }, ' on your take. CLEAN might tidy it.'], actions: [['Ideas with CLEAN', () => kidCommand('Clean it up but keep the character.', 'typed'), '', 'note-clean']] };
  return null;
}
function dismissNote(n) { if (S.kid.reply && n.id === S.kid.reply.id) S.kid.reply = null; else if (S.kid.event && n.id === S.kid.event.id) { S.kid.dismissed.push(n.id); S.kid.event = null; } else S.kid.dismissed.push(n.id); if (['done', 'error', 'unavailable'].includes(S.kid.phase)) S.kid.phase = 'ready'; changed(); }

// ---------------------------------------------------------------- the session notebook (production: NEW per-project state)
const nbId = () => 'n' + Date.now().toString(36) + Math.floor(Math.random() * 1e5);
function keepInNotebook(name, note, from) {
  const e = { id: nbId(), name: name.trim().slice(0, 40), note: (note || '').trim().slice(0, 80), from: from || S.preset.name, at: Date.now(), sound: copy(S.sound) };
  S.notebook.unshift(e); S.activeNote = e.id; S.kid.event = null;
  persistNow(); toast('Kept “' + e.name + '” in the notebook.'); changed(true);
  return e;
}
function entryState(e) { if (soundEq(S.sound, e.sound)) return 'now'; if (S.activeNote === e.id) return 'changed'; return ''; }
function needsKeepPrompt() {
  if (!isModified()) return false;
  const s = S.sound;
  return !(S.notebook.some((e) => soundEq(e.sound, s)) || S.lib.some((p) => soundEq(p.sound, s)) || ['A', 'B'].some((k) => S.ab[k] && soundEq(S.ab[k].sound, s)));
}
function guardUnsaved(what, fn) { if (needsKeepPrompt()) { openModal({ kind: 'unsaved', what, fn }); return; } fn(); }
function useEntry(e) { guardUnsaved('“' + e.name + '”', () => { loadSound(e.sound, 'Use “' + e.name + '”', { name: e.name, type: 'notebook', snap: copy(e.sound) }); S.activeNote = e.id; toast('Now using “' + e.name + '”.', 'Undo', undo); }); }

// ---------------------------------------------------------------- scenes and worlds
function loadProfile(pi) { const s = profileSound(pi, S.sound); loadSound(s, 'Use ' + PROFILES[pi].name, { name: PROFILES[pi].name, type: 'factory', snap: copy(s) }); S.activeNote = null; toast('Using ' + PROFILES[pi].name + '. Your tuning, gain, mix, CLEAN, AIR LAB and limiter were kept.', 'Undo', undo); }
function loadDefault() { const s = factoryDefault(); loadSound(s, 'Use Factory Default', { name: 'Factory Default', type: 'default', snap: copy(s) }); S.activeNote = null; toast('Every control back to its default.', 'Undo', undo); }
function loadWorld(w) { const s = worldSound(w, S.sound); loadSound(s, 'Use ' + w.name, { name: w.name, type: 'world', snap: copy(s) }); S.activeNote = null; toast('Using the draft world ' + w.name + '. Customise it in SHAPE, then keep your version.', 'Undo', undo); }
function loadUser(p) { loadSound(p.sound, 'Use “' + p.name + '”', { name: p.name, type: 'user', snap: copy(p.sound) }); S.activeNote = null; toast('Using your saved sound “' + p.name + '”.', 'Undo', undo); }
function sceneTarget(sc) { return sc.kind === 'factory' ? profileSound(sc.i, S.sound) : sc.kind === 'default' ? factoryDefault() : sc.kind === 'user' ? copy(sc.p.sound) : worldSound(sc.w, S.sound); }
function useScene(sc) { guardUnsaved(sc.kind === 'user' ? '“' + sc.name + '”' : sc.name, () => (sc.kind === 'factory' ? loadProfile(sc.i) : sc.kind === 'default' ? loadDefault() : sc.kind === 'user' ? loadUser(sc.p) : loadWorld(sc.w))); }
function lookScene(sc) {
  const target = sceneTarget(sc), why = {};
  const src = sc.kind === 'factory' ? PROFILES[sc.i].o : sc.kind === 'world' ? sc.w.o : sc.kind === 'user' ? sc.p.sound : {};
  for (const id of IDS) why[id] = src[id] !== undefined ? (sc.kind === 'user' ? 'From your saved sound' : 'Set by ' + sc.name) : 'Back to its default (not part of ' + sc.name + ')';
  openReview({ kind: 'scene', id: sc.name, title: sc.name, sticker: sc.kind === 'world' ? 'draft' : sc.kind === 'user' ? 'user' : 'factory', sub: sc.tag, rows: rowsFrom(S.sound, target, why, null, 1),
    keep: (t, n, all) => { if (all) { useScene(sc); return; } const before = snap(); S.sound = t; pushUndo('Use part of ' + sc.name, before); toast('Took ' + n + ' change' + (n > 1 ? 's' : '') + ' from ' + sc.name + '.', 'Undo', undo); } });
}

// ---------------------------------------------------------------- tools: Beat Lock, Reference Match, Build My Mix (the engines exist; analysis values here are SIMULATED)
async function beatAnalyse() {
  if (S.sim.sidechain !== 'beat' || S.sim.signal === 'none' || S.beat.busy) return;
  S.beat.busy = true; S.beat.analysed = false;
  changed(true);
  for (let i = 1; i <= 10; i++) { await later(160); S.beat.progress = i * 10; changed(true); }
  S.beat.busy = false; S.beat.analysed = true; changed(true);
}
function beatProposal() {
  // automode::beatMoves (SourceRelationship.cpp, shared with Build My Mix). What triggers each move is SIMULATED here:
  // the beat covers the voice at 2-5 kHz, the low-mids are busy, the beat is crowded. A move never pushes past its limit
  // and never pulls back a value already beyond it.
  const s = S.sound, t = copy(s), why = {};
  const lim = (base, delta, limit) => (delta >= 0 ? Math.min(base + delta, Math.max(base, limit)) : Math.max(base + delta, Math.min(base, limit)));
  t.eq_b2_gain = clampP('eq_b2_gain', lim(s.eq_b2_gain, 1.5, 4)); why.eq_b2_gain = 'The beat competes with your voice at 2–5 kHz: presence raised so the words stay clear (at most +4 dB in total). Simulated measurement';
  t.eq_b1_gain = clampP('eq_b1_gain', lim(s.eq_b1_gain, -1, -6)); why.eq_b1_gain = 'The beat is busy in the low-mids: the Body bell cut a little (never below −6 dB in total). Simulated measurement';
  t.eq_lo_gain = clampP('eq_lo_gain', lim(s.eq_lo_gain, -1, -6)); why.eq_lo_gain = 'The beat is busy in the low-mids: low shelf lowered (never below −6 dB in total). Simulated measurement';
  for (const [id, sw] of [['echo_mix', 'echo_on'], ['dim_mix', 'dim_on']]) if (on(s, sw)) { t[id] = clampP(id, s[id] * 0.85); why[id] = 'The beat is crowded: this ambience reduced so the vocal does not get lost. Simulated measurement'; }
  openReview({ kind: 'beat', title: 'Beat Lock', sticker: 'tool', sub: 'Conservative moves on your vocal’s own controls. The beat is never processed.', rows: rowsFrom(s, t, why, null, 1),
    advisory: 'BALANCE · advice only: vocal and beat are within 6 dB of each other (−3.0 dB, simulated). That is a measurement, not a verdict. GlitchKid never moves your faders.',
    keep: (target, n) => { const before = snap(); S.sound = target; pushUndo('Beat Lock (' + n + ')', before); toast('Applied ' + n + ' Beat Lock change' + (n > 1 ? 's' : '') + '.', 'Undo', undo); } });
}
async function refStep(kind) {
  if (S.ref.busy) return;
  if (kind === 'vocal' && S.sim.signal === 'none') { toast('Play your vocal in FL Studio first.'); return; }
  S.ref.busy = kind; changed(true);
  await later(1300); S.ref.busy = ''; if (kind === 'ref') S.ref.refDone = true; else S.ref.vocalDone = true; changed(true);
}
function refProposal() {
  // ReferenceMatch.cpp: a full mix gets half the measured difference × intensity; at most 3 dB a move; under 0.5 dB is ignored;
  // Presence stays at or under +4 dB, Body and Air shelf within ±6 dB; a band already beyond a limit is left alone rather than pulled back.
  // (That last rule is ReferenceMatch.h's documented bound. ReferenceMatch.cpp only skips a band beyond the limit in the step's direction, so today it could
  // pull Air shelf from −12 to −6 dB in one 6 dB move: see OPEN_DECISIONS.md. This prototype follows the documented rule.)
  const s = S.sound, k = S.ref.intensity / 100, t = copy(s), why = {}, left = [];
  const diff = { eq_b1_gain: -2, eq_b2_gain: 3, eq_hi_gain: 4 };            // the measured differences: SIMULATED
  const NAME = { eq_b1_gain: 'Body', eq_b2_gain: 'Presence', eq_hi_gain: 'Air shelf' };
  for (const [id, avg] of Object.entries(diff)) {
    const step = clamp(avg * 0.5 * k, -3, 3), cur = s[id], limit = id === 'eq_b2_gain' ? 4 : 6;
    if (Math.abs(step) < 0.5) continue;
    if (cur > limit || cur < -6 || (step > 0 && cur >= limit) || (step < 0 && cur <= -6)) { left.push(NAME[id]); continue; }
    const to = clampP(id, clamp(cur + step, -6, limit));
    if (Math.abs(to - cur) < 0.05) continue;
    t[id] = to;
    why[id] = 'The reference is about ' + Math.abs(avg) + ' dB ' + (avg > 0 ? 'brighter or fuller' : 'thinner or darker') + ' here. Only part of that is requested (' + S.ref.intensity + '% intensity; a full mix is not a vocal). Simulated measurement';
  }
  openReview({ kind: 'reference', title: 'Reference Match', sticker: 'tool', sub: 'Tonal guidance, never a copy. Only Body, Presence and Air shelf move. Confidence: medium (a full mix is never rated higher).' + (left.length ? ' ' + left.join(' and ') + ': already at or beyond its limit, so left alone.' : ''), rows: rowsFrom(s, t, why, null, 1),
    keep: (target, n) => { const before = snap(); S.sound = target; pushUndo('Reference Match (' + n + ')', before); toast('Applied ' + n + ' reference EQ change' + (n > 1 ? 's' : '') + '.', 'Undo', undo); } });
}
function startBuild() { const pi = PROFILES.findIndex((p) => p.name === S.preset.name); S.build = { step: 1, listen: 0, listened: false, listening: false, profile: pi >= 0 ? pi : 1, intensity: 100 }; changed(true); }
async function buildListen() {
  const b = S.build; if (!b || S.sim.signal === 'none' || b.listening) return;
  b.listening = true; changed(true);
  for (let i = 1; i <= 10; i++) { await later(200); if (S.build !== b) return; b.listen = i * 10; changed(true); }
  b.listening = false; b.listened = true; changed(true);
}
function buildReview() {
  const b = S.build, base = copy(S.sound), t = profileSound(b.profile, base), why = {}, prof = PROFILES[b.profile];
  const kk = 0.5 + 0.5 * b.intensity / 100;
  for (const id of ['echo_mix', 'dim_mix', 'dbl_amount', 'sat_drive']) t[id] = clampP(id, t[id] * kk);
  for (const id of IDS) why[id] = prof.o[id] !== undefined ? 'From the ' + prof.name + ' profile' : 'Back to its default: not part of this profile';
  why.mut_on = why.harm_on = why.stutter_on = 'Off: Build My Mix makes a vocal mix; creative effects can go back on afterwards';
  why.formant_shift = 'Back to 0: Build My Mix makes a vocal mix';
  const simIds = [];
  if (b.listened) { t.comp_thresh = clampP('comp_thresh', t.comp_thresh - 2); why.comp_thresh = 'Fitted: your loud words sit about 4 dB above the rest (simulated measurement)'; simIds.push('comp_thresh'); }
  openReview({ kind: 'build', title: 'Build My Mix · ' + prof.name, sticker: 'tool', sub: b.listened ? 'Fitted to your take.' : 'As designed: not fitted to your take.', rows: rowsFrom(base, t, why, null, 1, simIds),
    keep: (target, n) => { const before = snap(); S.sound = target; S.preset = { name: 'Built: ' + prof.name, type: 'built', snap: copy(target) }; S.activeNote = null; pushUndo('Build My Mix (' + n + ')', before); S.build = null; toast('Built your mix: ' + n + ' change' + (n > 1 ? 's' : '') + '. Not kept anywhere yet.', 'Undo', undo); } });
}

// ---------------------------------------------------------------- plumbing
let frame, topEl, railEl, mainEl, chainEl, helpEl, toastEl, modalEl, drawerEl = null;
const CTRL = new Map();
let binders = [];
const bind = (fn) => { binders.push(fn); fn(); };
function registerCtrl(id, fn) { if (!CTRL.has(id)) CTRL.set(id, new Set()); CTRL.get(id).add(fn); }
let updQueued = false, structural = false;
function changed(struct) { if (struct) structural = true; if (!updQueued) { updQueued = true; requestAnimationFrame(update); } }
function update() {
  updQueued = false;
  keepFocus(() => {
    if (structural) { structural = false; renderMain(); }
    for (const set of CTRL.values()) for (const fn of set) fn();
    renderTop(); renderRail(); renderChain();
    for (const fn of binders) fn();
    renderDrawer(); renderModal(); renderOnboarding();
  });
  persist();
}
const DEFAULT_HELP = 'Point at anything to read what it does. Hold K to talk to KID · D: DRY / PROCESSED · Ctrl/Cmd+Z / Shift+Z: undo / redo.';
function setHelp(t) { if (helpEl) $('.hl-text', helpEl).textContent = t || DEFAULT_HELP; }
let toastTimer = 0;
const put = (node, ...kids) => node.append(...kids.flat(Infinity).filter((k) => k != null && k !== false));   // native append would print null as text
function go(area) { S.view.area = area; S.view.editName = false; changed(true); }
function goModule(m) { const M = MOD[m]; if (M.ws === 'lab') S.view.area = 'lab'; else { S.view.area = 'shape'; S.view.shapeTab = M.tab; } changed(true); }

// ---------------------------------------------------------------- shell, top bar, rail, chain
function renderShell() {
  frame = $('#frame');
  frame.replaceChildren(svgFrom('<svg xmlns="http://www.w3.org/2000/svg" width="0" height="0" style="position:absolute" aria-hidden="true"><defs><linearGradient id="gk-arc" x1="0" y1="1" x2="1" y2="0"><stop offset="0" stop-color="#8f5bff"/><stop offset="1" stop-color="#ff4fb8"/></linearGradient><radialGradient id="gk-cap" cx="50%" cy="30%" r="75%"><stop offset="0" stop-color="#3b3249"/><stop offset="1" stop-color="#140f1a"/></radialGradient></defs></svg>'));
  topEl = el('header', { class: 'topbar', 'data-testid': 'topbar' });
  railEl = el('nav', { class: 'rail', 'aria-label': 'Areas' });
  mainEl = el('div', { class: 'main', id: 'gk-main' });
  chainEl = el('div', { class: 'chain', role: 'list', 'aria-label': 'Signal chain, in its fixed order' });
  helpEl = el('div', { class: 'helpline', 'aria-live': 'polite' }, el('span', { class: 'hl-text' }), el('span', { class: 'hl-tag' }, 'PROTOTYPE · simulated take'));
  toastEl = el('div', { class: 'toast-layer', 'aria-live': 'polite' });
  modalEl = el('div', { id: 'gk-modal' });
  frame.append(topEl, railEl, mainEl, chainEl, helpEl, toastEl, modalEl);
  setHelp('');
}
function renderTop() {
  const mod = isModified();
  let tape;
  if (S.view.editName) {
    tape = el('input', { class: 'tape-edit', 'data-key': 'tape-edit', 'data-testid': 'tape-edit', maxlength: 40, 'aria-label': 'Session name' });
    tape.value = S.session.name;
    const commit = (ok) => { if (!S.view.editName) return; if (ok && tape.value.trim()) S.session.name = tape.value.trim().slice(0, 40); S.view.editName = false; changed(); };
    tape.addEventListener('keydown', (e) => { if (e.key === 'Enter') commit(true); else if (e.key === 'Escape') commit(false); e.stopPropagation(); });
    tape.addEventListener('blur', () => commit(true));
    requestAnimationFrame(() => { if (document.activeElement !== tape) { tape.focus(); tape.select(); } });
  } else tape = tip(el('button', { class: 'tape', 'data-key': 'tape', 'data-testid': 'tape', 'aria-label': 'Session name: ' + S.session.name + '. Click to rename.', onclick: () => { S.view.editName = true; changed(); } }, el('span', { class: 'tape-name' }, S.session.name)), 'Your session’s name, on a strip of tape. Click to rename it (any language). Saved with the project.');
  const type = { default: 'Default', factory: 'Factory', world: 'Draft world', user: 'Saved sound', built: 'Built', notebook: 'Notebook', loose: 'Deleted' }[S.preset.type];
  const chip = tip(el('button', { class: 'sound-chip', 'data-key': 'sound-chip', 'data-testid': 'sound-chip', onclick: () => go('worlds') }, el('b', { 'data-testid': 'sound-name' }, S.preset.name),
    el('span', { 'data-testid': 'sound-meta' }, type, ' · ', mod ? el('span', { class: 'unsaved' }, '● Unsaved') : S.preset.type === 'built' || S.preset.type === 'loose' ? el('span', { class: 'unsaved' }, '● Not kept yet') : 'Saved')), 'The sound you started from, and whether you have changed it since. Click for WORLDS.');
  const ab = el('div', { class: 'ab', role: 'group', 'aria-label': 'A/B slots' }, ['A', 'B'].map((k) => {
    const sl = S.ab[k], active = S.ab.active === k, modk = active && abModified();
    const state = !sl ? 'empty' : modk ? 'changed' : active ? 'playing' : 'stored';
    const w = el('div', { class: 'ab-slot' + (active ? ' active' : '') + (modk ? ' modified' : ''), 'data-testid': 'ab-' + k, 'data-state': state },
      tip(el('button', { class: 'ab-main', 'data-key': 'ab-' + k, 'data-testid': 'ab-btn-' + k, 'aria-label': k + ': ' + state, onclick: () => abClick(k) }, el('b', null, k), el('span', null, state)), !sl ? 'Empty: click to store the current sound in ' + k + '.' : active ? (modk ? k + ' is playing; your changes are not in it yet. ⤓ stores them.' : k + ' is playing.') : 'Click to hear ' + k + '. GlitchKid asks first if the playing slot has changes.'));
    if (sl) w.append(tip(el('button', { class: 'ab-store', 'data-key': 'abstore-' + k, 'data-testid': 'ab-store-' + k, 'aria-label': 'Store the current sound in ' + k, onclick: () => abStore(k) }, '⤓'), 'Store the current sound in ' + k + ' (asks before replacing).'));
    return w;
  }));
  const lu = S.undo[S.undo.length - 1], lr = S.redo[S.redo.length - 1];
  const und = tip(el('button', { class: 'btn icon', 'data-key': 'undo', 'data-testid': 'undo', 'aria-label': 'Undo', 'aria-disabled': lu ? 'false' : 'true', onclick: () => (lu ? undo() : toast('Nothing to undo.')) }, icon('undo', 18)), lu ? 'Undo: ' + lu.label + ' (Ctrl/Cmd+Z)' : 'Nothing to undo.');
  const red = tip(el('button', { class: 'btn icon', 'data-key': 'redo', 'data-testid': 'redo', 'aria-label': 'Redo', 'aria-disabled': lr ? 'false' : 'true', onclick: () => (lr ? redo() : toast('Nothing to redo.')) }, icon('redo', 18)), lr ? 'Redo: ' + lr.label + ' (Ctrl/Cmd+Shift+Z)' : 'Nothing to redo.');
  const save = tip(el('button', { class: 'btn', 'data-key': 'save', 'data-testid': 'save', onclick: saveClicked }, icon('save', 16), 'Save…'), 'Save this sound as a preset you can use in any project. (The notebook keeps ideas for this session.)');
  const cmp = tip(el('div', { class: 'compare', role: 'group', 'aria-label': 'Compare with your original vocal' },
    el('button', { class: 'dry', 'data-key': 'cmp-dry', 'data-testid': 'cmp-dry', 'aria-pressed': S.bypass ? 'true' : 'false', onclick: () => setBypass(true) }, 'DRY'),
    el('button', { 'data-key': 'cmp-proc', 'data-testid': 'cmp-proc', 'aria-pressed': S.bypass ? 'false' : 'true', onclick: () => setBypass(false) }, 'PROCESSED')), 'DRY is your original vocal at matched loudness; PROCESSED is GlitchKid. Press D.');
  const help = tip(el('button', { class: 'btn icon ghost', 'data-key': 'help', 'data-testid': 'help', 'aria-label': 'Help', onclick: () => openModal({ kind: 'help' }) }, icon('help', 20)), 'Help and shortcuts.');
  topEl.replaceChildren(el('div', { class: 'logo' }, el('b', null, 'GLITCHKID'), el('span', null, 'VOCAL LAB')), tape, chip, el('div', { class: 'tb-gap' }), ab, und, red, save, cmp, help);
}
const RAIL = [['session', 'SESSION', 'Your take, ideas on the desk, KID and the notebook'], ['shape', 'SHAPE', 'Every control, by what it does'], ['lab', 'LAB', 'Experimental: formant, harmony, mutation, stutter'], ['worlds', 'WORLDS', 'Factory tapes and draft worlds'], ['tools', 'TOOLS', 'Beat Lock, Reference Match, Build My Mix']];
function renderRail() {
  const items = RAIL.map(([k, label, h]) => tip(el('button', { class: 'rail-item', 'data-key': 'rail-' + k, 'data-testid': 'rail-' + k, 'aria-current': S.view.area === k ? 'page' : null, onclick: () => go(k) }, icon(k), label), h));
  const ideas = S.desk.filter((c) => c.kind === 'kid' && c.state === 'new').length;
  const kid = tip(el('button', { class: 'rail-item', 'data-key': 'rail-kid', 'data-testid': 'rail-kid', onclick: () => { go('session'); S.view.rtab = 'kid'; requestAnimationFrame(() => { const i = frame.querySelector('[data-testid="kid-input"]'); if (i) i.focus(); }); } }, icon('kidface'), 'KID', ideas ? el('span', { class: 'rdot', 'aria-label': ideas + ' ideas from KID on the desk' }, String(ideas)) : null), 'Talk to KID (or hold K anywhere).');
  railEl.replaceChildren(...items, el('div', { class: 'rail-sep' }), tip(el('button', { class: 'rail-item master', 'data-key': 'rail-master', 'data-testid': 'rail-master', 'aria-current': S.view.area === 'master' ? 'page' : null, onclick: () => go('master') }, icon('master'), 'MASTER'), 'The Mastering Lab: for a finished mix on the master track, not your vocal.'), el('div', { class: 'rail-spacer' }), kid);
}
const CHAIN_SHORT = { FORMANT: 'FORM', HARMONY: 'HARM', MUTATE: 'MUT', DOUBLE: 'DBL', WARMTH: 'WARM', STUTTER: 'STUT', DIMENSION: 'DIM', LIMIT: 'LIM' };   // compact window only; the full name is the chip's label and help
function renderChain() {
  const s = heard(), short = S.sim.size === 'compact';
  chainEl.replaceChildren(tip(el('span', { class: 'chain-lbl' }, 'CHAIN 1→17'), 'The processing order is fixed. SHAPE and LAB group the controls; they never reorder the chain.'),
    ...CHAIN.map(([n, m, label]) => { const onn = !S.master.on && modOn(m, s), ws = MOD[m].ws;
      return tip(el('button', { class: 'cchip' + (onn ? ' on' : '') + ((S.view.area === 'shape' && ws !== 'lab') || (S.view.area === 'lab' && ws === 'lab') ? ' here' : ''), role: 'listitem', 'data-ws': ws, 'data-key': 'chain-' + n, 'data-testid': 'chain-' + n, 'data-on': onn ? '1' : '0', 'aria-label': n + ' ' + STAGE_NAME[n] + ', ' + (onn ? 'on' : 'off'), onclick: () => goModule(m) }, el('span', { class: 'cd' }), short ? CHAIN_SHORT[label] || label : label),
        'Stage ' + n + ' of 17: ' + STAGE_NAME[n] + ', ' + (onn ? 'on' : 'off') + (S.audition ? ' (what you’re hearing)' : '') + '. Click to open it.'); }));
}

// ---------------------------------------------------------------- main area
function renderMain() {
  CTRL.clear(); binders = [];
  const v = { session: viewSession, shape: viewShape, lab: viewLab, worlds: viewWorlds, tools: viewTools, master: viewMaster }[S.view.area]();
  mainEl.replaceChildren(v);
  if (drawerEl) mainEl.append(drawerEl);
}
function hearText() {
  if (S.bypass) return ['dry', 'HEARING · DRY (YOUR ORIGINAL)'];
  if (S.audition) return ['audition', 'HEARING · ' + S.audition.name.toUpperCase() + ' (NOT KEPT)'];
  return ['', 'HEARING · PROCESSED'];
}
function viewSession() {
  const src = el('span', { class: 'chip', 'data-testid': 'take-source' }), tr = el('span', { class: 'chip', 'data-testid': 'transport' });
  const nm = el('span', { class: 'micro', style: { marginLeft: 'auto', whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis' } });
  bind(() => {
    const file = S.sim.take === 'file';
    const cpt = S.sim.size === 'compact';
    src.className = 'chip ' + (file ? '' : 'live'); src.textContent = file ? (cpt ? 'FILE · look only' : 'FILE · hook_take_03.wav · look only') : cpt ? 'LIVE' : 'LIVE · from FL Studio';
    src.setAttribute('aria-label', file ? 'Loaded file hook_take_03.wav, for looking and measuring only' : 'Live input from FL Studio');
    const playing = S.sim.transport === 'playing';
    tr.className = 'chip ' + (S.master.on ? 'err' : playing ? 'play' : 'stop'); tr.replaceChildren(icon(playing ? 'play' : 'stop', 12), playing ? (cpt ? S.sim.bpm + ' BPM' : 'FL Studio playing · ' + S.sim.bpm + ' BPM') : cpt ? 'Stopped' : 'FL Studio stopped');
    tr.setAttribute('aria-label', playing ? 'FL Studio is playing at ' + S.sim.bpm + ' BPM' : 'FL Studio is stopped');
    nm.textContent = S.sim.size === 'compact' ? S.preset.name : '';
  });
  const cv = el('canvas', { id: 'gk-wave', 'aria-hidden': 'true' });
  const msg = el('div', { class: 'wave-msg', 'data-testid': 'wave-msg' });
  bind(() => {
    let parts = [];
    if (S.master.on) parts = [el('b', null, 'Mastering mode is on'), el('span', null, 'This instance is working on a full mix. Switch it off in MASTER to shape a vocal.')];
    else if (S.sim.engine === 'error') parts = [el('b', null, 'Not processing'), el('span', null, 'Audio passes through unchanged (design placeholder: no verified trigger yet).')];
    else if (S.sim.signal === 'none' && S.sim.take === 'live') parts = [el('b', null, 'No vocal yet'), el('span', null, 'Press play in FL Studio, or sing into the armed track. GlitchKid listens on this Mixer insert.')];
    else if (S.sim.transport === 'stopped' && S.sim.take === 'live') parts = [el('b', null, 'Paused'), el('span', null, 'Press play in FL Studio to hear changes. The picture still follows your settings.')];
    msg.replaceChildren(...parts);
    if (msg.parentElement) msg.parentElement.classList.toggle('has-msg', parts.length > 0);   // the picture steps back behind a message
  });
  const hear = el('span', { class: 'hear', 'data-testid': 'hear-chip' });
  bind(() => { const [c, t] = hearText(); hear.className = 'hear ' + c; hear.textContent = t; });
  const meters = el('span', { class: 'minimeter', 'aria-label': 'Input and output level (simulated)' }, 'IN', el('span', { class: 'mm in' }, el('i')), el('span', { class: 'mm-v', 'data-testid': 'mm-in' }, '—'), 'OUT', el('span', { class: 'mm out' }, el('i')), el('span', { class: 'mm-v' }, '—'), el('span', { class: 'clip', 'data-testid': 'clip', hidden: true }, 'CLIP'));
  const fileBtn = el('button', { class: 'btn small ghost', 'data-key': 'take-file', 'data-testid': 'take-file' });
  bind(() => { const file = S.sim.take === 'file', cpt = S.sim.size === 'compact'; fileBtn.replaceChildren(icon(file ? 'session' : 'file', 15), file ? (cpt ? 'Live input' : 'Back to live input') : cpt ? 'File…' : 'Look at a file…'); fileBtn.onclick = () => { S.sim.take = file ? 'live' : 'file'; renderSim(); changed(true); toast(file ? 'Back to the live input from FL Studio.' : 'Prototype: “hook_take_03.wav” loaded for looking and measuring. GlitchKid can’t play files: play your track in FL Studio to hear changes.'); }; });
  const take = el('section', { class: 'panel take', 'aria-label': 'The take' },
    el('div', { class: 'take-head' }, el('span', { class: 'hand' }, 'the take'), src, tr, nm, fileBtn),
    el('div', { class: 'wave-box' }, cv, el('span', { class: 'badge sim wave-tag' }, 'SIMULATED TAKE'), msg, el('div', { class: 'wave-foot' }, hear, meters)));
  const deskCards = el('div', { class: 'cards', 'data-testid': 'desk', role: 'list', 'aria-label': 'Ideas on the desk' });
  const count = el('span', { class: 'dim' });
  const fade = () => deskCards.classList.toggle('more', deskCards.scrollWidth - deskCards.scrollLeft - deskCards.clientWidth > 4);
  deskCards.addEventListener('scroll', fade, { passive: true });
  deskCards.addEventListener('wheel', (e) => { if (Math.abs(e.deltaY) > Math.abs(e.deltaX) && deskCards.scrollWidth > deskCards.clientWidth) { deskCards.scrollLeft += e.deltaY; e.preventDefault(); } }, { passive: false });
  bind(() => {
    const vis = S.desk.filter((c) => c.state !== 'rejected');
    count.textContent = vis.length + ' idea' + (vis.length === 1 ? '' : 's');
    deskCards.replaceChildren(...(vis.length ? vis.map(cardEl) : [el('div', { class: 'icard empty-card', 'data-testid': 'desk-empty' }, el('span', { class: 'hand', style: { fontSize: '20px' } }, 'Desk is clear'), 'Ask KID, or roll a happy accident.')]));
    requestAnimationFrame(fade);
  });
  const desk = el('section', { class: 'panel desk', 'aria-label': 'Ideas on the desk' },
    el('div', { class: 'desk-head' }, el('span', { class: 'hand' }, 'ideas on the desk'), count, el('span', { style: { flex: 1 } }),
      tip(el('button', { class: 'btn small', 'data-key': 'roll', 'data-testid': 'roll', onclick: rollAccident }, icon('dice', 16), 'Happy accident'), 'A random mix of real controls. Hear it first; keep it only if you love it.')), deskCards);
  const right = el('div', { class: 's-right' });
  bind(() => {
    const compact = S.sim.size === 'compact';
    const tabs = compact ? el('div', { class: 'rtabs', role: 'tablist' }, [['kid', 'KID'], ['notebook', 'NOTEBOOK · ' + S.notebook.length]].map(([k, t]) => el('button', { role: 'tab', 'data-key': 'rtab-' + k, 'data-testid': 'rtab-' + k, 'aria-selected': S.view.rtab === k ? 'true' : 'false', onclick: () => { S.view.rtab = k; changed(); } }, t))) : null;
    const parts = compact ? (S.view.rtab === 'kid' ? [kidNoteEl(), kidBarEl()] : [notebookEl()]) : [kidNoteEl(), kidBarEl(), notebookEl()];
    keepFocus(() => right.replaceChildren(...[tabs, ...parts].filter(Boolean)));
  });
  const eb = S.master.on ? el('div', { class: 'banner steel', 'data-testid': 'banner-master' }, el('b', null, 'Mastering mode is on. '), 'This instance masters a full mix; the vocal chain is not in use. Switch it off in MASTER.') : null;
  return el('div', { class: 'screen', 'data-testid': 'screen-session' }, el('div', { class: 'session' }, el('div', { class: 's-left' }, eb, take, desk), right));
}
function cardEl(c) {
  const rows = cardRows(c), n = rows.length, h = hearing('card', c.id);
  const st = { idea: ['idea', 'IDEA'], kid: ['kid', 'KID'], accident: ['accident', 'ACCIDENT'] }[c.kind];
  return el('article', { class: 'icard' + (h ? ' auditioning' : '') + (c.state === 'kept' ? ' kept' : ''), role: 'listitem', 'data-testid': 'card-' + c.id, 'aria-label': c.name },
    el('div', { class: 'ic-top' }, el('span', { class: 'sticker ' + st[0] }, st[1]), c.rev > 1 ? el('span', { class: 'badge' }, 'REV ' + c.rev) : null, h ? el('span', { class: 'sticker hearing' }, 'HEARING') : null, c.state === 'kept' ? el('span', { class: 'sticker kept' }, 'KEPT') : null),
    el('div', { class: 'ic-name', 'data-clamp': '1' }, c.name), el('div', { class: 'ic-desc', 'data-clamp': '1' }, c.desc), c.kind === 'kid' ? null : el('div', { class: 'ic-intent', 'data-clamp': '1' }, c.intent),
    n ? tip(el('button', { class: 'ic-look ic-push', 'data-key': 'look-' + c.id, 'data-testid': 'look-' + c.id, onclick: () => lookCard(c) }, 'Look inside · ' + n + ' change' + (n > 1 ? 's' : '')), 'See exactly which real controls change, tick only the ones you want.') : el('span', { class: 'ic-same ic-push' }, 'Already in your sound'),
    el('div', { class: 'ic-actions' },
      tip(el('button', { class: 'btn small', 'data-key': 'hear-' + c.id, 'data-testid': 'hear-' + c.id, 'aria-pressed': h ? 'true' : 'false', disabled: !n, onclick: () => toggleAudition({ kind: 'card', id: c.id, name: c.name, get: () => cardTarget(c) }) }, h ? 'Stop' : 'Hear'), 'Audition it. Nothing changes until you keep it.'),
      tip(el('button', { class: 'btn small keep', 'data-key': 'keep-' + c.id, 'data-testid': 'keep-' + c.id, disabled: !n, onclick: () => keepCard(c, cardTarget(c), false) }, 'Keep'), 'Keep every change in this idea. Undo takes it back.'),
      tip(el('button', { class: 'btn small icon', 'data-key': 'reject-' + c.id, 'data-testid': 'reject-' + c.id, 'aria-label': 'Put “' + c.name + '” away', onclick: () => rejectCard(c) }, '✕'), 'Put it away. Nothing changes.')));
}
function kidNoteEl() {
  const n = noteNow();
  if (!n) return el('div', { class: 'panel', style: { padding: '10px 12px', flex: 'none' }, 'data-testid': 'kidnote-idle' }, el('span', { class: 'hand', style: { fontSize: '18px', color: 'var(--paper)' } }, 'KID'), el('span', { class: 'dim' }, S.kid.quiet ? ' · notes are quiet. I’ll answer when you ask.' : ' · here when you want ideas.'));
  return el('div', { class: 'kidnote' + (n.kind === 'err' ? ' err' : ''), 'data-testid': 'kidnote', 'data-note': n.id, role: 'status' },
    el('div', { class: 'kn-row' }, svgFrom(KID_FACE), el('div', { style: { flex: 1, minWidth: 0 } },
      el('div', { class: 'kn-text' }, n.parts.map((p) => (typeof p === 'string' ? p : el('b', null, p.b)))),
      n.badge || n.meta ? el('div', { class: 'kn-meta' }, n.badge ? el('span', { class: 'badge' }, n.badge) : null, n.badge && n.meta ? ' ' : null, n.meta || null) : null,
      n.actions && n.actions.length ? el('div', { class: 'kn-actions' }, n.actions.map(([t, fn, cls, key]) => el('button', { class: 'btn small ' + (cls || ''), 'data-key': key, 'data-testid': key, onclick: () => { if (n === S.kid.reply || (S.kid.event && n.id === S.kid.event.id)) dismissNote(n); fn(); } }, t))) : null)),
    el('button', { class: 'kn-x', 'data-key': 'kn-x', 'data-testid': 'note-dismiss', 'aria-label': 'Dismiss this note', onclick: () => dismissNote(n) }, '✕'));
}
function kidBarEl() {
  const input = el('input', { class: 'field', 'data-key': 'kid-input', 'data-testid': 'kid-input', placeholder: S.sim.size === 'compact' ? 'Tell KID…' : 'Tell KID what you want…', 'aria-label': 'Tell KID what you want', maxlength: 160 });
  const send = () => { const v = input.value; input.value = ''; kidCommand(v, 'typed'); };
  input.addEventListener('keydown', (e) => { if (e.key === 'Enter') send(); else if (e.key === 'Escape') input.blur(); e.stopPropagation(); });
  const av = kidAvailable(), sp = speechAvailable(), ph = S.kid.phase;
  const mic = el('button', { class: 'mic' + (ph === 'recording' ? ' rec' : '') + (!av.ok || !sp.ok ? ' unavail' : ''), 'data-key': 'mic', 'data-testid': 'mic', 'aria-label': 'Hold to talk to KID' }, el('span', { class: 'mic-in' }, icon('mic', 18)));
  wireMic(mic);
  const status = el('div', { class: 'kid-status' + (ph === 'unavailable' || ph === 'error' || !av.ok ? ' err' : ''), 'data-testid': 'kid-status', 'data-phase': ph }, ph === 'recording' ? el('span', { class: 'cmdlevel', 'aria-hidden': 'true' }, [0, 1, 2, 3, 4].map(() => el('i'))) : null, ph === 'recording' ? ' ' : null,
    !av.ok ? 'Unavailable: ' + (S.master.on ? 'Mastering mode is on' : 'not processing') : (ph === 'ready' || ph === 'unavailable') && !sp.ok ? (sp.kind === 'missing' ? 'Speech isn’t set up yet. Typing works.' : 'Microphone blocked. Typing works.') : PHASE_TEXT[ph]);
  return el('section', { class: 'panel kidbar', 'aria-label': 'Talk to KID' }, tip(el('span', { class: 'micro', 'data-testid': 'kid-label' }, 'Tell KID · guided assistant'), 'KID is a guided creative assistant: GlitchKid’s rules turn your words into ideas made of its own controls. It is not generative AI, and it never applies anything.'),
    el('div', { class: 'kb-row' }, tip(mic, 'Push to talk: hold, speak, let go. A separate command mic, open only while held. Or hold K.'), input, el('button', { class: 'btn small', 'data-key': 'kid-send', 'data-testid': 'kid-send', onclick: send }, 'Send')), status);
}
function wireMic(b) {
  b.addEventListener('pointerdown', (e) => { e.preventDefault(); try { b.setPointerCapture(e.pointerId); } catch { /* synthetic events */ } micDown(); });
  b.addEventListener('pointerup', micUp); b.addEventListener('pointercancel', micUp);
  b.addEventListener('keydown', (e) => { if ((e.key === ' ' || e.key === 'Enter') && !e.repeat) { e.preventDefault(); micDown(); } });
  b.addEventListener('keyup', (e) => { if (e.key === ' ' || e.key === 'Enter') { e.preventDefault(); micUp(); } });
}
function ago(t) { const m = Math.round((Date.now() - t) / 60000); return m < 1 ? 'just now' : m < 60 ? m + ' min ago' : Math.round(m / 60) + ' h ago'; }
function notebookEl() {
  const list = S.notebook.map((e, i) => {
    const st = entryState(e), h = hearing('note', e.id);
    return el('div', { class: 'nb-entry' + (st ? ' active' : ''), 'data-testid': 'nb-' + i, 'aria-label': e.name },
      el('span', { class: 'nb-name' }, e.name), el('span', { class: 'nb-flags' }, h ? el('span', { class: 'sticker hearing' }, 'HEARING') : null, st === 'now' ? el('span', { class: 'sticker now' }, 'ON NOW') : st === 'changed' ? el('span', { class: 'sticker changed' }, 'ON NOW · CHANGED') : null),
      e.note ? el('span', { class: 'nb-note' }, e.note) : null, el('span', { class: 'nb-meta' }, 'kept ' + ago(e.at) + ' · from ' + e.from),
      el('div', { class: 'nb-acts' },
        tip(el('button', { class: 'btn small', 'data-key': 'nb-hear-' + i, 'data-testid': 'nb-hear-' + i, 'aria-pressed': h ? 'true' : 'false', disabled: st === 'now', onclick: () => toggleAudition({ kind: 'note', id: e.id, name: e.name, get: () => e.sound }) }, h ? 'Stop' : 'Hear'), 'Compare: hear this version without switching to it.'),
        tip(el('button', { class: 'btn small', 'data-key': 'nb-use-' + i, 'data-testid': 'nb-use-' + i, disabled: st === 'now', onclick: () => useEntry(e) }, 'Use'), 'Switch to this version (Undo goes back).'),
        tip(el('button', { class: 'btn small ghost', 'data-key': 'nb-ren-' + i, 'data-testid': 'nb-ren-' + i, onclick: () => openModal({ kind: 'rename', id: e.id }) }, 'Rename'), 'Rename or change the note.'),
        tip(el('button', { class: 'btn small ghost icon', 'data-key': 'nb-del-' + i, 'data-testid': 'nb-del-' + i, 'aria-label': 'Remove “' + e.name + '”', onclick: () => openModal({ kind: 'remove', id: e.id }) }, '✕'), 'Remove from the notebook (asks first).')));
  });
  return el('section', { class: 'notebook panel', 'aria-label': 'Session notebook', 'data-testid': 'notebook' },
    el('div', { class: 'nb-head' }, el('span', { class: 'hand' }, 'session notebook'), tip(el('button', { class: 'btn small keep', 'data-key': 'nb-keep', 'data-testid': 'nb-keep', onclick: () => openModal({ kind: 'keep', name: '', from: S.preset.name }) }, 'Keep this sound'), 'Keep what you hear now in this session’s notebook, with a name. Separate from factory sounds and presets.')),
    el('div', { class: 'nb-list' }, ...(list.length ? list : [el('div', { class: 'empty', 'data-testid': 'nb-empty' }, 'Nothing kept yet. When something sounds right, even by accident, press ', el('b', null, 'Keep this sound'), '.')])));
}

// ---------------------------------------------------------------- SHAPE, LAB
function areaHead(title, line, extra) { return el('div', { class: 'ws-head' }, el('div', null, el('h1', { class: 'h1' }, title), el('div', { class: 'ws-line' }, line)), extra || null); }
function viewShape() {
  const T = [['clean', 'CLEAN', '7·9', ['clean']], ['tune', 'TUNE', '1', ['tune']], ['tone', 'TONE', '6·10·11', ['eq', 'warmth', 'air']], ['dynamics', 'DYNAMICS', '8', ['comp']], ['space', 'SPACE', '5·13·14', ['doubler', 'echo', 'dimension']], ['output', 'OUTPUT', '15–17', ['gain', 'mix', 'limiter']]];
  const tabs = el('div', { class: 'tabs', role: 'tablist', 'aria-label': 'SHAPE sections' }, T.map(([k, label, st, mods]) => {
    const dot = el('span', { class: 'td' });
    const b = el('button', { class: 'tab', role: 'tab', 'data-key': 'tab-' + k, 'data-testid': 'tab-' + k, 'aria-selected': S.view.shapeTab === k ? 'true' : 'false', onclick: () => { S.view.shapeTab = k; changed(true); } }, dot, label, el('span', { class: 'ts' }, st));
    bind(() => { const o = mods.some((m) => modOn(m, S.sound)); dot.className = 'td' + (o ? ' on' : ''); b.setAttribute('aria-label', label + ', stages ' + st + ', ' + (o ? 'on' : 'off')); });
    return b;
  }));
  const body = { clean: shapeClean, tune: shapeTune, tone: shapeTone, dynamics: shapeDynamics, space: shapeSpace, output: outputCard }[S.view.shapeTab]();
  return el('div', { class: 'screen scroll', 'data-testid': 'screen-shape' }, masterBanner(), areaHead('shape', 'Every control, grouped by what it does. The numbers are each stage’s real place in the chain; grouping never reorders it.'), tabs, el('div', { style: { height: '12px' } }), body);
}
function shapeSpace() {
  return el('div', { class: 'grid2' },
    labCard('doubler', 'space-card', { sw: sw('dbl_on'), reset: resetModule('doubler', ['dbl_on', 'dbl_amount', 'dbl_width', 'dbl_detune', 'dbl_timing']), desc: 'A second, slightly different you. It runs early (stage 5), so EQ and compression treat the doubles like your voice.' },
      [knob('dbl_amount', { label: 'Amount', offWhen: () => !on(S.sound, 'dbl_on') }), knob('dbl_width', { label: 'Width', offWhen: () => !on(S.sound, 'dbl_on') })], () => el('div', { class: 'mod-row' }, knob('dbl_detune', { label: 'Detune' }), knob('dbl_timing', { label: 'Timing' }))),
    labCard('echo', 'space-card', { sw: sw('echo_on'), reset: resetModule('echo', ['echo_on', 'echo_sync', 'echo_time', 'echo_div', 'echo_fb', 'echo_tone', 'echo_width', 'echo_mix']), desc: 'Repeats that move with your song. GlitchKid has no reverb: Echo and Dimension are its space.' },
      [knob('echo_mix', { label: 'Mix', offWhen: () => !on(S.sound, 'echo_on') }), el('div', { style: { display: 'flex', flexDirection: 'column', gap: '8px' } }, el('div', { class: 'micro' }, 'Timing'), sw('echo_sync', { text: (x) => (x ? 'SYNC' : 'FREE') }), sel('echo_div'))],
      () => el('div', { class: 'mod-row' }, knob('echo_fb'), knob('echo_tone'), knob('echo_width'), knob('echo_time', { help: 'Used when Sync is off' }))),
    labCard('dimension', 'space-card', { sw: sw('dim_on'), reset: resetModule('dimension', ['dim_on', 'dim_width', 'dim_detune', 'dim_depth', 'dim_mix']), desc: 'Stereo width and slow movement around the voice.' },
      [knob('dim_mix', { label: 'Mix', offWhen: () => !on(S.sound, 'dim_on') }), knob('dim_width', { label: 'Width', offWhen: () => !on(S.sound, 'dim_on') })], () => el('div', { class: 'mod-row' }, knob('dim_depth', { label: 'Depth' }), knob('dim_detune', { label: 'Detune' }))));
}
function viewLab() {
  return el('div', { class: 'screen scroll', 'data-testid': 'screen-lab' }, masterBanner(), areaHead('lab', 'Experimental, kept apart so the main path stays simple. Formant, Harmony and Mutation run early (2–4); Stutter slices the finished voice (12).'),
    el('div', { class: 'grid2' },
      labCard('formant', '', { reset: resetModule('formant', ['formant_shift']), desc: 'The size of the voice, not its pitch. Down: bigger. Up: smaller. 0 = off; about 21 ms latency while not 0.' }, [knob('formant_shift', { size: 76, label: 'Shift' })]),
      labCard('harmony', '', { sw: sw('harm_on'), reset: resetModule('harmony', ['harm_on', 'harm_interval', 'harm_level']), desc: 'One extra voice at a fixed interval. Not key-aware. Adds Pitch Tune’s latency while on.' },
        [el('div', null, el('div', { class: 'micro', style: { marginBottom: '6px' } }, 'Interval'), sel('harm_interval')), knob('harm_level', { label: 'Level', offWhen: () => !on(S.sound, 'harm_on') })]),
      labCard('mutation', '', { sw: sw('mut_on'), reset: resetModule('mutation', ['mut_on', 'mut_amount', 'mut_character', 'mut_blend']), desc: 'Bends the voice into something new. MORE: its character and how much you hear.' },
        [knob('mut_amount', { size: 76, label: 'Amount', offWhen: () => !on(S.sound, 'mut_on') })], () => el('div', { class: 'mod-row' }, knob('mut_character', { help: 'Down ← robot → up' }), knob('mut_blend'))),
      labCard('stutter', '', { sw: sw('stutter_on'), reset: resetModule('stutter', ['stutter_on', 'stutter_rate', 'stutter_mix']), desc: 'Tempo-locked slices of the finished voice.' },
        [el('div', null, el('div', { class: 'micro', style: { marginBottom: '6px' } }, 'Slice'), seg('stutter_rate')), knob('stutter_mix', { label: 'Mix', offWhen: () => !on(S.sound, 'stutter_on') })])));
}
function masterBanner() { return S.master.on ? el('div', { class: 'banner steel', 'data-testid': 'banner-master', style: { marginBottom: '10px' } }, el('b', null, 'Mastering mode is on. '), 'These vocal controls are not in use in this instance until you switch it off in MASTER.') : null; }

// ---------------------------------------------------------------- WORLDS
function viewWorlds() {
  const scenes = [{ kind: 'default', name: 'Factory Default', tag: 'Every control at its default: your vocal passes through untouched.', hue: '#8a80a0' },
    ...PROFILES.map((p, i) => ({ kind: 'factory', i, name: p.name, tag: p.tag, hue: ['#8fb6d8', '#b47cff', '#ff4fb8', '#7d8bff', '#ffc163', '#ff8dd0', '#ff6479', '#5ee3ff'][i] }))];
  const drafts = WORLDS.map((w) => ({ kind: 'world', w, name: w.name, tag: w.tag, hue: w.hue }));
  const mine = S.lib.map((p) => ({ kind: 'user', p, name: p.name, tag: 'Saved by you. In every project.', hue: 'var(--acid)' }));
  const tape = (sc) => {
    const cur = S.preset.name === sc.name && (sc.kind === 'user') === (S.preset.type === 'user'), h = hearing('scene', sc.name);
    const badge = sc.kind === 'world' ? el('span', { class: 'badge draft' }, 'DRAFT · NOT LISTENING-TESTED') : sc.kind === 'user' ? el('span', { class: 'badge real' }, 'YOURS · SAVED PRESET') : el('span', { class: 'badge real' }, 'FACTORY · IN THE PLUG-IN');
    return el('article', { class: 'cassette' + (sc.kind === 'world' ? ' draft' : '') + (cur ? ' current' : ''), 'data-testid': 'scene-' + sc.name, 'aria-label': sc.name + ({ world: ', draft world', user: ', your saved sound' }[sc.kind] || ', factory sound') },
      el('div', { class: 'cs-label' }, el('div', { class: 'cs-stripe', style: { background: sc.hue } }), el('div', { class: 'cs-name' }, sc.name)),
      el('div', { style: { display: 'flex', gap: '5px', flexWrap: 'wrap' } }, badge, cur ? el('span', { class: 'sticker now' }, 'IN USE') : null, h ? el('span', { class: 'sticker hearing' }, 'HEARING') : null),
      el('div', { class: 'cs-tag' }, sc.tag),
      el('div', { class: 'cs-acts' },
        tip(el('button', { class: 'btn small', 'data-key': 'sc-hear-' + sc.name, 'data-testid': 'sc-hear-' + sc.name, 'aria-pressed': h ? 'true' : 'false', onclick: () => toggleAudition({ kind: 'scene', id: sc.name, name: sc.name, get: () => sceneTarget(sc) }) }, h ? 'Stop' : 'Hear'), 'Audition it without loading it.'),
        tip(el('button', { class: 'btn small', 'data-key': 'sc-look-' + sc.name, 'data-testid': 'sc-look-' + sc.name, onclick: () => lookScene(sc) }, 'Look'), 'See every setting it changes; take all of it or only what you tick.'),
        tip(el('button', { class: 'btn small primary', 'data-key': 'sc-use-' + sc.name, 'data-testid': 'sc-use-' + sc.name, onclick: () => useScene(sc) }, 'Use'), sc.kind === 'user' ? 'Load your saved sound: every control as you saved it (Undo goes back).' : 'Start from it (Undo goes back). Your tuning, gain, mix, CLEAN, AIR LAB and limiter stay.'),
        sc.kind === 'user' ? tip(el('button', { class: 'btn small ghost icon', 'data-key': 'sc-del-' + sc.name, 'data-testid': 'sc-del-' + sc.name, 'aria-label': 'Delete “' + sc.name + '”', onclick: () => openModal({ kind: 'deluser', name: sc.name }) }, '✕'), 'Delete this saved sound from every project (asks first).') : null),
      cur ? el('div', { class: 'cs-acts' }, el('button', { class: 'btn small ghost', 'data-key': 'sc-cust-' + sc.name, onclick: () => go('shape') }, 'Customise in SHAPE'), isModified() ? el('button', { class: 'btn small keep', 'data-key': 'sc-keep-' + sc.name, 'data-testid': 'sc-keep-' + sc.name, onclick: () => openModal({ kind: 'keep', name: 'My ' + sc.name, from: sc.name }) }, 'Keep my version') : null) : null);
  };
  const shelf = (attrs, list) => { const box = el('div', attrs); bind(() => box.replaceChildren(...list.map(tape))); return box; };
  return el('div', { class: 'screen scroll', 'data-testid': 'screen-worlds' }, masterBanner(), areaHead('worlds', 'Sound directions to start from. Hear without loading, look inside, use, customise, keep your version.'),
    el('div', { class: 'section-h' }, el('span', { class: 'h2' }, 'your saved sounds'), el('span', { class: 'dim' }, 'Presets you saved with Save…, in every project')),
    mine.length ? shelf({ class: 'tapes', 'data-testid': 'my-tapes' }, mine) : el('div', { class: 'empty', 'data-testid': 'my-tapes-empty', style: { marginBottom: '6px' } }, 'Nothing saved yet. ', el('b', null, 'Save…'), ' in the top bar keeps a sound for every project; the notebook keeps ideas for this session.'),
    el('div', { class: 'section-h' }, el('span', { class: 'h2' }, 'factory tapes'), el('span', { class: 'dim' }, 'Real presets in the plug-in today (8 profiles + Factory Default)')),
    shelf({ class: 'tapes' }, scenes),
    el('div', { class: 'section-h' }, el('span', { class: 'h2' }, 'draft worlds'), el('span', { class: 'dim' }, 'Valid settings designed in this prototype. Not factory presets; they need sound design and a listening test')),
    shelf({ class: 'tapes' }, drafts));
}

// ---------------------------------------------------------------- TOOLS
function viewTools() {
  const t = S.view.toolTab;
  const tabs = el('div', { class: 'tabs', role: 'tablist', 'aria-label': 'Tools' }, [['beat', 'BEAT LOCK'], ['reference', 'REFERENCE MATCH'], ['build', 'BUILD MY MIX']].map(([k, label]) =>
    el('button', { class: 'tab', role: 'tab', 'data-key': 'tool-' + k, 'data-testid': 'tool-' + k, 'aria-selected': t === k ? 'true' : 'false', onclick: () => { S.view.toolTab = k; changed(true); } }, label)));
  const body = t === 'beat' ? toolBeat() : t === 'reference' ? toolReference() : toolBuild();
  return el('div', { class: 'screen scroll', 'data-testid': 'screen-tools' }, masterBanner(), areaHead('tools', 'Fit the vocal to your track. Each tool proposes moves on GlitchKid’s own controls, you review every one, and Undo takes them back.', tabs), body);
}
function toolBeat() {
  const routed = S.sim.sidechain === 'beat', b = S.beat;
  const bands = [['Low', 30, 82], ['Low-mid', 62, 70], ['Mid', 74, 60], ['Presence', 58, 77], ['Air', 48, 40]];
  return el('section', { class: 'card mod', 'data-testid': 'tool-beat-panel' },
    el('div', { class: 'mod-head' }, el('span', { class: 'mod-name' }, 'Beat Lock'), el('span', { class: 'badge real' }, 'IN THE PLUG-IN'), el('span', { class: 'mod-sp' })),
    el('p', { class: 'mod-desc' }, 'How your vocal sits against the instrumental, and a few conservative moves on the vocal’s own controls. It never processes the beat, never moves your faders (balance is advice only), and has no reverb to suggest.'),
    routed ? el('div', { class: 'banner info', 'data-testid': 'beat-routed' }, el('b', null, 'Instrumental input: receiving the beat. '), '(Prototype: simulated routing.)')
      : el('div', { class: 'banner warn', 'data-testid': 'beat-unrouted' }, el('b', null, 'Nothing is routed to GlitchKid’s Instrumental input. '), 'Beat Lock needs the beat on GlitchKid’s second (sidechain) input: in FL Studio, send the beat’s Mixer track to this track as a sidechain and pick it as GlitchKid’s input. A track that already mixes vocal and beat is measured as “the vocal”. (FL Studio’s exact clicks are unverified here.)'),
    el('div', { class: 'mod-row' }, el('button', { class: 'btn primary', 'data-key': 'beat-analyse', 'data-testid': 'beat-analyse', disabled: !routed || S.sim.signal === 'none' || b.busy, onclick: beatAnalyse }, b.busy ? 'Listening… ' + b.progress + '%' : b.analysed ? 'Listen again' : 'Listen to both'),
      b.analysed ? el('button', { class: 'btn keep', 'data-key': 'beat-review', 'data-testid': 'beat-review', onclick: beatProposal }, 'Look at the suggestions') : null),
    b.analysed ? el('div', null, el('div', { style: { display: 'flex', gap: '10px', alignItems: 'center', margin: '4px 0 6px' } }, el('span', { class: 'badge sim' }, 'SIMULATED MEASUREMENTS'), el('span', { class: 'legend' }, el('span', null, el('i', { style: { background: 'var(--pink)' } }), 'your vocal'), el('span', null, el('i', { style: { background: '#6b7c96' } }), 'the beat')), el('span', { class: 'dim' }, 'Five coarse bands, digital level, not LUFS')),
      el('div', { class: 'bars', 'aria-label': 'Vocal and beat level in five bands (simulated)' }, bands.map(([n, v, bb]) => el('div', { class: 'bcol' }, el('div', { class: 'bpair' }, el('i', { class: 'v', style: { height: v + '%' } }), el('i', { class: 'b', style: { height: bb + '%' } })), el('span', { class: 'bl' }, n))))) : null);
}
function toolReference() {
  const r = S.ref;
  const ready = r.refDone && r.vocalDone;
  return el('section', { class: 'card mod', 'data-testid': 'tool-ref-panel' },
    el('div', { class: 'mod-head' }, el('span', { class: 'mod-name' }, 'Reference Match'), el('span', { class: 'badge real' }, 'IN THE PLUG-IN'), el('span', { class: 'mod-sp' })),
    el('p', { class: 'mod-desc' }, 'Tonal guidance from a song you have the right to use: small EQ moves toward its tone, never a copy. It compares tone only (not dynamics, width, space or loudness), moves only Body, Presence and Air shelf, at most 3 dB a move and half the measured difference.'),
    el('div', { class: 'micro' }, '1 · Choose a reference'),
    el('div', { class: 'mod-row', style: { alignItems: 'center' } }, el('button', { class: 'btn', 'data-key': 'ref-choose', 'data-testid': 'ref-choose', onclick: () => { r.file = 'reference_track.wav'; r.refDone = false; changed(true); toast('Prototype: “reference_track.wav” chosen (simulated file picker).'); } }, r.file ? 'Choose another…' : 'Choose a song file…'),
      el('span', { class: 'sub' }, r.file || 'No file chosen'),
      el('label', { class: 'sub', style: { display: 'flex', gap: '6px', alignItems: 'center' } }, el('button', { class: 'check', role: 'checkbox', 'data-key': 'ref-rights', 'data-testid': 'ref-rights', 'aria-checked': r.rights ? 'true' : 'false', 'aria-label': 'I have the right to use this song as a reference', onclick: () => { r.rights = !r.rights; changed(true); } }), 'I have the right to use this song')),
    el('div', { class: 'mod-row' }, el('button', { class: 'btn', 'data-key': 'ref-analyse', 'data-testid': 'ref-analyse', disabled: !r.file || !r.rights || !!r.busy, onclick: () => refStep('ref') }, r.busy === 'ref' ? 'Analysing…' : r.refDone ? '✓ Reference analysed' : 'Analyse the reference')),
    el('div', { class: 'micro' }, '2 · Analyse your vocal'),
    el('div', { class: 'mod-row' }, el('button', { class: 'btn', 'data-key': 'ref-vocal', 'data-testid': 'ref-vocal', disabled: !!r.busy, onclick: () => refStep('vocal') }, r.busy === 'vocal' ? 'Listening…' : r.vocalDone ? '✓ Your vocal analysed' : 'Analyse my vocal'), el('span', { class: 'dim' }, 'Play your vocal in FL Studio while it listens.')),
    el('div', { class: 'micro' }, '3 · Look, hear, apply'),
    ready ? el('div', null, el('span', { class: 'badge sim' }, 'SIMULATED MEASUREMENTS'), ' ', el('span', { class: 'sub' }, 'Confidence: medium (a full mix is never rated higher).')) : el('div', { class: 'dim' }, 'Nothing is suggested until both are analysed.'),
    hslider({ key: 'ref-intensity', name: 'Match intensity', lo: '0%', hi: '100%', get: () => r.intensity / 100, set: (v) => { r.intensity = Math.round(v * 100); changed(); }, text: () => r.intensity + '%', help: 'Scales the suggested moves. They never exceed half the measured difference.' }),
    el('div', { class: 'mod-row' }, el('button', { class: 'btn keep', 'data-key': 'ref-review', 'data-testid': 'ref-review', disabled: !ready, onclick: refProposal }, 'Look at the suggestions')));
}
function toolBuild() {
  const b = S.build;
  if (!b) return el('section', { class: 'card mod', 'data-testid': 'tool-build-panel' }, el('div', { class: 'mod-head' }, el('span', { class: 'mod-name' }, 'Build My Mix'), el('span', { class: 'badge real' }, 'IN THE PLUG-IN')),
    el('p', { class: 'mod-desc' }, 'Listens to your take, you pick a direction from the 8 factory profiles, and you review every change before anything is applied. Your tuning, gain, mix, CLEAN, AIR LAB and limiter are never changed.'),
    el('div', null, el('button', { class: 'btn primary', 'data-key': 'build-start', 'data-testid': 'build-start', onclick: startBuild }, 'Start Build My Mix')));
  const items = [['Level', 'peaks around −12 dBFS'], ['Noise floor', 'about −62 dBFS'], ['Tone', 'a little low-mid build-up'], ['Dynamics', 'loud words about 4 dB above the rest']];
  return el('section', { class: 'card mod', 'data-testid': 'tool-build-panel' }, el('div', { class: 'mod-head' }, el('span', { class: 'mod-name' }, 'Build My Mix'), el('span', { class: 'badge' }, 'STEP ' + b.step + ' OF 2'), el('span', { class: 'mod-sp' }), el('button', { class: 'btn small ghost', 'data-key': 'build-cancel', 'data-testid': 'build-cancel', onclick: () => { S.build = null; toast('Build My Mix cancelled. Nothing changed.'); changed(true); } }, 'Cancel')),
    b.step === 1 ? el('div', { style: { display: 'flex', flexDirection: 'column', gap: '8px' } },
      S.sim.signal === 'none' ? el('div', { class: 'banner warn', 'data-testid': 'build-nosignal' }, el('b', null, 'No vocal heard yet. '), 'Press play in FL Studio, or skip listening and use a direction as designed (not fitted to your voice).') : null,
      el('div', { class: 'progress' }, el('i', { style: { width: b.listen + '%' } })),
      el('div', { class: 'checklist' }, items.map(([k, v], i) => el('div', null, b.listen >= (i + 1) * 25 ? el('span', { class: 'ok' }, '✓ ') : '○ ', el('b', null, k), b.listen >= (i + 1) * 25 ? ': ' + v : ''))),
      b.listened ? el('span', { class: 'badge sim', style: { alignSelf: 'flex-start' } }, 'SIMULATED MEASUREMENTS') : null,
      el('div', { class: 'mod-row' }, b.listened ? el('button', { class: 'btn primary', 'data-key': 'build-next', 'data-testid': 'build-next', onclick: () => { b.step = 2; changed(true); } }, 'Next: direction') : el('button', { class: 'btn primary', 'data-key': 'build-listen', 'data-testid': 'build-listen', disabled: S.sim.signal === 'none' || b.listening, onclick: buildListen }, b.listening ? 'Listening…' : 'Start listening'),
        S.sim.signal === 'none' ? el('button', { class: 'btn', 'data-key': 'build-skip', 'data-testid': 'build-skip', onclick: () => { b.step = 2; changed(true); } }, 'Skip: use as designed') : null))
      : el('div', { style: { display: 'flex', flexDirection: 'column', gap: '8px' } },
        el('div', { class: 'pgrid', role: 'radiogroup', 'aria-label': 'Direction' }, PROFILES.map((p, i) => el('button', { class: 'pcard', role: 'radio', 'data-key': 'dir-' + i, 'data-testid': 'dir-' + p.name, 'aria-checked': b.profile === i ? 'true' : 'false', onclick: () => { b.profile = i; changed(true); } }, el('b', null, p.name), el('span', null, p.tag)))),
        hslider({ key: 'b-int', name: 'Intensity', lo: 'Subtle', hi: 'As designed', get: () => b.intensity / 100, set: (v) => { b.intensity = Math.round(v * 100); changed(); }, text: () => b.intensity + '%', help: 'Scales echo, Dimension, Doubler and saturation: 100% = as designed, 0% = half.' }),
        el('div', { class: 'mod-row' }, el('button', { class: 'btn', 'data-key': 'build-back', onclick: () => { b.step = 1; changed(true); } }, 'Back'), el('button', { class: 'btn keep', 'data-key': 'build-review', 'data-testid': 'build-review', onclick: buildReview }, 'Review the changes'))));
}

// ---------------------------------------------------------------- MASTER (the existing Mastering Lab, kept apart from vocal shaping)
function mdial(id) {
  const p = MP[id];
  const box = el('div', { class: 'knob' });
  const v = el('div', { class: 'k-value', 'data-testid': 'mval-' + id });
  const dial = el('div', { class: 'k-dial', role: 'slider', tabindex: 0, 'data-key': 'm-' + id, 'aria-label': p.name, 'aria-valuemin': p.min, 'aria-valuemax': p.max });
  const svg = svgFrom('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64"><path class="k-track" d="' + arcPath(32, 32, 27, -135, 135) + '"/><path class="k-val" d=""/><circle class="k-cap" cx="32" cy="32" r="19.5"/><line class="k-ptr" x1="32" y1="32" x2="32" y2="17"/></svg>');
  dial.append(svg);
  const sync = () => { const val = S.master[id], n = (val - p.min) / (p.max - p.min), a = -135 + 270 * n; svg.querySelector('.k-val').setAttribute('d', n < 0.003 ? '' : arcPath(32, 32, 27, -135, a)); svg.querySelector('.k-ptr').setAttribute('transform', 'rotate(' + a.toFixed(1) + ' 32 32)'); v.textContent = (p.unit === 'dB' && val > 0 ? '+' : '') + (p.step < 1 ? val.toFixed(1) : Math.round(val)) + (p.unit ? ' ' + p.unit : ''); dial.setAttribute('aria-valuenow', val); };
  const setV = (x) => { S.master[id] = clamp(Math.round(x / p.step) * p.step, p.min, p.max); sync(); };
  dial.addEventListener('pointerdown', (e) => { e.preventDefault(); dial.focus(); dial.setPointerCapture(e.pointerId); const v0 = S.master[id], y0 = e.clientY;
    const mv = (ev) => setV(v0 + (y0 - ev.clientY) / 170 * (p.max - p.min)); const up = () => { dial.removeEventListener('pointermove', mv); dial.removeEventListener('pointerup', up); };
    dial.addEventListener('pointermove', mv); dial.addEventListener('pointerup', up); });
  dial.addEventListener('keydown', (e) => { const d = { ArrowUp: 1, ArrowRight: 1, ArrowDown: -1, ArrowLeft: -1 }[e.key]; if (d) { e.preventDefault(); setV(S.master[id] + d * (p.max - p.min) / 100); } });
  binders.push(sync); sync();
  box.append(el('div', { class: 'k-label' }, p.name), tip(dial, p.name + ' (' + id + '): ' + p.min + ' to ' + p.max + (p.unit ? ' ' + p.unit : '') + ', default ' + p.def + '.'), v);
  return box;
}
function viewMaster() {
  const m = S.master;
  const sw2 = tip(el('button', { class: 'sw', role: 'switch', 'data-key': 'master-on', 'data-testid': 'master-on', 'aria-checked': m.on ? 'true' : 'false', onclick: () => (m.on ? (m.on = false, toast('Mastering mode off. The vocal chain is back.'), changed(true)) : openModal({ kind: 'master-on' })) }, el('span', { class: 'sw-track' }, el('span', { class: 'sw-knob' })), el('span', null, m.on ? 'MASTERING MODE ON' : 'MASTERING MODE OFF')), 'Mastering mode (master_on, not automatable): this instance masters a full mix instead of shaping a vocal.');
  const meters = [['Integrated', '−9.8 LUFS'], ['Short-term', '−9.1 LUFS'], ['True peak', '−1.0 dBTP'], ['Limiting', '−2.4 dB']];
  return el('div', { class: 'screen scroll master-zone', 'data-testid': 'screen-master' },
    areaHead('master', 'The Mastering Lab, unchanged in behaviour. It is for a finished stereo mix on FL Studio’s master track, not for your vocal.'),
    el('div', { class: 'banner steel', style: { marginBottom: '12px' } }, el('b', null, 'Not vocal shaping. '), 'With Mastering mode on, this whole instance runs the mastering chain instead of the vocal chain, and its latency changes. Put a separate GlitchKid on the master track for this.'),
    el('div', { class: 'card mod' }, el('div', { class: 'mod-head' }, el('span', { class: 'mod-name' }, 'Mastering mode'), el('span', { class: 'badge real' }, 'IN THE PLUG-IN'), el('span', { class: 'mod-sp' }), sw2),
      m.on ? el('div', { style: { display: 'flex', flexDirection: 'column', gap: '12px' } },
        el('div', null, el('div', { class: 'micro', style: { marginBottom: '6px' } }, 'Style · a real processing configuration'), el('div', { class: 'seg', role: 'radiogroup', 'aria-label': 'Master style' }, MASTER_STYLES.map((s, i) => el('button', { role: 'radio', 'data-key': 'mstyle-' + i, 'aria-checked': m.master_style === i ? 'true' : 'false', onclick: () => { m.master_style = i; changed(true); } }, s)))),
        el('div', { class: 'mod-row' }, Object.keys(MP).map(mdial)),
        el('div', null, el('div', { style: { display: 'flex', gap: '8px', alignItems: 'center', marginBottom: '6px' } }, el('span', { class: 'micro' }, 'Output'), el('span', { class: 'badge sim' }, 'SIMULATED')), el('div', { class: 'meterbig' }, meters.map(([k, v]) => el('div', { class: 'mb' }, el('b', null, v), el('span', null, k))))),
        el('div', { class: 'mod-row' }, el('button', { class: 'btn', 'data-key': 'm-load', onclick: () => { m.mix = 'my_mix.wav'; toast('Prototype: “my_mix.wav” loaded for analysis (simulated).'); changed(true); } }, m.mix ? 'Load another mix…' : 'Load a mix…'), el('button', { class: 'btn', 'data-key': 'm-export', disabled: !m.mix, onclick: () => toast('Prototype: export simulated. The plug-in renders a 24-bit WAV offline with these settings.') }, 'Export master…'), el('span', { class: 'dim' }, m.mix ? 'Loaded: ' + m.mix : 'Or put this instance on the master track and play.')))
        : el('p', { class: 'mod-desc' }, 'Off: this instance shapes a vocal. Turn it on only on a GlitchKid that sits on the master track.')));
}

// ---------------------------------------------------------------- the review drawer (cards, scenes, Beat Lock, Reference Match, Build My Mix)
function renderDrawer() {
  const r = S.review;
  if (!r) { if (drawerEl) { drawerEl.remove(); drawerEl = null; } return; }
  const prev = drawerEl ? drawerEl.querySelector('.dr-body') : null, st = prev && drawerEl.dataset.title === r.title ? prev.scrollTop : 0;
  const n = selectedCount(r), total = r.rows.length;
  const groups = []; let last = null, g = null;
  for (const row of r.rows) {
    const m = PM[row.id].mod;
    if (m !== last) { g = el('div', { class: 'prop-group' }, el('div', { class: 'pg-name' }, el('span', null, stageOfId(row.id) + ' · ' + MOD[m].name.toUpperCase()), el('span', null, MOD[m].ws === 'lab' ? 'LAB' : 'SHAPE'))); groups.push(g); last = m; }
    const tags = [];
    if (row.manual) tags.push(el('span', { class: 'badge warn' }, 'YOU MOVED THIS · YOUR VALUE IS KEPT'));
    if (row.limited) tags.push(el('span', { class: 'badge' }, 'AT ITS LIMIT'));
    if (row.sim) tags.push(el('span', { class: 'badge sim' }, 'SIMULATED MEASUREMENT'));
    g.append(el('div', { class: 'prow' + (row.sel && !row.manual ? '' : ' unsel') + (row.manual ? ' manual' : ''), 'data-testid': 'row-' + row.id },
      el('button', { class: 'check', role: 'checkbox', 'data-key': 'chk-' + row.id, 'data-testid': 'chk-' + row.id, 'aria-checked': row.sel && !row.manual ? 'true' : 'false', 'aria-label': 'Include the change to ' + PM[row.id].name, disabled: row.manual, onclick: () => { row.sel = !row.sel; changed(); } }),
      el('span', { class: 'pn' }, PM[row.id].name), el('span', { class: 'pv' }, fmt(row.id, S.sound[row.id]), ' → ', el('b', null, fmt(row.id, row.to))), el('span', { class: 'pr' }, row.why), tags.length ? el('span', { class: 'ptags' }, tags) : null));
  }
  const sticker = { idea: ['idea', 'IDEA'], kid: ['kid', 'KID'], accident: ['accident', 'ACCIDENT'], factory: ['idea', 'FACTORY'], draft: ['idea', 'DRAFT WORLD'], user: ['kept', 'YOURS'], tool: ['kid', 'TOOL'] }[r.sticker] || ['idea', 'IDEA'];
  const listening = !!S.audition && S.audition.kind === 'review';
  drawerEl = el('section', { class: 'drawer', id: 'gk-drawer', 'data-testid': 'review', 'data-title': r.title, role: 'dialog', 'aria-label': 'What changes: ' + r.title },
    el('div', { class: 'dr-head' }, el('div', { class: 'dr-title' }, el('div', { style: { display: 'flex', gap: '6px', alignItems: 'center' } }, el('span', { class: 'micro' }, 'What changes'), el('span', { class: 'sticker ' + sticker[0] }, sticker[1])), el('div', { class: 'h2' }, r.title), r.sub ? el('div', { class: 'sub' }, r.sub) : null, r.intent ? el('div', { class: 'sub dim' }, r.intent) : null),
      el('button', { class: 'btn icon ghost', 'data-key': 'dr-close', 'data-testid': 'review-close', 'aria-label': 'Close (nothing changes)', onclick: closeReview }, icon('close', 18))),
    el('div', { class: 'dr-body', 'data-testid': 'review-body' }, r.advisory ? el('div', { class: 'banner info', 'data-testid': 'advisory' }, r.advisory) : null,
      el('div', { style: { display: 'flex', alignItems: 'center', gap: '6px' } }, el('span', { class: 'dim', 'data-testid': 'review-count', style: { flex: 1 } }, total + ' change' + (total === 1 ? '' : 's') + ' · ' + n + ' ticked · in processing order'),
        el('button', { class: 'btn small ghost', 'data-key': 'tick-all', onclick: () => { r.rows.forEach((x) => { if (!x.manual) x.sel = true; }); changed(); } }, 'All'), el('button', { class: 'btn small ghost', 'data-key': 'tick-none', onclick: () => { r.rows.forEach((x) => { x.sel = false; }); changed(); } }, 'None')),
      ...(total ? groups : [el('div', { class: 'empty' }, 'Nothing to change: this is already your sound.')])),
    el('div', { class: 'dr-foot' },
      el('div', { class: 'ff' }, el('span', { class: 'micro' }, 'Listen'), el('div', { class: 'listen', role: 'group', 'aria-label': 'Listen to' },
        el('button', { 'data-key': 'listen-cur', 'data-testid': 'listen-current', 'aria-pressed': listening ? 'false' : 'true', onclick: () => stopAudition() }, 'Current'),
        el('button', { class: 'p', 'data-key': 'listen-prop', 'data-testid': 'listen-idea', 'aria-pressed': listening ? 'true' : 'false', onclick: () => { r.listen = 'proposal'; S.audition = { kind: 'review', id: r.title, name: r.title, get: () => proposalTarget(r) }; changed(); } }, 'With these')), el('span', { class: 'sp' }), el('span', { class: 'dr-note' }, 'Hear it before you keep it')),
      el('div', { class: 'ff' }, el('button', { class: 'btn', 'data-key': 'review-cancel', 'data-testid': 'review-cancel', onclick: closeReview }, 'Cancel'), el('span', { class: 'sp' }), el('button', { class: 'btn keep', 'data-key': 'review-keep', 'data-testid': 'review-keep', disabled: !n, onclick: keepReview }, 'Keep ' + n + ' change' + (n === 1 ? '' : 's'))),
      el('div', { class: 'dr-note' }, 'Nothing changes until you keep. Cancel or × leaves your sound exactly as it is.')));
  mainEl.querySelectorAll('#gk-drawer').forEach((x) => x.remove());
  mainEl.append(drawerEl);
  drawerEl.querySelector('.dr-body').scrollTop = st;
}

// ---------------------------------------------------------------- modals
function openModal(m) { S.modal = m; changed(); }
function closeModal() { S.modal = null; changed(); }
let lastFocus = null, modalSig = '';
function renderModal() {
  const sig = JSON.stringify([S.modal && S.modal.kind, S.modal && S.modal.id, S.modal && S.modal.dl]);
  if (!S.modal) { if (modalEl.childElementCount) { modalEl.replaceChildren(); modalSig = ''; if (lastFocus && lastFocus.isConnected) lastFocus.focus({ preventScroll: true }); } return; }
  if (sig === modalSig && modalEl.childElementCount) return;
  if (!modalEl.childElementCount) lastFocus = document.activeElement;
  modalSig = sig;
  const m = S.modal;
  const box = el('div', { class: 'modal', role: 'dialog', 'aria-modal': 'true', 'aria-labelledby': 'gk-mt', 'data-testid': 'modal-' + m.kind });
  const layer = el('div', { class: 'modal-layer', onclick: (e) => { if (e.target === layer) closeModal(); } }, box);
  modalEl.replaceChildren(layer);
  const title = (t) => el('h2', { class: 'mt-title', id: 'gk-mt' }, t);
  const btn = (t, cls, fn, key) => el('button', { class: 'btn ' + (cls || ''), 'data-key': 'm-' + key, 'data-testid': 'm-' + key, onclick: fn }, t);
  const nameField = (val, key) => { const i = el('input', { class: 'field', 'data-key': key, 'data-testid': key, maxlength: 60, 'aria-label': 'Name' }); i.value = val || ''; return i; };
  if (m.kind === 'keep' || m.kind === 'rename') {
    const e = m.kind === 'rename' ? S.notebook.find((x) => x.id === m.id) : null;
    const name = nameField(e ? e.name : m.name, 'keep-name'), note = el('input', { class: 'field', 'data-key': 'keep-note', 'data-testid': 'keep-note', maxlength: 80, placeholder: 'A note for later (optional): “use on the 2nd hook”', 'aria-label': 'Note' });
    note.value = e ? e.note : '';
    const err = el('div', { class: 'err-txt', 'data-testid': 'keep-error', 'aria-live': 'polite' });
    const go2 = () => {
      const n = name.value.trim();
      if (!n) { err.textContent = 'Give it a name, even a silly one.'; name.focus(); return; }
      if (n.length > 40) { err.textContent = 'Use 40 characters or fewer.'; return; }
      if (e) { e.name = n; e.note = note.value.trim().slice(0, 80); persistNow(); closeModal(); toast('Renamed.'); changed(true); return; }
      closeModal(); keepInNotebook(n, note.value, m.from); if (m.then) m.then();
    };
    for (const i of [name, note]) i.addEventListener('keydown', (ev) => { if (ev.key === 'Enter') go2(); else if (ev.key === 'Escape') closeModal(); ev.stopPropagation(); });
    put(box, title(e ? 'Rename' : 'Keep this sound'), el('p', null, e ? 'Change its name or note.' : 'It goes into this session’s notebook, with the project. Your factory sounds and presets are not touched.'), name, note, err,
      el('div', { class: 'mf' }, btn('Cancel', '', closeModal, 'cancel'), btn(e ? 'Rename' : 'Keep', 'keep', go2, 'keep')));
  } else if (m.kind === 'remove') {
    const e = S.notebook.find((x) => x.id === m.id);
    put(box, title('Remove “' + (e ? e.name : '') + '”?'), el('p', null, 'It leaves this session’s notebook. Your current sound doesn’t change. This can’t be undone.'),
      el('div', { class: 'mf' }, btn('Cancel', '', closeModal, 'cancel'), btn('Remove', 'danger', () => { S.notebook = S.notebook.filter((x) => x.id !== m.id); if (S.activeNote === m.id) S.activeNote = null; if (hearing('note', m.id)) S.audition = null; persistNow(); closeModal(); toast('Removed from the notebook.'); changed(true); }, 'remove')));
  } else if (m.kind === 'unsaved') {
    put(box, title('Your current sound isn’t kept anywhere'), el('p', null, 'You’ve changed it since you started from “' + S.preset.name + '”, and it isn’t in the notebook, an A/B slot or your saved sounds. Undo can still bring it back later.'),
      el('div', { class: 'mf' }, btn('Cancel', '', closeModal, 'cancel'), btn('Use ' + m.what + ' anyway', '', () => { closeModal(); m.fn(); }, 'discard'), btn('Keep it first', 'keep', () => openModal({ kind: 'keep', name: '', from: S.preset.name, then: m.fn }), 'keepfirst')));
  } else if (m.kind === 'save') {
    const input = nameField(m.name, 'save-name'), err = el('div', { class: 'err-txt', 'data-testid': 'save-error', 'aria-live': 'polite' });
    const check = () => { const p = nameProblem(input.value); err.textContent = p || (S.lib.some((x) => x.name.toLowerCase() === input.value.trim().toLowerCase()) ? 'A saved sound with this name exists. Saving will ask before replacing it.' : ''); return p; };
    const go2 = () => { if (check()) { input.focus(); return; } if (saveAs(input.value, false) === 'exists') openModal({ kind: 'overwrite', name: input.value.trim() }); else closeModal(); };
    input.addEventListener('input', check); input.addEventListener('keydown', (e) => { if (e.key === 'Enter') go2(); else if (e.key === 'Escape') closeModal(); e.stopPropagation(); });
    put(box, title('Save this sound'), el('p', null, 'A preset you can use in any project. (The notebook keeps ideas for this session only.)'), input, err, el('div', { class: 'mf' }, btn('Cancel', '', closeModal, 'cancel'), btn('Save', 'primary', go2, 'save')));
  } else if (m.kind === 'overwrite') {
    put(box, title('Replace “' + m.name + '”?'), el('p', null, 'A saved sound with this name exists. Replacing it changes it in every project. This can’t be undone.'),
      el('div', { class: 'mf' }, btn('Cancel', '', () => openModal({ kind: 'save', name: m.name }), 'cancel'), btn('Replace', 'danger', () => { saveAs(m.name, true); closeModal(); }, 'replace')));
  } else if (m.kind === 'deluser') {
    put(box, title('Delete “' + m.name + '”?'), el('p', null, 'This saved sound disappears from every project. Your current sound doesn’t change. This can’t be undone.'),
      el('div', { class: 'mf' }, btn('Cancel', '', closeModal, 'cancel'), btn('Delete', 'danger', () => {
        S.lib = S.lib.filter((p) => p.name !== m.name);
        if (S.preset.type === 'user' && S.preset.name === m.name) S.preset = { ...S.preset, type: 'loose' };   // the sound stays; it just isn't saved anywhere now
        if (hearing('scene', m.name)) S.audition = null;
        persistNow(); closeModal(); toast('Deleted “' + m.name + '”.'); changed(true);
      }, 'delete')));
  } else if (m.kind === 'abswitch') {
    put(box, title('Switch to ' + m.to + '?'), el('p', null, 'You’ve changed the sound since you stored it in ' + m.from + '. Those changes are not in ' + m.from + ' yet.'),
      el('div', { class: 'mf' }, btn('Cancel', '', closeModal, 'cancel'), btn('Switch without storing', '', () => { closeModal(); abRecall(m.to); }, 'switch'), btn('Store in ' + m.from + ', then switch', 'primary', () => { closeModal(); abStore(m.from, true); abRecall(m.to); }, 'storeswitch')));
  } else if (m.kind === 'abover') {
    put(box, title('Replace what’s in ' + m.slot + '?'), el('p', null, m.slot + ' holds a different sound. Storing now replaces it. This can’t be undone.'),
      el('div', { class: 'mf' }, btn('Cancel', '', closeModal, 'cancel'), btn('Replace ' + m.slot, 'danger', () => { closeModal(); abStore(m.slot, true); }, 'replace')));
  } else if (m.kind === 'master-on') {
    put(box, title('Turn on Mastering mode?'), el('p', null, 'This instance stops shaping a vocal and masters a full stereo mix instead. Its latency changes. Only do this on a GlitchKid that sits on FL Studio’s master track.'),
      el('div', { class: 'mf' }, btn('Cancel', '', closeModal, 'cancel'), btn('Turn on Mastering mode', 'primary', () => { S.master.on = true; S.audition = null; S.review = null; closeModal(); toast('Mastering mode is on for this instance.'); changed(true); }, 'masteron')));
  } else if (m.kind === 'speech') {
    const dl = m.dl || 0;
    put(box, title('Set up speech'), el('p', null, 'KID’s speech uses whisper.cpp’s ggml-base.en model (about 142 MiB, per whisper.cpp’s model list). It downloads once, is checked against a SHA-1 hash before it’s loaded, then works offline. Your voice is never uploaded.'),
      dl ? el('div', { class: 'progress' }, el('i', { style: { width: dl + '%' } })) : null,
      el('div', { class: 'mf' }, btn('Not now', '', closeModal, 'cancel'), btn(dl >= 100 ? 'Done' : dl ? 'Downloading…' : 'Download', 'primary', async () => {
        if (dl >= 100) { closeModal(); return; } if (dl) return;
        for (let i = 1; i <= 10; i++) { await later(140); if (!S.modal || S.modal.kind !== 'speech') return; S.modal.dl = i * 10; changed(); }
        S.sim.speech = 'ready'; renderSim(); S.kid.reply = null; if (S.kid.phase === 'unavailable') S.kid.phase = 'ready'; toast('Speech is ready.'); changed();
      }, 'download')));
  } else if (m.kind === 'help') {
    put(box, title('Help'), el('div', { class: 'help-grid' }, el('div', null, el('kbd', null, 'K'), ' hold: talk to KID'), el('div', null, el('kbd', null, 'D'), ': DRY / PROCESSED'), el('div', null, el('kbd', null, 'Ctrl/Cmd+Z'), ': undo'), el('div', null, el('kbd', null, 'Ctrl/Cmd+Shift+Z'), ': redo'), el('div', null, el('kbd', null, 'Esc'), ': close, stop hearing'), el('div', null, 'Double-click a knob: default')),
      el('p', null, el('b', null, 'Where’s play? '), 'FL Studio plays your track; GlitchKid processes it on this Mixer insert. It can look at a loaded file, but can’t play one.'),
      el('p', null, el('b', null, 'KID '), 'is a guided creative assistant, not generative AI: GlitchKid’s rule engine turns your words into ideas made of its own controls. Speech is transcribed on this computer, only while you hold the mic. KID never applies anything; you keep or put away.'),
      el('div', { class: 'mf' }, btn(S.kid.quiet ? 'KID notes: quiet' : 'KID notes: on', '', () => { S.kid.quiet = !S.kid.quiet; modalSig = ''; changed(); }, 'quiet'), btn('Show the intro again', '', () => { S.modal = null; S.onb = { open: true, step: 0, quiet: S.kid.quiet }; changed(); }, 'intro'), btn('Close', 'primary', closeModal, 'close')));
  }
  box.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') { e.stopPropagation(); closeModal(); return; }
    if (e.key !== 'Tab') return;
    const f = [...box.querySelectorAll('button:not([disabled]), input')]; if (!f.length) return;
    if (e.shiftKey && document.activeElement === f[0]) { e.preventDefault(); f[f.length - 1].focus(); } else if (!e.shiftKey && document.activeElement === f[f.length - 1]) { e.preventDefault(); f[0].focus(); }
  });
  requestAnimationFrame(() => { if (!box.contains(document.activeElement)) (box.querySelector('input') || box.querySelector('.btn.primary, .btn.keep') || box.querySelector('button')).focus(); });
}
function saveClicked() {
  if (S.preset.type === 'user' && !isModified()) { toast('“' + S.preset.name + '” is saved. Nothing has changed since.'); return; }
  openModal({ kind: 'save', name: S.preset.type === 'user' ? S.preset.name : '' });
}

// ---------------------------------------------------------------- first launch (3 short steps, skippable)
function renderOnboarding() {
  let layer = $('#gk-onb');
  if (!S.onb.open) { if (layer) layer.remove(); return; }
  const sig = JSON.stringify([S.onb.step, S.sim.signal, S.bypass, S.onb.quiet]);
  if (layer && layer.dataset.sig === sig) return;
  if (layer) layer.remove();
  const st = S.onb.step;
  const box = el('div', { class: 'modal onb', role: 'dialog', 'aria-modal': 'true', 'aria-labelledby': 'gk-onb-t', 'data-testid': 'onb-' + st });
  const dots = el('div', { class: 'dots' }, [0, 1, 2].map((i) => el('i', { class: i <= st ? 'on' : '' })));
  const nav = (back, label, fn) => el('div', { class: 'foot' }, el('button', { class: 'btn ghost', 'data-key': 'onb-skip', 'data-testid': 'onb-skip', onclick: () => finishOnb() }, 'Skip'), el('span', { class: 'sp' }),
    back ? el('button', { class: 'btn', 'data-key': 'onb-back', onclick: () => { S.onb.step--; changed(); } }, 'Back') : null, el('button', { class: 'btn primary', 'data-key': 'onb-next', 'data-testid': 'onb-next', onclick: fn || (() => { S.onb.step++; changed(); }) }, label));
  if (st === 0) {
    const name = el('input', { class: 'tape-edit', style: { width: '100%' }, 'data-key': 'onb-name', 'data-testid': 'onb-name', maxlength: 40, 'aria-label': 'Session name' });
    name.value = S.session.name;
    name.addEventListener('input', () => { if (name.value.trim()) S.session.name = name.value.trim().slice(0, 40); });
    name.addEventListener('keydown', (e) => { if (e.key === 'Enter') { S.onb.step++; changed(); } e.stopPropagation(); });
    put(box, dots, el('h2', { class: 'mt-title', id: 'gk-onb-t' }, 'This is your session'), el('p', { class: 'lead' }, 'GlitchKid is a vocal instrument on your track: play, try ideas, hear them, keep what’s yours. Name the session (any language):'), name, nav(false, 'Next'));
  } else if (st === 1) {
    const ok = S.sim.signal !== 'none';
    put(box, dots, el('h2', { class: 'mt-title', id: 'gk-onb-t' }, 'Your vocal comes from FL Studio'), el('p', { class: 'lead' }, 'Press play there. GlitchKid listens on this Mixer insert. DRY is always your original, at matched loudness.'),
      el('div', { class: 'meter-demo', 'data-testid': 'onb-signal' }, el('b', null, 'IN'), el('div', { class: 'mbar' }, el('i', { id: 'onb-bar' })), el('span', { style: { color: ok ? 'var(--good)' : 'var(--text-2)', fontWeight: '700' } }, ok ? '✓ Vocal coming in' : 'Waiting for sound…')),
      ok ? null : el('button', { class: 'btn small', 'data-key': 'onb-sim', 'data-testid': 'onb-sim', onclick: () => { S.sim.signal = 'live'; renderSim(); changed(); } }, 'Prototype: simulate playback'),
      el('div', { class: 'compare', style: { alignSelf: 'flex-start' } }, el('button', { class: 'dry', 'data-key': 'onb-dry', 'data-testid': 'onb-dry', 'aria-pressed': S.bypass ? 'true' : 'false', onclick: () => setBypass(true) }, 'DRY'), el('button', { 'data-key': 'onb-proc', 'aria-pressed': S.bypass ? 'false' : 'true', onclick: () => setBypass(false) }, 'PROCESSED')), nav(true, 'Next'));
  } else {
    put(box, dots, el('h2', { class: 'mt-title', id: 'gk-onb-t' }, 'KID leaves notes'), el('p', { class: 'lead' }, 'KID is a guided assistant that leaves ideas on your desk. It never applies anything: you hear, then keep or put away. It listens only while you hold its button, on a separate mic, and transcribes on this computer.'),
      el('div', { class: 'choice', role: 'radiogroup', 'aria-label': 'KID notes' },
        el('button', { role: 'radio', 'data-key': 'onb-notes-on', 'data-testid': 'onb-notes-on', 'aria-checked': S.onb.quiet ? 'false' : 'true', onclick: () => { S.onb.quiet = false; changed(); } }, el('b', null, 'Notes on'), el('span', null, 'KID drops the odd note about your settings.')),
        el('button', { role: 'radio', 'data-key': 'onb-notes-quiet', 'data-testid': 'onb-notes-quiet', 'aria-checked': S.onb.quiet ? 'true' : 'false', onclick: () => { S.onb.quiet = true; changed(); } }, el('b', null, 'Only when I ask'), el('span', null, 'KID stays quiet until you talk to it.'))),
      nav(true, 'Start the session', () => finishOnb()));
  }
  box.addEventListener('keydown', (e) => { if (e.key === 'Escape') { e.stopPropagation(); finishOnb(); } });
  frame.append(el('div', { class: 'modal-layer', id: 'gk-onb', 'data-sig': sig }, box));
  requestAnimationFrame(() => { const n = box.querySelector(st === 0 ? 'input' : '[data-key="onb-next"]'); if (n && !box.contains(document.activeElement)) n.focus({ preventScroll: true }); });
}
function finishOnb() { S.onb.open = false; S.onbDone = true; S.kid.quiet = S.onb.quiet; persistNow(); changed(true); }

// ---------------------------------------------------------------- the take (simulated) and the meters
let level = 0, hist = new Float32Array(600), hpos = 0, peakIn = -90, peakOut = -90, peakAge = 0, clipAt = -1e9;
const t0 = performance.now();
function simLevel(t) {
  if (S.sim.signal === 'none' || S.sim.engine === 'error' || S.sim.take === 'file' || S.sim.transport === 'stopped') return 0;
  const ph = t % 4.6, gate = ph < 3.5 ? clamp(Math.min(ph / 0.15, (3.5 - ph) / 0.2), 0, 1) : 0;
  return gate * (0.55 + 0.45 * Math.abs(Math.sin(t * 6.1)) * (0.65 + 0.35 * Math.sin(t * 1.7)));
}
function fileShape(i, n) { const x = i / n * 18; return Math.max(0, Math.sin(x * 2.1) * 0.5 + 0.5) * (0.4 + 0.6 * Math.abs(Math.sin(x * 9.3))) * ((x % 4.6) < 3.6 ? 1 : 0.05); }
function drawWave(cv, s, t) {
  const W = cv.clientWidth, H = cv.clientHeight, dpr = Math.min(2, window.devicePixelRatio || 1);
  if (!W || !H) return;
  if (cv.width !== Math.round(W * dpr) || cv.height !== Math.round(H * dpr)) { cv.width = Math.round(W * dpr); cv.height = Math.round(H * dpr); }
  const ctx = cv.getContext('2d'); ctx.setTransform(dpr, 0, 0, dpr, 0, 0); ctx.clearRect(0, 0, W, H);
  const mid = H / 2, n = hist.length, file = S.sim.take === 'file';
  const val = (i) => (file ? fileShape(i, n) : hist[(hpos + i) % n]);
  ctx.fillStyle = 'rgba(255,255,255,.04)'; ctx.fillRect(0, mid, W, 1);
  if (S.master.on || S.sim.engine === 'error') return;
  const dry = S.bypass;
  const bright = clamp((s.eq_hi_gain + 0.5 * s.eq_b2_gain) / 8, -1, 1), tb = (bright + 1) / 2;
  const col = dry ? [94, 227, 255] : [Math.round(143 + 112 * tb), Math.round(91 - 12 * tb), Math.round(255 - 71 * tb)];
  const spread = dry ? 0.9 : 0.9 + (on(s, 'dbl_on') ? s.dbl_width / 250 : 0) + (on(s, 'dim_on') ? s.dim_width / 300 : 0);
  const amp = (H / 2 - 6) * (dry ? 0.85 : 0.85 + 0.1 * clamp((s.comp_ratio - 1) / 5, 0, 1));
  const path = (dx, scale) => { const p = new Path2D(); for (let i = 0; i < n; i += 2) { const x = i / (n - 1) * W + dx; const y = mid - val(i) * amp * scale; if (i) p.lineTo(x, y); else p.moveTo(x, y); } for (let i = n - 1; i >= 0; i -= 2) { const x = i / (n - 1) * W + dx; p.lineTo(x, mid + val(i) * amp * scale * spread); } p.closePath(); return p; };
  if (!dry && on(s, 'echo_on')) for (let k = 1; k <= 3; k++) { ctx.fillStyle = 'rgba(' + col.join(',') + ',' + (0.08 * (s.echo_mix / 40) / k).toFixed(3) + ')'; ctx.fill(path(k * W * (0.03 + 0.012 * (s.echo_div || 0)), 0.9 - k * 0.15)); }
  if (!dry && on(s, 'dbl_on')) { ctx.strokeStyle = 'rgba(255,255,255,' + (0.12 + s.dbl_amount / 400).toFixed(3) + ')'; ctx.lineWidth = 1; ctx.stroke(path(4 + s.dbl_timing / 6, 0.92)); }
  const main = path(0, 1);
  const g = ctx.createLinearGradient(0, 0, W, 0); g.addColorStop(0, 'rgba(' + col.join(',') + ',.10)'); g.addColorStop(1, 'rgba(' + col.join(',') + ',.55)');
  ctx.fillStyle = g; ctx.fill(main);
  const charAmt = dry ? 0 : clamp((on(s, 'mut_on') ? 0.4 + s.mut_amount / 200 : 0) + (Math.abs(s.formant_shift) >= 0.05 ? 0.3 : 0) + (on(s, 'harm_on') ? 0.25 : 0), 0, 1);
  if (charAmt > 0) { ctx.strokeStyle = 'rgba(94,227,255,' + (0.5 * charAmt).toFixed(3) + ')'; ctx.lineWidth = 1.2; ctx.stroke(path(-3 * charAmt - 1, 1)); ctx.strokeStyle = 'rgba(255,79,184,' + (0.6 * charAmt).toFixed(3) + ')'; ctx.stroke(path(3 * charAmt + 1, 1)); }
  if (!dry && on(s, 'stutter_on')) { ctx.setLineDash([10, 6]); ctx.lineDashOffset = S.sim.motion ? -t * 40 : 0; }
  ctx.strokeStyle = 'rgba(' + col.join(',') + ',.95)'; ctx.lineWidth = dry ? 1.6 : 1.8 + clamp(s.sat_drive / 6, 0, 2);
  ctx.shadowColor = 'rgba(' + col.join(',') + ',.6)'; ctx.shadowBlur = dry ? 0 : 8;
  ctx.stroke(main); ctx.setLineDash([]); ctx.shadowBlur = 0;
  if (S.audition) { ctx.strokeStyle = 'rgba(94,227,255,.75)'; ctx.lineWidth = 2; ctx.strokeRect(1, 1, W - 2, H - 2); }
}
function loop(now) {
  const t = (now - t0) / 1000;
  level += (simLevel(t) - level) * 0.25;
  if (S.sim.transport === 'playing' && S.sim.take === 'live') { hist[hpos] = level; hpos = (hpos + 1) % hist.length; }
  const clipping = S.sim.signal === 'clipping';
  const inDb = level > 0.01 ? Math.min(0, -60 + (clipping ? 72 : 50) * level) : -90;
  const s = heard();
  let outDb = S.bypass ? inDb : inDb + s.gain + 0.5 * s.comp_makeup - (s.comp_ratio > 1.001 ? 2 : 0);
  if (!S.bypass && on(s, 'lim_on')) outDb = Math.min(outDb, s.lim_ceiling);
  outDb = Math.min(outDb, 0);
  if (level > 0.01 && inDb >= -0.1) clipAt = now;
  if (++peakAge > 90) { peakAge = 0; peakIn = inDb; peakOut = outDb; }
  peakIn = Math.max(peakIn, inDb); peakOut = Math.max(peakOut, outDb);
  const mm = frame && frame.querySelector('.minimeter');
  if (mm) {
    const w = (db) => clamp((db + 60) / 60, 0, 1) * 100 + '%';
    mm.querySelector('.mm.in i').style.width = w(inDb); mm.querySelector('.mm.out i').style.width = w(outDb);
    const vs = mm.querySelectorAll('.mm-v'); vs[0].textContent = peakIn > -60 ? String(Math.round(peakIn)) : '—'; vs[1].textContent = peakOut > -60 ? String(Math.round(peakOut)) : '—';
    const c = mm.querySelector('.clip'), onc = now - clipAt < 1500; if (c.hidden === onc) c.hidden = !onc;
  }
  const cv = frame && frame.querySelector('#gk-wave');
  if (cv) drawWave(cv, s, S.sim.motion ? t : 0);
  const ob = document.getElementById('onb-bar'); if (ob) ob.style.width = (level * 100).toFixed(1) + '%';
  if (S.kid.phase === 'recording') frame.querySelectorAll('.cmdlevel i').forEach((b, i) => { b.style.height = (3 + 10 * Math.abs(Math.sin(t * 9 + i * 1.3))) + 'px'; });
  requestAnimationFrame(loop);
}

// ---------------------------------------------------------------- prototype controls (outside the plug-in) and window size
const SIZES = { compact: [900, 580], standard: [1120, 680], large: [1360, 800] };
function applySize() {
  const sz = S.sim.size === 'fit' ? 'standard' : S.sim.size, [w, h] = SIZES[sz];
  const fr = $('#frame'); fr.style.width = w + 'px'; fr.style.height = h + 'px';
  fr.classList.remove('sz-compact', 'sz-standard', 'sz-large'); fr.classList.add('sz-' + sz);
  const k = S.sim.size === 'fit' ? Math.min(1, Math.max(0.3, (window.innerWidth - 36) / w)) : 1;
  fr.style.transform = 'scale(' + k + ')';
  $('#frameWrap').style.width = w * k + 'px'; $('#frameWrap').style.height = h * k + 'px';
}
function renderSim() {
  const grp = (label, key, opts, after) => el('div', { class: 'grp' }, el('span', null, label), ...opts.map(([v, t]) => el('button', { 'data-sim': key + '-' + v, 'aria-pressed': S.sim[key] === v ? 'true' : 'false', onclick: () => { S.sim[key] = v; if (after) after(v); renderSim(); changed(true); } }, t)));
  const say = el('select', { 'data-testid': 'kid-hears', 'aria-label': 'Simulated speech: what KID hears when you hold the mic' }, el('optgroup', { label: 'Requests' }, PHRASES.first.map((p, i) => el('option', { value: i }, p))), el('optgroup', { label: 'Follow-ups' }, PHRASES.follow.map((p, i) => el('option', { value: PHRASES.first.length + i }, p))));
  say.value = String(S.kid.phrase || 0); say.addEventListener('change', () => { S.kid.phrase = +say.value; });
  $('#sim').replaceChildren(
    grp('Vocal', 'signal', [['none', 'None'], ['live', 'Live'], ['clipping', 'Clipping']]),
    grp('Take', 'take', [['live', 'Live input'], ['file', 'Loaded file']]),
    grp('FL Studio', 'transport', [['playing', 'Playing'], ['stopped', 'Stopped']]),
    grp('Instrumental input', 'sidechain', [['none', 'Nothing routed'], ['beat', 'Beat routed']]),
    grp('Speech', 'speech', [['ready', 'Ready'], ['missing', 'Not set up'], ['denied', 'Mic blocked']], () => { S.kid.reply = null; if (S.kid.phase === 'unavailable') S.kid.phase = 'ready'; }),
    grp('Engine', 'engine', [['ok', 'Normal'], ['error', 'Not processing']], () => { S.kid.reply = null; if (S.kid.phase === 'unavailable') S.kid.phase = 'ready'; }),
    grp('Window', 'size', [['compact', 'Compact 900×580'], ['standard', 'Standard 1120×680'], ['large', 'Large 1360×800'], ['fit', 'Fit screen']], applySize),
    grp('Motion', 'motion', [[true, 'On'], [false, 'Off']], () => document.body.classList.toggle('reduce-motion', !S.sim.motion)),
    el('div', { class: 'grp' }, el('span', null, 'KID hears'), say),
    el('div', { class: 'grp' }, el('button', { 'data-sim': 'intro', onclick: () => { S.onb = { open: true, step: 0, quiet: S.kid.quiet }; changed(); } }, 'Restart intro'),
      el('button', { 'data-sim': 'reopen', onclick: () => { persistNow(); location.reload(); } }, 'Close & reopen project'),
      el('button', { 'data-sim': 'wipe', onclick: () => { store.del(LS); store.del(LS_LIB); location.reload(); } }, 'Reset prototype data')));
}
window.addEventListener('resize', () => { if (S.sim.size === 'fit') applySize(); });

// ---------------------------------------------------------------- keyboard
document.addEventListener('keydown', (e) => {
  const tag = e.target && e.target.tagName;
  if (tag === 'INPUT' || tag === 'SELECT' || tag === 'TEXTAREA') return;
  const mod = e.metaKey || e.ctrlKey;
  if (mod && e.key.toLowerCase() === 'z') { e.preventDefault(); if (e.shiftKey) redo(); else undo(); return; }
  if (mod && e.key.toLowerCase() === 'y') { e.preventDefault(); redo(); return; }
  if (mod && e.key.toLowerCase() === 's') { e.preventDefault(); saveClicked(); return; }
  if (S.modal || S.onb.open || mod || e.altKey) return;
  if (e.key === 'Escape') { if (S.review) closeReview(); else if (S.audition) stopAudition(); return; }
  if (e.key.toLowerCase() === 'd' && !e.repeat) { setBypass(!S.bypass); return; }
  if (e.key.toLowerCase() === 'k' && !e.repeat) micDown();
});
document.addEventListener('keyup', (e) => { if (e.key.toLowerCase() === 'k') micUp(); });

// ---------------------------------------------------------------- start
renderShell(); renderSim(); applySize();
document.body.classList.toggle('reduce-motion', !S.sim.motion);
renderMain(); update();
requestAnimationFrame(loop);
window.GK = { setSize: (sz) => { S.sim.size = sz; applySize(); renderSim(); changed(true); }, S, PARAMS, PM, PROFILES, WORLDS, IDEAS, USER_OWNED, KID_NEVER, CHAIN, pace, phaseLog, factoryDefault, profileSound, worldSound, soundEq, heard, cardTarget, cardRows, proposalTarget, isModified, needsKeepPrompt, modOn, fmt, persistNow, rollAccident };
})();
