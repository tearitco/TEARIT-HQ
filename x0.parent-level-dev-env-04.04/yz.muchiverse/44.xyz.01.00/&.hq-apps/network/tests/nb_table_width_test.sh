#!/bin/bash
# nb_table_width_test.sh - pins TABLE COMPUTED WIDTHS (column width as
# computed layout state, not the equal split).
#
# Two hermetic halves, no running browser, no live-state pollution:
#
#   PART 1 (projector): the new manager binary runs against a SCRATCH
#   house holding a crafted page.state (two uniform tables, one ragged
#   table, a SEL row inside the first table's run). Asserts ui.txt:
#     - every row of a uniform run carries the SAME ccounts, byte-exact
#       ("3,35" and "5,5" - code-point maxima, hand-counted below)
#     - the SEL row does NOT split the run (both rows past it carry counts)
#     - ragged rows carry cells but NO ccounts (documented fallback)
#     - a leading empty cell keeps its delimiter (2 columns, not 1)
#   PART 2 (renderer): an own fixture window proves pixels:
#     - uniform table: column 2 starts at the SAME x on every row, and
#       that x sits far left of the equal-split position (computed, not
#       equal - the discriminator)
#     - the leading-empty row draws only column 2, aligned with the rest
#     - ragged table: per-row equal split, pixel-identical to before
#       (2-col rows at avail/2, the 3-col row grids in thirds)
#
# Negative-tested like nb_hover_test.sh: the same fixture WITHOUT
# ccounts must come out equal-split (proves the test discriminates),
# and a corrupt arity ("7,7,7" on 2-col rows) must fall back to equal.
#
# Wire format: ccounts="n0,n1,..." - per-column code-point maxima of
# the table's run, computed where the whole table is in memory (the
# projector). The draw path paints deserialized tmp Elems whose parent
# is NULL, so no sibling scan can ever work there - the maxima ride the
# row like cells= itself (template attr, Elem field, frame round-trip).
#
# Hand counts for the PART 1 fixture (code points, continuation bytes
# skipped, capped at 256 - the projector's own metric):
#   table 1 col 0: ID=2, 1=1, 222=3, empty=0, x=1 -> 3
#   table 1 col 1: 31, 5, 35, 24, 0 -> 35
#   table 2 col 0: Key=3, "a & b"=5 -> 5
#   table 2 col 1: Value=5, 1=1 -> 5
#
# Usage: sh nb_table_width_test.sh
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HR="$(cd "$HERE/../../.." && pwd)"
PKG="$HERE/.."
RENDER="$HR/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
MANAGER="$PKG/+x/network_browser_manager.+x"
WORK=/tmp/nb_table_width
FAIL=0
# Backstop: any failure path below exits mid-test past the explicit
# kills (each part still kills AND verifies its own procs on the
# green path - the trap is only for the red ones). Without this, a
# single failed run leaks a renderer and a manager per part.
KILLPIDS=""
trap 'for p in $KILLPIDS; do kill "$p" 2>/dev/null; done' EXIT

note() { printf '%s\n' "$*"; }
fail() { echo "FAIL: $*"; FAIL=1; }

[ -x "$RENDER" ] || { echo "FAIL: renderer not built ($RENDER)"; exit 1; }
[ -x "$MANAGER" ] || { echo "FAIL: manager not built ($MANAGER)"; exit 1; }
command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }

rm -rf "$WORK"; mkdir -p "$WORK"

########## PART 1: projector emits run maxima ##########
S="$WORK/house"; mkdir -p "$S/#.desktop" "$S/&.hq-apps/network/tmp"
cat > "$S/#.desktop/network_browser_page.state.txt" <<'EOF'
URL|file:///tmp/ct.html
TITLE|Counts test
TEXT|Intro paragraph here
TROW|1|ID|A quite long description column
TROW|0|1|short
TROW|0|222|a much longer description text here
SEL|dummy-selector
TROW|0||leading empty first cell
TROW|0|x|
TEXT|Second table
TROW|1|Key|Value
TROW|0|a & b|1
TEXT|Ragged table
TROW|0|aa|bb
TROW|0|c1|c2|c3
TROW|0|dd|ee
EOF
cp "$S/#.desktop/network_browser_page.state.txt" "$WORK/page.orig"

# The scratch manager reads the REAL package's module_parent.pid for
# its parent check (read-only) and bakes ops paths into strings it
# never executes here (no fetch, no JS, no media rows). Everything it
# WRITES lands under $S.
setsid "$MANAGER" "$S" "$PKG" ui >"$WORK/manager.log" 2>&1 &
MPID=$!
KILLPIDS="$KILLPIDS $MPID"
UI="$S/#.desktop/network-browser-hq_ui.txt"
got=0
for _ in $(seq 1 40); do
    if grep -q 'c_.*_ccounts=' "$UI" 2>/dev/null; then got=1; break; fi
    sleep 0.5
done
[ "$got" = 1 ] || { echo "FAIL: scratch manager produced no ccounts in 20s"; tail -5 "$WORK/manager.log"; kill $MPID 2>/dev/null; exit 1; }
sleep 1  # let the tick settle so the file is whole

C1="$(grep -h 'ccounts=' "$UI" | head -5)"
note "table1 counts lines: $(printf '%s' "$C1" | tr '\n' ' ')"
echo "$C1" | grep -q 'ccounts=3,35' || fail "table1 rows do not carry ccounts=3,35"
[ "$(printf '%s\n' "$C1" | sed 's/^c_[0-9]*_//' | sort -u | wc -l)" = 1 ] || fail "table1 runs disagree with each other"
[ "$(grep -c 'ccounts=3,35' "$UI")" = 5 ] || fail "table1 ccounts count is $(grep -c 'ccounts=3,35' "$UI"), want 5 (SEL split the run?)"
C2="$(grep -h 'ccounts=5,5' "$UI" | wc -l)"
[ "$C2" = 2 ] || fail "table2 ccounts=5,5 count is $C2, want 2"
# ragged rows: cells yes, counts no.
[ "$(grep -c '_cells=aa' "$UI")" = 1 ] || fail "ragged row aa missing its cells payload"
if grep -q 'ccounts=.*,.*,.*' "$UI"; then fail "a 3-number counts line exists - ragged run leaked widths"; fi
# leading-empty fix: c_5_cells must start with the \x1f delimiter.
python3 - "$UI" <<'PY' || exit 1
import sys
lines = open(sys.argv[1], 'rb').read().split(b'\n')
hits = [l for l in lines if l.startswith(b'c_5_cells=')]
assert len(hits) == 1, hits
assert hits[0] == b'c_5_cells=\x1fleading empty first cell', hits[0]
print("leading-empty delimiter: present")
PY
# the manager must not rewrite its own input.
cmp -s "$S/#.desktop/network_browser_page.state.txt" "$WORK/page.orig" \
    || fail "scratch manager rewrote page.state"

kill $MPID 2>/dev/null
sleep 1
if kill -0 $MPID 2>/dev/null; then fail "scratch manager still alive"; fi

########## PART 2: renderer pixels ##########
python3 - "$WORK/cols.xhtpm" <<'PY'
import sys
F = '\x1f'
def C(*cells): return F.join(cells)
def J(*cells): return ' \u00b7 '.join(c for c in cells if c)
A = [C('ID', 'A quite long description column'),
     C('1', 'short'),
     C('222', 'a much longer description text here'),
     C('', 'leading empty first cell'),
     C('x', '')]
CA = '3,35'  # same hand counts as PART 1
B = [C('aa', 'bb'), C('c1', 'c2', 'c3'), C('dd', 'ee')]
out = ['<!-- cols fixture. cells= is \\x1F-joined (empties significant);',
       '     ccounts= is the projector\'s "n0,n1,..." run maxima for the',
       '     uniform table. Table B is ragged on purpose and carries no',
       '     counts (documented equal-split fallback). -->',
       '<window label="ColFix" class="col-fixture database-window">',
       '  <page id="main">',
       '    <text class="plain" label="rows above the tables"/>']
for r in A:
    out.append('    <text class="trow" label="%s" cells="%s" ccounts="%s"/>' % (J(*r.split(F)), r, CA))
out.append('    <text class="plain" label="between the tables"/>')
for r in B:
    out.append('    <text class="trow" label="%s" cells="%s"/>' % (J(*r.split(F)), r))
out += ['    <text class="plain" label="rows below the tables"/>', '  </page>', '</window>', '']
open(sys.argv[1], 'w').write('\n'.join(out))
PY
cat > "$WORK/cols.css" <<'EOF'
window { width: 760px; height: 340px; }
window.col-fixture { width: 760px; height: 340px; background-color: #141414; color: #dddddd; }
.plain, .trow { color: #cccccc; }
EOF

setsid "$RENDER" "$HR" "$WORK/cols.xhtpm" "$WORK" >"$WORK/render.log" 2>&1 &
RPID=$!
KILLPIDS="$KILLPIDS $RPID"
sleep 6
if ! kill -0 "$RPID" 2>/dev/null; then
    echo "FAIL: fixture renderer died"; head -5 "$WORK/render.log"; exit 1
fi
HIST="$HR/#.desktop/entity_menu_history/$RPID.txt"
rm -f /tmp/entity-menu-frame.png
printf '112\n' >> "$HIST"
for _ in $(seq 1 20); do
    [ -f /tmp/entity-menu-frame.png ] && break
    sleep 0.5
done
[ -f /tmp/entity-menu-frame.png ] || { echo "FAIL: no frame dump"; kill $RPID 2>/dev/null; exit 1; }
sleep 1
cp /tmp/entity-menu-frame.png "$WORK/cols.png"
cp /tmp/entity-menu-frame.png.frame.txt "$WORK/cols.frame.txt"

# colstarts via widthscan.py (geometry from the ascii frame's own
# row boxes - no window size hardcoded).
python3 "$HERE/widthscan.py" "$WORK/cols.png" "$WORK/cols.frame.txt" | tee "$WORK/cols.scan"

python3 - "$WORK/cols.scan" <<'PY' || exit 1
import sys
rows = []
for line in open(sys.argv[1]):
    lab, x, ww, n, starts = line.rstrip('\n').split('|')
    rows.append((lab, int(x), int(ww), list(map(int, starts.split(','))) if starts else []))
A = rows[:5]
B = rows[5:]
assert len(A) == 5 and len(B) == 3, [r[0] for r in rows]
# uniform table: one shared column-2 x ...
c2 = [r[3][1] for r in A[:3]]
assert max(c2) - min(c2) <= 2, c2
# ... far left of the equal-split position (the discriminator).
for lab, x, ww, st in A[:3]:
    avail = (x + ww) - st[0] - 4
    assert st[1] - st[0] < avail // 2 - 100, (lab, st, avail)
print('uniform table: shared col2 x=%d, computed (not equal)' % c2[0])
# leading-empty row: only column 2 ink, aligned with the rest.
le = A[3][3]
assert len(le) == 1 and abs(le[0] - c2[0]) <= 2, le
print('leading-empty row: col2 aligned at x=%d' % le[0])
# trailing-empty row: only column 1 ink.
assert len(A[4][3]) == 1, A[4][3]
# ragged table: per-row equal split (the documented fallback).
for lab, x, ww, st in B:
    avail = (x + ww) - st[0] - 4
    n = len(st)
    for i in range(1, n):
        want = st[0] + avail // n * i
        assert abs(st[i] - want) <= 4, (lab, st, avail)
print('ragged table: equal-split fallback intact')
PY

kill $RPID 2>/dev/null
sleep 1
if kill -0 $RPID 2>/dev/null; then fail "fixture renderer still alive"; fi

########## PART 3 (negative): no counts -> equal everywhere ##########
python3 - "$WORK/nocounts.xhtpm" <<'PY'
import sys
F = '\x1f'
A = ['ID' + F + 'A quite long description column',
     '1' + F + 'short',
     '222' + F + 'a much longer description text here']
out = ['<window label="NoCounts" class="col-fixture database-window">', '  <page id="main">']
for r in A:
    out.append('    <text class="trow" label="x" cells="%s"/>' % r)
out += ['  </page>', '</window>', '']
open(sys.argv[1], 'w').write('\n'.join(out))
PY
cp "$WORK/cols.css" "$WORK/nocounts.css"
setsid "$RENDER" "$HR" "$WORK/nocounts.xhtpm" "$WORK" >"$WORK/render3.log" 2>&1 &
RPID=$!
KILLPIDS="$KILLPIDS $RPID"
sleep 6
rm -f /tmp/entity-menu-frame.png
printf '112\n' >> "$HR/#.desktop/entity_menu_history/$RPID.txt"
for _ in $(seq 1 20); do
    [ -f /tmp/entity-menu-frame.png ] && break
    sleep 0.5
done
cp /tmp/entity-menu-frame.png "$WORK/nocounts.png"
cp /tmp/entity-menu-frame.png.frame.txt "$WORK/nocounts.frame.txt"
python3 "$HERE/widthscan.py" "$WORK/nocounts.png" "$WORK/nocounts.frame.txt" | tee "$WORK/nocounts.scan"
python3 - "$WORK/nocounts.scan" <<'PY' || { echo "FAIL: no-counts rows are not equal-split"; exit 1; }
import sys
for line in open(sys.argv[1]):
    lab, x, ww, n, starts = line.rstrip('\n').split('|')
    x, ww = int(x), int(ww)
    st = list(map(int, starts.split(',')))
    assert len(st) == 2, (lab, st)
    avail = (x + ww) - st[0] - 4
    # without counts the grid is the plain equal split, exactly.
    assert abs(st[1] - (st[0] + avail // 2)) <= 3, (lab, st, avail)
print('no-counts control: equal split, as required')
PY
kill $RPID 2>/dev/null
sleep 1
if kill -0 $RPID 2>/dev/null; then fail "part-3 renderer still alive"; fi

########## PART 4 (negative): corrupt arity -> equal fallback ##########
python3 - "$WORK/badcounts.xhtpm" <<'PY'
import sys
F = '\x1f'
A = ['ID' + F + 'A quite long description column',
     '1' + F + 'short']
out = ['<window label="BadCounts" class="col-fixture database-window">', '  <page id="main">']
for r in A:
    # three maxima for two columns: stale/hostile, must be ignored.
    out.append('    <text class="trow" label="x" cells="%s" ccounts="7,7,7"/>' % r)
out += ['  </page>', '</window>', '']
open(sys.argv[1], 'w').write('\n'.join(out))
PY
cp "$WORK/cols.css" "$WORK/badcounts.css"
setsid "$RENDER" "$HR" "$WORK/badcounts.xhtpm" "$WORK" >"$WORK/render4.log" 2>&1 &
RPID=$!
KILLPIDS="$KILLPIDS $RPID"
sleep 6
rm -f /tmp/entity-menu-frame.png
printf '112\n' >> "$HR/#.desktop/entity_menu_history/$RPID.txt"
for _ in $(seq 1 20); do
    [ -f /tmp/entity-menu-frame.png ] && break
    sleep 0.5
done
cp /tmp/entity-menu-frame.png "$WORK/badcounts.png"
cp /tmp/entity-menu-frame.png.frame.txt "$WORK/badcounts.frame.txt"
python3 "$HERE/widthscan.py" "$WORK/badcounts.png" "$WORK/badcounts.frame.txt" | tee "$WORK/badcounts.scan"
python3 - "$WORK/badcounts.scan" <<'PY' || { echo "FAIL: corrupt counts did not fall back"; exit 1; }
import sys
for line in open(sys.argv[1]):
    lab, x, ww, n, starts = line.rstrip('\n').split('|')
    x, ww = int(x), int(ww)
    st = list(map(int, starts.split(',')))
    assert len(st) == 2, (lab, st)
    avail = (x + ww) - st[0] - 4
    assert abs(st[1] - (st[0] + avail // 2)) <= 3, (lab, st, avail)
print('corrupt-counts control: equal fallback, as required')
PY
kill $RPID 2>/dev/null
sleep 1
if kill -0 $RPID 2>/dev/null; then fail "part-4 renderer still alive"; fi

[ "$FAIL" -eq 0 ] && echo "nb_table_width_test.sh PASS"
exit $FAIL
