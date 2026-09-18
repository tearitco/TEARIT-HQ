#!/bin/sh
# Xdnd drop onto File Explorer: mv $DROP_PATH into current dir.
# $1=package_dir $2=house_root (same as drop_action convention).
set -u
PKG="${1:-}"
DROP="${DROP_PATH:-}"
[ -n "$PKG" ] && [ -n "$DROP" ] || exit 0
[ -e "$DROP" ] || exit 0
DIR=""
if [ -f "$PKG/file_explorer_ui.txt" ]; then
    DIR=$(grep '^dir=' "$PKG/file_explorer_ui.txt" | sed 's/^dir=//')
fi
[ -n "$DIR" ] && [ -d "$DIR" ] || exit 0
case "$DIR" in
    "$DROP"|"$DROP"/*) exit 0 ;;
esac
base=$(basename "$DROP")
[ "$DROP" = "$DIR/$base" ] && exit 0
mv "$DROP" "$DIR/$base"
