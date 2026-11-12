#!/bin/bash
# nb_html_serialize_test.sh - innerHTML/outerHTML are real HTML serialization.
#
# The house serializer emitted text verbatim. That is not what a browser
# returns, and it breaks the round-trip pages actually rely on
# (`el.innerHTML = el.innerHTML`, template extraction, sanitizers):
#   - text must escape & < > and U+00A0, or a round-trip re-parses it;
#   - raw-text elements (script/style/...) must stay literal;
#   - void elements must not gain an end tag - `</br>` re-parses as `<br>`,
#     so a round-trip silently doubled a line break.
# outerHTML was also a read-only accessor: assigning to it did nothing,
# while libraries use outerHTML= to replace a node in place.
#
# This gate pins:
#   - escaped text and the parse -> serialize -> parse round-trip;
#   - raw script/style bodies, escaped title/textarea (RCDATA);
#   - void elements with no end tag and a stable <br> round-trip;
#   - outerHTML getter and setter: single/multi/text/null replacement, the
#     replaced node detached and wrapped, per-node childList records;
#   - spec errors for a detached node and for the documentElement;
#   - XMLSerializer keeps XML rules (&amp; yes, &nbsp; no, void end tags);
#   - the post-JS RENDER shows the replacement text once, not the old text.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_html_serialize.XXXXXX)"
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
dom += rec('title','','','Ser','title') + 'U\n'
dom += rec('body','','','', 'body') + 'D\n'
dom += rec('p','t','','tick 0','p id="t"')
dom += 'U\nU\nU\n'
open(p,'w').write(dom)
PY

cat > "$WORK/probe.js" <<'PROBE'
(function(){
  var pass=0,total=0;
  function check(n,g,w){ total++; if(String(g)===String(w)){pass++; console.log('TC_TESTOK '+n);} else {console.log('TC_TESTFAIL '+n+' got='+g+' want='+w);} }
  var d=document.createElement('div');

  /* --- text escaping and round-trip ------------------------------- */
  d.textContent='a & b < c > d';
  check('esc-text', d.innerHTML, 'a &amp; b &lt; c &gt; d');
  d.textContent='a\u00a0b';
  check('esc-nbsp', d.innerHTML, 'a&nbsp;b');
  d.innerHTML='x &amp; y &lt; z &gt; w';
  check('roundtrip-text', d.textContent, 'x & y < z > w');
  check('roundtrip-html', d.innerHTML, 'x &amp; y &lt; z &gt; w');
  d.innerHTML='<b title="q">a &amp; b</b>';
  d.innerHTML=d.innerHTML;
  check('roundtrip-stable', d.innerHTML, '<b title="q">a &amp; b</b>');
  check('roundtrip-text-once', d.textContent, 'a & b');

  /* --- raw-text stays literal; RCDATA is escaped ------------------ */
  d.innerHTML='<script>if (a < b && c > d) { q(); }</script>';
  check('raw-script', d.innerHTML, '<script>if (a < b && c > d) { q(); }</script>');
  d.innerHTML='<style>.x > .y { color: red }</style>';
  check('raw-style', d.innerHTML, '<style>.x > .y { color: red }</style>');
  d.innerHTML='<title>a &amp; b</title>';
  check('rcdata-title', d.innerHTML, '<title>a &amp; b</title>');
  d.innerHTML='<textarea>a &amp; <b>x</b></textarea>';
  check('rcdata-textarea-html', d.innerHTML, '<textarea>a &amp; &lt;b&gt;x&lt;/b&gt;</textarea>');
  check('rcdata-textarea-text', d.textContent, 'a & <b>x</b>');

  /* --- void elements have no end tag ------------------------------ */
  d.innerHTML='<br><img src="/x"><input type="text">';
  check('void-br', d.innerHTML.indexOf('</br>'), -1);
  check('void-img', d.innerHTML.indexOf('</img>'), -1);
  check('void-input', d.innerHTML.indexOf('</input>'), -1);
  check('void-markup', d.innerHTML, '<br><img src="/x"><input type="text">');
  var rb=document.createElement('div');
  rb.innerHTML='<br>';
  rb.innerHTML=rb.innerHTML;
  check('void-roundtrip-markup', rb.innerHTML, '<br>');
  check('void-roundtrip-count', rb.getElementsByTagName('br').length, 1);

  /* --- outerHTML getter ------------------------------------------- */
  var el=document.getElementById('t');
  check('oh-get', el.outerHTML, '<p id="t">tick 0</p>');

  /* --- outerHTML setter: one node, order, text, null -------------- */
  var host=document.createElement('div');
  host.innerHTML='<b id="old">old</b>';
  var old=host.firstChild;
  old.outerHTML='<i id="new">new</i>';
  check('oh-set-html', host.innerHTML, '<i id="new">new</i>');
  check('oh-set-old-detached', old.parentNode===null, true);
  check('oh-set-single', host.childNodes.length, 1);
  check('oh-set-new-id', host.firstChild.id, 'new');

  host.innerHTML='<b id="o2">o</b>';
  host.firstChild.outerHTML='<i>a</i><i>b</i>';
  check('oh-multi-order', host.innerHTML, '<i>a</i><i>b</i>');
  check('oh-multi-count', host.childNodes.length, 2);

  host.innerHTML='<b id="o3">o</b>';
  host.firstChild.outerHTML='plain & text';
  check('oh-text-escaped', host.innerHTML, 'plain &amp; text');
  check('oh-text-value', host.textContent, 'plain & text');

  host.innerHTML='<b id="o4">o</b>';
  host.firstChild.outerHTML=null;
  check('oh-null-removes', host.innerHTML+':'+host.childNodes.length, ':0');

  /* --- spec errors ------------------------------------------------- */
  var det=document.createElement('div'); var threw='no';
  try{det.outerHTML='x';}catch(e){threw='yes';}
  check('oh-detached-throws', threw, 'yes');
  var threw2='no';
  try{document.documentElement.outerHTML='<html></html>';}catch(e){threw2='yes';}
  check('oh-document-element-throws', threw2, 'yes');

  /* --- records: one per moved node, then the removed node --------- */
  var mo=new MutationObserver(function(){});
  host.innerHTML='<b id="o5">o</b>';
  var target=host.firstChild;
  mo.observe(host,{childList:true});
  target.outerHTML='<i>x</i><i>y</i>';
  var recs=mo.takeRecords();
  check('oh-mo-count', recs.length, 3);
  check('oh-mo-shape', recs.map(function(r){return (r.addedNodes.length?'+':'')+(r.removedNodes.length?'-':'');}).join(','), '+,+,-');
  check('oh-mo-removed-is-target', recs.length===3&&recs[2].removedNodes.length===1&&recs[2].removedNodes[0]===target, true);

  /* --- XMLSerializer keeps XML rules ------------------------------- */
  var xs=new XMLSerializer();
  var xd=document.createElement('div');
  xd.textContent='a\u00a0b & c';
  var xml=xs.serializeToString(xd);
  check('xml-amp', xml.indexOf('&amp;')>=0, true);
  check('xml-no-nbsp-entity', xml.indexOf('&nbsp;'), -1);
  check('xml-nbsp-raw', xml.indexOf('a\u00a0b')>=0, true);
  check('xml-void-closed', xs.serializeToString(document.createElement('br')), '<br></br>');

  /* --- visible effect: #t is replaced, not duplicated --------------- */
  el.outerHTML='<span id="newvis">newvis</span>';

  console.log('TC_TESTRESULT '+pass+'/'+total);
  return 'TC_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title ser:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 --drain-ms 2000 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>TC_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never completed"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

TOTAL=$(grep -o 'TC_TESTRESULT [0-9]*/[0-9]*' "$WORK/console.txt" | tail -1 | sed 's|TC_TESTRESULT ||')
grep -q '^LOG|TC_TESTRESULT 36/36' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held (saw ${TOTAL:-none})"; FAIL=1; }

if grep -q '^LOG|TC_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: HTML serialization is not browser-shaped"
    grep '^LOG|TC_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

# --- replacement is visible, old text is gone ---------------------------
python3 - "$WORK/out.json" <<'PY' || FAIL=1
import json, sys
d = json.load(open(sys.argv[1]))
if d.get("errors"):
    print("FAIL: harness errors:", d["errors"]); raise SystemExit(1)
renders = [f for f in d.get("frames", []) if f.startswith("RENDER")]
if len(renders) < 2:
    print("FAIL: no post-replacement RENDER frame (saw %d)" % len(renders)); raise SystemExit(1)
last = renders[-1]
rows = [l for l in last.split("\n")[1:] if l]
bad = []
if rows.count("TEXT|newvis") != 1: bad.append("TEXT|newvis is not emitted exactly once")
if "SEL|#newvis\nTEXT|newvis" not in last: bad.append("replacement text is not owned by #newvis")
if "TEXT|tick 0" in rows: bad.append("stale TEXT|tick 0 survived outerHTML=")
if "SEL|#t" in rows: bad.append("the replaced #t is still in the frame")
if bad:
    print("FAIL: post-replacement rows are wrong")
    for b in bad: print("  " + b)
    print("  rows: " + " ; ".join(rows))
    raise SystemExit(1)
print("rows: replacement text once, owned by #newvis, no stale #t")
PY

if [ "$FAIL" -eq 0 ]; then
    echo "nb_html_serialize_test.sh PASS (36/36 + rows)"
else
    echo "nb_html_serialize_test.sh FAIL"
fi
exit $FAIL
