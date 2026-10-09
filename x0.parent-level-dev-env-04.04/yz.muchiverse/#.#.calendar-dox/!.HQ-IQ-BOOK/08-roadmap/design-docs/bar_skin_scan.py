#!/usr/bin/env python3
"""Find RPG Maker cells that can tile as a text bar.

A middle cell passes when:
- an opaque band crosses the cell and overlaps the text rows (12..35)
- the band is shorter than the full cell (a bar, not a solid fill)
- column 0 and column 47 match within a per-pixel tolerance

A left cap joins that middle when the cap's column 47 matches the
middle's column 0. A right cap joins when its column 0 matches the
middle's column 47. MIRROR means the left cap flipped horizontally
is used as the right cap.

The mutant at the bottom must fail: a middle whose right column was
replaced so it no longer matches its left column.
"""
import os
import sys
from PIL import Image

BASE = os.environ.get(
    "RMMV_TILESETS",
    "/home/no/Downloads/TEARIT-HQ-2026-09-02/TEARIT-HQ-main+sluggi.00.01/"
    "#.NNEST_ASSETS/rmmv-www-img/tilesets",
)
TOL = 28  # max channel delta on an opaque pixel
TEXT_TOP, TEXT_BOT = 12, 36


def col_pixels(px, c, r, col):
    out = []
    for y in range(48):
        out.append(px[c * 48 + col, r * 48 + y])
    return out


def edge_ok(a, b):
    opaque = 0
    for pa, pb in zip(a, b):
        if pa[3] < 40 and pb[3] < 40:
            continue
        opaque += 1
        if abs(pa[3] - pb[3]) > 40:
            return False
        if max(abs(pa[i] - pb[i]) for i in range(3)) > TOL:
            return False
    # Transparent columns match everything. A real seam has paint.
    return opaque >= 6


def band(px, c, r):
    rows = []
    for y in range(48):
        opaque = 0
        for x in range(48):
            p = px[c * 48 + x, r * 48 + y]
            if p[3] >= 40 and not (p[0] > 200 and p[2] > 200 and p[1] < 80):
                opaque += 1
        rows.append(opaque >= 36)
    if not any(rows[y] for y in range(TEXT_TOP, TEXT_BOT)):
        return None
    ys = [i for i, on in enumerate(rows) if on]
    if not ys:
        return None
    h = ys[-1] - ys[0] + 1
    if h < 6 or h > 30:
        return None
    # the band itself must be solid across, no hole in the middle of it
    if any(not rows[y] for y in range(ys[0], ys[-1] + 1)):
        return None
    return ys[0], h


def scan(sheets):
    middles = []
    for name in sheets:
        path = os.path.join(BASE, name)
        if not os.path.isfile(path):
            continue
        im = Image.open(path).convert("RGBA")
        px = im.load()
        cols, rows = im.size[0] // 48, im.size[1] // 48
        for r in range(rows):
            for c in range(cols):
                b = band(px, c, r)
                if not b:
                    continue
                left, right = col_pixels(px, c, r, 0), col_pixels(px, c, r, 47)
                if edge_ok(left, right):
                    middles.append((name, c, r, b, im))
    return middles


def mutant_must_fail():
    """A middle that tiled, with its right column painted a different color."""
    im = Image.new("RGBA", (48, 48), (0, 0, 0, 0))
    px = im.load()
    for y in range(20, 32):
        for x in range(48):
            px[x, y] = (180, 140, 90, 255)
    assert band(px, 0, 0)
    assert edge_ok(col_pixels(px, 0, 0, 0), col_pixels(px, 0, 0, 47))
    for y in range(48):
        px[47, y] = (0, 255, 0, 255)
    assert band(px, 0, 0)
    assert not edge_ok(col_pixels(px, 0, 0, 0), col_pixels(px, 0, 0, 47))


if __name__ == "__main__":
    mutant_must_fail()
    names = sorted(
        n for n in os.listdir(BASE)
        if n.endswith(".png") and (
            n.endswith("_B.png") or n.endswith("_C.png") or n.endswith("_D.png")
            or n.endswith("_E.png") or "_A4" in n or "_A5" in n or n.startswith("Window")
        )
    )
    found = scan(names)
    print(f"middles {len(found)}")
    for name, c, r, b, _im in found:
        print(f"  {name} {c},{r} band_y={b[0]} h={b[1]}")
