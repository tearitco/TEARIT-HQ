#!/usr/bin/env python3
"""Write every TSOTS map as a desk of the pc-hq book maps/tsots.

One glyph per cell. W = blocked on all four sides. ~ = A1 water.
A MapInfos row with no JSON file still gets a desk, one floor cell.
"""
import json
import os
import sys

A1, A1_END = 2048, 2816


def find_data():
    base = "/home/no/Desktop"
    house = [os.path.join(base, n) for n in os.listdir(base) if n.startswith("🤖")][0]
    sec = [os.path.join(house, k) for k in os.listdir(house) if k.startswith("xv.")][0]
    proj = [os.path.join(sec, k) for k in os.listdir(sec) if k.startswith("RMMV_TSOTS")][0]
    for root, dirs, files in os.walk(proj):
        if "MapInfos.json" in files:
            return root
        dirs[:] = [d for d in dirs if d not in ("img", "audio", "movies", "js")]
    raise SystemExit("MapInfos.json not found")


def glyph(tid, flags):
    if tid <= 0:
        return "."
    if A1 <= tid < A1_END:
        return "~"
    flag = flags[tid] if 0 <= tid < len(flags) else 0
    if (flag & 0x0F) == 0x0F:
        return "W"
    return "."


def rows_for(mp, flags):
    w, h = mp["width"], mp["height"]
    cells = w * h
    raw = mp["data"]
    out = []
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
        out.append("".join(line))
    return out


def main():
    data = find_data()
    out = sys.argv[1]
    infos = json.load(open(os.path.join(data, "MapInfos.json"), encoding="utf-8-sig"))
    tilesets = json.load(open(os.path.join(data, "Tilesets.json"), encoding="utf-8-sig"))
    files = {}
    for name in os.listdir(data):
        if name.startswith("Map") and name.endswith(".json") and name != "MapInfos.json":
            num = name[3:-5]
            if num.isdigit():
                files[int(num)] = name
    pages = []
    seen = set()
    for info in infos:
        if not info or "id" not in info:
            continue
        mid = int(info["id"])
        if mid <= 0 or mid in seen:
            continue
        seen.add(mid)
        pages.append((mid, (info.get("name") or "").strip()))
    for mid in sorted(files):
        if mid not in seen:
            seen.add(mid)
            pages.append((mid, ""))
    max_w = max_h = 1
    for n, (mid, name) in enumerate(pages, start=1):
        desk = "map%03d" % mid
        folder = os.path.join(out, desk)
        os.makedirs(folder, exist_ok=True)
        path = files.get(mid)
        if path:
            mp = json.load(open(os.path.join(data, path), encoding="utf-8-sig"))
            flags = tilesets[mp["tilesetId"]]["flags"] if mp["tilesetId"] < len(tilesets) and tilesets[mp["tilesetId"]] else []
            rows = rows_for(mp, flags)
            max_w = max(max_w, mp["width"])
            max_h = max(max_h, mp["height"])
        else:
            rows = ["."]
        with open(os.path.join(folder, "map.txt"), "w") as f:
            f.write("\n".join(rows) + "\n")
        label = name if name else ("Map %03d" % mid)
        label = " ".join(label.split())[:48]
        with open(os.path.join(folder, "extrusion.pdl"), "w") as f:
            f.write(
                "SECTION      | KEY                | VALUE\n"
                "----------------------------------------------------------------------\n"
                "META         | map_id             | tsots\n"
                "META         | desk_id            | %s\n"
                "META         | note               | %s\n"
                "EXTRUDE      | W                  | 3\n"
                "EXTRUDE      | default             | 0\n" % (desk, label.replace("|", "/"))
            )
        pages[n - 1] = (mid, label, desk)
    cx = (max_w + 15) // 16
    cy = (max_h + 15) // 16
    n_chunks = cx * cy
    lines = [
        "SECTION      | KEY                | VALUE",
        "----------------------------------------------------------------------",
        "GAME         | type               | board-game",
        "GAME         | icon               | 🗺️",
        "GAME         | label              | TSOTS",
        "GAME         | n_chunks           | %d" % n_chunks,
    ]
    i = 0
    for y in range(cy):
        for x in range(cx):
            lines.append("GAME         | chunk_%d_x          | %d" % (i, x))
            lines.append("GAME         | chunk_%d_y          | %d" % (i, y))
            i += 1
    lines.append("GAME         | n_desks            | %d" % len(pages))
    for n, (mid, label, desk) in enumerate(pages, start=1):
        lines.append("GAME         | desk_%d_id          | %s" % (n, desk))
        lines.append("GAME         | desk_%d_label       | %s" % (n, label))
    with open(os.path.join(out, "game.pdl"), "w") as f:
        f.write("\n".join(lines) + "\n")
    missing = sum(1 for mid, _label, _desk in pages if mid not in files)
    print("desks", len(pages), "from_files", len(files), "no_file", missing, "chunks", n_chunks, "max", max_w, max_h)


if __name__ == "__main__":
    main()
