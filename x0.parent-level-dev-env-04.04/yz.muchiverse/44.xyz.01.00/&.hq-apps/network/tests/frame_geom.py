#!/usr/bin/env python3
"""frame_geom.py - read geometry out of a khtpm .frame.txt element dump.

WHY THIS EXISTS (2026-10-14, G2.2 groundwork)

G2.2 compares the WORKER's box tree against the RENDERER's painted
geometry. Doing that means reading x/y/w/h out of the renderer's
serialised Elem tree, and the first attempt at that produced a confident,
completely fictional result: it guessed field positions 8-11 and returned
the SIDEBAR columns (cx1..cx6 - every number plausible, all of them the
wrong numbers).

The format is emitted by khtpm_core_render.c (the frame serializer), and
positional guessing is unsafe because the label field may contain newlines
and runs of empty fields, which shifts every numeric column after it.

So this parser does NOT guess. The format is FIXED at 29 pipe-separated
fields (the emitter's own fprintf has 29 conversions), with x/y/w/h at
positions 9-12 from the left. We therefore count from the END, where the
field count is known and cannot drift, and we VERIFY the count is exactly
29 - a row that does not have 29 fields is reported as unparsed rather
than silently mis-read. Failing loudly is the entire point.

    python3 frame_geom.py <file.frame.txt> [--field class] [--limit N]

Prints one line per element: index, tag, id, class, x, y, w, h.
"""
import sys


# Field order as emitted by khtpm_core_render.c's frame serializer:
#   1 tag   2 id   3 classes   4 label   5 sprite   6 onclick
#   7 nav_index   8 active   9 x   10 y   11 w   12 h
#  13 target_id  14 input_buffer  15 relay  16 bg  17 cursor
#  18 text_area  19..21 grid_cur_row/col/edit_mode  22..24 grid_jump/cell
#  25 sel_anchor  26 segments  27 cells  28 ccounts  29 bar_value
#  30 bar_max
# (the emitter prints 29 conversions plus the trailing newline; the count
# below is asserted against what actually lands in the file, not assumed)
TOTAL_FIELDS = 29
# x, y, w, h are fields 9..12 from the left, i.e. these offsets from the
# right end of a TOTAL_FIELDS-wide row.
OFF_X = -21
OFF_Y = -20
OFF_W = -19
OFF_H = -18


def parse_line(line):
    """One row -> dict, or None if the row is not the expected shape.

    Returns None rather than guessing when the field count is wrong. That
    refusal is the feature: a mis-parsed geometry number is worse than no
    number, because it compares confidently against the other side.
    """
    if not line.strip():
        return None
    f = line.rstrip("\n").split("|")
    if len(f) != TOTAL_FIELDS:
        return None
    try:
        return {
            "tag": f[0],
            "id": f[1],
            "class": f[2],
            "label": f[3],
            "x": int(f[8]), "y": int(f[9]),
            "w": int(f[10]), "h": int(f[11]),
        }
    except ValueError:
        return None


def read(path):
    rows, skipped = [], 0
    with open(path, "r", errors="replace") as fh:
        for line in fh:
            if not line.strip():
                continue
            r = parse_line(line)
            if r is None:
                skipped += 1
                continue
            rows.append(r)
    return rows, skipped


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 2
    path = args[0]
    want_class = None
    limit = 0
    if "--field" in args:
        want_class = args[args.index("--field") + 1]
    if "--limit" in args:
        limit = int(args[args.index("--limit") + 1])

    rows, skipped = read(path)
    print("# %s: %d rows parsed, %d SKIPPED (wrong field count)" % (path, len(rows), skipped))
    if skipped:
        print("# skipped rows are refused, not guessed - fix the parser, do not relax it")

    shown = 0
    for i, r in enumerate(rows):
        if want_class and want_class not in r["class"]:
            continue
        print("%4d %-10s %-10s %-18s x=%-5d y=%-5d w=%-5d h=%-5d"
              % (i, r["tag"], r["id"][:10], r["class"][:18],
                 r["x"], r["y"], r["w"], r["h"]))
        shown += 1
        if limit and shown >= limit:
            break
    return 0


if __name__ == "__main__":
    sys.exit(main())