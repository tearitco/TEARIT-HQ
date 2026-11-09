#!/bin/bash
# nb_dataset_test.sh - dataset is a live DOMStringMap over data-* attributes.
#
# element.dataset was absent entirely (accessing it threw). These checks
# drive the real slice through nb_probe.py: dataset reads reflect data-*
# attributes, writes land in the attribute store (camelCase to kebab),
# Object.keys enumerates live keys, the object is stable across accesses,
# and deleting a key removes the attribute. Console messages use spaces
# because the capture rewrites '|' to a space.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_dataset.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

printf '/*nbjs-script-boundary*/\nconsole.log("LOADOK");\n' > "$WORK/page.js"
printf 'N|#document|||0||0|\nD\nN|html|||3|\n\n\n|4|html\nD\nN|head|||0||4|head\nD\nN|title|||0||5|title\nU\nN|body|||3|\n\n\n|4|body\nD\nN|p|t||6|tick 0|8|p id="t"\nN|p|||48|Timer fixture: this paragraph must never change.|1|p\nU\nU\nU\n' > "$WORK/dom.txt"

cat > "$WORK/probe.js" <<'PROBE'
(function(){ var pass=0,total=0; function check(n,g,w){ total++; if(g===w){pass++; console.log('DS_TESTOK '+n);} else {console.log('DS_TESTFAIL '+n+' got='+g+' want='+w);} } var doc=new DOMParser().parseFromString('<html><body><p id=x data-user-id="7" data-foo="bar">hi</p></body></html>','text/html'); var p=doc.getElementById('x'); check('read', p.dataset.userId, '7'); check('read2', p.dataset.foo, 'bar'); p.dataset.userId='9'; check('write-attr', p.getAttribute('data-user-id'), '9'); p.dataset.newKey='zz'; check('write-new', p.getAttribute('data-new-key'), 'zz'); check('keys', Object.keys(p.dataset).join(','), 'userId,foo,newKey'); check('same-object', String(p.dataset===p.dataset), 'true'); delete p.dataset.foo; check('deleted', String(p.dataset.foo), 'undefined'); console.log('DS_TESTRESULT '+pass+'/'+total); return 'DS_TESTDONE'; })();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title ds:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>DS_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never ran"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^LOG|DS_TESTRESULT 7/7' "$WORK/console.txt" \
    || { echo "FAIL: not every dataset assertion held"; FAIL=1; }

if grep -q '^LOG|DS_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: dataset is not behaviorally real"
    grep '^LOG|DS_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

if [ "$FAIL" -eq 0 ]; then
    echo "nb_dataset_test.sh PASS (7/7)"
else
    echo "nb_dataset_test.sh FAIL"
fi
exit $FAIL
