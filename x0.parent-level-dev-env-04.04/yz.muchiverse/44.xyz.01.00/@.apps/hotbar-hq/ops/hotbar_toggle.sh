#!/bin/sh
# hotbar_toggle.sh <mode> <package_dir> - show/hide the hotbar (pc-hq board: the "hotbar" cell in its bottom bar).
# Flips <package_dir>/state/<mode>/visible.txt; hotbar_manager republishes hb_visible within ~0.4 s and the
# board's live reparse drops/re-adds the overlay.
D="$2/state/$1"; mkdir -p "$D"
case "$(sed -n 's/^visible=//p' "$D/visible.txt" 2>/dev/null)" in 0) v=1 ;; *) v=0 ;; esac
printf 'visible=%s\n' "$v" > "$D/visible.txt"
