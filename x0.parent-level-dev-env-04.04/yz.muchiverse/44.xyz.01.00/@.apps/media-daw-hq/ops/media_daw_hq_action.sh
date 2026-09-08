#!/bin/sh
# media_daw_hq_action.sh <cmd> <pkg_dir> <house_root>
CMD="${1:-}"; PKG="${2:-}"
[ -n "$PKG" ] || exit 0
mkdir -p "$PKG/state"
SEQF="$PKG/state/media_daw_hq.seq"
N=$(( $(cat "$SEQF" 2>/dev/null || echo 0) + 1 ))
echo "$N" > "$SEQF"
printf 'seq=%s\ncmd=%s\n' "$N" "$CMD" > "$PKG/state/media_daw_hq_action.txt"
