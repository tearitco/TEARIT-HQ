#!/bin/bash
# test_proc_ledger_add.sh - proves proc_ledger_add.sh with the REAL reaper (kh_proc_reap_all from kh_proc_registry.h), on a scratch house, with throwaway `sleep` processes.
# A process registered by the helper must be killed by the reaper. The old hand-written "pid pid 0 0 name" line is reaped too (starttime 0 = unknown, not checked): that was a wrong theory for why
# the hotbar survived restarts (the real cause is in $.crypts/close_on_restart.pdl). The PID-reuse GUARD is tested for real: a line with a WRONG non-zero starttime must be skipped.
# PASS/FAIL lines + VERDICT; exit 0 = PASS. Never touches the real ledger or any real process.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; SHARED="$(cd "$HERE/.." && pwd)"; HELPER="$SHARED/ops/proc_ledger_add.sh"
T="$(mktemp -d)"; fail=0; PA=""; PB=""
cleanup() { [ -n "$PA" ] && kill "$PA" 2>/dev/null; [ -n "$PB" ] && kill "$PB" 2>/dev/null; rm -rf "$T"; }; trap cleanup EXIT
ck() { if [ "$2" = ok ]; then echo "PASS|$1"; else echo "FAIL|$1|$3"; fail=1; fi; }
mkdir -p "$T/house/#.desktop"; L="$T/house/#.desktop/livedesk_proc_list.txt"
cat > "$T/reap.c" <<'EOC'
#define KH_PROC_REGISTRY_IMPL
#include "kh_proc_registry.h"
#include <stdio.h>
int main(int argc, char **argv) { (void)argc; printf("signalled=%d\n", kh_proc_reap_all(argv[1], 300, 0)); return 0; }
EOC
gcc -std=gnu11 -Wall -o "$T/reap" "$T/reap.c" -I "$SHARED" 2>"$T/cc.txt" || { echo "FAIL|reaper-harness-builds|$(head -3 "$T/cc.txt")"; exit 1; }
setsid sleep 300 & PA=$!; setsid sleep 300 & PB=$!; sleep 0.3          # own process groups, like the real launchers (setsid)
sh "$HELPER" "$T/house" "$PA" "test-new"                                  # the fix
printf '%s %s 0 1 test-wrong-start\n' "$PB" "$PB" >> "$L"                # a recycled-pid lookalike: non-zero starttime that does not match
LINE="$(grep ' test-new$' "$L")"
echo "$LINE" | awk '{ok = ($1 ~ /^[0-9]+$/ && $2 ~ /^[0-9]+$/ && $3 == 0 && $4 ~ /^[0-9]+$/ && $4 > 0 && NF == 5)} END {exit !ok}' && ck helper-writes-5-real-fields ok || ck helper-writes-5-real-fields bad "line: $LINE"
S="$(sed 's/^.*) //' /proc/$PA/stat | awk '{print $20}')"; [ "$(echo "$LINE" | awk '{print $4}')" = "$S" ] && ck starttime-matches-proc ok || ck starttime-matches-proc bad "ledger $(echo "$LINE" | awk '{print $4}') vs proc $S"
kill -0 "$PA" 2>/dev/null && kill -0 "$PB" 2>/dev/null && ck both-alive-before-reap ok || ck both-alive-before-reap bad "setup failed"
"$T/reap" "$T/house" > "$T/out.txt" 2>&1; sleep 0.8
! kill -0 "$PA" 2>/dev/null && ck helper-registered-process-is-reaped ok || ck helper-registered-process-is-reaped bad "still alive after reap"
kill -0 "$PB" 2>/dev/null && ck wrong-nonzero-starttime-is-skipped-pid-reuse-guard ok || ck wrong-nonzero-starttime-is-skipped-pid-reuse-guard bad "a mismatching starttime WAS reaped: the guard is gone"
sh "$HELPER" "$T/house" 999999 "ghost-pid"; ! grep -q ghost-pid "$L" && ck dead-pid-writes-nothing ok || ck dead-pid-writes-nothing bad "wrote a line for a dead pid"
sh "$HELPER" "" "$PB" x; sh "$HELPER" "$T/house" "12x" y; [ "$(wc -l < "$L")" -le 2 ] && ck bad-arguments-ignored ok || ck bad-arguments-ignored bad "lines: $(wc -l < "$L")"
echo "VERDICT|$([ "$fail" = 0 ] && echo PASS || echo FAIL)"; exit "$fail"
