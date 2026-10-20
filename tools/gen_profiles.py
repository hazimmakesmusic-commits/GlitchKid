#!/usr/bin/env python3
"""Generates Source/Presets/Profiles.inc: the eight starting profiles of GlitchKid.

Each profile is a list of overrides on the Factory Default sound, written only with existing parameter IDs and only for the processing that matters for a
vocal mix: EQ, compressor, saturation, echo, Dimension (width) and the Doubler. Pitch Tune is NEVER part of a profile: on/off, strength, speed, key and scale
belong to the user (the TUNE screen). Run from the repository root:
    python3 tools/gen_profiles.py [tools/profile_makeup.json]
The optional JSON file holds per-profile makeup corrections (dB) measured by the test suite (PROFILELEVEL lines; see tools/calibrate_profiles.py), so each profile
comes out near the level of the input.
"""
import json, sys, os

DIVS = ["1/2", "1/4", "1/4 dotted", "1/4 triplet", "1/8", "1/8 dotted", "1/8 triplet", "1/16", "1/16 dotted", "1/16 triplet", "1/32"]

# eq: lo=(gain,freq) b1=(freq,gain) b2=(freq,gain) hi=(freq,gain)   comp=(thresh, ratio, attack, release, knee)
# echo=(division, feedback, tone, width, mix)   dim=(width, detune, depth, mix)   dbl=(amount, width, detune, timing)
PROFILES = [
 dict(name="CLEAN & NATURAL", tagline="Transparent, honest, lightly polished",
      description="A clean, natural vocal: gentle levelling and a touch of air, with no effects on the voice.",
      start="Good for a vocal that already sounds right. Pair with NATURAL tuning or none.",
      eq=dict(b1=(300, -1.0), b2=(3500, 1.0), hi=(11000, 1.0)), comp=(-18, 2.5, 15, 140, 10), sat=0),
 dict(name="MODERN MELODIC RAP", tagline="Glossy, smooth, a little space",
      description="The modern melodic-rap polish: smooth compression, presence and air, a light echo and a touch of width.",
      start="Works for sung or rapped melodies. Try it with LIGHT tuning.",
      eq=dict(lo=(1.0, 100), b1=(350, -1.5), b2=(4000, 2.0), hi=(10000, 2.0)), comp=(-19, 3.0, 10, 110, 8), sat=2.5,
      echo=(5, 22, 4500, 40, 14), dim=(50, 5, 45, 22), dbl=(20, 70, 6, 14)),
 dict(name="HARD-TUNED RAP", tagline="Tight, forward, built for hard tuning",
      description="A tight, forward tone with firm compression and a short echo: it stays out of the way of fast, hard-tuned vocals.",
      start="Set TUNE to HARD and choose your key and scale first: this profile does not switch tuning on.",
      eq=dict(lo=(-1.0, 100), b1=(300, -2.0), b2=(3200, 2.5), hi=(9500, 2.0)), comp=(-21, 4.0, 5, 80, 4), sat=3,
      echo=(7, 12, 6000, 20, 8), dbl=(15, 60, 4, 10)),
 dict(name="DARK & ATMOSPHERIC", tagline="Deep, wide, moody",
      description="A dark, spacious vocal: a rolled-off top, a long dotted echo, width and a soft double.",
      start="Best on a close, dry vocal. Ease ECHO MIX down if the words get blurry.",
      eq=dict(lo=(2.0, 90), b1=(300, -2.5), b2=(2500, -0.5), hi=(6500, -3.5)), comp=(-20, 3.0, 14, 160, 8), sat=3.5,
      echo=(2, 40, 2200, 75, 28), dim=(65, 8, 55, 35), dbl=(35, 85, 8, 20)),
 dict(name="BRIGHT & PRESENT", tagline="Up front, clear words, airy top",
      description="A forward, clear vocal: a lighter low end, strong presence and air, firm compression and almost no ambience.",
      start="Good for a dull or buried vocal. If it gets harsh, lower HIGH or PRESENCE in MODIFY.",
      eq=dict(lo=(-1.5, 120), b1=(400, -1.5), b2=(4000, 3.0), hi=(11000, 3.0)), comp=(-20, 3.5, 6, 90, 5), sat=1.5,
      echo=(7, 10, 7000, 20, 7), dbl=(12, 50, 4, 12)),
 dict(name="SMOOTH R&B", tagline="Warm, soft, gently wide",
      description="A warm, soft vocal: slow gentle compression, a warm low end, silky top and a soft quarter-note echo with width.",
      start="Suits sung vocals. Pair with NATURAL or LIGHT tuning.",
      eq=dict(lo=(1.5, 120), b1=(300, -1.0), b2=(3500, 1.0), hi=(9500, 1.5)), comp=(-17, 2.5, 18, 180, 12), sat=1.2,
      echo=(1, 28, 3500, 45, 18), dim=(45, 5, 40, 22), dbl=(25, 75, 6, 16)),
 dict(name="GRITTY RAP", tagline="Raw, dense, in your face",
      description="A dense, gritty vocal: hard, fast compression, real saturation and a doubled edge, with no echo.",
      start="Best on a loud, dry rap. Lower SATURATION in MODIFY if it feels harsh.",
      eq=dict(lo=(2.0, 100), b1=(450, -1.0), b2=(2800, 2.5), hi=(8000, 0.5)), comp=(-24, 5.0, 4, 70, 3), sat=7,
      dbl=(25, 70, 9, 10)),
 dict(name="SPACIOUS LEAD", tagline="Big, open, a lead that floats",
      description="A big, open lead vocal: balanced tone, a clear eighth-note echo, wide Dimension and a soft double.",
      start="Good for a hook or a sung lead. Keep ECHO MIX moderate in a busy beat.",
      eq=dict(lo=(0.5, 100), b1=(350, -1.5), b2=(3500, 1.5), hi=(9500, 1.5)), comp=(-18, 2.8, 12, 150, 9), sat=1.8,
      echo=(4, 32, 4000, 60, 24), dim=(70, 6, 60, 38), dbl=(30, 90, 7, 18)),
]

def f(x):
    s = ("%.4f" % x).rstrip("0").rstrip(".")
    s = s if s not in ("", "-0") else "0"
    return s + (".0f" if "." not in s else "f")

def build(p, corr):
    o = []
    eq = p["eq"]
    if "lo" in eq:
        o += [("eq_lo_gain", eq["lo"][0]), ("eq_lo_freq", eq["lo"][1])]
    if "b1" in eq:
        o += [("eq_b1_freq", eq["b1"][0]), ("eq_b1_gain", eq["b1"][1])]
    if "b2" in eq:
        o += [("eq_b2_freq", eq["b2"][0]), ("eq_b2_gain", eq["b2"][1])]
    if "hi" in eq:
        o += [("eq_hi_freq", eq["hi"][0]), ("eq_hi_gain", eq["hi"][1])]
    c = p["comp"]
    makeup = max(0.0, min(12.0, (-c[0]) * (1 - 1 / c[1]) * 0.35)) + corr
    makeup = max(0.0, min(24.0, round(makeup * 2) / 2))
    o += [("comp_thresh", c[0]), ("comp_ratio", c[1]), ("comp_attack", c[2]), ("comp_release", c[3]), ("comp_knee", c[4]), ("comp_makeup", makeup)]
    if p["sat"]:
        o.append(("sat_drive", p["sat"]))
    if p.get("echo"):
        e = p["echo"]
        o += [("echo_on", 1), ("echo_sync", 1), ("echo_div", e[0]), ("echo_fb", e[1]), ("echo_tone", e[2]), ("echo_width", e[3]), ("echo_mix", e[4])]
    if p.get("dim"):
        d = p["dim"]
        o += [("dim_on", 1), ("dim_width", d[0]), ("dim_detune", d[1]), ("dim_depth", d[2]), ("dim_mix", d[3])]
    if p.get("dbl"):
        d = p["dbl"]
        o += [("dbl_on", 1), ("dbl_amount", d[0]), ("dbl_width", d[1]), ("dbl_detune", d[2]), ("dbl_timing", d[3])]
    return o

def settings_text(o):
    d = dict(o)
    parts = ["%.1f:1 compression" % d["comp_ratio"]]
    if "sat_drive" in d:
        parts.append("%.1f dB saturation" % d["sat_drive"])
    parts.append("EQ tone shaping")
    if "echo_on" in d:
        parts.append("%s echo %d%%" % (DIVS[int(d["echo_div"])], d["echo_mix"]))
    if "dim_on" in d:
        parts.append("Dimension width")
    if "dbl_on" in d:
        parts.append("Doubler %d%%" % d["dbl_amount"])
    return ", ".join(parts) + ". Pitch Tune is left to you."

def main():
    corr = json.load(open(sys.argv[1])) if len(sys.argv) > 1 and os.path.exists(sys.argv[1]) else {}
    out = ["// GENERATED by tools/gen_profiles.py: do not edit by hand. Eight starting profiles as overrides on the Factory Default sound.\n",
           "// Original starting points. Pitch Tune (on, strength, speed, key, scale) is never part of a profile.\n\n"]
    ents = []
    for i, p in enumerate(PROFILES):
        o = build(p, corr.get(p["name"], 0.0))
        ids = [k for k, _ in o]
        assert len(ids) == len(set(ids)), p["name"]
        out.append("const Override profile%d[] = {\n    " % i + ", ".join('{ "%s", %s }' % (k, f(float(v))) for k, v in o) + " };\n")
        ents.append('    { { "%s", "%s", "%s", "%s", "%s" }, GK_ENTRY (profile%d) }' % (p["name"], p["tagline"], p["description"], settings_text(o), p["start"], i))
    out.append("\n#define GK_ENTRY(arr) arr, sizeof (arr) / sizeof (arr[0])\nconst Entry entries[count] = {\n" + ",\n".join(ents) + " };\n#undef GK_ENTRY\n")
    open("Source/Presets/Profiles.inc", "w").write("".join(out))
    print("wrote", len(PROFILES), "profiles")

main()
