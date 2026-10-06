#!/bin/bash
# nb_layout_test.sh - Milestone-1 proof harness. Fetch a fixture page
# through the real manager, capture the resulting page.state.txt, diff
# against an expected snapshot. Rebuild expected with:
#   NB_SNAPSHOT_UPDATE=1 sh nb_layout_test.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
NDIR="$(cd "$HERE/.." && pwd)"
HR="$(cd "$NDIR/../.." && pwd)"
REQ="$HR/#.desktop/network_browser_request.txt"
PF="$HR/#.desktop/network_browser_page.state.txt"
FX="$HERE/fixtures/mini-article.html"
[ -f "$FX" ] || { echo "missing $FX" >&2; exit 1; }
printf "go:file://%s\n" "$FX" > "$REQ"
for i in $(seq 1 40); do
    grep -q "URL|file://$FX" "$PF" && break
    sleep 0.5
done
sleep 1
OUT="$(mktemp)"
cp "$PF" "$OUT"
if [ -n "${NB_SNAPSHOT_UPDATE:-}" ]; then mkdir -p "$HERE/snapshots"; cp "$OUT" "$HERE/snapshots/mini-article.state.txt"; echo "snapshot updated"
else
    if [ ! -f "$HERE/snapshots/mini-article.state.txt" ]; then echo "no snapshot yet - run NB_SNAPSHOT_UPDATE=1 once"; exit 2; fi
    diff -u "$HERE/snapshots/mini-article.state.txt" "$OUT" && echo "PASS"
fi
