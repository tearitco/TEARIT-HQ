#!/bin/sh
# elem_click.sh <symbol> [pkg_dir] [house]  - periodic-table tile click.
#   <item action="'elem_click.sh' '${t.sym}'">  ->  $1=symbol $2=pkg $3=house
# Writes seq=/cmd=CLICK:<sym> for elements_palette_manager.+x's poll_action().
SYM="$1"; PKG="$2"
[ -n "$SYM" ] && [ -n "$PKG" ] || exit 0
mkdir -p "$PKG/state"
SEQF="$PKG/state/palettes-elements.seq"
N=$(( $(cat "$SEQF" 2>/dev/null || echo 0) + 1 )); echo "$N" > "$SEQF"
printf 'seq=%s\ncmd=CLICK:%s\n' "$N" "$SYM" > "$PKG/state/palettes-elements_action.txt"
