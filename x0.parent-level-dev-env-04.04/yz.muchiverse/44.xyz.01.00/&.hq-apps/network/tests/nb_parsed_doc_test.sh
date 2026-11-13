#!/bin/bash
# nb_parsed_doc_test.sh - DOMParser documents expose a real Document surface.
#
# parseFromString returned a push_node() wrapper: element-scoped queries
# worked, but the wrapper had no Document members. The host installs
# document.title on the live global only, so a parsed document's title read
# undefined, and createElement/createTextNode/createDocumentFragment were
# missing entirely. The live global also reported document.textContent as
# undefined, while the shared Node getter made parsed documents concatenate
# the whole tree.
#
# This gate pins:
#   - live document.textContent === null and assigning it is a no-op;
#   - parsed document.textContent === null (Document, not Element);
#   - parsed doc.title reads the first <title>, including RCDATA entities;
#   - doc.title replaces title text (both parse shapes), creating a <title>
#     under <head> when absent, synthesizing <head> only under an <html>
#     root, and leaving a fragment-like parse's root content intact;
#   - createElement/createElementNS/createTextNode/createDocumentFragment
#     exist and produce nodes that append into the parsed tree and serialize;
#   - parsed trees stay detached: nothing they contain reaches RENDER.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_parsed_doc.XXXXXX)"
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
dom += rec('title','','','Attrs','title') + 'U\n'
dom += rec('body','','','', 'body') + 'D\n'
dom += rec('p','t','','tick 0','p id="t"')
dom += 'U\nU\nU\n'
open(p,'w').write(dom)
PY

cat > "$WORK/probe.js" <<'PROBE'
(function(){
  var pass=0,total=0;
  function check(n,g,w){ total++; if(String(g)===String(w)){pass++; console.log('TC_TESTOK '+n);} else {console.log('TC_TESTFAIL '+n+' got='+g+' want='+w);} }

  /* --- live document: Document.textContent is null, assignment no-op --- */
  check('live-tc-null', document.textContent === null, true);
  document.textContent = 'nope';
  check('live-tc-set-noop', document.textContent === null, true);
  check('live-node-type', document.nodeType, 9);

  /* --- parsed document: same Document semantics ----------------------- */
  var doc = new DOMParser().parseFromString(
      '<html><head><title>P&nbsp;T</title></head><body><div id="host">seed</div></body></html>',
      'text/html');
  check('parsed-node-type', doc.nodeType, 9);
  check('parsed-tc-null', doc.textContent === null, true);
  check('parsed-documentElement', doc.documentElement && doc.documentElement.tagName, 'html');
  check('parsed-body', doc.body && doc.body.tagName, 'body');
  check('parsed-title', doc.title, 'P\u00a0T');

  /* --- factories exist and build nodes the parsed tree accepts -------- */
  check('factory-createElement', typeof doc.createElement, 'function');
  check('factory-createElementNS', typeof doc.createElementNS, 'function');
  check('factory-createTextNode', typeof doc.createTextNode, 'function');
  check('factory-createDocumentFragment', typeof doc.createDocumentFragment, 'function');

  var el = doc.createElement('span');
  el.id = 's1';
  el.textContent = 'hi & bye';
  check('created-element-tag', el.tagName, 'span');
  check('created-element-nodeType', el.nodeType, 1);
  doc.body.appendChild(el);
  check('append-element-html', doc.body.innerHTML,
        '<div id="host">seed</div><span id="s1">hi &amp; bye</span>');
  check('parsed-getElementById', doc.getElementById('s1').textContent, 'hi & bye');

  var tn = doc.createTextNode('tail');
  check('created-text-nodeType', tn.nodeType, 3);
  check('created-text-value', tn.textContent, 'tail');
  doc.body.appendChild(tn);
  check('append-text-html', doc.body.innerHTML,
        '<div id="host">seed</div><span id="s1">hi &amp; bye</span>tail');

  var frag = doc.createDocumentFragment();
  check('created-fragment-nodeType', frag.nodeType, 11);
  var fi = doc.createElement('i');
  fi.textContent = 'frag';
  frag.appendChild(fi);
  doc.body.appendChild(frag);
  check('append-fragment-html', doc.body.innerHTML,
        '<div id="host">seed</div><span id="s1">hi &amp; bye</span>tail<i>frag</i>');
  check('fragment-drained', frag.childNodes.length, 0);

  /* --- title setter replaces text on the tree parse path -------------- */
  doc.title = 'New & Title';
  check('title-set', doc.title, 'New & Title');
  check('title-set-head-html', doc.head.innerHTML, '<title>New &amp; Title</title>');
  doc.title = '';
  check('title-empty', doc.title, '');
  check('title-empty-html', doc.head.innerHTML, '<title></title>');

  /* --- no <head>: synthesize it only under an <html> root ------------- */
  var nh = new DOMParser().parseFromString('<html><body><p>x</p></body></html>', 'text/html');
  check('nohead-title-empty', nh.title, '');
  nh.title = 'Made';
  check('nohead-title-set', nh.title, 'Made');
  check('nohead-head-first', nh.documentElement.firstChild.tagName, 'head');
  check('nohead-head-html', nh.head.innerHTML, '<title>Made</title>');
  check('nohead-body-html', nh.body.innerHTML, '<p>x</p>');

  /* --- fragment-like parse must not gain a head inside its root ------- */
  var fr = new DOMParser().parseFromString('<div>x</div>', 'text/html');
  check('frag-title-empty', fr.title, '');
  fr.title = 'Frag';
  check('frag-title-set', fr.title, 'Frag');
  check('frag-root-intact', fr.documentElement.innerHTML, 'x');
  check('frag-title-node', fr.querySelector('title') && fr.querySelector('title').tagName, 'title');

  /* --- a document that is only a title element ------------------------ */
  var ot = new DOMParser().parseFromString('<title>Only</title>', 'text/html');
  check('only-title', ot.title, 'Only');
  ot.title = 'Changed';
  check('only-title-set', ot.title, 'Changed');

  console.log('TC_TESTRESULT '+pass+'/'+total);
  return 'TC_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title parsed-doc:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 --drain-ms 1500 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>TC_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never completed"; sed -n '1,40p' "$WORK/err.log"; exit 1; }

TOTAL=$(grep -o 'TC_TESTRESULT [0-9]*/[0-9]*' "$WORK/console.txt" | tail -1 | sed 's|TC_TESTRESULT ||')
grep -q '^LOG|TC_TESTRESULT 37/37' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held (saw ${TOTAL:-none})"; FAIL=1; }

if grep -q '^LOG|TC_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: the parsed-document surface is incomplete"
    grep '^LOG|TC_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

# --- parsed trees stay detached from the rendered document --------------
python3 - "$WORK/out.json" <<'PY' || FAIL=1
import json, sys
d = json.load(open(sys.argv[1]))
if d.get("errors"):
    print("FAIL: harness errors:", d["errors"]); raise SystemExit(1)
renders = [f for f in d.get("frames", []) if f.startswith("RENDER")]
if len(renders) < 2:
    print("FAIL: no post-JS RENDER frame (saw %d)" % len(renders)); raise SystemExit(1)
last = renders[-1]
rows = [l for l in last.split("\n")[1:] if l]
bad = []
if rows.count("TEXT|tick 0") != 1: bad.append("TEXT|tick 0 is not emitted exactly once")
if "TITLE|Attrs" not in rows: bad.append("the live <title> row disappeared")
if "TEXT|nope" in rows: bad.append("document.textContent assignment leaked into RENDER")
for leaked in ("seed", "Made", "Frag", "Changed", "hi & bye", "frag"):
    if any(leaked in r for r in rows): bad.append("parsed tree text leaked into RENDER: " + leaked)
if bad:
    print("FAIL: post-JS rows are wrong")
    for b in bad: print("  " + b)
    print("  rows: " + " ; ".join(rows))
    raise SystemExit(1)
print("rows: live frame unchanged; no parsed-document text leaked")
PY

if [ "$FAIL" -eq 0 ]; then
    echo "nb_parsed_doc_test.sh PASS (${TOTAL:-?} + rows)"
else
    echo "nb_parsed_doc_test.sh FAIL"
fi
exit $FAIL
