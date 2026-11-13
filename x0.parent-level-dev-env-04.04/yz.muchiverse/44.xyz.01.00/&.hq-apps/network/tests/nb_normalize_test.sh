#!/bin/bash
# nb_normalize_test.sh - Node.normalize() merges adjacent text nodes and
# removes empty text nodes recursively.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_normalize.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

printf '/*nbjs-script-boundary*/\n' > "$WORK/page.js"
cat > "$WORK/dom.txt" <<'DOM'
N|#document|||0||0|
D
N|html|||0||4|html
D
N|body|||0||4|body
U
U
DOM

cat > "$WORK/probe.js" <<'PROBE'
(function(){
  var pass=0,total=0;
  function check(n,g,w){ total++; if(String(g)===String(w)){pass++; console.log('NM_TESTOK '+n);} else {console.log('NM_TESTFAIL '+n+' got='+g+' want='+w);} }

  var d=document.createElement('div');
  var t1=document.createTextNode('a');
  var t2=document.createTextNode('b');
  var t3=document.createTextNode('');
  d.appendChild(t1); d.appendChild(t2); d.appendChild(t3);
  d.normalize();
  check('merge-len', d.childNodes.length, 1);
  check('merge-text', d.textContent, 'ab');
  check('merge-same', d.firstChild===t1, true);
  check('merge-val', d.firstChild.nodeValue, 'ab');

  var d2=document.createElement('div');
  var e=document.createElement('span');
  e.appendChild(document.createTextNode('x'));
  e.appendChild(document.createTextNode('y'));
  d2.appendChild(document.createTextNode('1'));
  d2.appendChild(document.createTextNode(''));
  d2.appendChild(e);
  d2.appendChild(document.createTextNode('2'));
  d2.appendChild(document.createTextNode('3'));
  d2.normalize();
  check('rec-len', d2.childNodes.length, 3);
  check('rec-text', d2.textContent, '1xy23');
  check('child-len', e.childNodes.length, 1);
  check('child-text', e.textContent, 'xy');

  var d3=document.createElement('div');
  d3.appendChild(document.createTextNode(''));
  d3.normalize();
  check('only-empty', d3.childNodes.length, 0);
  check('only-empty-text', d3.textContent, '');

  var d4=document.createElement('div');
  d4.appendChild(document.createComment('c'));
  d4.appendChild(document.createTextNode('a'));
  d4.appendChild(document.createTextNode('b'));
  d4.normalize();
  check('comment-boundary-len', d4.childNodes.length, 2);
  check('comment-boundary-type', d4.firstChild.nodeType, 8);
  check('comment-boundary-text', d4.lastChild.nodeValue, 'ab');

  console.log('NM_TESTRESULT '+pass+'/'+total);
  return 'NM_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title nm:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 --drain-ms 1000 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>NM_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never completed"; sed -n '1,80p' "$WORK/console.txt"; sed -n '1,40p' "$WORK/err.log"; exit 1; }

TOTAL=$(grep -o 'NM_TESTRESULT [0-9]*/[0-9]*' "$WORK/console.txt" | tail -1 | sed 's|NM_TESTRESULT ||')
grep -q '^LOG|NM_TESTRESULT 13/13' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held (saw ${TOTAL:-none})"; FAIL=1; }

if grep -q '^LOG|NM_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: normalize semantics incorrect"
    grep '^LOG|NM_TESTFAIL' "$WORK/console.txt"
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
    echo "nb_normalize_test.sh PASS (${TOTAL:-?})"
else
    echo "nb_normalize_test.sh FAIL"
fi
exit $FAIL
