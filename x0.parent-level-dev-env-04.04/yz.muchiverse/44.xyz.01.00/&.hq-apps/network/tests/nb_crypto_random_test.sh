#!/bin/bash
# nb_crypto_random_test.sh - crypto.getRandomValues must be REAL entropy, not a
# fixed sequence. Hermetic: no window, no renderer, no network.
#
# WHY THIS EXISTS (2026-11-02)
# ---------------------------
# getRandomValues filled its buffer with rand()&0xFF and NOTHING in the worker
# called srand(), so glibc seeded from 1 and EVERY worker process returned the
# same bytes. Measured before the fix, three separate processes:
#
#     67c6697351ff4aec   uuid=29cdbaab-f2fb-4346-bcc2-54f81be8e78d
#     67c6697351ff4aec   uuid=29cdbaab-f2fb-4346-bcc2-54f81be8e78d
#     67c6697351ff4aec   uuid=29cdbaab-f2fb-4346-bcc2-54f81be8e78d
#
# That is a correctness bug in a CSPRNG-shaped API and a loud fingerprint: a
# browser whose "random" values repeat every session is self-identifying.
#
# The fix reads the kernel CSPRNG (getrandom(2)) per call. The assertion is
# therefore CROSS-PROCESS: two independent worker runs must not agree. A
# single-process test cannot see this class of bug at all, which is precisely
# why it survived - so this gate runs the worker twice.
#
# Collision probability for two 8-byte draws is ~2^-64; a failure here means
# the values are not random, not that they were unlucky.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }

WORK="$(mktemp -d /tmp/nb_rng.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

printf '/*nbjs-script-boundary*/\nvar __nb_rng_fixture = 1;\n' > "$WORK/page.js"
printf 'N|#document|||0||0|\nD\nN|html|||0||0|\nU\n' > "$WORK/dom.txt"
cat > "$WORK/probe.js" <<'PROBE'
(function(){var a=new Uint8Array(8);try{ crypto.getRandomValues(a); }catch(e){ return 'RANDRESULT THROW '+e.message; }var s=''; for(var i=0;i<8;i++){ s+=('0'+a[i].toString(16)).slice(-2); }var u; try{ u=crypto.randomUUID(); }catch(e){ u='THROW:'+e.message; }return 'RANDRESULT '+s+' '+u;})();
PROBE

run_once() {
    python3 "$HERE/nb_probe.py" \
        --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
        --href https://example.invalid/ --title rng:fixture \
        --probe "$WORK/probe.js" --console "$WORK/console.$1.txt" \
        --load-timeout 30 --eval-timeout 30 > "$WORK/out.$1.json" 2> "$WORK/err.$1.log"
    sed -n 's/^=>RANDRESULT //p' "$WORK/console.$1.txt" | head -1
}

A="$(run_once a)"
B="$(run_once b)"
echo "run a: ${A:-none}"
echo "run b: ${B:-none}"

[ -z "$A" ] && { echo "FAIL: run a produced no result"; sed -n '1,20p' "$WORK/err.a.log"; FAIL=1; }
[ -z "$B" ] && { echo "FAIL: run b produced no result"; sed -n '1,20p' "$WORK/err.b.log"; FAIL=1; }

case "$A" in THROW*) echo "FAIL: getRandomValues threw: $A"; FAIL=1;; esac

# 1. the actual bug: cross-process determinism
if [ -n "$A" ] && [ "$A" = "$B" ]; then
    echo "FAIL: two separate worker processes returned identical randoms"
    echo "      ('$A') - getRandomValues is not drawing real entropy"
    FAIL=1
fi

# 2. shape: an 8-byte hex draw and a v4 UUID (version nibble 4, variant 8/9/a/b)
# grep -E, not awk: POSIX awk does not support {n} interval quantifiers, and an
# interval regex that silently never matches would fail a correct UUID.
BYTES="$(printf '%s' "$A" | cut -d' ' -f1)"
UUID="$(printf '%s' "$A" | cut -d' ' -f2)"
printf '%s' "$BYTES" | grep -Eq '^[0-9a-f]{16}$' \
    || { echo "FAIL: random bytes are not 8 lowercase hex bytes: '$BYTES'"; FAIL=1; }
printf '%s' "$UUID" | grep -Eq '^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$' \
    || { echo "FAIL: randomUUID is not a v4 UUID: '$UUID'"; FAIL=1; }

if [ "$FAIL" -eq 0 ]; then
    echo "nb_crypto_random_test.sh PASS (entropy differs across processes; uuid is v4)"
else
    echo "nb_crypto_random_test.sh FAIL"
fi
exit $FAIL
