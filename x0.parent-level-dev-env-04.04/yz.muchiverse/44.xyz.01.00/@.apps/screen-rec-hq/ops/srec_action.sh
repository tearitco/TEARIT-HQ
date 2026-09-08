#!/bin/sh
# srec_action.sh <cmd> <pkg_dir> <house_root>
# tiny shim: bump seq + write cmd to state/screen_rec_action.txt.
# dispatch() appends '<pkg_dir>' '<house_root>' as $2 $3.
CMD="${1:-}"
PKG="${2:-}"
[ -n "$PKG" ] || exit 0
mkdir -p "$PKG/state"
SEQF="$PKG/state/screen_rec.seq"
N=$(( $(cat "$SEQF" 2>/dev/null || echo 0) + 1 ))
echo "$N" > "$SEQF"
printf 'seq=%s\ncmd=%s\n' "$N" "$CMD" > "$PKG/state/screen_rec_action.txt"
