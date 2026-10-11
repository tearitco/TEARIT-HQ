#!/bin/bash
# nb_theme_test.sh - light-theme readability pin for the browser.
#
# The 2026-10-04 trap: on a light user theme g_theme_fg is #000000 while
# the browser window forces dark chrome, so any text-drawing element
# WITHOUT its own color rule renders black-on-black. Three layers:
#
#   A. static cross-check (tests/themecheck.py, instant): every
#      <text>/<item>/<cli_io> in network-browser-hq.xhtpm carries a
#      class list with a color rule, and container tags carry no label=.
#   B. arithmetic (same script, instant): every color: rule contrasts
#      >= 4.5 against its background (else both window surfaces).
#   C. pixels (own fixture window, hermetic): one row per text class,
#      styled by a COPY of the real css, scanned for dark-neutral ink
#      with no bright neighbor - the bug's exact signature. Rows
#      without ink fail too (a non-rendering row must not vacuous-pass).
#
# Negative-tested: a template copy with a bogus class fails A, and a
# fixture css copy minus the .nb-text rule renders black text that C
# flags on the th-text row.
#
# Usage: sh nb_theme_test.sh
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HR="$(cd "$HERE/../../.." && pwd)"
RENDER="$HR/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
WORK=/tmp/nb_theme_test
FAIL=0

note() { printf '%s\n' "$*"; }
fail() { echo "FAIL: $*"; FAIL=1; }

[ -x "$RENDER" ] || { echo "FAIL: renderer not built ($RENDER)"; exit 1; }
command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }

rm -rf "$WORK"; mkdir -p "$WORK"
KILLPIDS=""
trap 'for p in $KILLPIDS; do kill "$p" 2>/dev/null; done' EXIT

########## PARTS A+B: static cross-check + arithmetic ##########
note "== static cross-check + arithmetic"
python3 "$HERE/themecheck.py" "$HERE/../network-browser-hq.xhtpm" \
    "$HERE/../network-browser-hq.css" || { echo "FAIL: themecheck on the real files"; exit 1; }

########## PART C: pixels on a fixture window ##########
note "== fixture pixels"
python3 - "$WORK/theme.xhtpm" <<'PY'
import sys
F = '\x1f'
J = '\x1e'
def T(t): return 'text' + F + t + F
def L(t, u): return 'link' + F + t + F + u + F + 'true'
segs = J.join([T('See the '), L('LINKTEXT', 'https://example.com/t'), T('here.')])
cells_a, counts_a = 'Alpha' + F + 'Beta value', '5,10'
cells_h, counts_h = 'Name' + F + 'Detail text', '4,11'
# (tag, id, class, label, extra-attrs) - tag+class pairs mirror the
# real template exactly (see nb_theme_test.sh header for the mapping).
# Transparent rows (every <text>, plus items with no bg of their own)
# carry bg="#1a1a1a": text elements paint no background themselves, and
# in the browser they sit on the nb-content container (#1a1a1a, whose
# own bg Part B pins) over a light clear - without the stand-in the
# fixture page stays clear-light and even correct white text reads as
# missing ink. cli_io/button/check/quiet rows paint their own dark
# boxes via their classes, so they need none; <bar> paints its own
# track (#222) the same way.
BG = ' bg="#1a1a1a"'
rows = [
    ('text', 'th-text', 'nb-text', 'Theme body text row', ''),
    ('text', 'th-rich', 'nb-rich', 'See the LINKTEXT here.', ' segments="%s"' % segs),
    ('text', 'th-list', 'nb-list', '- list item one', ''),
    ('text', 'th-drow', 'nb-drow', 'Term :: definition text', ''),
    ('text', 'th-summary', 'nb-summary', 'Summary of details content', ''),
    ('text', 'th-code', 'nb-code', 'x = 1  # code sample', ''),
    ('text', 'th-trow', 'nb-trow', 'Alpha \u00b7 Beta value', ' cells="%s" ccounts="%s"' % (cells_a, counts_a)),
    ('text', 'th-thead', 'nb-trow thead', 'Name \u00b7 Detail text', ' cells="%s" ccounts="%s"' % (cells_h, counts_h)),
    ('text', 'th-h1', 'nb-h1', 'Theme heading one', ''),
    ('text', 'th-h3', 'nb-h3', 'Theme heading three', ''),
    ('text', 'th-h6', 'nb-h6', 'Theme heading six', ''),
    ('text', 'th-block', 'block-title', 'Theme block title', ''),
    ('text', 'th-console', 'nb-console-text', 'console line here', ''),
    ('text', 'th-title', 'page-title', 'Theme page title', ''),
    ('text', 'th-warn', 'nb-warn', '[showing 900 of 1502 rows]', ''),
    ('text', 'th-status', 'nb-status', 'Status: ready', ''),
    ('text', 'th-quiet', 'quiet', 'Theme quiet row', ''),
    ('item', 'th-side', 'nb-side', 'Theme sidebar entry', ''),
    ('item', 'th-link', 'nb-link', 'Theme content link', ''),
    ('item', 'th-button', 'nb-button', 'Theme button', ''),
    ('item', 'th-check', 'nb-check', 'Theme check row', ''),
    ('item', 'th-selopt', 'nb-selopt', 'Theme select option', ''),
    ('item', 'th-file', 'nb-file', 'Theme file row', ''),
    ('item', 'th-qsprite', 'quiet sprite-flow', 'Theme media label', ''),
    ('cli_io', 'th-addr', 'top nb-address', 'https://example.com/theme-probe', ' content="https://example.com/theme-probe"'),
    ('cli_io', 'th-input', 'nb-input', 'typed value here', ' content="typed value here"'),
    ('bar', 'th-bar', '', '00:30', ' value="30" max="100"'),
]
out = ['<window label="ThemeFix" class="theme-fixture database-window">', '  <page id="main">',
       '    <scrolllist class="nb-content">']
for tag, i, c, lab, extra in rows:
    # rows that paint no background of their own get the nb-content
    # stand-in (see BG above); the rest already own a dark box.
    needs_bg = (tag == 'text' or
                (tag == 'item' and c in ('nb-side', 'nb-link', 'nb-selopt', 'nb-file')))
    bg = BG if needs_bg else ''
    if tag == 'bar':
        out.append('      <%s id="%s"%s label="%s"/>' % (tag, i, extra, lab))
    elif tag == 'cli_io':
        out.append('      <%s id="%s" class="%s" target_id="%s" label="%s"%s/>' % (tag, i, c, i, lab, extra))
    else:
        out.append('      <%s id="%s" class="%s" label="%s"%s%s/>' % (tag, i, c, lab, extra, bg))
out += ['    </scrolllist>', '  </page>', '</window>', '']
open(sys.argv[1], 'w').write('\n'.join(out))
print('fixture rows: %d' % len(rows))
PY
# The REAL rules (copied, never retyped - no drift), plus only a window
# size override for the taller fixture.
cp "$HERE/../network-browser-hq.css" "$WORK/theme.css"
cat >> "$WORK/theme.css" <<'EOF'
window.theme-fixture { width: 760px; height: 900px; }
.theme-fixture { width: 760px; height: 900px; background-color: #141414; color: #dddddd; }
EOF

setsid "$RENDER" "$HR" "$WORK/theme.xhtpm" "$WORK" >"$WORK/render.log" 2>&1 &
RPID=$!
KILLPIDS="$KILLPIDS $RPID"
sleep 6
if ! kill -0 "$RPID" 2>/dev/null; then
    echo "FAIL: fixture renderer died"; head -5 "$WORK/render.log"; exit 1
fi
# Raise above any occluders: XGetImage reads screen pixels, and an
# obscured window captures whoever covers it (the hover-session lesson).
WID="$(xwininfo -root -tree 2>/dev/null | grep '(has no name)' | grep '764x904' | awk '{print $1}' | head -1)"
[ -n "$WID" ] || { echo "FAIL: fixture window not found"; exit 1; }
python3 - "$WID" <<'PY'
import ctypes, sys
x11 = ctypes.CDLL('libX11.so.6')
x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
x11.XOpenDisplay.restype = ctypes.c_void_p
x11.XRaiseWindow.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
x11.XFlush.argtypes = [ctypes.c_void_p]
d = x11.XOpenDisplay(None)
assert d, 'no display'
x11.XRaiseWindow(d, int(sys.argv[1], 16))
x11.XFlush(d)
print('raised', sys.argv[1])
PY
sleep 1
HIST="$HR/#.desktop/entity_menu_history/$RPID.txt"
rm -f /tmp/entity-menu-frame.png
printf '112\n' >> "$HIST"
for _ in $(seq 1 20); do
    [ -f /tmp/entity-menu-frame.png ] && break
    sleep 0.5
done
[ -f /tmp/entity-menu-frame.png ] || { echo "FAIL: no frame dump"; exit 1; }
sleep 1
cp /tmp/entity-menu-frame.png "$WORK/theme.png"
cp /tmp/entity-menu-frame.png.frame.txt "$WORK/theme.frame.txt"

python3 "$HERE/themescan.py" "$WORK/theme.png" "$WORK/theme.frame.txt" | tee "$WORK/theme.scan"
python3 - "$WORK/theme.scan" <<'PY' || exit 1
import sys
want = ['th-text', 'th-rich', 'th-list', 'th-drow', 'th-summary', 'th-code',
        'th-trow', 'th-thead', 'th-h1', 'th-h3', 'th-h6', 'th-block',
        'th-console', 'th-title', 'th-warn', 'th-status', 'th-quiet',
        'th-side', 'th-link', 'th-button', 'th-check', 'th-selopt',
        'th-file', 'th-qsprite', 'th-addr', 'th-input', 'th-bar']
seen, bad = {}, []
for line in open(sys.argv[1]):
    p = line.rstrip('\n').split(' ')
    if p[0] == 'INK':
        seen[p[1]] = int(p[2])
    elif p[0] == 'DARK':
        bad.append(p)
    elif p[0] in ('GEOMFAIL', 'NOBOX'):
        print('FAIL: %s' % line.rstrip('\n'))
        sys.exit(1)
missing = [r for r in want if r not in seen]
if missing:
    print('FAIL: rows never rendered: %s' % missing)
    sys.exit(1)
dry = [r for r in want if seen[r] < 40]
if dry:
    print('FAIL: rows rendered no ink: %s' % dry)
    sys.exit(1)
print('rows rendered with ink: %d/%d' % (len(seen), len(want)))
if bad:
    for b in bad[:15]:
        print('DARK INK: row=%s y=%s x=%s w=%s core=%s' % tuple(b[1:6]))
    sys.exit(1)
print('no dark-neutral ink on any row')
PY
kill $RPID 2>/dev/null
sleep 1
if kill -0 $RPID 2>/dev/null; then fail "fixture renderer still alive"; fi

########## PART D: negatives ##########
note "== negatives"
# D1 (static): a template copy with an uncovered class must fail the
# cross-check. Instant, no relaunch.
cp "$HERE/../network-browser-hq.xhtpm" "$WORK/mut.xhtpm"
cp "$HERE/../network-browser-hq.css" "$WORK/mut.css"
printf '<text label="bogus" class="nb-bogus"/>\n' >> "$WORK/mut.xhtpm"
if python3 "$HERE/themecheck.py" "$WORK/mut.xhtpm" "$WORK/mut.css" >/dev/null 2>&1; then
    fail "D1: bogus class passed the cross-check"
else
    note "D1: bogus class fails the cross-check, as required"
fi
# D2 (pixels): the fixture css minus the .nb-text rule must render
# black text that the scanner flags on the th-text row. A passing
# scanner nobody can fail is decoration.
python3 - "$WORK/theme.css" "$WORK/norule.css" <<'PY'
import re, sys
src, dst = sys.argv[1], sys.argv[2]
css = open(src).read()
css2, n = re.subn(r'\.nb-text\s*\{[^}]*\}\n?', '', css, count=1)
assert n == 1, 'nb-text rule not found'
open(dst, 'w').write(css2)
print('nb-text rule removed for D2')
PY
cp "$WORK/theme.xhtpm" "$WORK/norule.xhtpm"
setsid "$RENDER" "$HR" "$WORK/norule.xhtpm" "$WORK" >"$WORK/render_d2.log" 2>&1 &
RPID=$!
KILLPIDS="$KILLPIDS $RPID"
sleep 6
# the fixture dir now holds norule.css next to norule.xhtpm (same stem
# merge as the main pass) - but theme.css is still present; the merge
# is same-stem only, so norule.xhtpm reads norule.css. Move theme.css
# aside so there is no doubt which sheet applies.
mv "$WORK/theme.css" "$WORK/theme.css.kept"
WID="$(xwininfo -root -tree 2>/dev/null | grep '(has no name)' | grep '764x904' | awk '{print $1}' | head -1)"
python3 - "$WID" <<'PY'
import ctypes, sys
x11 = ctypes.CDLL('libX11.so.6')
x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
x11.XOpenDisplay.restype = ctypes.c_void_p
x11.XRaiseWindow.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
x11.XFlush.argtypes = [ctypes.c_void_p]
d = x11.XOpenDisplay(None)
x11.XRaiseWindow(d, int(sys.argv[1], 16))
x11.XFlush(d)
PY
sleep 1
rm -f /tmp/entity-menu-frame.png
printf '112\n' >> "$HR/#.desktop/entity_menu_history/$RPID.txt"
for _ in $(seq 1 20); do
    [ -f /tmp/entity-menu-frame.png ] && break
    sleep 0.5
done
cp /tmp/entity-menu-frame.png "$WORK/norule.png"
cp /tmp/entity-menu-frame.png.frame.txt "$WORK/norule.frame.txt"
mv "$WORK/theme.css.kept" "$WORK/theme.css"
python3 "$HERE/themescan.py" "$WORK/norule.png" "$WORK/norule.frame.txt" > "$WORK/norule.scan"
if grep -q '^DARK th-text ' "$WORK/norule.scan"; then
    note "D2: rule-less nb-text renders black and is flagged, as required"
    grep '^DARK th-text ' "$WORK/norule.scan" | head -3
else
    echo "FAIL: D2: no dark ink flagged on th-text without its rule"
    grep -c '^DARK' "$WORK/norule.scan" | xargs echo "total DARK lines:"
    exit 1
fi
kill $RPID 2>/dev/null
sleep 1
if kill -0 $RPID 2>/dev/null; then fail "D2 renderer still alive"; fi

[ "$FAIL" -eq 0 ] && echo "nb_theme_test.sh PASS"
exit $FAIL
