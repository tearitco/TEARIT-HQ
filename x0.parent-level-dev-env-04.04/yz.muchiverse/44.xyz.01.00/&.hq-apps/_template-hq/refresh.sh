#!/bin/sh
# refresh.sh - the <module> backend. Loops, re-publishing state/ui.txt
# every REFRESH_SEC. The renderer SIGTERMs it on window close, so no
# guard is needed. `--once` publishes a single frame and exits (used by
# actions.sh for an instant update).
#
# argv when launched as a <module>:  <house_root> <package_dir>
# argv from actions.sh:              --once <package_dir>
set -u
ONCE=0
[ "${1:-}" = "--once" ] && { ONCE=1; shift; }
HOUSE_ROOT="${1:-${KHTPM_HOUSE:-}}"
PKG="${2:-${KHTPM_PKG:-}}"
[ -n "$PKG" ] || PKG=$(cd "$(dirname "$0")" && pwd)
[ "$ONCE" = 1 ] && PKG="${1:-$PKG}"      # --once passes pkg as $1
REFRESH_SEC="${TEMPLATE_REFRESH_SEC:-4}"

publish() {
    mkdir -p "$PKG/state"
    tick=$(cat "$PKG/state/counter.txt" 2>/dev/null || echo 0)
    last=$(cat "$PKG/state/last_action.txt" 2>/dev/null || echo "-")
    tmp="$PKG/state/ui.txt.tmp.$$"
    {
        echo "stamp=$(date '+%H:%M:%S')"
        echo "tick=$tick"
        echo "last_action=$last"
        # demo rows - a real backend would list real things here
        i=0
        while [ "$i" -lt "$tick" ] && [ "$i" -lt 12 ]; do
            echo "row_${i}_text=item $i"
            i=$((i + 1))
        done
        echo "rows_count=$i"
        [ "$i" -eq 0 ] && echo "no_rows=1" || echo "no_rows=0"
    } > "$tmp"
    mv -f "$tmp" "$PKG/state/ui.txt"
}

trap 'exit 0' TERM INT HUP
publish
[ "$ONCE" = 1 ] && exit 0
while :; do
    sleep "$REFRESH_SEC"
    publish
done
