#!/bin/sh
CMD="${1:-}"; PKG="${2:-}"
[ -n "$PKG" ] || exit 0
mkdir -p "$PKG/state"
SEQF="$PKG/state/tile_editor.seq"
N=$(( $(cat "$SEQF" 2>/dev/null || echo 0) + 1 ))
echo "$N" > "$SEQF"
printf 'seq=%s\ncmd=%s\n' "$N" "$CMD" > "$PKG/state/tile_editor_action.txt"
