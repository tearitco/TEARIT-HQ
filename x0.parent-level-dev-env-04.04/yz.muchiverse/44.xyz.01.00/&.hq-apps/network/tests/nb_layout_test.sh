#!/bin/bash
# nb_layout_test.sh - layout proof harness. Fetches each fixture page
# through the real manager, captures page.state.txt, diffs against an
# expected snapshot. Rebuild expected with:
#   NB_SNAPSHOT_UPDATE=1 sh nb_layout_test.sh
# Sprite dirs (mN) and absolute house paths are normalized before diff
# so snapshots are machine-independent and run-order stable.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
NDIR="$(cd "$HERE/.." && pwd)"
HR="$(cd "$NDIR/../.." && pwd)"
REQ="$HR/#.desktop/network_browser_request.txt"
PF="$HR/#.desktop/network_browser_page.state.txt"
norm() {
    sed -e "s|file://$HERE|FIXDIR|g" \
        -e "s|$HR|HOUSE|g" \
        -e "s|nb_sprites/m[0-9]*|nb_sprites/mN|g" \
        -e "s|/tmp/nb_img_[0-9a-fx]*\.png|TMPIMG|g" "$1"
}
FAIL=0
for FX in "$HERE"/fixtures/*.html; do
    BASE="$(basename "$FX" .html)"
    printf 'go:file://%s\n' "$FX" > "$REQ"
    for i in $(seq 1 60); do
        grep -q "URL|file://$FX" "$PF" 2>/dev/null && break
        sleep 0.5
    done
    sleep 2
    OUT="$(mktemp)"
    norm "$PF" > "$OUT"
    SNAP="$HERE/snapshots/$BASE.state.txt"
    if [ -n "${NB_SNAPSHOT_UPDATE:-}" ]; then
        mkdir -p "$HERE/snapshots"; cp "$OUT" "$SNAP"; echo "snapshot updated: $BASE"
    else
        if [ ! -f "$SNAP" ]; then echo "no snapshot for $BASE - run NB_SNAPSHOT_UPDATE=1 once"; FAIL=1; continue; fi
        if diff -q "$SNAP" "$OUT" >/dev/null; then echo "PASS: $BASE"
        else echo "FAIL: $BASE"; diff -u "$SNAP" "$OUT" | head -20; FAIL=1; fi
    fi
    rm -f "$OUT"
done
exit $FAIL
