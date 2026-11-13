#!/bin/bash
# nb_rawtext_elements_test.sh - xmp/iframe/noembed/noframes/plaintext are raw text.
#
# The parser treated these as normal elements, so markup inside them was
# parsed and entities decoded. In HTML they are raw-text elements: their
# content is data, not markup, until the matching end tag (plaintext has no
# end tag and consumes the rest). This gate pins literal capture on both parse
# shapes, no entity decoding, permissive end-tag closing, plaintext tail
# behavior, serializer round-trip, and that RENDER never leaks their text.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_rawtext_elements.XXXXXX)"
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

  function rawCase(tag, label){
    var d=document.createElement('div');
    d.innerHTML='<'+tag+'><b>&amp;</b></'+tag+'>after';
    check(label+'-ih', d.innerHTML, '<'+tag+'><b>&amp;</b></'+tag+'>after');
    check(label+'-tc', d.textContent, '<b>&amp;</b>after');
    check(label+'-kids', d.childNodes.length, 2);
    check(label+'-inner-kids', d.firstChild.childNodes.length, 1);
    check(label+'-inner-type', d.firstChild.firstChild.nodeType, 3);
    check(label+'-after', d.lastChild.textContent, 'after');
  }
  rawCase('xmp','xmp');
  rawCase('iframe','iframe');
  rawCase('noembed','noembed');
  rawCase('noframes','noframes');

  /* permissive close (whitespace before '>') still works */
  var d=document.createElement('div');
  d.innerHTML='<xmp><p>x</p></XMP >z';
  check('xmp-ws-ih', d.innerHTML, '<xmp><p>x</p></xmp>z');
  check('xmp-ws-tc', d.textContent, '<p>x</p>z');

  /* plaintext consumes the rest of the fragment (no end tag) */
  d=document.createElement('div');
  d.innerHTML='<plaintext><b>&amp;</b><p>x';
  check('plaintext-ih', d.innerHTML, '<plaintext><b>&amp;</b><p>x</plaintext>');
  check('plaintext-tc', d.textContent, '<b>&amp;</b><p>x');
  check('plaintext-kids', d.childNodes.length, 1);
  check('plaintext-inner-kids', d.firstChild.childNodes.length, 1);

  /* round-trip: serialized markup reparses to the same literal content */
  var r=document.createElement('div');
  r.innerHTML='<xmp>a<b>b</b>c</xmp>';
  r.innerHTML=r.innerHTML;
  check('roundtrip-ih', r.innerHTML, '<xmp>a<b>b</b>c</xmp>');
  check('roundtrip-tc', r.textContent, 'a<b>b</b>c');

  console.log('TC_TESTRESULT '+pass+'/'+total);
  return 'TC_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title rawtext:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 --drain-ms 1000 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>TC_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never completed"; sed -n '1,40p' "$WORK/err.log"; exit 1; }

TOTAL=$(grep -o 'TC_TESTRESULT [0-9]*/[0-9]*' "$WORK/console.txt" | tail -1 | sed 's|TC_TESTRESULT ||')
grep -q '^LOG|TC_TESTRESULT 32/32' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held (saw ${TOTAL:-none})"; FAIL=1; }

if grep -q '^LOG|TC_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: raw-text elements are not captured literally"
    grep '^LOG|TC_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

# --- RENDER must not leak raw-text element bodies -----------------------
python3 - "$WORK/dom2.txt" <<'PY'
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
dom += rec('xmp','','','<b>LEAK</b>','xmp')
dom += rec('iframe','','','<p>FRAME</p>','iframe')
dom += rec('noembed','','','<i>EMB</i>','noembed')
dom += rec('noframes','','','<u>NOF</u>','noframes')
dom += 'U\nU\nU\n'
open(p,'w').write(dom)
PY
cat > "$WORK/probe2.js" <<'PROBE'
(function(){ return 'OK'; })();
PROBE
printf '/*nbjs-script-boundary*/\nconsole.log("LOADOK");\n' > "$WORK/page2.js"
python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page2.js" --dom "$WORK/dom2.txt" \
    --href https://example.invalid/ --title rawtext:render \
    --probe "$WORK/probe2.js" --console "$WORK/c2.console.txt" \
    --load-timeout 30 --eval-timeout 60 --drain-ms 1000 > "$WORK/c2.out.json" 2> "$WORK/c2.err" \
    || { echo "FAIL: harness could not complete render probe"; sed -n '1,30p' "$WORK/c2.err"; exit 1; }

python3 - "$WORK/c2.out.json" <<'PY' || FAIL=1
import json, sys
d = json.load(open(sys.argv[1]))
if d.get("errors"):
    print("FAIL: harness errors (render):", d["errors"]); raise SystemExit(1)
renders = [f for f in d.get("frames", []) if f.startswith("RENDER")]
if not renders:
    print("FAIL: no RENDER frame"); raise SystemExit(1)
last = renders[-1]
rows = [l for l in last.split("\n")[1:] if l]
bad = []
for leaked in ("LEAK", "FRAME", "EMB", "NOF", "<b>", "<p>", "<i>", "<u>"):
    if any(leaked in r for r in rows):
        bad.append("raw-text body leaked into RENDER: " + leaked)
if rows.count("TEXT|tick 0") != 1:
    bad.append("TEXT|tick 0 not emitted exactly once")
if bad:
    print("FAIL: RENDER leaked raw-text content")
    for b in bad: print("  " + b)
    print("  rows: " + " ; ".join(rows))
    raise SystemExit(1)
print("rows: no raw-text leakage, body text intact")
PY

if [ "$FAIL" -eq 0 ]; then
    echo "nb_rawtext_elements_test.sh PASS (${TOTAL:-?} + rows)"
else
    echo "nb_rawtext_elements_test.sh FAIL"
fi
exit $FAIL
