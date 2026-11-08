#!/bin/bash
# nb_intersection_observer_test.sh - initial IntersectionObserver entries
# are proven from real box geometry, not just API existence.
#
# The historical class only made typeof IntersectionObserver "function";
# observe() was inert. This behavior gate creates an observer over a DOM
# point inside the viewport and checks the initial entry.
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

WORK="$(mktemp -d /tmp/nb_intersection_observer.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

printf '/*nbjs-script-boundary*/\nconsole.log("LOADOK");\n' > "$WORK/page.js"
printf 'N|#document|||0||0|\nD\nN|html|||3|\n\n\n|4|html\nD\nN|head|||0||4|head\nD\nN|title|||0||5|title\nU\nN|body|||3|\n\n\n|4|body\nD\nN|p|t||6|tick 0|8|p id="t"\nN|p|||48|Timer fixture: this paragraph must never change.|1|p\nU\nU\nU\n' > "$WORK/dom.txt"

cat > "$WORK/probe.js" <<'PROBE'
(function(){   var pass=0,total=0;   function check(name,got,want){ total++; if(got===want){ pass++; console.log('IO_TESTOK '+name); } else { console.log('IO_TESTFAIL '+name+' got='+got+' want='+want); }   }   var seen=[];   var el=document.querySelector('#t');   var obs=new IntersectionObserver(function(entries,observer){       var e=entries[0];       seen.push(entries.length+':'+e.isIntersecting+':'+e.intersectionRatio+':'+(e.target===el)+':'+(this===observer)+':'+(e.boundingClientRect.width>0&&e.boundingClientRect.height>0));     });   obs.observe(el);   check('initial-visible-entry', seen[0]||'none', '1:true:1:true:true:true');   var far=document.createElement('div');   far.setAttribute('style','position:fixed;top:900px;left:0px;width:10px;height:10px');   document.querySelector('body').appendChild(far);   var obs2=new IntersectionObserver(function(entries){       seen.push('far:'+entries[0].isIntersecting+':'+entries[0].intersectionRatio);     });   obs2.observe(far);   check('off-viewport-entry', seen[1]||'none', 'far:false:0');   console.log('IO_TESTRESULT '+pass+'/'+total);   return 'IO_TESTDONE'; })();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title intersection:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>IO_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never ran"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^LOG|IO_TESTRESULT 2/2' "$WORK/console.txt" \
    || { echo "FAIL: not every IntersectionObserver assertion held"; FAIL=1; }

if grep -q '^LOG|IO_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: IntersectionObserver initial entries are not real"
    grep '^LOG|IO_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

if [ "$FAIL" -eq 0 ]; then
    echo "nb_intersection_observer_test.sh PASS (2/2)"
else
    echo "nb_intersection_observer_test.sh FAIL"
fi
exit $FAIL
