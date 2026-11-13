#!/bin/bash
# nb_xml_comment_serialize_test.sh - XMLSerializer comment data must be
# well-formed XML (no "--" and must not end with "-").
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_xml_comment_ser.XXXXXX)"
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
  function check(n,g,w){ total++; if(String(g)===String(w)){pass++; console.log('XCS_TESTOK '+n);} else {console.log('XCS_TESTFAIL '+n+' got='+g+' want='+w);} }

  var d=document.createElement('div'); d.appendChild(document.createComment('a--b'));
  var x=new XMLSerializer().serializeToString(d);
  check('no-double', x.indexOf('--b')<0 && x.indexOf('a--')<0, true);
  check('has-space-break', x.indexOf('a- -b')>=0, true);
  check('xml-shape', x, '<div><!--a- -b--></div>');

  var d2=document.createElement('div'); d2.appendChild(document.createComment('end-'));
  var x2=new XMLSerializer().serializeToString(d2);
  check('end-not-dash', x2.indexOf('end--->')<0, true);
  check('end-space', x2.indexOf('end- -->')>=0 || x2.indexOf('end-  -->')>=0 || x2.indexOf('end- -')>=0 || x2===' <div><!--end- --></div>'.trim() ? true : x2.indexOf('end- --')>=0, true);

  var d3=document.createElement('div'); d3.appendChild(document.createComment('a--b-->c'));
  var x3=new XMLSerializer().serializeToString(d3);
  check('close-no-raw', x3.indexOf('-->c')<0, true);
  check('close-no-double', x3.indexOf('--b')<0 && x3.indexOf('b--&')<0, true);

  var d4=document.createElement('div'); d4.appendChild(document.createComment('ok'));
  check('plain-xml', new XMLSerializer().serializeToString(d4), '<div><!--ok--></div>');

  /* HTML path keeps readable "--" (not a premature close) */
  var d5=document.createElement('div'); d5.appendChild(document.createComment('a--b'));
  check('html-keeps', d5.innerHTML, '<!--a--b-->');

  console.log('XCS_TESTRESULT '+pass+'/'+total);
  return 'XCS_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title xcs:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 --drain-ms 1000 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>XCS_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never completed"; sed -n '1,80p' "$WORK/console.txt"; sed -n '1,40p' "$WORK/err.log"; exit 1; }

TOTAL=$(grep -o 'XCS_TESTRESULT [0-9]*/[0-9]*' "$WORK/console.txt" | tail -1 | sed 's|XCS_TESTRESULT ||')
grep -q '^LOG|XCS_TESTRESULT 9/9' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held (saw ${TOTAL:-none})"; FAIL=1; }

if grep -q '^LOG|XCS_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: XML comment serialization is not well-formed"
    grep '^LOG|XCS_TESTFAIL' "$WORK/console.txt"
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
    echo "nb_xml_comment_serialize_test.sh PASS (${TOTAL:-?})"
else
    echo "nb_xml_comment_serialize_test.sh FAIL"
fi
exit $FAIL
