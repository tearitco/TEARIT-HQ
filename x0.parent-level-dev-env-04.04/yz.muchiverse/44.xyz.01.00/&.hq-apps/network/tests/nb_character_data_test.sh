#!/bin/bash
# nb_character_data_test.sh - Text data edits deliver characterData records.
#
# Text nodes had no .data/nodeValue accessor at all, and MutationObserver
# characterData was an honest no-op with no mutation source. These checks
# drive the real slice through nb_probe.py: Text.data get/set, nodeValue,
# append into an element, and a characterData record (async, oldValue
# preserved), plus nodeValue === null on elements. Console messages use
# spaces because the capture rewrites '|' to a space.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_character_data.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

printf '/*nbjs-script-boundary*/\nconsole.log("LOADOK");\n' > "$WORK/page.js"
printf 'N|#document|||0||0|\nD\nN|html|||3|\n\n\n|4|html\nD\nN|head|||0||4|head\nD\nN|title|||0||5|title\nU\nN|body|||3|\n\n\n|4|body\nD\nN|p|t||6|tick 0|8|p id="t"\nN|p|||48|Timer fixture: this paragraph must never change.|1|p\nU\nU\nU\n' > "$WORK/dom.txt"

cat > "$WORK/probe.js" <<'PROBE'
(function(){ var pass=0,total=0; function check(n,g,w){ total++; if(g===w){pass++; console.log('CD_TESTOK '+n);} else {console.log('CD_TESTFAIL '+n+' got='+g+' want='+w);} } var doc=new DOMParser().parseFromString('<html><body><p id=x><b>bold</b></p></body></html>','text/html'); var p=doc.getElementById('x'); var tn=document.createTextNode('old'); tn.data='init'; check('data-get', tn.data, 'init'); tn.data='new'; check('data-set', tn.data, 'new'); check('value-text', String(tn.nodeValue), 'new'); p.appendChild(tn); check('linked', p.textContent.indexOf('new')>=0 ? 'yes' : 'no', 'yes'); var calls=0; var got=[]; var obs=new MutationObserver(function(records){ calls++; for(var i=0;i<records.length;i++) got.push(records[i].type+':'+records[i].oldValue); }); obs.observe(tn,{characterData:true}); tn.data='later'; check('sync-pending', String(calls), '0'); queueMicrotask(function(){ check('data-notified', String(calls)+'@'+got.join(','), '1@characterData:new'); check('value-element', String(p.nodeValue), 'null'); console.log('CD_TESTRESULT '+pass+'/'+total); }); return 'CD_TESTDONE'; })();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title cd:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>CD_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never ran"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^LOG|CD_TESTRESULT 7/7' "$WORK/console.txt" \
    || { echo "FAIL: not every CharacterData assertion held"; FAIL=1; }

if grep -q '^LOG|CD_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: CharacterData is not behaviorally real"
    grep '^LOG|CD_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

if [ "$FAIL" -eq 0 ]; then
    echo "nb_character_data_test.sh PASS (7/7)"
else
    echo "nb_character_data_test.sh FAIL"
fi
exit $FAIL
