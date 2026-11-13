#!/bin/bash
# nb_import_adopt_test.sh - document.importNode / document.adoptNode.
#
# Parsed and live documents can import (clone into document context) and adopt
# (detach + retarget owner) nodes. Imported/adopted nodes and their subtrees
# must report the target document as ownerDocument.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_import_adopt.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

printf '/*nbjs-script-boundary*/\nconsole.log("LOADOK");\n' > "$WORK/page.js"
python3 - "$WORK/dom.txt" <<'PY'
import sys
p = sys.argv[1]
def rec(tag, id_, cls, text, attrs):
    return "N|%s|%s|%s|%d|%s|%d|%s\n" % (tag, id_, cls, len(text), text, len(attrs), attrs)
dom  = rec('#document','','','', '') + 'D\n'
dom += rec('html','','','', 'html') + 'D\n'
dom += rec('head','','','', 'head') + 'D\n'
dom += rec('title','','','T','title') + 'U\n'
dom += rec('body','','','', 'body') + 'D\n'
dom += rec('div','host','','','div id="host"') + 'D\n'
dom += rec('span','s1','','hi','span id="s1"')
dom += 'U\nU\nU\nU\n'
open(p,'w').write(dom)
PY

cat > "$WORK/probe.js" <<'PROBE'
(function(){
  var pass=0,total=0;
  function check(n,g,w){ total++; if(String(g)===String(w)){pass++; console.log('IA_TESTOK '+n);} else {console.log('IA_TESTFAIL '+n+' got='+g+' want='+w);} }

  var doc=new DOMParser().parseFromString('<html><body><div id="x"><span id="y">t</span></div></body></html>','text/html');
  var x=doc.getElementById('x');
  var host=document.getElementById('host');

  /* importNode shallow: clone owned by target, original unchanged */
  var i1=document.importNode(x,false);
  check('imp-shallow-owner', i1.ownerDocument===document, true);
  check('imp-shallow-not-same', i1!==x, true);
  check('imp-shallow-no-child', i1.firstChild===null, true);
  check('imp-orig-kept', x.parentNode!==null, true);

  /* importNode deep */
  var i2=document.importNode(x,true);
  check('imp-deep-owner', i2.ownerDocument===document, true);
  check('imp-deep-child-owner', i2.firstChild && i2.firstChild.ownerDocument===document, true);
  check('imp-deep-text', String(i2.textContent).indexOf('t')>=0, true);

  /* import live node into parsed doc */
  var i3=doc.importNode(host,true);
  check('imp-into-parsed-owner', i3.ownerDocument===doc, true);
  check('imp-into-parsed-child', i3.firstChild && i3.firstChild.ownerDocument===doc, true);

  /* adoptNode detached */
  var a=doc.createElement('p'); a.textContent='z';
  check('adopt-det-before', a.ownerDocument===doc, true);
  var ad=document.adoptNode(a);
  check('adopt-det-same', ad===a, true);
  check('adopt-det-owner', a.ownerDocument===document, true);
  check('adopt-det-parent', a.parentNode===null, true);

  /* adoptNode attached node from parsed doc */
  var ad2=document.adoptNode(x);
  check('adopt-att-removed', doc.getElementById('x')===null, true);
  check('adopt-att-owner', ad2.ownerDocument===document, true);
  check('adopt-att-child-owner', ad2.firstChild && ad2.firstChild.ownerDocument===document, true);

  /* importNode/adoptNode document throws NotSupportedError */
  var e1='',e2='';
  try{document.importNode(doc,true);}catch(e){e1=e.name;}
  try{document.adoptNode(doc);}catch(e){e2=e.name;}
  check('import-doc-throws', e1==='NotSupportedError', true);
  check('adopt-doc-throws', e2==='NotSupportedError', true);

  /* fragment import deep */
  var f=doc.createDocumentFragment();
  var s=doc.createElement('span'); s.textContent='q'; f.appendChild(s);
  var fi=document.importNode(f,true);
  check('frag-import-owner', fi.ownerDocument===document, true);
  check('frag-import-child', fi.firstChild && fi.firstChild.ownerDocument===document, true);
  check('frag-import-text', String(fi.firstChild && fi.firstChild.textContent), 'q');

  /* appended imported node keeps target owner */
  document.body.appendChild(i2);
  check('imported-appended-owner', i2.ownerDocument===document, true);

  console.log('IA_TESTRESULT '+pass+'/'+total);
  return 'IA_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title ia:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 --drain-ms 1000 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>IA_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never completed"; sed -n '1,60p' "$WORK/console.txt"; sed -n '1,40p' "$WORK/err.log"; exit 1; }

TOTAL=$(grep -o 'IA_TESTRESULT [0-9]*/[0-9]*' "$WORK/console.txt" | tail -1 | sed 's|IA_TESTRESULT ||')
grep -q '^LOG|IA_TESTRESULT 22/22' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held (saw ${TOTAL:-none})"; FAIL=1; }

if grep -q '^LOG|IA_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: import/adopt is incorrect"
    grep '^LOG|IA_TESTFAIL' "$WORK/console.txt"
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
    echo "nb_import_adopt_test.sh PASS (${TOTAL:-?})"
else
    echo "nb_import_adopt_test.sh FAIL"
fi
exit $FAIL
