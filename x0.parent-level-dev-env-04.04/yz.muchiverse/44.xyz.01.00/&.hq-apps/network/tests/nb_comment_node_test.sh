#!/bin/bash
# nb_comment_node_test.sh - comment nodes (createComment, data, serialization, parse)
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_comment_node.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

printf '/*nbjs-script-boundary*/\nconsole.log("LOADOK");\n' > "$WORK/page.js"
python3 - "$WORK/dom.txt" <<'PYDOM'
import sys
p = sys.argv[1]
def rec(tag, id_, cls, text, attrs):
    return "N|%s|%s|%s|%d|%s|%d|%s\n" % (tag, id_, cls, len(text), text, len(attrs), attrs)
dom  = rec('#document','','','', '') + 'D\n'
dom += rec('html','','','', 'html') + 'D\n'
dom += rec('head','','','', 'head') + 'D\n'
dom += rec('title','','','t','title') + 'U\n'
dom += rec('body','','','', 'body') + 'D\n'
dom += rec('div','host','','','div id="host"') + 'U\n'
dom += 'U\nU\nU\n'
open(p,'w').write(dom)
PYDOM

cat > "$WORK/probe.js" <<'PROBE'
(function(){
  var pass=0,total=0;
  function check(n,g,w){ total++; if(String(g)===String(w)){pass++; console.log('CM_TESTOK '+n);} else {console.log('CM_TESTFAIL '+n+' got='+g+' want='+w);} }

  var c = document.createComment(' hi ');
  check('type', c.nodeType, 8);
  check('name', c.nodeName, '#comment');
  check('nv', c.nodeValue, ' hi ');
  check('tc', c.textContent, ' hi ');
  c.nodeValue = 'bye';
  check('set-nv', c.nodeValue, 'bye');
  check('set-tc-prop', c.textContent, 'bye');
  c.textContent = 'z';
  check('set-tc', c.nodeValue, 'z');
  c.nodeValue = 'ok';

  var host0 = document.createElement('div');
  document.documentElement.appendChild(host0);
  host0.appendChild(c);
  check('parent', c.parentNode === host0, true);
  check('owner', c.ownerDocument === document, true);
  var ih = host0.innerHTML;
  check('ser', ih.indexOf('<!--ok-->') >= 0, true);

  /* comment does not contribute to element textContent */
  var host = document.createElement('div');
  host.innerHTML = 'A<!--hidden-->B';
  check('inner', host.innerHTML, 'A<!--hidden-->B');
  check('no-text', host.textContent, 'AB');
  check('childCount', host.childNodes.length, 3);

  /* parsed comments preserved in DOMParser node mode */
  var doc = new DOMParser().parseFromString('<html><body><div>x<!--y-->z</div></body></html>','text/html');
  var d = doc.body.firstChild;
  check('parse-ih', d.innerHTML, 'x<!--y-->z');
  check('parse-tc', d.textContent, 'xz');
  check('parse-nt', d.childNodes[1] && d.childNodes[1].nodeType, 8);

  /* clone/import */
  var cl = c.cloneNode(true);
  check('clone-nt', cl.nodeType, 8);
  check('clone-val', cl.nodeValue, 'ok');
  var im = document.importNode(d.childNodes[1], true);
  check('import-owner', im.ownerDocument === document, true);
  check('import-nt', im.nodeType, 8);

  console.log('CM_TESTRESULT '+pass+'/'+total);
  return 'CM_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title cm:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 --drain-ms 1000 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>CM_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never completed"; sed -n '1,60p' "$WORK/console.txt"; sed -n '1,40p' "$WORK/err.log"; exit 1; }

TOTAL=$(grep -o 'CM_TESTRESULT [0-9]*/[0-9]*' "$WORK/console.txt" | tail -1 | sed 's|CM_TESTRESULT ||')
grep -q '^LOG|CM_TESTRESULT 20/20' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held (saw ${TOTAL:-none})"; FAIL=1; }

if grep -q '^LOG|CM_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: comment nodes are incorrect"
    grep '^LOG|CM_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

# render must not emit comment content
python3 - "$WORK/out.json" <<'PY' || FAIL=1
import json, sys
d = json.load(open(sys.argv[1]))
if d.get("errors"):
    print("FAIL: harness errors:", d["errors"]); raise SystemExit(1)
frames = "\n".join(d.get("frames", []))
if "hidden" in frames or "CM" in frames:
    print("FAIL: comment leaked into render frames"); raise SystemExit(1)
print("rows: no harness errors")
PY

if [ "$FAIL" -eq 0 ]; then
    echo "nb_comment_node_test.sh PASS (${TOTAL:-?})"
else
    echo "nb_comment_node_test.sh FAIL"
fi
exit $FAIL
