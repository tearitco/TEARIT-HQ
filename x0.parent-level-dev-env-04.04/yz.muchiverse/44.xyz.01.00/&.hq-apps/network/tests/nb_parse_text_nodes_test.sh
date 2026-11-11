#!/bin/bash
# nb_parse_text_nodes_test.sh - parsed character data is a real #text node.
#
# Before this slice, nb_parse_html folded every text run into the current
# element's own `text` field. parseFromString('<div>x<i></i>y</div>') reported
# one child, innerHTML lost the text, and sibling/Range traversal could not
# see it. This gate drives the three script-facing parse surfaces (DOMParser,
# innerHTML=, insertAdjacentHTML) over the real worker and asserts:
#   - the browser-shaped child list (text nodes with nodeType 3)
#   - innerHTML round-trip and textContent across mixed content
#   - entity decoding inside a text node
#   - Range over parsed text, cloneNode(true) of a mixed tree
#   - replacing APIs drop the folded direct text a LOAD element carries
#   - the post-JS RENDER frame carries the new text once: a captioned <a>
#     emits its LINK row, its text child does not repeat as TEXT, and the
#     stale LOAD text is gone.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_parse_text.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

printf '/*nbjs-script-boundary*/\nconsole.log("LOADOK");\n' > "$WORK/page.js"

# LOAD fixture: a page element whose text lives in the folded element field
# (that is the fetch.dom wire model). Replacing its innerHTML must drop it.
python3 - "$WORK/dom.txt" <<'PY'
import sys
rows = [
    "N|#document|||0||0|",
    "D",
    "N|html|||3|",
    "",
    "",
    "|4|html",
    "D",
    "N|head|||0||4|head",
    "D",
    "N|title|||0||5|title",
    "U",
    "N|body|||3|",
    "",
    "",
    "|4|body",
    "D",
    "N|p|t||6|tick 0|8|p id=\"t\"",
    "U",
    "U",
    "U",
]
open(sys.argv[1], "w").write("\n".join(rows) + "\n")
PY

cat > "$WORK/probe.js" <<'PROBE'
(function(){
  var pass=0,total=0;
  function check(n,g,w){ total++; if(String(g)===String(w)){pass++; console.log('PT_TESTOK '+n);} else {console.log('PT_TESTFAIL '+n+' got='+g+' want='+w);} }
  function val(x){ return x ? String(x.nodeValue) : 'none'; }

  var doc=new DOMParser().parseFromString('<html><body><div id="a">x<i id="i1">MID</i>y</div><p id="p">hello</p><p id="e">a &amp; b</p></body></html>','text/html');
  var a=doc.getElementById('a'), i1=doc.getElementById('i1'), p=doc.getElementById('p'), e=doc.getElementById('e');

  check('kids-count', a.childNodes.length, 3);
  check('first-is-text', a.firstChild ? a.firstChild.nodeName+':'+a.firstChild.nodeType : 'none', '#text:3');
  check('first-value', val(a.firstChild), 'x');
  check('last-value', val(a.lastChild), 'y');
  check('inner-html-roundtrip', a.innerHTML, 'x<i id="i1">MID</i>y');
  check('text-content', a.textContent, 'xMIDy');
  check('prev-sibling-text', val(i1.previousSibling), 'x');
  check('next-sibling-text', val(i1.nextSibling), 'y');
  check('entities-decoded', e.textContent, 'a & b');
  check('query-still-elements', doc.querySelectorAll('i').length, 1);
  check('paragraph-first-is-text', p.firstChild ? String(p.firstChild.nodeType) : 'none', '3');

  var r=doc.createRange(); r.selectNodeContents(p);
  check('range-text', r.toString(), 'hello');

  var c=a.cloneNode(true);
  check('clone-kids', c.childNodes.length, 3);
  check('clone-text', c.textContent, 'xMIDy');
  check('clone-inner', c.innerHTML, 'x<i id="i1">MID</i>y');

  var t=document.getElementById('t');
  t.innerHTML='<a href="/x">hi</a> tail';
  check('set-kids', t.childNodes.length, 2);
  check('set-first-name', t.firstChild ? t.firstChild.nodeName : 'none', 'a');
  check('set-last-text', val(t.lastChild), ' tail');
  check('set-text-content', t.textContent, 'hi tail');
  check('set-inner-html', t.innerHTML, '<a href="/x">hi</a> tail');
  check('old-folded-text-gone', String(t.textContent).indexOf('tick 0'), -1);
  check('set-second-is-text', t.childNodes[1] ? String(t.childNodes[1].nodeType) : 'none', '3');

  var ia=document.createElement('div');
  ia.insertAdjacentHTML('beforeend','a<b>b</b>c');
  check('ia-kids', ia.childNodes.length, 3);
  check('ia-text', ia.textContent, 'abc');
  check('ia-inner', ia.innerHTML, 'a<b>b</b>c');

  var b=document.createElement('b'); b.appendChild(document.createTextNode('Q'));
  var rc=document.getElementById('t');
  rc.replaceChildren(document.createTextNode('zz'), b);
  check('replace-kids', rc.childNodes.length, 2);
  check('replace-text', rc.textContent, 'zzQ');
  check('replace-drops-folded', String(rc.textContent).indexOf('tick 0'), -1);
  rc.replaceChildren();
  check('replace-empty', String(rc.childNodes.length)+':'+rc.textContent, '0:');

  /* Leave visible content: the worker emits RENDER only when the row buffer
   * is non-empty, and the row assertions below run on the post-EVAL frame. */
  rc.innerHTML='<a href="/x">hi</a> tail';

  console.log('PT_TESTRESULT '+pass+'/'+total);
  return 'PT_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title pt:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 --drain-ms 2000 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>PT_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never completed"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^LOG|PT_TESTRESULT 29/29' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held"; FAIL=1; }

if grep -q '^LOG|PT_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: parsed character data is not browser-shaped"
    grep '^LOG|PT_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

# The post-JS DOM must RENDER exactly once: LINK carries the link caption,
# its text child adds no duplicate TEXT row, and the replaced LOAD text is gone.
python3 - "$WORK/out.json" <<'PY' || FAIL=1
import json, sys
d = json.load(open(sys.argv[1]))
if d.get("errors"):
    print("FAIL: harness errors:", d["errors"]); raise SystemExit(1)
renders = [f for f in d.get("frames", []) if f.startswith("RENDER")]
if len(renders) < 2:
    print("FAIL: no post-JS RENDER frame (saw %d render frames)" % len(renders)); raise SystemExit(1)
rows = [l for l in renders[-1].split("\n")[1:] if l]
want = ["LINK|/x hi", "SEL|#t", "TEXT| tail"]
bad = []
for w in want:
    if w not in rows: bad.append("missing " + w)
if "TEXT|hi" in rows: bad.append("duplicate TEXT|hi from the captioned link")
if "TEXT|tick 0" in rows: bad.append("stale TEXT|tick 0 survived innerHTML=")
if "SEL|#t\nTEXT| tail" not in renders[-1]:
    bad.append("tail TEXT is not owned by #t")
if bad:
    print("FAIL: post-JS rows are wrong")
    for b in bad: print("  " + b)
    print("  rows: " + " ; ".join(rows))
    raise SystemExit(1)
print("post-JS rows: LINK once, no duplicate caption text, no stale folded text")
PY

if [ "$FAIL" -eq 0 ]; then
    echo "nb_parse_text_nodes_test.sh PASS (29/29 + rows)"
else
    echo "nb_parse_text_nodes_test.sh FAIL"
fi
exit $FAIL
