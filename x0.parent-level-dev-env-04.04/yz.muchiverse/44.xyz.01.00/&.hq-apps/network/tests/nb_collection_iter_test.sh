#!/bin/bash
# nb_collection_iter_test.sh - NodeList/HTMLCollection iteration and item/forEach compat.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_coliter.XXXXXX)"
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
U
U
U
DOM

cat > "$WORK/probe.js" <<'PROBE'
(function(){
  var pass=0,total=0;
  function check(n,g,w){ total++; if(String(g)===String(w)){pass++; console.log('CI_TESTOK '+n);} else {console.log('CI_TESTFAIL '+n+' got='+g+' want='+w);} }

  var q = document.querySelectorAll('div');
  check('nl-len', q.length, 2);
  check('nl-item-fn', typeof q.item, 'function');
  check('nl-item-0', q.item(0) && q.item(0).id, 'a');
  check('nl-item-1', q.item(1) && q.item(1).id, 'b');
  check('nl-item-oob', q.item(5), 'null');
  check('nl-forEach-fn', typeof q.forEach, 'function');
  check('nl-iter-fn', typeof q[Symbol.iterator], 'function');

  var ids=[]; q.forEach(function(n){ ids.push(n.id); });
  check('nl-forEach-order', ids.join(','), 'a,b');

  var ids2 = Array.from(q).map(function(n){ return n.id; });
  check('nl-array-from', ids2.join(','), 'a,b');

  var ids3=[]; for (var n of q) ids3.push(n.id);
  check('nl-for-of', ids3.join(','), 'a,b');

  var byTag = document.getElementsByTagName('div');
  check('hc-item-fn', typeof byTag.item, 'function');
  check('hc-item-0', byTag.item(0) && byTag.item(0).id, 'a');
  check('hc-forEach-fn', typeof byTag.forEach, 'function');
  check('hc-iter-fn', typeof byTag[Symbol.iterator], 'function');
  var hids=[]; for (var m of byTag) hids.push(m.id);
  check('hc-for-of', hids.join(','), 'a,b');

  console.log('CI_TESTRESULT '+pass+'/'+total);
  return 'CI_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title ci:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 --drain-ms 1000 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>CI_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never completed"; sed -n '1,80p' "$WORK/console.txt"; sed -n '1,40p' "$WORK/err.log"; exit 1; }

TOTAL=$(grep -o 'CI_TESTRESULT [0-9]*/[0-9]*' "$WORK/console.txt" | tail -1 | sed 's|CI_TESTRESULT ||')
grep -q '^LOG|CI_TESTRESULT 15/15' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held (saw ${TOTAL:-none})"; FAIL=1; }

if grep -q '^LOG|CI_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: collection iteration/item semantics incorrect"
    grep '^LOG|CI_TESTFAIL' "$WORK/console.txt"
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
    echo "nb_collection_iter_test.sh PASS (${TOTAL:-?})"
else
    echo "nb_collection_iter_test.sh FAIL"
fi
exit $FAIL
