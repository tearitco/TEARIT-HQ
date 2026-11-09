#!/bin/bash
# nb_atob_test.sh - atob() must return a BINARY string and must not drop a
# padded tail. Hermetic: no window, no renderer, no network.
#
# WHY THIS EXISTS (2026-11-02)
# ---------------------------
# `rung6_atob_test.js` round-tripped ASCII only ("hello", "abc"), so it passed
# while the shipped C atob() was wrong in two ways ASCII cannot expose:
#
#   1. It handed its raw decoded bytes to JS_NewStringLen(), which decodes
#      UTF-8. Every byte >= 0x80 therefore came back as U+FFFD:
#      atob('yhXN') returned [FFFD,15,FFFD] instead of [CA,15,CD].
#   2. It never flushed the final partial base64 group, so a padded tail lost
#      its last byte: 'yhbPEA==' decoded 4 bytes' worth of base64 into 3.
#
# Both are invisible to an ASCII round-trip and fatal to any consumer that
# charCodeAt()s the result. Cloudflare's orchestrate script does exactly that -
# a rolling-XOR deobfuscator over atob output, whose result it feeds to
# BigInt() - so it died with "invalid bigint literal" on every request.
# Checks 4-6 below are that real decoder, on real literals lifted from a
# script Cloudflare actually served.
#
# NOTE ON THE ASSERTION FORMAT: console.log text goes through the worker's
# console capture, which rewrites '|' to a space, so messages here are joined
# with spaces. A pipe in a logged message would be unmatchable by any grep.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_atob.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

# A two-line DOM and a one-line page: nothing here depends on layout, so a
# failure can only mean the primitive is wrong.
printf '/*nbjs-script-boundary*/\nvar __nb_atob_fixture = 1;\n' > "$WORK/page.js"
printf 'N|#document|||0||0|\nD\nN|html|||0||0|\nU\n' > "$WORK/dom.txt"

cat > "$WORK/probe.js" <<'PROBE'
(function(){   var pass=0,total=0;   function codes(s){var a=[];for(var i=0;i<s.length;i++)a.push(s.charCodeAt(i).toString(16));return a.join(',');}   function check(name,got,want){     total++;     if(got===want){ pass++; console.log('ATOB_TESTOK '+name); }     else { console.log('ATOB_TESTFAIL '+name+' got=['+got+'] want=['+want+']'); }   }   check('atob-binary-bytes', codes(atob('yhXN')), 'ca,15,cd');   check('atob-padded-tail-2', String(atob('yhbPEA==').length), '4');   check('atob-padded-tail-1', String(atob('yhXN').length), '3');   function a0(m){ var C=atob(m),W=8443,x='',L=0,r=[];     while(L<C.length){ W=(W<<8|W>>>8)&65535; var N=C.charCodeAt(L++)^W>>>8;       if(N===255){ W=(W<<8|W>>>8)&65535; N=C.charCodeAt(L++)^W>>>8; W=(W<<8|W>>>8)&65535; N|=(C.charCodeAt(L++)^W>>>8)<<8; }       r.push(N); if(r.length===128){ x+=String.fromCharCode.apply(String,r); r=[]; } }     x+=String.fromCharCode.apply(String,r); return x; }   check('cf-decoder-small',  a0('yhXN'),     '156');   check('cf-decoder-medium', a0('yhbPEA=='), '1640');   check('cf-decoder-bigint', a0('y1jLEJ4ZnxOfQ5oRyBLDQc8ZmkTIFMsTnhSZQZ9EmhPMQc1ByhPNEctCzRDDQs4QwhnDE8JFyhDMFJ4XyRCdFZoTyELJRZlEw0PJRp1EyhKYEMJCnhDLEc5BzxbIFZpBwkTJEMkSnxidF8lGwhCeRMoRzRHLQ8gXzxKZEJlBnkbOQsxEmhfIRMxFzBmaRp0WmESZRJ5BmRfJFMISmEXLQcMVw0XPQ8pGz0PJF5oRz0WZQpkUmEXIQp5BmEafQcIYyUaeF88UzROeF81GzRXPQZpCy0POGcxEzkXME80YzUWaEc8ZyxLIRcNGzRCaRc0TzRWaE8sQzhWdRclDzkWZEp5CnUI='),  '0x00e9d3dca1328a49ad3403e4badda37a6a13610b608b5099839e1074e720f5a33b2ebd8c2ffd12c09be0015a4635aa9d2022d8f72f90ed11610c3742b0baef5b7da73d7e79aff6cdbdeab72492ce0a858e4c1f4c27a14ebbb4ce3beacfda982fe74463e76f654aab0c597d5e73686ea149023e8f60ae6365a30055fe2c5eb2ebfb');   console.log('ATOB_TESTRESULT '+pass+'/'+total);   return 'ATOB_TESTDONE'; })();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title atob:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

# Anchor every assertion to its OUTPUT line. The console capture also echoes
# the probe's own SOURCE, and that source contains these very tokens as
# string literals - an unanchored grep matches the echo and reports a failure
# that the run did not have (and a pass that it did not have either).
grep -q '^=>ATOB_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never ran"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^LOG|ATOB_TESTRESULT 6/6' "$WORK/console.txt" \
    || { echo "FAIL: not every atob assertion held"; FAIL=1; }

# Each failure is named, so a red run says WHICH property broke.
if grep -q '^LOG|ATOB_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: atob is not a binary string and/or drops a padded tail"
    grep '^LOG|ATOB_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

if [ "$FAIL" -eq 0 ]; then
    echo "nb_atob_test.sh PASS (6/6)"
else
    echo "nb_atob_test.sh FAIL"
fi
exit $FAIL
