#!/bin/bash
# nb_document_collections_test.sh - live document collections and localName.
#
# document.scripts, .images, .links, .forms, and element.localName were all
# absent (reads threw). These checks drive the real slice through
# nb_probe.py over a parsed document: each collection sees its elements,
# and localName matches the element tag. Console messages use spaces
# because the capture rewrites '|' to a space.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_document_collections.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

printf '/*nbjs-script-boundary*/\nconsole.log("LOADOK");\n' > "$WORK/page.js"
printf 'N|#document|||0||0|\nD\nN|html|||3|\n\n\n|4|html\nD\nN|head|||0||4|head\nD\nN|title|||0||5|title\nU\nN|body|||3|\n\n\n|4|body\nD\nN|p|t||6|tick 0|8|p id="t"\nN|p|||48|Timer fixture: this paragraph must never change.|1|p\nU\nU\nU\n' > "$WORK/dom.txt"

cat > "$WORK/probe.js" <<'PROBE'
(function(){ var pass=0,total=0; function check(n,g,w){ total++; if(g===w){pass++; console.log('DOC_TESTOK '+n);} else {console.log('DOC_TESTFAIL '+n+' got='+g+' want='+w);} } var doc=new DOMParser().parseFromString('<html><head></head><body><script src="x.js"></script><img src="a.png"><form><input></form><p>hi</p></body></html>','text/html'); check('scripts', String(doc.scripts.length)+':'+(doc.scripts.item(0)?doc.scripts.item(0).tagName:'none'), '1:script'); check('images', String(doc.images.length), '1'); check('forms', String(doc.forms.length), '1'); check('links', String(doc.links.length), '0'); check('localName', doc.getElementsByTagName('p').item(0)?doc.getElementsByTagName('p').item(0).localName:'none', 'p'); console.log('DOC_TESTRESULT '+pass+'/'+total); return 'DOC_TESTDONE'; })();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title doc:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>DOC_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never ran"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^LOG|DOC_TESTRESULT 5/5' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held"; FAIL=1; }

if grep -q '^LOG|DOC_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: document collections are not behaviorally real"
    grep '^LOG|DOC_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

if [ "$FAIL" -eq 0 ]; then
    echo "nb_document_collections_test.sh PASS (5/5)"
else
    echo "nb_document_collections_test.sh FAIL"
fi
exit $FAIL
