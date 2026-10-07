#!/bin/sh
# hotbar_toggle.sh <mode> <package_dir> [hide|show|toggle] - show/hide the hotbar (pc-hq board: the "hotbar" cell in its bottom bar).
# Flips <package_dir>/state/<mode>/visible.txt; hotbar_manager republishes hb_visible within ~0.4 s and the
# board's live reparse drops/re-adds the overlay.
# hotbar_toggle.sh <mode> <package_dir> [hide|show|toggle]   (default toggle). The overlay's "_" uses HIDE: a toggle there cancelled itself when the owner clicked twice
# before the first click showed (2026-10-06); hide/show are idempotent, so extra clicks are harmless.
D="$2/state/$1"; mkdir -p "$D"
case "${3:-toggle}" in
    hide) v=0 ;;
    show) v=1 ;;
    *) case "$(sed -n 's/^visible=//p' "$D/visible.txt" 2>/dev/null)" in 0) v=1 ;; *) v=0 ;; esac ;;
esac
printf 'visible=%s\n' "$v" > "$D/visible.txt"
