#!/bin/bash
# nb_collection_records_test.sh - record/token/attribute surfaces carry real types.
#
# MutationRecord, DOMTokenList, and NamedNodeMap were inert plain stubs:
# classList item() did not exist, attributes were bare arrays, mutation
# records were plain objects (instanceof false). These checks drive the real
# slice through nb_probe.py: classList.item + DOMTokenList prototype,
# attributes.item + NamedNodeMap prototype, and attribute records carrying
# MutationRecord identity. Console messages use spaces because the capture
# rewrites '|' to a space.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_collection_records.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

printf '/*nbjs-script-boundary*/\nconsole.log("LOADOK");\n' > "$WORK/page.js"
printf 'N|#document|||0||0|\nD\nN|html|||3|\n\n\n|4|html\nD\nN|head|||0||4|head\nD\nN|title|||0||5|title\nU\nN|body|||3|\n\n\n|4|body\nD\nN|p|t||6|tick 0|8|p id="t"\nN|p|||48|Timer fixture: this paragraph must never change.|1|p\nU\nU\nU\n' > "$WORK/dom.txt"

cat > "$WORK/probe.js" <<'PROBE'
(function(){ var pass=0,total=0; function check(n,g,w){ total++; if(g===w){pass++; console.log('LR_TESTOK '+n);} else {console.log('LR_TESTFAIL '+n+' got='+g+' want='+w);} } var doc=new DOMParser().parseFromString('<html><body><p id=x class="a b">hi</p></body></html>','text/html'); var p=doc.getElementById('x'); var cl=p.classList; check('tokenlist-item', String(cl.item(1)), 'b'); check('tokenlist-instanceof', String(cl instanceof DOMTokenList), 'true'); var attrs=p.attributes; check('namednode-item', attrs.item(0)?attrs.item(0).name:'none', 'id'); check('namednode-instanceof', String(attrs instanceof NamedNodeMap), 'true'); var calls=0; var rec=[]; var obs=new MutationObserver(function(records){ calls++; rec=records; }); obs.observe(doc.body,{attributes:true,subtree:true}); p.setAttribute('data-k','v'); queueMicrotask(function(){ check('record-instanceof', String(rec[0] instanceof MutationRecord), 'true'); check('record-type', rec[0]?rec[0].type:'none', 'attributes'); check('record-attr', rec[0]?rec[0].attributeName:'none', 'data-k'); console.log('LR_TESTRESULT '+pass+'/'+total); }); return 'LR_TESTDONE'; })();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title lr:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>LR_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never ran"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^LOG|LR_TESTRESULT 7/7' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held"; FAIL=1; }

if grep -q '^LOG|LR_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: collection/record surfaces are not behaviorally real"
    grep '^LOG|LR_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

if [ "$FAIL" -eq 0 ]; then
    echo "nb_collection_records_test.sh PASS (7/7)"
else
    echo "nb_collection_records_test.sh FAIL"
fi
exit $FAIL
