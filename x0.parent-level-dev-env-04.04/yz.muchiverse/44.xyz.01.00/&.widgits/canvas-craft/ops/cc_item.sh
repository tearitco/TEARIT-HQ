#!/bin/sh
# cc_item.sh - Canvas-Craft action shim.
#   <item action="'cc_item.sh' 'SELECT <id>'">  ->  $1="SELECT <id>" $2=pkg $3=house
# Writes seq=/cmd= for canvascraft_manager.+x's poll_action().
LINE="$1"; PKG="$2"
case "$LINE" in
  "SELECT "*)  CMD="SELECT_RECIPE:${LINE#SELECT }" ;;
  CRAFT)       CMD="CRAFT" ;;
  *) exit 0 ;;
esac
SEQF="$PKG/canvas-craft.seq"
N=$(( $(cat "$SEQF" 2>/dev/null || echo 0) + 1 )); echo "$N" > "$SEQF"
printf 'seq=%s\ncmd=%s\n' "$N" "$CMD" > "$PKG/canvas-craft_action.txt"
