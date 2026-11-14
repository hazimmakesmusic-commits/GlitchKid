#!/usr/bin/env python3
"""GLITCHKID logo system generator.

One geometric construction -> every logo file in Assets/Brand. Run it after changing the geometry:

    python3 tools/brand/gen_brand.py            # writes Assets/Brand/*.svg
    python3 tools/brand/gen_brand.py --preview  # also writes PNG previews to the given folder (needs Pillow)

The emblem is a forward-leaning "GK": a G whose top is a blade cut to a point, whose crossbar runs on into a K, and whose K leg breaks
out of the letter as a spike (the broken signal). The wordmark is drawn on the same construction: straight strokes, 45-degree cuts,
the same italic lean, one glitch slice through the K. Everything is plain polygons (no fonts, no strokes), so the paths are exact at
every size and the plug-in draws the very same outlines (it loads the white versions from BinaryData).
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, "..", "..", "Assets", "Brand"))
LEAN = 0.24  # the italic lean: x moves right by LEAN per unit of height above the baseline

# ---------------------------------------------------------------- geometry helpers


def skew(poly, base_y=100.0):
    return [(x + (base_y - y) * LEAN, y) for x, y in poly]


def translate(poly, dx, dy=0.0):
    return [(x + dx, y + dy) for x, y in poly]


def area(poly):
    a = 0.0
    for i in range(len(poly)):
        x1, y1 = poly[i]
        x2, y2 = poly[(i + 1) % len(poly)]
        a += x1 * y2 - x2 * y1
    return a / 2.0


def orient(poly, clockwise=True):
    """Clockwise on screen (y down) = positive shoelace area. Outer contours clockwise, holes anticlockwise (non-zero fill)."""
    return poly if (area(poly) > 0) == clockwise else list(reversed(poly))


def bbox(polys):
    xs = [x for p in polys for x, _ in p]
    ys = [y for p in polys for _, y in p]
    return min(xs), min(ys), max(xs), max(ys)


def path_d(polys):
    return " ".join("M" + " L".join(f"{x:.2f},{y:.2f}" for x, y in p) + " Z" for p in polys)


# ---------------------------------------------------------------- the emblem: a forward-leaning GK with a broken K leg

def emblem():
    # drawn upright on a 120 x 100 grid, then leaned
    body = [  # the G: blade top (cut to a point at the upper right), left spine, base
        (16, 66), (20, 40), (38, 20), (60, 12), (116, 0),  # outer edge up to the blade tip
        (66, 24), (46, 30), (36, 42), (33, 64),            # inner edge back down
        (42, 74), (70, 74), (74, 58), (92, 58),            # inner base, up the right stem to the crossbar's underside
        (84, 86), (34, 86),                                # outer base
    ]
    bar = [(46, 44), (104, 44), (100, 58), (43, 58)]        # the crossbar, running on into the K
    leg = [(74, 58), (100, 58), (124, 100), (94, 82)]       # the K leg breaking out of the letter as a spike (the broken signal)
    arm = [(82, 44), (104, 44), (126, 18), (98, 34)]        # the K arm, cut upward to a point
    polys = [orient(skew(p)) for p in (body, bar, leg, arm)]
    return polys


# ---------------------------------------------------------------- the wordmark: GLITCHKID

S = 17.0  # stroke
C = 14.0  # outer 45-degree cut


def letter_G(w=86):
    return [[(w, 0), (w - 6, S), (C + 8, S), (S, C + 8), (S, 100 - C - 8), (C + 8, 100 - S), (w - S, 100 - S), (w - S, 64), (46, 64), (49, 50),
             (w, 50), (w, 100 - C), (w - C, 100), (C, 100), (0, 100 - C), (0, C), (C, 0)]]


def letter_L(w=66):
    return [[(0, 0), (S, 0), (S, 100 - S), (w, 100 - S), (w - 4, 100), (C, 100), (0, 100 - C)]]


def letter_I():
    return [[(0, 0), (S, 0), (S, 100), (0, 100)]]


def letter_T(w=78):
    m = w / 2.0
    return [[(0, 0), (w, 0), (w - 4, S), (m + S / 2, S), (m + S / 2, 100), (m - S / 2, 100), (m - S / 2, S), (4, S)]]


def letter_C(w=80):
    return [[(w, 0), (w - 4, S), (C + 8, S), (S, C + 8), (S, 100 - C - 8), (C + 8, 100 - S), (w - 4, 100 - S), (w, 100), (C, 100), (0, 100 - C),
             (0, C), (C, 0)]]


def letter_H(w=80):
    return [[(0, 0), (S, 0), (S, 41), (w - S, 41), (w - S, 0), (w, 0), (w, 100), (w - S, 100), (w - S, 59), (S, 59), (S, 100), (0, 100)]]


def letter_K(w=80):
    stem = [(0, 0), (S, 0), (S, 100), (0, 100)]
    arm = [(S, 44), (w - 24, 0), (w, 0), (S + 10, 52), (S, 52)]           # upper arm
    leg = [(S + 12, 58), (S + 30, 58), (w, 100), (w - 22, 100)]            # leg, below the glitch slice (the gap at y 52..58)
    return [stem, arm, leg]


def letter_D(w=84):
    outer = [(0, 0), (w - C - 8, 0), (w, C + 8), (w, 100 - C - 8), (w - C - 8, 100), (0, 100)]
    inner = [(S, S), (S, 100 - S), (w - C - 14, 100 - S), (w - S, 100 - C - 12), (w - S, C + 12), (w - C - 14, S)]
    return [outer, inner]


def wordmark():
    letters = [letter_G(), letter_L(), letter_I(), letter_T(), letter_C(), letter_H(), letter_K(), letter_I(), letter_D()]
    gap = 11.0
    x = 0.0
    polys = []
    for parts in letters:
        x0, _, x1, _ = bbox(parts)
        for k, p in enumerate(parts):
            q = skew(translate(p, x - x0))
            hole = parts is letters[-1] and k == 1
            polys.append(orient(q, clockwise=not hole))
        x += (x1 - x0) + gap
    return polys


# ---------------------------------------------------------------- SVG output

CHROME = [("0", "#F7F8FA"), ("0.38", "#C3C8D0"), ("0.5", "#8A909A"), ("0.62", "#DDE1E6"), ("1", "#6E747E")]


def normalise(polys, pad=0.0):
    x0, y0, x1, y1 = bbox(polys)
    out = [translate(p, -x0 + pad, -y0 + pad) for p in polys]
    return out, (x1 - x0) + 2 * pad, (y1 - y0) + 2 * pad


def svg(polys, w, h, fill, defs="", extra="", title="GLITCHKID"):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w:.2f} {h:.2f}" width="{w:.0f}" height="{h:.0f}">\n'
            f"  <title>{title}</title>\n{defs}"
            f'  <path fill="{fill}" fill-rule="nonzero" d="{path_d(polys)}"/>\n{extra}</svg>\n')


def chrome_defs(gid, w, h):
    stops = "".join(f'<stop offset="{o}" stop-color="{c}"/>' for o, c in CHROME)
    return (f'  <defs><linearGradient id="{gid}" x1="0" y1="0" x2="{w * 0.25:.1f}" y2="{h:.1f}" gradientUnits="userSpaceOnUse">'
            f"{stops}</linearGradient></defs>\n")


def write(name, text):
    with open(os.path.join(OUT, name), "w") as f:
        f.write(text)
    print("wrote", os.path.join("Assets/Brand", name))


def main():
    os.makedirs(OUT, exist_ok=True)
    em, ew, eh = normalise(emblem(), 2.0)
    wm, ww, wh = normalise(wordmark(), 2.0)

    # emblem and wordmark: primary (chrome), white, black, single colour (the default profile's ultraviolet)
    for name, polys, w, h in (("emblem", em, ew, eh), ("wordmark", wm, ww, wh)):
        write(f"glitchkid-{name}.svg", svg(polys, w, h, f"url(#chrome-{name})", chrome_defs(f"chrome-{name}", w, h)))
        write(f"glitchkid-{name}-white.svg", svg(polys, w, h, "#FFFFFF"))
        write(f"glitchkid-{name}-black.svg", svg(polys, w, h, "#000000"))
        write(f"glitchkid-{name}-mono.svg", svg(polys, w, h, "#8C4CF7"))

    # the lockup: emblem, wordmark and the tagline (set in Inter, the interface typeface)
    scale = eh / wh * 0.62
    lock_w = ew + 18 + ww * scale
    lock = [translate(p, 0, 0) for p in em] + [[(ew + 18 + x * scale, eh * 0.19 + y * scale) for x, y in p] for p in wm]
    tagline = (f'  <text x="{ew + 18 + 2:.1f}" y="{eh * 0.19 + wh * scale + 16:.1f}" fill="#B9BEC7" font-family="Inter, Helvetica, Arial, sans-serif" '
               f'font-size="9" letter-spacing="4.2">— BUILT TO SOUND DIFFERENT</text>\n')
    write("glitchkid-lockup.svg", svg(lock, lock_w, eh + 14, "url(#chrome-lockup)", chrome_defs("chrome-lockup", lock_w, eh), tagline))

    # the app icon: emblem only, on a graphite tile with the default profile's corner light
    size = 1024.0
    k = size * 0.56 / max(ew, eh)
    ox, oy = (size - ew * k) / 2, (size - eh * k) / 2
    icon_polys = [[(ox + x * k, oy + y * k) for x, y in p] for p in em]
    icon_defs = (
        '  <defs>\n'
        '    <linearGradient id="tile" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#17151E"/><stop offset="1" stop-color="#08070B"/></linearGradient>\n'
        '    <radialGradient id="glow" cx="1" cy="1" r="0.9"><stop offset="0" stop-color="#8C4CF7" stop-opacity="0.85"/>'
        '<stop offset="0.45" stop-color="#FF4FB8" stop-opacity="0.25"/><stop offset="1" stop-color="#FF4FB8" stop-opacity="0"/></radialGradient>\n'
        f'    <linearGradient id="chrome-icon" x1="{ox:.0f}" y1="{oy:.0f}" x2="{ox + ew * k * 0.25:.0f}" y2="{oy + eh * k:.0f}" gradientUnits="userSpaceOnUse">'
        + "".join(f'<stop offset="{o}" stop-color="{c}"/>' for o, c in CHROME) + "</linearGradient>\n"
        '  </defs>\n'
        '  <rect width="1024" height="1024" rx="224" fill="url(#tile)"/>\n'
        '  <rect width="1024" height="1024" rx="224" fill="url(#glow)"/>\n'
        '  <rect x="6" y="6" width="1012" height="1012" rx="218" fill="none" stroke="#FFFFFF" stroke-opacity="0.08" stroke-width="4"/>\n')
    write("glitchkid-icon.svg", svg(icon_polys, size, size, "url(#chrome-icon)", icon_defs, title="GLITCHKID app icon"))

    if "--preview" in sys.argv:
        preview(em, ew, eh, wm, ww, wh, sys.argv[sys.argv.index("--preview") + 1])


def preview(em, ew, eh, wm, ww, wh, folder):
    from PIL import Image, ImageDraw
    os.makedirs(folder, exist_ok=True)

    def render(polys, w, h, scale, name, holes=()):
        img = Image.new("RGB", (int(w * scale) + 20, int(h * scale) + 20), (16, 16, 20))
        d = ImageDraw.Draw(img)
        for p in polys:
            col = (16, 16, 20) if area(p) < 0 else (230, 232, 236)
            d.polygon([(10 + x * scale, 10 + y * scale) for x, y in p], fill=col)
        img.save(os.path.join(folder, name))

    render(em, ew, eh, 4.0, "emblem_400.png")
    render(em, ew, eh, 0.28, "emblem_28.png")
    render(wm, ww, wh, 1.0, "wordmark_100.png")
    render(wm, ww, wh, 0.14, "wordmark_14.png")


if __name__ == "__main__":
    main()
