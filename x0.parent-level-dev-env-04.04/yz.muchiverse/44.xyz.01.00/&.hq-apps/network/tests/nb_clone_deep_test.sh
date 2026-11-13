#!/bin/bash
# nb_clone_deep_test.sh - cloneNode deep/default semantics (elements, text,
# comments, DocumentFragment). DOM spec: deep defaults to false.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_clone_deep.XXXXXX)"
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
  function check(n,g,w){ total++; if(String(g)===String(w)){pass++; console.log('CD_TESTOK '+n);} else {console.log('CD_TESTFAIL '+n+' got='+g+' want='+w);} }

  var d=document.createElement('div');
  d.innerHTML='<span id="s">x</span><!--c--><b>y</b>';
  check('el-def-kids', d.cloneNode().childNodes.length, 0);
  check('el-false-kids', d.cloneNode(false).childNodes.length, 0);
  check('el-true-kids', d.cloneNode(true).childNodes.length, 3);
  check('el-true-span', d.cloneNode(true).querySelector('#s')!==null, true);
  check('el-tag', d.cloneNode().tagName.toLowerCase(), 'div');
  check('el-distinct', d.cloneNode(true)!==d, true);

  var t=document.createTextNode('hi');
  check('text-def', t.cloneNode().textContent, 'hi');
  check('text-false', t.cloneNode(false).nodeType, 3);

  var cm=document.createComment('note');
  var cc=cm.cloneNode();
  check('comment-type', cc.nodeType, 8);
  check('comment-val', cc.nodeValue, 'note');
  check('comment-false', cm.cloneNode(false).data, 'note');

  var f=document.createDocumentFragment();
  f.appendChild(document.createElement('p'));
  f.appendChild(document.createComment('z'));
  f.appendChild(document.createTextNode('t'));
  check('frag-def', f.cloneNode().childNodes.length, 0);
  check('frag-false', f.cloneNode(false).childNodes.length, 0);
  check('frag-true', f.cloneNode(true).childNodes.length, 3);
  check('frag-true-types', (function(){
    var c=f.cloneNode(true).childNodes; return c[0].nodeType+','+c[1].nodeType+','+c[2].nodeType;
  })(), '1,8,3');
  check('frag-deep-copy', f.cloneNode(true).firstChild!==f.firstChild, true);

  var nested=document.createDocumentFragment();
  var n1=document.createElement('div'); n1.appendChild(document.createElement('i'));
  nested.appendChild(n1);
  check('frag-nested-shallow', nested.cloneNode().childNodes.length, 0);
  check('frag-nested-deep', nested.cloneNode(true).firstChild.firstChild.tagName.toLowerCase(), 'i');

  console.log('CD_TESTRESULT '+pass+'/'+total);
  return 'CD_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title cd:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 --drain-ms 1000 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>CD_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never completed"; sed -n '1,60p' "$WORK/console.txt"; sed -n '1,40p' "$WORK/err.log"; exit 1; }

TOTAL=$(grep -o 'CD_TESTRESULT [0-9]*/[0-9]*' "$WORK/console.txt" | tail -1 | sed 's|CD_TESTRESULT ||')
grep -q '^LOG|CD_TESTRESULT 18/18' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held (saw ${TOTAL:-none})"; FAIL=1; }

if grep -q '^LOG|CD_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: cloneNode deep/default semantics are incorrect"
    grep '^LOG|CD_TESTFAIL' "$WORK/console.txt"
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
    echo "nb_clone_deep_test.sh PASS (${TOTAL:-?})"
else
    echo "nb_clone_deep_test.sh FAIL"
fi
exit $FAIL
