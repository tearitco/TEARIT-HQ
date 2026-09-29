#!/usr/bin/env python3
"""csv_to_png_preview.py - real, reusable visual verification tool for
this house's own sprite.csv format (# resolution=N header, r,g,b,a rows,
same real format tp_asset_to_sprite.c/tp_rmmv_character_extract.c/
mr_monster_extract.c all write). Confirms an extraction op's real crop/
downscale looks right BEFORE spawning a live desktop window, without
needing a running X server for the check itself.

Usage: python3 csv_to_png_preview.py <sprite.csv> <output.png> [scale]
  scale: nearest-neighbor upscale factor for easier viewing (default 4)
"""
import sys
from PIL import Image


def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <sprite.csv> <output.png> [scale]", file=sys.stderr)
        return 1
    csv_path, out_path = sys.argv[1], sys.argv[2]
    scale = int(sys.argv[3]) if len(sys.argv) > 3 else 4

    n = None
    rows = []
    with open(csv_path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            if line.startswith("# resolution="):
                n = int(line.split("=", 1)[1])
                continue
            if line.startswith("#") or line.startswith("r,g,b,a"):
                continue
            r, g, b, a = map(int, line.split(","))
            rows.append((r, g, b, a))

    if n is None:
        # Fallback: infer a square resolution from row count.
        n = int(len(rows) ** 0.5)

    img = Image.new("RGBA", (n, n))
    img.putdata(rows)
    if scale > 1:
        img = img.resize((n * scale, n * scale), Image.NEAREST)
    img.save(out_path)
    print(f"saved {out_path} ({n}x{n} source, {scale}x preview)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
