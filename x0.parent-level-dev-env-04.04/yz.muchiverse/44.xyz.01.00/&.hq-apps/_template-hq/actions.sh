#!/bin/sh
# actions.sh <verb> <package_dir>
# The xhtpm's <item action="sh '${PKG}/actions.sh' <verb> '${PKG}'"/>
# rows call this. A real app would instead append seq=/cmd= to a
# <name>_action.txt that its compiled manager polls (see
# X11-HQ-APP-DESIGN-WISDOMS.md #1). This trivial demo just mutates a
# counter file the refresh loop folds into state/ui.txt.
set -u
VERB="${1:-}"
PKG="${2:-$(cd "$(dirname "$0")" && pwd)}"
mkdir -p "$PKG/state"
CNT="$PKG/state/counter.txt"
n=$(cat "$CNT" 2>/dev/null || echo 0)
case "$VERB" in
    bump)  n=$((n + 1)) ;;
    reset) n=0 ;;
    *)     echo "actions.sh: unknown verb '$VERB'" >&2; exit 2 ;;
esac
printf '%s\n' "$n" > "$CNT"
printf '%s\n' "$VERB" > "$PKG/state/last_action.txt"
# refresh the view immediately (don't wait for the poll tick)
sh "$PKG/refresh.sh" --once "$PKG" >/dev/null 2>&1 || true
