#!/bin/bash
set -u
HARNESS="$(cd "$(dirname "$0")/.." && pwd)"
HOUSE="$(cd "$HARNESS/../.." && pwd)"
PM="$HOUSE/&.widgits/proc-monitor"
OPS="$PM/ops/+x"
PROOF="$HARNESS/proof/harness-$(date +%Y%m%d-%H%M%S)"
WD="$HARNESS/workdir/run-$$"
RUNTIME="$WD/runtime"
mkdir -p "$PROOF" "$RUNTIME" "$WD/fake_sess/pieces/system" "$WD/fake_sess2/pieces/system"
FAIL=0
pass(){ echo "PASS: $1"; }
fail(){ echo "FAIL: $1"; FAIL=1; }
cleanup() {
  [ -n "${PID1:-}" ] && kill -9 "$PID1" 2>/dev/null || true
  [ -n "${PID2:-}" ] && kill -9 "$PID2" 2>/dev/null || true
  rm -rf "$WD" 2>/dev/null || true
}
trap cleanup EXIT
echo "=== proc-monitor demo ==="
sleep 60 &
PID1=$!
sleep 60 &
PID2=$!
"$OPS/runtime_register.+x" "$RUNTIME" "$PID1" mutaclysm "$WD/fake_sess" app 1 "mutaclysm-demo"
"$OPS/runtime_register.+x" "$RUNTIME" "$PID2" agy-editor "$WD/fake_sess2" app 0 "editor-headless"
"$OPS/runtime_list.+x" "$RUNTIME" | tee "$PROOF/02_list.txt"
grep -q "pid=$PID1" "$PROOF/02_list.txt" && grep -q "pid=$PID2" "$PROOF/02_list.txt" && pass "list shows both pids" || fail "list"
grep -q "alive=1" "$PROOF/02_list.txt" && pass "alive" || fail "alive"
"$OPS/runtime_focus.+x" "$RUNTIME" "$PID1" | tee "$PROOF/03_focus.txt"
cp "$RUNTIME/current_focus.txt" "$PROOF/03_current_focus.txt"
grep -q "pid=$PID1" "$RUNTIME/current_focus.txt" && pass "FOCUS mutaclysm" || fail "focus"
"$OPS/runtime_kill.+x" "$RUNTIME" "$PID1" --soft | tee "$PROOF/04_soft.txt"
[ -f "$WD/fake_sess/pieces/system/quit_request.txt" ] && pass "SOFT quit_request" || fail "soft"
"$OPS/runtime_kill.+x" "$RUNTIME" "$PID2" | tee "$PROOF/05_kill.txt"
kill -9 "$PID2" 2>/dev/null || true
PID2=""
"$OPS/runtime_list.+x" "$RUNTIME" --gc | tee "$PROOF/06_gc.txt"
grep -q "editor-headless" "$PROOF/06_gc.txt" && fail "editor still listed" || pass "editor gone after kill"
kill -9 "$PID1" 2>/dev/null || true
PID1=""
"$OPS/runtime_list.+x" "$RUNTIME" --gc | tee "$PROOF/07_final.txt"
echo "Proof: $PROOF"
[ "$FAIL" -eq 0 ] && { echo "=== ALL PASS — proc-monitor ==="; exit 0; }
echo "=== FAILED ==="; exit 1
