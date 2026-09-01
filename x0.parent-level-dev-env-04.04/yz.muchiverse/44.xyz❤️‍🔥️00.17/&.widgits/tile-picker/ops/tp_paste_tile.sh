#!/bin/sh
# tp_paste_tile.sh <package_dir> <house_root> - real METHOD action for
# a placed tile's own "Paste" row (direct instruction 2026-09-01:
# "placed tiles dont have cancel/copy/paste/delete or events").
#
# Clones whatever tp_copy_tile.sh last recorded in the shared
# #.desktop/tile_clipboard.txt into a brand-new, real sibling package -
# fresh instance_id (a pasted tile is its own real entity, not a
# mirror), own desktop_pos.txt (one real grid cell offset from
# whichever tile Paste was actually clicked on, so it doesn't spawn
# exactly on top of it) - then spawns its own real live window, same
# spawn convention as every other entity in this house.
PKG="$1"
HOUSE="$2"

if [ -z "$PKG" ] || [ -z "$HOUSE" ]; then
    exit 1
fi

CLIP="$HOUSE/#.desktop/tile_clipboard.txt"
[ -f "$CLIP" ] || exit 0
SRC=$(head -n1 "$CLIP")
if [ -z "$SRC" ] || [ ! -d "$SRC" ]; then
    exit 0
fi

PARENT=$(dirname "$SRC")
BASE=$(basename "$SRC")
SUFFIX=$(tr -dc 'a-z0-9' < /dev/urandom 2>/dev/null | head -c5)
[ -z "$SUFFIX" ] && SUFFIX=$$
NEW="$PARENT/${BASE}_copy_${SUFFIX}"
[ -e "$NEW" ] && exit 0

cp -r "$SRC" "$NEW" 2>/dev/null || exit 1

# Fresh identity + a clean start - no inherited history/relay backlog.
tr -dc 'A-Z0-9' < /dev/urandom 2>/dev/null | head -c4 > "$NEW/instance_id.txt"
rm -f "$NEW/history.txt" "$NEW/interact_relay.txt"

# Real offset from PKG's own real position (the tile Paste was clicked
# on), not the copy source's - lands next to whichever tile the user
# actually right-clicked. 80px matches GRID_CELL_PX's own default.
PX=0
PY=0
if [ -f "$PKG/desktop_pos.txt" ]; then
    PX=$(awk -F= '/^x=/{print $2}' "$PKG/desktop_pos.txt")
    PY=$(awk -F= '/^y=/{print $2}' "$PKG/desktop_pos.txt")
fi
[ -z "$PX" ] && PX=0
[ -z "$PY" ] && PY=0
printf 'x=%d\ny=%d\n' "$((PX + 80))" "$((PY + 80))" > "$NEW/desktop_pos.txt"

setsid "$HOUSE/*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x" "$NEW" >/dev/null 2>&1 < /dev/null &
