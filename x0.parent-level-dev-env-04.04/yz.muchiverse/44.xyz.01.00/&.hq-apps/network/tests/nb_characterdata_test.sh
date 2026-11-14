#!/bin/bash
# nb_characterdata_test.sh - CharacterData length and data mutation methods
# on Text/Comment nodes (append/insert/delete/replace/substringData).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_characterdata.XXXXXX)"
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

  var t=document.createTextNode('hello');
  check('t-length', t.length, 5);
  check('t-sub', t.substringData(1,3), 'ell');
  check('t-sub-tail', t.substringData(2, 10), 'llo');

  t.appendData('!');
  check('t-append', t.data, 'hello!');
  check('t-append-len', t.length, 6);

  t.insertData(5, ' world');
  check('t-insert', t.data, 'hello world!');

  t.deleteData(5, 6);
  check('t-delete', t.data, 'hello!');

  t.replaceData(0, 5, 'bye');
  check('t-replace', t.data, 'bye!');
  check('t-nodevalue', t.nodeValue, 'bye!');
  check('t-textcontent', t.textContent, 'bye!');

  var c=document.createComment('abc');
  check('c-length', c.length, 3);
  c.appendData('d');
  check('c-append', c.data, 'abcd');
  c.insertData(1, 'Z');
  check('c-insert', c.data, 'aZbcd');
  c.deleteData(1,1);
  check('c-delete', c.data, 'abcd');
  c.replaceData(1,2,'XY');
  check('c-replace', c.data, 'aXYd');
  check('c-nodevalue', c.nodeValue, 'aXYd');

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
    || { echo "FAIL: the probe never completed"; sed -n '1,80p' "$WORK/console.txt"; sed -n '1,40p' "$WORK/err.log"; exit 1; }

TOTAL=$(grep -o 'CD_TESTRESULT [0-9]*/[0-9]*' "$WORK/console.txt" | tail -1 | sed 's|CD_TESTRESULT ||')
grep -q '^LOG|CD_TESTRESULT 16/16' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held (saw ${TOTAL:-none})"; FAIL=1; }

if grep -q '^LOG|CD_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: CharacterData method semantics incorrect"
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
    echo "nb_characterdata_test.sh PASS (${TOTAL:-?})"
else
    echo "nb_characterdata_test.sh FAIL"
fi
exit $FAIL
