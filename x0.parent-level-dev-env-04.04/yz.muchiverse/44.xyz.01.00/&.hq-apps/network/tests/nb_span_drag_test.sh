#!/bin/bash
# nb_span_drag_test.sh - pins INLINE SPAN step-11 mouse drag-select.
#
# A real button-1 press on a link span ARMS a deferred click instead of
# dispatching: release on the same link completes it, motion past slop
# first turns it into a span selection (anchor holds the press link).
# Relay clicks carry no release, so they keep instant dispatch.
#
# Hermetic: own fixture window (one 3-link row, actions to click.log).
# Realness comes from synthetic XSendEvent press/motion/release
# (tests/xsendevent.py, stdlib ctypes - our dispatcher never checks
# send_event, so these drive the REAL handler path). Link geometry
# comes from the parked capture's own blue underlines, never hardcoded.
# Copied bytes come back through the live X clipboard (xclipget.py).
#
# Steps (Esc hygiene first):
#   T1. press+release on link1, no motion -> LINK1 (deferred click)
#   T2. press link1, motions to link3, release -> no dispatch, full
#       band, clipboard is exactly "ONE then TWO then THREE"
#   T3. press, 2px jitter, release -> LINK1 (slop, still a click)
#   T4. press link1, release off-row -> no dispatch, copy is whole label
#   T5. relay press (no token) -> instant LINK dispatch (protocol compat)
#   T6. press with NO release -> nothing (pairing required); late
#       release on the same link -> LINK dispatch (pending survives)
#
# Usage: sh nb_span_drag_test.sh
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HR="$(cd "$HERE/../../.." && pwd)"
RENDER="$HR/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
WORK=/tmp/nb_span_drag
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
# Segments wire format + BARE action commands (the digit-test lessons).
python3 - "$WORK/drag.xhtpm" "$WORK/action.sh" <<'PY'
import sys
FS, J = '\x1f', '\x1e'
def T(t): return 'text' + FS + t + FS
def L(t, a): return 'link' + FS + t + FS + 'https://example.com/' + t + FS + a
A = "'" + sys.argv[2] + "'"
segs = J.join([T('go to '), L('ONE', A + " 'LINK1'"), T(' then '),
               L('TWO', A + " 'LINK2'"), T(' then '),
               L('THREE', A + " 'LINK3'"), T(' now')])
doc = ('<window label="SpanDrag" class="spandrag-fixture database-window">\n'
       '  <page id="main">\n'
       '    <text id="m1" class="seg" label="go to ONE then TWO then THREE now" segments="%s"/>\n'
       '  </page>\n</window>\n' % segs)
open(sys.argv[1], 'w').write(doc)
PY
cat > "$WORK/drag.css" <<'EOF'
window { width: 760px; height: 260px; }
window.spandrag-fixture { width: 760px; height: 260px; background-color: #141414; color: #dddddd; }
.seg { color: #cccccc; }
EOF

setsid "$RENDER" "$HR" "$WORK/drag.xhtpm" "$WORK" >"$WORK/render.log" 2>&1 &
RPID=$!
KILLPIDS="$KILLPIDS $RPID"
sleep 6
if ! kill -0 "$RPID" 2>/dev/null; then
    echo "FAIL: fixture renderer died"; head -5 "$WORK/render.log"; exit 1
fi
HIST="$HR/#.desktop/entity_menu_history/$RPID.txt"
# Window id by geometry (the fixture is the only 764x264 unnamed window
# we launched; hover/digit-test precedent), then raise: XGetImage reads
# screen pixels, so an obscured window captures its occluder.
WID=""
for _ in $(seq 1 20); do
    WID="$(xwininfo -root -tree 2>/dev/null | grep '(has no name)' | grep '764x264' | awk '{print $1}' | head -1)"
    [ -n "$WID" ] && break
    sleep 1
done
[ -n "$WID" ] || { echo "FAIL: fixture window not found"; exit 1; }
note "fixture window $WID"
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
PY
xe() { python3 "$HERE/xsendevent.py" "$WID" "$@"; sleep 0.6; }
dump() {  # dump <tag>
    rm -f /tmp/entity-menu-frame.png
    printf '112\n' >> "$HIST"
    for _ in $(seq 1 20); do
        [ -f /tmp/entity-menu-frame.png ] && { sleep 1; cp /tmp/entity-menu-frame.png "$WORK/frame_$1.png"; return 0; }
        sleep 0.5
    done
    return 1
}
clip() {
    C="$(python3 "$HERE/xclipget.py" 2>/dev/null)" && { printf '%s' "$C"; return 0; }
    sleep 2
    python3 "$HERE/xclipget.py" 2>/dev/null
}
# linkcenters <png>: centers of the 3 link underlines (densest blue row).
linkcenters() { python3 - "$1" <<'PY' || exit 1
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
w, h, px = decode(sys.argv[1])
def blue(x, y):
    i = (y * w + x) * 3
    return abs(px[i] - 0x8f) <= 14 and abs(px[i + 1] - 0xb8) <= 14 and abs(px[i + 2] - 0xff) <= 14
best, bestn = 0, -1
for y in range(h):
    n = sum(1 for x in range(w) if blue(x, y))
    if n > bestn: best, bestn = y, n
runs = []
s = None
for x in range(w):
    if blue(x, best) and s is None: s = x
    if not blue(x, best) and s is not None: runs.append((s, x - 1)); s = None
if s is not None: runs.append((s, w - 1))
runs = [(a, b) for a, b in runs if b - a >= 2]
if len(runs) != 3:
    print('FAIL: want 3 link underlines, found %d' % len(runs))
    sys.exit(1)
ys = best
print('%d %d %d %d' % ((runs[0][0] + runs[0][1]) // 2, (runs[1][0] + runs[1][1]) // 2, (runs[2][0] + runs[2][1]) // 2, ys))
PY
}

rm -f "$WORK/click.log"
dump parked || { echo "FAIL: no parked dump"; exit 1; }
LC="$(linkcenters "$WORK/frame_parked.png")" || exit 1
L1X="$(printf '%s' "$LC" | awk '{print $1}')"
L3X="$(printf '%s' "$LC" | awk '{print $3}')"
LY="$(printf '%s' "$LC" | awk '{print $4}')"
note "link centers: one=$L1X three=$L3X y=$LY (from parked underlines)"

# T1. synthetic click (press+release, no motion) dispatches.
xe press "$L1X" "$LY"; xe release "$L1X" "$LY"
sleep 1
grep -q 'CLICKED LINK1' "$WORK/click.log" 2>/dev/null \
    && note "T1: press+release dispatched LINK1" \
    || fail "T1: expected LINK1 (log: $(cat "$WORK/click.log" 2>/dev/null))"

# T2. drag across to link 3: no dispatch, full band, exact range copy.
N0="$(wc -l < "$WORK/click.log" 2>/dev/null || echo 0)"
xe press "$L1X" "$LY"
xe move "$(( (L1X + L3X) / 2 ))" "$LY"
xe move "$L3X" "$LY"
xe release "$L3X" "$LY"
sleep 1
N1="$(wc -l < "$WORK/click.log" 2>/dev/null || echo 0)"
[ "$N1" = "$N0" ] \
    && note "T2: drag dispatched nothing" \
    || fail "T2: drag dispatched (log: $(cat "$WORK/click.log" 2>/dev/null))"
dump drag || { echo "FAIL: no drag dump"; exit 1; }
BAND="$(python3 - "$WORK/frame_drag.png" "$L1X" "$L3X" "$LY" <<'PY'
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
png, x0, x1, y = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
w, h, px = decode(png)
n = 0
for yy in range(max(0, y - 20), min(h, y + 20)):
    for xx in range(max(0, x0 - 10), min(w, x1 + 10)):
        i = (yy * w + xx) * 3
        if abs(px[i] - 0x2f) <= 10 and abs(px[i + 1] - 0x5f) <= 10 and abs(px[i + 2] - 0x8f) <= 10:
            n += 1
print(n)
PY
)"
note "drag band px: $BAND"
[ "${BAND:-0}" -gt 300 ] || fail "T2: no highlight band ($BAND)"
GOT="$(printf '3\n' >> "$HIST"; sleep 1; clip)"
[ "$GOT" = "ONE then TWO then THREE" ] \
    && note "T2: drag copy is exact" \
    || fail "T2: clipboard was [$GOT]"
[ "$FAIL" = 0 ] || true

# T3. sub-slop jitter is still a click.
N0="$(wc -l < "$WORK/click.log")"
xe press "$L1X" "$LY"; xe move "$((L1X + 2))" "$((LY + 1))"; xe release "$((L1X + 2))" "$((LY + 1))"
sleep 1
grep -q 'CLICKED LINK1' "$WORK/click.log" 2>/dev/null \
    && [ "$(wc -l < "$WORK/click.log")" = "$((N0 + 1))" ] \
    && note "T3: 2px jitter still dispatched once" \
    || fail "T3: expected one more LINK1 (log: $(cat "$WORK/click.log" 2>/dev/null))"

# T4. release off-row: no dispatch, selection cleared.
xe press "$L1X" "$LY"; xe release 700 200
sleep 1
N1="$(wc -l < "$WORK/click.log")"
[ "$N1" = "$((N0 + 1))" ] \
    && note "T4: off-row release dispatched nothing" \
    || fail "T4: unexpected dispatch (log: $(cat "$WORK/click.log" 2>/dev/null))"
GOT="$(printf '3\n' >> "$HIST"; sleep 1; clip)"
[ "$GOT" = "go to ONE then TWO then THREE now" ] \
    && note "T4: copy after is the whole label" \
    || fail "T4: clipboard was [$GOT]"

# T5. relay press (no token) still dispatches instantly, no release needed.
N0="$(wc -l < "$WORK/click.log")"
printf 'MOUSE_EVENT: 1 %s %s 1\n' "$L1X" "$LY" >> "$HIST"
sleep 2
[ "$(wc -l < "$WORK/click.log")" = "$((N0 + 1))" ] \
    && grep -q 'CLICKED LINK1' "$WORK/click.log" \
    && note "T5: relay click dispatched instantly" \
    || fail "T5: relay click did nothing (log: $(cat "$WORK/click.log" 2>/dev/null))"

# T6. press alone dispatches nothing; the late release completes it.
N0="$(wc -l < "$WORK/click.log")"
xe press "$L1X" "$LY"
sleep 1
[ "$(wc -l < "$WORK/click.log")" = "$N0" ] \
    && note "T6: press alone dispatched nothing" \
    || fail "T6: press dispatched early (log: $(cat "$WORK/click.log" 2>/dev/null))"
xe release "$L1X" "$LY"
sleep 1
[ "$(wc -l < "$WORK/click.log")" = "$((N0 + 1))" ] \
    && note "T6: late release completed the click" \
    || fail "T6: late release did nothing (log: $(cat "$WORK/click.log" 2>/dev/null))"

kill $RPID 2>/dev/null
sleep 1
if kill -0 $RPID 2>/dev/null; then fail "fixture renderer still alive"; fi

[ "$FAIL" -eq 0 ] && echo "nb_span_drag_test.sh PASS"
exit $FAIL
