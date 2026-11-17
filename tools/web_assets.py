#!/usr/bin/env python3
"""Website product assets from REAL plug-in renders (no drawing over the interface).

1. Render the screens from the current build (2x, the real editor driven by the real UI):
     GLITCHKID_TEST_ONLY="Website shots" GLITCHKID_WEB_SHOTS_DIR=<shots> ./GlitchKidTests
2. Convert them for the web:
     python3 tools/web_assets.py <shots>

Writes website/src/assets/product/<name>.webp (1x, 1120 wide) and <name>@2x.webp (2240 wide) for every page, unaltered crops of three modules
(EQ LAB, AIR LAB, SPACE) at 2x, and website/src/assets/og-image.png (1200x630: the real lockup and the real Home screen).
"""
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "website" / "src" / "assets" / "product"
VOID = (8, 13, 13)
PAGES = {  # render name -> web name
    "web_home": "home", "web_sounds": "advanced-sounds", "web_tune": "advanced-tune", "web_clean": "advanced-clean",
    "web_shape": "advanced-shape", "web_space": "advanced-space", "web_finish": "advanced-finish",
}
# crops in 1x editor coordinates (the renders are 2x): (source, x, y, w, h)
CROPS = {
    "detail-eqlab": ("web_shape", 156, 84, 948, 272),
    "detail-airlab": ("web_shape", 900, 362, 212, 166),
    "detail-space": ("web_space", 136, 126, 980, 314),
}


def save_webp(img, path, quality):
    img.convert("RGB").save(path, "WEBP", quality=quality, method=6)
    print(f"  {path.relative_to(ROOT)}  {img.width}x{img.height}  {path.stat().st_size // 1024} KB")


def main(shots: Path):
    OUT.mkdir(parents=True, exist_ok=True)
    for src, name in PAGES.items():
        img = Image.open(shots / f"{src}.png")
        assert img.size == (2240, 1360), f"{src}: expected a 2x render, got {img.size}"
        save_webp(img, OUT / f"{name}@2x.webp", 80)
        save_webp(img.resize((1120, 680), Image.LANCZOS), OUT / f"{name}.webp", 84)
    for name, (src, x, y, w, h) in CROPS.items():
        img = Image.open(shots / f"{src}.png").crop((x * 2, y * 2, (x + w) * 2, (y + h) * 2))
        save_webp(img, OUT / f"{name}@2x.webp", 82)

    # social preview: the real lockup over VOID, the real Home screen bleeding off the right edge, one line of copy
    og = Image.new("RGB", (1200, 630), VOID)
    home = Image.open(shots / "web_home.png").convert("RGB").resize((1120, 680), Image.LANCZOS)
    og.paste(home, (500, 196))
    d = ImageDraw.Draw(og)
    d.rectangle((499, 195, 1200, 630), outline=(41, 58, 52), width=1)
    shade = Image.linear_gradient("L").rotate(90).resize((180, 434))  # a soft edge where the screen meets the field
    og.paste(Image.new("RGB", (180, 434), VOID), (500, 196), shade)
    lockup = Image.open(ROOT / "Assets" / "Brand" / "exports" / "lockup" / "glitchkid-lockup-viridian.png").convert("RGBA")
    lockup = lockup.resize((560, 140), Image.LANCZOS)
    og.paste(lockup, (48, 40), lockup)
    bold = ImageFont.truetype(str(ROOT / "Assets" / "Fonts" / "Inter-Bold.otf"), 30)
    small = ImageFont.truetype(str(ROOT / "Assets" / "Fonts" / "Inter-Regular.otf"), 19)
    d.text((60, 470), "FREE BETA", font=bold, fill=(0, 221, 165))
    d.text((60, 510), "Join the waitlist", font=bold, fill=(241, 247, 244))
    d.text((60, 560), "Vocal production plug-in · VST3 · macOS", font=small, fill=(145, 166, 156))
    og.save(ROOT / "website" / "src" / "assets" / "og-image.png", optimize=True)
    print("  website/src/assets/og-image.png  1200x630")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(Path(sys.argv[1]))
