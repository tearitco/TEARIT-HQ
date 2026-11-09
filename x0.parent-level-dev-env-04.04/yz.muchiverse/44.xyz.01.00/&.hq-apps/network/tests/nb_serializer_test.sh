#!/bin/bash
# nb_serializer_test.sh - XMLSerializer + outerHTML deliver real markup.
#
# The historical XMLSerializer was an inert plain stub: the method did not
# exist, so every call threw "not a function". These checks drive the real
# slice through nb_probe.py: serializeToString on a live element, outerHTML,
# a JS-created element with setAttribute + textContent, null rejection, and
# a parsed document. Console messages use spaces because the capture
# rewrites '|' to a space.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_serializer.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

printf '/*nbjs-script-boundary*/\nconsole.log("LOADOK");\n' > "$WORK/page.js"
printf 'N|#document|||0||0|\nD\nN|html|||3|\n\n\n|4|html\nD\nN|head|||0||4|head\nD\nN|title|||0||5|title\nU\nN|body|||3|\n\n\n|4|body\nD\nN|p|t||6|tick 0|8|p id="t"\nN|p|||48|Timer fixture: this paragraph must never change.|1|p\nU\nU\nU\n' > "$WORK/dom.txt"

cat > "$WORK/probe.js" <<'PROBE'
(function(){ var pass=0,total=0; function check(n,g,w){ total++; if(g===w){pass++; console.log('SR_TESTOK '+n);} else {console.log('SR_TESTFAIL '+n+' got='+g+' want='+w);} } var el=document.querySelector('#t'); var s=new XMLSerializer(); check('method', typeof s.serializeToString, 'function'); check('element', s.serializeToString(el), '<p id="t">tick 0</p>'); check('outerHTML', el.outerHTML, '<p id="t">tick 0</p>'); var d=document.createElement('div'); d.setAttribute('id','z'); d.textContent='hi'; check('created', s.serializeToString(d), '<div id="z">hi</div>'); var threw='no'; try{s.serializeToString(null);}catch(e){threw='yes';} check('null-throws', threw, 'yes'); var doc=new DOMParser().parseFromString('<div><p id="q">Q</p></div>','text/html'); check('parsedDoc', s.serializeToString(doc), '<div><p id="q">Q</p></div>'); console.log('SR_TESTRESULT '+pass+'/'+total); return 'SR_TESTDONE'; })();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title serializer:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>SR_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never ran"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^LOG|SR_TESTRESULT 6/6' "$WORK/console.txt" \
    || { echo "FAIL: not every serializer assertion held"; FAIL=1; }

if grep -q '^LOG|SR_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: XMLSerializer is not behaviorally real"
    grep '^LOG|SR_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

if [ "$FAIL" -eq 0 ]; then
    echo "nb_serializer_test.sh PASS (6/6)"
else
    echo "nb_serializer_test.sh FAIL"
fi
exit $FAIL
