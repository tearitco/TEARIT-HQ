#!/bin/bash
# verify.sh - the deterministic SCORER for quest Q009 (phone.send + server.route). Nothing is scored by anyone saying "done": this builds a SANDBOX (two fake entities with phones
# made by the real phone_ensure_op), runs the real ops against it, and prints PASS|FAIL lines. The live phones are never written (their checksums are compared before/after).
# Output (stable): one `PASS|<check>` or `FAIL|<check>|<why>` per check, then `VERDICT|<PASS|FAIL>|checks=N|failed=N`. Exit 0 = PASS, 1 = FAIL, 2 = usage / ops missing.
# Usage: verify.sh            run all checks
#        verify.sh --selftest prove the scorer: a deliberately BROKEN router (routes nothing) must make it FAIL, the real one must PASS
# ROUTER=<path> overrides the router op (that is how --selftest injects the broken one).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$HERE/../../.." && pwd)"
OPS="$HOUSE/&.widgits/_shared-lib/ops/+x"
SEND="$OPS/phone_send_op.+x"; ENSURE="$OPS/phone_ensure_op.+x"; ROUTER="${ROUTER:-$OPS/server_route_op.+x}"
for b in "$SEND" "$ENSURE" "$ROUTER"; do [ -x "$b" ] || { echo "missing $b (sh '&.widgits/_shared-lib/ops/build_phone_ensure_op.sh')" >&2; exit 2; }; done

run_checks() {
    local T N=0 F=0 A B NA NB SD out
    T="$(mktemp -d)"; SD="$T/server"; mkdir -p "$SD" "$T/pals/ent_a" "$T/pals/ent_b"
    printf 'PAL | name | ent_a\n' > "$T/pals/ent_a/pal.pdl"; printf 'PAL | name | ent_b\n' > "$T/pals/ent_b/pal.pdl"
    "$ENSURE" "$HOUSE" --apply --pals-root "$T/pals" --index "$SD/phones.index" --report /dev/null >/dev/null 2>&1
    A="$T/pals/ent_a/inventory/zz.phone"; B="$T/pals/ent_b/inventory/zz.phone"
    NA="$(grep -m1 'number' "$A/phone.pdl" | awk -F'|' '{gsub(/ /,"",$3); print $3}')"; NB="$(grep -m1 'number' "$B/phone.pdl" | awk -F'|' '{gsub(/ /,"",$3); print $3}')"
    local live_before live_after
    live_before="$(find "$HOUSE/xyzfs/users" -path '*/inventory/zz.phone/*' -type f 2>/dev/null | sort | head -40 | xargs -r sha256sum 2>/dev/null | sha256sum | cut -c1-16)"
    ck() { N=$((N+1)); if [ "$2" = ok ]; then echo "PASS|$1"; else F=$((F+1)); echo "FAIL|$1|$3"; fi; }
    lines() { grep -vc '^#' "$1" 2>/dev/null || true; }
    [ -n "$NA" ] && [ -n "$NB" ] && [ "$NA" != "$NB" ] && ck sandbox-phones ok || ck sandbox-phones bad "numbers a=$NA b=$NB"

    # 1. A -> B, delivered once, in both histories, one ledger row
    "$SEND" "$A" "$NB" say t1 "hello b" || true
    out="$("$ROUTER" "$SD" 2>&1)"
    [ "$(lines "$B/inbox.txt")" = 1 ] && grep -q '|say|t1|hello b' "$B/inbox.txt" && ck delivered-to-inbox ok || ck delivered-to-inbox bad "B inbox: $(lines "$B/inbox.txt") lines ($out)"
    grep -q '|say|t1|hello b' "$A/history.txt" && grep -q '|say|t1|hello b' "$B/history.txt" && ck in-both-histories ok || ck in-both-histories bad "history missing the line"
    [ "$(grep -c '|route|' "$SD/ledger.txt" 2>/dev/null)" = 1 ] && ck one-ledger-row ok || ck one-ledger-row bad "ledger: $(cat "$SD/ledger.txt" 2>/dev/null | wc -l) rows"
    [ "$(lines "$A/inbox.txt")" = 0 ] && ck sender-inbox-untouched ok || ck sender-inbox-untouched bad "A inbox changed"
    # 2. second pass: the cursor means nothing is routed twice
    "$ROUTER" "$SD" >/dev/null 2>&1
    [ "$(lines "$B/inbox.txt")" = 1 ] && [ "$(grep -c '|route|' "$SD/ledger.txt" 2>/dev/null)" = 1 ] && ck cursor-no-redelivery ok || ck cursor-no-redelivery bad "re-routed on the second pass"
    # 3. unknown number -> fail message back to the sender, never a crash
    "$SEND" "$A" "999-9999-9999" say t2 "anyone there" || true
    "$ROUTER" "$SD" >/dev/null 2>&1; local rc=$?
    [ "$rc" = 0 ] && grep -q '|server|' "$A/inbox.txt" 2>/dev/null && grep -q '|fail|t2|' "$A/inbox.txt" && ck unknown-number-fails-back ok || ck unknown-number-fails-back bad "rc=$rc, A inbox: $(tail -1 "$A/inbox.txt" 2>/dev/null)"
    # 4. spoofed sender number is rejected and not delivered
    printf '%s|%s|%s|say|t3|i am b\n' "$(date +%s)000" "$NB" "$NA" >> "$A/outbox.txt"
    "$ROUTER" "$SD" >/dev/null 2>&1
    grep -q 'spoofed-from' "$SD/ledger.txt" && ! grep -q '|t3|i am b' "$A/inbox.txt" && ck spoof-rejected ok || ck spoof-rejected bad "spoof not rejected or delivered"
    # 5. send-side validation: each refusal exits 1 and writes nothing
    local before after
    before="$(wc -c < "$A/outbox.txt")"
    "$SEND" "$A" "$NB" nonsense r "x" 2>/dev/null; local r1=$?
    "$SEND" "$A" "$NB" say r "has|pipe" 2>/dev/null; local r2=$?
    "$SEND" "$A" "12" say r "bad number" 2>/dev/null; local r3=$?
    "$SEND" "$A" "$NB" say r "" 2>/dev/null; local r4=$?
    after="$(wc -c < "$A/outbox.txt")"
    [ "$r1$r2$r3$r4" = 1111 ] && [ "$before" = "$after" ] && ck send-refuses-bad-input ok || ck send-refuses-bad-input bad "exit codes $r1$r2$r3$r4, outbox bytes $before -> $after"
    # 6. rate cap from tunables.conf: cap 2 -> only 2 of 4 move, rest wait; edit the file (no recompile) and the rest move
    local base_ms=2000000000000 n0
    for i in 1 2 3 4; do "$SEND" "$A" "$NB" say "c$i" "burst $i" || true; done
    printf 'route_max_msgs_per_min=2\n' > "$SD/tunables.conf"; n0="$(lines "$B/inbox.txt")"
    "$ROUTER" "$SD" --now-ms "$base_ms" >/dev/null 2>&1
    [ "$(( $(lines "$B/inbox.txt") - n0 ))" = 2 ] && grep -q 'throttled' "$SD/observations.log" && ck rate-cap-throttles ok || ck rate-cap-throttles bad "moved $(( $(lines "$B/inbox.txt") - n0 )) of 4 with cap 2"
    printf 'route_max_msgs_per_min=100\n' > "$SD/tunables.conf"
    "$ROUTER" "$SD" --now-ms "$((base_ms+1000))" >/dev/null 2>&1
    [ "$(( $(lines "$B/inbox.txt") - n0 ))" = 4 ] && ck tunable-edit-needs-no-recompile ok || ck tunable-edit-needs-no-recompile bad "after raising the cap B got $(( $(lines "$B/inbox.txt") - n0 )) of 4"
    # 7. outbox rotated (file smaller than the cursor): the router resyncs and still delivers the new message
    : > "$A/outbox.txt"; "$SEND" "$A" "$NB" say r1 "after rotation" || true; n0="$(lines "$B/inbox.txt")"
    "$ROUTER" "$SD" --now-ms "$((base_ms+200000))" >/dev/null 2>&1
    [ "$(( $(lines "$B/inbox.txt") - n0 ))" = 1 ] && grep -q '|resync|' "$SD/ledger.txt" && ck rotation-resync ok || ck rotation-resync bad "message after rotation not delivered / no resync row"
    # 8. every decision left an observation row (the dataset)
    [ "$(wc -l < "$SD/observations.log" 2>/dev/null)" -ge 8 ] && grep -q '|routed$' "$SD/observations.log" && ck observations-logged ok || ck observations-logged bad "observations.log has $(wc -l < "$SD/observations.log" 2>/dev/null) rows"
    # 9. live phones untouched
    live_after="$(find "$HOUSE/xyzfs/users" -path '*/inventory/zz.phone/*' -type f 2>/dev/null | sort | head -40 | xargs -r sha256sum 2>/dev/null | sha256sum | cut -c1-16)"
    [ "$live_before" = "$live_after" ] && ck live-phones-untouched ok || ck live-phones-untouched bad "live phone files changed during the test"
    rm -rf "$T"
    echo "VERDICT|$([ "$F" = 0 ] && echo PASS || echo FAIL)|checks=$N|failed=$F"
    [ "$F" = 0 ]
}

case "${1:-}" in
    --selftest)
        BROKEN="$(mktemp)"; printf '#!/bin/sh\nexit 0\n' > "$BROKEN"; chmod +x "$BROKEN"
        ROUTER="$BROKEN" run_checks >/dev/null 2>&1; rcb=$?
        rm -f "$BROKEN"
        run_checks >/dev/null 2>&1; rcg=$?
        if [ "$rcb" = 1 ] && [ "$rcg" = 0 ]; then echo "SELFTEST PASS (a router that routes nothing fails; the real router passes)"; exit 0; fi
        echo "SELFTEST FAIL (broken router rc=$rcb want 1, real router rc=$rcg want 0)"; exit 1 ;;
    "") run_checks 2>/dev/null ;;
    *) echo "usage: verify.sh [--selftest]" >&2; exit 2 ;;
esac
