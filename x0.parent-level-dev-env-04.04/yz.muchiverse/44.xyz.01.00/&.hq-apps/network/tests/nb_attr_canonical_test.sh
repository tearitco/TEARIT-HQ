#!/bin/bash
# nb_attr_canonical_test.sh - parser-sourced attribute serialization is canonical/safe.
# Parser/raw attribute blobs may contain raw '&' or source entity forms; serializer
# must emit safe, semantically equivalent markup (decode+re-escape) without
# double-escaping normalized (setAttribute-produced) blobs.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_attr_canon.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

printf '/*nbjs-script-boundary*/\n' > "$WORK/page.js"
cat > "$WORK/dom.txt" <<'DOM'
N|#document|||0||0|
D
N|html|||0||4|html
D
N|body|||0||4|body
U
U
DOM

cat > "$WORK/probe.js" <<'PROBE'
(function(){
  var pass=0,total=0;
  function check(n,g,w){ total++; if(String(g)===String(w)){pass++; console.log('AC_TESTOK '+n);} else {console.log('AC_TESTFAIL '+n+' got='+g+' want='+w);} }

  var doc=new DOMParser().parseFromString('<html><body><div id="x" data-a="1&amp;2" data-b="a&b" title="say &quot;hi&quot;" disabled data-empty=""></div></body></html>','text/html');
  var d=doc.querySelector('#x');
  check('found', !!d, true);
  check('get-amp', d.getAttribute('data-a'), '1&2');
  check('get-raw', d.getAttribute('data-b'), 'a&b');
  check('get-quot', d.getAttribute('title'), 'say "hi"');
  check('has-disabled', d.hasAttribute('disabled'), true);
  check('get-empty', d.getAttribute('data-empty'), '');
  check('get-empty-present', d.hasAttribute('data-empty'), true);

  var ih=d.outerHTML;
  check('ser-amp', ih.indexOf('data-a="1&amp;2"')>=0, true);
  check('ser-raw-safe', ih.indexOf('data-b="a&amp;b"')>=0, true);
  check('ser-quot', ih.indexOf('title="say &quot;hi&quot;"')>=0, true);
  check('ser-disabled', /\sdisabled(?=[\s>])/.test(ih) || ih.indexOf(' disabled>')>=0 || ih.indexOf(' disabled ')>=0, true);
  check('ser-empty', ih.indexOf('data-empty=""')>=0, true);
  check('no-raw-amp-b', ih.indexOf('a&b"')<0, true);

  /* normalized/setAttribute blobs must not double-escape */
  var e=document.createElement('div');
  e.setAttribute('data-x','1&2');
  e.setAttribute('title','"hi"');
  e.setAttribute('data-q','a&b');
  check('set-get', e.getAttribute('data-x'), '1&2');
  var eh=e.outerHTML;
  check('set-ser-amp', eh.indexOf('data-x="1&amp;2"')>=0, true);
  check('set-ser-quot', eh.indexOf('title="&quot;hi&quot;"')>=0, true);
  check('set-ser-raw', eh.indexOf('data-q="a&amp;b"')>=0, true);
  check('no-double-amp', eh.indexOf('&amp;amp;')<0, true);

  /* id/class still reflected and serialized */
  check('id-reflected', d.id, 'x');
  check('id-in-ser', ih.indexOf('id="x"')>=0, true);

  console.log('AC_TESTRESULT '+pass+'/'+total);
  return 'AC_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title ac:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 --drain-ms 1000 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>AC_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never completed"; sed -n '1,60p' "$WORK/console.txt"; sed -n '1,40p' "$WORK/err.log"; exit 1; }

TOTAL=$(grep -o 'AC_TESTRESULT [0-9]*/[0-9]*' "$WORK/console.txt" | tail -1 | sed 's|AC_TESTRESULT ||')
grep -q '^LOG|AC_TESTRESULT 20/20' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held (saw ${TOTAL:-none})"; FAIL=1; }

if grep -q '^LOG|AC_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: attribute canonicalization is incorrect"
    grep '^LOG|AC_TESTFAIL' "$WORK/console.txt"
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
    echo "nb_attr_canonical_test.sh PASS (${TOTAL:-?})"
else
    echo "nb_attr_canonical_test.sh FAIL"
fi
exit $FAIL
