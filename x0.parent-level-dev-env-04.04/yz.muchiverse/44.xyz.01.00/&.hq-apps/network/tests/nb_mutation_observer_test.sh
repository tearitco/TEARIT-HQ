#!/bin/bash
# nb_mutation_observer_test.sh - behavioral (not existence) checks for
# MutationObserver. Hermetic: no window, no renderer, no network.
#
# The historical stub only made MutationObserver exist; observe() and the
# callbacks did nothing. These checks drive its first honest slice through
# nb_probe.py: childList/subtree append and remove, attribute set/remove,
# attribute filtering, disconnect, and takeRecords. Console messages use
# spaces because the capture rewrites '|' to a space.
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
(function(){   var pass=0,total=0;   function check(name,got,want){ total++; if(got===want){ pass++; console.log('MO_TESTOK '+name); } else { console.log('MO_TESTFAIL '+name+' got='+got+' want='+want); }   }   var events=[];   var body=document.querySelector('body');   var obs=new MutationObserver(function(records,observer){       var r=records[0];       events.push(records.length+':'+r.type+':'+r.target.tagName+':'+r.addedNodes.length+':'+r.removedNodes.length+':'+(r.attributeName||'')+':'+(r.oldValue===null?'null':r.oldValue)+':'+(this===observer));     });   check('constructor-method-shape', typeof obs.observe+' '+typeof obs.disconnect+' '+typeof obs.takeRecords, 'function function function');   obs.observe(body,{childList:true,subtree:true,attributes:true,attributeFilter:['data-mut']});   var el=document.createElement('div');   el.setAttribute('data-mut','one');   body.appendChild(el);   check('body-append', events[events.length-1], '1:childList:body:1:0::null:true');   var child=document.createElement('span');   el.appendChild(child);   check('subtree-append', events[events.length-1], '1:childList:div:1:0::null:true');   el.removeChild(child);   check('subtree-remove', events[events.length-1], '1:childList:div:0:1::null:true');   el.setAttribute('data-mut','two');   check('attribute-set', events[events.length-1], '1:attributes:div:0:0:data-mut:one:true');   el.removeAttribute('data-mut');   check('attribute-remove', events[events.length-1], '1:attributes:div:0:0:data-mut:two:true');   check('records-cleared-after-callback', String(obs.takeRecords().length), '0');   var before=events.length;   obs.disconnect();   body.appendChild(document.createElement('p'));   check('disconnect-stops-callbacks', String(events.length), String(before));   console.log('MO_TESTRESULT '+pass+'/'+total);   return 'MO_TESTDONE'; })();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title mutation:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>MO_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never ran"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^LOG|MO_TESTRESULT 8/8' "$WORK/console.txt" \
    || { echo "FAIL: not every MutationObserver assertion held"; FAIL=1; }

if grep -q '^LOG|MO_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: MutationObserver is not behaviorally real"
    grep '^LOG|MO_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

if [ "$FAIL" -eq 0 ]; then
    echo "nb_mutation_observer_test.sh PASS (8/8)"
else
    echo "nb_mutation_observer_test.sh FAIL"
fi
exit $FAIL
