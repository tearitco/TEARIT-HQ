#!/bin/bash
# nb_nodelist_test.sh - query lists expose the NodeList/HTMLCollection surface.
#
# NodeList and HTMLCollection were inert plain stubs: new NodeList() made an
# empty object and query results were bare arrays, so list.item() was
# undefined and instanceof failed. These checks drive the real slice through
# nb_probe.py: querySelectorAll (NodeList) and getElementsByTagName /
# getElementsByClassName (HTMLCollection) expose item() and their
# prototypes. Console messages use spaces because the capture rewrites '|'
# to a space.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_nodelist.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

printf '/*nbjs-script-boundary*/\nconsole.log("LOADOK");\n' > "$WORK/page.js"
printf 'N|#document|||0||0|\nD\nN|html|||3|\n\n\n|4|html\nD\nN|head|||0||4|head\nD\nN|title|||0||5|title\nU\nN|body|||3|\n\n\n|4|body\nD\nN|p|t||6|tick 0|8|p id="t"\nN|p|||48|Timer fixture: this paragraph must never change.|1|p\nU\nU\nU\n' > "$WORK/dom.txt"

cat > "$WORK/probe.js" <<'PROBE'
(function(){ var pass=0,total=0; function check(n,g,w){ total++; if(g===w){pass++; console.log('NL_TESTOK '+n);} else {console.log('NL_TESTFAIL '+n+' got='+g+' want='+w);} } var list=document.querySelectorAll('#t'); check('nodelist-method', typeof list.item, 'function'); check('nodelist-item', list.item(0)?list.item(0).tagName:'none', 'p'); check('nodelist-instanceof', String(list instanceof NodeList), 'true'); var cols=document.getElementsByTagName('p'); check('collection-method', typeof cols.item, 'function'); check('collection-instanceof', String(cols instanceof HTMLCollection), 'true'); check('collection-length', String(cols.length), '2'); var cls=document.getElementsByClassName('missing'); check('collection-empty', String(cls.length)+':'+(cls.item(0)===null?'null':'x'), '0:null'); console.log('NL_TESTRESULT '+pass+'/'+total); return 'NL_TESTDONE'; })();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title nodelist:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>NL_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never ran"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^LOG|NL_TESTRESULT 7/7' "$WORK/console.txt" \
    || { echo "FAIL: not every list assertion held"; FAIL=1; }

if grep -q '^LOG|NL_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: NodeList surface is not behaviorally real"
    grep '^LOG|NL_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

if [ "$FAIL" -eq 0 ]; then
    echo "nb_nodelist_test.sh PASS (7/7)"
else
    echo "nb_nodelist_test.sh FAIL"
fi
exit $FAIL
