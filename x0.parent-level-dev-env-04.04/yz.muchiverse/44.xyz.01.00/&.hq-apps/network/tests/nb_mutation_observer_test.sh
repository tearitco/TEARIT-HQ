#!/bin/bash
# nb_mutation_observer_test.sh - behavioral (not existence) checks for
# MutationObserver. Hermetic: no window, no renderer, no network.
#
# The historical stub only made MutationObserver exist; observe() and the
# callbacks did nothing. These checks drive the async slice through
# nb_probe.py: childList/subtree append and remove, attribute set/remove,
# attribute filtering, microtask batching, takeRecords stealing, and
# disconnect suppression. Console messages use spaces because the capture
# rewrites '|' to a space.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_mutation_observer.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

printf '/*nbjs-script-boundary*/\nconsole.log("LOADOK");\n' > "$WORK/page.js"
printf 'N|#document|||0||0|\nD\nN|html|||3|\n\n\n|4|html\nD\nN|head|||0||4|head\nD\nN|title|||0||5|title\nU\nN|body|||3|\n\n\n|4|body\nD\nN|p|t||6|tick 0|8|p id="t"\nN|p|||48|Timer fixture: this paragraph must never change.|1|p\nU\nU\nU\n' > "$WORK/dom.txt"

cat > "$WORK/probe.js" <<'PROBE'
(function(){ var pass=0,total=0; function check(n,g,w){ total++; if(g===w){pass++; console.log('MO_TESTOK '+n);} else {console.log('MO_TESTFAIL '+n+' got='+g+' want='+w);} } var calls=0; var batches=[]; var body=document.querySelector('body'); var obs=new MutationObserver(function(records,observer){ calls++; batches.push(records); }); check('constructor-method-shape', typeof obs.observe+' '+typeof obs.disconnect+' '+typeof obs.takeRecords, 'function function function'); obs.observe(body,{childList:true,subtree:true,attributes:true,attributeFilter:['data-mut']}); var el=document.createElement('div'); body.appendChild(el); el.setAttribute('data-mut','one'); check('sync-pending', String(calls), '0'); queueMicrotask(function(){ var b0=batches[0]; check('batched-delivery', String(calls), '1'); check('batched-record-count', String(b0?b0.length:0), '2'); check('batched-first-type', b0&&b0[0]?b0[0].type:'none', 'childList'); check('batched-second-type', b0&&b0[1]?b0[1].type:'none', 'attributes'); check('records-cleared-after-callback', String(obs.takeRecords().length), '0'); var child=document.createElement('span'); el.appendChild(child); queueMicrotask(function(){ var b1=batches[1]; check('subtree-append', String(calls)+':'+(b1&&b1[0]?b1[0].type+':'+b1[0].target.tagName:'none'), '2:childList:div'); el.removeChild(child); queueMicrotask(function(){ var b2=batches[2]; check('subtree-remove', String(calls)+':'+(b2&&b2[0]?b2[0].type+':'+b2[0].removedNodes.length:'none'), '3:childList:1'); el.setAttribute('data-mut','two'); queueMicrotask(function(){ var b3=batches[3]; check('attribute-set', String(calls)+':'+(b3&&b3[0]?b3[0].attributeName+':'+b3[0].oldValue:'none'), '4:data-mut:one'); el.removeAttribute('data-mut'); queueMicrotask(function(){ var b4=batches[4]; check('attribute-remove', String(calls)+':'+(b4&&b4[0]?b4[0].attributeName+':'+b4[0].oldValue:'none'), '5:data-mut:two'); var callsBefore=calls; body.appendChild(document.createElement('b')); var stolen=obs.takeRecords(); check('takeRecords-steals-pending', String(stolen.length), '1'); queueMicrotask(function(){ check('takeRecords-prevents-callback', String(calls), String(callsBefore)); body.appendChild(document.createElement('u')); var callsBefore2=calls; obs.disconnect(); queueMicrotask(function(){ check('disconnect-prevents-callback', String(calls), String(callsBefore2)); check('disconnect-clears-queue', String(obs.takeRecords().length), '0'); console.log('MO_TESTRESULT '+pass+'/'+total); }); }); }); }); }); }); }); return 'MO_TESTDONE'; })();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title mutation:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>MO_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never ran"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^LOG|MO_TESTRESULT 15/15' "$WORK/console.txt" \
    || { echo "FAIL: not every MutationObserver assertion held"; FAIL=1; }

if grep -q '^LOG|MO_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: MutationObserver is not behaviorally real"
    grep '^LOG|MO_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

if [ "$FAIL" -eq 0 ]; then
    echo "nb_mutation_observer_test.sh PASS (15/15)"
else
    echo "nb_mutation_observer_test.sh FAIL"
fi
exit $FAIL
