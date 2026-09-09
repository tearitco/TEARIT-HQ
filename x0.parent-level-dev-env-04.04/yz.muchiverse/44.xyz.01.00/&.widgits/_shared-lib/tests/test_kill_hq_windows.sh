#!/bin/sh
# test_kill_hq_windows.sh — PROC-LIFECYCLE-CONSOLIDATE-REGISTRIES.md §2.
# Verifies kill_hq_windows.sh reads the proc-ledger
# (#.desktop/livedesk_proc_list.txt), kills by process GROUP, tolerates
# a dead-PID row and a legacy 4-field row, and does NOT prune the ledger.
# Desktop-safe: every process it starts is its own `setsid sleep` in /tmp.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
# tests/ -> _shared-lib/ -> &.widgits/ -> 44.xyz.01.00/
HOUSEBASE="$(cd "$HERE/../../.." && pwd)"
KILL_SH="$(echo "$HOUSEBASE"/*.monads/*.livedesk-taskbar/ops/kill_hq_windows.sh)"
[ -f "$KILL_SH" ] || { echo "FAIL: kill_hq_windows.sh not found ($KILL_SH)"; exit 2; }

fails=0
ck(){ if [ "$1" = "$2" ]; then echo "  ok   $3"; else echo "  FAIL $3 (got '$1' want '$2')"; fails=$((fails+1)); fi; }

HOUSE="$(mktemp -d /tmp/tkhw_XXXXXX)"
mkdir -p "$HOUSE/#.desktop"
LEDGER="$HOUSE/#.desktop/livedesk_proc_list.txt"

# (a) live group-leader, 5-field row
setsid sleep 300 & A=$!
# (b) dead PID, 5-field row
setsid sleep 300 & B=$!; kill -KILL "$B" 2>/dev/null; wait "$B" 2>/dev/null
# (c) live group-leader, LEGACY 4-field row (pid pgid starttime name)
setsid sleep 300 & C=$!

sleep 0.2
{
  echo "$A $A 999 0 tb-launch"
  echo "$B $B 999 0 tb-launch"
  echo "$C $C 0 legacy-4field"
} > "$LEDGER"
ledger_before="$(cat "$LEDGER")"

sh "$KILL_SH" "$HOUSE" >/dev/null 2>&1
sleep 1.5

kill -0 "$A" 2>/dev/null && a_alive=1 || a_alive=0
kill -0 "$C" 2>/dev/null && c_alive=1 || c_alive=0
ck "$a_alive" 0 "live 5-field group-leader reaped"
ck "$c_alive" 0 "live legacy 4-field group-leader reaped"
ck "$([ -f "$LEDGER" ] && echo 1 || echo 0)" 1 "ledger file still present (not consumed)"
ck "$(cat "$LEDGER")" "$ledger_before" "ledger NOT pruned by the emergency script"

# cleanup any stragglers
for p in "$A" "$C"; do kill -KILL "$p" 2>/dev/null; wait "$p" 2>/dev/null; done
rm -rf "$HOUSE"

if [ "$fails" -eq 0 ]; then echo "ALL PASS"; exit 0; else echo "$fails FAILURE(S)"; exit 1; fi
