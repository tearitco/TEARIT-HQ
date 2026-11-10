#!/bin/bash
# nb_match_media_test.sh - matchMedia evaluates against the real viewport.
#
# The prelude used to define matchMedia as a constant-false stub, so every
# query took its "no" branch. These checks drive the native evaluator through
# nb_probe.py at a pinned 800x600 viewport: width/height boundaries, the
# level-4 range form, media types, orientation, comma OR, negation, an
# unknown feature (must stay false), and the listener surface
# (add/remove/dispatch + onchange). Console messages use spaces because the
# capture rewrites '|' to a space.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_match_media.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

printf '/*nbjs-script-boundary*/\nconsole.log("LOADOK");\n' > "$WORK/page.js"
printf 'N|#document|||0||0|\nD\nN|html|||3|\n\n\n|4|html\nD\nN|head|||0||4|head\nD\nN|title|||0||5|title\nU\nN|body|||3|\n\n\n|4|body\nD\nN|p|t||6|tick 0|8|p id="t"\nN|p|||48|Timer fixture: this paragraph must never change.|1|p\nU\nU\nU\n' > "$WORK/dom.txt"

cat > "$WORK/probe.js" <<'PROBE'
(function(){
  var pass=0,total=0;
  function check(n,g,w){ total++; if(g===w){pass++; console.log('MQ_TESTOK '+n);} else {console.log('MQ_TESTFAIL '+n+' got='+g+' want='+w);} }
  function mq(q){ return matchMedia(q); }
  check('callable', typeof matchMedia, 'function');
  check('min-width-true', mq('(min-width: 600px)').matches, true);
  check('min-width-false', mq('(min-width: 1200px)').matches, false);
  check('max-width-boundary', mq('(max-width: 800px)').matches, true);
  check('width-exact', mq('(width: 800px)').matches, true);
  check('height-exact', mq('(height: 600px)').matches, true);
  check('range-form', mq('(width >= 700px)').matches, true);
  check('orientation-landscape', mq('(orientation: landscape)').matches, true);
  check('orientation-portrait', mq('(orientation: portrait)').matches, false);
  check('scheme-light', mq('(prefers-color-scheme: light)').matches, true);
  check('media-type-screen', mq('screen and (min-width: 600px)').matches, true);
  check('media-type-print', mq('print and (min-width: 600px)').matches, false);
  check('comma-or', mq('(min-width: 1200px), (max-width: 900px)').matches, true);
  check('negation', mq('not all and (min-width: 1200px)').matches, true);
  check('unknown-feature', mq('(totally-unknown: 1)').matches, false);
  check('media-string', mq('(min-width: 600px)').media, '(min-width: 600px)');
  check('resolution', mq('(min-resolution: 2dppx)').matches, false);
  check('pointer-fine', mq('(pointer: fine)').matches, true);
  var hits=0; var l=function(){hits++;}; var x=mq('(min-width: 1px)');
  x.addEventListener('change', l);
  x.dispatchEvent({type:'change'});
  check('listener-called', hits, 1);
  x.removeEventListener('change', l);
  x.dispatchEvent({type:'change'});
  check('listener-removed', hits, 1);
  x.onchange = l;
  x.dispatchEvent({type:'change'});
  check('onchange-called', hits, 2);
  console.log('MQ_TESTRESULT '+pass+'/'+total);
  return 'MQ_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title mq:fixture \
    --width 800 --height 600 \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>MQ_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never ran"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^LOG|MQ_TESTRESULT 21/21' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held"; FAIL=1; }

if grep -q '^LOG|MQ_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: matchMedia is not behaviorally real"
    grep '^LOG|MQ_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

if [ "$FAIL" -eq 0 ]; then
    echo "nb_match_media_test.sh PASS (21/21)"
else
    echo "nb_match_media_test.sh FAIL"
fi
exit $FAIL
