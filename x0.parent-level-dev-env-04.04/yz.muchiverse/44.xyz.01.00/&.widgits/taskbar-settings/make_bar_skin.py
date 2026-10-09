#!/usr/bin/python3 -I
"""make_bar_skin.py <sheet.png> <id> <left col,row> <mid col,row> <right col,row>
Cuts three 48x48 RPG Maker cells out of a tileset sheet into skins/<id>/{left,mid,right}/sprite.csv, the same
sprite.csv format the Palettes crops use ('# resolution=48' header, then r,g,b,a rows, row-major).
Cells are 0-based (col,row). Source cells come from design-docs RMMV-TILING-AND-WINDOW-SKIN-PRIMER.md (bar_skin_scan.py)."""
import os, sys
from PIL import Image

sheet, sid = sys.argv[1], sys.argv[2]
cells = {k: tuple(int(v) for v in sys.argv[3 + i].split(",")) for i, k in enumerate(("left", "mid", "right"))}
im = Image.open(sheet).convert("RGBA")
here = os.path.dirname(os.path.abspath(__file__))
for part, (c, r) in cells.items():
    d = os.path.join(here, "skins", sid, part)
    os.makedirs(d, exist_ok=True)
    tile = im.crop((c * 48, r * 48, c * 48 + 48, r * 48 + 48))
    with open(os.path.join(d, "sprite.csv"), "w") as f:
        f.write("# resolution=48\n# scale=1.0\n# transform=0,0,0\nr,g,b,a\n")
        for y in range(48):
            for x in range(48):
                p = tile.getpixel((x, y))
                f.write("%d,%d,%d,%d\n" % p)
print("wrote", sid)
