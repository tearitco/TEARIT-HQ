#!/bin/bash
# nb_sibling_nav_test.sh - structural sibling/child navigation accessors.
#
# The 2026-11-10 insertAdjacent gate had to work around `lastChild` and
# `previousSibling` reading undefined, and `parentElement`, the
# `*ElementChild` / `*ElementSibling` pair and `childElementCount` were
# absent too. This gate drives the new accessors through nb_probe.py over a
# parsed document with real text nodes between elements: the *Sibling
# properties must see text, the *ElementSibling ones must skip it, and every
# value must follow a live mutation without a cache.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_sibling_nav.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

printf '/*nbjs-script-boundary*/\nconsole.log("LOADOK");\n' > "$WORK/page.js"
printf 'N|#document|||0||0|\nD\nN|html|||3|\n\n\n|4|html\nD\nN|head|||0||4|head\nD\nN|title|||0||5|title\nU\nN|body|||3|\n\n\n|4|body\nD\nN|p|t||6|tick 0|8|p id="t"\nN|p|||48|Timer fixture: this paragraph must never change.|1|p\nU\nU\nU\n' > "$WORK/dom.txt"

cat > "$WORK/probe.js" <<'PROBE'
(function(){
  var pass=0,total=0;
  function check(n,g,w){ total++; if(String(g)===String(w)){pass++; console.log('SN_TESTOK '+n);} else {console.log('SN_TESTFAIL '+n+' got='+g+' want='+w);} }
  function idof(x){ return x && x.id ? x.id : (x ? ('#'+x.nodeName) : 'null'); }
  /* Parsed markup puts character data on the element's own text field
   * rather than in child text nodes, so this gate builds the mixed tree with
   * explicit createTextNode/insertBefore calls. That is the same tree shape
   * a script produces, and it tests the accessors rather than the parser. */
  var doc=new DOMParser().parseFromString('<html><body><div id="a"><i id="i1"></i></div><span id="s1"></span><span id="s2"></span></body></html>','text/html');
  var a=doc.getElementById('a'), i1=doc.getElementById('i1'), s1=doc.getElementById('s1'), s2=doc.getElementById('s2');
  var body=doc.body, htmlEl=doc.documentElement || doc.getElementsByTagName('html').item(0);
  a.insertBefore(document.createTextNode('x'), i1);
  a.appendChild(document.createTextNode('y'));

  check('last-child-node', a.lastChild?String(a.lastChild.nodeValue):'none', 'y');
  check('prev-sibling-text', i1.previousSibling?String(i1.previousSibling.nodeValue):'none', 'x');
  check('next-sibling-text', i1.nextSibling?String(i1.nextSibling.nodeValue):'none', 'y');
  check('text-has-prev', i1.nextSibling && i1.nextSibling.previousSibling?idof(i1.nextSibling.previousSibling):'none', 'i1');
  check('last-child-is-last', a.lastChild && a.lastChild.nextSibling===null?'yes':'no', 'yes');

  check('first-element-child', idof(a.firstElementChild), 'i1');
  check('last-element-child', idof(a.lastElementChild), 'i1');
  check('next-element-sibling', idof(s1.nextElementSibling), 's2');
  check('prev-element-sibling', idof(s2.previousElementSibling), 's1');
  check('prev-element-skips-nothing', idof(s1.previousElementSibling), 'a');
  check('no-next-element', String(s2.nextElementSibling), 'null');

  check('parent-element', idof(i1.parentElement), 'a');
  check('text-parent-element', i1.nextSibling?idof(i1.nextSibling.parentElement):'none', 'a');
  check('html-parent-element', String(htmlEl?htmlEl.parentElement:'missing'), 'null');

  check('count-body', body.childElementCount, 3);
  check('count-a', a.childElementCount, 1);
  check('count-agrees-children', a.children.length+':'+a.childElementCount, '1:1');

  var det=document.createElement('div');
  check('detached-nulls', [det.firstElementChild,det.lastElementChild,det.parentElement,det.previousSibling].map(function(v){return String(v);}).join(':')+':'+det.childElementCount, 'null:null:null:null:0');

  var b=document.createElement('b'); b.id='b';
  a.appendChild(b);
  check('live-append-element', idof(a.lastElementChild)+':'+a.childElementCount, 'b:2');
  a.appendChild(document.createTextNode('z'));
  check('live-append-text', String(a.lastChild.nodeValue)+':'+idof(a.lastElementChild)+':'+a.childElementCount, 'z:b:2');

  var tpl=document.createElement('template');
  var frag=tpl.content;
  var f1=document.createElement('b'); f1.id='f1'; frag.appendChild(f1);
  frag.appendChild(document.createTextNode('t'));
  var f2=document.createElement('i'); f2.id='f2'; frag.appendChild(f2);
  check('fragment-first', idof(frag.firstElementChild), 'f1');
  check('fragment-last', idof(frag.lastElementChild), 'f2');
  check('fragment-count', frag.childElementCount, 2);
  check('fragment-prev-sibling', (frag.lastChild && frag.lastChild.previousSibling) ? String(frag.lastChild.previousSibling.nodeValue) : 'none', 't');
  check('fragment-prev-element', idof(frag.lastElementChild.previousElementSibling), 'f1');

  check('proto-node', ('lastChild' in Node.prototype)+':'+('previousSibling' in Node.prototype)+':'+('parentElement' in Node.prototype), 'true:true:true');
  check('proto-element', ('firstElementChild' in Element.prototype)+':'+('lastElementChild' in Element.prototype)+':'+('nextElementSibling' in Element.prototype)+':'+('previousElementSibling' in Element.prototype)+':'+('childElementCount' in Element.prototype), 'true:true:true:true:true');

  console.log('SN_TESTRESULT '+pass+'/'+total);
  return 'SN_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title sn:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>SN_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never ran"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^LOG|SN_TESTRESULT 27/27' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held"; FAIL=1; }

if grep -q '^LOG|SN_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: structural navigation is not behaviorally real"
    grep '^LOG|SN_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

if [ "$FAIL" -eq 0 ]; then
    echo "nb_sibling_nav_test.sh PASS (27/27)"
else
    echo "nb_sibling_nav_test.sh FAIL"
fi
exit $FAIL
