#!/usr/bin/env python3
"""Paint a TSOTS map the way RPG Maker MV Tilemap does, at 24 px per cell.

Sheets come from tileset_registry.pdl (sheet_root + per-sheet keys).
Writes map.painted.png, and cells.png / cells.txt for the 3D atlas.
Does not touch the renderer.
"""
import json
import os
import sys
from PIL import Image

TILE_A5, TILE_A1, TILE_A2, TILE_A3, TILE_A4 = 1536, 2048, 2816, 4352, 5888
TILE_MAX = 8192
OUT = 24  # px per cell. Source sheets are 48. --native48 paints at 48 then the caller downscales.

FLOOR = [
    [[2,4],[1,4],[2,3],[1,3]],[[2,0],[1,4],[2,3],[1,3]],
    [[2,4],[3,0],[2,3],[1,3]],[[2,0],[3,0],[2,3],[1,3]],
    [[2,4],[1,4],[2,3],[3,1]],[[2,0],[1,4],[2,3],[3,1]],
    [[2,4],[3,0],[2,3],[3,1]],[[2,0],[3,0],[2,3],[3,1]],
    [[2,4],[1,4],[2,1],[1,3]],[[2,0],[1,4],[2,1],[1,3]],
    [[2,4],[3,0],[2,1],[1,3]],[[2,0],[3,0],[2,1],[1,3]],
    [[2,4],[1,4],[2,1],[3,1]],[[2,0],[1,4],[2,1],[3,1]],
    [[2,4],[3,0],[2,1],[3,1]],[[2,0],[3,0],[2,1],[3,1]],
    [[0,4],[1,4],[0,3],[1,3]],[[0,4],[3,0],[0,3],[1,3]],
    [[0,4],[1,4],[0,3],[3,1]],[[0,4],[3,0],[0,3],[3,1]],
    [[2,2],[1,2],[2,3],[1,3]],[[2,2],[1,2],[2,3],[3,1]],
    [[2,2],[1,2],[2,1],[1,3]],[[2,2],[1,2],[2,1],[3,1]],
    [[2,4],[3,4],[2,3],[3,3]],[[2,4],[3,4],[2,1],[3,3]],
    [[2,0],[3,4],[2,3],[3,3]],[[2,0],[3,4],[2,1],[3,3]],
    [[2,4],[1,4],[2,5],[1,5]],[[2,0],[1,4],[2,5],[1,5]],
    [[2,4],[3,0],[2,5],[1,5]],[[2,0],[3,0],[2,5],[1,5]],
    [[0,4],[3,4],[0,3],[3,3]],[[2,2],[1,2],[2,5],[1,5]],
    [[0,2],[1,2],[0,3],[1,3]],[[0,2],[1,2],[0,3],[3,1]],
    [[2,2],[3,2],[2,3],[3,3]],[[2,2],[3,2],[2,1],[3,3]],
    [[2,4],[3,4],[2,5],[3,5]],[[2,0],[3,4],[2,5],[3,5]],
    [[0,4],[1,4],[0,5],[1,5]],[[0,4],[3,0],[0,5],[1,5]],
    [[0,2],[3,2],[0,3],[3,3]],[[0,2],[1,2],[0,5],[1,5]],
    [[0,4],[3,4],[0,5],[3,5]],[[2,2],[3,2],[2,5],[3,5]],
    [[0,2],[3,2],[0,5],[3,5]],[[0,0],[1,0],[0,1],[1,1]],
]
WALL = [
    [[2,2],[1,2],[2,1],[1,1]],[[0,2],[1,2],[0,1],[1,1]],
    [[2,0],[1,0],[2,1],[1,1]],[[0,0],[1,0],[0,1],[1,1]],
    [[2,2],[3,2],[2,1],[3,1]],[[0,2],[3,2],[0,1],[3,1]],
    [[2,0],[3,0],[2,1],[3,1]],[[0,0],[3,0],[0,1],[3,1]],
    [[2,2],[1,2],[2,3],[1,3]],[[0,2],[1,2],[0,3],[1,3]],
    [[2,0],[1,0],[2,3],[1,3]],[[0,0],[1,0],[0,3],[1,3]],
    [[2,2],[3,2],[2,3],[3,3]],[[0,2],[3,2],[0,3],[3,3]],
    [[2,0],[3,0],[2,3],[3,3]],[[0,0],[3,0],[0,3],[3,3]],
]
WATERFALL = [
    [[2,0],[1,0],[2,1],[1,1]],[[0,0],[1,0],[0,1],[1,1]],
    [[2,0],[3,0],[2,1],[3,1]],[[0,0],[3,0],[0,1],[3,1]],
]

SHEET_KEYS = ("a1", "a2", "a3", "a4", "a5", "b", "c", "d", "e")


def house_assets():
    here = os.path.abspath(__file__)
    d = here
    for _ in range(8):
        d = os.path.dirname(d)
        cand = os.path.join(d, "#.NNEST_ASSETS")
        if os.path.isdir(cand):
            return cand
    raise SystemExit("no #.NNEST_ASSETS above the painter")


def load_registry(path):
    root = ""
    rows = {}
    for line in open(path, encoding="utf-8"):
        parts = [p.strip() for p in line.split("|")]
        if len(parts) < 3 or parts[0] != "TILESET":
            continue
        key, val = parts[1], parts[2]
        if key == "sheet_root":
            root = val
        else:
            rows[key] = val
    if not root:
        raise SystemExit("tileset_registry.pdl has no TILESET | sheet_root")
    base = os.path.join(house_assets(), root)
    return base, rows


def sheet_image(cache, base, rows, slug, part):
    key = "%s.%s" % (slug, part)
    name = rows.get(key, "")
    if not name:
        return None
    path = name if os.path.isabs(name) else os.path.join(base, name)
    if path not in cache:
        if not os.path.isfile(path):
            cache[path] = None
        else:
            cache[path] = Image.open(path).convert("RGBA")
    return cache[path]


def kind_shape(tile):
    return (tile - TILE_A1) // 48, (tile - TILE_A1) % 48


def is_a1(t):
    return TILE_A1 <= t < TILE_A2
def is_a2(t):
    return TILE_A2 <= t < TILE_A3
def is_a3(t):
    return TILE_A3 <= t < TILE_A4
def is_a4(t):
    return TILE_A4 <= t < TILE_MAX
def is_a5(t):
    return TILE_A5 <= t < TILE_A1


def autotile_origin(tile, frame=0):
    """Return (sheet_index 0..8, block_x, block_y, table). Frame 0 only for A1."""
    kind, shape = kind_shape(tile)
    tx, ty = kind % 8, kind // 8
    table = FLOOR
    if is_a1(tile):
        water = [0, 1, 2, 1][frame % 4]
        if kind == 0:
            bx, by = water * 2, 0
        elif kind == 1:
            bx, by = water * 2, 3
        elif kind == 2:
            bx, by = 6, 0
        elif kind == 3:
            bx, by = 6, 3
        else:
            bx = (tx // 4) * 8
            by = ty * 6 + (tx // 2) % 2 * 3
            if kind % 2 == 0:
                bx += water * 2
            else:
                bx += 6
                table = WATERFALL
                by += frame % 3
        return 0, bx, by, table
    if is_a2(tile):
        return 1, tx * 2, (ty - 2) * 3, FLOOR
    if is_a3(tile):
        return 2, tx * 2, (ty - 6) * 2, WALL
    if is_a4(tile):
        by = int((ty - 10) * 2.5 + (0.5 if ty % 2 == 1 else 0))
        table = WALL if ty % 2 == 1 else FLOOR
        return 3, tx * 2, by, table
    return None


def blit_quad(dst, src, sx, sy, sw, sh, dx, dy, dw, dh):
    if src is None:
        return
    # Source sheets are 48 px tiles. Destination cell is 24, so half is 12.
    # Sample 2 source px per dest px.
    crop = src.crop((int(sx), int(sy), int(sx + sw), int(sy + sh)))
    if crop.size != (dw, dh):
        crop = crop.resize((int(dw), int(dh)), Image.NEAREST)
    dst.alpha_composite(crop, (int(dx), int(dy)))


def draw_autotile(dst, src, tile, dx, dy, flags, table_edge=False):
    origin = autotile_origin(tile, 0)
    if not origin or src is None:
        return
    _, bx, by, table = origin
    _, shape = kind_shape(tile)
    quads = table[shape]
    half_s = 24  # source quadrant
    half_d = OUT // 2
    is_table = is_a2(tile) and flags and (flags[tile] & 0x80)
    if table_edge:
        for i in range(2):
            qsx, qsy = quads[2 + i]
            sx1 = (bx * 2 + qsx) * half_s
            sy1 = (by * 2 + qsy) * half_s + half_s / 2
            dx1 = dx + (i % 2) * half_d
            blit_quad(dst, src, sx1, sy1, half_s, half_s / 2, dx1, dy, half_d, half_d / 2)
        return
    for i in range(4):
        qsx, qsy = quads[i]
        sx1 = (bx * 2 + qsx) * half_s
        sy1 = (by * 2 + qsy) * half_s
        dx1 = dx + (i % 2) * half_d
        dy1 = dy + (i // 2) * half_d
        if is_table and qsy in (1, 5):
            qsx2 = [0, 3, 2, 1][qsx] if qsy == 1 else qsx
            qsy2 = 3
            sx2 = (bx * 2 + qsx2) * half_s
            sy2 = (by * 2 + qsy2) * half_s
            blit_quad(dst, src, sx2, sy2, half_s, half_s, dx1, dy1, half_d, half_d)
            blit_quad(dst, src, sx1, sy1, half_s, half_s / 2, dx1, dy1 + half_d / 2, half_d, half_d / 2)
        else:
            blit_quad(dst, src, sx1, sy1, half_s, half_s, dx1, dy1, half_d, half_d)


def draw_normal(dst, sheets, tile, dx, dy):
    if is_a5(tile):
        src = sheets[4]
        set_id = 4
    else:
        set_id = 5 + tile // 256
        src = sheets[set_id] if 0 <= set_id < len(sheets) else None
    if src is None:
        return
    w = h = 48
    sx = (tile // 128 % 2 * 8 + tile % 8) * w
    sy = ((tile % 256) // 8 % 16) * h
    blit_quad(dst, src, sx, sy, w, h, dx, dy, OUT, OUT)


def draw_tile(dst, sheets, flags, tile, dx, dy, table_edge=False):
    if table_edge:
        draw_autotile(dst, sheets[1], tile, dx, dy, flags, table_edge=True)
        return
    if tile <= 0 or tile >= TILE_MAX:
        return
    if tile >= TILE_A1:
        origin = autotile_origin(tile, 0)
        if not origin:
            return
        draw_autotile(dst, sheets[origin[0]], tile, dx, dy, flags)
    else:
        draw_normal(dst, sheets, tile, dx, dy)


def draw_shadow(dst, bits, dx, dy):
    if not (bits & 0x0F):
        return
    half = OUT // 2
    shade = Image.new("RGBA", (half, half), (0, 0, 0, 128))
    for i in range(4):
        if bits & (1 << i):
            dst.alpha_composite(shade, (dx + (i % 2) * half, dy + (i // 2) * half))


def higher(flags, tile):
    return bool(flags) and tile > 0 and (flags[tile] & 0x10)


def is_table(flags, tile):
    return is_a2(tile) and flags and (flags[tile] & 0x80)


def is_shadowing(tile):
    return is_a3(tile) or is_a4(tile)


def paint_cell_full(sheets, flags, tiles, shadow, do_shadow):
    """tiles is 4 ids. One 24px cell, MV order."""
    cell = Image.new("RGBA", (OUT, OUT), (0, 0, 0, 0))
    t0, t1, t2, t3 = tiles[0], tiles[1], tiles[2], tiles[3]
    lower, upper = [], []
    for t in (t0, t1):
        (upper if higher(flags, t) else lower).append(t)
    # shadow marker
    lower.append(("shadow", shadow))
    if is_table(flags, 0) :
        pass
    # table edge uses the tile above; caller passes it as tiles[4] if present
    above = tiles[4] if len(tiles) > 4 else 0
    if is_table(flags, above) and not is_table(flags, t1) and not is_shadowing(t0):
        lower.append(("edge", above))
    for t in (t2, t3):
        (upper if higher(flags, t) else lower).append(t)
    for item in lower:
        if isinstance(item, tuple) and item[0] == "shadow":
            if do_shadow:
                draw_shadow(cell, item[1], 0, 0)
        elif isinstance(item, tuple) and item[0] == "edge":
            draw_tile(cell, sheets, flags, item[1], 0, 0, table_edge=True)
        else:
            draw_tile(cell, sheets, flags, item, 0, 0)
    for t in upper:
        draw_tile(cell, sheets, flags, t, 0, 0)
    return cell


def paint_stack(sheets, flags, ids):
    cell = Image.new("RGBA", (OUT, OUT), (0, 0, 0, 0))
    for t in ids:
        draw_tile(cell, sheets, flags, t, 0, 0)
    return cell


def wall_cell(flags, t2, t3):
    for t in (t2, t3):
        if t <= 0:
            continue
        if is_a3(t):
            return True
        if is_a4(t):
            kind, _ = kind_shape(t)
            ty = kind // 8
            if ty % 2 == 1:
                return True
        if (t < TILE_A5 or is_a5(t)) and flags and (flags[t] & 0x0F) == 0x0F and not (flags[t] & 0x10):
            # B-E are < TILE_A5 (and not 0). A5 is the static sheet.
            if t < TILE_A5 or is_a5(t):
                if t < TILE_A1:
                    return True
    return False


def slug_for(name):
    return name.strip().lower().replace(" ", "_")


def load_map(data_dir, mid):
    doc = json.load(open(os.path.join(data_dir, "Map%03d.json" % mid), encoding="utf-8-sig"))
    tilesets = json.load(open(os.path.join(data_dir, "Tilesets.json"), encoding="utf-8-sig"))
    ts = tilesets[doc["tilesetId"]]
    return doc, ts


def find_data():
    base = "/home/no/Desktop"
    for root, dirs, files in os.walk(base):
        dirs[:] = [d for d in dirs if d not in ("img", "audio", "movies", "js", "node_modules", ".git")]
        if "Map004.json" in files and "Tilesets.json" in files and "RMMV_TSOTS" in root:
            return root
    raise SystemExit("TSOTS data not found")


def wall_ids(flags, t2, t3):
    """C2 rule. A3, or A4 on an odd kind-row, or B-E/A5 fully blocked and not star."""
    hit = []
    for t in (t2, t3):
        if t <= 0 or not flags or t >= len(flags):
            continue
        take = False
        if is_a3(t):
            take = True
        elif is_a4(t):
            kind, _ = kind_shape(t)
            if (kind // 8) % 2 == 1:
                take = True
        elif t < TILE_A1 and (flags[t] & 0x0F) == 0x0F and not (flags[t] & 0x10):
            take = True
        if take:
            hit.append(t)
    return hit


def write_atlas(path_png, path_txt, w, h, floors, walls):
    """floors/walls are lists of PIL images length w*h, or None for blank wall."""
    blank = Image.new("RGBA", (OUT, OUT), (0, 0, 0, 0))
    slots = [blank]
    index = {blank.tobytes(): 0}

    def slot(im):
        if im is None:
            return 0
        key = im.tobytes()
        if key in index:
            return index[key]
        # treat fully transparent as blank
        if im.getextrema()[3][1] == 0:
            index[key] = 0
            return 0
        index[key] = len(slots)
        slots.append(im)
        return index[key]

    tokens = []
    for i in range(w * h):
        tokens.append("%d,%d" % (slot(floors[i]), slot(walls[i])))
    cols = 16
    n = len(slots)
    rows = (n + cols - 1) // cols
    atlas = Image.new("RGBA", (cols * OUT, rows * OUT), (0, 0, 0, 0))
    for i, im in enumerate(slots):
        atlas.paste(im, ((i % cols) * OUT, (i // cols) * OUT))
    atlas.save(path_png)
    with open(path_txt, "w", encoding="utf-8") as f:
        f.write("width=%d\nheight=%d\ntile_px=%d\natlas_cols=16\natlas_tiles=%d\ncells\n" % (w, h, OUT, n))
        for y in range(h):
            f.write(" ".join(tokens[y * w:(y + 1) * w]))
            f.write("\n")
    return n


def paint_one(data, base, rows, cache, mid, out_dir, do_shadow):
    doc, ts = load_map(data, mid)
    w, h = int(doc["width"]), int(doc["height"])
    raw = doc["data"]
    flags = ts.get("flags") or []
    names = ts.get("tilesetNames") or [""] * 9
    slug = slug_for(ts.get("name") or "tileset")
    sheets = []
    for part, fname in zip(SHEET_KEYS, names):
        sheets.append(sheet_image(cache, base, rows, slug, part) if fname else None)
    img = Image.new("RGBA", (w * OUT, h * OUT), (0, 0, 0, 0))
    for y in range(h):
        for x in range(w):
            ids = [raw[L * w * h + y * w + x] for L in range(6)]
            above = raw[1 * w * h + (y - 1) * w + x] if y else 0
            cell = paint_cell_full(sheets, flags, ids[:4] + [above], ids[4], do_shadow)
            img.alpha_composite(cell, (x * OUT, y * OUT))
    os.makedirs(out_dir, exist_ok=True)
    img.save(os.path.join(out_dir, "map.painted.png"))
    floors, walls = [], []
    for y in range(h):
        for x in range(w):
            ids = [raw[L * w * h + y * w + x] for L in range(4)]
            floors.append(paint_stack(sheets, flags, ids[:2]))
            wids = wall_ids(flags, ids[2], ids[3])
            walls.append(paint_stack(sheets, flags, wids) if wids else None)
    n = write_atlas(os.path.join(out_dir, "cells.png"), os.path.join(out_dir, "cells.txt"), w, h, floors, walls)
    link = os.path.join(out_dir, "map.png")
    # Keep an existing export. Desks with no picture point at the paint.
    if not os.path.lexists(link):
        os.symlink(os.path.join(out_dir, "map.painted.png"), link)
    print("map%03d" % mid, w, h, "atlas", n, "slug", slug, flush=True)


def main():
    global OUT
    do_shadow = "--shadow" in sys.argv
    native48 = "--native48" in sys.argv
    if native48:
        OUT = 48
    reg = None
    here = os.path.dirname(os.path.abspath(__file__))
    # registry sits next to palettes, walk up
    d = here
    for _ in range(6):
        cand = os.path.join(d, "&.widgits", "palettes", "tilesets", "tileset_registry.pdl")
        # this file is already inside widgits/tsots-map-convert
        alt = os.path.join(os.path.dirname(d), "palettes", "tilesets", "tileset_registry.pdl")
        if os.path.isfile(cand):
            reg = cand
            break
        if os.path.isfile(alt):
            reg = alt
            break
        d = os.path.dirname(d)
    if not reg:
        reg = os.path.join(os.path.dirname(here), "palettes", "tilesets", "tileset_registry.pdl")
    base, rows = load_registry(reg)
    data = find_data()
    if "--all" in sys.argv:
        book = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "@.apps", "piececraft-hq", "pieces", "system", "maps", "tsots"))
        ids = []
        for name in sorted(os.listdir(book)):
            if name.startswith("map") and name[3:].isdigit():
                ids.append(int(name[3:]))
        cache = {}
        print("book", book, "desks", len(ids), flush=True)
        for mid in ids:
            paint_one(data, base, rows, cache, mid, os.path.join(book, "map%03d" % mid), do_shadow)
        return
    mid = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 4
    doc, ts = load_map(data, mid)
    w, h = int(doc["width"]), int(doc["height"])
    raw = doc["data"]
    flags = ts.get("flags") or []
    names = ts.get("tilesetNames") or [""] * 9
    slug = slug_for(ts.get("name") or "tileset")
    cache = {}
    sheets = []
    for part, fname in zip(SHEET_KEYS, names):
        sheets.append(sheet_image(cache, base, rows, slug, part) if fname else None)
    # also allow direct filename match if the registry key used the filename
    img = Image.new("RGBA", (w * OUT, h * OUT), (0, 0, 0, 0))
    for y in range(h):
        for x in range(w):
            ids = [raw[L * w * h + y * w + x] for L in range(6)]
            above = raw[1 * w * h + (y - 1) * w + x] if y else 0
            cell = paint_cell_full(sheets, flags, ids[:4] + [above], ids[4], do_shadow)
            img.alpha_composite(cell, (x * OUT, y * OUT))
    out_dir = sys.argv[2] if len(sys.argv) > 2 else "/tmp"
    os.makedirs(out_dir, exist_ok=True)
    if native48:
        img = img.reduce(2)
    painted = os.path.join(out_dir, "map.painted.png")
    img.save(painted)
    print("painted", painted, img.size, "shadow", do_shadow, "slug", slug, "native48", native48)
    if "--cells" in sys.argv:
        floors, walls = [], []
        for y in range(h):
            for x in range(w):
                ids = [raw[L * w * h + y * w + x] for L in range(4)]
                floors.append(paint_stack(sheets, flags, ids[:2]))
                wids = wall_ids(flags, ids[2], ids[3])
                walls.append(paint_stack(sheets, flags, wids) if wids else None)
        n = write_atlas(os.path.join(out_dir, "cells.png"), os.path.join(out_dir, "cells.txt"), w, h, floors, walls)
        print("atlas_tiles", n)


if __name__ == "__main__":
    main()
