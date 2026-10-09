#!/usr/bin/env python3
"""Copy each TSOTS map's RPG Maker parallax onto its pc-hq desk.

Reads parallaxName, parallaxLoopX/Y, parallaxSx/Sy, parallaxShow from
the map JSON. Writes parallax.pdl beside layers.txt. The picture is a
relative symlink, parallax.png, plus parallax.rgba (raw RGBA8) for the
3D loader, which does not decode PNG. Unique pictures live once under
#.NNEST_ASSETS/tsots-parallax/. Desks with an empty name get show=0
and no picture.

sx/sy are stored. Nothing animates them. Loop flags are for the
renderers: loop tiles with the view, no-loop stays pinned to the
top-left of the view.

Does not edit the camera, the atlas, or map.png.
"""
import json
import os
import sys

from PIL import Image

BOOK = os.path.normpath(os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "../../@.apps/piececraft-hq/pieces/system/maps/tsots"))


def find_data():
    base = "/home/no/Desktop"
    for root, dirs, files in os.walk(base):
        dirs[:] = [d for d in dirs if d not in ("img", "audio", "movies", "js", "node_modules", ".git")]
        if "Map004.json" in files and "Tilesets.json" in files and "RMMV_TSOTS" in root:
            return root
    raise SystemExit("TSOTS data not found")


def assets_dir():
    here = os.path.abspath(__file__)
    # walk up to the repo root that holds #.NNEST_ASSETS
    p = here
    for _ in range(12):
        p = os.path.dirname(p)
        cand = os.path.join(p, "#.NNEST_ASSETS")
        if os.path.isdir(cand):
            return os.path.join(cand, "tsots-parallax")
    raise SystemExit("#.NNEST_ASSETS not found")


def write_unique(src_png, dest_dir, name):
    os.makedirs(dest_dir, exist_ok=True)
    png = os.path.join(dest_dir, name + ".png")
    rgba = os.path.join(dest_dir, name + ".rgba")
    if not os.path.isfile(png):
        im = Image.open(src_png).convert("RGBA")
        im.save(png)
        with open(rgba, "wb") as f:
            f.write(im.tobytes())
        return im.size
    im = Image.open(png)
    return im.size


def link(desk, name, target):
    """Point desk/name at target with a relative symlink."""
    path = os.path.join(desk, name)
    rel = os.path.relpath(target, desk)
    if os.path.lexists(path):
        if os.path.islink(path) and os.readlink(path) == rel:
            return
        os.remove(path)
    os.symlink(rel, path)


def main():
    data = find_data()
    img_dir = os.path.join(os.path.dirname(data), "img", "parallaxes")
    store = assets_dir()
    show_n = 0
    for fn in sorted(os.listdir(data)):
        if not (fn.startswith("Map") and fn.endswith(".json") and fn != "MapInfos.json"):
            continue
        mid = int(fn[3:-5])  # Map004.json -> 4, Map1001.json -> 1001
        doc = json.load(open(os.path.join(data, fn), encoding="utf-8-sig"))
        name = (doc.get("parallaxName") or "").strip()
        desk = os.path.join(BOOK, "map%03d" % mid)
        if not os.path.isdir(desk):
            print("skip missing desk", mid)
            continue
        pdl = os.path.join(desk, "parallax.pdl")
        if not name or not doc.get("parallaxShow", True):
            with open(pdl, "w", encoding="utf-8") as f:
                f.write("PARALLAX | show | 0\nPARALLAX | name | \n")
            continue
        src = os.path.join(img_dir, name + ".png")
        if not os.path.isfile(src):
            raise SystemExit("missing parallax png " + src)
        w, h = write_unique(src, store, name)
        link(desk, "parallax.png", os.path.join(store, name + ".png"))
        link(desk, "parallax.rgba", os.path.join(store, name + ".rgba"))
        with open(pdl, "w", encoding="utf-8") as f:
            f.write(
                "PARALLAX | show | 1\n"
                "PARALLAX | name | %s\n"
                "PARALLAX | loop_x | %d\n"
                "PARALLAX | loop_y | %d\n"
                "PARALLAX | sx | %d\n"
                "PARALLAX | sy | %d\n"
                "PARALLAX | width | %d\n"
                "PARALLAX | height | %d\n"
                % (
                    name,
                    1 if doc.get("parallaxLoopX") else 0,
                    1 if doc.get("parallaxLoopY") else 0,
                    int(doc.get("parallaxSx") or 0),
                    int(doc.get("parallaxSy") or 0),
                    w, h,
                )
            )
        show_n += 1
        print("map%03d" % mid, name, w, h)
    print("desks with a picture", show_n)


if __name__ == "__main__":
    sys.exit(main() or 0)
