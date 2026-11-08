#!/bin/bash
# nb_scroll_test.sh - page-driven scroll is proven, not just API presence.
#
# The renderer owns the native pane scroll and exposes no channel, so this
# gate proves the worker-local page scroll: window.scrollTo/scrollBy,
# window.scrollY, viewport-relative getBoundingClientRect, scrollIntoView,
# and IntersectionObserver crossings driven by that scroll. Hermetic: no
# window, no renderer, no network.
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

WORK="$(mktemp -d /tmp/nb_scroll.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

printf '/*nbjs-script-boundary*/\nconsole.log("LOADOK");\n' > "$WORK/page.js"
printf 'N|#document|||0||0|\nD\nN|html|||3|\n\n\n|4|html\nD\nN|head|||0||4|head\nD\nN|title|||0||5|title\nU\nN|body|||3|\n\n\n|4|body\nD\nN|p|t||6|tick 0|8|p id="t"\nN|p|||48|Timer fixture: this paragraph must never change.|1|p\nU\nU\nU\n' > "$WORK/dom.txt"

cat > "$WORK/probe.js" <<'PROBE'
(function(){ var pass=0,total=0; function check(n,g,w){ total++; if(g===w){pass++; console.log('SC_TESTOK '+n);} else {console.log('SC_TESTFAIL '+n+' got='+g+' want='+w);} } var el=document.querySelector('#t'); check('initial-scroll', String(window.scrollY), '0'); check('initial-rect-y', String(Math.round(el.getBoundingClientRect().y)), '30'); var calls=0; var last='none'; var obs=new IntersectionObserver(function(entries){ calls++; last=entries[0].isIntersecting+':'+entries[0].intersectionRatio; }); obs.observe(el); queueMicrotask(function(){ check('io-initial', String(calls)+':'+last, '1:true:1'); window.scrollTo(0,600); check('scrollY', String(window.scrollY), '600'); check('rect-y-after-scroll', String(Math.round(el.getBoundingClientRect().y)), '-570'); queueMicrotask(function(){ check('io-scroll-crossing', String(calls)+':'+last, '2:false:0'); window.scrollTo(0,0); queueMicrotask(function(){ check('io-scroll-back', String(calls)+':'+last, '3:true:1'); check('scrollY-reset', String(window.scrollY), '0'); var target=document.querySelector('#nb-auto-1'); target.scrollIntoView({block:'start'}); check('scrollIntoView', String(window.scrollY), '60'); console.log('SC_TESTRESULT '+pass+'/'+total); }); }); }); return 'SC_TESTDONE'; })();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title scroll:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>SC_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never ran"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^LOG|SC_TESTRESULT 9/9' "$WORK/console.txt" \
    || { echo "FAIL: not every scroll assertion held"; FAIL=1; }

if grep -q '^LOG|SC_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: page-driven scroll is not behaviorally real"
    grep '^LOG|SC_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

if [ "$FAIL" -eq 0 ]; then
    echo "nb_scroll_test.sh PASS (9/9)"
else
    echo "nb_scroll_test.sh FAIL"
fi
exit $FAIL
