#!/bin/bash
# nb_comment_serialize_test.sh - comment HTML serialization is safe for
# premature-close sequences (--> and --!>).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_comment_ser.XXXXXX)"
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
  function check(n,g,w){ total++; if(String(g)===String(w)){pass++; console.log('CS_TESTOK '+n);} else {console.log('CS_TESTFAIL '+n+' got='+g+' want='+w);} }

  var c=document.createComment('a--b-->c');
  var d=document.createElement('div'); d.appendChild(c);
  var h=d.innerHTML;
  check('has-escaped-gt', h.indexOf('--&gt;')>=0, true);
  check('no-raw-close', h.indexOf('-->c')<0, true);
  var p=new DOMParser().parseFromString('<div>'+h+'</div>','text/html');
  var dv=p.querySelector('div');
  check('reparse-kids', dv.childNodes.length, 1);
  check('reparse-type', dv.childNodes[0].nodeType, 8);
  check('reparse-text', dv.textContent, '');

  var c2=document.createComment('x--!>y');
  var d2=document.createElement('div'); d2.appendChild(c2);
  var h2=d2.innerHTML;
  check('bang-escaped', h2.indexOf('--!&gt;')>=0, true);
  var p2=new DOMParser().parseFromString('<div>'+h2+'</div>','text/html');
  check('bang-kids', p2.querySelector('div').childNodes.length, 1);
  check('bang-type', p2.querySelector('div').childNodes[0].nodeType, 8);

  var c3=document.createComment('ok');
  var d3=document.createElement('div'); d3.appendChild(c3);
  check('plain', d3.innerHTML, '<!--ok-->');

  console.log('CS_TESTRESULT '+pass+'/'+total);
  return 'CS_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title cs:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 --drain-ms 1000 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>CS_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never completed"; sed -n '1,60p' "$WORK/console.txt"; sed -n '1,40p' "$WORK/err.log"; exit 1; }

TOTAL=$(grep -o 'CS_TESTRESULT [0-9]*/[0-9]*' "$WORK/console.txt" | tail -1 | sed 's|CS_TESTRESULT ||')
grep -q '^LOG|CS_TESTRESULT 9/9' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held (saw ${TOTAL:-none})"; FAIL=1; }

if grep -q '^LOG|CS_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: comment serialization is not safe"
    grep '^LOG|CS_TESTFAIL' "$WORK/console.txt"
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
    echo "nb_comment_serialize_test.sh PASS (${TOTAL:-?})"
else
    echo "nb_comment_serialize_test.sh FAIL"
fi
exit $FAIL
