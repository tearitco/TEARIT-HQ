#!/bin/bash
# nb_range_selection_test.sh - Ranges select real content; selections hold them.
#
# Range/Selection were inert stubs (no createRange/getSelection at all).
# These checks drive the real slice through nb_probe.py: selectNodeContents
# and selectNode text over a parsed document, collapsed factory ranges,
# and a Selection that tracks the range's text and anchors (plus removal).
# Console messages use spaces because the capture rewrites '|' to a space.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_range_selection.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

printf '/*nbjs-script-boundary*/\nconsole.log("LOADOK");\n' > "$WORK/page.js"
printf 'N|#document|||0||0|\nD\nN|html|||3|\n\n\n|4|html\nD\nN|head|||0||4|head\nD\nN|title|||0||5|title\nU\nN|body|||3|\n\n\n|4|body\nD\nN|p|t||6|tick 0|8|p id="t"\nN|p|||48|Timer fixture: this paragraph must never change.|1|p\nU\nU\nU\n' > "$WORK/dom.txt"

cat > "$WORK/probe.js" <<'PROBE'
(function(){ var pass=0,total=0; function check(n,g,w){ total++; if(g===w){pass++; console.log('RS_TESTOK '+n);} else {console.log('RS_TESTFAIL '+n+' got='+g+' want='+w);} } var doc=new DOMParser().parseFromString('<html><body><p id=x>hello <b>brave</b></p></body></html>','text/html'); var p=doc.getElementById('x'); var r=doc.createRange(); r.selectNodeContents(p); check('contents-text', r.toString(), 'hello brave'); check('collapsed-false', String(r.collapsed), 'false'); var r2=doc.createRange(); r2.selectNode(p); check('node-text', r2.toString(), 'hello brave'); var sel=window.getSelection(); sel.removeAllRanges(); sel.addRange(r); check('sel-count', String(sel.rangeCount), '1'); check('sel-text', sel.toString(), 'hello brave'); check('sel-anchor', sel.anchorNode===p ? 'yes' : 'no', 'yes'); sel.removeAllRanges(); check('sel-cleared', String(sel.rangeCount), '0'); var r3=doc.createRange(); r3.collapse(true); check('factory-collapsed', String(r3.collapsed), 'true'); console.log('RS_TESTRESULT '+pass+'/'+total); return 'RS_TESTDONE'; })();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title range:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>RS_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never ran"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^LOG|RS_TESTRESULT 8/8' "$WORK/console.txt" \
    || { echo "FAIL: not every Range/Selection assertion held"; FAIL=1; }

if grep -q '^LOG|RS_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: Range/Selection is not behaviorally real"
    grep '^LOG|RS_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

if [ "$FAIL" -eq 0 ]; then
    echo "nb_range_selection_test.sh PASS (8/8)"
else
    echo "nb_range_selection_test.sh FAIL"
fi
exit $FAIL
