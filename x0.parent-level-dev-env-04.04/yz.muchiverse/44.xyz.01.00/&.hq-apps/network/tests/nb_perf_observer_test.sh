#!/bin/bash
# nb_perf_observer_test.sh - PerformanceObserver delivers User Timing marks.
#
# The historical class was an inert plain stub: the methods did not exist,
# so any page creating an observer crashed. These checks drive the real
# slice through nb_probe.py: unbuffered observers see only later marks,
# buffered observers replay earlier marks, takeRecords steals pending
# entries, and disconnect suppresses further delivery. Console messages
# use spaces because the capture rewrites '|' to a space.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_perf_observer.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

printf '/*nbjs-script-boundary*/\nconsole.log("LOADOK");\n' > "$WORK/page.js"
printf 'N|#document|||0||0|\nD\nN|html|||3|\n\n\n|4|html\nD\nN|head|||0||4|head\nD\nN|title|||0||5|title\nU\nN|body|||3|\n\n\n|4|body\nD\nN|p|t||6|tick 0|8|p id="t"\nN|p|||48|Timer fixture: this paragraph must never change.|1|p\nU\nU\nU\n' > "$WORK/dom.txt"

cat > "$WORK/probe.js" <<'PROBE'
(function(){ var pass=0,total=0; function check(n,g,w){ total++; if(g===w){pass++; console.log('PO_TESTOK '+n);} else {console.log('PO_TESTFAIL '+n+' got='+g+' want='+w);} } performance.mark('pre-a'); var calls=0; var seen=[]; var obs=new PerformanceObserver(function(entries,observer){ calls++; for(var i=0;i<entries.length;i++) seen.push(entries[i].name+':'+entries[i].entryType); }); obs.observe({entryTypes:['mark']}); check('method', typeof obs.observe+' '+typeof obs.disconnect+' '+typeof obs.takeRecords, 'function function function'); check('sync-pending', String(calls), '0'); queueMicrotask(function(){ check('unbuffered-empty', String(calls)+'@'+seen.length, '0@0'); performance.mark('post-b'); queueMicrotask(function(){ check('future-mark', String(calls)+':'+seen.join(','), '1:post-b:mark'); performance.mark('pre-c'); check('takeRecords-pending', String(obs.takeRecords().length), '1'); queueMicrotask(function(){ check('takeRecords-suppresses', String(calls), '1'); var calls2=0; var seen2=[]; var obs2=new PerformanceObserver(function(entries){ calls2++; for(var i=0;i<entries.length;i++) seen2.push('buf:'+entries[i].name); }); obs2.observe({entryTypes:['mark'],buffered:true}); queueMicrotask(function(){ check('buffered-replay', String(calls2)+':'+seen2.join(','), '1:buf:pre-a,buf:post-b,buf:pre-c'); obs2.disconnect(); performance.mark('post-d'); queueMicrotask(function(){ check('disconnect-suppresses', String(calls2), '1'); check('live-observer-still-fires', String(calls), '2'); console.log('PO_TESTRESULT '+pass+'/'+total); }); }); }); }); }); return 'PO_TESTDONE'; })();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title perf:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>PO_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never ran"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^LOG|PO_TESTRESULT 9/9' "$WORK/console.txt" \
    || { echo "FAIL: not every PerformanceObserver assertion held"; FAIL=1; }

if grep -q '^LOG|PO_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: PerformanceObserver is not behaviorally real"
    grep '^LOG|PO_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

if [ "$FAIL" -eq 0 ]; then
    echo "nb_perf_observer_test.sh PASS (9/9)"
else
    echo "nb_perf_observer_test.sh FAIL"
fi
exit $FAIL
