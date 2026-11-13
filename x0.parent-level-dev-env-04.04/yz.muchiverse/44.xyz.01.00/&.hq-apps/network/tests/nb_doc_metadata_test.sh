#!/bin/bash
# nb_doc_metadata_test.sh - cheap document metadata on live and parsed documents.
#
# DOMParser documents and the live document lacked basic Document properties
# (contentType, URL/documentURI, compatMode, doctype, characterSet/charset/
# inputEncoding, implementation), so libraries probing them saw undefined.
# Parsed documents report the MIME type passed to parseFromString and the
# inert "about:blank" URL; the live document reports text/html and the page
# href. Both expose a DOMImplementation with the existing constructors.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_doc_meta.XXXXXX)"
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
dom += rec('p','t','','tick 0','p id="t"')
dom += 'U\nU\nU\n'
open(p,'w').write(dom)
PY

cat > "$WORK/probe.js" <<'PROBE'
(function(){
  var pass=0,total=0;
  function check(n,g,w){ total++; if(String(g)===String(w)){pass++; console.log('TC_TESTOK '+n);} else {console.log('TC_TESTFAIL '+n+' got='+g+' want='+w);} }

  var h=new DOMParser().parseFromString('<html><body></body></html>','text/html');
  check('html-contentType', h.contentType, 'text/html');
  check('html-URL', h.URL, 'about:blank');
  check('html-documentURI', h.documentURI, 'about:blank');
  check('html-compatMode', h.compatMode, 'CSS1Compat');
  check('html-doctype', h.doctype === null, true);
  check('html-characterSet', h.characterSet, 'UTF-8');
  check('html-charset', h.charset, 'UTF-8');
  check('html-inputEncoding', h.inputEncoding, 'UTF-8');
  check('html-impl-type', typeof h.implementation, 'object');
  check('html-impl-createHTMLDocument', typeof h.implementation.createHTMLDocument, 'function');
  check('html-impl-createDocument', typeof h.implementation.createDocument, 'function');
  check('html-impl-hasFeature', h.implementation.hasFeature('HTML','1.0'), true);

  var x=new DOMParser().parseFromString('<root/>','application/xml');
  check('xml-contentType', x.contentType, 'application/xml');
  check('xml-URL', x.URL, 'about:blank');
  check('xml-documentURI', x.documentURI, 'about:blank');
  check('xml-compatMode', x.compatMode, 'CSS1Compat');
  check('xml-doctype', x.doctype === null, true);

  check('live-contentType', document.contentType, 'text/html');
  check('live-URL', document.URL, 'https://example.invalid/path?q=1');
  check('live-documentURI', document.documentURI, 'https://example.invalid/path?q=1');
  check('live-compatMode', document.compatMode, 'CSS1Compat');
  check('live-doctype', document.doctype === null, true);
  check('live-characterSet', document.characterSet, 'UTF-8');
  check('live-impl-type', typeof document.implementation, 'object');
  check('live-impl-createHTMLDocument', typeof document.implementation.createHTMLDocument, 'function');
  check('live-impl-hasFeature', document.implementation.hasFeature('XML','1.0'), true);

  console.log('TC_TESTRESULT '+pass+'/'+total);
  return 'TC_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href 'https://example.invalid/path?q=1' --title meta:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 --drain-ms 1000 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>TC_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never completed"; sed -n '1,40p' "$WORK/err.log"; exit 1; }

TOTAL=$(grep -o 'TC_TESTRESULT [0-9]*/[0-9]*' "$WORK/console.txt" | tail -1 | sed 's|TC_TESTRESULT ||')
grep -q '^LOG|TC_TESTRESULT 26/26' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held (saw ${TOTAL:-none})"; FAIL=1; }

if grep -q '^LOG|TC_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: document metadata is incomplete"
    grep '^LOG|TC_TESTFAIL' "$WORK/console.txt"
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
    echo "nb_doc_metadata_test.sh PASS (${TOTAL:-?})"
else
    echo "nb_doc_metadata_test.sh FAIL"
fi
exit $FAIL
