#!/usr/bin/env python3
"""Write one events.txt per TSOTS desk.

An event is a character standing on a cell, not a tile layer. The
active page is the last page whose conditions are all off (no switch,
no variable, no item). Pages that need a switch stay hidden, the same
way a new game would hide them.

A page with an empty characterName is logic only and is not an entity.
The line is:

    x y r g b charset index direction pattern

r g b is the center pixel of that charset frame, for the 3D box.
Sheets are copied once to #.NNEST_ASSETS/tsots-characters/.
"""
import json
import os
import shutil

from PIL import Image

BOOK = os.path.normpath(os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "../../@.apps/piececraft-hq/pieces/system/maps/tsots"))
FLAGS = ("actorValid", "itemValid", "selfSwitchValid",
         "switch1Valid", "switch2Valid", "variableValid")


def find_data():
    base = "/home/no/Desktop"
    for root, dirs, files in os.walk(base):
        dirs[:] = [d for d in dirs if d not in ("img", "audio", "movies", "js", "node_modules", ".git")]
        if "Map004.json" in files and "Tilesets.json" in files and "RMMV_TSOTS" in root:
            return root
    raise SystemExit("TSOTS data not found")


def assets_dir():
    p = os.path.abspath(__file__)
    for _ in range(12):
        p = os.path.dirname(p)
        cand = os.path.join(p, "#.NNEST_ASSETS")
        if os.path.isdir(cand):
            return os.path.join(cand, "tsots-characters")
    raise SystemExit("#.NNEST_ASSETS not found")


def active_page(event):
    page = None
    for p in event.get("pages") or []:
        cond = p.get("conditions") or {}
        if not any(cond.get(k) for k in FLAGS):
            page = p
    return page


def frame_box(im, name, index, direction, pattern):
    big = name[:1] == "$"
    sw, sh = im.size
    row = {2: 0, 4: 1, 6: 2, 8: 3}.get(int(direction), 0)
    pattern = max(0, min(2, int(pattern)))
    if big:
        pw, ph = sw // 3, sh // 4
        sx, sy = pattern * pw, row * ph
    else:
        pw, ph = sw // 12, sh // 8
        index = max(0, min(7, int(index)))
        sx = ((index % 4) * 3 + pattern) * pw
        sy = ((index // 4) * 4 + row) * ph
    if pw < 1 or ph < 1:
        return 200, 40, 40
    px = im.getpixel((sx + pw // 2, sy + ph // 2))
    return px[0], px[1], px[2]


def main():
    data = find_data()
    chars = os.path.join(os.path.dirname(data), "img", "characters")
    store = assets_dir()
    os.makedirs(store, exist_ok=True)
    cache = {}
    total = 0
    for fn in sorted(os.listdir(data)):
        if not (fn.startswith("Map") and fn.endswith(".json") and fn != "MapInfos.json"):
            continue
        mid = int(fn[3:-5])
        desk = os.path.join(BOOK, "map%03d" % mid)
        if not os.path.isdir(desk):
            continue
        doc = json.load(open(os.path.join(data, fn), encoding="utf-8-sig"))
        lines = []
        for ev in doc.get("events") or []:
            if not ev:
                continue
            page = active_page(ev)
            if not page:
                continue
            im = page.get("image") or {}
            name = (im.get("characterName") or "").strip()
            if not name or any(c in name for c in " \t/\\"):
                continue
            src = os.path.join(chars, name + ".png")
            if not os.path.isfile(src):
                continue
            if name not in cache:
                shutil.copyfile(src, os.path.join(store, name + ".png"))
                cache[name] = Image.open(src).convert("RGBA")
            r, g, b = frame_box(cache[name], name, im.get("characterIndex") or 0,
                                im.get("direction") or 2, im.get("pattern") or 1)
            lines.append("%d %d %d %d %d %s %d %d %d\n" % (
                int(ev["x"]), int(ev["y"]), r, g, b, name,
                int(im.get("characterIndex") or 0),
                int(im.get("direction") or 2),
                int(im.get("pattern") or 1)))
        with open(os.path.join(desk, "events.txt"), "w", encoding="utf-8") as f:
            f.writelines(lines)
        total += len(lines)
        if lines:
            print("map%03d" % mid, len(lines))
    print("entities", total, "sheets", len(cache))


if __name__ == "__main__":
    main()
