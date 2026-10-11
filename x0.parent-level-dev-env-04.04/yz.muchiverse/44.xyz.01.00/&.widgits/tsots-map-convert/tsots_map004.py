#!/usr/bin/env python3
"""Convert TSOTS Map004.json into the pc-hq book/page files.

Book: pieces/system/maps/tsots/game.pdl
Page: pieces/system/maps/tsots/map004/map.txt + extrusion.pdl

One glyph per cell from the top non-empty of the four ground layers.
W = blocked on all four sides (flag bits 0x0F). ~ = A1 water.
Everything else is floor '.'. No new renderer. The map PNG is not written.
"""
import json
import os
import sys

A1 = 2048
A1_END = 2816


def find_data():
    base = "/home/no/Desktop"
    house = [os.path.join(base, n) for n in os.listdir(base) if n.startswith("🤖")][0]
    sec = [os.path.join(house, k) for k in os.listdir(house) if k.startswith("xv.")][0]
    proj = [os.path.join(sec, k) for k in os.listdir(sec) if k.startswith("RMMV_TSOTS")][0]
    for root, dirs, files in os.walk(proj):
        if "Map004.json" in files:
            return root
        dirs[:] = [d for d in dirs if d not in ("img", "audio", "movies", "js")]
    raise SystemExit("Map004.json not found")


def glyph(tid, flags):
    if tid <= 0:
        return "."
    if A1 <= tid < A1_END:
        return "~"
    flag = flags[tid] if tid < len(flags) else 0
    if (flag & 0x0F) == 0x0F:
        return "W"
    return "."


def main():
    data = find_data()
    mp = json.load(open(os.path.join(data, "Map004.json"), encoding="utf-8-sig"))
    tilesets = json.load(open(os.path.join(data, "Tilesets.json"), encoding="utf-8-sig"))
    flags = tilesets[mp["tilesetId"]]["flags"]
    w, h = mp["width"], mp["height"]
    cells = w * h
    raw = mp["data"]
    rows = []
    for y in range(h):
        line = []
        for x in range(w):
            i = y * w + x
            tid = 0
            for layer in (3, 2, 1, 0):
                v = raw[layer * cells + i]
                if v:
                    tid = v
                    break
            line.append(glyph(tid, flags))
        rows.append("".join(line))
    out = sys.argv[1]
    desk = os.path.join(out, "map004")
    os.makedirs(desk, exist_ok=True)
    with open(os.path.join(desk, "map.txt"), "w") as f:
        f.write("\n".join(rows) + "\n")
    with open(os.path.join(desk, "extrusion.pdl"), "w") as f:
        f.write(
            "SECTION      | KEY                | VALUE\n"
            "----------------------------------------------------------------------\n"
            "META         | map_id             | tsots\n"
            "META         | desk_id            | map004\n"
            "META         | note               | Map004 Outside. W blocked, ~ A1 water.\n"
            "EXTRUDE      | W                  | 3\n"
            "EXTRUDE      | default             | 0\n"
        )
    with open(os.path.join(out, "game.pdl"), "w") as f:
        f.write(
            "SECTION      | KEY                | VALUE\n"
            "----------------------------------------------------------------------\n"
            "GAME         | type               | board-game\n"
            "GAME         | icon               | 🗺️\n"
            "GAME         | label              | TSOTS\n"
            "GAME         | n_chunks           | 1\n"
            "GAME         | chunk_0_x          | 0\n"
            "GAME         | chunk_0_y          | 0\n"
            "GAME         | n_desks            | 1\n"
            "GAME         | desk_1_id          | map004\n"
            "GAME         | desk_1_label       | Map 004\n"
        )
    print(f"{w}x{h} walls {sum(r.count('W') for r in rows)} water {sum(r.count('~') for r in rows)}")


if __name__ == "__main__":
    main()
