#!/bin/bash
# nb_span_select_test.sh - pins INLINE SPAN step-10 keyboard range selection.
#
# Shift+Left/Right extends a selection anchor across the focused row's
# links (entered with the anchor collapsed); the range highlights with
# the fields' own #2f5f8f band and unarmed Ctrl+C copies its plain text.
# Collapsed or absent anchors copy the whole label, exactly as before.
#
# Hermetic: own fixture window (an item row, a 3-link row, a plain row,
# a wrapped 3-link row), link actions append to click.log. Copied bytes
# are read back through the LIVE X11 clipboard (tests/xclipget.py) -
# a real end-to-end read, not a state-file peek. Nav numbers and row
# boxes come from the ascii frame, never hardcoded.
#
# Steps (Esc hygiene between - drops stale cursors, resets accum):
#   S1. focus r2, Shift+Right x2, dump: #2f5f8f band where parked has none
#   S2. Ctrl+C -> clipboard is exactly "ONE then TWO"
#   S3. Esc, focus r2, Shift+Right x1, Ctrl+C -> whole label (collapsed)
#   S4. Esc, Ctrl+C -> whole label (no anchor)
#   S5. focus r1, Shift+Right, Ctrl+C -> whole r2 label (plain rows step,
#       never select)
#   S6. focus r4, Shift+Right x2, dump: bands on 2+ line-bands; Ctrl+C ->
#       exact wrapped range text
#   S7. Esc, focus r2, Right, Enter -> LINK1 action (Enter dispatches the
#       cursor link; plain entry cleared any range)
#   S8. Esc, focus r2, Shift+Right x2, relay-click the row's text span,
#       Ctrl+C -> whole label (clicks clear the range)
#
# Usage: sh nb_span_select_test.sh
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HR="$(cd "$HERE/../../.." && pwd)"
RENDER="$HR/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
WORK=/tmp/nb_span_select
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
# \x1E; text spans keep the trailing empty url field; actions are BARE
# commands (dispatch wraps in sh '...' itself). r4's paragraph is long
# enough to wrap at 760px, exercising the wrapped bands.
python3 - "$WORK/selsplit.xhtpm" "$WORK/action.sh" <<'PY'
import sys
FS, J = '\x1f', '\x1e'
def T(t): return 'text' + FS + t + FS
def L(t, a): return 'link' + FS + t + FS + 'https://example.com/' + t + FS + a
A = "'" + sys.argv[2] + "'"
segs2 = J.join([T('go to '), L('ONE', A + " 'LINK1'"), T(' then '),
                L('TWO', A + " 'LINK2'"), T(' then '),
                L('THREE', A + " 'LINK3'"), T(' now')])
segs4 = J.join([T('Begin '), L('ALPHA', A + " 'LINKA'"),
                T(' middle words here and here plus a few more words to pad the row '),
                L('BETA', A + " 'LINKB'"),
                T(' more words to wrap the row around the pane edge and past it '),
                L('GAMMA', A + " 'LINKC'"),
                T(' final tail words at the very end of this long row here yes')])
doc = ('<window label="SelSplit" class="selsplit-fixture database-window">\n'
       '  <page id="main">\n'
       '    <item id="r1" class="plain" label="plain row one" action="%s \'ROW1\'"/>\n'
       '    <text id="r2" class="seg" label="go to ONE then TWO then THREE now" segments="%s"/>\n'
       '    <text id="r3" class="plain" label="plain row three"/>\n'
       '    <text id="r4" class="seg" label="Begin ALPHA middle words here and here plus a few more words to pad the row BETA more words to wrap the row around the pane edge and past it GAMMA final tail words at the very end of this long row here yes" segments="%s"/>\n'
       '  </page>\n</window>\n' % (A, segs2, segs4))
open(sys.argv[1], 'w').write(doc)
PY
cat > "$WORK/selsplit.css" <<'EOF'
window { width: 560px; height: 340px; }
window.selsplit-fixture { width: 560px; height: 340px; background-color: #141414; color: #dddddd; }
.plain, .seg { color: #cccccc; }
EOF

setsid "$RENDER" "$HR" "$WORK/selsplit.xhtpm" "$WORK" >"$WORK/render.log" 2>&1 &
RPID=$!
KILLPIDS="$KILLPIDS $RPID"
sleep 6
if ! kill -0 "$RPID" 2>/dev/null; then
    echo "FAIL: fixture renderer died"; head -5 "$WORK/render.log"; exit 1
fi
HIST="$HR/#.desktop/entity_menu_history/$RPID.txt"

dump() {  # dump <tag>: relay 'p', copy the renderer's own PNG+frame aside.
    rm -f /tmp/entity-menu-frame.png
    printf '112\n' >> "$HIST"
    for _ in $(seq 1 20); do
        [ -f /tmp/entity-menu-frame.png ] && { sleep 1; cp /tmp/entity-menu-frame.png "$WORK/frame_$1.png"; cp /tmp/entity-menu-frame.png.frame.txt "$WORK/frame_$1.frame.txt"; return 0; }
        sleep 0.5
    done
    return 1
}
# send <codes...>: one KEY_PRESSED line per relay code. 220/221 are
# Shift+Left/Right, 203/202 plain, 13 Return, 27 Escape, 3 Ctrl+C.
send() { for c in "$@"; do printf '%s\n' "$c" >> "$HIST"; done; sleep 1.2; }
digits() { printf '%s\n' "$1" | fold -w1 | while IFS= read -r ch; do [ -n "$ch" ] && printf '%d\n' "'$ch"; done >> "$HIST"; sleep 1.2; }
# clip: request the live clipboard, retrying once (X round-trip under
# load). Prints the bytes; an empty/TIMEOUT print fails the caller.
clip() {
    C="$(python3 "$HERE/xclipget.py" 2>/dev/null)" && { printf '%s' "$C"; return 0; }
    sleep 2
    python3 "$HERE/xclipget.py" 2>/dev/null
}
# rowbox <frame> <id>: "x y w h" of a th-/r- row (21-field tail peel).
rowbox() { python3 - "$1" "$2" <<'PY'
import sys
for line in open(sys.argv[1], encoding='utf-8', errors='replace'):
    f = line.rstrip('\n').split('|')
    if len(f) > 1 and f[1] == sys.argv[2]:
        try:
            # front fields (see themescan.py: end-peel breaks on every
            # frame-tail extension).
            nav, active, x, y, ww, hh = (int(f[i]) for i in (6, 7, 8, 9, 10, 11))
            print('%d %d %d %d' % (x, y, ww, hh))
            break
        except (ValueError, IndexError):
            pass
PY
}

dump parked || { echo "FAIL: no parked dump"; exit 1; }
# nav numbers from the frame (field 6 both tags: text rows carry an
# empty action slot at field 5 - the digit-test lesson).
NAVS="$(python3 - "$WORK/frame_parked.frame.txt" <<'PY'
import sys
nav = {}
for line in open(sys.argv[1], encoding='utf-8', errors='replace'):
    f = line.rstrip('\n').split('|')
    if len(f) < 7:
        continue
    if f[0] == 'item' and f[1] == 'r1':
        nav['r1'] = f[6]
    elif f[0] == 'text' and f[1] in ('r2', 'r4'):
        nav[f[1]] = f[6]
print('%s %s %s' % (nav.get('r1', ''), nav.get('r2', ''), nav.get('r4', '')))
PY
)"
R1N="$(printf '%s' "$NAVS" | awk '{print $1}')"
R2N="$(printf '%s' "$NAVS" | awk '{print $2}')"
R4N="$(printf '%s' "$NAVS" | awk '{print $3}')"
case "$R1N" in ''|*[!0-9]*) echo "FAIL: no nav for r1 ($NAVS)"; exit 1;; esac
case "$R2N" in ''|*[!0-9]*) echo "FAIL: no nav for r2 ($NAVS)"; exit 1;; esac
case "$R4N" in ''|*[!0-9]*) echo "FAIL: no nav for r4 ($NAVS)"; exit 1;; esac
note "nav numbers: r1=$R1N r2=$R2N r4=$R4N"
R2LABEL="go to ONE then TWO then THREE now"

# S1. highlight: extend twice on r2, band pixels where parked has none.
# #2f5f8f count in the r2 band (delete-first fresh frames throughout:
# existence, not mtime, proves freshness).
bandsel() { python3 - "$1" "$2" "$3" "$4" "$5" <<'PY'
import struct, sys, zlib
def decode(png):
    d = open(png, 'rb').read()
    pos, raw = 8, b''
    while pos < len(d):
        ln = struct.unpack('>I', d[pos:pos + 4])[0]
        typ = d[pos + 4:pos + 8]
        body = d[pos + 8:pos + 8 + ln]
        if typ == b'IHDR':
            w, h, bitd, ctyp = struct.unpack('>IIBB', body[:10])
        elif typ == b'IDAT':
            raw += body
        pos += 12 + ln
    px = zlib.decompress(raw)
    ch, stride = 3, w * 3
    out = bytearray(w * h * 3)
    prev = bytearray(stride)
    i, o = 0, 0
    for _ in range(h):
        f = px[i]; i += 1
        line = bytearray(px[i:i + stride]); i += stride
        if f == 1:
            for x in range(ch, stride): line[x] = (line[x] + line[x - ch]) & 0xff
        elif f == 2:
            for x in range(stride): line[x] = (line[x] + prev[x]) & 0xff
        elif f == 3:
            for x in range(stride):
                a = line[x - ch] if x >= ch else 0
                line[x] = (line[x] + ((a + prev[x]) >> 1)) & 0xff
        elif f == 4:
            for x in range(stride):
                a = line[x - ch] if x >= ch else 0
                b, c = prev[x], prev[x - ch] if x >= ch else 0
                pp = a + b - c
                pa, pb, pc = abs(pp - a), abs(pp - b), abs(pp - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[x] = (line[x] + pr) & 0xff
        out[o:o + stride] = line; o += stride; prev = line
    return w, h, out
png, x, y, ww, hh = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
w, h, px = decode(png)
n = 0
for yy in range(max(0, y + 4), min(h, y + hh - 4)):
    for xx in range(max(0, x), min(w, x + ww)):
        i = (yy * w + xx) * 3
        if abs(px[i] - 0x2f) <= 10 and abs(px[i + 1] - 0x5f) <= 10 and abs(px[i + 2] - 0x8f) <= 10:
            n += 1
print(n)
PY
}
R2BOX="$(rowbox "$WORK/frame_parked.frame.txt" r2)"
[ -n "$R2BOX" ] || { echo "FAIL: no box for r2"; exit 1; }
PARKED_BAND="$(bandsel "$WORK/frame_parked.png" $R2BOX)"
send 27; digits "$R2N"; send 221 221
dump sel || { echo "FAIL: no selected dump"; exit 1; }
SEL_BAND="$(bandsel "$WORK/frame_sel.png" $R2BOX)"
note "r2 band #2f5f8f px: parked=$PARKED_BAND selected=$SEL_BAND"
[ "${PARKED_BAND:-x}" = 0 ] || fail "S1: parked band is not clean ($PARKED_BAND)"
[ "${SEL_BAND:-0}" -gt 100 ] || fail "S1: no highlight band ($SEL_BAND)"
[ "$FAIL" = 0 ] && note "S1: highlight band on extend"

# S2. range copy is the exact contiguous text, links as text, no URLs.
GOT="$(send 3 >/dev/null; clip)"
[ "$GOT" = "ONE then TWO" ] \
    && note "S2: clipboard is exactly [ONE then TWO]" \
    || fail "S2: clipboard was [$GOT], want [ONE then TWO]"

# S3. collapsed entry copies the whole label.
send 27; digits "$R2N"; send 221
GOT="$(send 3 >/dev/null; clip)"
[ "$GOT" = "$R2LABEL" ] \
    && note "S3: collapsed anchor copies whole label" \
    || fail "S3: clipboard was [$GOT]"

# S4. no anchor copies the whole label.
send 27
GOT="$(send 3 >/dev/null; clip)"
[ "$GOT" = "$R2LABEL" ] \
    && note "S4: Esc + copy gives whole label" \
    || fail "S4: clipboard was [$GOT]"

# S5. plain rows step, never select: focus r1, Shift+Right moves focus
# to r2, and the copy is r2's whole label (no range started).
send 27; digits "$R1N"; send 221
GOT="$(send 3 >/dev/null; clip)"
[ "$GOT" = "$R2LABEL" ] \
    && note "S5: Shift+Right on a plain row steps focus, selects nothing" \
    || fail "S5: clipboard was [$GOT]"

# S6. wrapped row: bands on 2+ line-bands, exact range copy.
R4BOX="$(rowbox "$WORK/frame_parked.frame.txt" r4)"
[ -n "$R4BOX" ] || { echo "FAIL: no box for r4"; exit 1; }
send 27; digits "$R4N"; send 221 221
dump selw || { echo "FAIL: no wrapped dump"; exit 1; }
NLINES="$(python3 - "$WORK/frame_selw.png" $R4BOX <<'PY'
import struct, sys, zlib
def decode(png):
    d = open(png, 'rb').read()
    pos, raw = 8, b''
    while pos < len(d):
        ln = struct.unpack('>I', d[pos:pos + 4])[0]
        typ = d[pos + 4:pos + 8]
        body = d[pos + 8:pos + 8 + ln]
        if typ == b'IHDR':
            w, h, bitd, ctyp = struct.unpack('>IIBB', body[:10])
        elif typ == b'IDAT':
            raw += body
        pos += 12 + ln
    px = zlib.decompress(raw)
    ch, stride = 3, w * 3
    out = bytearray(w * h * 3)
    prev = bytearray(stride)
    i, o = 0, 0
    for _ in range(h):
        f = px[i]; i += 1
        line = bytearray(px[i:i + stride]); i += stride
        if f == 1:
            for x in range(ch, stride): line[x] = (line[x] + line[x - ch]) & 0xff
        elif f == 2:
            for x in range(stride): line[x] = (line[x] + prev[x]) & 0xff
        elif f == 3:
            for x in range(stride):
                a = line[x - ch] if x >= ch else 0
                line[x] = (line[x] + ((a + prev[x]) >> 1)) & 0xff
        elif f == 4:
            for x in range(stride):
                a = line[x - ch] if x >= ch else 0
                b, c = prev[x], prev[x - ch] if x >= ch else 0
                pp = a + b - c
                pa, pb, pc = abs(pp - a), abs(pp - b), abs(pp - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[x] = (line[x] + pr) & 0xff
        out[o:o + stride] = line; o += stride; prev = line
    return w, h, out
png, x, y, ww, hh = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
w, h, px = decode(png)
def isband(yy):
    for xx in range(max(0, x), min(w, x + ww), 3):
        i = (yy * w + xx) * 3
        if abs(px[i] - 0x2f) <= 10 and abs(px[i + 1] - 0x5f) <= 10 and abs(px[i + 2] - 0x8f) <= 10:
            return True
    return False
print(sum(1 for yy in range(max(0, y + 4), min(h, y + hh - 4), 12) if isband(yy)))
PY
)"
note "wrapped highlight line-bands: $NLINES"
[ "${NLINES:-0}" -ge 2 ] || fail "S6: wrapped band is not on 2+ lines ($NLINES)"
GOT="$(send 3 >/dev/null; clip)"
WANT="ALPHA middle words here and here plus a few more words to pad the row BETA"
[ "$GOT" = "$WANT" ] \
    && note "S6: wrapped range copy is exact" \
    || fail "S6: clipboard was [$GOT]"

# S7. Enter after extending dispatches the cursor link (composition).
rm -f "$WORK/click.log"
send 27; digits "$R2N"; send 203 203 13
sleep 1
grep -q 'CLICKED LINK2' "$WORK/click.log" 2>/dev/null \
    && note "S7: Right+Right+Enter dispatched LINK2" \
    || fail "S7: expected LINK2 (log: $(cat "$WORK/click.log" 2>/dev/null))"

# S8. clicks clear the range: extend, relay-click the row's text span
# (left part, never a link), copy the whole label afterwards.
send 27; digits "$R2N"; send 221 221
set -- $R2BOX
printf 'MOUSE_EVENT: 1 %d %d 1\n' "$(($1 + 10))" "$(($2 + 15))" >> "$HIST"
sleep 1.5
GOT="$(send 3 >/dev/null; clip)"
[ "$GOT" = "$R2LABEL" ] \
    && note "S8: click cleared the range, copy is whole label" \
    || fail "S8: clipboard was [$GOT]"

kill $RPID 2>/dev/null
sleep 1
if kill -0 $RPID 2>/dev/null; then fail "fixture renderer still alive"; fi

[ "$FAIL" -eq 0 ] && echo "nb_span_select_test.sh PASS"
exit $FAIL
