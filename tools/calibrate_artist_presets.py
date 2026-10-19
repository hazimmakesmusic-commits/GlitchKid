#!/usr/bin/env python3
"""Reads the PRESETLEVEL lines the test suite prints and updates tools/artist_makeup.json so each preset lands near the input level.
usage: calibrate_artist_presets.py test.log"""
import json, re, sys, os
path = "tools/artist_makeup.json"
corr = json.load(open(path)) if os.path.exists(path) else {}
for line in open(sys.argv[1]):
    m = re.search(r"PRESETLEVEL (.+?) \| (-?[\d.]+) dB \| peak ([\d.]+)", line)
    if m:
        import math
        a, d, pk = m.group(1), float(m.group(2)), float(m.group(3))
        if pk > 0.9:
            d = max(d, 20 * math.log10(pk / 0.9))  # a peaky preset is turned down until its peak is safe
        corr[a] = round(corr.get(a, 0.0) - d * 0.9, 2)
json.dump(corr, open(path, "w"), indent=1, sort_keys=True)
print(len(corr), "corrections")
