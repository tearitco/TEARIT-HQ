#!/bin/bash
# nb_resize_observer_test.sh - initial ResizeObserver entries are proven
# from the worker's real box geometry, not just API existence.
#
# The historical class only made typeof ResizeObserver "function";
# observe() was inert. This behavior gate creates an observer over a DOM
# node and checks the initial entry.
#
# Console text has '|' rewritten to a space, so the fixture never relies on
# pipes in logged assertions.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_resize_observer.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

printf '/*nbjs-script-boundary*/\nconsole.log("LOADOK");\n' > "$WORK/page.js"
printf 'N|#document|||0||0|\nD\nN|html|||3|\n\n\n|4|html\nD\nN|head|||0||4|head\nD\nN|title|||0||5|title\nU\nN|body|||3|\n\n\n|4|body\nD\nN|p|t||6|tick 0|8|p id="t"\nN|p|||48|Timer fixture: this paragraph must never change.|1|p\nU\nU\nU\n' > "$WORK/dom.txt"

cat > "$WORK/probe.js" <<'PROBE'
(function(){   var pass=0,total=0;   function check(name,got,want){ total++; if(got===want){ pass++; console.log('RO_TESTOK '+name); } else { console.log('RO_TESTFAIL '+name+' got='+got+' want='+want); }   }   var events=[];   var el=document.querySelector('#t');   var obs=new ResizeObserver(function(entries,observer){       var e=entries[0];       events.push(entries.length+':'+e.contentRect.width+'x'+e.contentRect.height+':'+(e.target===el)+':'+(this===observer)+':'+e.borderBoxSize[0].inlineSize+'x'+e.borderBoxSize[0].blockSize);     });   obs.observe(el);   check('initial-box-entry', events[0]||'none', '1:800x30:true:true:800x30');   var obs2=new ResizeObserver(function(entries){       events.push('second:'+entries[0].contentRect.width+'x'+entries[0].contentRect.height);     });   obs2.observe(document.querySelector('#nb-auto-1'));   check('second-target-entry', events[1]||'none', 'second:800x30');   console.log('RO_TESTRESULT '+pass+'/'+total);   return 'RO_TESTDONE'; })();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title resize:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>RO_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never ran"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^LOG|RO_TESTRESULT 2/2' "$WORK/console.txt" \
    || { echo "FAIL: not every ResizeObserver assertion held"; FAIL=1; }

if grep -q '^LOG|RO_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: ResizeObserver initial entries are not real"
    grep '^LOG|RO_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

if [ "$FAIL" -eq 0 ]; then
    echo "nb_resize_observer_test.sh PASS (2/2)"
else
    echo "nb_resize_observer_test.sh FAIL"
fi
exit $FAIL
