#!/bin/bash
# nb_text_content_test.sh - textContent assignment authors a real Text node.
#
# 9.26 made parsed character data a real #text child. The script-facing
# setter was still the folded model: `el.textContent = 'x'` stored one C
# string on the element, so childNodes stayed 0, firstChild was null, a
# Range could not touch the text, cloneNode had nothing to clone, and a
# childList MutationObserver saw nothing. All of that differs from a browser
# for the most common text update a page performs. This gate pins the
# browser shape:
#   - element.textContent = s  ->  zero children when s is "", else exactly
#     one Text node; existing children are gone; markup in s is literal text.
#   - only null maps to "": undefined becomes "undefined", 0 becomes "0".
#   - a Text node's own textContent replaces its data and adds no child.
#   - a childList record reports the added Text node.
#   - RENDER emits the new text once, owned by the element.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_text_content.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

printf '/*nbjs-script-boundary*/\nconsole.log("LOADOK");\n' > "$WORK/page.js"
# One LOAD paragraph whose text arrived folded on the element, so the gate
# also proves the old folded text is dropped when the setter takes over.
printf 'N|#document|||0||0|\nD\nN|html|||3|\n\n\n|4|html\nD\nN|head|||0||4|head\nD\nN|title|||0||5|title\nU\nN|body|||3|\n\n\n|4|body\nD\nN|p|t||6|tick 0|8|p id="t"\nU\nU\nU\n' > "$WORK/dom.txt"

cat > "$WORK/probe.js" <<'PROBE'
(function(){
  var pass=0,total=0;
  function check(n,g,w){ total++; if(String(g)===String(w)){pass++; console.log('TC_TESTOK '+n);} else {console.log('TC_TESTFAIL '+n+' got='+g+' want='+w);} }
  var p=document.getElementById('t');

  /* --- folded LOAD text is the starting point, not the end state --- */
  check('start-folded-value', p.textContent, 'tick 0');
  check('start-folded-child-count', p.childNodes.length, 0);

  /* --- element assignment authors one real Text node --- */
  p.textContent='fresh';
  check('set-child-count', p.childNodes.length, 1);
  check('set-text', p.textContent, 'fresh');
  check('set-first-is-text', p.firstChild.nodeType, 3);
  check('set-first-data', p.firstChild.data, 'fresh');
  check('set-first-owner', p.firstChild.parentNode.id, 't');
  check('set-drops-folded', p.textContent.indexOf('tick 0'), -1);

  /* --- replacement removes element children too --- */
  var d=document.createElement('div');
  d.innerHTML='<span>one</span><span>two</span>';
  check('replace-before', d.childNodes.length, 2);
  d.textContent='z';
  check('replace-child-count', d.childNodes.length, 1);
  check('replace-value', d.textContent, 'z');
  check('replace-drops-elements', d.querySelector('span')===null, true);

  /* --- empty / null / undefined / numbers, as a browser converts them --- */
  d.textContent='';
  check('empty-string', d.childNodes.length+':'+d.textContent, '0:');
  d.textContent=null;
  check('null-is-empty', d.childNodes.length+':'+d.textContent, '0:');
  d.textContent=undefined;
  check('undefined-is-string', d.childNodes.length+':'+d.textContent, '1:undefined');
  d.textContent=0;
  check('number-is-string', d.childNodes.length+':'+d.textContent, '1:0');

  /* --- markup is literal text, not parsed --- */
  d.textContent='<b>hi</b>';
  check('markup-stays-text', d.childNodes.length+':'+d.firstChild.nodeType, '1:3');
  check('markup-value', d.textContent, '<b>hi</b>');
  check('markup-no-element', d.getElementsByTagName('b').length, 0);

  /* --- a Text node replaces its data and gains no child --- */
  var t=document.createTextNode('orig');
  t.textContent='new';
  check('text-node-data', t.textContent+':'+t.nodeValue+':'+t.childNodes.length, 'new:new:0');
  t.textContent='';
  check('text-node-empty', t.textContent+':'+t.childNodes.length, ':0');

  /* --- clone sees the authored node --- */
  var cl=p.cloneNode(true);
  check('clone-text', cl.childNodes.length+':'+cl.textContent, '1:fresh');

  /* --- Range can select inside the authored node --- */
  var r=document.createRange();
  r.setStart(p.firstChild,1); r.setEnd(p.firstChild,5);
  check('range-on-authored-text', r.toString(), 'resh');

  /* --- childList record reports the added Text node --- */
  var mo=new MutationObserver(function(){});
  mo.observe(p,{childList:true});
  p.textContent='observed';
  var recs=mo.takeRecords();
  check('mo-record-count', recs.length, 1);
  check('mo-type', recs.length?recs[0].type:'none', 'childList');
  check('mo-target', recs.length&&recs[0].target===p, true);
  check('mo-added-count', recs.length?recs[0].addedNodes.length:'none', 1);
  check('mo-added-is-text', recs.length&&recs[0].addedNodes[0]?recs[0].addedNodes[0].nodeType:'none', 3);

  console.log('TC_TESTRESULT '+pass+'/'+total);
  return 'TC_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title tc:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 --drain-ms 2000 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>TC_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never completed"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

TOTAL=$(grep -o 'TC_TESTRESULT [0-9]*/[0-9]*' "$WORK/console.txt" | tail -1 | sed 's|TC_TESTRESULT ||')
grep -q '^LOG|TC_TESTRESULT 28/28' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held (saw ${TOTAL:-none})"; FAIL=1; }

if grep -q '^LOG|TC_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: textContent assignment is not browser-shaped"
    grep '^LOG|TC_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

# The post-JS DOM must render the authored text exactly once, under its owner.
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
if "TEXT|observed" not in rows: bad.append("missing TEXT|observed")
if rows.count("TEXT|observed") != 1: bad.append("TEXT|observed is not emitted exactly once")
if "SEL|#t\nTEXT|observed" not in renders[-1]: bad.append("authored text is not owned by #t")
if "TEXT|tick 0" in rows: bad.append("stale TEXT|tick 0 survived the setter")
if bad:
    print("FAIL: post-JS rows are wrong")
    for b in bad: print("  " + b)
    print("  rows: " + " ; ".join(rows))
    raise SystemExit(1)
print("post-JS rows: authored text once, owned by #t, no stale folded text")
PY

if [ "$FAIL" -eq 0 ]; then
    echo "nb_text_content_test.sh PASS (28/28 + rows)"
else
    echo "nb_text_content_test.sh FAIL"
fi
exit $FAIL
