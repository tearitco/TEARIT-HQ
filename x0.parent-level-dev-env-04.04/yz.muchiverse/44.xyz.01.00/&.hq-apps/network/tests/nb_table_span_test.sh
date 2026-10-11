#!/bin/bash
# nb_table_span_test.sh - pins TABLE SPANS (colspan/rowspan honored).
#
# The extractor expands spans at fetch time, so every emitted TROW row
# is already grid-aligned and the projector/renderer need no cross-row
# state: colspan pads with empties (text stays in the first slot -
# repeating it would read twice in labels and copies), rowspan repeats
# its text into the rows below (each row keeps its context). Genuinely
# ragged rows (missing cells, no spans) still emit short and keep the
# equal-split fallback downstream - honest, not faked.
#
# Hermetic: a scratch house runs the real manager binary against the
# committed fixture (tests/fixtures/table-span/table-span.html - a
# SUBDIRECTORY on purpose, so nb_layout_test.sh's fixtures/*.html glob
# never picks it up: its page.state needs the NEW extractor, and a
# snapshot cut from the long-running old-binary browser would go stale
# on relaunch; the byte-exact expects below are the golden instead).
# The go:file fetch drives the true pipeline (fetch -> extract ->
# project), not a hand-fed page.state.
#
# Fixture covers: header colspan=2, body rowspan=2, colspan=0 (clamps
# to 1), an attribute VALUE containing the word (quote-aware parsing),
# a caption-separated second table, and a ragged control table.
#
# Asserts on ui.txt: table-1 rows carry identical ccounts=10,5
# (hand-counted below), cells payloads byte-exact (header keeps its
# trailing empty), ragged rows carry cells but NO counts.
#
# Hand counts for table 1 (code points): col 0 "Price plan"=10,
# "Group"=5 x2, "Zero span"=9, "Quoted"=6 -> 10; col 1 ""=0, "Free"=4,
# "Pro"=3, "After"=5, "Ok"=2 -> 5.
#
# Negative: the same fixture with colspan="abc" makes the header
# single-column, the run goes ragged, and NO row carries counts (junk
# degrades to the old grid, never to garbage).
#
# Usage: sh nb_table_span_test.sh
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HR="$(cd "$HERE/../../.." && pwd)"
PKG="$HERE/.."
MANAGER="$PKG/+x/network_browser_manager.+x"
WORK=/tmp/nb_table_span
FAIL=0

note() { printf '%s\n' "$*"; }
fail() { echo "FAIL: $*"; FAIL=1; }

[ -x "$MANAGER" ] || { echo "FAIL: manager not built ($MANAGER)"; exit 1; }
command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }

rm -rf "$WORK"; mkdir -p "$WORK"
KILLPIDS=""
trap 'for p in $KILLPIDS; do kill "$p" 2>/dev/null; done' EXIT

run_case() {  # run_case <name> <fixture> : project it, leave UI at $WORK/ui_<name>.txt
    S="$WORK/house_$1"; rm -rf "$S"; mkdir -p "$S/#.desktop" "$S/&.hq-apps/network/tmp"
    setsid "$MANAGER" "$S" "$PKG" ui >"$WORK/manager_$1.log" 2>&1 &
    MPID=$!
    KILLPIDS="$KILLPIDS $MPID"
    sleep 2
    printf 'go:file://%s\n' "$2" > "$S/#.desktop/network_browser_request.txt"
    UI="$S/#.desktop/network-browser-hq_ui.txt"
    for _ in $(seq 1 40); do
        if grep -q 'c_.*_ccounts=\|content_count=' "$UI" 2>/dev/null; then break; fi
        sleep 0.5
    done
    sleep 1
    cp "$UI" "$WORK/ui_$1.txt" 2>/dev/null
    cp "$S/#.desktop/network_browser_page.state.txt" "$WORK/page_$1.txt" 2>/dev/null
    kill $MPID 2>/dev/null
}

# page.state shape: expanded rows, captions kept, ragged table intact.
run_case main "$HERE/fixtures/table-span/table-span.html"
python3 - "$WORK/page_main.txt" <<'PY' || exit 1
import sys
rows = [l.rstrip('\n') for l in open(sys.argv[1]) if l.startswith('TROW|')]
want = ['TROW|1|Price plan|',
        'TROW|0|Group|Free',
        'TROW|0|Group|Pro',
        'TROW|0|Zero span|After',
        'TROW|0|Quoted|Ok',
        'TROW|1|Key|Value',
        'TROW|0|a|1|EXTRA',
        'TROW|0|b|2']
assert rows == want, rows
print('page.state rows: expanded exactly')
PY
# table 1: one shared grid (identical counts on all five rows).
C1="$(grep -h 'ccounts=' "$WORK/ui_main.txt")"
note "table1 counts: $(printf '%s' "$C1" | tr '\n' ' ')"
echo "$C1" | grep -q 'ccounts=10,5' || fail "table1 rows do not carry ccounts=10,5"
[ "$(printf '%s\n' "$C1" | sed 's/^c_[0-9]*_//' | sort -u | wc -l)" = 1 ] || fail "table1 runs disagree"
[ "$(grep -c 'ccounts=10,5' "$WORK/ui_main.txt")" = 5 ] || fail "table1 count is not 5"
# header cells payload keeps its trailing empty (2 columns, not 1).
python3 - "$WORK/ui_main.txt" <<'PY' || exit 1
import sys
lines = open(sys.argv[1], 'rb').read().split(b'\n')
hits = [l for l in lines if l.startswith(b'c_') and b'_cells=Price plan' in l]
assert len(hits) == 1 and hits[0].endswith(b'Price plan\x1f'), hits
print('header cells: positional, trailing empty kept')
PY
# ragged table: cells yes, counts no (documented fallback).
python3 - "$WORK/ui_main.txt" <<'PY' || exit 1
import sys
lines = open(sys.argv[1], 'rb').read().split(b'\n')
cells = [l for l in lines
         if b'_cells=' in l and (b'a\x1f1\x1fEXTRA' in l or l.endswith(b'b\x1f2'))]
assert len(cells) == 2, cells
counts = [l for l in lines if b'ccounts=' in l]
assert counts and all(l.count(b',') == 1 for l in counts), counts
print('ragged table: cells exact, all counts 2-column (fallback intact)')
PY

# Negative: junk colspan degrades the whole run to the old grid.
sed 's/colspan="2"/colspan="abc"/' "$HERE/fixtures/table-span/table-span.html" > "$WORK/junk.html"
run_case junk "$WORK/junk.html"
if grep -q 'ccounts=' "$WORK/ui_junk.txt"; then
    fail "junk colspan still produced counts"
else
    note "junk colspan: no counts anywhere, honest fallback"
fi
grep -qx 'TROW|1|Price plan' "$WORK/page_junk.txt" \
    && note "junk header fell back to a single cell" \
    || fail "junk header row missing"

for p in $KILLPIDS; do
    if kill -0 "$p" 2>/dev/null; then fail "scratch manager $p still alive"; fi
done

[ "$FAIL" -eq 0 ] && echo "nb_table_span_test.sh PASS"
exit $FAIL
