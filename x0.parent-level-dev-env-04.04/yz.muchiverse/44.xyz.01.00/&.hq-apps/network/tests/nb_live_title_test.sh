#!/bin/bash
# nb_live_title_test.sh - document.title stays in sync with the live <title>.
#
# The host used to mirror document.title in g_title only. dom_render_rows
# prefers the <title> element's text over g_title, so setting document.title
# never changed the element or the rendered TITLE row, and the getter could
# disagree with the DOM. document.title must read the <title> element when
# one exists, and setting it must update (or create) that element so the
# getter, the element and the RENDER frame all agree.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_live_title.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

cat > "$WORK/probe.js" <<'PROBE'
(function(){
  var pass=0,total=0;
  function check(n,g,w){ total++; if(String(g)===String(w)){pass++; console.log('TC_TESTOK '+n);} else {console.log('TC_TESTFAIL '+n+' got='+g+' want='+w);} }

  /* element text is authoritative over the LOAD-provided g_title */
  check('get-initial', document.title, 'Orig');
  var te = document.querySelector('title');
  check('el-initial', te ? te.textContent : 'NONE', 'Orig');

  document.title = 'Changed';
  check('get-after-set', document.title, 'Changed');
  check('el-after-set', document.querySelector('title').textContent, 'Changed');

  document.title = 'A & B';
  check('get-special', document.title, 'A & B');
  check('el-special', document.querySelector('title').textContent, 'A & B');

  document.title = '';
  check('get-empty', document.title, '');
  check('el-empty', document.querySelector('title').textContent, '');

  /* when no <title> element exists, the setter creates one under <head> */
  var t2 = document.querySelector('title');
  if (t2 && t2.parentNode) t2.parentNode.removeChild(t2);
  check('no-title-after-remove', document.querySelector('title'), null);
  document.title = 'Created';
  var t3 = document.querySelector('title');
  check('created-exists', !!t3, true);
  check('created-text', t3 ? t3.textContent : 'NONE', 'Created');
  check('created-parent', t3 && t3.parentNode ? t3.parentNode.tagName : 'NONE', 'head');
  check('get-created', document.title, 'Created');

  console.log('TC_TESTRESULT '+pass+'/'+total);
  return 'TC_TESTDONE';
})();
PROBE

run_case() {
  name="$1"; dom="$2"; href="$3"; title="$4"; console="$5"; out="$6"
  printf '/*nbjs-script-boundary*/\nconsole.log("LOADOK");\n' > "$WORK/$name.js"
  python3 "$HERE/nb_probe.py" \
      --worker "$WORKER" --page "$WORK/$name.js" --dom "$dom" \
      --href "$href" --title "$title" \
      --probe "$WORK/probe.js" --console "$console" \
      --load-timeout 30 --eval-timeout 60 --drain-ms 1500 > "$out" 2> "$WORK/$name.err" \
      || { echo "FAIL: harness could not complete $name"; sed -n '1,30p' "$WORK/$name.err"; exit 1; }
  grep -q '^=>TC_TESTDONE' "$console" \
      || { echo "FAIL: probe $name never completed"; sed -n '1,30p' "$WORK/$name.err"; exit 1; }
}

# --- case 1: live page with an existing <title> element -----------------
python3 - "$WORK/dom1.txt" <<'PY'
import sys
p = sys.argv[1]
def rec(tag, id_, cls, text, attrs):
    return "N|%s|%s|%s|%d|%s|%d|%s\n" % (tag, id_, cls, len(text), text, len(attrs), attrs)
dom  = rec('#document','','','', '') + 'D\n'
dom += rec('html','','','', 'html') + 'D\n'
dom += rec('head','','','', 'head') + 'D\n'
dom += rec('title','','','Orig','title') + 'U\n'
dom += rec('body','','','', 'body') + 'D\n'
dom += rec('p','t','','tick 0','p id="t"')
dom += 'U\nU\nU\n'
open(p,'w').write(dom)
PY
run_case c1 "$WORK/dom1.txt" https://example.invalid/ live:fixture "$WORK/c1.console.txt" "$WORK/c1.out.json"

TOTAL1=$(grep -o 'TC_TESTRESULT [0-9]*/[0-9]*' "$WORK/c1.console.txt" | tail -1 | sed 's|TC_TESTRESULT ||')
grep -q '^LOG|TC_TESTRESULT 13/13' "$WORK/c1.console.txt" \
    || { echo "FAIL: not every assertion held (case 1 saw ${TOTAL1:-none})"; FAIL=1; }
if grep -q '^LOG|TC_TESTFAIL' "$WORK/c1.console.txt"; then
    echo "FAIL: document.title is not in sync with the live <title>"
    grep '^LOG|TC_TESTFAIL' "$WORK/c1.console.txt"
    FAIL=1
fi

python3 - "$WORK/c1.out.json" <<'PY' || FAIL=1
import json, sys
d = json.load(open(sys.argv[1]))
if d.get("errors"):
    print("FAIL: harness errors (case 1):", d["errors"]); raise SystemExit(1)
renders = [f for f in d.get("frames", []) if f.startswith("RENDER")]
if len(renders) < 2:
    print("FAIL: no post-JS RENDER frame (case 1, saw %d)" % len(renders)); raise SystemExit(1)
last = renders[-1]
rows = [l for l in last.split("\n")[1:] if l]
bad = []
if rows.count("TITLE|Created") != 1: bad.append("TITLE|Created is not emitted exactly once")
if "SEL|#t\nTEXT|tick 0" not in last: bad.append("body content changed unexpectedly")
if "TITLE|Orig" in rows: bad.append("stale TITLE|Orig survived")
if bad:
    print("FAIL: post-JS rows are wrong (case 1)")
    for b in bad: print("  " + b)
    print("  rows: " + " ; ".join(rows))
    raise SystemExit(1)
print("rows: TITLE updated to Created, body unchanged")
PY

# --- case 2: no <title> element, setter must create it under <head> ------
cat > "$WORK/probe2.js" <<'PROBE'
(function(){
  var pass=0,total=0;
  function check(n,g,w){ total++; if(String(g)===String(w)){pass++; console.log('TC_TESTOK '+n);} else {console.log('TC_TESTFAIL '+n+' got='+g+' want='+w);} }
  check('initial-empty', document.title, '');
  check('no-title', document.querySelector('title'), null);
  document.title = 'Made';
  check('get-made', document.title, 'Made');
  var t = document.querySelector('title');
  check('made-exists', !!t, true);
  check('made-text', t ? t.textContent : 'NONE', 'Made');
  check('made-parent', t && t.parentNode ? t.parentNode.tagName : 'NONE', 'head');
  console.log('TC_TESTRESULT '+pass+'/'+total);
  return 'TC_TESTDONE';
})();
PROBE
python3 - "$WORK/dom2.txt" <<'PY'
import sys
p = sys.argv[1]
def rec(tag, id_, cls, text, attrs):
    return "N|%s|%s|%s|%d|%s|%d|%s\n" % (tag, id_, cls, len(text), text, len(attrs), attrs)
dom  = rec('#document','','','', '') + 'D\n'
dom += rec('html','','','', 'html') + 'D\n'
dom += rec('head','','','', 'head') + 'U\n'
dom += rec('body','','','', 'body') + 'D\n'
dom += rec('p','t','','tick 0','p id="t"')
dom += 'U\nU\nU\n'
open(p,'w').write(dom)
PY
printf '/*nbjs-script-boundary*/\nconsole.log("LOADOK");\n' > "$WORK/c2.js"
python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/c2.js" --dom "$WORK/dom2.txt" \
    --href https://example.invalid/ --title '' \
    --probe "$WORK/probe2.js" --console "$WORK/c2.console.txt" \
    --load-timeout 30 --eval-timeout 60 --drain-ms 1500 > "$WORK/c2.out.json" 2> "$WORK/c2.err" \
    || { echo "FAIL: harness could not complete case 2"; sed -n '1,30p' "$WORK/c2.err"; exit 1; }
grep -q '^=>TC_TESTDONE' "$WORK/c2.console.txt" \
    || { echo "FAIL: probe case 2 never completed"; exit 1; }
TOTAL2=$(grep -o 'TC_TESTRESULT [0-9]*/[0-9]*' "$WORK/c2.console.txt" | tail -1 | sed 's|TC_TESTRESULT ||')
grep -q '^LOG|TC_TESTRESULT 6/6' "$WORK/c2.console.txt" \
    || { echo "FAIL: not every assertion held (case 2 saw ${TOTAL2:-none})"; FAIL=1; }
if grep -q '^LOG|TC_TESTFAIL' "$WORK/c2.console.txt"; then
    echo "FAIL: document.title did not create the missing <title>"
    grep '^LOG|TC_TESTFAIL' "$WORK/c2.console.txt"
    FAIL=1
fi

python3 - "$WORK/c2.out.json" <<'PY' || FAIL=1
import json, sys
d = json.load(open(sys.argv[1]))
if d.get("errors"):
    print("FAIL: harness errors (case 2):", d["errors"]); raise SystemExit(1)
renders = [f for f in d.get("frames", []) if f.startswith("RENDER")]
if len(renders) < 2:
    print("FAIL: no post-JS RENDER frame (case 2, saw %d)" % len(renders)); raise SystemExit(1)
last = renders[-1]
rows = [l for l in last.split("\n")[1:] if l]
if rows.count("TITLE|Made") != 1:
    print("FAIL: created title not rendered (case 2)")
    print("  rows: " + " ; ".join(rows))
    raise SystemExit(1)
if "SEL|#t\nTEXT|tick 0" not in last:
    print("FAIL: body content changed unexpectedly (case 2)")
    print("  rows: " + " ; ".join(rows))
    raise SystemExit(1)
print("rows: created TITLE rendered, body unchanged")
PY

if [ "$FAIL" -eq 0 ]; then
    echo "nb_live_title_test.sh PASS (${TOTAL1:-?} + ${TOTAL2:-?} + rows)"
else
    echo "nb_live_title_test.sh FAIL"
fi
exit $FAIL
