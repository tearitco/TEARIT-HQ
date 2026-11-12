#!/bin/bash
# nb_attrs_serialize_test.sh - attribute values are markup-safe and entities
# decode to real UTF-8.
#
# innerHTML/outerHTML emit the stored attribute blob verbatim (that is what
# makes parser-sourced markup stable). setAttribute used to store its value
# raw, so a value containing '"' truncated the quoted attribute, and a value
# containing '&' could start an entity on the next parse. The entity decoder
# also mapped &nbsp; to a plain space and &mdash;/&copy; to single Latin-1
# bytes, so the serializer's own escapes did not round-trip.
#
# This gate pins:
#   - setAttribute stores a markup-safe value: & -> &amp;, " -> &quot;,
#     U+00A0 -> &nbsp;, while < and > stay literal (spec attribute mode);
#   - getAttribute returns the original string, before and after re-parsing
#     the serialized markup;
#   - a literal "&amp;" survives as a literal (escaped twice, decoded once);
#   - neighbouring attributes survive updates and removeAttribute;
#   - id/className properties update the serialized attribute, not just the
#     selector field;
#   - parser-sourced attributes keep their source escapes (no double
#     escaping) and decode for getAttribute;
#   - &nbsp;, &mdash;, &copy; and numeric references decode to their real
#     code points on both parse paths and survive a serialize/reparse loop;
#   - the decoded '&' reaches a visible RENDER row.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_attrs_serialize.XXXXXX)"
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
  var d=document.createElement('div');

  /* --- setAttribute stores markup-safe values ----------------------- */
  d.setAttribute('title','a & b');
  check('set-amp-outer', d.outerHTML, '<div title="a &amp; b"></div>');
  check('set-amp-get', d.getAttribute('title'), 'a & b');
  d.setAttribute('data-x','say "hi"');
  check('set-quote-outer', d.outerHTML.indexOf('data-x="say &quot;hi&quot;"')>=0, true);
  check('set-quote-get', d.getAttribute('data-x'), 'say "hi"');
  check('set-quote-no-broken-blob', d.outerHTML.split('"').length-1, 4);
  d.setAttribute('data-y','&amp;');
  check('set-literal-outer', d.outerHTML.indexOf('data-y="&amp;amp;"')>=0, true);
  check('set-literal-get', d.getAttribute('data-y'), '&amp;');
  d.setAttribute('data-n','a\u00a0b');
  check('set-nbsp-outer', d.outerHTML.indexOf('data-n="a&nbsp;b"')>=0, true);
  check('set-nbsp-get-code', d.getAttribute('data-n').charCodeAt(1), 160);
  d.setAttribute('data-lt','a < b > c');
  check('set-ltgt-outer', d.outerHTML.indexOf('data-lt="a < b > c"')>=0, true);
  check('set-ltgt-get', d.getAttribute('data-lt'), 'a < b > c');

  /* --- every value survives serialize -> reparse -------------------- */
  var rt=document.createElement('div');
  rt.innerHTML=d.outerHTML;
  var r=rt.firstChild;
  check('rt-amp', r.getAttribute('title'), 'a & b');
  check('rt-quote', r.getAttribute('data-x'), 'say "hi"');
  check('rt-literal', r.getAttribute('data-y'), '&amp;');
  check('rt-nbsp', r.getAttribute('data-n').charCodeAt(1), 160);
  check('rt-ltgt', r.getAttribute('data-lt'), 'a < b > c');

  /* --- updates keep their neighbours; remove keeps the rest --------- */
  check('neighbour-title', d.getAttribute('title'), 'a & b');
  d.removeAttribute('data-x');
  check('remove-gone', d.getAttribute('data-x'), null);
  check('remove-neighbour', d.getAttribute('data-n').charCodeAt(1), 160);
  check('remove-serialized', d.outerHTML.indexOf('data-x='), -1);

  /* --- dataset uses the same write path ----------------------------- */
  var ds=document.createElement('span');
  ds.dataset.foo='x&y';
  check('dataset-outer', ds.outerHTML, '<span data-foo="x&amp;y"></span>');
  check('dataset-get', ds.getAttribute('data-foo'), 'x&y');

  /* --- id/className properties reach the serialized attribute ------- */
  var el=document.createElement('b');
  el.id='i&d'; el.className='a"b';
  check('id-outer', el.outerHTML.indexOf('id="i&amp;d"')>=0, true);
  check('class-outer', el.outerHTML.indexOf('class="a&quot;b"')>=0, true);
  check('id-class-get', el.getAttribute('id')+'|'+el.className, 'i&d|a"b');
  el.id=''; el.className='';
  check('empty-id-class', el.outerHTML, '<b id="" class=""></b>');

  /* --- parser-sourced attributes stay as authored ------------------- */
  var pa=document.createElement('div');
  pa.innerHTML='<a title="a &amp; b" href="/x?a=1&amp;b=2">x</a>';
  var a=pa.firstChild;
  check('parse-get', a.getAttribute('title')+'|'+a.getAttribute('href'), 'a & b|/x?a=1&b=2');
  check('parse-outer', pa.outerHTML, '<div><a title="a &amp; b" href="/x?a=1&amp;b=2">x</a></div>');
  var pn=document.createElement('div');
  pn.innerHTML='<i title="a &#65; &#160; b">x</i>';
  check('parse-numeric-get', pn.firstChild.getAttribute('title'), 'a A \u00a0 b');
  check('parse-numeric-outer', pn.outerHTML, '<div><i title="a &#65; &#160; b">x</i></div>');

  /* --- entities decode to real code points (UTF-8) ------------------ */
  var tx=document.createElement('div');
  tx.innerHTML='a&nbsp;b &mdash; &copy; &#8212; &#65;';
  check('text-codes', Array.prototype.map.call(tx.textContent, function(c){return c.charCodeAt(0);}).join(','),
        '97,160,98,32,8212,32,169,32,8212,32,65');
  check('text-outer', tx.innerHTML, 'a&nbsp;b \u2014 \u00a9 \u2014 A');
  tx.innerHTML=tx.innerHTML;
  check('text-roundtrip', tx.textContent, 'a\u00a0b \u2014 \u00a9 \u2014 A');
  check('text-roundtrip-codes', tx.textContent.charCodeAt(1), 160);

  /* --- visible: decoded '&' renders as text, not markup ------------- */
  document.getElementById('t').innerHTML='A &amp; B';

  console.log('TC_TESTRESULT '+pass+'/'+total);
  return 'TC_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title attrs:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 --drain-ms 2000 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>TC_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never completed"; sed -n '1,40p' "$WORK/err.log"; exit 1; }

TOTAL=$(grep -o 'TC_TESTRESULT [0-9]*/[0-9]*' "$WORK/console.txt" | tail -1 | sed 's|TC_TESTRESULT ||')
grep -q '^LOG|TC_TESTRESULT 34/34' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held (saw ${TOTAL:-none})"; FAIL=1; }

if grep -q '^LOG|TC_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: attribute values/entities are not markup-safe"
    grep '^LOG|TC_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

# --- decoded text reaches the frame -------------------------------------
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
if rows.count("TEXT|A & B") != 1: bad.append("TEXT|A & B is not emitted exactly once")
if "SEL|#t\nTEXT|A & B" not in last: bad.append("decoded text is not owned by #t")
if "TEXT|tick 0" in rows: bad.append("stale TEXT|tick 0 survived")
if any("&amp;" in r or "&lt;" in r for r in rows): bad.append("an entity leaked into the rendered text")
if bad:
    print("FAIL: post-JS rows are wrong")
    for b in bad: print("  " + b)
    print("  rows: " + " ; ".join(rows))
    raise SystemExit(1)
print("rows: decoded '&' rendered once under #t, no entity text")
PY

if [ "$FAIL" -eq 0 ]; then
    echo "nb_attrs_serialize_test.sh PASS (${TOTAL:-?} + rows)"
else
    echo "nb_attrs_serialize_test.sh FAIL"
fi
exit $FAIL
