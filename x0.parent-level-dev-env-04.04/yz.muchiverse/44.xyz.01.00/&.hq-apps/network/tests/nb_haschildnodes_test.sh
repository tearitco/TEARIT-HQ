#!/bin/bash
# nb_haschildnodes_test.sh - Node.hasChildNodes() reflects child presence.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_haschildnodes.XXXXXX)"
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
U
U
DOM

cat > "$WORK/probe.js" <<'PROBE'
(function(){
  var pass=0,total=0;
  function check(n,g,w){ total++; if(String(g)===String(w)){pass++; console.log('HCN_TESTOK '+n);} else {console.log('HCN_TESTFAIL '+n+' got='+g+' want='+w);} }

  var e=document.createElement('div');
  check('empty-elem', e.hasChildNodes(), false);
  e.appendChild(document.createTextNode('x'));
  check('text-child', e.hasChildNodes(), true);
  e.textContent='';
  check('cleared', e.hasChildNodes(), false);
  e.appendChild(document.createElement('span'));
  check('elem-child', e.hasChildNodes(), true);

  var live=document.getElementById('a');
  check('live-empty', live.hasChildNodes(), false);

  var f=document.createDocumentFragment();
  check('frag-empty', f.hasChildNodes(), false);
  f.appendChild(document.createElement('p'));
  check('frag-child', f.hasChildNodes(), true);

  var t=document.createTextNode('hi');
  check('textnode', t.hasChildNodes(), false);
  var c=document.createComment('z');
  check('comment', c.hasChildNodes(), false);

  var pd=new DOMParser().parseFromString('<div><span></span></div>','text/html');
  check('parsed-root', pd.hasChildNodes(), true);
  check('parsed-div', pd.querySelector('div').hasChildNodes(), true);
  check('parsed-span', pd.querySelector('span').hasChildNodes(), false);

  console.log('HCN_TESTRESULT '+pass+'/'+total);
  return 'HCN_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title hcn:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 --drain-ms 1000 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>HCN_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never completed"; sed -n '1,80p' "$WORK/console.txt"; sed -n '1,40p' "$WORK/err.log"; exit 1; }

TOTAL=$(grep -o 'HCN_TESTRESULT [0-9]*/[0-9]*' "$WORK/console.txt" | tail -1 | sed 's|HCN_TESTRESULT ||')
grep -q '^LOG|HCN_TESTRESULT 12/12' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held (saw ${TOTAL:-none})"; FAIL=1; }

if grep -q '^LOG|HCN_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: hasChildNodes semantics incorrect"
    grep '^LOG|HCN_TESTFAIL' "$WORK/console.txt"
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
    echo "nb_haschildnodes_test.sh PASS (${TOTAL:-?})"
else
    echo "nb_haschildnodes_test.sh FAIL"
fi
exit $FAIL
