#!/bin/bash
# nb_scroll_metrics_test.sh - viewport scroll surface.
#
# Before 2026-11-11 the worker exposed window.scrollTo/scrollY but none of the
# properties pages actually read back:
#   - window.pageXOffset/pageYOffset: absent (lazy loaders do `pageYOffset + innerHeight`)
#   - document.scrollingElement: absent (libs test which element scrolls)
#   - documentElement.clientHeight/clientWidth: CSS-only, so 0 on unstyled <html>
#   - scrollTop/scrollLeft/scrollHeight/scrollWidth: absent on every element
# That made every infinite-scroll / sticky-header check compare against
# undefined or 0. This gate pins the surface to the worker's real box model:
# documentElement.scrollHeight is the height nb_doc_root_h already reports,
# clientHeight is the viewport, and scrollTop moves the same g_scroll_y that
# getBoundingClientRect uses.
#
# It also pins the model boundary honestly: <body> is NOT the scrolling
# element in this standards-mode document. body.scrollHeight is its own
# content height and body.scrollTop is a per-node worker-local value, the
# way inner scroll containers behave (no renderer channel drives them).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_scroll_metrics.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

printf '/*nbjs-script-boundary*/\nconsole.log("LOADOK");\n' > "$WORK/page.js"
# One <p id=t> (one 30px row), so every expected height is the box model's
# own number rather than a guess.
printf 'N|#document|||0||0|\nD\nN|html|||3|\n\n\n|4|html\nD\nN|head|||0||4|head\nD\nN|title|||0||5|title\nU\nN|body|||3|\n\n\n|4|body\nD\nN|p|t||6|tick 0|8|p id="t"\nU\nU\nU\n' > "$WORK/dom.txt"

cat > "$WORK/probe.js" <<'PROBE'
(function(){
  var pass=0,total=0;
  function check(n,g,w){ total++; if(String(g)===String(w)){pass++; console.log('SM_TESTOK '+n);} else {console.log('SM_TESTFAIL '+n+' got='+g+' want='+w);} }
  var de=document.documentElement, b=document.body;

  /* --- virgin viewport --- */
  check('initial-scrollx', window.scrollX, 0);
  check('initial-scrolly', window.scrollY, 0);
  check('initial-pagex', window.pageXOffset, 0);
  check('initial-pagey', window.pageYOffset, 0);
  check('scrolling-element-is-root', document.scrollingElement===de, true);
  check('scrolling-element-tag', document.scrollingElement?document.scrollingElement.tagName:'missing', 'html');

  /* --- root client box is the viewport, not an unstyled <html> box --- */
  check('root-client-width', de.clientWidth, 800);
  check('root-client-height', de.clientHeight, 600);

  /* --- document scrollHeight is the box model's document height,
   *     raised to clientHeight (CSSOM minimum) --- */
  check('short-doc-scroll-height', de.scrollHeight, 600);
  check('root-scroll-width', de.scrollWidth, 800);

  /* --- <body> is an ordinary element here (standards-mode model) --- */
  check('body-content-height', b.scrollHeight, 30);
  check('body-is-not-scroller', b.scrollTop, 0);

  /* --- window.scrollTo moves the scroll model and the rects built on it --- */
  var t=document.getElementById('t');
  var top0=t.getBoundingClientRect().top;
  window.scrollTo(0,120);
  check('scrollto-y', window.scrollY, 120);
  check('scrollto-pagey', window.pageYOffset, 120);
  check('scrollto-root-top', de.scrollTop, 120);
  check('scrollto-body-top', b.scrollTop, 0);
  check('scrollto-moves-rect', top0-t.getBoundingClientRect().top, 120);

  /* --- documentElement.scrollTop writes through to the same offset --- */
  de.scrollTop=45;
  check('set-root-top', window.scrollY, 45);
  check('set-root-top-pagey', window.pageYOffset, 45);

  /* --- body.scrollTop is per-node: it must not hijack the viewport --- */
  b.scrollTop=7;
  check('body-top-stored', b.scrollTop, 7);
  check('body-top-does-not-scroll', window.scrollY, 45);

  /* --- negative offsets clamp at the top --- */
  de.scrollTop=-20;
  check('negative-clamps', window.scrollY, 0);

  /* --- options form + horizontal axis --- */
  window.scrollTo({top:33,left:11});
  check('options-y', window.scrollY, 33);
  check('options-x', window.scrollX, 11);
  check('options-root-left', de.scrollLeft, 11);
  check('options-pagex', window.pageXOffset, 11);
  window.scrollTo(0,0);

  /* --- inner element: declared client box vs content, and its own offsets --- */
  var d=document.createElement('div');
  d.setAttribute('style','height:100px');
  var p=document.createElement('p'); p.textContent='x'; d.appendChild(p);
  check('inner-client-height', d.clientHeight, 100);
  check('inner-scroll-height', d.scrollHeight, 100);
  d.scrollTop=40; d.scrollLeft=5;
  check('inner-offsets', d.scrollTop+','+d.scrollLeft, '40,5');
  d.scrollTop=-3;
  check('inner-negative-clamps', d.scrollTop, 0);
  check('inner-width-is-client', d.scrollWidth+':'+d.clientWidth, '0:0');
  var d2=document.createElement('div');
  check('inner-offsets-are-per-node', d2.scrollTop+','+d2.scrollLeft, '0,0');

  /* --- display:none has no box, so no scroll extent --- */
  var h=document.createElement('div');
  h.setAttribute('style','display:none');
  var hp=document.createElement('p'); hp.textContent='x'; h.appendChild(hp);
  check('hidden-scroll-height', h.scrollHeight, 0);
  check('hidden-client-height', h.clientHeight, 0);

  /* --- empty element --- */
  var e=document.createElement('div');
  check('empty-box', e.scrollHeight+','+e.clientHeight+','+e.scrollTop, '0,0,0');

  /* --- growing the document grows documentElement.scrollHeight --- */
  for (var i=0;i<25;i++){ var q=document.createElement('p'); q.textContent='x'; b.appendChild(q); }
  check('grown-scroll-height', de.scrollHeight, 780);
  check('grown-client-height', de.clientHeight, 600);

  check('proto-element-surface',
    ('scrollTop' in Element.prototype)+':'+('scrollLeft' in Element.prototype)+':'+
    ('scrollHeight' in Element.prototype)+':'+('scrollWidth' in Element.prototype),
    'true:true:true:true');
  check('proto-window-surface',
    ('pageXOffset' in window)+':'+('pageYOffset' in window), 'true:true');

  console.log('SM_TESTRESULT '+pass+'/'+total);
  return 'SM_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title sm:fixture \
    --width 800 --height 600 \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>SM_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never ran"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

TOTAL=$(grep -o 'SM_TESTRESULT [0-9]*/[0-9]*' "$WORK/console.txt" | tail -1 | sed 's|SM_TESTRESULT ||')
grep -q '^LOG|SM_TESTRESULT 39/39' "$WORK/console.txt" \
    || { echo "FAIL: not every assertion held (saw ${TOTAL:-none})"; FAIL=1; }

if grep -q '^LOG|SM_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: the scroll surface is not behaviorally real"
    grep '^LOG|SM_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

if [ "$FAIL" -eq 0 ]; then
    echo "nb_scroll_metrics_test.sh PASS (39/39)"
else
    echo "nb_scroll_metrics_test.sh FAIL"
fi
exit $FAIL
