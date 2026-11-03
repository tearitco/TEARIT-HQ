#!/bin/bash
# nb_text_blob_test.sh - TextEncoder/TextDecoder must be correct UTF-8, and a
# Blob must expose its readers. Hermetic: no window, no renderer, no network.
#
# WHY THIS EXISTS (2026-11-02)
# ---------------------------
# The atob bug (same day) survived for months because the only test for it
# round-tripped ASCII, and every failure it caused lived above 0x7F. These
# encoders are the same shape of risk: they are trivially correct for ASCII
# and wrong for everything else, so the assertions below are deliberately
# weighted to 2-, 3- and 4-byte sequences, surrogate pairs, a LONE surrogate
# (which must yield U+FFFD, not a throw) and malformed/truncated input.
#
# Blob.text()/arrayBuffer() are checked for being PROMISE-returning, not just
# present: a page that awaits a plain value observes undefined and carries on.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }

WORK="$(mktemp -d /tmp/nb_text.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM
printf '/*nbjs-script-boundary*/\nvar __nb_text_fixture = 1;\n' > "$WORK/page.js"
printf 'N|#document|||0||0|\nD\nN|html|||0||0|\nU\n' > "$WORK/dom.txt"
cat > "$WORK/probe.js" <<'PROBE'
(function(){var pass=0,total=0;function T(n,g,w){total++;if(String(g)===String(w)){pass++;console.log('TXTOK '+n);}else{console.log('TXTFAIL '+n+' got=['+g+'] want=['+w+']');}}var e=new TextEncoder();T('te-ascii', e.encode('A').join(','), '65');T('te-2byte', e.encode('\u00e9').join(','), '195,169');T('te-3byte', e.encode('\u20ac').join(','), '226,130,172');T('te-4byte-surrogate', e.encode('\ud83d\ude00').join(','), '240,159,152,128');T('te-lone-surrogate', e.encode('\ud800').join(','), '239,191,189');var d=new TextDecoder();T('td-2byte', d.decode(new Uint8Array([195,169])), '\u00e9');T('td-3byte', d.decode(new Uint8Array([226,130,172])), '\u20ac');T('td-4byte', d.decode(new Uint8Array([240,159,152,128])), '\ud83d\ude00');T('roundtrip', d.decode(e.encode('h\u00e9llo\u20ac\ud83d\ude00')), 'h\u00e9llo\u20ac\ud83d\ude00');T('td-malformed', d.decode(new Uint8Array([255,65])), '\ufffdA');T('td-truncated', d.decode(new Uint8Array([226,130])), '\ufffd');T('te-encoding', e.encoding, 'utf-8');T('td-encoding', d.encoding, 'utf-8');var b=new Blob(['hi']);T('blob-size', b.size, '2');T('blob-text-fn', typeof b.text, 'function');T('blob-ab-fn', typeof b.arrayBuffer, 'function');var p=null; try{ p=b.text(); }catch(err){}T('blob-text-promise', !!(p && typeof p.then==='function'), 'true');var q=null; try{ q=b.arrayBuffer(); }catch(err){}T('blob-ab-promise', !!(q && typeof q.then==='function'), 'true');var sl=b.slice(0,1);T('blob-slice-text-fn', typeof sl.text, 'function');T('blob-slice-size', sl.size, '1');console.log('TXTRESULT '+pass+'/'+total);return 'TXTDONE';})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title text:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 30 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

# Anchored to OUTPUT lines: the capture also echoes the probe's own source,
# which contains these tokens as literals.
grep -q '^=>TXTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never ran"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

if grep -q '^LOG|TXTFAIL' "$WORK/console.txt"; then
    echo "FAIL: a text/blob assertion did not hold"
    grep '^LOG|TXTFAIL' "$WORK/console.txt"
    FAIL=1
fi
res=$(sed -n 's/^LOG|TXTRESULT //p' "$WORK/console.txt" | head -1)
[ -z "$res" ] && { echo "FAIL: no result line"; FAIL=1; }
echo "measured: ${res:-none}"

if [ "$FAIL" -eq 0 ]; then
    echo "nb_text_blob_test.sh PASS ($res assertions)"
    exit 0
fi
echo "nb_text_blob_test.sh FAIL"
exit 1
