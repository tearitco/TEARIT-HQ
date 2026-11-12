#!/bin/bash
# nb_document_fragment_test.sh - a DocumentFragment is a real detached node.
#
# `document.createDocumentFragment()` did not exist at all (only
# <template>.content and shadow roots made fragments), and the shared
# insert paths had no fragment step. So the most common templating move -
# build a fragment, fill it, then append it once - could not run, and a
# fragment that did reach appendChild/insertBefore would have been stored as
# a child node the tree can never hold. Browsers do not store it: inserting a
# fragment MOVES its children into the target, in order, and leaves the
# fragment empty.
#
# This gate pins the browser shape:
#   - document.createDocumentFragment() and new DocumentFragment() exist and
#     return nodeType 11 with an empty child list.
#   - A fragment is a normal parent: appendChild/insertBefore,
#     getElementById, childNodes, textContent and cloneNode work, scoped to
#     its own subtree (the live document cannot see its contents).
#   - appendChild(fragment) returns the fragment, moves every child in order,
#     empties the fragment, and never inserts a fragment node.
#   - insertBefore(fragment, ref) inserts the children at the ref position;
#     replaceChild(fragment, old) puts them where old was and returns old.
#   - append/prepend/replaceChildren expand a fragment too.
#   - A childList observer sees one record per moved child.
#   - RENDER emits moved text once, under its owner.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_document_fragment.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

printf '/*nbjs-script-boundary*/\nconsole.log("LOADOK");\n' > "$WORK/page.js"
# A valid nb_serialize stream: tag|id|class|text-len|text|attr-len|attrs.
# The attr blob INCLUDES the tag name, so its length must be exact.
printf 'N|#document|||0||0|\nD\nN|html|||3|\n\n\n|4|html\nD\nN|head|||0||4|head\nD\nN|title|||0||5|title\nU\nN|body|||3|\n\n\n|4|body\nD\nN|p|t||6|tick 0|8|p id="t"\nU\nU\nU\n' > "$WORK/dom.txt"

cat > "$WORK/probe.js" <<'PROBE'
(function(){
  var pass=0,total=0;
  function check(n,g,w){ total++; if(String(g)===String(w)){pass++; console.log('DF_TESTOK '+n);} else {console.log('DF_TESTFAIL '+n+' got='+g+' want='+w);} }

  /* --- the factory and the constructor both exist --- */
  check('factory-type', typeof document.createDocumentFragment, 'function');
  check('constructor-type', typeof DocumentFragment, 'function');
  check('new-node-type', new DocumentFragment().nodeType, 11);

  /* --- a fresh fragment is a detached nodeType-11 parent --- */
  var f=document.createDocumentFragment();
  check('fresh-type', f.nodeType, 11);
  check('fresh-name', f.nodeName, '#document-fragment');
  check('fresh-children', f.childNodes.length, 0);
  check('fresh-parent', f.parentNode, null);
  check('fresh-empty', f.hasChildNodes(), false);

  /* --- it is a normal parent, scoped to its own subtree --- */
  var s1=document.createElement('span'); s1.id='s1'; s1.textContent='alpha';
  var s2=document.createElement('span'); s2.id='s2'; s2.textContent='beta';
  check('append-returns-child', f.appendChild(s1)===s1, true);
  f.appendChild(s2);
  check('child-count', f.childNodes.length, 2);
  check('first-last', f.firstChild.id+':'+f.lastChild.id, 's1:s2');
  check('child-owner', s1.parentNode.nodeType, 11);
  check('parent-identity', s1.parentNode===f, true);
  check('fragment-contains', f.contains(s1), true);
  check('fragment-scope', f.getElementById('s1')?f.getElementById('s1').id:'none', 's1');
  check('document-scope', document.getElementById('s1')===null, true);
  check('text-aggregate', f.textContent, 'alphabeta');

  /* --- textContent on a fragment: one real Text child, none when empty --- */
  f.textContent='x';
  check('text-set', f.childNodes.length+':'+f.textContent+':'+f.firstChild.nodeType, '1:x:3');
  f.textContent='';
  check('text-empty', f.childNodes.length+':'+f.textContent, '0:');

  /* --- appending a fragment MOVES its children into the host --- */
  var host=document.createElement('div'); host.id='host'; document.body.appendChild(host);
  var frag=document.createDocumentFragment();
  var x1=document.createElement('i'); x1.id='x1'; x1.textContent='one';
  var x2=document.createElement('i'); x2.id='x2'; x2.textContent='two';
  frag.appendChild(x1); frag.appendChild(x2);
  var moved=host.appendChild(frag);
  check('move-returns-fragment', moved===frag, true);
  check('move-host-count', host.childNodes.length, 2);
  check('move-fragment-empty', frag.childNodes.length, 0);
  check('move-order', host.firstChild.id+':'+host.lastChild.id, 'x1:x2');
  check('move-parent', x1.parentNode.id, 'host');
  var sawFragNode=false;
  for (var i=0;i<host.childNodes.length;i++)
    if (host.childNodes[i].nodeType===11) sawFragNode=true;
  check('move-no-fragment-node', sawFragNode, false);

  /* --- an empty fragment is a no-op that still returns the fragment --- */
  var empty=document.createDocumentFragment();
  var before=host.childNodes.length;
  var er=host.appendChild(empty);
  check('empty-returns-fragment', er===empty, true);
  check('empty-no-change', host.childNodes.length, before);
  check('empty-still-detached', empty.parentNode, null);

  /* --- insertBefore expands at the reference position --- */
  var ref=host.lastChild;
  var f2=document.createDocumentFragment();
  var y1=document.createElement('b'); y1.id='y1';
  var y2=document.createElement('b'); y2.id='y2';
  f2.appendChild(y1); f2.appendChild(y2);
  host.insertBefore(f2, ref);
  function ids(){ var a=[]; for (var j=0;j<host.childNodes.length;j++)
      a.push(host.childNodes[j].id||host.childNodes[j].nodeName); return a.join(','); }
  check('insert-order', ids(), 'x1,y1,y2,x2');
  check('insert-fragment-empty', f2.childNodes.length, 0);

  /* --- replaceChild puts the children where the old child was --- */
  var f3=document.createDocumentFragment();
  var z=document.createElement('u'); z.id='z1';
  f3.appendChild(z);
  var replaced=host.replaceChild(f3, ref);
  check('replace-returns-old', replaced===ref, true);
  check('replace-order', ids(), 'x1,y1,y2,z1');
  check('replace-old-detached', ref.parentNode, null);
  check('replace-fragment-empty', f3.childNodes.length, 0);

  /* --- ParentNode helpers expand a fragment too --- */
  var f4=document.createDocumentFragment();
  var w1=document.createElement('em'); w1.id='w1';
  var w2=document.createElement('em'); w2.id='w2';
  f4.appendChild(w1); f4.appendChild(w2);
  host.append(f4);
  check('append-method', ids(), 'x1,y1,y2,z1,w1,w2');
  check('append-method-empty', f4.childNodes.length, 0);
  var f5=document.createDocumentFragment();
  var p1=document.createElement('s'); p1.id='p1';
  f5.appendChild(p1);
  host.prepend(f5);
  check('prepend-method', host.firstChild.id, 'p1');
  check('prepend-method-empty', f5.childNodes.length, 0);
  var f6=document.createDocumentFragment();
  var r1=document.createElement('q'); r1.id='r1'; r1.textContent='rtext';
  f6.appendChild(r1);
  host.replaceChildren(f6);
  check('replaceChildren-method', host.childNodes.length+':'+host.firstChild.id, '1:r1');
  check('replaceChildren-method-empty', f6.childNodes.length, 0);

  /* --- one childList record per moved child --- */
  var mo=new MutationObserver(function(){});
  mo.observe(host,{childList:true});
  var f7=document.createDocumentFragment();
  var m1=document.createElement('span'); m1.id='m1';
  var m2=document.createElement('span'); m2.id='m2';
  f7.appendChild(m1); f7.appendChild(m2);
  host.appendChild(f7);
  var recs=mo.takeRecords();
  check('mo-record-count', recs.length, 2);
  check('mo-type', recs.length?recs[0].type:'none', 'childList');
  check('mo-added-first', recs.length&&recs[0].addedNodes[0]?recs[0].addedNodes[0].id:'none', 'm1');
  check('mo-added-second', recs.length>1&&recs[1].addedNodes[0]?recs[1].addedNodes[0].id:'none', 'm2');

  /* --- cloneNode copies the fragment shape, not the identity --- */
  var cf=f7.cloneNode(true);
  check('clone-empty', cf.nodeType+':'+cf.childNodes.length, '11:0');
  var f8=document.createDocumentFragment();
  var c1=document.createElement('span'); c1.textContent='clone me';
  f8.appendChild(c1);
  var cf8=f8.cloneNode(true);
  check('clone-full', cf8.nodeType+':'+cf8.childNodes.length+':'+cf8.textContent, '11:1:clone me');
  check('clone-distinct', cf8.firstChild!==c1, true);

  /* Leave visible fragment-authored content for the post-JS renderer check. */
  var vis=document.createDocumentFragment();
  var v1=document.createElement('span'); v1.textContent='visone';
  var v2=document.createElement('span'); v2.textContent='vistwo';
  vis.appendChild(v1); vis.appendChild(v2);
  document.body.appendChild(vis);

  console.log('DF_TESTRESULT '+pass+'/'+total);
  return 'DF_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title df:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 --drain-ms 2000 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>DF_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never completed"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

TOTAL=$(grep -o 'DF_TESTRESULT [0-9]*/[0-9]*' "$WORK/console.txt" | tail -1 | sed 's|DF_TESTRESULT ||')
grep -q '^LOG|DF_TESTRESULT 47/47' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held (saw ${TOTAL:-none})"; FAIL=1; }

if grep -q '^LOG|DF_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: DocumentFragment is not a browser-shaped node"
    grep '^LOG|DF_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

# Moved fragment children must render once, owned by their own element.
python3 - "$WORK/out.json" <<'PY' || FAIL=1
import json, sys
d = json.load(open(sys.argv[1]))
if d.get("errors"):
    print("FAIL: harness errors:", d["errors"]); raise SystemExit(1)
renders = [f for f in d.get("frames", []) if f.startswith("RENDER")]
if len(renders) < 2:
    print("FAIL: no post-JS RENDER frame (saw %d render frames)" % len(renders)); raise SystemExit(1)
rows = [l for l in renders[-1].split("\n")[1:] if l]
bad = []
for want in ("TEXT|rtext", "TEXT|visone", "TEXT|vistwo"):
    if want not in rows: bad.append("missing " + want)
    if rows.count(want) != 1: bad.append(want + " is not emitted exactly once")
if any(r.endswith("|#document-fragment") for r in rows):
    bad.append("a fragment node reached the renderer")
if bad:
    print("FAIL: post-JS rows are wrong")
    for b in bad: print("  " + b)
    print("  rows: " + " ; ".join(rows))
    raise SystemExit(1)
print("post-JS rows: moved text once per owner, no fragment node")
PY

if [ "$FAIL" -eq 0 ]; then
    echo "nb_document_fragment_test.sh PASS (47/47 + rows)"
else
    echo "nb_document_fragment_test.sh FAIL"
fi
exit $FAIL
