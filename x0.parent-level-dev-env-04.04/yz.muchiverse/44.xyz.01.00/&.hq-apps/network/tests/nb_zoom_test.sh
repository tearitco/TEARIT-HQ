#!/bin/bash
# nb_zoom_test.sh - pins PAGE ZOOM (G3.2): Ctrl+= / Ctrl+- / Ctrl+0.
#
# Per-window TEXT zoom. Glyphs grow, the column does not, and a wrapped
# row gets TALLER because it needs more visual lines. That is deliberately
# not a full-page zoom: ROW_H is a `scaled(24)` macro, so scaling inside
# scaled() would also move the row pitch and with it every nav slot,
# scroll offset and hit-test rect in the window (see zoomed() in
# khtpm_core_render.c).
#
# It is also deliberately NOT the house UI scale. That one was measured
# shrinking text: one Ctrl+= took a 60px content row to 46px and a wrapped
# paragraph onto one line (gap plan §8.5). This knob writes no pdl and
# affects no other window, which the last assertion here pins.
#
# Three independent channels, so a zoom that moves one of them cannot pass:
#   STATE   the mirror's [ZOOM] line
#   GEOM    a row's HEIGHT in the 206 full-tree dump - grows only if the
#           wrapper re-measured the new font against the same column.
#           Note what these two do and do NOT cover: layout's wrap_line_count()
#           and the draw both resolve their font through font_for()'s single
#           cache, so "layout zooms but the draw does not" is not reachable
#           from here - the first version of this suite claimed to catch it
#           and a mutant proved it could not. What the pair DOES catch is a
#           zoom that is wired to nothing: negative-tested, and the state
#           assertions still pass while geometry and pixels fail, which is
#           precisely the failure a state-only suite would have shipped.
#   PIXELS  real ink in the SAME row before and after - grows only if the
#           glyphs themselves got bigger
#
# HERMETIC: its own fixture window, reusing the selrun fixture (it already
# has rows long enough to wrap at every scale this suite visits).
#
# Usage: sh nb_zoom_test.sh
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HR="$(cd "$HERE/../../.." && pwd)"
RENDER="$HR/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
WORK=/tmp/nb_zoom_test
FRAME=/tmp/entity-menu-frame.png.frame.txt
RECEIPT=/tmp/entity-menu-frame.png.receipt.txt
PDL="$HR/#.desktop/hq_ui.pdl"
FAIL=0
fail() { echo "FAIL: $*"; FAIL=1; }

[ -x "$RENDER" ] || { echo "FAIL: renderer not built ($RENDER)"; exit 1; }

# The house scale must be IDENTICAL afterwards. Page zoom persists nothing,
# so this is a real assertion about the feature, not just tidiness - it is
# what makes this knob safe to use on any window at any time.
HOUSE_SCALE_BEFORE="$(grep -m1 '^font_scale=' "$PDL" 2>/dev/null | cut -d= -f2)"

rm -rf "$WORK"; mkdir -p "$WORK"
cp "$HERE/fixtures/selrun/selrun.xhtpm" "$WORK/selrun.xhtpm"
setsid "$RENDER" "$HR" "$WORK/selrun.xhtpm" "$WORK" >"$WORK/render.log" 2>&1 &
RPID=$!
trap 'kill ${RPID:-0} 2>/dev/null' EXIT INT TERM
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

zoom() { grep '\[ZOOM\]' "$MIRROR" 2>/dev/null | head -1 | sed -n 's/.*page=\([0-9]*\)%.*/\1/p'; }

# grab OUR png and OUR copy of the frame dump: the dump path is fixed and
# shared house-wide, so another window can land in it at any moment.
grab() {
    rm -f "$RECEIPT"
    printf '206\n' >> "$RELAY"
    for _ in $(seq 1 25); do [ -f "$RECEIPT" ] && break; sleep 0.5; done
    [ -f "$RECEIPT" ] || return 1
    sleep 1
    cp /tmp/entity-menu-frame.png "$WORK/$1.png" 2>/dev/null || return 1
    cp "$FRAME" "$WORK/$1.frame.txt" 2>/dev/null || return 1
    grep -q '|r1|' "$WORK/$1.frame.txt"
}
rowh() { awk -F'|' '$2=="r1" {print $12; exit}' "$1"; }

# ---- baseline -----------------------------------------------------------
grab base || { echo "FAIL: no frame dump from the fixture window"; exit 1; }
BASE_PCT="$(zoom)"
[ -n "$BASE_PCT" ] || { echo "FAIL: mirror has no [ZOOM] line"; exit 1; }
BASE_H="$(rowh "$WORK/base.frame.txt")"
echo "  baseline: page=${BASE_PCT}%  r1 height=${BASE_H}px"
[ -n "$BASE_H" ] || { echo "FAIL: could not read r1's height"; exit 1; }

# ---- zoom in: relay 226 = Ctrl+= ---------------------------------------
printf '226\n' >> "$RELAY"; sleep 2.5
PCT1="$(zoom)"
grab in1 || { echo "FAIL: no frame dump after zoom in"; exit 1; }
H1="$(rowh "$WORK/in1.frame.txt")"
echo "  after Ctrl+=: page=${PCT1}%  r1 height=${H1}px"

if [ "$PCT1" = "$((BASE_PCT + 25))" ]; then
    echo "PASS: Ctrl+= steps the zoom up exactly 25% ($BASE_PCT -> $PCT1)"
else
    fail "Ctrl+= gave page=${PCT1}%, expected $((BASE_PCT + 25))%"
fi
# Height tracks the VISUAL LINE COUNT, so it only moves once the wrap
# actually changes. At the first step r1 still fits two lines at both 100%
# and 125%, and asserting growth here asserted a coincidence - the
# first version of this suite failed for exactly that reason while the
# feature was working. The wrap is asserted below, where it really
# happens; here we only require it not to have SHRUNK.
if [ -n "$H1" ] && [ "$H1" -ge "$BASE_H" ]; then
    echo "PASS: r1 did not shrink at the first step (${BASE_H}px -> ${H1}px; same line count)"
else
    fail "r1 height ${BASE_H} -> ${H1}: zoom made the row SHRINK"
fi

# Ink is measured on the SHORTEST row (r5, "Row five: short."). A short
# row's box is one fixed ROW_H tall whatever the font does - ROW_H is NOT
# zoomed - so its ink is purely a function of glyph size. Measuring a
# WRAPPING row instead compares two moving targets: the box grows AND the
# characters per line shrink, and the ink can fall even as the text gets
# bigger (measured: 41071 -> 39616 on r1 at 125%, purely from re-wrap).
python3 - "$HERE" "$WORK/base.png" "$WORK/base.frame.txt" \
         "$WORK/in1.png" "$WORK/in1.frame.txt" <<'PY' || fail "text did not get visibly bigger"
import importlib.util, sys
here, p0, f0, p1, f1 = sys.argv[1:6]
spec = importlib.util.spec_from_file_location('ws', here + '/widthscan.py')
m = importlib.util.module_from_spec(spec)
exec(compile(open(spec.origin).read().replace('\nmain()\n', '\n'), 'ws', 'exec'), m.__dict__)

def box_of(frame):
    for line in open(frame):
        f = line.rstrip('\n').split('|')
        if len(f) > 11 and f[1] == 'r5':
            return int(f[9]), int(f[11])
    raise AssertionError('r5 not in ' + frame)

def ink_run(png, frame):
    """Width in px of the row's ink run - i.e. how wide the text is.

    Two earlier versions of this measure were wrong in instructive ways:

    - ink COUNT barely moved (22813 -> 22672 px) at a real 25% zoom,
      because a row box is mostly not-text and the count is dominated by
      whatever else is painted in it.
    - a fixed "bright pixels are text" threshold then measured a row whose
      background is WHITE with DARK text (this fixture has no CSS, and the
      box is #ffffff), so it counted the background and reported a 786px
      run - the whole window.

    So: take the box's MODAL colour as its background and call everything
    that differs from it ink. That works whichever way round the window
    paints, which is the only version of this measurement worth keeping.
    """
    import collections
    w, h, px = m.decode(png)
    y, rh = box_of(frame)
    y0, y1 = max(0, y), min(h, y + rh)
    hist = collections.Counter()
    for yy in range(y0, y1):
        row = yy * w
        for xx in range(w):
            o = (row + xx) * 3
            hist[(px[o], px[o+1], px[o+2])] += 1
    bg = hist.most_common(1)[0][0]
    # A column that is ink in EVERY row of the box is a rule or an edge,
    # not a glyph - the row outline runs the full width and made the run
    # measure the whole window (788 px) at every scale.
    height = max(1, y1 - y0)
    first = last = None
    for xx in range(w):
        cnt = 0
        for yy in range(y0, y1):
            o = (yy * w + xx) * 3
            if (abs(px[o] - bg[0]) + abs(px[o+1] - bg[1]) + abs(px[o+2] - bg[2])) > 90:
                cnt += 1
        if 0 < cnt < height - 1:
            if first is None:
                first = xx
            last = xx
    return (last - first + 1) if first is not None else 0

a, b = ink_run(p0, f0), ink_run(p1, f1)
print('  width of the r5 text run: %d px baseline -> %d px zoomed in' % (a, b))
assert a > 0 and b > a, ('glyphs did not get bigger: text run %d px -> %d px' % (a, b))
print('PASS: the glyphs themselves got bigger (text run widened on real pixels)')
PY

# ---- zoom in twice more: geometry must keep climbing -------------------
printf '226\n' >> "$RELAY"; sleep 2.5
printf '226\n' >> "$RELAY"; sleep 2.5
PCT3="$(zoom)"
grab in3 || { echo "FAIL: no frame dump at 150%"; exit 1; }
H3="$(rowh "$WORK/in3.frame.txt")"
echo "  at ${PCT3}%: r1 height=${H3}px"
if [ "$PCT3" = "$((BASE_PCT + 75))" ] && [ -n "$H3" ] && [ "$H3" -gt "$H1" ]; then
    echo "PASS: zoom is monotonic - r1 keeps growing (${BASE_H} -> ${H1} -> ${H3}px) up to ${PCT3}%"
else
    fail "zoom not monotonic: heights ${BASE_H} -> ${H1} -> ${H3}, scale ${PCT3}% (expected $((BASE_PCT + 75))%)"
fi

# ---- reset: relay 228 = Ctrl+0 -----------------------------------------
printf '228\n' >> "$RELAY"; sleep 2.5
PCT0="$(zoom)"
grab back || { echo "FAIL: no frame dump after reset"; exit 1; }
H0="$(rowh "$WORK/back.frame.txt")"
if [ "$PCT0" = "100" ] && [ "$H0" = "$BASE_H" ]; then
    echo "PASS: Ctrl+0 resets to 100% and the layout returns exactly (r1 back to ${H0}px)"
else
    fail "reset gave page=${PCT0}% r1=${H0}px; expected 100% and ${BASE_H}px"
fi

# ---- the floor is a floor: relay 227 = Ctrl+-, clamped at 50% ----------
printf '227\n' >> "$RELAY"; sleep 2.0
printf '227\n' >> "$RELAY"; sleep 2.0
PCTF="$(zoom)"
printf '228\n' >> "$RELAY"; sleep 1.5     # leave it at 100 for the next run
if [ "$PCTF" = "50" ]; then
    echo "PASS: Ctrl+- steps down and clamps at the 50% floor (not 25)"
else
    fail "floor wrong: two Ctrl+- from 100 gave ${PCTF}%, expected the clamp at 50"
fi

# ---- and it touched nothing outside this window -------------------------
HOUSE_SCALE_AFTER="$(grep -m1 '^font_scale=' "$PDL" 2>/dev/null | cut -d= -f2)"
if [ "$HOUSE_SCALE_BEFORE" = "$HOUSE_SCALE_AFTER" ]; then
    echo "PASS: page zoom wrote no house state (hq_ui.pdl font_scale still $HOUSE_SCALE_AFTER)"
else
    fail "page zoom changed the HOUSE scale: $HOUSE_SCALE_BEFORE -> $HOUSE_SCALE_AFTER"
fi

[ "$FAIL" -eq 0 ] && echo "nb_zoom_test.sh PASS"
exit $FAIL