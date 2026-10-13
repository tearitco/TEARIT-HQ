#!/bin/bash
# nb_span_digit_test.sh - pins INLINE SPAN step-9 digit slots and step-12
# dot-addressing.
#
# Digits address ROWS - always - unless the segment cursor is already
# active on the focused row (entered with Right), in which case digits
# address that row's LINKS: 1..n jumps the cursor, 0 drops back to
# row-level like Escape, out-of-range digits hold the cursor (an
# overshoot must not yank focus to another row). Consumed digits reset
# the row accumulator so they cannot leak into a later row number.
#
# A '.' arms link entry for the very next digit ("5.2" addresses row
# 5's link 2 with no Right entry): one-shot, voided by any other key,
# miss-safe (ignored, never a row jump), single digits only.
#
# Hermetic: own fixture window (an item row with an action, a 3-link
# segments row, a plain row), actions append to click.log - arrival of
# the right action proves the whole chain (digit focus, cursor select,
# Enter dispatch). Nav numbers are READ from the ascii frame, never
# hardcoded (the row-2-vs-20 lesson: hardcoded navs pass spuriously).
#
# Steps (Esc hygiene first - drops stale cursors and resets accum):
#   A. <nav-r1> Enter            -> ROW1   (digits still address rows)
#   B. <nav-r2> Right 3 Enter    -> LINK3  (direct select, no walking)
#   C. <nav-r2> Right 1 9 Enter  -> LINK1  (overshoot holds)
#   D. <nav-r2> Right 2 0 Enter  -> nothing (0 drops; text-row Enter is a no-op)
#   E. <nav-r2> . 2 Enter        -> LINK2  (dot-addressing, no entry)
#   F. <nav-r2> . 9 Enter        -> nothing (miss safe); <nav-r1> Enter
#      -> ROW1 (disarmed, accumulator fresh)
#   G. <nav-r1> . 2 Enter        -> ROW1 (dot on a plain row harmless)
#
# Usage: sh nb_span_digit_test.sh
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HR="$(cd "$HERE/../../.." && pwd)"
RENDER="$HR/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
WORK=/tmp/nb_span_digit
FAIL=0

note() { printf '%s\n' "$*"; }
fail() { echo "FAIL: $*"; FAIL=1; }

[ -x "$RENDER" ] || { echo "FAIL: renderer not built ($RENDER)"; exit 1; }
command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }

rm -rf "$WORK"; mkdir -p "$WORK"
KILLPIDS=""
trap 'for p in $KILLPIDS; do kill "$p" 2>/dev/null; done' EXIT

cat > "$WORK/action.sh" <<EOF
#!/bin/sh
echo "CLICKED \$1" >> "$WORK/click.log"
EOF
chmod +x "$WORK/action.sh"
# Segments wire format: kind \x1F text \x1F url \x1F action, joined by
# \x1E; text spans keep the trailing empty url field. Actions are BARE
# commands (dispatch wraps in sh '...' itself - a `sh '...'` prefix
# double-wraps into broken quoting and dies silently).
python3 - "$WORK/segdig.xhtpm" "$WORK/action.sh" <<'PY'
import sys
FS, J = '\x1f', '\x1e'
def T(t): return 'text' + FS + t + FS
def L(t, a): return 'link' + FS + t + FS + 'https://example.com/' + t + FS + a
A = "'" + sys.argv[2] + "'"
segs = J.join([T('go to '), L('ONE', A + " 'LINK1'"), T(' then '),
               L('TWO', A + " 'LINK2'"), T(' then '),
               L('THREE', A + " 'LINK3'"), T(' now')])
doc = ('<window label="SegDig" class="segdig-fixture database-window">\n'
       '  <page id="main">\n'
       '    <text class="plain" label="plain row above"/>\n'
       '    <item id="r1" class="plain" label="plain row one" action="%s \'ROW1\'"/>\n'
       '    <text id="r2" class="seg" label="go to ONE then TWO then THREE now" segments="%s"/>\n'
       '    <text class="plain" label="plain row below"/>\n'
       '  </page>\n</window>\n' % (A, segs))
open(sys.argv[1], 'w').write(doc)
PY
cat > "$WORK/segdig.css" <<'EOF'
window { width: 760px; height: 260px; }
window.segdig-fixture { width: 760px; height: 260px; background-color: #141414; color: #dddddd; }
.plain, .seg { color: #cccccc; }
EOF

setsid "$RENDER" "$HR" "$WORK/segdig.xhtpm" "$WORK" >"$WORK/render.log" 2>&1 &
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
[ -f /tmp/entity-menu-frame.png ] || { echo "FAIL: no frame dump"; exit 1; }
sleep 1
cp /tmp/entity-menu-frame.png.frame.txt "$WORK/segdig.frame.txt"
# nav numbers from the frame (field 6 for both tags: text rows carry
# an empty action slot at field 5, so field 5 is nav only by luck on
# items - the off-by-one that cost a debug round). No pipes in our
# labels or actions, so positional split is exact.
NAVS="$(python3 - "$WORK/segdig.frame.txt" <<'PY'
import sys
nav = {}
for line in open(sys.argv[1], encoding='utf-8', errors='replace'):
    f = line.rstrip('\n').split('|')
    if len(f) < 7:
        continue
    if f[0] == 'item' and f[1] == 'r1':
        nav['r1'] = f[6]
    elif f[0] == 'text' and f[1] == 'r2':
        nav['r2'] = f[6]
print('%s %s' % (nav.get('r1', ''), nav.get('r2', '')))
PY
)"
R1N="$(printf '%s' "$NAVS" | awk '{print $1}')"
R2N="$(printf '%s' "$NAVS" | awk '{print $2}')"
case "$R1N" in ''|*[!0-9]*) echo "FAIL: no nav number for r1 ($NAVS)"; exit 1;; esac
case "$R2N" in ''|*[!0-9]*) echo "FAIL: no nav number for r2 ($NAVS)"; exit 1;; esac
note "nav numbers: r1=$R1N r2=$R2N"

# send <codes...> : one KEY_PRESSED line per relay code.
send() { for c in "$@"; do printf '%s\n' "$c" >> "$HIST"; done; sleep 1.2; }
# digits <number> : ASCII code per digit (fold needs the trailing \n or
# `while read` drops the last digit - the row-2-vs-20 lesson).
digits() { printf '%s\n' "$1" | fold -w1 | while IFS= read -r ch; do [ -n "$ch" ] && printf '%d\n' "'$ch"; done >> "$HIST"; sleep 1.2; }
rm -f "$WORK/click.log"

# A. digits still address rows.
send 27; digits "$R1N"; send 13
grep -q 'CLICKED ROW1' "$WORK/click.log" 2>/dev/null \
    && note "A: digit focuses row, Enter dispatches row action" \
    || fail "A: expected ROW1 (log: $(cat "$WORK/click.log" 2>/dev/null))"

# B. direct link select: Right enters span mode, 3 jumps to link 3.
send 27; digits "$R2N"; send 203 51 13
grep -q 'CLICKED LINK3' "$WORK/click.log" 2>/dev/null \
    && note "B: Right+3+Enter dispatched LINK3" \
    || fail "B: expected LINK3 (log: $(cat "$WORK/click.log" 2>/dev/null))"

# C. overshoot holds: 1 selects link 1, 9 is out of range for 3
# links, Enter dispatches link 1 - focus never leaves the row.
send 27; digits "$R2N"; send 203 49 57 13
grep -q 'CLICKED LINK1' "$WORK/click.log" 2>/dev/null \
    && note "C: Right+1+9+Enter dispatched LINK1 (overshoot held)" \
    || fail "C: expected LINK1 (log: $(cat "$WORK/click.log" 2>/dev/null))"

# D. 0 drops to row-level: Right+2 selects link 2, 0 drops the cursor,
# Enter on the actionless text row dispatches nothing.
BEFORE_D="$(wc -l < "$WORK/click.log")"
send 27; digits "$R2N"; send 203 50 48 13
AFTER_D="$(wc -l < "$WORK/click.log")"
[ "$AFTER_D" = "$BEFORE_D" ] \
    && note "D: Right+2+0+Enter dispatched nothing (cursor dropped)" \
    || fail "D: expected silence (log: $(cat "$WORK/click.log" 2>/dev/null))"

# E. dot-addressing: focus, dot, digit jumps straight to the link.
send 27; digits "$R2N"; send 46 50 13
grep -q 'CLICKED LINK2' "$WORK/click.log" 2>/dev/null \
    && note "E: dot+2+Enter dispatched LINK2 (no Right entry)" \
    || fail "E: expected LINK2 (log: $(cat "$WORK/click.log" 2>/dev/null))"

# F. miss-safe: dot+9 on a 3-link row dispatches nothing, disarms, and
# leaves the accumulator fresh (next digit row-jumps normally).
BEFORE_F="$(wc -l < "$WORK/click.log")"
send 27; digits "$R2N"; send 46 57 13
sleep 1
[ "$(wc -l < "$WORK/click.log")" = "$BEFORE_F" ] \
    && note "F: dot+9+Enter dispatched nothing (miss safe)" \
    || fail "F: miss dispatched (log: $(cat "$WORK/click.log" 2>/dev/null))"
digits "$R1N"; send 13
grep -q 'CLICKED ROW1' "$WORK/click.log" 2>/dev/null \
    && note "F: digit after a miss row-jumps (disarmed, fresh accum)" \
    || fail "F: expected ROW1 (log: $(cat "$WORK/click.log" 2>/dev/null))"

# G. dot on a plain row: miss, Enter activates the row itself.
send 27; digits "$R1N"; send 46 50 13
NROW1="$(grep -c 'CLICKED ROW1' "$WORK/click.log" 2>/dev/null || echo 0)"
[ "$NROW1" -ge 2 ] \
    && note "G: dot+2 on a plain row fell through to ROW1" \
    || fail "G: expected ROW1 (log: $(cat "$WORK/click.log" 2>/dev/null))"
if grep -q 'CLICKED LINK2' "$WORK/click.log" 2>/dev/null; then
    NL2="$(grep -c 'CLICKED LINK2' "$WORK/click.log")"
    [ "$NL2" = 1 ] || fail "G: dot on plain row dispatched a link"
fi

kill $RPID 2>/dev/null
sleep 1
if kill -0 $RPID 2>/dev/null; then fail "fixture renderer still alive"; fi

[ "$FAIL" -eq 0 ] && echo "nb_span_digit_test.sh PASS"
exit $FAIL
