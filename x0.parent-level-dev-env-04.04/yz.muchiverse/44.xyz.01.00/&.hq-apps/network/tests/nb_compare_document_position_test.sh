#!/bin/bash
# nb_compare_document_position_test.sh - Node.compareDocumentPosition and
# DOCUMENT_POSITION_* constants.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_cmpdocpos.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

printf '/*nbjs-script-boundary*/\n' > "$WORK/page.js"
cat > "$WORK/dom.txt" <<'DOM'
N|#document|||0||0|
D
N|html|||0||4|html
D
N|body|||0||4|body
D
N|div|a||0||3|div
U
N|div|b||0||3|div
D
N|span|c||0||4|span
U
U
U
U
U
DOM

cat > "$WORK/probe.js" <<'PROBE'
(function(){
  var pass=0,total=0;
  function check(n,g,w){ total++; if(String(g)===String(w)){pass++; console.log('CDP_TESTOK '+n);} else {console.log('CDP_TESTFAIL '+n+' got='+g+' want='+w);} }

  var D=Node.DOCUMENT_POSITION_DISCONNECTED;
  var P=Node.DOCUMENT_POSITION_PRECEDING;
  var F=Node.DOCUMENT_POSITION_FOLLOWING;
  var C=Node.DOCUMENT_POSITION_CONTAINS;
  var CB=Node.DOCUMENT_POSITION_CONTAINED_BY;
  var IS=Node.DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC;

  check('const-D', D, 1);
  check('const-P', P, 2);
  check('const-F', F, 4);
  check('const-C', C, 8);
  check('const-CB', CB, 16);
  check('const-IS', IS, 32);
  check('proto-D', Node.prototype.DOCUMENT_POSITION_DISCONNECTED, 1);

  var a=document.getElementById('a');
  var b=document.getElementById('b');
  var c=document.getElementById('c');
  var body=document.body;

  check('same', a.compareDocumentPosition(a), 0);
  check('parent-vs-child', body.compareDocumentPosition(a), CB|F);
  check('child-vs-parent', a.compareDocumentPosition(body), C|P);
  check('ancestor-vs-desc', b.compareDocumentPosition(c), CB|F);
  check('desc-vs-ancestor', c.compareDocumentPosition(b), C|P);
  check('sib-a-before-b', a.compareDocumentPosition(b), F);
  check('sib-b-after-a', b.compareDocumentPosition(a), P);
  check('a-before-desc-of-b', a.compareDocumentPosition(c), F);
  check('desc-of-b-after-a', c.compareDocumentPosition(a), P);

  var x=document.createElement('div');
  var dx=a.compareDocumentPosition(x);
  check('disc-flag', (dx & D)!==0, true);
  check('disc-spec', (dx & IS)!==0, true);
  check('disc-order', (dx & (P|F))!==0, true);

  var pd=new DOMParser().parseFromString('<div><span id="s"></span></div>','text/html');
  var s=pd.querySelector('#s');
  var cross=a.compareDocumentPosition(s);
  check('cross-doc-disc', (cross & D)!==0, true);

  console.log('CDP_TESTRESULT '+pass+'/'+total);
  return 'CDP_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title cdp:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 --drain-ms 1000 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>CDP_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never completed"; sed -n '1,80p' "$WORK/console.txt"; sed -n '1,40p' "$WORK/err.log"; exit 1; }

TOTAL=$(grep -o 'CDP_TESTRESULT [0-9]*/[0-9]*' "$WORK/console.txt" | tail -1 | sed 's|CDP_TESTRESULT ||')
grep -q '^LOG|CDP_TESTRESULT 20/20' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held (saw ${TOTAL:-none})"; FAIL=1; }

if grep -q '^LOG|CDP_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: compareDocumentPosition semantics incorrect"
    grep '^LOG|CDP_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

python3 - "$WORK/out.json" <<'PY' || FAIL=1
import json, sys
d = json.load(open(sys.argv[1]))
if d.get("errors"):
    print("FAIL: harness errors:", d["errors"]); raise SystemExit(1)
print("rows: no harness errors")
PY

if [ "$FAIL" -eq 0 ]; then
    echo "nb_compare_document_position_test.sh PASS (${TOTAL:-?})"
else
    echo "nb_compare_document_position_test.sh FAIL"
fi
exit $FAIL
