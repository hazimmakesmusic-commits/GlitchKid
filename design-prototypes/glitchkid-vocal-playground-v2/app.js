/* GlitchKid — The Vocal Playground, prototype v2 (design only; no audio, nothing here runs in the plug-in).
   Copied from the GlitchKid source and checked on 2026-11-04:
     - every parameter ID, range, default, skew centre and choice list (Source/PluginProcessor.cpp, createLayout): 68 sound parameters + Bypass;
     - the fixed processing order (processChunk): 1 Tune, 2 Formant, 3 Harmony, 4 Mutation, 5 Doubler, 6 EQ, 7 CLEAN corrective, 8 Compressor,
       9 CLEAN de-esser, 10 Saturation, 11 AIR LAB, 12 Stutter, 13 Echo, 14 Dimension, 15 Gain, 16 Mix, 17 Output Limiter;
     - the 8 factory profiles and their exact overrides (Source/Presets/Profiles.inc) and the 22 parameters a profile load keeps;
     - the TONE / PUNCH / SPACE macro formulas and the tuning styles (Source/Macros/MacroEngine.cpp);
     - KID's allowlist (KidDirector::isAllowed: never Gain, Mix, the Output Limiter or Bypass).
   Simulated and labelled in the UI: every level, meter and measurement, the speech recognition, and KID's interpretation (simple rules modelled on KidDirector).
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

// ---------------------------------------------------------------- state and persistence
const LS_SESSION = 'gk-proto-v2-session', LS_LIB = 'gk-proto-v2-library';
const store = {
  get(k) { try { return JSON.parse(localStorage.getItem(k)); } catch { return null; } },
  set(k, v) { try { localStorage.setItem(k, JSON.stringify(v)); } catch { /* private window: the prototype still works, it just forgets */ } },
  del(k) { try { localStorage.removeItem(k); } catch { /* ignore */ } },
};
const prefersReduced = !!(window.matchMedia && matchMedia('(prefers-reduced-motion: reduce)').matches);
const S = {
  sound: factoryDefault(), bypass: false,
  preset: { name: 'Factory Default', type: 'default', snap: factoryDefault() },
  ab: { A: null, B: null, active: null },
  undo: [],
  view: { ws: 'booth', tab: 'clean', filter: 'all', adv: {} },
  drawer: null,                                   // 'kid' | 'build'
  kid: { phase: 'ready', log: [], prop: null, mem: null, phrase: 0, holdStart: 0, busy: false, lastApplied: 0 },
  build: null,                                    // { step, listen, listened, profile, intensity, prop }
  preview: null,                                  // { name, sound }
  sim: { signal: 'vocal', speech: 'ready', net: 'online', engine: 'ok', size: 'fit', motion: !prefersReduced },
  onb: { open: false, step: 0, path: 'build' }, onbDone: false,
  flags: { compared: false },
  lib: [],
  modal: null, menu: null,
};
const phaseLog = [];                              // QA reads the order of KID's states
(function restore() {
  const lib = store.get(LS_LIB);
  if (Array.isArray(lib)) S.lib = lib.map((p) => ({ name: String((p && p.name) || '').slice(0, 40), sound: sanitizeSound(p && p.sound) })).filter((p) => p.name && p.sound);
  const ss = store.get(LS_SESSION);
  if (ss && typeof ss === 'object') {
    const snd = sanitizeSound(ss.sound);
    if (snd) S.sound = snd;
    if (ss.preset && typeof ss.preset.name === 'string') S.preset = { name: ss.preset.name.slice(0, 48), type: ['default', 'factory', 'user', 'built'].includes(ss.preset.type) ? ss.preset.type : 'built', snap: sanitizeSound(ss.preset.snap) || copy(S.sound) };
    for (const k of ['A', 'B']) { const sl = ss.ab && ss.ab[k]; const s2 = sl && sanitizeSound(sl.sound); S.ab[k] = s2 ? { sound: s2, name: String(sl.name || '').slice(0, 48) } : null; }
    S.ab.active = ss.ab && (ss.ab.active === 'A' || ss.ab.active === 'B') && S.ab[ss.ab.active] ? ss.ab.active : null;
    S.bypass = !!ss.bypass; S.onbDone = !!ss.onbDone; S.flags.compared = !!(ss.flags && ss.flags.compared);
    if (ss.sim && typeof ss.sim.motion === 'boolean') S.sim.motion = ss.sim.motion;
  }
  S.onb.open = !S.onbDone;
})();
let persistTimer = 0;
function persistNow() {
  store.set(LS_SESSION, { sound: S.sound, bypass: S.bypass, preset: S.preset, ab: S.ab, onbDone: S.onbDone, flags: S.flags, sim: { motion: S.sim.motion } });
  store.set(LS_LIB, S.lib);
}
function persist() { clearTimeout(persistTimer); persistTimer = setTimeout(persistNow, 150); }

// ---------------------------------------------------------------- core actions
const snap = () => ({ sound: copy(S.sound), preset: copy(S.preset), abActive: S.ab.active });
function pushUndo(label, before) { S.undo.push({ label, ...before }); if (S.undo.length > 40) S.undo.shift(); }
const isModified = () => !soundEq(S.sound, S.preset.snap);
const abModified = () => !!(S.ab.active && S.ab[S.ab.active] && !soundEq(S.sound, S.ab[S.ab.active].sound));
function heard() {                                 // what the artist would hear right now (DRY is shown separately)
  if (S.preview) return S.preview.sound;
  const pr = activeProposal();
  if (pr && pr.listen === 'proposal') return proposalTarget(pr);
  return S.sound;
}
function setParam(id, v, src = 'manual') {
  v = clampP(id, v);
  if (S.sound[id] === v) return;
  S.sound[id] = v;
  if (src === 'manual') manualEdit([id]);
  changed();
}
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
function undo() {
  if (gest) gestureEnd(0);
  const e = S.undo.pop();
  if (!e) { toast('Nothing to undo.'); return; }
  S.sound = e.sound; S.preset = e.preset; S.ab.active = e.abActive; S.preview = null;
  if (S.kid.phase === 'applied') S.kid.phase = 'ready';
  toast('Undone: ' + e.label);
  changed(true);
}
function loadSound(next, label, preset) {
  const before = snap();
  S.sound = copy(next); S.preset = preset; S.preview = null;
  pushUndo(label, before);
  changed(true);
}
function loadProfile(pi) { const s = profileSound(pi, S.sound); loadSound(s, 'Load ' + PROFILES[pi].name, { name: PROFILES[pi].name, type: 'factory', snap: copy(s) }); toast('Loaded ' + PROFILES[pi].name + '. Your tuning, gain, mix, CLEAN, AIR LAB and limiter were kept.', 'Undo', undo); }
function loadDefault() { const s = factoryDefault(); loadSound(s, 'Load Factory Default', { name: 'Factory Default', type: 'default', snap: copy(s) }); toast('Every control is back to its default.', 'Undo', undo); }
function loadUser(i) { const p = S.lib[i]; if (!p) return; loadSound(p.sound, 'Load ' + p.name, { name: p.name, type: 'user', snap: copy(p.sound) }); toast('Loaded “' + p.name + '”.', 'Undo', undo); }
function stepScene(dir) {
  const i = PROFILES.findIndex((p) => p.name === S.preset.name);
  loadProfile(i < 0 ? (dir > 0 ? 0 : PROFILES.length - 1) : (i + dir + PROFILES.length) % PROFILES.length);
}
function setBypass(on) { if (S.bypass === on) return; S.bypass = on; if (on) S.flags.compared = true; changed(); }

// ---------------------------------------------------------------- A / B (approved rules: empty captures, occupied recalls, nothing is overwritten silently)
function abClick(k) {
  if (!S.ab[k]) { S.ab[k] = { sound: copy(S.sound), name: S.preset.name }; S.ab.active = k; toast('Stored the current sound in ' + k + '.'); changed(); return; }
  if (S.ab.active === k) { toast(abModified() ? k + ' is playing, with changes that are not stored in ' + k + ' yet. ⤓ stores them.' : k + ' is already playing.'); return; }
  if (abModified()) { openModal({ kind: 'abswitch', from: S.ab.active, to: k }); return; }
  abRecall(k);
}
function abRecall(k) { const before = snap(); S.sound = copy(S.ab[k].sound); S.ab.active = k; S.preview = null; pushUndo('Switch to ' + k, before); toast('Now playing ' + k + '.'); changed(true); }
function abStore(k, confirmed) {
  if (S.ab[k] && !confirmed && !soundEq(S.ab[k].sound, S.sound)) { openModal({ kind: 'abover', slot: k }); return; }
  S.ab[k] = { sound: copy(S.sound), name: S.preset.name }; S.ab.active = k; toast('Stored the current sound in ' + k + '.'); changed();
}

// ---------------------------------------------------------------- presets: save, save as, overwrite (asks), delete (asks)
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
  toast('Saved “' + n + '”. It’s in SCENES › Yours, in every project.');
  changed(true);
  return 'ok';
}
function saveClicked() {
  if (S.preset.type === 'user' && isModified()) { openMenu('save'); return; }
  if (S.preset.type === 'user') { toast('“' + S.preset.name + '” is saved. Nothing has changed since.'); return; }
  openModal({ kind: 'save', name: '' });
}

// ---------------------------------------------------------------- proposals (KID and Build My Mix share one review)
function activeProposal() { if (S.drawer === 'build' && S.build && S.build.prop) return S.build.prop; return S.kid.prop; }
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
function manualEdit(ids) {
  const pr = activeProposal();
  if (!pr) return;
  const hit = [];
  for (const id of ids) { const r = pr.rows.find((x) => x.id === id); if (r && !r.manual) { r.manual = true; r.sel = false; hit.push(PM[id].name); } }
  if (hit.length && pr.src === 'kid') kidSay(['You moved ', { b: hit.join(', ') }, ' by hand, so I left ' + (hit.length > 1 ? 'them' : 'it') + ' alone. Your value is kept.']);
}
function applyProposal() {
  const pr = activeProposal();
  if (!pr) return;
  const n = selectedCount(pr);
  if (!n) { toast('Nothing is ticked. Tick at least one change to apply.'); return; }
  const before = snap();
  S.sound = proposalTarget(pr);
  pr.listen = 'current';
  const label = n + ' change' + (n > 1 ? 's' : '');
  if (pr.src === 'build') {
    S.preset = { name: 'Built: ' + PROFILES[S.build.profile].name, type: 'built', snap: copy(S.sound) };
    pushUndo('Build My Mix (' + label + ')', before);
    S.build = null; S.drawer = null;
    toast('Applied ' + label + ' from Build My Mix. Not saved yet.', 'Undo', undo);
  } else {
    pushUndo('KID (' + label + ')', before);
    S.kid.prop = null;
    S.kid.mem = { dir: zeroDir(), locks: S.kid.mem ? S.kid.mem.locks : [], role: S.kid.mem ? S.kid.mem.role : '', base: copy(S.sound) };
    S.kid.lastApplied = n;
    kidSay(['Applied ', { b: label }, '. Undo is in the top bar. Tell me what to change next.']);
    setPhase('applied');
  }
  changed(true);
}
function cancelProposal() {
  if (S.drawer === 'build') { S.build = null; S.drawer = null; toast('Build My Mix cancelled. Your sound is unchanged.'); changed(true); return; }
  if (S.kid.prop) { S.kid.prop = null; S.kid.mem = null; kidSay('Cancelled. Your sound is exactly as it was.'); setPhase('ready'); changed(); }
}

// ---------------------------------------------------------------- KID: words -> intent -> direction (with memory) -> validated proposal
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
function kidAvailable() {
  if (S.sim.engine === 'mastering') return { ok: false, why: 'Mastering mode is on in this instance, so the vocal chain is not in use. KID shapes vocals only. Switch Mastering mode off (MASTER page) to continue.' };
  if (S.sim.engine === 'error') return { ok: false, why: 'GlitchKid is not processing right now, so KID can’t propose changes. Audio passes through unprocessed.' };
  return { ok: true };
}
function speechAvailable() {
  if (S.sim.speech === 'missing') return { ok: false, kind: 'missing', why: 'Speech isn’t set up on this computer yet. You can type to KID now, or set up speech once (a one-time download; after that it works offline).' };
  if (S.sim.speech === 'denied') return { ok: false, kind: 'denied', why: 'GlitchKid isn’t allowed to use a microphone. macOS: System Settings › Privacy & Security › Microphone, allow your DAW. Windows: Settings › Privacy › Microphone. Typing still works.' };
  return { ok: true };
}
const PHASES = {
  ready: ['Ready', ''], recording: ['Listening… let go to send', 'rec'], transcribing: ['Transcribing on this computer', 'busy'], heard: ['Heard you', 'busy'],
  interpreting: ['Reading your words', 'busy'], preparing: ['Building a proposal', 'busy'], proposal: ['Proposal ready', 'ready'], applied: ['Applied', 'ready'],
  unavailable: ['Unavailable', 'err'], error: ['Didn’t catch that', 'err'],
};
function setPhase(p) { S.kid.phase = p; phaseLog.push(p); changed(); }
function kidSay(parts, cls) { S.kid.log.push({ who: 'kid', parts: Array.isArray(parts) ? parts : [parts], cls }); }
const PHRASES = {
  first: ['Make this vocal darker and more aggressive.', 'Clean it up but keep the character.', 'Make it wider but keep it upfront.', 'Give my ad-libs a distant, psychedelic sound.', 'I don’t know what it needs. Help me improve it.'],
  follow: ['A little less.', 'Keep the vocal upfront.', 'Make it darker.', 'That’s too much width.', 'Keep the clean sound but add more character.'],
};
const ALL_PHRASES = [...PHRASES.first, ...PHRASES.follow];
const dimWord = (d) => ({ bright: 'tone change', warm: 'warmth', presence: 'presence', punch: 'aggression', width: 'width', depth: 'space', clean: 'clean-up', exp: 'mutation' }[d]);
async function kidCommand(text, via) {
  text = String(text || '').trim().slice(0, 160);
  if (!text) return;
  openDrawer('kid');
  const av = kidAvailable();
  if (!av.ok) { kidSay(av.why, 'err'); setPhase('unavailable'); return; }
  if (S.kid.busy) return;
  S.kid.busy = true;
  try {
    if (via === 'voice') { setPhase('transcribing'); await later(650); }
    S.kid.log.push({ who: 'you', text, via });
    setPhase('heard'); await later(350);
    setPhase('interpreting'); await later(500);
    const f = parseCommand(text);
    if (f.build) { S.kid.busy = false; startBuild(); return; }
    if (!f.understood) { kidSay('I didn’t hear a sound word in that. Try “darker”, “wider”, “more space”, “cleaner” or “a little less”. Your sound hasn’t changed.', 'err'); setPhase('error'); return; }
    if (f.improve && S.sim.signal === 'none') { kidSay('To know what your vocal needs I have to hear it. Press play in FL Studio and ask again. Nothing was changed.', 'err'); setPhase('error'); return; }
    if (f.scale && !Object.keys(f.ops).length && !S.kid.prop) { kidSay('There’s no proposal to scale back right now. Ask for a change, or use Undo to step back.'); setPhase('ready'); return; }
    setPhase('preparing'); await later(550);
    if (!S.kid.prop) {                            // a new proposal starts from the sound as it is now; constraints and role from an applied direction carry over
      const keep = S.kid.mem || { locks: [], role: '' };
      S.kid.mem = { dir: zeroDir(), locks: keep.locks || [], role: keep.role || '', base: copy(S.sound) };
    }
    const mem = S.kid.mem, prev = S.kid.prop;
    const said = [];
    if (f.scale) {
      const dims = Object.keys(f.ops);
      for (const dd of (dims.length ? dims : DIMS)) mem.dir[dd] *= f.scale;
      said.push([dims.length ? 'Less ' + dims.map(dimWord).join(' and ') + ': about ' + Math.round(f.scale * 100) + '% of what it was.' : 'Less of everything I proposed: about ' + Math.round(f.scale * 100) + '% of it.']);
    } else {
      for (const [dd, v] of Object.entries(f.ops)) mem.dir[dd] = clamp(mem.dir[dd] + v * f.mag, -2, 2);
      const words = f.heard.filter((h) => !/keep/.test(h));
      if (words.length) said.push(['Heard: ', { b: words.join(', ') }, '.']);
    }
    if (f.improve) { mem.dir.clean = Math.max(mem.dir.clean, 1); mem.dir.presence += 0.5; mem.dir.punch += 0.5; said.push(['From about 12 s of your vocal (', { b: 'simulated measurements' }, '): some low-mid build-up near 300 Hz, sharp S sounds, and words that jump in level. I’d clean it, bring the words forward and even them out.']); }
    for (const l of f.locks) if (!mem.locks.includes(l)) mem.locks.push(l);
    if (f.locks.length) said.push(['Keeping: ', { b: f.locks.filter((l) => l !== 'presence').map((l) => ({ front: 'the vocal upfront', char: 'your character', warm: 'your warmth', clean: 'your clean sound' }[l])).join(', ') }, '.']);
    if (f.role) { mem.role = f.role; said.push(['For ', { b: 'ad-libs' }, ': longer throws, further back.']); }
    if (f.newWords.length) said.push([{ badge: 'NEW WORDS' }, ' “' + f.newWords.join('”, “') + '” aren’t in KID’s vocabulary today; this prototype maps them as INTENT_PARAMETER_MAP describes.']);
    const { t, why, notes } = decide(mem);
    const rev = prev ? prev.rev + 1 : 1;
    const rows = rowsFrom(mem.base, t, why, prev ? prev.rows : null, rev, f.improve ? ['clean_amount', 'comp_ratio', 'eq_b2_gain'] : null);
    const removed = prev ? prev.rows.filter((r) => !rows.some((x) => x.id === r.id)).map((r) => PM[r.id].name) : [];
    if (removed.length) said.push(['No longer changing: ' + removed.join(', ') + '.']);
    for (const n of notes) said.push([n]);
    said.push(['Kept as yours: gain, mix and the output limiter' + (S.sound.tune_on >= 0.5 ? ', and your tuning key and scale' : '') + '.']);
    kidSay(said.flatMap((x, i) => (i ? [' ', ...x] : x)));
    if (!rows.length) { kidSay('That leaves nothing to change. Your sound is already there.'); S.kid.prop = null; setPhase('ready'); return; }
    S.kid.prop = { src: 'kid', rows, rev, listen: prev ? prev.listen : 'current' };
    setPhase('proposal');
  } finally { S.kid.busy = false; changed(); }
}
// push-to-talk: the command mic is a separate device and only opens while the button is held
function micDown() {
  openDrawer('kid');
  const av = kidAvailable(), sp = speechAvailable();
  if (!av.ok) { kidSay(av.why, 'err'); setPhase('unavailable'); return; }
  if (!sp.ok) { kidSay(sp.why, 'err'); setPhase('unavailable'); if (sp.kind === 'missing') openModal({ kind: 'speech' }); return; }
  if (S.kid.busy) return;
  S.kid.holdStart = performance.now();
  setPhase('recording');
}
function micUp() {
  if (S.kid.phase !== 'recording') return;
  if (performance.now() - S.kid.holdStart < 300 * pace.k) { kidSay('Hold the button while you speak, then let go.'); setPhase(S.kid.prop ? 'proposal' : 'ready'); return; }
  kidCommand(ALL_PHRASES[S.kid.phrase] || ALL_PHRASES[0], 'voice');
}
function openDrawer(mode) {
  if (S.drawer === mode) return;
  if (S.drawer === 'build' && mode === 'kid') { S.build = null; toast('Build My Mix closed. Nothing was applied.'); }
  S.drawer = mode; changed();
}
function closeDrawer() {
  if (S.drawer === 'build') { cancelProposal(); return; }
  if (S.kid.prop) S.kid.prop.listen = 'current';     // closing never leaves an unapplied proposal playing
  S.drawer = null;
  if (['applied', 'error', 'unavailable'].includes(S.kid.phase)) S.kid.phase = 'ready';
  changed();
}

// ---------------------------------------------------------------- Build My Mix (MixEngine: Factory Default + profile; the 22 user-owned parameters and Bypass never change)
function startBuild() {
  if (S.kid.prop) { S.kid.prop = null; S.kid.mem = null; kidSay('I set your waiting proposal aside (nothing was applied) and opened Build My Mix.'); if (S.kid.phase === 'proposal') S.kid.phase = 'ready'; }
  const av = kidAvailable();
  const pi = PROFILES.findIndex((p) => p.name === S.preset.name);
  S.build = { step: 1, listen: 0, listened: false, listening: false, profile: pi >= 0 ? pi : 1, intensity: 100, prop: null, err: av.ok ? '' : av.why };
  S.drawer = 'build';
  changed(true);
}
async function buildListen() {
  if (!S.build || S.sim.signal === 'none' || S.build.listening) return;
  S.build.listening = true; changed();
  for (let i = 1; i <= 10; i++) { await later(220); if (!S.build) return; S.build.listen = i * 10; changed(); }
  S.build.listening = false; S.build.listened = true; changed();
}
function buildProposal() {
  const b = S.build, base = copy(S.sound), t = profileSound(b.profile, base), why = {}, prof = PROFILES[b.profile];
  const k = 0.5 + 0.5 * b.intensity / 100;           // intensity: 100 % = as designed, 0 % = half (MixEngine)
  const scaled = ['echo_mix', 'dim_mix', 'dbl_amount', 'sat_drive'];
  for (const id of scaled) t[id] = clampP(id, t[id] * k);
  for (const id of IDS) why[id] = prof.o[id] !== undefined ? 'From the ' + prof.name + ' profile' + (scaled.includes(id) && b.intensity < 100 ? ', at ' + b.intensity + '% intensity' : '') : 'Back to its default: not part of this profile';
  why.mut_on = why.harm_on = why.stutter_on = 'Off: Build My Mix makes a vocal mix. Your creative effects can go back on afterwards';
  why.formant_shift = 'Back to 0: Build My Mix makes a vocal mix. Your formant can go back afterwards';
  const simIds = [];
  if (b.listened) {                                  // the two "fitted" changes are simulated measurements in this prototype
    t.comp_thresh = clampP('comp_thresh', t.comp_thresh - 2); why.comp_thresh = 'Fitted to your vocal: its loud words sit about 4 dB above the rest (simulated measurement)'; simIds.push('comp_thresh');
    t.comp_makeup = clampP('comp_makeup', t.comp_makeup - 1); why.comp_makeup = 'Fitted to your vocal: a little less makeup, because it’s already hot (simulated measurement)'; simIds.push('comp_makeup');
  }
  b.prop = { src: 'build', rows: rowsFrom(base, t, why, null, 1, simIds), rev: 1, listen: 'current' };
  b.step = 3;
  changed(true);
}

// ---------------------------------------------------------------- scene preview (an audition: nothing is loaded until "Load it")
function previewScene(name, sound) { S.preview = { name, sound: copy(sound) }; changed(true); }
function endPreview(keep) {
  const pv = S.preview;
  if (!pv) return;
  S.preview = null;
  if (keep) {
    const pi = PROFILES.findIndex((p) => p.name === pv.name);
    if (pi >= 0) loadProfile(pi); else if (pv.name === 'Factory Default') loadDefault(); else { const ui = S.lib.findIndex((p) => p.name === pv.name); if (ui >= 0) loadUser(ui); }
  } else { toast('Back to your sound. Nothing was loaded.'); changed(true); }
}

// ---------------------------------------------------------------- update loop and rendering plumbing
let frame, mainEl, topEl, railEl, chainEl, metersEl, helpEl, toastEl, modalEl;
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
const DEFAULT_HELP = 'Point at any control to read what it does. Hold K to talk to KID · D: DRY / PROCESSED · Ctrl/Cmd+Z: Undo.';
function tip(node, text) {
  node.setAttribute('aria-description', text);
  node.addEventListener('pointerenter', () => setHelp(text));
  node.addEventListener('pointerleave', () => setHelp(''));
  node.addEventListener('focus', () => setHelp(text));
  node.addEventListener('blur', () => setHelp(''));
  return node;
}
function setHelp(t) { if (helpEl) $('.hl-text', helpEl).textContent = t || DEFAULT_HELP; }
let toastTimer = 0;
function toast(msg, actLabel, act) {
  if (!toastEl) return;
  toastEl.replaceChildren(el('div', { class: 'toast', role: 'status', 'data-testid': 'toast' }, el('span', null, msg), actLabel ? el('button', { 'data-key': 'toast-act', onclick: () => { toastEl.replaceChildren(); act(); } }, actLabel) : null));
  clearTimeout(toastTimer); toastTimer = setTimeout(() => toastEl.replaceChildren(), 5200);
}

// ---------------------------------------------------------------- controls
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
function macroSlider(key) {
  const m = MACROS[key];
  return hslider({ key: 'macro-' + key, name: m.name, lo: m.lo, hi: m.hi, gesture: m.name + ' big move', get: () => m.get(S.sound), set: (v) => setMany(m.set(v)), text: () => m.text(S.sound),
    help: m.name + ': one move for several real controls — ' + m.ids + '. Open SHAPE or SPACE to see each one.' });
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
function stageBadge(m) { return tip(el('span', { class: 'badge stage' }, 'Stage ' + MOD[m].stage.join(' & ') + ' of 17'), 'Its fixed position in the processing order. Workspaces never change the order.'); }
function neighbors(stage) {
  const prev = STAGE_NAME[stage - 1], next = STAGE_NAME[stage + 1];
  return el('span', { class: 'neighbors' }, prev ? prev + ' → ' : 'input → ', el('b', null, STAGE_NAME[stage]), next ? ' → ' + next : ' → your mixer');
}
function onBadge(m) { const b = el('span', { class: 'badge' }); bind(() => { const on = modOn(m, S.sound); b.className = 'badge ' + (on ? 'on' : 'off'); b.textContent = on ? '● ON' : '○ OFF'; }); return b; }
function resetModule(m, ids, name) {
  const nm = name || MOD[m].name;
  return tip(el('button', { class: 'btn small ghost', 'data-key': 'reset-' + m, 'data-testid': 'reset-' + m, onclick: () => { gestureStart('Reset ' + nm); setMany(Object.fromEntries(ids.map((k) => [k, PM[k].def]))); gestureEnd(0); toast(nm + ' is back to its defaults.', 'Undo', undo); } }, 'Reset'), 'Put ' + nm + ' back to its defaults. Undo brings your settings back.');
}

// ---------------------------------------------------------------- the visual (illustrative): a picture of the settings, moving with a simulated level
function traits(s) {
  return {
    bright: clamp((s.eq_hi_gain + 0.5 * s.eq_b2_gain + (s.air_on >= 0.5 ? s.air_air / 40 : 0)) / 8, -1, 1),
    punch: clamp((s.comp_ratio - 1) / 5, 0, 1),
    warm: clamp(s.sat_drive / 9 + Math.max(0, s.eq_lo_gain) / 10, 0, 1),
    width: (s.dbl_on >= 0.5 ? s.dbl_width / 100 * 0.6 : 0) + (s.dim_on >= 0.5 ? (0.3 + s.dim_mix / 100) * 0.6 : 0),
    space: s.echo_on >= 0.5 ? clamp(s.echo_mix / 45, 0, 1) : 0,
    fb: s.echo_on >= 0.5 ? s.echo_fb / 90 : 0,
    char: clamp((s.mut_on >= 0.5 ? 0.35 + s.mut_amount / 200 : 0) + (Math.abs(s.formant_shift) >= 0.05 ? Math.min(0.5, Math.abs(s.formant_shift) / 12) : 0) + (s.harm_on >= 0.5 ? 0.25 : 0), 0, 1),
    stutter: s.stutter_on >= 0.5 ? s.stutter_mix / 100 : 0,
    tune: s.tune_on >= 0.5 ? (s.tune_strength / 100) * (s.tune_speed < 10 ? 1 : 0.6) : 0,
    clean: s.clean_on >= 0.5 ? s.clean_amount / 100 : 0,
  };
}
function mixRgb(a, b, t) { return a.map((v, i) => Math.round(v + (b[i] - v) * t)); }
const rgba = (c, a) => 'rgba(' + c[0] + ',' + c[1] + ',' + c[2] + ',' + Math.max(0, a).toFixed(3) + ')';
function drawOrb(ctx, W, H, s, o) {
  const tr = traits(s), cx = W / 2, cy = H / 2, R = Math.min(W, H) * 0.4, lv = o.level;
  ctx.clearRect(0, 0, W, H);
  const glow = ctx.createRadialGradient(cx, cy, 0, cx, cy, R * 1.25);
  glow.addColorStop(0, 'rgba(143,91,255,' + (0.18 + 0.22 * lv).toFixed(3) + ')'); glow.addColorStop(1, 'rgba(143,91,255,0)');
  ctx.fillStyle = glow; ctx.fillRect(0, 0, W, H);
  if (o.idle) {
    ctx.strokeStyle = 'rgba(199,190,214,.35)'; ctx.lineWidth = 1.5; ctx.setLineDash([3, 5]);
    ctx.beginPath(); ctx.arc(cx, cy, R * 0.32 * (1 + 0.04 * Math.sin(o.t * 1.2)), 0, Math.PI * 2); ctx.stroke(); ctx.setLineDash([]);
    return;
  }
  if (o.dry) {                                       // the original: one plain line, in the input colour
    ctx.strokeStyle = 'rgba(94,227,255,.9)'; ctx.lineWidth = 2;
    ctx.beginPath();
    for (let k = 0; k <= 180; k++) { const a = k / 180 * Math.PI * 2, r = R * (0.36 + 0.42 * lv) * (1 + 0.025 * Math.sin(5 * a + o.t * 2)); const x = cx + r * Math.cos(a), y = cy + r * Math.sin(a); if (k) ctx.lineTo(x, y); else ctx.moveTo(x, y); }
    ctx.closePath(); ctx.stroke();
    return;
  }
  const tb = (tr.bright + 1) / 2, col = mixRgb([143, 91, 255], [255, 79, 184], tb), col2 = mixRgb([125, 139, 255], [255, 134, 207], tb);
  const rot = o.motion ? o.t * 0.12 : 0, petals = 6 + Math.round(tr.tune * 6), sharp = 1 + tr.punch * 3, wob = 0.06 * (1 - tr.clean) + 0.05 * tr.char;
  const rings = Math.round(tr.space * 5 + tr.fb * 2);
  for (let i = 1; i <= rings; i++) {                 // echo: rings that drift outward
    const drift = o.motion ? ((o.t * 0.25) % 1) : 0.5;
    const r = R * (0.62 + (i - 1 + drift) * 0.14) * (0.85 + 0.25 * lv);
    ctx.strokeStyle = rgba(col2, 0.32 * (1 - i / (rings + 1)) * (0.4 + tr.space)); ctx.lineWidth = 1.2;
    ctx.beginPath(); ctx.arc(cx, cy, r, 0, Math.PI * 2); ctx.stroke();
  }
  const flower = (fx, fy, scale, alpha, dir, flip) => {
    const rad = R * scale * (0.34 + 0.5 * lv);
    ctx.save(); ctx.translate(fx, fy); if (flip) ctx.scale(-1, 1); ctx.rotate(dir * rot);
    const path = new Path2D();
    for (let k = 0; k <= 240; k++) {
      const a = k / 240 * Math.PI * 2;
      const r = rad * (0.6 + 0.4 * Math.pow(Math.abs(Math.cos(petals * a / 2)), sharp)) * (1 + wob * Math.sin(3 * a + o.t * 1.7));
      if (k) path.lineTo(r * Math.cos(a), r * Math.sin(a)); else path.moveTo(r * Math.cos(a), r * Math.sin(a));
    }
    path.closePath();
    const g = ctx.createRadialGradient(0, 0, 0, 0, 0, rad);
    g.addColorStop(0, rgba([255, 134, 207], 0.42 * alpha)); g.addColorStop(1, rgba(col, 0.08 * alpha));
    ctx.fillStyle = g; ctx.fill(path);
    if (tr.char > 0.05) {                           // character: a chromatic split
      ctx.save(); ctx.translate(-5 * tr.char, -1.5); ctx.strokeStyle = rgba([94, 227, 255], 0.45 * alpha * tr.char); ctx.lineWidth = 1.3; ctx.stroke(path); ctx.restore();
      ctx.save(); ctx.translate(5 * tr.char, 1.5); ctx.strokeStyle = rgba([255, 79, 184], 0.55 * alpha * tr.char); ctx.lineWidth = 1.3; ctx.stroke(path); ctx.restore();
    }
    if (tr.stutter > 0.05) { ctx.setLineDash([9, 5]); ctx.lineDashOffset = o.motion ? -o.t * 30 : 0; }
    ctx.strokeStyle = rgba(col, alpha); ctx.lineWidth = 2 + tr.warm * 2.5;
    ctx.shadowColor = rgba(col, 0.7 * alpha); ctx.shadowBlur = 10 + 10 * tr.warm;
    ctx.stroke(path); ctx.setLineDash([]); ctx.shadowBlur = 0;
    ctx.restore();
  };
  if (tr.width > 0.05) for (const side of [-1, 1]) flower(cx + side * R * (0.55 + 0.6 * tr.width), cy, 0.55, 0.4, -1, side < 0);
  flower(cx, cy, 1, 1, 1, false);
}
let level = 0;
function simLevel(t) {                             // a phrase-shaped envelope; there is no audio here
  if (S.sim.signal === 'none' || S.sim.engine === 'error') return 0;
  const ph = t % 4.6, gate = ph < 3.5 ? clamp(Math.min(ph / 0.15, (3.5 - ph) / 0.2), 0, 1) : 0;
  return gate * (0.55 + 0.45 * Math.abs(Math.sin(t * 6.1)) * (0.65 + 0.35 * Math.sin(t * 1.7)));
}
let peakIn = -90, peakOut = -90, peakAge = 0, clipAt = -1e9;
const t0 = performance.now();
function loop(now) {
  const t = (now - t0) / 1000;
  level += (simLevel(t) - level) * 0.25;
  const clipping = S.sim.signal === 'clipping';
  const inDb = level > 0.01 ? Math.min(0, -60 + (clipping ? 72 : 50) * level) : -90;
  const s = heard();
  let outDb = S.bypass ? inDb : inDb + s.gain + 0.5 * s.comp_makeup - (s.comp_ratio > 1.001 ? 2 : 0);
  if (!S.bypass && s.lim_on >= 0.5) outDb = Math.min(outDb, s.lim_ceiling);
  outDb = Math.min(outDb, 0);
  if (level > 0.01 && inDb >= -0.1) clipAt = now;
  if (++peakAge > 90) { peakAge = 0; peakIn = inDb; peakOut = outDb; }
  peakIn = Math.max(peakIn, inDb); peakOut = Math.max(peakOut, outDb);
  if (metersEl) {
    const h = (db) => clamp((db + 60) / 60, 0, 1) * 100 + '%';
    $('.mt-in .mt-fill', metersEl).style.height = h(inDb);
    $('.mt-out .mt-fill', metersEl).style.height = h(outDb);
    $('.mt-in .mt-val', metersEl).textContent = peakIn > -60 ? String(Math.round(peakIn)) : '—';
    $('.mt-out .mt-val', metersEl).textContent = peakOut > -60 ? String(Math.round(peakOut)) : '—';
    const clip = $('.mt-clip', metersEl), on = now - clipAt < 1500;
    clip.classList.toggle('on', on); clip.textContent = on ? 'CLIP' : '';
  }
  const cv = frame && frame.querySelector('#gk-orb');
  if (cv) {
    const ctx = cv.getContext('2d');
    ctx.setTransform(cv.width / 360, 0, 0, cv.width / 360, 0, 0);
    drawOrb(ctx, 360, 360, s, { t: S.sim.motion ? t : 0, level, dry: S.bypass, motion: S.sim.motion, idle: level < 0.02 && (S.sim.signal === 'none' || S.sim.engine === 'error') });
  }
  const ob = document.getElementById('onb-bar');
  if (ob) ob.style.width = (level * 100).toFixed(1) + '%';
  if (S.kid.phase === 'recording') frame.querySelectorAll('.cmdlevel i').forEach((b, i) => { b.style.height = (4 + 18 * Math.abs(Math.sin(t * 9 + i * 1.3)) * (0.5 + 0.5 * Math.sin(t * 2.3 + i))) + 'px'; });
  requestAnimationFrame(loop);
}

// ---------------------------------------------------------------- shell
function renderShell() {
  frame = $('#frame');
  frame.replaceChildren(svgFrom('<svg xmlns="http://www.w3.org/2000/svg" width="0" height="0" style="position:absolute" aria-hidden="true"><defs><linearGradient id="gk-arc" x1="0" y1="1" x2="1" y2="0"><stop offset="0" stop-color="#8f5bff"/><stop offset="1" stop-color="#ff4fb8"/></linearGradient><radialGradient id="gk-cap" cx="50%" cy="30%" r="75%"><stop offset="0" stop-color="#3a3349"/><stop offset="1" stop-color="#141019"/></radialGradient></defs></svg>'));
  topEl = el('header', { class: 'topbar', 'data-testid': 'topbar' });
  railEl = el('nav', { class: 'rail', 'aria-label': 'Workspaces' });
  mainEl = el('div', { class: 'main', id: 'gk-main' });
  metersEl = el('div', { class: 'meters', 'data-testid': 'meters', 'aria-label': 'Input and output level (simulated in this prototype)' },
    el('div', { class: 'mt' },
      el('div', { class: 'mt-col mt-in' }, el('div', { class: 'mt-lbl' }, 'IN'), el('div', { class: 'mt-bar' }, el('div', { class: 'mt-fill' })), el('div', { class: 'mt-val' }, '—')),
      el('div', { class: 'mt-col mt-out' }, el('div', { class: 'mt-lbl' }, 'OUT'), el('div', { class: 'mt-bar' }, el('div', { class: 'mt-fill' })), el('div', { class: 'mt-val' }, '—')),
      el('div', { class: 'mt-clip', 'data-testid': 'clip-in', 'aria-live': 'polite' })));
  tip(metersEl, 'Peak level coming in (cyan) and going out (violet to pink), in dBFS. CLIP lights when the input reaches full scale. Simulated in this prototype.');
  chainEl = el('div', { class: 'chain', role: 'list', 'aria-label': 'Signal chain, in its fixed order' });
  helpEl = el('div', { class: 'helpline', 'aria-live': 'polite' }, el('span', { class: 'hl-text' }), el('span', { class: 'hl-kbd' }, 'PROTOTYPE · simulated audio'));
  toastEl = el('div', { class: 'toast-layer', 'aria-live': 'polite' });
  modalEl = el('div', { id: 'gk-modal' });
  frame.append(topEl, railEl, mainEl, metersEl, chainEl, helpEl, toastEl, modalEl);
  setHelp('');
}
function renderTop() {
  const mod = isModified();
  const type = { default: 'Default', factory: 'Factory', user: 'Yours', built: 'Built' }[S.preset.type];
  const pill = tip(el('button', { class: 'scene-pill', 'data-key': 'scene-pill', 'data-testid': 'scene-pill', onclick: () => go('scenes') },
    el('span', { class: 'sp-dot' }),
    el('span', { class: 'sp-txt' }, el('span', { class: 'sp-name', 'data-testid': 'scene-name' }, S.preset.name),
      el('span', { class: 'sp-meta', 'data-testid': 'scene-meta' }, type, ' · ', mod ? el('span', { class: 'unsaved' }, '● Unsaved') : S.preset.type === 'built' ? el('span', { class: 'unsaved' }, '● Not saved') : 'Saved')),
    el('span', { class: 'sp-caret' }, icon('down', 16))), 'The sound that is loaded. Click to browse Scenes. “Unsaved changes” means it differs from what was loaded or saved.');
  const prev = tip(el('button', { class: 'btn icon', 'data-key': 'scene-prev', 'aria-label': 'Previous factory sound', onclick: () => stepScene(-1) }, icon('left', 18)), 'Previous factory sound (Undo goes back).');
  const next = tip(el('button', { class: 'btn icon', 'data-key': 'scene-next', 'aria-label': 'Next factory sound', onclick: () => stepScene(1) }, icon('right', 18)), 'Next factory sound (Undo goes back).');
  const ab = el('div', { class: 'ab', role: 'group', 'aria-label': 'A/B comparison slots' }, ['A', 'B'].map((k) => {
    const sl = S.ab[k], active = S.ab.active === k, modk = active && abModified();
    const state = !sl ? 'empty' : modk ? 'changed' : active ? 'playing' : 'stored';
    const wrap = el('div', { class: 'ab-slot' + (active ? ' active' : '') + (modk ? ' modified' : ''), 'data-testid': 'ab-' + k, 'data-state': state });
    wrap.append(tip(el('button', { class: 'ab-main', 'data-key': 'ab-' + k, 'data-testid': 'ab-btn-' + k, 'aria-label': k + ': ' + state, onclick: () => abClick(k) }, el('b', null, k), el('span', null, state)),
      !sl ? 'Slot ' + k + ' is empty. Click to store the current sound in it.' : active ? (modk ? k + ' is playing; your changes are not stored in it. ⤓ stores them.' : k + ' is playing.') : 'Click to hear ' + k + '. If the slot that is playing has unstored changes, GlitchKid asks first.'));
    if (sl) wrap.append(tip(el('button', { class: 'ab-store', 'data-key': 'abstore-' + k, 'data-testid': 'ab-store-' + k, 'aria-label': 'Store the current sound in ' + k, onclick: () => abStore(k) }, '⤓'), 'Store the current sound in ' + k + '. If ' + k + ' holds something different, GlitchKid asks before replacing it.'));
    return wrap;
  }));
  const save = tip(el('button', { class: 'btn', 'data-key': 'save', 'data-testid': 'save', onclick: saveClicked }, icon('save', 16), 'Save'), 'Save this sound as a preset. Ctrl/Cmd+S.');
  const last = S.undo[S.undo.length - 1];
  const und = tip(el('button', { class: 'btn undo-btn', 'data-key': 'undo', 'data-testid': 'undo', 'aria-disabled': last ? 'false' : 'true', onclick: () => (last ? undo() : toast('Nothing to undo.')) }, icon('undo', 16), 'Undo'), last ? 'Undo: ' + last.label + '. Ctrl/Cmd+Z.' : 'Nothing to undo yet.');
  const cmp = tip(el('div', { class: 'compare', role: 'group', 'aria-label': 'Compare the original with GlitchKid' },
    el('button', { class: 'dry', 'data-key': 'cmp-dry', 'data-testid': 'cmp-dry', 'aria-pressed': S.bypass ? 'true' : 'false', onclick: () => setBypass(true) }, 'DRY'),
    el('button', { 'data-key': 'cmp-proc', 'data-testid': 'cmp-proc', 'aria-pressed': S.bypass ? 'false' : 'true', onclick: () => setBypass(false) }, 'PROCESSED')),
  'DRY plays your original vocal at matched loudness, so louder never fools you; PROCESSED plays GlitchKid. Press D to switch.');
  const help = tip(el('button', { class: 'btn icon ghost', 'data-key': 'help', 'data-testid': 'help', 'aria-label': 'Help', onclick: () => openModal({ kind: 'help' }) }, icon('help', 20)), 'Help, shortcuts and how KID works.');
  topEl.replaceChildren(el('div', { class: 'logo' }, el('b', null, 'GLITCHKID'), el('span', null, 'Vocal Playground')), pill, el('div', { class: 'step-btns' }, prev, next), ab, el('div', { class: 'tb-gap' }), save, und, cmp, help);
}
const RAIL = [['booth', 'BOOTH', 'Home: your sound, the visual, KID'], ['shape', 'SHAPE', 'Clean, tune, tone, dynamics'], ['character', 'CHARACTER', 'Experimental Lab: formant, harmony, mutation, stutter'], ['space', 'SPACE', 'Doubler, echo, Dimension, output'], ['scenes', 'SCENES', 'Factory sounds, yours, and concepts']];
function renderRail() {
  const items = RAIL.map(([k, label, help]) => tip(el('button', { class: 'rail-item', 'data-key': 'rail-' + k, 'data-testid': 'rail-' + k, 'aria-current': S.view.ws === k ? 'page' : null, onclick: () => go(k) }, icon(k), label), help));
  const waiting = !!S.kid.prop && S.drawer !== 'kid';
  const kb = tip(el('button', { class: 'rail-item rail-kid', 'data-key': 'rail-kid', 'data-testid': 'rail-kid', onclick: () => (S.drawer === 'kid' ? closeDrawer() : openDrawer('kid')) }, icon('mic'), 'KID', el('span', { class: 'ks' }, waiting ? 'Waiting' : S.drawer === 'kid' ? 'Open' : 'Ask'), waiting ? el('span', { class: 'rdot', 'aria-label': 'One proposal waiting' }, '1') : null),
    waiting ? 'KID has a proposal waiting for your review. Nothing has been applied.' : 'Open KID, your vocal director. Hold K anywhere to talk.');
  railEl.replaceChildren(...items, el('div', { class: 'rail-spacer' }), kb);
}
function renderChain() {
  const s = heard();
  const chips = CHAIN.map(([n, m, label]) => {
    const on = modOn(m, s), ws = MOD[m].ws;
    return tip(el('button', { class: 'cchip' + (on ? ' on' : '') + (S.view.ws === ws ? ' here' : ''), role: 'listitem', 'data-ws': ws, 'data-key': 'chain-' + n, 'data-testid': 'chain-' + n, 'data-on': on ? '1' : '0', 'aria-label': n + ' ' + STAGE_NAME[n] + ', ' + (on ? 'on' : 'off'), onclick: () => goModule(m) }, el('span', { class: 'cd' }), label),
      'Stage ' + n + ' of 17: ' + STAGE_NAME[n] + ' (' + WS[ws].name + '), ' + (on ? 'on' : 'off') + '. Click to open it.');
  });
  chainEl.replaceChildren(tip(el('span', { class: 'chain-lbl' }, 'CHAIN 1→17'), 'The processing order is fixed: 1 to 17, left to right. Workspaces group the controls; they never reorder the chain.'), ...chips);
}
function go(ws) { S.view.ws = ws; if (ws !== 'scenes') S.preview = null; changed(true); }
function goModule(m) { const M = MOD[m]; S.view.ws = M.ws; if (M.tab) S.view.tab = M.tab; S.preview = null; changed(true); }

// ---------------------------------------------------------------- main area
function renderMain() {
  CTRL.clear(); binders = [];
  const v = { booth: viewBooth, shape: viewShape, character: viewCharacter, space: viewSpace, scenes: viewScenes }[S.view.ws]();
  mainEl.replaceChildren(v);
  if (S.preview) mainEl.append(previewBar());
  if (drawerEl) mainEl.append(drawerEl);
}
function engineBanner() {
  if (S.sim.engine === 'mastering') return el('div', { class: 'banner warn', 'data-testid': 'banner-engine', style: { marginBottom: '10px' } }, el('b', null, 'Mastering mode is on. '), 'This instance is processing a full mix, so these vocal controls are not in use. Switch Mastering mode off on the MASTER page (unchanged by this redesign).');
  if (S.sim.engine === 'error') return el('div', { class: 'banner bad', 'data-testid': 'banner-engine', style: { marginBottom: '10px' } }, el('b', null, 'GlitchKid isn’t processing. '), 'Audio passes through unchanged. Design placeholder: the plug-in has no verified trigger for this state yet.');
  return null;
}
function viewBooth() {
  const name = el('h2', { class: 'h1', 'data-testid': 'booth-name' }), tag = el('div', { class: 'sub' }), badges = el('div', { class: 'row' });
  bind(() => {
    name.textContent = S.preset.name;
    const pi = PROFILES.findIndex((p) => p.name === S.preset.name);
    tag.textContent = S.preset.type === 'factory' && pi >= 0 ? PROFILES[pi].tag : S.preset.type === 'user' ? 'Your saved sound' : S.preset.type === 'built' ? 'Build My Mix result. Save it to keep it.' : 'Every module at its default: your vocal passes through unchanged.';
    const list = [el('span', { class: 'badge ' + (S.preset.type === 'user' ? 'user' : 'factory') }, { default: 'FACTORY DEFAULT', factory: 'FACTORY', user: 'YOURS', built: 'BUILT' }[S.preset.type])];
    if (isModified()) list.push(el('span', { class: 'badge unsaved' }, '● UNSAVED'));
    else if (S.preset.type === 'built') list.push(el('span', { class: 'badge unsaved' }, '● NOT SAVED'));
    if (S.ab.active) list.push(el('span', { class: 'badge' }, 'PLAYING ' + S.ab.active));
    badges.replaceChildren(...list);
  });
  const scene = el('section', { class: 'card scene-card', 'aria-label': 'Loaded sound' }, el('div', { class: 'micro' }, 'Now loaded'), name, tag, badges);
  const macros = el('section', { class: 'card macros', 'aria-label': 'Big moves' }, el('div', { class: 'micro' }, 'Big moves'), macroSlider('tone'), macroSlider('punch'), macroSlider('space'));
  const sig = el('section', { class: 'card signal-card', 'data-testid': 'signal-card', 'aria-live': 'polite' });
  bind(() => {
    let cls, icn, title, text;
    if (S.sim.engine === 'mastering') { cls = 'none'; icn = '!'; title = 'Mastering mode is on'; text = 'The vocal chain isn’t in use here. Switch it off on MASTER.'; }
    else if (S.sim.engine === 'error') { cls = 'clip'; icn = '!'; title = 'Not processing'; text = 'Audio passes through unchanged (design placeholder).'; }
    else if (S.sim.signal === 'none') { cls = 'none'; icn = '–'; title = 'No vocal coming in'; text = 'Press play in FL Studio. GlitchKid must sit on the vocal’s Mixer insert.'; }
    else if (S.sim.signal === 'clipping') { cls = 'clip'; icn = '!'; title = 'Input is clipping'; text = 'Lower the recording level. Clipping already in the audio can’t be repaired.'; }
    else { cls = 'ok'; icn = '✓'; title = 'Receiving your vocal'; text = 'Levels are on the right (simulated here).'; }
    sig.className = 'card signal-card ' + cls;
    sig.replaceChildren(el('div', { class: 'si', 'aria-hidden': 'true' }, icn), el('div', null, el('b', null, title), el('p', null, text)));
  });
  const cv = el('canvas', { id: 'gk-orb', width: 720, height: 720, 'aria-hidden': 'true' });
  const chip = el('span', { class: 'hear-chip', 'data-testid': 'hear-chip' }), msg = el('div', { class: 'orb-msg' });
  bind(() => {
    const pr = activeProposal();
    chip.className = 'hear-chip' + (S.bypass ? ' dry' : S.preview || (pr && pr.listen === 'proposal') ? ' prop' : '');
    chip.textContent = S.bypass ? 'HEARING · DRY (ORIGINAL)' : S.preview ? 'PREVIEW · ' + S.preview.name : pr && pr.listen === 'proposal' ? 'HEARING · PROPOSAL, NOT APPLIED' : 'HEARING · PROCESSED';
    const quiet = S.sim.signal === 'none' || S.sim.engine === 'error';
    msg.replaceChildren(...(quiet ? [el('b', null, S.sim.engine === 'error' ? 'Not processing' : 'Waiting for your vocal'), el('span', null, S.sim.engine === 'error' ? 'Audio passes through unchanged.' : 'Play your track. The visual wakes up when sound arrives.')] : []));
  });
  const orb = tip(el('div', { class: 'orb-box', 'data-testid': 'orb' }, cv, el('span', { class: 'badge sim orb-tag' }, 'ILLUSTRATIVE'), msg, el('div', { class: 'orb-state' }, chip)),
    'Illustrative visual: drawn from your settings and moved by a simulated level. In the plug-in it would follow your real input and output.');
  const ns = el('section', { class: 'card next-step', 'data-testid': 'next-step' });
  bind(() => {
    let t, d, b;
    if (S.sim.signal === 'none') { t = 'Play your vocal'; d = 'GlitchKid needs to hear something.'; b = ['How to route it', () => openModal({ kind: 'help' })]; }
    else if (S.preset.type === 'default' && !isModified()) { t = 'Choose a starting sound'; d = 'Pick a Scene, or ask KID to build one with you.'; b = ['Browse Scenes', () => go('scenes')]; }
    else if (!S.flags.compared) { t = 'Compare with your original'; d = 'Switch to DRY for a moment. Same loudness, so it’s a fair test.'; b = ['Hear DRY', () => setBypass(true)]; }
    else if (isModified() || S.preset.type !== 'user') { t = 'Save this sound'; d = 'Saved sounds are in SCENES › Yours, in every project.'; b = ['Save', saveClicked]; }
    else { t = 'Shape the details'; d = 'Every control is in SHAPE, CHARACTER and SPACE.'; b = ['Open SHAPE', () => go('shape')]; }
    ns.replaceChildren(el('div', { class: 'ns-txt' }, el('span', { class: 'micro' }, 'Next step'), el('b', null, t), d), el('button', { class: 'btn small', 'data-key': 'ns-btn', 'data-testid': 'next-step-btn', onclick: b[1] }, b[0]));
  });
  const status = el('div', { class: 'kid-status', 'data-testid': 'kid-status' }), micB = el('button', { class: 'mic', 'data-key': 'mic-big', 'data-testid': 'mic-big', 'aria-label': 'Hold to talk to KID' }, el('span', { class: 'mic-in' }, icon('mic'), el('span', null, 'HOLD TO TALK')));
  wireMic(micB);
  bind(() => {
    const av = kidAvailable(), sp = speechAvailable(), ph = S.kid.phase;
    micB.className = 'mic' + (ph === 'recording' ? ' rec' : '') + (!av.ok || !sp.ok ? ' unavail' : '');
    status.className = 'kid-status' + (!av.ok || !sp.ok ? ' err' : '');
    status.textContent = !av.ok ? 'Unavailable: ' + (S.sim.engine === 'mastering' ? 'Mastering mode is on' : 'not processing') : !sp.ok ? (sp.kind === 'missing' ? 'Speech isn’t set up yet. Typing works.' : 'Microphone blocked. Typing works.') : ph === 'recording' ? 'Listening… let go to send' : S.sim.net === 'offline' ? 'Ready. Offline is fine: it all runs here.' : 'Ready. Speech stays on this computer.';
  });
  const input = el('input', { class: 'field', 'data-key': 'kid-type', 'data-testid': 'kid-type', placeholder: 'Or type a request', 'aria-label': 'Type a request to KID', maxlength: 160 });
  const send = () => { const v = input.value; input.value = ''; kidCommand(v, 'typed'); };
  input.addEventListener('keydown', (e) => { if (e.key === 'Enter') send(); else if (e.key === 'Escape') input.blur(); e.stopPropagation(); });
  const waiting = el('div');
  bind(() => waiting.replaceChildren(...(S.kid.prop && S.drawer !== 'kid' ? [el('div', { class: 'waiting', 'data-testid': 'proposal-waiting' }, el('span', { style: { flex: 1 } }, el('b', null, 'A proposal is waiting. '), 'Nothing has been applied.'), el('button', { class: 'btn small', 'data-key': 'review-waiting', 'data-testid': 'review-waiting', onclick: () => openDrawer('kid') }, 'Review'))] : [])));
  const kid = el('section', { class: 'card kid-card', 'aria-label': 'KID, your vocal director' },
    el('div', { class: 'kc-head' }, el('b', null, 'KID'), el('span', null, 'Your vocal director'), el('span', { class: 'dim' }, 'You speak. It builds.')),
    micB, status,
    el('div', { class: 'chips', 'aria-label': 'Examples' }, ['Darker and more aggressive', 'Clean it up, keep the character'].map((t) => el('button', { class: 'chip', 'data-key': 'ex-' + t, onclick: () => kidCommand(t, 'typed') }, '“' + t + '”'))),
    el('div', { class: 'type-row' }, input, el('button', { class: 'btn small', 'data-key': 'kid-send', 'data-testid': 'kid-send', onclick: send }, 'Send')),
    waiting, el('div', { class: 'kid-sep' }),
    tip(el('button', { class: 'btn primary', 'data-key': 'build-start', 'data-testid': 'build-start', style: { width: '100%' }, onclick: startBuild }, 'Help me build my vocal'), 'Build My Mix: GlitchKid listens to your vocal, you choose a direction, then you review every change before anything is applied.'),
    el('div', { class: 'dr-note' }, 'Listens, proposes, then waits for you. Nothing changes until you apply.'));
  const grid = el('div', { class: 'booth' }, el('div', { class: 'b-left' }, scene, macros, sig), el('div', { class: 'b-center' }, orb, el('div', { class: 'orb-caption' }, 'Illustrative: a picture of your settings, moving with a simulated level.'), ns), el('div', { class: 'b-right' }, kid));
  bind(() => grid.classList.toggle('with-drawer', !!S.drawer));   // the visual stays in view while KID or Build My Mix is open
  return el('div', { class: 'screen', 'data-testid': 'screen-booth' }, grid);
}
function wireMic(b) {
  b.addEventListener('pointerdown', (e) => { e.preventDefault(); try { b.setPointerCapture(e.pointerId); } catch { /* synthetic events */ } micDown(); });
  b.addEventListener('pointerup', micUp);
  b.addEventListener('pointercancel', micUp);
  b.addEventListener('keydown', (e) => { if ((e.key === ' ' || e.key === 'Enter') && !e.repeat) { e.preventDefault(); micDown(); } });
  b.addEventListener('keyup', (e) => { if (e.key === ' ' || e.key === 'Enter') { e.preventDefault(); micUp(); } });
  tip(b, 'Push to talk: hold, speak, let go. The command mic is separate from your vocal and only listens while held. Space or Enter also hold it.');
}
function wsHead(ws, extra) {
  return el('div', { class: 'ws-head' }, el('div', null, el('h1', { class: 'h1' }, el('span', { class: 'ws-mark', style: { background: WS[ws].color } }), WS[ws].name + ' — ' + WS[ws].line),
    el('div', { class: 'ws-line' }, { shape: 'Clean, tune, tone and dynamics. Grouped for you; the processing order never changes.', character: 'The Experimental Lab. Formant, Harmony and Mutation run early (2–4); Stutter acts on the finished voice (12).', space: 'Doubler (5, early), Echo (13), Dimension (14), then the output stage (15–17).' }[ws])), extra || null);
}
function viewShape() {
  const tabs = el('div', { class: 'tabs', role: 'tablist', 'aria-label': 'SHAPE modules' }, [['clean', 'CLEAN', '7·9', ['clean']], ['tune', 'TUNE', '1', ['tune']], ['tone', 'TONE', '6·10·11', ['eq', 'warmth', 'air']], ['dynamics', 'DYNAMICS', '8', ['comp']]].map(([k, label, st, mods]) => {
    const dot = el('span', { class: 'td' });
    const b = el('button', { class: 'tab', role: 'tab', 'data-key': 'tab-' + k, 'data-testid': 'tab-' + k, 'aria-selected': S.view.tab === k ? 'true' : 'false', onclick: () => { S.view.tab = k; changed(true); } }, dot, label, el('span', { class: 'ts' }, st));
    bind(() => { const on = mods.some((m) => modOn(m, S.sound)); dot.className = 'td' + (on ? ' on' : ''); b.setAttribute('aria-label', label + ', stage ' + st + ', ' + (on ? 'on' : 'off')); });
    return b;
  }));
  const body = { clean: shapeClean, tune: shapeTune, tone: shapeTone, dynamics: shapeDynamics }[S.view.tab]();
  return el('div', { class: 'screen scroll', 'data-testid': 'screen-shape' }, engineBanner(), wsHead('shape', tabs), body);
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
  const eq = modCard('eq', { reset: resetModule('eq', eqIds), desc: 'Four bands. At 0 dB a band leaves the voice untouched. The TONE big move on the Booth moves Air shelf and Presence together.' },
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
  return modCard('comp', { reset: resetModule('comp', ids), desc: 'Evens out loud and quiet words. Ratio 1:1 = no compression (the default). The PUNCH big move sets all six controls in one go.' },
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
function viewCharacter() {
  return el('div', { class: 'screen scroll', 'data-testid': 'screen-character' }, engineBanner(), wsHead('character'),
    el('div', { class: 'grid2' },
      labCard('formant', '', { reset: resetModule('formant', ['formant_shift']), desc: 'The size of the voice, not its pitch. Down: bigger. Up: smaller. 0 = off; adds about 21 ms latency while not 0.' }, [knob('formant_shift', { size: 76, label: 'Shift' })]),
      labCard('harmony', '', { sw: sw('harm_on'), reset: resetModule('harmony', ['harm_on', 'harm_interval', 'harm_level']), desc: 'One extra voice at a fixed interval. Not key-aware. Adds Pitch Tune’s latency while on.' },
        [el('div', null, el('div', { class: 'micro', style: { marginBottom: '6px' } }, 'Interval'), sel('harm_interval')), knob('harm_level', { label: 'Level', offWhen: () => S.sound.harm_on < 0.5 })]),
      labCard('mutation', '', { sw: sw('mut_on'), reset: resetModule('mutation', ['mut_on', 'mut_amount', 'mut_character', 'mut_blend']), desc: 'Bends the voice into something new. MORE: its character (down, robot, up) and how much you hear.' },
        [knob('mut_amount', { size: 76, label: 'Amount', offWhen: () => S.sound.mut_on < 0.5 })], () => el('div', { class: 'mod-row' }, knob('mut_character', { help: 'Down ← robot → up' }), knob('mut_blend'))),
      labCard('stutter', '', { sw: sw('stutter_on'), reset: resetModule('stutter', ['stutter_on', 'stutter_rate', 'stutter_mix']), desc: 'Tempo-locked slices of the finished voice (stage 12, after the tone stages).' },
        [el('div', null, el('div', { class: 'micro', style: { marginBottom: '6px' } }, 'Slice'), seg('stutter_rate')), knob('stutter_mix', { label: 'Mix', offWhen: () => S.sound.stutter_on < 0.5 })])));
}
function viewSpace() {
  return el('div', { class: 'screen scroll', 'data-testid': 'screen-space' }, engineBanner(), wsHead('space'),
    el('div', { class: 'grid2' },
      labCard('doubler', 'space-card', { sw: sw('dbl_on'), reset: resetModule('doubler', ['dbl_on', 'dbl_amount', 'dbl_width', 'dbl_detune', 'dbl_timing']), desc: 'A second, slightly different you, spread wide. It runs early (stage 5), so EQ and compression treat the doubles like your voice.' },
        [knob('dbl_amount', { label: 'Amount', offWhen: () => S.sound.dbl_on < 0.5 }), knob('dbl_width', { label: 'Width', offWhen: () => S.sound.dbl_on < 0.5 })], () => el('div', { class: 'mod-row' }, knob('dbl_detune', { label: 'Detune' }), knob('dbl_timing', { label: 'Timing' }))),
      labCard('echo', 'space-card', { sw: sw('echo_on'), reset: resetModule('echo', ['echo_on', 'echo_sync', 'echo_time', 'echo_div', 'echo_fb', 'echo_tone', 'echo_width', 'echo_mix']), desc: 'Repeats that move with your song, with their own tone and width.' },
        [knob('echo_mix', { size: 72, label: 'Mix', offWhen: () => S.sound.echo_on < 0.5 }), el('div', { style: { display: 'flex', flexDirection: 'column', gap: '8px' } }, el('div', { class: 'micro' }, 'Timing'), sw('echo_sync', { text: (x) => (x ? 'SYNC' : 'FREE'), help: 'Sync: repeats follow the song tempo (Division). Free: set a time in ms.' }), sel('echo_div'))],
        () => el('div', { class: 'mod-row' }, knob('echo_fb'), knob('echo_tone'), knob('echo_width'), knob('echo_time', { help: 'Used when Sync is off' }))),
      labCard('dimension', 'space-card', { sw: sw('dim_on'), reset: resetModule('dimension', ['dim_on', 'dim_width', 'dim_detune', 'dim_depth', 'dim_mix']), desc: 'Stereo width and gentle movement around the voice.' },
        [knob('dim_mix', { label: 'Mix', offWhen: () => S.sound.dim_on < 0.5 }), knob('dim_width', { label: 'Width', offWhen: () => S.sound.dim_on < 0.5 })], () => el('div', { class: 'mod-row' }, knob('dim_depth', { label: 'Depth' }), knob('dim_detune', { label: 'Detune' }))),
      outputCard()));
}
function outputCard() {
  return el('section', { class: 'card lab-card space-card', 'aria-label': 'Output', 'data-testid': 'mod-output' },
    el('div', { class: 'mod-head' }, el('span', { class: 'mod-name' }, 'Output'), el('span', { class: 'badge stage' }, 'Stages 15 · 16 · 17'), el('span', { class: 'mod-sp' }), resetModule('limiter', ['gain', 'mix', 'lim_on', 'lim_ceiling', 'lim_drive', 'lim_release'], 'Output')),
    el('p', { class: 'mod-desc' }, 'Gain (15), dry/wet Mix (16), then the Output Limiter (17): the last thing before your mixer. KID never changes these.'),
    el('div', { class: 'mod-row' }, knob('gain'), knob('mix', { help: 'Dry/wet blend. 100% = fully processed' }), el('div', { style: { display: 'flex', flexDirection: 'column', gap: '8px' } }, el('div', { class: 'micro' }, 'Limiter'), sw('lim_on')), knob('lim_ceiling', { offWhen: () => S.sound.lim_on < 0.5 })),
    advanced('limiter', 'More', () => el('div', { class: 'mod-row' }, knob('lim_drive'), knob('lim_release'))));
}

// ---------------------------------------------------------------- scenes
function sceneThumb(sound) {
  const cv = el('canvas', { width: 416, height: 168, 'aria-hidden': 'true' });
  requestAnimationFrame(() => { const ctx = cv.getContext('2d'); ctx.setTransform(2, 0, 0, 2, 0, -124); drawOrb(ctx, 208, 208, sound, { t: 1.2, level: 0.62, dry: false, motion: false }); });
  return cv;
}
function sceneCard(o) {
  const cur = S.preset.name === o.name && !o.concept;
  const card = el('article', { class: 'card scard' + (o.concept ? ' concept' : '') + (cur ? ' current' : ''), 'data-testid': 'scene-' + o.name, 'aria-label': o.name + (o.concept ? ', concept placeholder' : '') },
    o.concept ? el('div', { class: 'concept-art' }, 'No sound defined') : sceneThumb(o.sound),
    el('span', { class: 'sn' }, o.name),
    el('div', null, el('span', { class: 'badge ' + o.badgeCls }, o.badge), cur ? el('span', { class: 'badge', style: { marginLeft: '4px' } }, 'LOADED') : null),
    el('div', { class: 'st' }, o.tag));
  if (o.concept) card.append(tip(el('button', { class: 'btn small', disabled: true }, 'Not a preset yet'), 'Concept name only. It needs sound design and a listening test before it can become a factory preset.'));
  else card.append(el('div', { class: 'sa' },
    tip(el('button', { class: 'btn small', 'data-key': 'pv-' + o.name, 'data-testid': 'preview-' + o.name, onclick: () => previewScene(o.name, o.sound) }, 'Preview'), 'Hear it without loading it. Your sound comes back when you leave the preview.'),
    tip(el('button', { class: 'btn small primary', 'data-key': 'ld-' + o.name, 'data-testid': 'load-' + o.name, onclick: o.load }, 'Load'), 'Load it (Undo goes back).'),
    o.del ? tip(el('button', { class: 'btn small ghost', 'data-key': 'del-' + o.name, 'data-testid': 'delete-' + o.name, 'aria-label': 'Delete ' + o.name, onclick: o.del }, 'Delete…'), 'Delete this saved sound (asks first; can’t be undone).') : null));
  return card;
}
function viewScenes() {
  const f = S.view.filter;
  const filt = el('div', { class: 'sc-filter seg', role: 'radiogroup', 'aria-label': 'Show' }, [['all', 'All'], ['factory', 'Factory'], ['yours', 'Yours'], ['concepts', 'Concepts']].map(([k, t]) =>
    el('button', { role: 'radio', 'data-key': 'filt-' + k, 'data-testid': 'filter-' + k, 'aria-checked': f === k ? 'true' : 'false', onclick: () => { S.view.filter = k; changed(true); } }, t)));
  const out = el('div', { class: 'screen scroll', 'data-testid': 'screen-scenes', style: S.preview ? { paddingTop: '64px' } : null },
    el('div', { class: 'ws-head' }, el('div', null, el('h1', { class: 'h1' }, 'SCENES — sounds as places'), el('div', { class: 'ws-line' }, 'Preview anything without losing your sound. Factory sounds set EQ, compression, saturation, echo, Dimension and Doubler; your tuning, gain, mix, CLEAN, AIR LAB and limiter stay as they are.')), filt));
  if (f === 'all' || f === 'factory') {
    out.append(el('div', { class: 'sc-section' }, el('h2', { class: 'h2' }, 'FACTORY'), el('span', { class: 'dim' }, 'In the plug-in today: 8 starting profiles and Factory Default')));
    out.append(el('div', { class: 'sc-grid' }, sceneCard({ name: 'Factory Default', tag: 'Everything at its default: your vocal passes through unchanged.', sound: factoryDefault(), badge: 'FACTORY', badgeCls: 'factory', load: loadDefault }),
      PROFILES.map((p, i) => sceneCard({ name: p.name, tag: p.tag, sound: profileSound(i, S.sound), badge: 'FACTORY', badgeCls: 'factory', load: () => loadProfile(i) }))));
  }
  if (f === 'all' || f === 'yours') {
    out.append(el('div', { class: 'sc-section' }, el('h2', { class: 'h2' }, 'YOURS'), el('span', { class: 'dim' }, 'Saved on this computer, available in every project')));
    out.append(!S.lib.length ? el('div', { class: 'empty', 'data-testid': 'yours-empty' }, 'Nothing saved yet. When a sound feels right, press Save in the top bar. It will appear here.')
      : el('div', { class: 'sc-grid' }, S.lib.map((p, i) => sceneCard({ name: p.name, tag: 'Your saved sound', sound: p.sound, badge: 'YOURS', badgeCls: 'user', load: () => loadUser(i), del: () => openModal({ kind: 'delete', index: i }) }))));
  }
  if (f === 'all' || f === 'concepts') {
    out.append(el('div', { class: 'sc-section' }, el('h2', { class: 'h2' }, 'CONCEPTS'), el('span', { class: 'badge concept' }, 'PLACEHOLDERS'), el('span', { class: 'dim' }, 'Names only: no sound has been designed or listening-tested')));
    out.append(el('div', { class: 'sc-grid' }, CONCEPTS.map(([n, t]) => sceneCard({ name: n, tag: t, concept: true, badge: 'CONCEPT', badgeCls: 'concept' }))));
  }
  return out;
}
function previewBar() {
  return el('div', { class: 'preview-bar', role: 'status', 'data-testid': 'preview-bar' }, el('span', { class: 'badge', style: { borderColor: 'var(--pink)', color: '#ffd6ee' } }, 'PREVIEW'),
    el('span', { class: 'pv-txt' }, 'You are hearing ', el('b', null, S.preview.name), '. Nothing is loaded; your sound is safe.'),
    el('button', { class: 'btn small', 'data-key': 'pv-back', 'data-testid': 'preview-back', onclick: () => endPreview(false) }, 'Back to my sound'),
    el('button', { class: 'btn small primary', 'data-key': 'pv-keep', 'data-testid': 'preview-keep', onclick: () => endPreview(true) }, 'Load it'));
}

// ---------------------------------------------------------------- the director drawer (KID and Build My Mix)
let drawerEl = null, followBuilt = false;
function renderDrawer() {
  if (!S.drawer) { if (drawerEl) { drawerEl.remove(); drawerEl = null; } return; }
  if (!drawerEl || drawerEl.dataset.mode !== S.drawer) {
    if (drawerEl) drawerEl.remove();
    drawerEl = el('section', { class: 'drawer', id: 'gk-drawer', 'data-mode': S.drawer, 'data-testid': 'drawer-' + S.drawer, role: 'complementary', 'aria-label': S.drawer === 'kid' ? 'KID' : 'Build My Mix' },
      el('div', { class: 'dr-head' }), el('div', { class: 'dr-body', 'aria-live': 'polite', 'data-testid': 'drawer-body' }), el('div', { class: 'dr-follow' }), el('div', { class: 'dr-foot' }));
    followBuilt = false;
    mainEl.append(drawerEl);
  }
  const [head, body, follow, foot] = drawerEl.children;
  const st = body.scrollTop;
  const jump = S.drawer === 'kid' ? renderKid(head, body, follow, foot) : renderBuild(head, body, follow, foot);
  body.scrollTop = jump != null ? jump : st;
}
function scrollKey(key, body) {                      // something new: show the newest exchange (your words, then KID's answer) from its start
  if (drawerEl.dataset.seen === key) return null;
  drawerEl.dataset.seen = key;
  const bs = [...body.querySelectorAll('.bubble')];
  if (!bs.length) return 0;
  const last = bs[bs.length - 1], prev = bs[bs.length - 2];
  const target = prev && prev.classList.contains('you') && !last.classList.contains('you') ? prev : last;
  return Math.max(0, target.offsetTop - 8);
}
function phasePill() {
  const pr = S.kid.prop;
  const [txt, cls] = S.kid.phase === 'proposal' && pr && pr.listen === 'proposal' ? ['Auditioning the proposal', 'rec'] : PHASES[S.kid.phase];
  return el('span', { class: 'phase ' + cls, 'data-testid': 'kid-phase', 'data-phase': S.kid.phase }, el('span', { class: 'pd' }), txt);
}
function partsNodes(parts) { return parts.map((p) => (typeof p === 'string' ? p : p.b ? el('b', null, p.b) : p.badge ? el('span', { class: 'badge concept' }, p.badge) : '')); }
function renderKid(head, body, follow, foot) {
  head.replaceChildren(el('span', { class: 'dr-title' }, 'KID'), phasePill(), el('span', { class: 'dr-sp' }), tip(el('span', { class: 'badge concept' }, 'CONCEPT'), 'Speech and interpretation are simulated in this prototype.'),
    tip(el('button', { class: 'btn icon ghost', 'data-key': 'dr-close', 'data-testid': 'drawer-close', 'aria-label': 'Close KID (a waiting proposal is kept; nothing is applied)', onclick: closeDrawer }, icon('close', 18)), 'Close. A waiting proposal is kept for later; nothing is applied.'));
  const msgs = [];
  if (!S.kid.log.length) msgs.push(el('div', { class: 'bubble kid' }, el('b', null, 'Tell me what you want to hear.'), ' Hold the mic and speak, or type. I propose changes to GlitchKid’s own controls, show you every one with its reason, and wait. Nothing changes until you apply.'));
  for (const m of S.kid.log) {
    if (m.who === 'you') msgs.push(el('div', { class: 'bubble you' }, el('span', { class: 'via' }, m.via === 'voice' ? 'You said (transcribed on this computer)' : 'You typed'), m.text));
    else msgs.push(el('div', { class: 'bubble kid' + (m.cls === 'err' ? ' err' : '') }, partsNodes(m.parts)));
  }
  const ph = S.kid.phase;
  if (ph === 'recording') msgs.push(el('div', { class: 'bubble kid', 'data-testid': 'recording' }, el('div', { class: 'cmdlevel', 'aria-hidden': 'true' }, [0, 1, 2, 3, 4, 5, 6, 7].map(() => el('i'))), 'Command mic open (separate from your vocal). Let go to send.'));
  else if (['transcribing', 'heard', 'interpreting', 'preparing'].includes(ph)) msgs.push(el('div', { class: 'bubble kid' }, PHASES[ph][0] + '…'));
  if (ph === 'unavailable' && S.sim.speech === 'missing' && kidAvailable().ok) msgs.push(el('button', { class: 'btn small', 'data-key': 'setup-speech', 'data-testid': 'setup-speech', onclick: () => openModal({ kind: 'speech' }) }, 'Set up speech…'));
  if (S.kid.prop) msgs.push(proposalView(S.kid.prop));
  body.replaceChildren(...msgs);
  if (!followBuilt) {                              // built once per drawer, so typing is never interrupted
    const input = el('input', { class: 'field', 'data-key': 'kid-follow', 'data-testid': 'kid-follow', placeholder: 'Follow up: “a little less”, “keep it upfront”…', 'aria-label': 'Type a follow-up to KID', maxlength: 160 });
    const send = () => { const v = input.value; input.value = ''; kidCommand(v, 'typed'); };
    input.addEventListener('keydown', (e) => { if (e.key === 'Enter') send(); else if (e.key === 'Escape') { input.blur(); closeDrawer(); } e.stopPropagation(); });
    const micS = el('button', { class: 'mic small', 'data-key': 'mic-small', 'data-testid': 'mic-small', 'aria-label': 'Hold to talk to KID' }, el('span', { class: 'mic-in' }, icon('mic')));
    wireMic(micS);
    follow.replaceChildren(el('div', { class: 'fr' }, micS, input, el('button', { class: 'btn small', 'data-key': 'kid-follow-send', onclick: send }, 'Send')),
      el('div', { class: 'chips', 'data-testid': 'follow-chips' }, PHRASES.follow.map((p) => el('button', { class: 'chip', 'data-key': 'fchip-' + p, 'data-testid': 'chip-' + p, onclick: () => kidCommand(p, 'typed') }, p))));
    followBuilt = true;
  }
  const jump = scrollKey('kid:' + S.kid.log.length + ':' + (S.kid.prop ? S.kid.prop.rev : 0) + ':' + ph, body);
  const busy = S.kid.busy || ['recording', 'transcribing', 'heard', 'interpreting', 'preparing'].includes(ph);
  if (S.kid.prop) {
    const pr = S.kid.prop, n = selectedCount(pr);
    foot.replaceChildren(el('div', { class: 'ff' }, el('span', { class: 'micro' }, 'Listen'), listenToggle(pr), el('span', { class: 'sp' }), el('span', { class: 'dr-note' }, 'Audition before you apply')),
      el('div', { class: 'ff' }, tip(el('button', { class: 'btn', 'data-key': 'prop-cancel', 'data-testid': 'prop-cancel', disabled: busy, onclick: cancelProposal }, 'Cancel'), 'Discard the proposal. Your sound stays exactly as it is.'), el('span', { class: 'sp' }),
        el('button', { class: 'btn primary', 'data-key': 'prop-apply', 'data-testid': 'prop-apply', disabled: busy || !n, onclick: applyProposal }, 'Apply ' + n + ' change' + (n === 1 ? '' : 's'))),
      el('div', { class: 'dr-note' }, 'Nothing changes until Apply. × keeps it for later; Cancel discards it.'));
  } else if (ph === 'applied') {
    foot.replaceChildren(el('div', { class: 'ff' }, el('span', { class: 'dr-note', style: { flex: 1 } }, 'Applied ' + S.kid.lastApplied + ' change' + (S.kid.lastApplied === 1 ? '' : 's') + '.'), el('button', { class: 'btn', 'data-key': 'kid-undo', 'data-testid': 'kid-undo', onclick: undo }, icon('undo', 16), 'Undo'), el('button', { class: 'btn primary', 'data-key': 'kid-done', 'data-testid': 'kid-done', onclick: closeDrawer }, 'Done')));
  } else {
    foot.replaceChildren(el('div', { class: 'dr-note' }, S.sim.net === 'offline' ? 'You’re offline. That’s fine: speech and KID run entirely on this computer.' : 'KID only listens while you hold the mic. Speech is transcribed on this computer, then discarded.'));
  }
  return jump;
}
function listenToggle(pr) {
  return el('div', { class: 'listen', role: 'group', 'aria-label': 'Listen to' },
    tip(el('button', { 'data-key': 'listen-cur', 'data-testid': 'listen-current', 'aria-pressed': pr.listen === 'current' ? 'true' : 'false', onclick: () => { pr.listen = 'current'; changed(); } }, 'Current'), 'Hear your sound as it is now.'),
    tip(el('button', { class: 'p', 'data-key': 'listen-prop', 'data-testid': 'listen-proposal', 'aria-pressed': pr.listen === 'proposal' ? 'true' : 'false', onclick: () => { pr.listen = 'proposal'; changed(); } }, 'Proposal'), 'Audition: hear the ticked changes without applying them. Nothing is saved.'));
}
function proposalView(pr) {
  const n = selectedCount(pr), total = pr.rows.length;
  const box = el('div', { class: 'prop', 'data-testid': 'proposal', 'data-rev': pr.rev },
    el('div', { class: 'prop-head' }, el('b', null, pr.src === 'kid' ? 'Proposal' : 'Your mix, if you apply'), pr.src === 'kid' ? el('span', { class: 'badge', 'data-testid': 'prop-rev' }, 'REVISION ' + pr.rev) : null,
      el('span', { style: { flex: 1 } }),
      el('button', { class: 'btn small ghost', 'data-key': 'tick-all', 'aria-label': 'Tick every change', onclick: () => { pr.rows.forEach((r) => { if (!r.manual) r.sel = true; }); changed(); } }, 'All'),
      el('button', { class: 'btn small ghost', 'data-key': 'tick-none', 'aria-label': 'Untick every change', onclick: () => { pr.rows.forEach((r) => { r.sel = false; }); changed(); } }, 'None'),
      el('span', { class: 'dim', style: { flexBasis: '100%', fontSize: '12.5px' }, 'data-testid': 'prop-count' }, total + ' change' + (total === 1 ? '' : 's') + ' · ' + n + ' ticked · grouped by stage, in processing order')));
  let lastMod = null, group = null;
  for (const r of pr.rows) {
    const m = PM[r.id].mod;
    if (m !== lastMod) { group = el('div', { class: 'prop-group' }, el('div', { class: 'pg-name' }, el('span', null, stageOfId(r.id) + ' · ' + MOD[m].name.toUpperCase()), el('span', { class: 'dim' }, WS[MOD[m].ws].name))); box.append(group); lastMod = m; }
    const tags = [];
    if (r.manual) tags.push(el('span', { class: 'badge warn' }, 'YOU MOVED THIS · YOUR VALUE IS KEPT'));
    if (r.tag) tags.push(el('span', { class: 'badge' }, r.tag.toUpperCase()));
    if (r.limited) tags.push(el('span', { class: 'badge' }, 'AT ITS LIMIT'));
    if (r.sim) tags.push(el('span', { class: 'badge sim' }, 'SIMULATED MEASUREMENT'));
    group.append(el('div', { class: 'prow' + (r.sel && !r.manual ? '' : ' unsel') + (r.manual ? ' manual' : ''), 'data-testid': 'row-' + r.id },
      el('button', { class: 'check', role: 'checkbox', 'data-key': 'chk-' + r.id, 'data-testid': 'chk-' + r.id, 'aria-checked': r.sel && !r.manual ? 'true' : 'false', 'aria-label': 'Apply the change to ' + PM[r.id].name, disabled: r.manual, onclick: () => { r.sel = !r.sel; changed(); } }),
      el('span', { class: 'pn' }, PM[r.id].name), el('span', { class: 'pv' }, fmt(r.id, S.sound[r.id]), ' → ', el('b', null, fmt(r.id, r.to))),
      el('span', { class: 'pr' }, r.why), tags.length ? el('span', { class: 'ptags' }, tags) : null));
  }
  return box;
}
function renderBuild(head, body, follow, foot) {
  const b = S.build;
  follow.replaceChildren(); follow.style.display = 'none';
  const jump = drawerEl.dataset.seen === 'build:' + b.step ? null : (drawerEl.dataset.seen = 'build:' + b.step, 0);
  head.replaceChildren(el('span', { class: 'dr-title' }, 'BUILD MY MIX'), el('span', { class: 'steps', 'aria-label': 'Step ' + b.step + ' of 3', 'data-testid': 'build-step', 'data-step': b.step }, ['1 Listen', '2 Direction', '3 Review'].map((s, i) => el('span', { class: i + 1 === b.step ? 'on' : '' }, s))), el('span', { class: 'dr-sp' }),
    tip(el('button', { class: 'btn icon ghost', 'data-key': 'dr-close', 'data-testid': 'drawer-close', 'aria-label': 'Close Build My Mix (nothing is applied)', onclick: closeDrawer }, icon('close', 18)), 'Close. Nothing is applied.'));
  if (b.err) { body.replaceChildren(el('div', { class: 'banner bad' }, b.err)); foot.replaceChildren(el('div', { class: 'ff' }, el('span', { class: 'sp' }), el('button', { class: 'btn', 'data-key': 'b-close2', onclick: closeDrawer }, 'Close'))); return jump; }
  if (b.step === 1) {
    const items = [['Level', 'peaks around −12 dBFS'], ['Noise floor', 'about −62 dBFS'], ['Tone', 'a little low-mid build-up near 300 Hz'], ['Dynamics', 'loud words about 4 dB above the rest']];
    body.replaceChildren(el('div', { class: 'bubble kid' }, el('b', null, 'First I listen. '), 'Play 10–20 seconds of your vocal. I measure its level, noise and tone, then fit the direction you choose to your voice.'),
      S.sim.signal === 'none' ? el('div', { class: 'banner warn', 'data-testid': 'build-nosignal' }, el('b', null, 'No vocal heard yet. '), 'Press play in FL Studio. Or skip listening and use a direction as designed: it won’t be fitted to your voice, and the review will say so.') : null,
      S.sim.signal === 'clipping' ? el('div', { class: 'banner bad' }, el('b', null, 'Your input is clipping. '), 'I can still listen, but fix the recording level first if you can: clipping is baked into the audio.') : null,
      el('div', { class: 'progress', 'aria-label': 'Listening progress' }, el('i', { style: { width: b.listen + '%' } })),
      el('div', { class: 'checklist' }, items.map(([k, v], i) => el('div', null, b.listen >= (i + 1) * 25 ? el('span', { class: 'ok' }, '✓ ') : '○ ', el('b', null, k), b.listen >= (i + 1) * 25 ? ': ' + v : ''))),
      b.listened ? el('span', { class: 'badge sim', style: { alignSelf: 'flex-start' } }, 'SIMULATED MEASUREMENTS') : null);
    foot.replaceChildren(el('div', { class: 'ff' }, el('button', { class: 'btn', 'data-key': 'b-cancel', 'data-testid': 'build-cancel', onclick: cancelProposal }, 'Cancel'), el('span', { class: 'sp' }),
      S.sim.signal === 'none' ? el('button', { class: 'btn', 'data-key': 'b-skip', 'data-testid': 'build-skip', onclick: () => { b.step = 2; b.listened = false; changed(); } }, 'Skip: use as designed') : null,
      b.listened ? el('button', { class: 'btn primary', 'data-key': 'b-next1', 'data-testid': 'build-next', onclick: () => { b.step = 2; changed(); } }, 'Next: direction') : el('button', { class: 'btn primary', 'data-key': 'b-listen', 'data-testid': 'build-listen', disabled: S.sim.signal === 'none' || b.listening, onclick: buildListen }, b.listening ? 'Listening…' : 'Start listening')));
  } else if (b.step === 2) {
    body.replaceChildren(el('div', { class: 'bubble kid' }, b.listened ? 'I’ve heard enough. ' : 'Not fitted to your voice (you skipped listening). ', el('b', null, 'Pick a direction.'), ' These are GlitchKid’s 8 factory profiles.'),
      el('div', { class: 'pgrid', role: 'radiogroup', 'aria-label': 'Direction' }, PROFILES.map((p, i) => el('button', { class: 'pcard', role: 'radio', 'data-key': 'dir-' + i, 'data-testid': 'dir-' + p.name, 'aria-checked': b.profile === i ? 'true' : 'false', onclick: () => { b.profile = i; changed(); } }, el('b', null, p.name), el('span', null, p.tag)))),
      hslider({ key: 'b-intensity', name: 'Intensity', lo: 'Subtle', hi: 'As designed', local: true, get: () => b.intensity / 100, set: (v) => { b.intensity = Math.round(v * 100); changed(); }, text: () => b.intensity + '%', help: 'Scales echo, Dimension, Doubler and saturation: 100% = as designed, 0% = half.' }));
    foot.replaceChildren(el('div', { class: 'ff' }, el('button', { class: 'btn', 'data-key': 'b-back', onclick: () => { b.step = 1; changed(); } }, 'Back'), el('button', { class: 'btn', 'data-key': 'b-cancel2', 'data-testid': 'build-cancel', onclick: cancelProposal }, 'Cancel'), el('span', { class: 'sp' }),
      el('button', { class: 'btn primary', 'data-key': 'b-review', 'data-testid': 'build-review', onclick: buildProposal }, 'Review changes')));
  } else {
    const pr = b.prop, n = selectedCount(pr);
    body.replaceChildren(el('div', { class: 'bubble kid' }, el('b', null, PROFILES[b.profile].name), b.listened ? ', fitted to your vocal.' : ', as designed (not fitted).', ' Untick anything you want to keep as it is. Your tuning, gain, mix, CLEAN, AIR LAB and limiter are never changed.'), proposalView(pr));
    foot.replaceChildren(el('div', { class: 'ff' }, el('span', { class: 'micro' }, 'Listen'), listenToggle(pr), el('span', { class: 'sp' }), el('button', { class: 'btn small ghost', 'data-key': 'b-back3', onclick: () => { pr.listen = 'current'; b.step = 2; changed(); } }, 'Back')),
      el('div', { class: 'ff' }, el('button', { class: 'btn', 'data-key': 'b-cancel3', 'data-testid': 'build-cancel', onclick: cancelProposal }, 'Cancel'), el('span', { class: 'sp' }), el('button', { class: 'btn primary', 'data-key': 'b-apply', 'data-testid': 'build-apply', disabled: !n, onclick: applyProposal }, 'Apply ' + n + ' change' + (n === 1 ? '' : 's'))),
      el('div', { class: 'dr-note' }, 'Nothing changes until Apply. Cancel or × leaves your sound exactly as it is.'));
  }
  return jump;
}

// ---------------------------------------------------------------- modals and menus
function openModal(m) { S.modal = m; S.menu = null; changed(); }
function closeModal() { S.modal = null; changed(); }
let lastFocus = null, modalSig = '';
function renderModal() {
  const sig = JSON.stringify([S.modal && S.modal.kind, S.modal && S.modal.dl, S.menu]);
  if (!S.modal && !S.menu) { if (modalEl.childElementCount) { modalEl.replaceChildren(); modalSig = ''; if (lastFocus && lastFocus.isConnected) lastFocus.focus({ preventScroll: true }); } return; }
  if (sig === modalSig && modalEl.childElementCount) return;    // keep typing and focus inside an open dialog
  if (!modalEl.childElementCount) lastFocus = document.activeElement;
  modalSig = sig;
  modalEl.replaceChildren();
  if (S.menu) { renderMenu(); return; }
  const m = S.modal;
  const box = el('div', { class: 'modal', role: 'dialog', 'aria-modal': 'true', 'aria-labelledby': 'gk-mt', 'data-testid': 'modal-' + m.kind });
  const layer = el('div', { class: 'modal-layer', onclick: (e) => { if (e.target === layer) closeModal(); } }, box);
  modalEl.append(layer);
  const title = (t) => el('h2', { class: 'mt-title', id: 'gk-mt' }, t);
  const btn = (t, cls, fn, key) => el('button', { class: 'btn ' + (cls || ''), 'data-key': 'm-' + key, 'data-testid': 'm-' + key, onclick: fn }, t);
  if (m.kind === 'save') {
    const input = el('input', { class: 'field', 'data-key': 'save-name', 'data-testid': 'save-name', maxlength: 60, 'aria-label': 'Name', placeholder: 'e.g. Night Drive Lead' });
    input.value = m.name || '';
    const err = el('div', { class: 'err-txt', 'data-testid': 'save-error', 'aria-live': 'polite' });
    const check = () => { const p = nameProblem(input.value); const exists = S.lib.some((x) => x.name.toLowerCase() === input.value.trim().toLowerCase()); err.textContent = p || (exists ? 'A sound called “' + input.value.trim() + '” exists. Saving will ask before replacing it.' : ''); return p; };
    const go2 = () => { if (check()) { input.focus(); return; } const r = saveAs(input.value, false); if (r === 'exists') openModal({ kind: 'overwrite', name: input.value.trim() }); else closeModal(); };
    input.addEventListener('input', check); input.addEventListener('keydown', (e) => { if (e.key === 'Enter') go2(); else if (e.key === 'Escape') closeModal(); e.stopPropagation(); });
    box.append(title('Save this sound'), el('p', null, 'Saved sounds live on this computer and appear in SCENES › Yours in every project. A/B slots are separate: they live inside this project.'), input, err,
      el('div', { class: 'mf' }, btn('Cancel', '', closeModal, 'cancel'), btn('Save', 'primary', go2, 'save')));
  } else if (m.kind === 'overwrite') {
    box.append(title('Replace “' + m.name + '”?'), el('p', null, 'A saved sound with this name already exists. Replacing it overwrites it with the current sound, in every project. This can’t be undone.'),
      el('div', { class: 'mf' }, btn('Cancel', '', () => openModal({ kind: 'save', name: m.name }), 'cancel'), btn('Replace', 'danger', () => { saveAs(m.name, true); closeModal(); }, 'replace')));
  } else if (m.kind === 'update') {
    box.append(title('Update “' + S.preset.name + '”?'), el('p', null, 'Your saved sound will be replaced with what you hear now, in every project. This can’t be undone. To keep both, choose Save as new.'),
      el('div', { class: 'mf' }, btn('Cancel', '', closeModal, 'cancel'), btn('Save as new…', '', () => openModal({ kind: 'save', name: '' }), 'saveasnew'), btn('Replace', 'danger', () => { saveAs(S.preset.name, true); closeModal(); }, 'replace')));
  } else if (m.kind === 'abswitch') {
    box.append(title('Switch to ' + m.to + '?'), el('p', null, 'You’ve changed the sound since you stored it in ' + m.from + '. Those changes are not in ' + m.from + ' yet.'),
      el('div', { class: 'mf' }, btn('Cancel', '', closeModal, 'cancel'), btn('Switch without storing', '', () => { closeModal(); abRecall(m.to); }, 'switch'), btn('Store in ' + m.from + ', then switch', 'primary', () => { closeModal(); abStore(m.from, true); abRecall(m.to); }, 'storeswitch')));
  } else if (m.kind === 'abover') {
    box.append(title('Replace what’s in ' + m.slot + '?'), el('p', null, m.slot + ' holds a different sound. Storing now replaces it with the current sound. This can’t be undone.'),
      el('div', { class: 'mf' }, btn('Cancel', '', closeModal, 'cancel'), btn('Replace ' + m.slot, 'danger', () => { closeModal(); abStore(m.slot, true); }, 'replace')));
  } else if (m.kind === 'delete') {
    const p = S.lib[m.index];
    box.append(title('Delete “' + (p ? p.name : '') + '”?'), el('p', null, 'It disappears from SCENES › Yours in every project. Projects that use it keep their own copy of the sound. This can’t be undone.'),
      el('div', { class: 'mf' }, btn('Cancel', '', closeModal, 'cancel'), btn('Delete', 'danger', () => { const nm = p.name; S.lib.splice(m.index, 1); if (S.preset.type === 'user' && S.preset.name === nm) S.preset.type = 'built'; persistNow(); closeModal(); toast('Deleted “' + nm + '”.'); changed(true); }, 'delete')));
  } else if (m.kind === 'reset-all') {
    box.append(title('Reset everything?'), el('p', null, 'Every control goes back to Factory Default, including your tuning, CLEAN, AIR LAB and limiter. Undo can bring it back.'),
      el('div', { class: 'mf' }, btn('Cancel', '', closeModal, 'cancel'), btn('Reset everything', 'danger', () => { closeModal(); loadDefault(); }, 'reset')));
  } else if (m.kind === 'speech') {
    const dl = m.dl || 0;
    box.append(title('Set up speech'), el('p', null, 'KID’s speech uses whisper.cpp’s ggml-base.en model (about 142 MiB, per whisper.cpp’s model list). It downloads once from huggingface.co, is checked against a SHA-1 hash before it is ever loaded, and then works offline. Your voice is never uploaded.'),
      S.sim.net === 'offline' ? el('div', { class: 'banner warn', 'data-testid': 'speech-offline' }, el('b', null, 'You’re offline. '), 'The one-time download needs internet. Typing to KID works now.') : null,
      dl ? el('div', { class: 'progress' }, el('i', { style: { width: dl + '%' } })) : null,
      el('div', { class: 'mf' }, btn('Not now', '', closeModal, 'cancel'), btn(dl ? (dl >= 100 ? 'Done' : 'Downloading…') : 'Download', 'primary', async () => {
        if (dl >= 100) { closeModal(); return; }
        if (S.sim.net === 'offline' || dl) return;
        for (let i = 1; i <= 10; i++) { await later(150); if (!S.modal || S.modal.kind !== 'speech') return; S.modal.dl = i * 10; changed(); }
        S.sim.speech = 'ready'; renderSim(); kidSay('Speech is set up. Hold the mic and talk.'); if (S.kid.phase === 'unavailable') S.kid.phase = 'ready'; toast('Speech is ready. It works offline from now on.'); changed();
      }, 'download')));
  } else if (m.kind === 'help') {
    box.style.width = '600px';
    box.append(title('Help'), el('div', { class: 'help-grid' },
      el('div', null, el('kbd', null, 'K'), ' hold: talk to KID'), el('div', null, el('kbd', null, 'D'), ': DRY / PROCESSED'),
      el('div', null, el('kbd', null, 'Ctrl/Cmd+Z'), ': Undo'), el('div', null, el('kbd', null, 'Ctrl/Cmd+S'), ': Save'),
      el('div', null, el('kbd', null, 'Esc'), ': close a dialog, KID or a preview'), el('div', null, 'Double-click a knob: back to its default')),
      el('p', null, el('b', null, 'No sound? '), 'GlitchKid must sit on your vocal’s Mixer insert in FL Studio, and the track must play. The IN meter on the right shows what arrives.'),
      el('p', null, el('b', null, 'How KID works. '), 'Speech: transcribed on this computer by whisper.cpp, only while you hold the mic, from a separate command mic, then discarded. Decisions: GlitchKid’s rule engine turns your words into changes to its own controls, each with a reason. Nothing is sent anywhere, and nothing changes until you apply.'),
      el('p', null, el('b', null, 'Order. '), 'SHAPE, CHARACTER and SPACE group the controls; the processing order is fixed (the chain at the bottom, 1 → 17).'),
      el('div', { class: 'mf' }, btn('Reset everything…', 'danger', () => openModal({ kind: 'reset-all' }), 'resetall'), btn('Show the intro again', '', () => { S.modal = null; S.onb = { open: true, step: 0, path: 'build' }; changed(); }, 'intro'), btn('Close', 'primary', closeModal, 'close')));
  }
  box.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') { e.stopPropagation(); closeModal(); return; }
    if (e.key !== 'Tab') return;
    const f = [...box.querySelectorAll('button:not([disabled]), input, select')];
    if (!f.length) return;
    if (e.shiftKey && document.activeElement === f[0]) { e.preventDefault(); f[f.length - 1].focus(); } else if (!e.shiftKey && document.activeElement === f[f.length - 1]) { e.preventDefault(); f[0].focus(); }
  });
  requestAnimationFrame(() => { if (!box.contains(document.activeElement)) (box.querySelector('input') || box.querySelector('.btn.primary') || box.querySelector('button')).focus(); });
}
function openMenu(kind) { S.menu = kind; changed(); }
function renderMenu() {
  const sv = frame.querySelector('[data-key="save"]');
  const r = sv.getBoundingClientRect(), fr = frame.getBoundingClientRect(), k = fr.width / 1120;
  const close = () => { S.menu = null; changed(); };
  const menu = el('div', { class: 'menu', role: 'menu', 'data-testid': 'save-menu', style: { left: ((r.left - fr.left) / k - 70) + 'px', top: ((r.bottom - fr.top) / k + 6) + 'px' } },
    el('button', { role: 'menuitem', 'data-key': 'menu-update', 'data-testid': 'menu-update', onclick: () => openModal({ kind: 'update' }) }, 'Update “' + S.preset.name + '”…'),
    el('button', { role: 'menuitem', 'data-key': 'menu-saveas', 'data-testid': 'menu-saveas', onclick: () => openModal({ kind: 'save', name: '' }) }, 'Save as new…'),
    el('div', { class: 'msep' }),
    el('button', { role: 'menuitem', 'data-key': 'menu-cancel', onclick: close }, 'Cancel'));
  menu.addEventListener('keydown', (e) => { if (e.key === 'Escape') close(); });
  const layer = el('div', { style: { position: 'absolute', inset: '0', zIndex: '75' }, onclick: (e) => { if (e.target === layer) close(); } }, menu);
  modalEl.append(layer);
  requestAnimationFrame(() => menu.querySelector('button').focus());
}

// ---------------------------------------------------------------- first launch
function renderOnboarding() {
  let layer = $('#gk-onb');
  if (!S.onb.open) { if (layer) layer.remove(); return; }
  const sig = JSON.stringify([S.onb.step, S.onb.path, S.sim.signal, S.bypass]);
  if (layer && layer.dataset.sig === sig) return;
  if (layer) layer.remove();
  const st = S.onb.step;
  const box = el('div', { class: 'modal onb', role: 'dialog', 'aria-modal': 'true', 'aria-labelledby': 'gk-onb-t', 'data-testid': 'onb-' + st });
  const dots = el('div', { class: 'dots', 'aria-label': 'Step ' + (st + 1) + ' of 5' }, [0, 1, 2, 3, 4].map((i) => el('i', { class: i <= st ? 'on' : '' })));
  const nav = (back, nextLabel, nextFn) => el('div', { class: 'foot' }, el('button', { class: 'btn ghost', 'data-key': 'onb-skip', 'data-testid': 'onb-skip', onclick: () => finishOnb(true) }, 'Skip intro'), el('span', { class: 'sp' }),
    back ? el('button', { class: 'btn', 'data-key': 'onb-back', 'data-testid': 'onb-back', onclick: () => { S.onb.step--; changed(); } }, 'Back') : null,
    el('button', { class: 'btn primary', 'data-key': 'onb-next', 'data-testid': 'onb-next', onclick: nextFn || (() => { S.onb.step++; changed(); }) }, nextLabel));
  const h = (t) => el('h2', { id: 'gk-onb-t' }, t), lead = (t) => el('p', { class: 'lead' }, t);
  if (st === 0) box.append(dots, h('Welcome to GlitchKid'), lead('One insert for your vocal: clean it, tune it, shape its tone, give it character and put it in a space. You stay in control: nothing changes until you choose.'), nav(false, 'Start'));
  if (st === 1) {
    const ok = S.sim.signal !== 'none';
    box.append(dots, h('Let’s hear your vocal'), lead('Press play in FL Studio, or sing into the armed track. GlitchKid listens on this Mixer insert.'),
      el('div', { class: 'meter-demo', 'data-testid': 'onb-signal' }, el('b', null, 'IN'), el('div', { class: 'mbar' }, el('i', { id: 'onb-bar' })), el('span', { style: { color: ok ? 'var(--good)' : 'var(--text-2)', fontWeight: '700' } }, ok ? '✓ Vocal coming in' : 'Waiting for sound…')),
      ok ? null : el('div', { class: 'banner info' }, el('b', null, 'Nothing? '), 'GlitchKid must be on the vocal’s Mixer insert, and the track must be routed to it. ', el('button', { class: 'btn small', 'data-key': 'onb-sim-play', 'data-testid': 'onb-sim-play', onclick: () => { S.sim.signal = 'vocal'; renderSim(); changed(); } }, 'Prototype: simulate playback')),
      nav(true, ok ? 'Next' : 'Skip this check'));
  }
  if (st === 2) box.append(dots, h('Hear the difference'), lead('DRY is your original vocal, PROCESSED is GlitchKid. They are level-matched, so louder never fools you. The switch is always at the top right.'),
    el('div', { class: 'compare', style: { alignSelf: 'flex-start' } }, el('button', { class: 'dry', 'data-key': 'onb-dry', 'data-testid': 'onb-dry', 'aria-pressed': S.bypass ? 'true' : 'false', onclick: () => setBypass(true) }, 'DRY'), el('button', { 'data-key': 'onb-proc', 'data-testid': 'onb-proc', 'aria-pressed': S.bypass ? 'false' : 'true', onclick: () => setBypass(false) }, 'PROCESSED')),
    el('p', { class: 'dim', style: { fontSize: '13px', margin: '0' } }, 'With nothing loaded yet, both sound the same: a fresh GlitchKid passes your vocal through untouched.'), nav(true, 'Next'));
  if (st === 3) box.append(dots, h('How do you want to start?'),
    el('div', { class: 'paths', role: 'radiogroup', 'aria-label': 'Starting path' },
      el('button', { class: 'path', role: 'radio', 'data-key': 'onb-p-build', 'data-testid': 'onb-path-build', 'aria-checked': S.onb.path === 'build' ? 'true' : 'false', onclick: () => { S.onb.path = 'build'; changed(); } }, el('b', null, 'Help me build my vocal'), el('span', null, 'GlitchKid listens, you pick a direction, and you review every change before it’s applied.')),
      el('button', { class: 'path', role: 'radio', 'data-key': 'onb-p-pro', 'data-testid': 'onb-path-pro', 'aria-checked': S.onb.path === 'pro' ? 'true' : 'false', onclick: () => { S.onb.path = 'pro'; changed(); } }, el('b', null, 'I know what I’m doing'), el('span', null, 'Straight to the controls. Every parameter is in SHAPE, CHARACTER and SPACE.'))),
    nav(true, 'Next'));
  if (st === 4) box.append(dots, h('Three workspaces, one director'),
    el('div', { class: 'ws3' }, el('div', null, el('b', null, 'SHAPE'), 'Clean, tune, tone, dynamics: make it work.'), el('div', null, el('b', null, 'CHARACTER'), 'Formant, harmony, mutation, stutter: make it yours.'), el('div', null, el('b', null, 'SPACE'), 'Doubler, echo, Dimension, output: put it in its world.')),
    el('div', { class: 'banner info', 'data-testid': 'onb-privacy' }, el('b', null, 'KID listens only while you hold its button. '), 'Your voice command comes from a separate microphone, never your vocal track. It is transcribed on this computer, then discarded. Nothing is uploaded, and KID changes nothing until you apply.'),
    nav(true, S.onb.path === 'build' ? 'Finish: build my vocal' : 'Finish', () => finishOnb(false)));
  box.addEventListener('keydown', (e) => { if (e.key === 'Escape') { e.stopPropagation(); finishOnb(true); } });
  frame.append(el('div', { class: 'modal-layer', id: 'gk-onb', 'data-sig': sig }, box));
  requestAnimationFrame(() => { const n = box.querySelector('[data-key="onb-next"]'); if (n && !box.contains(document.activeElement)) n.focus({ preventScroll: true }); });
}
function finishOnb(skipped) {
  S.onb.open = false; S.onbDone = true; persistNow();
  if (!skipped && S.onb.path === 'build') startBuild();
  else if (!skipped) { S.view.ws = 'shape'; toast('Every control lives in SHAPE, CHARACTER and SPACE. KID is at the bottom of the rail.'); }
  changed(true);
}

// ---------------------------------------------------------------- prototype controls (outside the plug-in)
function renderSim() {
  const grp = (label, key, opts, after) => el('div', { class: 'grp' }, el('span', null, label), ...opts.map(([v, t]) => el('button', { 'data-sim': key + '-' + v, 'aria-pressed': S.sim[key] === v ? 'true' : 'false', onclick: () => { S.sim[key] = v; if (after) after(v); renderSim(); changed(true); } }, t)));
  $('#sim').replaceChildren(
    grp('Vocal input', 'signal', [['none', 'None'], ['vocal', 'Vocal'], ['clipping', 'Clipping']]),
    grp('Speech', 'speech', [['ready', 'Ready'], ['missing', 'Not set up'], ['denied', 'Mic blocked']], () => { if (S.kid.phase === 'unavailable') S.kid.phase = 'ready'; }),
    grp('Network', 'net', [['online', 'Online'], ['offline', 'Offline']]),
    grp('Engine', 'engine', [['ok', 'Normal'], ['mastering', 'Mastering mode on'], ['error', 'Not processing']], () => { if (S.kid.phase === 'unavailable') S.kid.phase = 'ready'; }),
    grp('Window', 'size', [['fit', 'Fit'], ['80', '80%'], ['100', '100%'], ['125', '125%']], applySize),
    grp('Motion', 'motion', [[true, 'On'], [false, 'Off']], () => document.body.classList.toggle('reduce-motion', !S.sim.motion)),
    el('div', { class: 'grp' }, el('span', null, 'KID hears'), (() => {
      const s = el('select', { 'data-testid': 'phrase-big', 'aria-label': 'Simulated speech: what KID hears when you hold the mic' },
        el('optgroup', { label: 'First commands' }, PHRASES.first.map((p, i) => el('option', { value: i }, p))),
        el('optgroup', { label: 'Follow-ups' }, PHRASES.follow.map((p, i) => el('option', { value: PHRASES.first.length + i }, p))));
      s.value = String(S.kid.phrase);
      s.addEventListener('change', () => { S.kid.phrase = +s.value; });
      return s;
    })()),
    el('div', { class: 'grp' }, el('button', { 'data-sim': 'intro', onclick: () => { S.onb = { open: true, step: 0, path: 'build' }; changed(); } }, 'Restart intro'),
      el('button', { 'data-sim': 'reopen', onclick: () => { persistNow(); location.reload(); } }, 'Close & reopen project'),
      el('button', { 'data-sim': 'wipe', onclick: () => { store.del(LS_SESSION); store.del(LS_LIB); location.reload(); } }, 'Reset prototype data')));
}
function applySize() {
  let k = { 80: 0.8, 100: 1, 125: 1.25 }[S.sim.size];
  if (S.sim.size === 'fit') k = Math.min(1.25, Math.max(0.3, (window.innerWidth - 40) / 1120));
  $('#frame').style.transform = 'scale(' + k + ')';
  $('#frameWrap').style.width = 1120 * k + 'px'; $('#frameWrap').style.height = 680 * k + 'px';
}
window.addEventListener('resize', () => { if (S.sim.size === 'fit') applySize(); });

// ---------------------------------------------------------------- keyboard
document.addEventListener('keydown', (e) => {
  const tag = e.target && e.target.tagName;
  if (tag === 'INPUT' || tag === 'SELECT' || tag === 'TEXTAREA') return;
  if ((e.metaKey || e.ctrlKey) && e.key.toLowerCase() === 'z') { e.preventDefault(); undo(); return; }
  if ((e.metaKey || e.ctrlKey) && e.key.toLowerCase() === 's') { e.preventDefault(); saveClicked(); return; }
  if (S.modal || S.onb.open || e.metaKey || e.ctrlKey || e.altKey) return;
  if (e.key === 'Escape') { if (S.menu) { S.menu = null; changed(); } else if (S.preview) endPreview(false); else if (S.drawer) closeDrawer(); return; }
  if (e.key.toLowerCase() === 'd' && !e.repeat) { setBypass(!S.bypass); return; }
  if (e.key.toLowerCase() === 'k' && !e.repeat) micDown();
});
document.addEventListener('keyup', (e) => { if (e.key.toLowerCase() === 'k') micUp(); });

// ---------------------------------------------------------------- start
renderShell(); renderSim(); applySize();
document.body.classList.toggle('reduce-motion', !S.sim.motion);
renderMain(); update();
requestAnimationFrame(loop);
window.GK = { setSize: (sz) => { S.sim.size = sz; applySize(); renderSim(); }, S, PARAMS, PM, PROFILES, USER_OWNED, KID_NEVER, CHAIN, pace, phaseLog, parseCommand, factoryDefault, profileSound, soundEq, heard, proposalTarget, isModified, abModified, modOn, fmt, persistNow };
})();
