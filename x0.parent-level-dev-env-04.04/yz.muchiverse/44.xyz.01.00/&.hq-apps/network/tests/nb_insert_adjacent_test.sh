#!/bin/bash
# nb_insert_adjacent_test.sh - insertAdjacentHTML/Text/Element.
#
# These three Element methods were absent (reads threw). By 2026-11-09 real
# pages use insertAdjacentHTML wherever they want one snippet without
# replacing the host's children. This gate drives the real slice through
# nb_probe.py and checks the four positions, text and element moves, the
# no-parent no-op, the SyntaxError on an unknown position, the ancestor
# cycle guard, and the childList MutationObserver records the insertion
# produces. Console messages use spaces because the capture rewrites '|'
# to a space.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_insert_adjacent.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

printf '/*nbjs-script-boundary*/\nconsole.log("LOADOK");\n' > "$WORK/page.js"
printf 'N|#document|||0||0|\nD\nN|html|||3|\n\n\n|4|html\nD\nN|head|||0||4|head\nD\nN|title|||0||5|title\nU\nN|body|||3|\n\n\n|4|body\nD\nN|p|t||6|tick 0|8|p id="t"\nN|p|||48|Timer fixture: this paragraph must never change.|1|p\nU\nU\nU\n' > "$WORK/dom.txt"

cat > "$WORK/probe.js" <<'PROBE'
(function(){
  var pass=0,total=0;
  function check(n,g,w){ total++; if(String(g)===String(w)){pass++; console.log('IA_TESTOK '+n);} else {console.log('IA_TESTFAIL '+n+' got='+g+' want='+w);} }
  function idof(x){ return x ? x.id : 'none'; }
  function kids(n){ var a=[]; for(var i=0;i<n.childNodes.length;i++){ var c=n.childNodes[i]; a.push(c.id||('#'+c.nodeName)); } return a.join(','); }
  var doc=new DOMParser().parseFromString('<html><body><div id="host"><i id="mid"></i></div><span id="tail"></span></body></html>','text/html');
  var host=doc.getElementById('host'), tail=doc.getElementById('tail'), body=doc.body;
  check('host-present', idof(host), 'host');
  check('method-present', typeof host.insertAdjacentHTML+':'+typeof host.insertAdjacentText+':'+typeof host.insertAdjacentElement, 'function:function:function');
  check('proto-present', typeof Element.prototype.insertAdjacentHTML, 'function');

  host.insertAdjacentHTML('BEFOREBEGIN','<b id="bb"></b>');
  host.insertAdjacentHTML('afterbegin','<b id="ab"></b>');
  host.insertAdjacentHTML('beforeend','<b id="be"></b>');
  host.insertAdjacentHTML('afterend','<b id="ae"></b>');
  check('body-order', kids(body), 'bb,host,ae,tail');
  check('host-order', kids(host), 'ab,mid,be');

  tail.insertAdjacentText('afterbegin','A');
  tail.insertAdjacentText('beforeend','B');
  check('text-first', tail.childNodes[0]?String(tail.childNodes[0].nodeValue):'none', 'A');
  check('text-last', tail.childNodes[tail.childNodes.length-1]?String(tail.childNodes[tail.childNodes.length-1].nodeValue):'none', 'B');

  var mv=document.createElement('u'); mv.id='mv';
  body.appendChild(mv);
  var ret=host.insertAdjacentElement('beforebegin',mv);
  check('elem-return', idof(ret), 'mv');
  check('elem-moved', (mv.parentNode===body && mv.nextSibling===host)?'yes':'no', 'yes');
  var ret2=tail.insertAdjacentElement('afterend',mv);
  check('elem-second-return', idof(ret2), 'mv');
  check('elem-second-place', (tail.nextSibling===mv && mv.parentNode===body)?'yes':'no', 'yes');

  var threw='no';
  try { host.insertAdjacentHTML('sideways','<x></x>'); } catch(e) { threw = String(e).indexOf('SyntaxError')>=0 ? 'syntax' : String(e); }
  check('invalid-html', threw, 'syntax');
  threw='no';
  try { host.insertAdjacentText('middle','x'); } catch(e) { threw = String(e).indexOf('SyntaxError')>=0 ? 'syntax' : String(e); }
  check('invalid-text', threw, 'syntax');

  var det=document.createElement('div');
  var detached='yes';
  try { det.insertAdjacentHTML('beforebegin','<b id="nope"></b>'); } catch(e) { detached='threw'; }
  check('detached-noop', detached+':'+(det.firstChild?'child':'empty'), 'yes:empty');
  check('detached-elem-null', String(det.insertAdjacentElement('afterend', document.createElement('s'))), 'null');

  threw='no';
  try { host.insertAdjacentElement('beforeend', body); } catch(e) { threw = String(e).indexOf('HierarchyRequestError')>=0 ? 'hierarchy' : String(e); }
  check('ancestor-guard', threw, 'hierarchy');

  try {
    var mo=new MutationObserver(function(rs){});
    mo.observe(host,{childList:true});
    host.insertAdjacentHTML('beforeend','<em id="m1"></em><em id="m2"></em>');
    var recs=mo.takeRecords();
    check('mo-records', recs.length, 2);
    check('mo-types', recs.length?recs[0].type:'none', 'childList');
    check('mo-added', (recs.length && recs[0].addedNodes && recs[0].addedNodes.length)?recs[0].addedNodes[0].id:'none', 'm1');
    check('mo-target', recs.length?idof(recs[0].target):'none', 'host');
    mo.disconnect();
  } catch(e) { check('mo-ran', String(e), 'no-throw'); }

  console.log('IA_TESTRESULT '+pass+'/'+total);
  return 'IA_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title ia:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>IA_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never ran"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^LOG|IA_TESTRESULT 20/20' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held"; FAIL=1; }

if grep -q '^LOG|IA_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: insertAdjacent* is not behaviorally real"
    grep '^LOG|IA_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

if [ "$FAIL" -eq 0 ]; then
    echo "nb_insert_adjacent_test.sh PASS (20/20)"
else
    echo "nb_insert_adjacent_test.sh FAIL"
fi
exit $FAIL
