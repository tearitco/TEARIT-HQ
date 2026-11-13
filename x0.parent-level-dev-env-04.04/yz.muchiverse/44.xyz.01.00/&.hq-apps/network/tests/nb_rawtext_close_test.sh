#!/bin/bash
# nb_rawtext_close_test.sh - raw-text/RCDATA end tags allow whitespace before '>'.
#
# HTML permits whitespace (space/tab/LF/FF/CR) between a raw-text or RCDATA
# element's end-tag name and '>' (e.g. </script >, </style\t>). The parser
# used to require an immediate '>', so the close was missed and the element
# swallowed following markup as data. This gate pins permissive matching on
# both parse paths (folded/manager fetch and the tree path used by innerHTML),
# case-insensitive tag names, RCDATA entity decoding, and that following text
# becomes a real sibling.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_rawtext_close.XXXXXX)"
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

  /* script: space before '>' closes; following text is a sibling */
  var d=document.createElement('div');
  d.innerHTML='<script>var x=1;</script >after';
  check('script-space-ih', d.innerHTML, '<script>var x=1;</script>after');
  check('script-space-after', d.lastChild.textContent, 'after');

  /* script: tab and mixed case close tag */
  d.innerHTML='<script>var y=2;</SCRIPT\t>tail';
  check('script-tab-ih', d.innerHTML, '<script>var y=2;</script>tail');
  check('script-tab-after', d.lastChild.textContent, 'tail');

  /* style: whitespace close */
  d.innerHTML='<style>.a{b:1}</style >z';
  check('style-space-ih', d.innerHTML, '<style>.a{b:1}</style>z');
  check('style-space-after', d.lastChild.textContent, 'z');

  /* RCDATA title: entities decode, whitespace close, sibling preserved */
  d.innerHTML='<title>Hi &amp; bye</TITLE >after';
  check('title-tc', d.firstChild.textContent, 'Hi & bye');
  check('title-after', d.lastChild.textContent, 'after');
  check('title-childcount', d.childNodes.length, 2);

  /* textarea: literal content + whitespace close */
  d.innerHTML='<textarea><b>&amp;</b></TEXTAREA >ok';
  check('textarea-tc', d.firstChild.textContent, '<b>&</b>');
  check('textarea-after', d.lastChild.textContent, 'ok');

  /* newline before '>' */
  d.innerHTML='<script>z</script\n>n';
  check('script-nl', d.innerHTML, '<script>z</script>n');

  console.log('TC_TESTRESULT '+pass+'/'+total);
  return 'TC_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title rawclose:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 --drain-ms 1000 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>TC_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never completed"; sed -n '1,40p' "$WORK/err.log"; exit 1; }

TOTAL=$(grep -o 'TC_TESTRESULT [0-9]*/[0-9]*' "$WORK/console.txt" | tail -1 | sed 's|TC_TESTRESULT ||')
grep -q '^LOG|TC_TESTRESULT 12/12' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held (saw ${TOTAL:-none})"; FAIL=1; }

if grep -q '^LOG|TC_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: permissive raw-text/RCDATA close is not recognized"
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
    echo "nb_rawtext_close_test.sh PASS (${TOTAL:-?})"
else
    echo "nb_rawtext_close_test.sh FAIL"
fi
exit $FAIL
