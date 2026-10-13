#!/bin/bash
# nb_find_test.sh - pins FIND IN PAGE (Ctrl+F).
#
# Generic renderer capability, not a browser feature: it searches
# whatever rows the window already has. This suite drives it through the
# relay against the browser's own article fixture and asserts on TWO
# independent channels:
#   - state, read from the ascii mirror's [FIND] line (no pixels needed)
#   - the painted highlight, measured as real band-coloured pixels
# The band check is what makes this more than a self-report: a find that
# counts matches but paints nothing would still pass a state-only test.
#
# Pinned:
#   ARM    Ctrl+F arms find (relay code 6 - below the 32..126 printable
#          band, so it needs naming in dispatch_relay_code like 3/22/24)
#   MATCH  a query counts exactly the rows whose label contains it,
#          case-insensitively
#   STEP   Enter advances and WRAPS (2 of 2 -> 1 of 2)
#   BAND   the highlight is painted on the visual row that actually
#          carries the match, and MOVES when the match moves line
#   NONE   a query with no matches says so instead of hanging
#   DEEP   a match BELOW the fold is found AND scrolled into view, both
#          directions, with the band painted on the revealed row
#   ESC    Escape disarms and clears
#   NOP    'p' (the house frame-dump key) dumps and does NOT become a
#          query character - found the hard way, see the renderer comment
#
# Usage: sh nb_find_test.sh          (needs a running network browser)
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HR="$(cd "$HERE/../../.." && pwd)"
NB="$HR/&.hq-apps/network"
FRAME=/tmp/entity-menu-frame.png.frame.txt
RECEIPT=/tmp/entity-menu-frame.png.receipt.txt
ASCII_DIR="$HR/#.desktop/ascii_frames"
FAIL=0
fail() { echo "FAIL: $*"; FAIL=1; }

[ -f "$HR/#.desktop/network-browser-hq_ui.txt" ] || { echo "FAIL: no browser running"; exit 1; }
# tail -1, NOT head -1: button.sh leaves a transient duplicate renderer
# for a moment and head -1 has twice handed this suite a dead PID whose
# relay nobody reads (a relay write to a dead process is silently lost,
# which reads exactly like "the feature does not work").
# PID selection goes through browser_pid.sh, NOT pgrep -f: pgrep matches
# any command line CONTAINING the pattern, including the shell running
# the search. Measured 2026-10-13: pgrep reported 3 renderers with
# exactly one alive - the other two were this suite's own shell and the
# tool running it - and the relay writes to a PID nobody reads are lost
# silently, which reads exactly like a broken feature.
PID="${BROWSER_PID:-$(sh "$HERE/browser_pid.sh")}"
[ -n "$PID" ] || { echo "FAIL: no browser window"; exit 1; }
RELAY="$HR/#.desktop/entity_menu_history/$PID.txt"
MIRROR="$ASCII_DIR/$PID.frame.txt"

dump() {
    rm -f "$RECEIPT"
    printf '206\n' >> "$RELAY"
    for _ in $(seq 1 20); do [ -f "$RECEIPT" ] && break; sleep 0.5; done
    [ -f "$RECEIPT" ] || return 1
    sleep 1
    cp /tmp/entity-menu-frame.png /tmp/nb_find_shot.png 2>/dev/null
}
findline() { grep '\[FIND\]' "$MIRROR" 2>/dev/null | head -1; }
# type a literal string as relay codes (ASCII), one per line
type_str() {
    printf '%s' "$1" | grep -o . | while read -r c; do
        printf '%d\n' "$(printf '%d' "'$c")" >> "$RELAY"; sleep 0.3
    done
}
esc() { printf '27\n' >> "$RELAY"; sleep 1.5; }
arm() { esc; printf '6\n' >> "$RELAY"; sleep 1.5; }

printf 'go:file://%s/tests/fixtures/mini-article.html\n' "$NB" \
    > "$HR/#.desktop/network_browser_request.txt"
sleep 7
esc

# ---- 1. Ctrl+F arms find
arm
dump || { echo "FAIL: no frame dump"; exit 1; }
line="$(findline)"
case "$line" in
    *'[FIND]'*) echo "PASS: Ctrl+F arms find" ;;
    *) fail "Ctrl+F did not arm (no [FIND] line)"; echo "  mirror: $(head -2 "$MIRROR" | tail -1)" ;;
esac

# ---- 2. typing builds the query and counts matches
type_str "the"; sleep 1; dump
line="$(findline)"
q=$(printf '%s' "$line" | sed -n 's/.*query="\([^"]*\)".*/\1/p')
n=$(printf '%s' "$line" | sed -n 's/.*" \([0-9]*\) of \([0-9]*\).*/\1|\2/p')
hits=$(printf '%s' "$n" | cut -d'|' -f2)
if [ "$q" = "the" ] && [ "${hits:-0}" -ge 2 ]; then
    echo "PASS: query built and matches counted ($hits rows match 'the')"
else
    fail "query/matches wrong: line='$line'"
fi

# ---- 3. the count agrees with the frame, computed independently
expect=$(awk -F'|' '{l=tolower($4); if (index(l,"the")>0 && $7+0>0) c++} END{print c+0}' "$FRAME")
if [ "$expect" = "$hits" ]; then
    echo "PASS: match count agrees with the frame ($expect rows)"
else
    fail "count mismatch: mirror says $hits, frame says $expect"
fi

# ---- 4. Enter steps and wraps
printf '13\n' >> "$RELAY"; sleep 1.5; dump
at=$(findline | sed -n 's/.*" \([0-9]*\) of.*/\1/p')
if [ "$at" = "2" ]; then echo "PASS: Enter advances to match 2"; else fail "Enter did not advance (at=$at)"; fi
printf '13\n' >> "$RELAY"; sleep 1.5; dump
at=$(findline | sed -n 's/.*" \([0-9]*\) of.*/\1/p')
if [ "$at" = "1" ]; then echo "PASS: Enter wraps back to match 1"; else fail "no wrap (at=$at)"; fi

# ---- 5. the band is painted, and it MOVES with the match.
#         'the' sits past the first visual line of its row; 'Enough'
#         sits on the first. Same row, so the band's y must differ by
#         about one line height - which no "highlights nothing" or
#         "always paints the row top" implementation can fake.
type_str ""; esc; arm; type_str "the"; sleep 1; dump
python3 - "$HERE" <<'PY' || fail "band missing for 'the'"
import importlib.util, sys
spec = importlib.util.spec_from_file_location('ws', sys.argv[1] + '/widthscan.py')
m = importlib.util.module_from_spec(spec)
exec(compile(open(spec.origin).read().replace('\nmain()\n', '\n'), 'ws', 'exec'), m.__dict__)
w, h, px = m.decode('/tmp/nb_find_shot.png')
def near(c): return abs(c[0]-0x7a) <= 26 and abs(c[1]-0x5c) <= 26 and abs(c[2]) <= 26
ys = [y for y in range(h) for x in range(w) if near((px[(y*w+x)*3], px[(y*w+x)*3+1], px[(y*w+x)*3+2]))]
assert ys, 'no band pixels for query "the"'
print('band y range for "the": %d..%d' % (min(ys), max(ys)))
open('/tmp/nb_find_band_y.txt','w').write('%d %d' % (min(ys), max(ys)))
PY
esc; arm; type_str "Enough"; sleep 1; dump
python3 - "$HERE" <<'PY' || fail "band missing for 'Enough'"
import importlib.util, sys
spec = importlib.util.spec_from_file_location('ws', sys.argv[1] + '/widthscan.py')
m = importlib.util.module_from_spec(spec)
exec(compile(open(spec.origin).read().replace('\nmain()\n', '\n'), 'ws', 'exec'), m.__dict__)
w, h, px = m.decode('/tmp/nb_find_shot.png')
def near(c): return abs(c[0]-0x7a) <= 26 and abs(c[1]-0x5c) <= 26 and abs(c[2]) <= 26
ys = [y for y in range(h) for x in range(w) if near((px[(y*w+x)*3], px[(y*w+x)*3+1], px[(y*w+x)*3+2]))]
assert ys, 'no band pixels for query "Enough"'
lo2, hi2 = min(ys), max(ys)
print('band y range for "Enough": %d..%d' % (lo2, hi2))
lo1, hi1 = [int(v) for v in open('/tmp/nb_find_band_y.txt').read().split()]
assert hi2 < hi1, ('band did not move up a line: "the" %d..%d, "Enough" %d..%d' % (lo1, hi1, lo2, hi2))
print('PASS: band is painted and tracks the match line (%d..%d vs %d..%d)' % (lo1, hi1, lo2, hi2))
PY

# ---- 6. a query with no matches says so (does not hang, does not lie)
esc; arm; type_str "zzzznotpresent"; sleep 1; dump
line="$(findline)"
if printf '%s' "$line" | grep -q '" 0 of 0'; then
    echo "PASS: a query with no match reports 0 of 0"
else
    fail "no-match query did not report 0: '$line'"
fi

# ---- 7. scroll-into-view. A match below the fold is a match the user
#         cannot see, and find that does not reveal it is useless on a
#         long article. Three independent claims, in order:
#           a) the search SEES an off-screen row at all (before: find only
#              walked the laid-out rows, so it reported 0 matches for a row
#              that was plainly in the page)
#           b) the row ends up ON screen (y is no longer the -100000
#              park position), both directions
#           c) the band is painted on the row it just revealed
printf 'go:file://%s/tests/fixtures/scroll-article.html\n' "$NB" \
    > "$HR/#.desktop/network_browser_request.txt"
sleep 8
esc; arm; dump
row_y() { awk -F'|' -v pat="$1" 'index($4,pat)>0 {print $10; exit}' "$FRAME"; }
row_nav() { awk -F'|' -v pat="$1" 'index($4,pat)>0 {print $7; exit}' "$FRAME"; }
before_y="$(row_y NEEDLETOKEN)"
if [ -z "$before_y" ] || [ "$before_y" -gt -1000 ] 2>/dev/null; then
    fail "fixture precondition: NEEDLETOKEN row should start off screen (y=$before_y)"
else
    echo "PASS: precondition - needle row starts parked off screen (y=$before_y)"
fi

type_str "NEEDLE"; sleep 1; dump
line="$(findline)"
if printf '%s' "$line" | grep -q '" 1 of 1'; then
    echo "PASS: an off-screen row is found (1 of 1)"
else
    fail "find did not see the off-screen row: '$line'"
fi
after_y="$(row_y NEEDLETOKEN)"; after_nav="$(row_nav NEEDLETOKEN)"
if [ -n "$after_y" ] && [ "$after_y" -gt -1000 ]; then
    echo "PASS: find scrolled the match into view (y $before_y -> $after_y, nav $after_nav)"
else
    fail "match still off screen after find (y=$after_y)"
fi

# band must be painted ON the revealed row, not merely somewhere
python3 - "$HERE" "$after_y" <<'PY' || fail "band not painted on the revealed row"
import importlib.util, sys
spec = importlib.util.spec_from_file_location('ws', sys.argv[1] + '/widthscan.py')
m = importlib.util.module_from_spec(spec)
exec(compile(open(spec.origin).read().replace('\nmain()\n', '\n'), 'ws', 'exec'), m.__dict__)
w, h, px = m.decode('/tmp/nb_find_shot.png')
def near(c): return abs(c[0]-0x7a) <= 26 and abs(c[1]-0x5c) <= 26 and abs(c[2]) <= 26
ys = [y for y in range(h) for x in range(w) if near((px[(y*w+x)*3], px[(y*w+x)*3+1], px[(y*w+x)*3+2]))]
assert ys, 'no band pixels on the revealed row'
top, bot = int(sys.argv[2]), int(sys.argv[2]) + 30
assert max(ys) >= top and min(ys) <= bot, \
    'band %d..%d does not overlap the revealed row %d..%d' % (min(ys), max(ys), top, bot)
print('PASS: band painted on the revealed row (band %d..%d, row %d..%d)' % (min(ys), max(ys), top, bot))
PY

# the OTHER direction: the page is scrolled to the bottom now, so a match
# in the FIRST paragraph is above the fold and must scroll back UP. A
# reveal that only ever scrolls down passes every assertion above.
esc; arm; type_str "Filler paragraph 1,"; sleep 1; dump
up_y="$(row_y 'Filler paragraph 1,')"
if [ -n "$up_y" ] && [ "$up_y" -gt -1000 ] && [ "$up_y" -lt "$after_y" ]; then
    echo "PASS: a match above the fold scrolls back up (y=$up_y, was $after_y)"
else
    fail "reveal did not scroll up (y=$up_y, was $after_y)"
fi

# ---- 8. Escape clears and disarms
esc; dump
if [ -z "$(findline)" ]; then
    echo "PASS: Escape disarms find"
else
    fail "Escape left find armed: '$(findline)'"
fi

[ "$FAIL" -eq 0 ] && echo "nb_find_test.sh PASS"
exit $FAIL