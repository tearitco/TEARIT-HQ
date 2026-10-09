#!/usr/bin/env python3
"""Phase 1: write layers.txt beside each TSOTS desk map.txt.

Six planes, row-major, one decimal id per cell. map.txt is not touched.
Also links the existing 24 px MapNNN.png as map.png so the 2D blit has
a file. That link is not the 48 px pass image.
"""
import json
import os
import sys

DATA = sys.argv[1]
BOOK = sys.argv[2]
PNG_DIR = sys.argv[3]


def main():
    n_ok = 0
    n_png = 0
    missing = []
    for name in sorted(os.listdir(DATA)):
        if not (name.startswith("Map") and name.endswith(".json") and name != "MapInfos.json"):
            continue
        mid = int(name[3:-5])
        desk = "map%03d" % mid
        dest_dir = os.path.join(BOOK, desk)
        if not os.path.isdir(dest_dir):
            missing.append(desk)
            continue
        with open(os.path.join(DATA, name), encoding="utf-8-sig") as f:
            doc = json.load(f)
        w, h = int(doc["width"]), int(doc["height"])
        data = doc["data"]
        if len(data) != w * h * 6:
            raise SystemExit("%s data %d != %d" % (name, len(data), w * h * 6))
        out = os.path.join(dest_dir, "layers.txt")
        with open(out, "w", encoding="utf-8") as f:
            f.write("width=%d\nheight=%d\nlayers=6\n" % (w, h))
            for layer in range(6):
                f.write("layer %d\n" % layer)
                base = layer * w * h
                for y in range(h):
                    row = data[base + y * w: base + y * w + w]
                    f.write(" ".join(str(int(v)) for v in row))
                    f.write("\n")
        n_ok += 1
        src = os.path.join(PNG_DIR, "Map%03d.png" % mid)
        link = os.path.join(dest_dir, "map.png")
        if os.path.isfile(src):
            if os.path.lexists(link):
                os.remove(link)
            os.symlink(src, link)
            n_png += 1
    print("layers", n_ok, "png", n_png, "missing_desks", len(missing))
    if missing:
        print("missing", " ".join(missing[:12]))


if __name__ == "__main__":
    main()
