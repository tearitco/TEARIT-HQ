#!/bin/bash
# nb_text_whole_split_test.sh - Text.wholeText and Text.splitText semantics.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_textsplit.XXXXXX)"
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
  function check(n,g,w){ total++; if(String(g)===String(w)){pass++; console.log('TS_TESTOK '+n);} else {console.log('TS_TESTFAIL '+n+' got='+g+' want='+w);} }

  var d=document.getElementById('a');
  var t=document.createTextNode('helloworld');
  d.appendChild(t);
  check('whole-alone', t.wholeText, 'helloworld');

  var r=t.splitText(5);
  check('split-left', t.data, 'hello');
  check('split-right', r.data, 'world');
  check('split-return-nodeType', r.nodeType, 3);
  check('split-sibling', t.nextSibling===r, true);
  check('split-parent', r.parentNode===d, true);
  check('split-prev', r.previousSibling===t, true);
  check('whole-after-split', t.wholeText, 'helloworld');
  check('whole-from-right', r.wholeText, 'helloworld');

  var t2=document.createTextNode('!');
  d.insertBefore(t2, r);
  check('whole-contiguous-three', t.wholeText, 'hello!world');
  check('whole-mid', t2.wholeText, 'hello!world');
  check('child-count', d.childNodes.length, 3);

  var dt=document.createTextNode('abc');
  var dr=dt.splitText(1);
  check('det-left', dt.data, 'a');
  check('det-right', dr.data, 'bc');
  check('det-parent-null', dr.parentNode===null, true);
  check('det-whole', dt.wholeText, 'a');

  var e0=document.createTextNode('x');
  var r0=e0.splitText(0);
  check('split-zero-left', e0.data, '');
  check('split-zero-right', r0.data, 'x');

  var elen=document.createTextNode('xy');
  var ren=elen.splitText(2);
  check('split-end-left', elen.data, 'xy');
  check('split-end-right', ren.data, '');

  var bad=false, bname='';
  try { document.createTextNode('z').splitText(5); } catch(e){ bad=true; bname=e && e.name; }
  check('oob-throws', bad, true);
  check('oob-name', bname, 'IndexSizeError');
  var bad2=false;
  try { document.createTextNode('z').splitText(-1); } catch(e){ bad2=(e&&e.name==='IndexSizeError'); }
  check('neg-throws', bad2, true);

  console.log('TS_TESTRESULT '+pass+'/'+total);
  return 'TS_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title ts:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 --drain-ms 1000 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>TS_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never completed"; sed -n '1,80p' "$WORK/console.txt"; sed -n '1,40p' "$WORK/err.log"; exit 1; }

TOTAL=$(grep -o 'TS_TESTRESULT [0-9]*/[0-9]*' "$WORK/console.txt" | tail -1 | sed 's|TS_TESTRESULT ||')
grep -q '^LOG|TS_TESTRESULT 23/23' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held (saw ${TOTAL:-none})"; FAIL=1; }

if grep -q '^LOG|TS_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: Text.wholeText/splitText semantics incorrect"
    grep '^LOG|TS_TESTFAIL' "$WORK/console.txt"
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
    echo "nb_text_whole_split_test.sh PASS (${TOTAL:-?})"
else
    echo "nb_text_whole_split_test.sh FAIL"
fi
exit $FAIL
