#!/bin/bash
# loop_test.sh - proves the router LOOP (^.hai-server/button.sh + router.pal under prisc+x) on a SANDBOX copy, never the live server dir. PASS/FAIL lines; exit 0 = all pass.
# Checks: refuses to start on the live dir without the owner's OK; starts on a sandbox; a message sent AFTER start is delivered by the loop (no manual router call);
# the loop is steady (a second message also arrives, the ledger has exactly 2 route rows, nothing is routed twice); stop kills it by pid and leaves nothing running.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; HOUSE="$(cd "$HERE/../../.." && pwd)"
OPS="$HOUSE/&.widgits/_shared-lib/ops/+x"; BTN="$HOUSE/^.hai-server/button.sh"
T="$(mktemp -d)"; SD="$T/server"; mkdir -p "$SD" "$T/pals/ent_a" "$T/pals/ent_b"; fail=0
ck() { if [ "$2" = ok ]; then echo "PASS|$1"; else echo "FAIL|$1|$3"; fail=1; fi; }
printf 'PAL | name | ent_a\n' > "$T/pals/ent_a/pal.pdl"; printf 'PAL | name | ent_b\n' > "$T/pals/ent_b/pal.pdl"
"$OPS/phone_ensure_op.+x" "$HOUSE" --apply --pals-root "$T/pals" --index "$SD/phones.index" --report /dev/null >/dev/null 2>&1
A="$T/pals/ent_a/inventory/zz.phone"; B="$T/pals/ent_b/inventory/zz.phone"
NB="$(grep -m1 number "$B/phone.pdl" | awk -F'|' '{gsub(/ /,"",$3); print $3}')"
lines() { grep -vc '^#' "$1" 2>/dev/null || true; }
# 1. the live dir is protected
out="$(sh "$BTN" start 2>&1)"; rc=$?
[ "$rc" = 1 ] && echo "$out" | grep -q 'owner' && ck refuses-live-without-ok ok || ck refuses-live-without-ok bad "rc=$rc out=$out"
# 2. start on the sandbox
HAI_SERVER_DIR="$SD" sh "$BTN" start >"$T/start.out" 2>&1; sleep 1
PID="$(cat "$SD/router.pid" 2>/dev/null)"
[ -n "$PID" ] && kill -0 "$PID" 2>/dev/null && ck loop-starts ok || ck loop-starts bad "no live pid ($PID); start said: $(tail -2 "$T/start.out" 2>/dev/null); log: $(tail -2 "$SD/router.log" 2>/dev/null)"
# 3. a message sent after start is delivered by the loop alone
"$OPS/phone_send_op.+x" "$A" "$NB" say L1 "from the loop test" || true
for i in 1 2 3 4 5 6 7 8; do [ "$(lines "$B/inbox.txt")" -ge 1 ] && break; sleep 1; done
[ "$(lines "$B/inbox.txt")" = 1 ] && grep -q '|L1|from the loop test' "$B/inbox.txt" && ck loop-delivers ok || ck loop-delivers bad "B inbox has $(lines "$B/inbox.txt") lines after 8 s"
# 4. steady: a second message also arrives and nothing is routed twice
"$OPS/phone_send_op.+x" "$A" "$NB" say L2 "second" || true
for i in 1 2 3 4 5 6 7 8; do [ "$(lines "$B/inbox.txt")" -ge 2 ] && break; sleep 1; done
sleep 3
[ "$(lines "$B/inbox.txt")" = 2 ] && [ "$(grep -c '|route|' "$SD/ledger.txt" 2>/dev/null)" = 2 ] && ck loop-steady-no-duplicates ok || ck loop-steady-no-duplicates bad "inbox $(lines "$B/inbox.txt") lines, ledger $(grep -c '|route|' "$SD/ledger.txt" 2>/dev/null) route rows"
# 5. stop by pid, nothing left running
HAI_SERVER_DIR="$SD" sh "$BTN" stop >/dev/null 2>&1; sleep 1
! kill -0 "$PID" 2>/dev/null && [ ! -f "$SD/router.pid" ] && ck stop-kills-by-pid ok || ck stop-kills-by-pid bad "pid $PID still alive or pidfile left"
[ "$(HAI_SERVER_DIR="$SD" sh "$BTN" status 2>&1)" = "router not running" ] && ck status-after-stop ok || ck status-after-stop bad "status says running"
rm -rf "$T"
echo "VERDICT|$([ "$fail" = 0 ] && echo PASS || echo FAIL)"; exit "$fail"
