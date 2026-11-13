#!/bin/bash
# nb_attr_api_test.sh - Element attribute API helpers:
# getAttributeNames, hasAttributes, toggleAttribute.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_attr_api.XXXXXX)"
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
  function check(n,g,w){ total++; if(String(g)===String(w)){pass++; console.log('AA_TESTOK '+n);} else {console.log('AA_TESTFAIL '+n+' got='+g+' want='+w);} }
  function j(v){ return JSON.stringify(v); }

  var d=document.createElement('div');
  check('has0', d.hasAttributes(), false);
  check('names0', j(d.getAttributeNames()), '[]');

  d.setAttribute('ID','x');
  d.setAttribute('data-b','2');
  d.setAttribute('class','c');
  check('has1', d.hasAttributes(), true);
  check('names1', j(d.getAttributeNames()), '["id","data-b","class"]');
  check('get-id', d.getAttribute('id'), 'x');

  check('tog-add', d.toggleAttribute('hidden'), true);
  check('tog-add-has', d.hasAttribute('hidden'), true);
  check('tog-add-get', d.getAttribute('hidden'), '');
  check('tog-remove', d.toggleAttribute('hidden'), false);
  check('tog-remove-has', d.hasAttribute('hidden'), false);

  check('force-true-new', d.toggleAttribute('data-x', true), true);
  check('force-true-exists', d.toggleAttribute('id', true), true);
  check('force-false-exists', d.toggleAttribute('id', false), false);
  check('force-false-gone', d.hasAttribute('id'), false);
  check('force-false-absent', d.toggleAttribute('nope', false), false);
  check('force-false-absent-still', d.hasAttribute('nope'), false);

  var names = d.getAttributeNames();
  check('is-array', Array.isArray(names), true);
  check('has-data-b', names.indexOf('data-b')>=0, true);
  check('no-id', names.indexOf('id')<0, true);

  var doc=new DOMParser().parseFromString('<html><body><span id="s" disabled data-z="z"></span></body></html>','text/html');
  var s=doc.querySelector('#s');
  check('parsed-has', s.hasAttributes(), true);
  var pn = s.getAttributeNames();
  check('parsed-id', pn.indexOf('id')>=0, true);
  check('parsed-disabled', pn.indexOf('disabled')>=0, true);
  check('parsed-data', pn.indexOf('data-z')>=0, true);
  check('parsed-no-tag', pn.indexOf('span')<0, true);

  console.log('AA_TESTRESULT '+pass+'/'+total);
  return 'AA_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title aa:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 --drain-ms 1000 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>AA_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never completed"; sed -n '1,60p' "$WORK/console.txt"; sed -n '1,40p' "$WORK/err.log"; exit 1; }

TOTAL=$(grep -o 'AA_TESTRESULT [0-9]*/[0-9]*' "$WORK/console.txt" | tail -1 | sed 's|AA_TESTRESULT ||')
grep -q '^LOG|AA_TESTRESULT 24/24' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held (saw ${TOTAL:-none})"; FAIL=1; }

if grep -q '^LOG|AA_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: attribute API helpers are incorrect"
    grep '^LOG|AA_TESTFAIL' "$WORK/console.txt"
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
    echo "nb_attr_api_test.sh PASS (${TOTAL:-?})"
else
    echo "nb_attr_api_test.sh FAIL"
fi
exit $FAIL
