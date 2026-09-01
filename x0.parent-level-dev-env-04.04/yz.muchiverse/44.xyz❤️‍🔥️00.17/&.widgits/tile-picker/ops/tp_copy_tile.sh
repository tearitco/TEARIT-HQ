#!/bin/sh
# tp_copy_tile.sh <package_dir> <house_root> - real METHOD action for a
# placed tile's own "Copy" row (direct instruction 2026-09-01: "placed
# tiles dont have cancel/copy/paste/delete or events" - dev-only tiles,
# no retrofit needed for anything already placed before this).
#
# Writes this tile's own real package_dir into a single, shared,
# house-wide clipboard marker file (#.desktop/tile_clipboard.txt) -
# tp_paste_tile.sh reads it back. Last Copy wins, same "one shared
# slot" convention a real OS clipboard uses - no per-user/per-session
# scoping needed for a single-player desktop.
PKG="$1"
HOUSE="$2"
if [ -z "$PKG" ] || [ -z "$HOUSE" ]; then
    exit 1
fi
mkdir -p "$HOUSE/#.desktop"
printf '%s\n' "$PKG" > "$HOUSE/#.desktop/tile_clipboard.txt"
