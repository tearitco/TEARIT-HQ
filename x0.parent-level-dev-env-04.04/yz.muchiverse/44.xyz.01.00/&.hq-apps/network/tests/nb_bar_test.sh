#!/bin/bash
# nb_bar_test.sh - pins the video-playhead <bar> rendering.
#
# Two behaviors, both pixel-proven on a hermetic fixture window:
#   FILL: bar_value/bar_max survive the frame round trip (they never
#   did - every default-mode bar drew track-only while clicks, which
#   use the live tree, worked fine). value 0/50/100 give none/half/
#   full green fill; max=0 and garbage values give track-only, never a
#   crash or a garbage span.
#   LABEL: the centered mm:ss label splits at the fill edge - the part
#   over the fill draws dark-on-green, the rest as before. A single
#   color read ~2.4:1 over the fill; the split reads 4.8/9.9.
#
# Fixture rows (from the ascii frame's own boxes, never hardcoded):
#   b0 value=0, b1 value=50, b2 value=100, b3 max=0, all max=100.
# Negative: value="abc" (atoi 0) renders track-only like max=0.
#
# Usage: sh nb_bar_test.sh
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HR="$(cd "$HERE/../../.." && pwd)"
RENDER="$HR/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
WORK=/tmp/nb_bar_test
FAIL=0

note() { printf '%s\n' "$*"; }
fail() { echo "FAIL: $*"; FAIL=1; }

[ -x "$RENDER" ] || { echo "FAIL: renderer not built ($RENDER)"; exit 1; }
command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }

rm -rf "$WORK"; mkdir -p "$WORK"
KILLPIDS=""
trap 'for p in $KILLPIDS; do kill "$p" 2>/dev/null; done' EXIT

cat > "$WORK/bar.xhtpm" <<'EOF'
<window label="BarFix" class="barfix-fixture database-window">
  <page id="main">
    <scrolllist class="nb-content">
    <bar id="b0" value="0" max="100" label="00:00"/>
    <bar id="b1" value="50" max="100" label="00:30"/>
    <bar id="b2" value="100" max="100" label="01:00"/>
    <bar id="b3" value="30" max="0" label="xx:xx"/>
    <bar id="b4" value="abc" max="100" label="yy:yy"/>
    </scrolllist>
  </page>
</window>
EOF
cat > "$WORK/bar.css" <<'EOF'
window { width: 760px; height: 300px; }
window.barfix-fixture { width: 760px; height: 300px; background-color: #141414; color: #dddddd; }
.nb-content { background-color: #1a1a1a; color: #cccccc; }
EOF

setsid "$RENDER" "$HR" "$WORK/bar.xhtpm" "$WORK" >"$WORK/render.log" 2>&1 &
RPID=$!
KILLPIDS="$KILLPIDS $RPID"
sleep 6
if ! kill -0 "$RPID" 2>/dev/null; then
    echo "FAIL: fixture renderer died"; head -5 "$WORK/render.log"; exit 1
fi
HIST="$HR/#.desktop/entity_menu_history/$RPID.txt"
WID="$(xwininfo -root -tree 2>/dev/null | grep '(has no name)' | grep '764x304' | awk '{print $1}' | head -1)"
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
PY
sleep 1
rm -f /tmp/entity-menu-frame.png
printf '112\n' >> "$HIST"
for _ in $(seq 1 20); do
    [ -f /tmp/entity-menu-frame.png ] && break
    sleep 0.5
done
[ -f /tmp/entity-menu-frame.png ] || { echo "FAIL: no frame dump"; exit 1; }
sleep 1
cp /tmp/entity-menu-frame.png "$WORK/bar.png"
cp /tmp/entity-menu-frame.png.frame.txt "$WORK/bar.frame.txt"

# scan <png> <frame> <id>: "fill <n>" (green px in the row box) and
# "ink <dark> <light>" (glyph-ink px left/right of the fill edge,
# playhead +-3px excluded). Fill edge = x + w*value/max from the
# fixture constants below (the frame carries no value/max).
python3 - "$WORK/bar.png" "$WORK/bar.frame.txt" > "$WORK/bar.scan" <<'PY' || exit 1
import struct, sys, zlib
VALS = {'b0': (0, 100), 'b1': (50, 100), 'b2': (100, 100), 'b3': (30, 0), 'b4': (0, 100)}
def decode(png):
    d = open(png, 'rb').read()
    assert d[:8] == b'\x89PNG\r\n\x1a\n'
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
    assert bitd == 8 and ctyp == 2
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
def lum(r, g, b):
    f = lambda v: (v / 12.92 if v <= 0.03928 else ((v + 0.055) / 1.055) ** 2.4)
    return 0.2126 * f(r / 255) + 0.7152 * f(g / 255) + 0.0722 * f(b / 255)
png, frame = sys.argv[1], sys.argv[2]
w, h, px = decode(png)
def P(x, y):
    x = max(0, min(w - 1, x)); y = max(0, min(h - 1, y))
    i = (y * w + x) * 3
    return (px[i], px[i + 1], px[i + 2])
for line in open(frame, encoding='utf-8', errors='replace'):
    f = line.rstrip('\n').split('|')
    if not f or f[0] != 'bar' or len(f) < 2 or f[1] not in VALS:
        continue
    rid = f[1]
    try:
        # front fields (see themescan.py: end-peel breaks on every
        # frame-tail extension; our labels carry no pipes).
        x, y, ww, hh = (int(f[i]) for i in (8, 9, 10, 11))
    except (ValueError, IndexError):
        print('FAIL: geometry peel failed on %s' % rid)
        sys.exit(1)
    v, m = VALS[rid]
    edge = x + (ww * v // m if m > 0 else 0)
    fill = darkL = lightL = darkR = lightR = 0
    for yy in range(max(0, y + 4), min(h, y + hh - 4)):
        # runs by local similarity; surfaces (track/bg) run long,
        # glyphs and chrome run short - width separates them without
        # any assumed background color. Fill runs are long too, so
        # they are counted before the width gate, not through it.
        xx = max(0, x)
        xe = min(w, x + ww)
        while xx < xe:
            c0 = P(xx, yy)
            x0 = xx
            while xx < xe and abs(P(xx, yy)[0] - c0[0]) + abs(P(xx, yy)[1] - c0[1]) + abs(P(xx, yy)[2] - c0[2]) <= 60:
                xx += 1
            width = xx - x0
            if abs(c0[0] - 0x2f) <= 20 and abs(c0[1] - 0x8f) <= 20 and abs(c0[2] - 0x5f) <= 20:
                fill += width
                continue
            # glyph cores are often 1-2px islands: no width floor
            # (speckle is absorbed by count thresholds an order of
            # magnitude below real glyph masses).
            if width > 120:
                continue
            if c0 == (20, 20, 20):
                continue  # badge backing chip, not text

            # the 1px playhead (#ffcc00) is the only
            # saturated-bright thing here; light glyphs are neutral.
            saturated = (c0[0] - c0[2]) > 100
            L = lum(*c0)
            neutral = abs(c0[0] - c0[1]) < 30 and abs(c0[1] - c0[2]) < 30
            mid = x0 + width // 2
            if mid < edge:
                dfill = abs(c0[0] - 0x2f) + abs(c0[1] - 0x8f) + abs(c0[2] - 0x5f)
                if dfill < 60:
                    continue
                if L < 0.12 and neutral:
                    darkL += width
                elif L > 0.4 and not saturated:
                    lightL += width
            else:
                dtrack = abs(c0[0] - 0x22) + abs(c0[1] - 0x22) + abs(c0[2] - 0x22)
                if dtrack < 60:
                    continue
                if L < 0.12 and neutral:
                    darkR += width
                elif L > 0.4 and not saturated:
                    lightR += width
    print('ROW %s fill=%d darkL=%d lightL=%d darkR=%d lightR=%d edge=%d' % (
        rid, fill, darkL, lightL, darkR, lightR, edge))
PY
python3 - "$WORK/bar.scan" <<'PY' || exit 1
import sys
rows = {}
cat = open(sys.argv[1]).read()
print(cat, end='' if cat.endswith('\n') else '\n')
for line in cat.split('\n'):
    p = line.rstrip('\n').split(' ')
    if p[0] != 'ROW' or len(p) != 8:
        continue
    rows[p[1]] = {k: int(v.split('=')[1]) for k, v in zip(
        ['fill', 'darkL', 'lightL', 'darkR', 'lightR', 'edge'], p[2:])}
for r in ('b0', 'b1', 'b2', 'b3', 'b4'):
    assert r in rows, 'row %s missing' % r
b0, b1, b2, b3, b4 = (rows[r] for r in ('b0', 'b1', 'b2', 'b3', 'b4'))
assert b0['fill'] == 0, b0
assert b3['fill'] == 0, b3
assert b4['fill'] == 0, b4
assert b1['fill'] > 200, b1
assert b2['fill'] > 600, b2
print('fills: none/half/full/degenerate all correct')
# darkR is deliberately unasserted: antialiased fringes of light
# glyphs read dark-neutral, so it cannot distinguish track from text.
# lightR>10 already proves the right part drew light (a dark-on-track
# bug would leave it near zero).
assert b1['darkL'] > 10, b1
assert b1['lightR'] > 10, b1
assert b1['lightL'] < 15, b1
assert b2['darkL'] > 10, b2
assert b2['lightR'] < 15, b2
assert b0['lightR'] > 10 and b0['darkL'] < 10, b0
assert b3['lightR'] > 10 and b3['darkL'] < 10, b3
assert b4['lightR'] > 10 and b4['darkL'] < 10, b4
print('labels: two-tone split + single-color ends all correct')
PY
kill $RPID 2>/dev/null
sleep 1
if kill -0 $RPID 2>/dev/null; then fail "fixture renderer still alive"; fi

[ "$FAIL" -eq 0 ] && echo "nb_bar_test.sh PASS"
exit $FAIL
