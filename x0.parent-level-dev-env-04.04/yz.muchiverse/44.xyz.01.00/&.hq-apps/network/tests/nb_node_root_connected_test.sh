#!/bin/bash
# nb_node_root_connected_test.sh - Node.isConnected and Node.getRootNode().
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_node_root_connected.XXXXXX)"
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
  function check(n,g,w){ total++; if(String(g)===String(w)){pass++; console.log('NRC_TESTOK '+n);} else {console.log('NRC_TESTFAIL '+n+' got='+g+' want='+w);} }

  var live=document.getElementById('a');
  check('live-connected', live.isConnected, true);
  check('live-root-doc', live.getRootNode()===document, true);
  check('doc-root-doc', document.getRootNode()===document, true);
  check('doc-connected', document.isConnected, true);

  var det=document.createElement('div');
  check('det-connected', det.isConnected, false);
  check('det-root-self', det.getRootNode()===det, true);
  document.body.appendChild(det);
  check('app-connected', det.isConnected, true);
  check('app-root-doc', det.getRootNode()===document, true);
  document.body.removeChild(det);
  check('rem-connected', det.isConnected, false);

  var f=document.createDocumentFragment();
  var fc=document.createElement('p');
  f.appendChild(fc);
  check('frag-connected', f.isConnected, false);
  check('frag-child-connected', fc.isConnected, false);
  check('frag-child-root', fc.getRootNode()===f, true);
  check('frag-root-self', f.getRootNode()===f, true);

  var pd=new DOMParser().parseFromString('<html><body><div id="x"></div></body></html>','text/html');
  var px=pd.querySelector('#x');
  check('parsed-doc-connected', pd.isConnected, true);
  check('parsed-child-connected', px.isConnected, true);
  check('parsed-child-root', px.getRootNode()===pd, true);

  console.log('NRC_TESTRESULT '+pass+'/'+total);
  return 'NRC_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title nrc:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 --drain-ms 1000 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>NRC_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never completed"; sed -n '1,80p' "$WORK/console.txt"; sed -n '1,40p' "$WORK/err.log"; exit 1; }

TOTAL=$(grep -o 'NRC_TESTRESULT [0-9]*/[0-9]*' "$WORK/console.txt" | tail -1 | sed 's|NRC_TESTRESULT ||')
grep -q '^LOG|NRC_TESTRESULT 16/16' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held (saw ${TOTAL:-none})"; FAIL=1; }

if grep -q '^LOG|NRC_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: isConnected/getRootNode semantics incorrect"
    grep '^LOG|NRC_TESTFAIL' "$WORK/console.txt"
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
    echo "nb_node_root_connected_test.sh PASS (${TOTAL:-?})"
else
    echo "nb_node_root_connected_test.sh FAIL"
fi
exit $FAIL
