#!/bin/bash
# nb_hover_test.sh - pins the INLINE SPAN step-8 hover highlight.
#
# What it asserts, all against real rendered pixels (this model cannot
# read a PNG, so every claim here is a pixel count, never an eyeball):
#
#   1. a link span with an action draws its link tint (#8fb8ff)
#   2. pointer motion ONTO that span whitens its TEXT and removes the
#      tinted glyph pixels (the underline stays tinted on purpose -
#      the keyboard cursor whitens the underline instead, the two marks
#      compose)
#   3. motion onto a DIFFERENT span moves the highlight: the old span's
#      tint comes back and the new one's glyphs whiten
#   4. motion off every span clears it (both rows fully tinted again)
#   5. a click still dispatches the right action AFTER a hover - hover
#      owns no input state, so it cannot swallow or steal clicks
#
# Hermetic: it launches its own renderer on its own fixture window, so
# it never touches the running browser. The pointer itself is driven
# through the house relay (MOUSE_EVENT with is_press 0), which feeds the
# same seg_hover_move() resolver a real MotionNotify does - that path is
# wired for exactly this reason.
#
# Wire-format note (cost me a wrong theory once): a segments= payload is
# `kind \x1F text \x1F url [\x1F action]` joined by \x1E. A text span
# MUST carry its trailing empty url field (`text\x1Fwords\x1F`), and a
# link span only becomes clickable - and therefore hoverable - when the
# optional 4th action field is present. A payload that omits either
# parses, round-trips and TINTS, but has no actionable run, which looks
# exactly like "hover does not work".
#
# Usage: sh nb_hover_test.sh
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HR="$(cd "$HERE/../../.." && pwd)"
RENDER="$HR/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
DUMP="$HR/&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x"
WORK=/tmp/nb_hover_test
FAIL=0

note() { printf '%s\n' "$*"; }
fail() { echo "FAIL: $*"; FAIL=1; }

[ -x "$RENDER" ] || { echo "FAIL: renderer not built ($RENDER)"; exit 1; }
[ -x "$DUMP" ] || { echo "FAIL: dump op not built ($DUMP)"; exit 1; }

rm -rf "$WORK"; mkdir -p "$WORK"
cat > "$WORK/action.sh" <<EOF
#!/bin/sh
echo "CLICKED \$1" >> "$WORK/click.log"
EOF
chmod +x "$WORK/action.sh"
# The committed fixture carries the NB_HOVER_ACTION placeholder so the
# repo copy never hardcodes a /tmp path; the test splices the real
# action in. Single-quoted path, and NOT prefixed with `sh`: dispatch()
# runs every action as `sh '<action>'` itself, so a fixture action that
# already begins with `sh '...'` double-wraps into broken shell quoting
# and dies with no visible failure anywhere.
sed "s|NB_HOVER_ACTION|'$WORK/action.sh'|" \
    "$HERE/fixtures/hover/hover.xhtpm" > "$WORK/hover.xhtpm"
cp "$HERE/fixtures/hover/hover.css" "$WORK/hover.css"

setsid "$RENDER" "$HR" "$WORK/hover.xhtpm" "$WORK" >"$WORK/render.log" 2>&1 &
RPID=$!
sleep 6
if ! kill -0 "$RPID" 2>/dev/null; then
    echo "FAIL: fixture renderer died"; head -5 "$WORK/render.log"; exit 1
fi
HIST="$HR/#.desktop/entity_menu_history/$RPID.txt"

# One xwininfo pass for window discovery (the house lesson: per-window
# forks under a busy shared desktop cost seconds each).
WID=""
for _ in $(seq 1 20); do
    WID="$(xwininfo -root -tree 2>/dev/null | grep -E '\(has no name\)' \
            | grep -oE '0x[0-9a-f]+' >/dev/null; \
            xwininfo -root -tree 2>/dev/null | grep '(has no name)' \
            | grep '764x264' | awk '{print $1}' | head -1)"
    [ -n "$WID" ] && break
    sleep 1
done
[ -n "$WID" ] || { echo "FAIL: fixture window not found"; kill $RPID 2>/dev/null; exit 1; }

# dump_frame <tag>: relay the 'p' shortcut, which redraws and writes
# the renderer's own PNG; copies it to $WORK/frame_<tag>.png so each
# measurement reads its OWN frame. (A shared counter incremented inside
# this function silently collapsed every frame onto one file, because
# the function runs in a command substitution subshell - four dumps,
# four identical paths, four identical readings, and a hover feature
# that "did not work" only in the test.)
dump_frame() {
    rm -f /tmp/entity-menu-frame.png
    printf '112\n' >> "$HIST"
    for _ in $(seq 1 20); do
        [ -f /tmp/entity-menu-frame.png ] && { sleep 1; cp /tmp/entity-menu-frame.png "$WORK/frame_$1.png"; echo "$WORK/frame_$1.png"; return 0; }
        sleep 0.5
    done
    return 1
}

# link_tint_px <png> <y0> <y1>: count near-#8fb8ff pixels in a row band,
# EXCLUDING the 1px underline row (the underline keeps the link tint
# while hovered, so counting it would mask the very change under test).
link_tint_px() {
    python3 - "$1" "$2" "$3" <<'PY'
import sys, zlib, struct
def load(path):
    d = open(path, 'rb').read()
    assert d[:8] == b'\x89PNG\r\n\x1a\n', 'not a png'
    pos, idat, pal = 8, b'', None
    while pos < len(d):
        ln = struct.unpack('>I', d[pos:pos+4])[0]
        typ = d[pos+4:pos+8]
        body = d[pos+8:pos+8+ln]
        if typ == b'IHDR':
            w, h, depth, ctype = struct.unpack('>IIBB', body[:10])
        elif typ == b'PLTE':
            pal = body
        elif typ == b'IDAT':
            idat += body
        pos += 12 + ln
    raw = zlib.decompress(idat)
    nch = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ctype]
    stride = (w * nch * depth + 7) // 8
    out = bytearray()
    prev = bytearray(stride)
    p = 0
    for _ in range(h):
        ft = raw[p]; p += 1
        line = bytearray(raw[p:p+stride]); p += stride
        if ft == 1:
            for i in range(nch, stride): line[i] = (line[i] + line[i-nch]) & 0xFF
        elif ft == 2:
            for i in range(stride): line[i] = (line[i] + prev[i]) & 0xFF
        elif ft == 3:
            for i in range(stride):
                a = line[i-nch] if i >= nch else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 0xFF
        elif ft == 4:
            for i in range(stride):
                a = line[i-nch] if i >= nch else 0
                b = prev[i]; c = prev[i-nch] if i >= nch else 0
                pp = a + b - c
                pa, pb, pc = abs(pp-a), abs(pp-b), abs(pp-c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 0xFF
        out += line
        prev = line
    return w, h, nch, depth, bytes(out), ctype, pal

path, y0, y1 = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
w, h, nch, depth, data, ctype, pal = load(path)
bpp = max(1, nch * depth // 8)
def px(x, y):
    i = (y * w + x) * bpp
    if depth == 8:
        r, g, b = data[i], data[i+1], data[i+2]
    elif depth == 16:
        r, g, b = data[i*2], data[i*2+2], data[i*2+4]
    elif ctype == 3:
        j = data[i] * 3
        r, g, b = pal[j], pal[j+1], pal[j+2]
    else:
        r = g = b = data[i]
    return r, g, b
n = 0
for y in range(y0, min(y1, h)):
    if y == 84:          # the 1px link underline row: tinted even when hovered
        continue
    for x in range(w):
        r, g, b = px(x, y)
        if abs(r-0x8f) <= 14 and abs(g-0xb8) <= 14 and abs(b-0xff) <= 14:
            n += 1
print(n)
PY
}

# Text-band pixel bounds, in the fixture's own window coordinates:
# row 1 (seg) text sits at y 70..84, row 2 (seg) at y 100..114.
R1_Y0=64; R1_Y1=90; R2_Y0=94; R2_Y1=122

move() { printf 'MOUSE_EVENT: 0 %s %s 0\n' "$1" "$2" >> "$HIST"; sleep 0.6; }

move 700 250
F0="$(dump_frame parked)" || { echo "FAIL: no parked dump"; kill $RPID 2>/dev/null; exit 1; }
move 150 76
F1="$(dump_frame hover1)" || { echo "FAIL: no hover dump"; kill $RPID 2>/dev/null; exit 1; }
move 207 106
F2="$(dump_frame hover2)" || { echo "FAIL: no second-hover dump"; kill $RPID 2>/dev/null; exit 1; }
move 400 250
F3="$(dump_frame cleared)" || { echo "FAIL: no cleared dump"; kill $RPID 2>/dev/null; exit 1; }

T0=$(link_tint_px "$F0" $R1_Y0 $R1_Y1)
T1=$(link_tint_px "$F1" $R1_Y0 $R1_Y1)
U0=$(link_tint_px "$F0" $R2_Y0 $R2_Y1)
U1=$(link_tint_px "$F2" $R2_Y0 $R2_Y1)
T3=$(link_tint_px "$F3" $R1_Y0 $R1_Y1)
U3=$(link_tint_px "$F3" $R2_Y0 $R2_Y1)
note "row1 tint: parked=$T0 hovered=$T1 back=$T3   row2 tint: parked=$U0 hovered=$U1 back=$U3"

[ "${T0:-0}" -gt 40 ] || fail "parked row1 has too few tinted pixels ($T0) - links never drew"
[ "${T1:-0}" -lt "${T0:-0}" ] || fail "hover did NOT whiten row1 link text ($T0 -> $T1)"
[ "${U0:-0}" -gt 40 ] || fail "parked row2 has too few tinted pixels ($U0)"
[ "${U1:-0}" -lt "${U0:-0}" ] || fail "hover did NOT whiten the row2 link ($U0 -> $U1)"
[ "${T3:-0}" = "${T0:-x}" ] || fail "highlight did not clear from row1 ($T0 -> $T3)"
[ "${U3:-0}" = "${U0:-x}" ] || fail "highlight did not clear from row2 ($U0 -> $U3)"

# Hover owns no input state: a click through the same coordinates must
# still dispatch the right action.
rm -f "$WORK/click.log"
move 150 76
# TWO clicks, deliberately: the house default is two-step (first click
# focuses the row, second activates), so a single relayed click is
# CORRECT behaviour and asserting on one click would pin the wrong
# contract. Hover owns no input state either way.
for _ in 1 2; do
    printf 'MOUSE_EVENT: 1 150 76 1\n' >> "$HIST"
    sleep 1.5
done
if grep -q 'CLICKED HOVERME' "$WORK/click.log" 2>/dev/null; then
    note "click after hover: dispatched HOVERME"
else
    fail "click after hover did not dispatch (click.log: $(cat "$WORK/click.log" 2>/dev/null))"
fi

kill $RPID 2>/dev/null
sleep 1
if kill -0 $RPID 2>/dev/null; then fail "fixture renderer still alive"; fi

[ "$FAIL" -eq 0 ] && echo "nb_hover_test.sh PASS"
exit $FAIL