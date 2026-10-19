#!/usr/bin/env python3
"""Generates Source/Presets/ArtistPresets.inc, the 50 artist-inspired factory presets.

Every preset is a list of overrides on the Factory Default sound, written only with existing parameter IDs. The artist names are reference
labels, nothing more: the settings are original starting points described by what they do to a vocal. Run from the repository root:
    python3 tools/gen_artist_presets.py [tools/artist_makeup.json]
The optional JSON file holds per-preset makeup corrections (dB) measured by the test suite (see Tests/ArtistPresetTests.cpp), so the presets
come out near the level of the input instead of louder or quieter.
"""
import json, sys, os

# tags
ML, HL, TR, DR, RG, RB, EX, PJ, HI = 1, 2, 4, 8, 16, 32, 64, 128, 256
# echo divisions: 0 1/2, 1 1/4, 2 1/4 dotted, 3 1/4 trip, 4 1/8, 5 1/8 dotted, 6 1/8 trip, 7 1/16, 8 1/16 dotted, 9 1/16 trip, 10 1/32

def preset(artist, region, tags, intent, tune=None, formant=None, eq=None, comp=None, sat=0.0, echo=None, dim=None, dbl=None,
           mut=None, harm=None, stutter=None):
    return dict(artist=artist, region=region, tags=tags, intent=intent, tune=tune, formant=formant, eq=eq or {}, comp=comp, sat=sat,
                echo=echo, dim=dim, dbl=dbl, mut=mut, harm=harm, stutter=stutter)

I, D = "INT", "DHH"
# eq: lo=(gain[,freq]) b1=(freq,gain[,q]) b2=(freq,gain) hi=(freq,gain)   comp=(thresh, ratio, attack, release, knee)   tune=(strength, speed)
# echo=(div, feedback, tone, width, mix)  dim=(width, detune, depth, mix)  dbl=(amount, width, detune, timing)  mut=(amount, character, blend)
P = [
 preset("Travis Scott", I, ML|TR, "atmospheric, spacious, layered trap vocals", tune=(85,12), eq=dict(lo=(1.5,100), b1=(400,-2.0), hi=(8000,-3.0)), comp=(-20,3,12,120,8), sat=4, echo=(2,40,2500,70,28), dim=(60,6,50,35), dbl=(35,80,8,18)),
 preset("Juice WRLD", I, ML|RB, "emotional melodic rap, smooth tuning, intimate delays", tune=(70,35), eq=dict(lo=(1.0,100), b1=(350,-1.5), b2=(3500,1.5), hi=(9000,1.0)), comp=(-18,2.5,15,150,10), sat=1.5, echo=(5,25,3500,20,15)),
 preset("Don Toliver", I, ML|RB|TR, "fluid melodic trap, glossy vocal texture", tune=(80,18), eq=dict(lo=(-1.0,100), b2=(4500,2.5), hi=(10000,2.0)), comp=(-19,3,8,90,6), sat=2.5, echo=(4,30,5000,55,22), dim=(55,5,45,30), dbl=(20,70,6,14)),
 preset("The Kid LAROI", I, RB|ML, "modern pop-rap, bright and forward vocals", tune=(60,40), eq=dict(b1=(300,-1.0), b2=(3000,2.0), hi=(10000,2.5)), comp=(-18,3,6,100,6), sat=1.0, echo=(7,15,6000,25,10), dbl=(15,60,5,12)),
 preset("Future", I, TR|ML, "dark trap vocal texture, controlled saturation", tune=(90,8), formant=-1.0, eq=dict(lo=(2.0,120), b1=(250,-2.0), b2=(2500,1.0), hi=(7000,-2.0)), comp=(-22,4,10,140,4), sat=6, echo=(6,30,2200,40,18)),
 preset("Playboi Carti", I, RG|EX|TR, "aggressive, distorted, experimental vocal character", tune=(55,50), formant=2.5, eq=dict(lo=(-3.0,100), b2=(3000,3.0), hi=(8000,4.0)), comp=(-24,6,3,60,2), sat=11, echo=(7,30,4000,60,18), dim=(70,10,60,30), mut=(12,55,35)),
 preset("Lil Uzi Vert", I, ML|TR|RG, "energetic melodic rap, bright tuning character", tune=(95,5), formant=1.5, eq=dict(lo=(-1.0,100), b2=(3500,2.5), hi=(9000,3.5)), comp=(-20,3.5,8,90,5), sat=4, echo=(7,25,7000,45,16), dbl=(30,75,10,16)),
 preset("Yeat", I, RG|EX|TR, "futuristic, saturated rage/trap character", tune=(75,15), formant=2.0, eq=dict(lo=(1.0,100), b1=(600,-2.0), b2=(2800,2.5), hi=(9000,2.0)), comp=(-24,5,4,70,3), sat=9, echo=(9,35,5000,75,20), dim=(65,9,55,35), mut=(8,50,25)),
 preset("Ken Carson", I, RG|TR, "aggressive, synthetic, high-energy trap character", tune=(85,10), formant=1.0, eq=dict(lo=(-2.0,100), b1=(400,-1.0), b2=(3200,2.5), hi=(8500,3.0)), comp=(-23,5,4,80,3), sat=8, echo=(7,22,4500,50,14), dbl=(25,80,9,12), mut=(10,60,30)),
 preset("Destroy Lonely", I, TR|ML|EX, "dark, atmospheric underground trap", tune=(70,25), formant=-1.5, eq=dict(lo=(2.0,90), b1=(300,-2.5), b2=(2500,-1.0), hi=(6000,-4.0)), comp=(-21,3.5,14,160,6), sat=5, echo=(2,45,1800,80,30), dim=(70,8,60,40), dbl=(40,85,8,22)),
 preset("Drake", I, ML|RB, "polished melodic rap/R&B vocal presentation", tune=(55,45), eq=dict(lo=(1.0,110), b1=(250,-1.0), b2=(3500,1.5), hi=(11000,1.5)), comp=(-17,2.5,14,140,10), sat=0.8, echo=(4,18,4500,30,10), dbl=(12,50,4,12)),
 preset("Post Malone", I, ML|RB, "melodic pop-rap with spacious ambience", tune=(50,50), eq=dict(lo=(1.5,120), b1=(400,-1.5), b2=(2800,1.0), hi=(8000,-1.0)), comp=(-19,3,12,130,8), sat=3.5, echo=(2,38,3000,60,24), dim=(50,5,45,28), dbl=(28,75,7,16)),
 preset("Young Thug", I, ML|EX, "elastic, expressive melodic vocal character", tune=(65,20), formant=1.0, eq=dict(b1=(500,1.5), b2=(3000,3.0), hi=(9000,2.0)), comp=(-20,3,6,70,6), sat=3, echo=(6,28,5500,50,16), dim=(55,9,55,25), mut=(6,55,20)),
 preset("Gunna", I, ML|TR, "smooth, controlled melodic trap", tune=(75,22), eq=dict(lo=(1.0,100), b1=(350,-1.5), b2=(3200,1.0), hi=(8500,-1.5)), comp=(-20,3,12,130,8), sat=1.8, echo=(5,20,3500,35,12)),
 preset("Lil Baby", I, TR|ML, "upfront, articulate modern trap vocals", tune=(70,28), eq=dict(lo=(1.5,110), b1=(500,1.0), b2=(3500,2.0), hi=(9000,1.0)), comp=(-21,4,8,100,5), sat=2.5, echo=(4,12,5000,15,8)),
 preset("21 Savage", I, TR|HL, "dry, dark, punchy vocal presentation", eq=dict(lo=(2.0,100), b1=(300,-2.0), b2=(2500,1.5), hi=(7500,-1.0)), comp=(-23,4.5,6,90,3), sat=3),
 preset("Central Cee", I, DR|HL, "clean, forward UK rap/drill presentation", eq=dict(lo=(-2.0,120), b1=(400,-1.0), b2=(3500,3.0), hi=(10000,3.0)), comp=(-22,4,5,80,4), sat=1.2, echo=(7,10,6000,20,7)),
 preset("Dave", I, HL|DR, "clear, focused UK rap vocal presentation", eq=dict(b1=(300,-1.5), b2=(3000,2.5), hi=(9000,1.5)), comp=(-20,3.5,10,120,6), sat=0.6, dbl=(10,40,3,10)),
 preset("A Boogie wit da Hoodie", I, ML|RB, "melodic rap with tuned vocal layers", tune=(85,15), eq=dict(lo=(1.0,100), b1=(350,-1.5), b2=(3800,2.0), hi=(10000,2.5)), comp=(-19,3,8,110,7), sat=2, echo=(4,25,5500,45,18), dbl=(30,80,6,16), harm=(8,-16)),
 preset("Rod Wave", I, ML|RB, "emotional, sustained melodic vocal presentation", tune=(65,30), eq=dict(lo=(1.5,130), b1=(300,-1.0), b2=(3500,1.0), hi=(8000,-1.5)), comp=(-17,2.5,18,180,12), sat=2, echo=(2,35,2800,50,20), dim=(45,5,40,22)),
 preset("Lil Peep", I, ML|EX, "hazy, intimate emo-rap texture", tune=(45,45), formant=-0.5, eq=dict(lo=(1.0,100), b1=(400,-2.0), b2=(2800,-1.0), hi=(6500,-3.5)), comp=(-20,3,16,180,8), sat=6, echo=(2,45,2000,70,28), dim=(60,8,55,32)),
 preset("XXXTentacion", I, RG|EX, "raw, dynamic, emotionally contrasting vocal character", eq=dict(lo=(2.0,100), b1=(450,-1.0), b2=(3000,3.0), hi=(8000,2.0)), comp=(-26,6,2,70,2), sat=10, echo=(7,20,4000,30,10)),
 preset("blackbear", I, RB|ML, "dark pop-rap/R&B texture", tune=(70,30), eq=dict(lo=(1.0,100), b1=(300,-1.5), b2=(3200,1.5), hi=(7500,-2.0)), comp=(-19,3,10,120,7), sat=4.5, echo=(5,28,3000,45,16), dim=(40,6,40,20)),
 preset("Machine Gun Kelly", I, RB|RG, "alternative pop-punk/rap vocal character", tune=(30,80), eq=dict(b1=(800,1.0), b2=(3500,3.0), hi=(9000,2.0)), comp=(-23,5,4,80,3), sat=7, echo=(4,15,4500,25,8), dbl=(35,90,12,20)),
 preset("Kendrick Lamar", I, HL, "clear, dynamic, expressive rap presentation", eq=dict(b1=(250,-1.0), b2=(3000,2.0), hi=(11000,2.0)), comp=(-18,2.2,14,160,10), sat=0.5, dbl=(8,40,3,10)),

 preset("KR$NA", D, HL|HI, "crisp, articulate, punchy technical rap", eq=dict(lo=(-1.0,100), b1=(300,-1.5), b2=(3800,3.0), hi=(10000,2.5)), comp=(-21,4.5,5,70,3), sat=1.5, echo=(7,10,6500,15,6), dbl=(15,55,4,10)),
 preset("DIVINE", D, HL|HI, "gritty, upfront Mumbai street-rap presentation", eq=dict(lo=(2.5,100), b1=(350,-1.0), b2=(2500,2.0), hi=(7500,0.5)), comp=(-24,5,6,80,3), sat=5),
 preset("Seedhe Maut", D, HL|HI, "energetic Delhi underground rap character", eq=dict(lo=(1.0,100), b1=(500,-1.0), b2=(3500,3.5), hi=(9000,1.5)), comp=(-22,4,4,70,4), sat=3, echo=(7,14,5000,25,8), dbl=(22,70,7,12)),
 preset("Raftaar", D, HL|HI|RB, "polished, punchy, high-energy rap", tune=(40,50), eq=dict(lo=(1.5,100), b1=(400,-1.5), b2=(3500,2.5), hi=(10000,2.5)), comp=(-20,4,6,90,5), sat=2.5, echo=(4,15,5500,25,8), dbl=(18,60,5,12)),
 preset("Emiway Bantai", D, HL|HI, "raw, forward, energetic street-rap character", eq=dict(lo=(1.0,100), b1=(600,1.0), b2=(3200,3.0), hi=(9000,1.0)), comp=(-22,3.5,5,80,3), sat=4.5, dbl=(25,70,8,10)),
 preset("Ikka", D, HL|HI|RB, "controlled, full-bodied Hindi rap presentation", tune=(35,60), eq=dict(lo=(2.5,110), b1=(300,-1.5), b2=(2800,1.5), hi=(8000,-0.5)), comp=(-19,3.5,10,120,8), sat=2, echo=(5,15,3500,25,8)),
 preset("Talha Anjum", D, ML|HI|RB, "emotional Urdu/Hinglish rap with atmospheric depth", tune=(60,35), eq=dict(lo=(1.0,100), b1=(350,-1.5), b2=(3500,1.5), hi=(9000,1.0)), comp=(-18,2.8,14,150,10), sat=2, echo=(2,35,3000,60,22), dim=(50,5,45,25)),
 preset("Talhah Yunus", D, ML|HI, "dark, intimate melodic rap presentation", tune=(70,25), formant=-1.0, eq=dict(lo=(1.5,100), b1=(300,-2.0), b2=(2800,0.5), hi=(7000,-2.5)), comp=(-19,3,12,150,8), sat=3, echo=(5,32,2500,55,20), dim=(45,6,45,22)),
 preset("Young Stunners", D, ML|HI|TR, "duo-inspired layered, moody rap textures", tune=(65,25), eq=dict(lo=(1.0,100), b1=(400,-1.5), b2=(3500,1.5), hi=(8500,0.5)), comp=(-20,3,10,120,7), sat=3, echo=(4,28,4000,65,18), dim=(50,6,45,24), dbl=(40,90,9,20)),
 preset("Badshah", D, RB|HI, "polished commercial rap and pop-rap presentation", tune=(40,50), eq=dict(lo=(1.5,100), b1=(400,-1.0), b2=(4000,2.5), hi=(10500,3.0)), comp=(-19,3.5,8,100,6), sat=2.5, echo=(4,15,6000,30,9), dbl=(22,70,6,14)),
 preset("Yo Yo Honey Singh", D, RB|PJ|HI, "bright, punchy commercial hip-hop vocal character", tune=(50,40), eq=dict(lo=(2.0,100), b1=(450,-1.0), b2=(3800,3.0), hi=(10000,3.0)), comp=(-21,4,6,90,4), sat=3.5, echo=(7,18,6000,35,10), dbl=(30,80,8,12)),
 preset("Sidhu Moose Wala", D, PJ|HL, "powerful, forward Punjabi vocal presentation", eq=dict(lo=(2.0,120), b1=(400,-1.0), b2=(3000,2.0), hi=(8500,0.5)), comp=(-22,4,8,100,5), sat=4, echo=(4,12,4500,20,7), dbl=(20,65,6,14)),
 preset("AP Dhillon", D, PJ|ML|RB, "smooth Punjabi melodic vocals with atmospheric space", tune=(60,35), eq=dict(lo=(1.0,100), b1=(300,-1.5), b2=(3500,1.5), hi=(9500,1.5)), comp=(-18,2.8,12,140,10), sat=1.5, echo=(2,32,3500,60,22), dim=(50,5,45,28)),
 preset("Shubh", D, PJ|ML|TR, "dark, restrained Punjabi melodic-rap character", tune=(65,28), formant=-0.8, eq=dict(lo=(1.5,100), b1=(350,-2.0), b2=(2800,-0.5), hi=(7000,-2.5)), comp=(-20,3.2,10,130,6), sat=3.5, echo=(5,25,2800,40,14)),
 preset("Karan Aujla", D, PJ|ML|RB, "modern Punjabi melodic vocal presentation", tune=(55,32), eq=dict(lo=(1.5,100), b1=(400,-1.0), b2=(3600,2.0), hi=(9500,1.5)), comp=(-19,3.2,9,110,7), sat=2.2, echo=(4,22,5000,40,14), dbl=(18,65,5,14)),
 preset("Diljit Dosanjh", D, PJ|RB, "polished, bright Punjabi pop vocal character", tune=(35,55), eq=dict(lo=(1.0,100), b1=(300,-1.0), b2=(4000,2.5), hi=(11000,3.0)), comp=(-18,3,10,110,8), sat=1, echo=(4,14,6500,30,8), dbl=(16,60,4,14)),
 preset("Chaar Diwaari", D, EX|HI|RG, "experimental, chaotic, genre-bending vocal texture", tune=(70,10), formant=3.0, eq=dict(b2=(2500,3.0), hi=(8000,3.0)), comp=(-24,5,3,70,2), sat=8, echo=(9,40,4500,80,24), mut=(25,45,50), stutter=(3,30)),
 preset("Dhanji", D, EX|HL|HI, "raw, unconventional underground rap character", formant=-2.0, eq=dict(lo=(2.0,100), b1=(500,-2.0), b2=(2200,2.5), hi=(7000,1.0)), comp=(-23,4,4,80,3), sat=6, echo=(6,30,3000,45,14), mut=(6,20,25)),
 preset("Yashraj", D, HL|HI|ML, "articulate, expressive contemporary Indian rap", tune=(30,70), eq=dict(b1=(350,-1.5), b2=(3500,2.5), hi=(9500,2.0)), comp=(-20,3.5,10,120,6), sat=1.5, echo=(7,12,5500,25,7), dbl=(12,50,4,12)),
 preset("Naam Sujal", D, HI|HL|RB, "youthful, energetic Hindi rap presentation", tune=(45,45), eq=dict(lo=(-0.5,100), b1=(400,-1.0), b2=(3800,3.0), hi=(10500,3.0)), comp=(-20,3.5,6,80,5), sat=2, echo=(7,15,6000,30,9), dbl=(18,60,6,10)),
 preset("Sikander Kahlon", D, HL|HI, "gritty, dense, underground rap character", eq=dict(lo=(2.5,100), b1=(450,-1.5), b2=(2800,2.0), hi=(7500,-1.5)), comp=(-25,5,5,90,3), sat=7, echo=(7,12,3500,25,8), dbl=(28,75,9,12)),
 preset("Prabh Deep", D, HL|HI|ML|EX, "textured, atmospheric, expressive rap", formant=-1.0, eq=dict(lo=(1.5,100), b1=(350,-1.0), b2=(3000,1.5), hi=(8000,-1.0)), comp=(-19,3,14,150,8), sat=3.5, echo=(2,30,2800,55,18), dim=(40,5,40,20)),
 preset("MC Stan", D, RG|EX|HI, "gritty, bass-heavy, experimental rap character", tune=(60,20), eq=dict(lo=(3.0,80), b1=(300,-2.0), b2=(3000,3.0), hi=(8500,2.0)), comp=(-25,6,4,80,2), sat=10, echo=(7,18,4000,30,10), mut=(5,50,20)),
 preset("King", D, RB|ML|HI, "polished, melodic, pop-rap vocal presentation", tune=(65,25), eq=dict(lo=(1.0,100), b1=(350,-1.5), b2=(3800,2.0), hi=(10500,2.5)), comp=(-18,3,9,110,8), sat=1.5, echo=(4,24,5500,45,16), dim=(45,5,45,24), dbl=(20,70,6,14)),
 preset("Panther", D, HL|HI|TR, "energetic, punchy, contemporary Indian rap character", tune=(50,45), eq=dict(lo=(1.5,100), b1=(400,-1.0), b2=(3500,3.0), hi=(9500,1.5)), comp=(-21,4.5,6,80,4), sat=4, echo=(7,16,5000,25,8), dbl=(24,75,8,10)),
]
assert len(P) == 50

def f(x):
    s = ("%.4f" % x).rstrip("0").rstrip(".")
    s = s if s not in ("", "-0") else "0"
    return s + (".0f" if "." not in s else "f")

def build(p, corr):
    o = []
    if p["tune"]:
        o += [("tune_on", 1), ("tune_strength", p["tune"][0]), ("tune_speed", p["tune"][1])]
    if p["formant"]:
        o.append(("formant_shift", p["formant"]))
    eq = p["eq"]
    for band, ids in (("lo", ("eq_lo_gain", "eq_lo_freq")), ("b1", ("eq_b1_freq", "eq_b1_gain", "eq_b1_q")), ("b2", ("eq_b2_freq", "eq_b2_gain")), ("hi", ("eq_hi_freq", "eq_hi_gain"))):
        if band not in eq:
            continue
        v = eq[band]
        if band == "lo":
            o += [("eq_lo_gain", v[0]), ("eq_lo_freq", v[1])]
        elif band == "b1":
            o += [("eq_b1_freq", v[0]), ("eq_b1_gain", v[1])] + ([("eq_b1_q", v[2])] if len(v) > 2 else [])
        else:
            o += [(band == "b2" and "eq_b2_freq" or "eq_hi_freq", v[0]), (band == "b2" and "eq_b2_gain" or "eq_hi_gain", v[1])]
    c = p["comp"]
    makeup = max(0.0, min(12.0, (-c[0]) * (1 - 1 / c[1]) * 0.35)) + corr
    makeup = max(0.0, min(24.0, round(makeup * 2) / 2))
    o += [("comp_thresh", c[0]), ("comp_ratio", c[1]), ("comp_attack", c[2]), ("comp_release", c[3]), ("comp_knee", c[4]), ("comp_makeup", makeup)]
    if p["sat"]:
        o.append(("sat_drive", p["sat"]))
    if p["echo"]:
        e = p["echo"]
        o += [("echo_on", 1), ("echo_sync", 1), ("echo_div", e[0]), ("echo_fb", e[1]), ("echo_tone", e[2]), ("echo_width", e[3]), ("echo_mix", e[4])]
    if p["dim"]:
        d = p["dim"]
        o += [("dim_on", 1), ("dim_width", d[0]), ("dim_detune", d[1]), ("dim_depth", d[2]), ("dim_mix", d[3])]
    if p["dbl"]:
        d = p["dbl"]
        o += [("dbl_on", 1), ("dbl_amount", d[0]), ("dbl_width", d[1]), ("dbl_detune", d[2]), ("dbl_timing", d[3])]
    if p["mut"]:
        m = p["mut"]
        o += [("mut_on", 1), ("mut_amount", m[0]), ("mut_character", m[1]), ("mut_blend", m[2])]
    if p["harm"]:
        o += [("harm_on", 1), ("harm_interval", p["harm"][0]), ("harm_level", p["harm"][1])]
    if p["stutter"]:
        o += [("stutter_on", 1), ("stutter_rate", p["stutter"][0]), ("stutter_mix", p["stutter"][1])]
    return o

DIVS = ["1/2", "1/4", "1/4 dotted", "1/4 triplet", "1/8", "1/8 dotted", "1/8 triplet", "1/16", "1/16 dotted", "1/16 triplet", "1/32"]

def settings_text(p, o):
    d = dict(o)
    parts = []
    parts.append("Pitch Tune %d%% / %d ms" % (d["tune_strength"], d["tune_speed"]) if "tune_on" in d else "no Pitch Tune")
    if "formant_shift" in d:
        parts.append("Formant %+.1f st" % d["formant_shift"])
    parts.append("%.0f:1 compression" % d["comp_ratio"] if d["comp_ratio"] == int(d["comp_ratio"]) else "%.1f:1 compression" % d["comp_ratio"])
    if "sat_drive" in d:
        parts.append("%.1f dB saturation" % d["sat_drive"])
    if "echo_on" in d:
        parts.append("%s echo %d%%" % (DIVS[int(d["echo_div"])], d["echo_mix"]))
    if "dim_on" in d:
        parts.append("Dimension")
    if "dbl_on" in d:
        parts.append("Doubler %d%%" % d["dbl_amount"])
    if "mut_on" in d:
        parts.append("Mutation %d%%" % d["mut_amount"])
    if "harm_on" in d:
        parts.append("octave-up Harmony")
    if "stutter_on" in d:
        parts.append("Stutter")
    return ", ".join(parts) + "."

def start_text(p, o):
    d = dict(o)
    s = ("Set your KEY and SCALE first: this preset pulls notes toward them. " if "tune_on" in d else "No tuning here, so start from an in-tune take. ")
    s += "Use a dry vocal with peaks around -6 dB. "
    if p["tags"] & (RG | EX) and d.get("sat_drive", 0) >= 7:
        s += "It is saturated: lower the SATURATION knob if it feels harsh."
    elif p["tags"] & (ML | RB) and "echo_on" in d:
        s += "Ease ECHO MIX down if the words get blurry."
    else:
        s += "Adjust COMPRESSOR THRESHOLD to match how loud you rap."
    return s

def main():
    corr = {}
    if len(sys.argv) > 1 and os.path.exists(sys.argv[1]):
        corr = json.load(open(sys.argv[1]))
    out = ["// GENERATED by tools/gen_artist_presets.py: do not edit by hand. 50 artist-inspired presets as overrides on the Factory Default sound.\n",
           "// The names are reference labels only; the settings are original starting points, not official, licensed or exact recreations.\n\n"]
    ents = []
    for i, p in enumerate(P):
        o = build(p, corr.get(p["artist"], 0.0))
        ids = [k for k, _ in o]
        assert len(ids) == len(set(ids)), p["artist"]
        out.append("const Override preset%d[] = {\n    " % i)
        out.append(", ".join('{ "%s", %s }' % (k, f(float(v))) for k, v in o))
        out.append(" };\n")
        name = p["artist"].upper() + " STYLE"
        tags = p["tags"]
        ents.append('    { { "%s", "%s", Region::%s, %d, "%s", "%s", "%s" }, GK_ENTRY (preset%d) }' % (
            name, p["artist"], "international" if p["region"] == I else "dhh", tags, p["intent"][0].upper() + p["intent"][1:] + ".",
            settings_text(p, o), start_text(p, o), i))
    out.append("\n#define GK_ENTRY(arr) arr, sizeof (arr) / sizeof (arr[0])\nconst Entry entries[count] = {\n" + ",\n".join(ents) + " };\n#undef GK_ENTRY\n")
    open("Source/Presets/ArtistPresets.inc", "w").write("".join(out))
    print("wrote", len(P), "presets")

main()
