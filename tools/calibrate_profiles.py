#!/usr/bin/env python3
"""Reads the PROFILELEVEL lines the test suite prints and updates tools/profile_makeup.json so each profile lands near the input level and keeps headroom.
usage: calibrate_profiles.py test.log"""
import json, math, os, re, sys
path = "tools/profile_makeup.json"
corr = json.load(open(path)) if os.path.exists(path) else {}
for line in open(sys.argv[1]):
    m = re.search(r"PROFILELEVEL (.+?) \| (-?[\d.]+) dB \| peak ([\d.]+)", line)
    if m:
        name, d, pk = m.group(1), float(m.group(2)), float(m.group(3))
        if pk > 0.9:
            d = max(d, 20 * math.log10(pk / 0.9))  # a peaky profile is turned down until its peak is safe
        corr[name] = round(corr.get(name, 0.0) - d * 0.9, 2)
json.dump(corr, open(path, "w"), indent=1, sort_keys=True)
print(len(corr), "corrections")
