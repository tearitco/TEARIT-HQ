#!/bin/bash
# nb_media_test.sh - G2.5: a viewport-width-dependent rule switches at the
# breakpoint, proven on BOTH sides. Hermetic: no window, no renderer, no net.
#
# WHY THIS EXISTS (2026-11-04)
# ---------------------------
# nb_css.c skipped every @media block wholesale, on the stated ground that
# "the engine has no viewport, so a media query can never fire" (nb_css.h).
# The worker DOES have one: LOAD carries the pane's content width and
# window.innerWidth is exported from that same number. So a page whose layout
# switches at a breakpoint could never switch here.
#
# TWO CHANNELS per width, because the property alone is a self-report:
#   - the computed display (what the cascade decided)
#   - the element's own rect (the LAYOUT consequence: display:none must
#     collapse it to 0x0, which is the thing a page actually observes)
# The four widths straddle the boundary (767/768), so an off-by-one in the
# comparison fails here rather than in the field.
#
# COMMA-LIST NOTE - the fixture writes TWO base rules, not one
# `.narrow, .wide {}` list. That is not style. This engine computes a
# selector list's specificity as the SUM over the list, so `.narrow, .wide`
# scores two classes and outranks the single-class rule inside the @media
# block, which then never applies. Measured 2026-11-04, same stylesheet
# either way:
#     comma-list base      vp=700  wide.display='block'   <- override defeated
#     separate base rules  vp=700  wide.display='none'    <- correct
#     separate base rules  vp=900  wide.display='block'   <- correct
# CSS takes the MAX over a selector list, not the sum. That deviation is
# PRE-EXISTING and untouched here (fixing it moves the cascade for every page
# and deserves its own bite with its own evidence); this test is about the
# breakpoint switch, so it avoids the unrelated trap and says so out loud.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0
fail() { echo "FAIL: $*"; FAIL=1; }

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_media.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM
printf 'N|#document|||0||0|\nD\nN|html|||0||0|\nU\n' > "$WORK/dom.txt"
cat > "$WORK/page.js" <<'PAGE'
/*nbjs-script-boundary*/
var n=document.createElement('p');n.setAttribute('id','narrow');n.setAttribute('class','narrow');n.textContent='NARROW BRANCH';document.documentElement.appendChild(n);var w=document.createElement('p');w.setAttribute('id','wide');w.setAttribute('class','wide');w.textContent='WIDE BRANCH';document.documentElement.appendChild(w);

PAGE
cat > "$WORK/style.css" <<'CSS'
.narrow { display: block; }
.wide { display: block; }
@media (max-width: 767px) { .narrow { display: block; } .wide { display: none; } }
@media (min-width: 768px) { .narrow { display: none; } .wide { display: block; } }

CSS
cat > "$WORK/probe.js" <<'PROBE'
(function(){function info(id){var e=document.getElementById(id);if(!e)return id+':disp=missing,w=-1,h=-1';var d='?';try{var s=getComputedStyle(e);d=s?String(s.display):'nostyle';}catch(err){d='THROW';}var w=-1,h=-1;try{var r=e.getBoundingClientRect();w=Math.round(r.width);h=Math.round(r.height);}catch(err){}return id+':disp='+d+',w='+w+',h='+h;}return 'MEDIA width='+innerWidth+' '+info('narrow')+' '+info('wide');})();
PROBE

# at <width> <visible-branch> <hidden-branch>
at() {
    _w="$1"; _vis="$2"; _hid="$3"
    python3 "$HERE/nb_probe.py" \
        --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
        --css "$WORK/style.css" --width "$_w" \
        --href https://example.invalid/ --title media:fixture \
        --probe "$WORK/probe.js" --console "$WORK/console.$_w.txt" \
        --load-timeout 30 --eval-timeout 30 > "$WORK/out.$_w.json" 2> "$WORK/err.$_w.log"
    line="$(sed -n 's/^=>MEDIA //p' "$WORK/console.$_w.txt" | head -1)"
    if [ -z "$line" ]; then
        fail "width $_w: no result"; sed -n '1,20p' "$WORK/err.$_w.log"; return
    fi
    g() { printf '%s' "$line" | sed -n "s/.*$1:disp=\([^,]*\),w=\([^,]*\),h=\([^,]*\).*/\2|\1/p"; }
    gw="$(printf '%s' "$line" | sed -n 's/^width=\([0-9]*\).*/\1/p')"
    hid_w="$(g "$_hid" | cut -d'|' -f1)"; hid_d="$(g "$_hid" | cut -d'|' -f2)"
    vis_w="$(g "$_vis" | cut -d'|' -f1)"; vis_d="$(g "$_vis" | cut -d'|' -f2)"
    ok=1
    [ "$gw" = "$_w" ] || { fail "width $_w: worker laid out at innerWidth=$gw"; ok=0; }
    [ "$hid_d" = "none" ] || { fail "width $_w: $_hid display should be none, got [$hid_d]"; ok=0; }
    [ "$hid_w" = "0" ] || { fail "width $_w: $_hid should collapse to w=0, got [$hid_w]"; ok=0; }
    [ -n "$vis_d" ] && [ "$vis_d" != "none" ] || { fail "width $_w: $_vis should be visible, got [$vis_d]"; ok=0; }
    [ -n "$vis_w" ] && [ "$vis_w" -gt 0 ] 2>/dev/null || { fail "width $_w: $_vis should have width > 0, got [$vis_w]"; ok=0; }
    [ "$ok" = 1 ] && echo "PASS: width $_w -> $_vis visible (w=$vis_w), $_hid display:none (w=0)"
}

at 700  narrow wide
at 767  narrow wide
at 768  wide   narrow
at 900  wide   narrow

if [ "$FAIL" -eq 0 ]; then
    echo "nb_media_test.sh PASS (G2.5: @media switches at the breakpoint, both sides)"
else
    echo "nb_media_test.sh FAIL"
fi
exit $FAIL
