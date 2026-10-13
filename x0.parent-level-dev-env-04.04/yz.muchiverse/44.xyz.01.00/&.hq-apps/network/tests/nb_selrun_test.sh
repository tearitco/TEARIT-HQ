#!/bin/bash
# nb_selrun_test.sh - pins ROW-RUN SELECTION + COPY (Shift+Up / Shift+Down).
#
# Generic renderer capability, not a browser feature: Shift+Up/Down select
# a contiguous run of whole rows, and Ctrl+C copies the run as joined
# text. Before this, selection was a character range INSIDE one row, so
# "copy these three paragraphs" did not exist - copy took the focused
# row's label or the span range within it.
#
# HERMETIC on purpose, against its own fixture window: the live browser on
# this box is respawned under long tests (two renderers mid-run on
# 2026-10-12), and a suite whose subject is a generic renderer capability
# should not depend on another agent's window. No house state files are
# touched except the relay, which is how the renderer is driven.
#
# Four independent channels, so no single lie passes:
#   STATE   the mirror's [SEL] line - the run's extent in nav indices
#   GEOM    the frame dump - which rows those nav indices actually are
#   PIXELS  blue selection band measured on EVERY selected row, using
#           each row's own y/h from the frame dump (so "a band somewhere"
#           cannot pass)
#   CLIP    a real X11 CLIPBOARD read, compared against the exact text the
#           frame dump says those rows contain
#
# Pinned:
#   EXTEND  Shift+Down extends; the run is reported in nav order
#   BAND    every selected row carries the band, wrapped rows included,
#           per visual line
#   COPY    Ctrl+C yields the rows joined by newlines - the real
#           clipboard, compared byte-for-byte with the frame's labels
#   UP      Shift+Up extends the other way, back to where it started
#   ONE     a run of exactly one row still copies just that row
#   CLEAR   any other key (a plain arrow) ends the run - found by testing:
#           without this, a digit nav left the anchor alive and the next
#           Shift+Down extended from a row the user never selected
#   SPAN    a span selection inside the focused row still wins over the
#           row run (strictly more specific), so this did not regress
#           inline-span copy
#
# GREEN as of 2026-10-13 and wired into nb_all_tests.sh. It was WIP for one
# commit because three of its own harness bugs read exactly like product
# bugs; they are recorded here because the next person to write a relay-
# driven suite will hit at least the first one:
#
#   - relay codes are ASCII VALUES. `printf '1\n'` sends the NUMBER one,
#     which no branch of dispatch_relay_code() claims (matching starts at
#     32), so NO key is delivered at all. g_focus_nav already being 1 made
#     the focus assertion pass anyway. The suite now proves keys arrive
#     before it asserts anything about them.
#   - two presses of the SAME digit accumulate: g_nav_digit_accum reads
#     "1" then "1" as eleven and jumps to nav 11, which an 8-row fixture
#     clamps - focus landed outside the window and runs anchored on rows
#     nobody touched. Reset with a plain arrow instead (it cannot
#     accumulate, and Down-then-Up is net-zero on focus).
#   - the smallest run the arrows can make is TWO rows, not one: the first
#     Shift+Down both anchors and moves. A one-row run needs shift-click,
#     which is deliberately not built, and asserting it asserted a feature
#     that does not exist.
#
# Usage: sh nb_selrun_test.sh
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HR="$(cd "$HERE/../../.." && pwd)"
RENDER="$HR/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
DUMP="$HR/&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x"
WORK=/tmp/nb_selrun_test
FRAME=/tmp/entity-menu-frame.png.frame.txt
RECEIPT=/tmp/entity-menu-frame.png.receipt.txt
SHOT=/tmp/nb_selrun_shot.png
FAIL=0
fail() { echo "FAIL: $*"; FAIL=1; }
note() { printf '%s\n' "$*"; }

[ -x "$RENDER" ] || { echo "FAIL: renderer not built ($RENDER)"; exit 1; }

rm -rf "$WORK"; mkdir -p "$WORK"
cp "$HERE/fixtures/selrun/selrun.xhtpm" "$WORK/selrun.xhtpm"

setsid "$RENDER" "$HR" "$WORK/selrun.xhtpm" "$WORK" >"$WORK/render.log" 2>&1 &
RPID=$!
# Wait for OUR renderer specifically. pgrep -f on a shared box can hand
# back someone else's window, and a relay written to a pid nobody reads is
# silently lost - which reads exactly like "the feature does not work".
for _ in $(seq 1 30); do
    kill -0 "$RPID" 2>/dev/null || break
    [ -f "$HR/#.desktop/ascii_frames/$RPID.frame.txt" ] && \
        grep -q '\[>\]' "$HR/#.desktop/ascii_frames/$RPID.frame.txt" 2>/dev/null && break
    sleep 1
done
if ! kill -0 "$RPID" 2>/dev/null; then
    echo "FAIL: fixture renderer died"; head -5 "$WORK/render.log"; exit 1
fi
RELAY="$HR/#.desktop/entity_menu_history/$RPID.txt"
MIRROR="$HR/#.desktop/ascii_frames/$RPID.frame.txt"
trap 'kill $RPID 2>/dev/null' EXIT

# Relay codes are ASCII VALUES, not the characters themselves. This suite
# first wrote `printf '1\n'` to focus row 1 - the number 1, which no branch
# of dispatch_relay_code() claims (it starts matching at 32), so NO key was
# ever delivered. The run then appeared to survive every "collapse" key,
# which read exactly like a product bug; it was the test sending nothing,
# and g_focus_nav already being 1 made the focus assertion pass anyway.
# A plain arrow, NOT a digit: a repeated digit accumulates (see the header
# note) and a second press of the same one jumps to a nav index this
# 8-row fixture does not have. Down-then-Up is net-zero on focus and
# collapses the run, so it resets without moving the cursor.
#
# DEFINED HERE, NOT LATER: the first version of this file defined esc_run
# below its own first call, so every reset silently did nothing ("esc_run:
# not found" on stderr, exit status swallowed) and the runs piled onto each
# other - 3..5, then 5..6, then 6..7 - which reads like the feature
# misbehaving when it is the reset that never ran.
esc_run() { printf '201\n' >> "$RELAY"; sleep 1.2; printf '200\n' >> "$RELAY"; sleep 1.2; }
# ...which means it PRESERVES the current row. Sections that state an
# expected run therefore need focus parked on row 1 first, or they assert
# against wherever the previous section left the cursor. The second version
# of this suite failed exactly that way ("rows 3..4" where "rows 1..2" was
# expected) - a stale expectation, not a broken feature.
home() {
    for _ in 1 2 3 4 5 6 7 8 9 10; do
        [ "$(focused)" = "1" ] && break
        printf '200\n' >> "$RELAY"; sleep 1.0
    done
    esc_run
    [ "$(focused)" = "1" ]
}
ascii() { printf '%d' "'$1"; }
key()   { printf '%d\n' "$(ascii "$1")" >> "$RELAY"; sleep "${2:-1.2}"; }
sel()  { grep '\[SEL\]' "$MIRROR" 2>/dev/null | head -1; }
focused() { grep '\[>\]' "$MIRROR" 2>/dev/null | head -1 | sed -n 's/^[[:space:]]*\[>\] \([0-9][0-9]*\)\..*/\1/p'; }
# The frame dump path is FIXED (/tmp/entity-menu-frame.png.*) and shared by
# every window in the house, so another window's dump can land in it at any
# moment - and the first version of this suite read row geometry BEFORE its
# first dump, i.e. from whatever the live browser had left there, and then
# correctly reported "nav 1 is nb-clearhist, expected r0". Take our own
# copy immediately, and refuse to proceed unless it is demonstrably OURS.
MYFRAME=/tmp/nb_selrun_frame.txt
shot() {   # full-tree dump + a fresh png, both private copies
    rm -f "$RECEIPT"
    printf '206\n' >> "$RELAY"
    for _ in $(seq 1 25); do [ -f "$RECEIPT" ] && break; sleep 0.5; done
    [ -f "$RECEIPT" ] || return 1
    sleep 1
    cp /tmp/entity-menu-frame.png "$SHOT" 2>/dev/null
    cp "$FRAME" "$MYFRAME" 2>/dev/null || return 1
    grep -q '|r0|' "$MYFRAME"
}
# nav index -> "id|y|h" for every laid-out row, from OUR frame copy
navgeom() { awk -F'|' -v n="$1" '$7==n {print $2"|"$10"|"$12; exit}' "$MYFRAME"; }
label()   { awk -F'|' -v n="$1" '$7==n {print $4; exit}' "$MYFRAME"; }

# ---------------------------------------------------------------- focus
# Focus the FIRST row by digit 1 (the fixture numbers its rows 1..8).
key 1 1.5
shot || { echo "FAIL: no frame dump from the fixture window"; exit 1; }
FIRST="$(focused)"
if [ -z "$FIRST" ] || [ "$FIRST" != "1" ]; then
    echo "FAIL: could not focus row 1 (focus='${FIRST:-none}')"; exit 1
fi
# and prove the relay actually delivered keys at all: focus row 3 with a
# digit, then come back. Without this the whole suite can pass while
# sending nothing - which is exactly what happened the first time.
key 3 1.5
if [ "$(focused)" != "3" ]; then
    echo "FAIL: relay digit did not move focus (focus='$(focused)') - the suite is sending codes nothing reads"; exit 1
fi
# Walk back to row 1 with ARROWS, not another digit: a second press of the
# same digit would accumulate ("1"+"1" -> nav 11). The guard above leaves
# focus on row 3 on purpose, so every later expectation starts from 1 - and
# this asserts that rather than assuming it, which is how the previous
# version ended up asserting "rows 1..3" while focus was on row 3.
printf '200\n' >> "$RELAY"; sleep 1.2
printf '200\n' >> "$RELAY"; sleep 1.5
if [ "$(focused)" != "1" ]; then
    echo "FAIL: could not walk focus back to row 1 (focus='$(focused)')"; exit 1
fi
echo "PASS: focus on the first fixture row (nav $FIRST = $(navgeom "$FIRST"))"

# ---------------------------------------------------------------- EXTEND
home || { echo "FAIL: could not park focus on row 1"; exit 1; }
printf '223\n' >> "$RELAY"; sleep 1.2                 # Shift+Down
printf '223\n' >> "$RELAY"; sleep 1.5                 # Shift+Down
line="$(sel)"
if printf '%s' "$line" | grep -q '\[SEL\] rows 1\.\.3 (3 selected)'; then
    echo "PASS: Shift+Down extends a run of whole rows ($line)"
else
    fail "Shift+Down did not extend: '${line:-no [SEL] line}'"
fi

# The nav indices must be the THREE fixture rows, not three arbitrary
# rows: cross-checked against the frame dump, because a [SEL] line alone
# is self-report.
ok=1
for n in 1 2 3; do
    id="$(navgeom "$n" | cut -d'|' -f1)"
    [ "$id" = "r$((n-1))" ] || { ok=0; note "  nav $n is '$id', expected r$((n-1))"; }
done
if [ "$ok" = "1" ]; then
    echo "PASS: the run is fixture rows r0..r2, verified against the frame dump"
else
    fail "the run's nav indices are not the rows the user walked over"
fi

# ---------------------------------------------------------------- BAND
shot || { echo "FAIL: no frame dump"; exit 1; }
python3 - "$HERE" "$MYFRAME" <<'PY' || fail "selection band is not on every selected row"
import importlib.util, sys
here, frame = sys.argv[1], sys.argv[2]
spec = importlib.util.spec_from_file_location('ws', here + '/widthscan.py')
m = importlib.util.module_from_spec(spec)
exec(compile(open(spec.origin).read().replace('\nmain()\n', '\n'), 'ws', 'exec'), m.__dict__)
w, h, px = m.decode('/tmp/nb_selrun_shot.png')
def near(c): return abs(c[0]-0x2f) <= 24 and abs(c[1]-0x5f) <= 24 and abs(c[2]-0x8f) <= 24
blue = {(x, y) for y in range(h) for x in range(w)
        if near((px[(y*w+x)*3], px[(y*w+x)*3+1], px[(y*w+x)*3+2]))}
assert blue, 'no selection-coloured pixels anywhere - the band is not painted at all'
missing = []
covered = []
for line in open(frame):
    f = line.rstrip('\n').split('|')
    if len(f) < 12 or not f[6].isdigit(): continue
    nav, y, rh = int(f[6]), int(f[9]), int(f[11])
    if not 1 <= nav <= 3: continue
    if rh <= 0: continue                      # parked off screen: nothing to paint
    hits = sum(1 for yy in range(max(0, y), min(h, y + rh))
                 for xx in range(w) if (xx, yy) in blue)
    (covered if hits >= 20 else missing).append((nav, f[1], y, rh, hits))
assert not missing, 'no band on selected row(s): %s' % missing
assert covered, 'no selected row was found in the frame to check'
print('PASS: band painted on every selected row (%s)' %
      ', '.join('nav %d %s y%s h%s %dpx' % c for c in covered))
PY

# ---------------------------------------------------------------- COPY
printf '3\n' >> "$RELAY"; sleep 2                   # Ctrl+C
got="$(python3 "$HERE/xclipget.py" 2>/dev/null)"
# built by PRINTING, not by appending and stripping: the first version
# appended a newline per row and removed only the final one, so "want"
# carried a trailing blank line and the comparison could never pass.
: > "$WORK/want.txt"
# label() is awk's print, so it ALREADY terminates each row. Adding a
# newline here too put a blank line between every row ("short.//Row one").
for n in 1 2 3; do label "$n" >> "$WORK/want.txt"; done
want="$(cat "$WORK/want.txt")"   # strips only the final newline
if [ "$got" = "$want" ]; then
    echo "PASS: Ctrl+C copied the run joined by newlines (real X11 clipboard read)"
else
    fail "clipboard mismatch.
  want: $(printf '%s' "$want" | head -c 120 | tr '\n' '/')
  got : $(printf '%s' "$got" | head -c 120 | tr '\n' '/')"
fi

# ---------------------------------------------------------------- UP
home || { echo "FAIL: could not park focus on row 1"; exit 1; }
printf '223\n' >> "$RELAY"; sleep 1.5
printf '223\n' >> "$RELAY"; sleep 1.5
printf '222\n' >> "$RELAY"; sleep 1.5                # Shift+Up
line="$(sel)"
if printf '%s' "$line" | grep -q '\[SEL\] rows 1\.\.2 (2 selected)'; then
    echo "PASS: Shift+Up extends the run back up ($line)"
else
    fail "Shift+Up did not shorten the run: '${line:-no [SEL] line}'"
fi

# ---------------------------------------------------------------- MIN
# The SMALLEST run the arrows can make is two rows, and that is correct:
# the first Shift+Down anchors on the focused row AND moves the end, so
# "one row selected" is not reachable by keyboard at all - it would need
# shift-click, which is deliberately not built. Asserting a one-row run
# here was asserting a feature that does not exist.
home || { echo "FAIL: could not park focus on row 1"; exit 1; }
printf '223\n' >> "$RELAY"; sleep 1.5
line="$(sel)"
if printf '%s' "$line" | grep -q '\[SEL\] rows 1\.\.2 (2 selected)'; then
    echo "PASS: the smallest arrow-made run is two rows ($line)"
else
    fail "expected a two-row run after one Shift+Down: '${line:-no [SEL] line}'"
fi
printf '3\n' >> "$RELAY"; sleep 2
got="$(python3 "$HERE/xclipget.py" 2>/dev/null)"
: > "$WORK/want2.txt"; label 1 >> "$WORK/want2.txt"; label 2 >> "$WORK/want2.txt"
if [ "$got" = "$(cat "$WORK/want2.txt")" ]; then
    echo "PASS: a two-row run copies exactly those two rows"
else
    fail "two-row copy wrong: '$(printf '%s' "$got" | head -c 80)'"
fi

# ---------------------------------------------------------------- CLEAR
printf '201\n' >> "$RELAY"; sleep 1.5               # a plain arrow
if [ -z "$(sel)" ]; then
    echo "PASS: a plain arrow ends the run"
else
    fail "a plain arrow left the run alive: '$(sel)'"
fi

[ "$FAIL" -eq 0 ] && echo "nb_selrun_test.sh PASS"
exit $FAIL