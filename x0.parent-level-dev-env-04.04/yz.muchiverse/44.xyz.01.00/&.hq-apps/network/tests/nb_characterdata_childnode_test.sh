#!/bin/bash
# nb_characterdata_childnode_test.sh - ChildNode methods on CharacterData
# (Text/Comment): remove/before/after/replaceWith.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_char_childnode.XXXXXX)"
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
  function check(n,g,w){ total++; if(String(g)===String(w)){pass++; console.log('CCN_TESTOK '+n);} else {console.log('CCN_TESTFAIL '+n+' got='+g+' want='+w);} }

  var d=document.createElement('div');
  var t=document.createTextNode('hi');
  var c=document.createComment('x');
  d.appendChild(t); d.appendChild(c);

  check('t-remove-fn', typeof t.remove, 'function');
  check('t-before-fn', typeof t.before, 'function');
  check('t-after-fn', typeof t.after, 'function');
  check('t-replaceWith-fn', typeof t.replaceWith, 'function');
  check('c-remove-fn', typeof c.remove, 'function');
  check('c-before-fn', typeof c.before, 'function');

  t.after(document.createTextNode('!'));
  check('after-text', d.textContent, 'hi!');

  c.before(document.createTextNode('?'));
  check('before-text', d.textContent, 'hi!?');

  c.remove();
  check('remove-len', d.childNodes.length, 3);
  check('remove-text', d.textContent, 'hi!?');

  t.replaceWith(document.createTextNode('bye'));
  check('replace-text', d.textContent, 'bye!?');
  check('replaced-parent', t.parentNode, null);

  var d2=document.createElement('div');
  var c2=document.createComment('a');
  d2.appendChild(c2);
  c2.replaceWith(document.createTextNode('z'));
  check('comment-replaced', d2.textContent, 'z');

  console.log('CCN_TESTRESULT '+pass+'/'+total);
  return 'CCN_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title ccn:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 --drain-ms 1000 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>CCN_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never completed"; sed -n '1,80p' "$WORK/console.txt"; sed -n '1,40p' "$WORK/err.log"; exit 1; }

TOTAL=$(grep -o 'CCN_TESTRESULT [0-9]*/[0-9]*' "$WORK/console.txt" | tail -1 | sed 's|CCN_TESTRESULT ||')
grep -q '^LOG|CCN_TESTRESULT 13/13' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held (saw ${TOTAL:-none})"; FAIL=1; }

if grep -q '^LOG|CCN_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: CharacterData ChildNode methods incorrect"
    grep '^LOG|CCN_TESTFAIL' "$WORK/console.txt"
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
    echo "nb_characterdata_childnode_test.sh PASS (${TOTAL:-?})"
else
    echo "nb_characterdata_childnode_test.sh FAIL"
fi
exit $FAIL
