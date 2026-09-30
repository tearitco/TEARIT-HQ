#!/bin/sh
# pchq_place_cell.sh <house> <x> <y> [glyph]
# Stamp one glyph into chunk_0_0_z0.txt. Glyph defaults to pc_brush
# label if present, else '.'.
set -eu
HOUSE="${1:?}"
X="${2:?}"
Y="${3:?}"
GLYPH="${4:-}"
PCHQ="$HOUSE/@.apps/piececraft-hq"
MAP="$PCHQ/pieces/system/chunks/chunk_0_0/chunk_0_0_z0.txt"
BRUSH="$HOUSE/&.widgits/palettes/state/pc_brush.txt"
if [ -z "$GLYPH" ] && [ -f "$BRUSH" ]; then
    # last path component of sprite dir, else first char of file
    GLYPH=$(basename "$(cat "$BRUSH")" | cut -c1)
fi
[ -n "$GLYPH" ] || GLYPH="."
[ -f "$MAP" ] || exit 1
# 0-based, 16-wide rows
python3 - "$MAP" "$X" "$Y" "$GLYPH" <<'PY'
import sys
path, xs, ys, g = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4][:1]
rows = open(path).read().splitlines()
if ys < 0 or ys >= len(rows): sys.exit(1)
row = list(rows[ys].ljust(16)[:16])
if xs < 0 or xs >= len(row): sys.exit(1)
row[xs] = g
rows[ys] = ''.join(row)
open(path, 'w').write('\n'.join(rows) + '\n')
print('placed', g, 'at', xs, ys)
PY
