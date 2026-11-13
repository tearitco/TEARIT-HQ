#!/bin/bash
# nb_issamenode_test.sh - Node.isSameNode identity semantics.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_issamenode.XXXXXX)"
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
  function check(n,g,w){ total++; if(String(g)===String(w)){pass++; console.log('SN_TESTOK '+n);} else {console.log('SN_TESTFAIL '+n+' got='+g+' want='+w);} }

  var d=document.getElementById('a');
  var d2=document.getElementById('a');
  var e=document.createElement('div');
  check('same', d.isSameNode(d), true);
  check('same-id', d.isSameNode(d2), true);
  check('diff-elem', d.isSameNode(e), false);
  check('null', d.isSameNode(null), false);
  check('undef', d.isSameNode(undefined), false);
  check('plain-obj', d.isSameNode({}), false);
  check('no-arg', d.isSameNode(), false);

  var t=document.createTextNode('x');
  check('text-same', t.isSameNode(t), true);
  check('text-clone', t.isSameNode(t.cloneNode()), false);

  var c=document.createComment('x');
  check('comment-same', c.isSameNode(c), true);
  check('comment-clone', c.isSameNode(c.cloneNode()), false);

  var f=document.createDocumentFragment();
  f.appendChild(document.createElement('p'));
  check('frag-same', f.isSameNode(f), true);
  check('frag-clone-shallow', f.isSameNode(f.cloneNode()), false);
  check('frag-clone-deep', f.isSameNode(f.cloneNode(true)), false);

  var pd=new DOMParser().parseFromString('<html><body></body></html>','text/html');
  check('parsed-doc-same', pd.isSameNode(pd), true);

  console.log('SN_TESTRESULT '+pass+'/'+total);
  return 'SN_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title sn:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 --drain-ms 1000 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>SN_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never completed"; sed -n '1,80p' "$WORK/console.txt"; sed -n '1,40p' "$WORK/err.log"; exit 1; }

TOTAL=$(grep -o 'SN_TESTRESULT [0-9]*/[0-9]*' "$WORK/console.txt" | tail -1 | sed 's|SN_TESTRESULT ||')
grep -q '^LOG|SN_TESTRESULT 15/15' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held (saw ${TOTAL:-none})"; FAIL=1; }

if grep -q '^LOG|SN_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: isSameNode semantics incorrect"
    grep '^LOG|SN_TESTFAIL' "$WORK/console.txt"
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
    echo "nb_issamenode_test.sh PASS (${TOTAL:-?})"
else
    echo "nb_issamenode_test.sh FAIL"
fi
exit $FAIL
