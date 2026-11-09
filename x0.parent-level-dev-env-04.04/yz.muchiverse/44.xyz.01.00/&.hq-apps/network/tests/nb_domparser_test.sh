#!/bin/bash
# nb_domparser_test.sh - DOMParser delivers parsed documents, not {}.
#
# The historical stub (nb_empty_object) returned a plain empty object, so
# every library parsing a snippet crashed on the first query. These checks
# drive the real slice through nb_probe.py: HTML snippets with
# documentElement/body, getElementById, class queries, tag lists, XML RSS
# item extraction, document nodeType, unknown-mime rejection, and the main
# document staying untouched. Console messages use spaces because the
# capture rewrites '|' to a space.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_domparser.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

printf '/*nbjs-script-boundary*/\nconsole.log("LOADOK");\n' > "$WORK/page.js"
printf 'N|#document|||0||0|\nD\nN|html|||3|\n\n\n|4|html\nD\nN|head|||0||4|head\nD\nN|title|||0||5|title\nU\nN|body|||3|\n\n\n|4|body\nD\nN|p|t||6|tick 0|8|p id="t"\nN|p|||48|Timer fixture: this paragraph must never change.|1|p\nU\nU\nU\n' > "$WORK/dom.txt"

cat > "$WORK/probe.js" <<'PROBE'
(function(){ var pass=0,total=0; function check(n,g,w){ total++; if(g===w){pass++; console.log('DP_TESTOK '+n);} else {console.log('DP_TESTFAIL '+n+' got='+g+' want='+w);} } try{var doc=new DOMParser().parseFromString('<html><head><title>T</title></head><body><p id=x class=y>hello</p><p class=y>world</p></body></html>','text/html'); check('documentElement', doc.documentElement?doc.documentElement.tagName:'none', 'html'); check('body', doc.body?doc.body.tagName:'none', 'body'); check('byId', doc.getElementById('x')?doc.getElementById('x').textContent:'none', 'hello'); check('classCount', String(doc.querySelectorAll('.y').length), '2'); check('queryOne', doc.querySelector('#x')?doc.querySelector('#x').textContent:'none', 'hello'); check('tagCount', String(doc.getElementsByTagName('p').length), '2'); check('mainDocUntouched', String(document.querySelector('#t')?document.querySelector('#t').textContent:'none'), 'tick 0'); var doc2=new DOMParser().parseFromString('<rss><channel><item><desc>N1</desc></item><item><desc>N2</desc></item></channel></rss>','application/xml'); check('xmlItems', String(doc2.getElementsByTagName('item').length), '2'); check('xmlFirstDesc', doc2.querySelector('desc')?doc2.querySelector('desc').textContent:'none', 'N1'); check('nodeType', String(doc.nodeType), '9'); var threw='no'; try{new DOMParser().parseFromString('<p>x</p>','text/plain');}catch(e){threw='yes';} check('unknown-mime-throws', threw, 'yes'); }catch(e){ console.log('DP_THROW '+(e&&e.message?e.message:e)); } console.log('DP_TESTRESULT '+pass+'/'+total); return 'DP_TESTDONE'; })();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title domparser:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>DP_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never ran"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^LOG|DP_TESTRESULT 11/11' "$WORK/console.txt" \
    || { echo "FAIL: not every DOMParser assertion held"; FAIL=1; }

if grep -q '^LOG|DP_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: DOMParser is not behaviorally real"
    grep '^LOG|DP_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

if [ "$FAIL" -eq 0 ]; then
    echo "nb_domparser_test.sh PASS (11/11)"
else
    echo "nb_domparser_test.sh FAIL"
fi
exit $FAIL
