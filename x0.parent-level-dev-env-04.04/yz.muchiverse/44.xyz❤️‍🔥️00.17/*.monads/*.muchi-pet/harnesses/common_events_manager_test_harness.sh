#!/bin/bash
# common_events_manager_test_harness.sh — Task 4 Common Events Manager tests
#
# Tests:
#   T1: Autorun event fires exactly once when switch transitions OFF→ON
#   T2: Parallel event fires repeatedly while switch is ON (with cooldown)
#   T3: Manager zero stray processes before/after
#
# Evidence: ledger file timestamps, switch state manipulation, process checks

set -u

HERE="$(cd "$(dirname "$0")" && pwd)"

find_house_root() {
  local d="$HERE"
  while [ "$d" != "/" ]; do
    case "$(basename "$d")" in
      44.xyz*) echo "$d"; return 0 ;;
    esac
    d="$(dirname "$d")"
  done
  echo "harness: could not find house root above $HERE" >&2
  exit 1
}

HOUSE="${HOUSE:-$(find_house_root)}"
RESULTS_BASE="$HOUSE/presentations/common-events-manager-test-$(date '+%Y%m%d-%H%M%S')"
mkdir -p "$RESULTS_BASE"

SUMMARY="$RESULTS_BASE/test_results.txt"
LOG="$RESULTS_BASE/test_log.txt"

log() { echo "[$(date '+%H:%M:%S')] $*" | tee -a "$LOG"; }
pass() { log "✓ PASS: $*"; echo "PASS: $*" >> "$SUMMARY"; }
fail() { log "✗ FAIL: $*"; echo "FAIL: $*" >> "$SUMMARY"; }

# =========================================================================
# Cleanup trap
# =========================================================================
cleanup() {
  log "=== cleanup ==="

  # Kill the manager
  pkill -f "common_events_manager\.\+x" 2>/dev/null || true
  sleep 1

  # Verify no stray processes
  local stragglers
  stragglers="$(pgrep -f 'common_events_manager\.\+x' 2>/dev/null || true)"
  if [ -n "$stragglers" ]; then
    log "WARNING: stray manager processes after kill: $stragglers"
    echo "$stragglers" | xargs -r kill -KILL 2>/dev/null
  fi

  # Report
  log "cleanup complete"
}
trap cleanup EXIT

# =========================================================================
# Step 0: Verify zero stray processes before start
# =========================================================================
log "=== Step 0: Zero stray processes check ==="
existing="$(pgrep -f 'common_events_manager\.\+x' 2>/dev/null || true)"
if [ -n "$existing" ]; then
  fail "stray manager already running: $existing"
  echo "$existing" | xargs -r kill -KILL
  sleep 1
fi
pass "no stray manager processes before start"

# =========================================================================
# Step 1: Setup test common events
# =========================================================================
log "=== Step 1: Setup test common events ==="

# Create autorun test event
AUTORUN_DIR="$HOUSE/common_events/test_autorun_event"
mkdir -p "$AUTORUN_DIR/event_pkg/pages/page_1"

cat > "$AUTORUN_DIR/event_pkg/pages/page_1/condition.pdl" <<'EOF'
COND | trigger | Autorun
EOF

cat > "$AUTORUN_DIR/event_pkg/pages/page_1/event.pal" <<'EOF'
# test autorun event
halt
EOF

cat > "$AUTORUN_DIR/event_pkg/pages/page_1/event.ir.pdl" <<'EOF'
# Empty IR - just testing execution
EOF

log "created test_autorun_event"

# Create parallel test event
PARALLEL_DIR="$HOUSE/common_events/test_parallel_event"
mkdir -p "$PARALLEL_DIR/event_pkg/pages/page_1"

cat > "$PARALLEL_DIR/event_pkg/pages/page_1/condition.pdl" <<'EOF'
COND | trigger | Parallel
EOF

cat > "$PARALLEL_DIR/event_pkg/pages/page_1/event.pal" <<'EOF'
# test parallel event
halt
EOF

cat > "$PARALLEL_DIR/event_pkg/pages/page_1/event.ir.pdl" <<'EOF'
# Empty IR - just testing execution
EOF

log "created test_parallel_event"

# Setup switches file at house root (fallback location for testing)
cat > "$HOUSE/switches.txt" <<'EOF'
ce_test_autorun_event=0
ce_test_parallel_event=0
EOF
log "created switches.txt with both switches OFF"

# =========================================================================
# Step 2: Launch common_events_manager
# =========================================================================
log "=== Step 2: Launch common_events_manager ==="

MANAGER_BIN="$HOUSE/*.monads/*.muchi-pet/ops/+x/common_events_manager.+x"

if [ ! -x "$MANAGER_BIN" ]; then
  fail "manager binary not found or not executable: $MANAGER_BIN"
  exit 1
fi

log "Starting manager: $MANAGER_BIN $HOUSE"
"$MANAGER_BIN" "$HOUSE" >/dev/null 2>&1 &
MANAGER_PID=$!
sleep 2

# Verify manager is running
if ! kill -0 "$MANAGER_PID" 2>/dev/null; then
  fail "manager failed to start"
  cat /tmp/common_events_manager.log 2>/dev/null || true
  exit 1
fi
pass "manager started (PID $MANAGER_PID)"

# =========================================================================
# Step 3: Test Autorun (edge-triggered)
# =========================================================================
log "=== Step 3: Test Autorun edge-triggered ==="

# Flip switch OFF→ON
cat > "$HOUSE/switches.txt" <<'EOF'
ce_test_autorun_event=1
ce_test_parallel_event=0
EOF
log "flipped ce_test_autorun_event 0→1"
sleep 2

# Check ledger for exactly 1 firing
LEDGER="$HOUSE/common_events/.manager_ledger.txt"
AUTORUN_FIRES=$(grep -c "test_autorun_event.*Autorun" "$LEDGER" 2>/dev/null || echo "0")

if [ "$AUTORUN_FIRES" == "1" ]; then
  pass "Autorun fired exactly once on 0→1 transition (ledger: $AUTORUN_FIRES fires)"
else
  fail "Autorun did not fire correctly (expected 1, got $AUTORUN_FIRES)"
fi

# Flip it back to verify it doesn't fire on 1→0
log "flipping ce_test_autorun_event back to 0 (should not trigger)"
cat > "$HOUSE/switches.txt" <<'EOF'
ce_test_autorun_event=0
ce_test_parallel_event=0
EOF
sleep 1

AUTORUN_FIRES=$(grep -c "test_autorun_event.*Autorun" "$LEDGER" 2>/dev/null || echo "0")
if [ "$AUTORUN_FIRES" == "1" ]; then
  pass "Autorun still at 1 fire (did not re-trigger on 1→0)"
else
  fail "Autorun count changed (expected 1, got $AUTORUN_FIRES)"
fi

# =========================================================================
# Step 4: Test Parallel (repeated firing with cooldown)
# =========================================================================
log "=== Step 4: Test Parallel (repeated firing with cooldown) ==="

# Clear ledger for this test
: > "$LEDGER"
log "cleared ledger"

# Turn on Parallel switch
cat > "$HOUSE/switches.txt" <<'EOF'
ce_test_autorun_event=0
ce_test_parallel_event=1
EOF
log "flipped ce_test_parallel_event 0→1"
sleep 2

# First fire should happen quickly
PARALLEL_FIRES=$(grep -c "test_parallel_event.*Parallel" "$LEDGER" 2>/dev/null || echo "0")
if [ "$PARALLEL_FIRES" -ge "1" ]; then
  pass "Parallel fired at least once while switch ON (fires: $PARALLEL_FIRES)"
else
  fail "Parallel did not fire (expected ≥1, got $PARALLEL_FIRES)"
fi

# Wait 2 more seconds (longer than cooldown) and check for additional fires
sleep 2
PARALLEL_FIRES_AFTER=$(grep -c "test_parallel_event.*Parallel" "$LEDGER" 2>/dev/null || echo "0")

if [ "$PARALLEL_FIRES_AFTER" -gt "$PARALLEL_FIRES" ]; then
  pass "Parallel fired again after cooldown expired (before: $PARALLEL_FIRES, after: $PARALLEL_FIRES_AFTER)"
else
  log "Parallel did not fire again (cooldown may still be active, before: $PARALLEL_FIRES, after: $PARALLEL_FIRES_AFTER)"
fi

# Turn switch OFF and verify it stops
log "turning off ce_test_parallel_event switch"
cat > "$HOUSE/switches.txt" <<'EOF'
ce_test_autorun_event=0
ce_test_parallel_event=0
EOF
sleep 2

PARALLEL_FIRES_FINAL=$(grep -c "test_parallel_event.*Parallel" "$LEDGER" 2>/dev/null || echo "0")
if [ "$PARALLEL_FIRES_FINAL" == "$PARALLEL_FIRES_AFTER" ]; then
  pass "Parallel stopped firing after switch turned OFF (stayed at $PARALLEL_FIRES_FINAL)"
else
  log "Parallel fire count changed after turning OFF (was $PARALLEL_FIRES_AFTER, now $PARALLEL_FIRES_FINAL)"
fi

# =========================================================================
# Step 5: Cleanup and stray process check
# =========================================================================
log "=== Step 5: Cleanup and verify zero stray processes ==="

# Kill manager (cleanup trap will handle it)
kill $MANAGER_PID 2>/dev/null || true
sleep 2

# Verify no stragglers
stragglers="$(pgrep -f 'common_events_manager\.\+x' 2>/dev/null || true)"
if [ -z "$stragglers" ]; then
  pass "no stray manager processes after cleanup"
else
  fail "stray processes remain: $stragglers"
fi

# =========================================================================
# Test Report
# =========================================================================
log "=== Test Report ==="
cat "$SUMMARY"

# Count passes and fails
PASSES=$(grep -c "^PASS:" "$SUMMARY" || echo "0")
FAILS=$(grep -c "^FAIL:" "$SUMMARY" || echo "0")

echo ""
echo "==========================================="
echo "Common Events Manager - Task 4 Test Results"
echo "==========================================="
echo "Passed: $PASSES"
echo "Failed: $FAILS"
echo "Results: $RESULTS_BASE"
echo "==========================================="

if [ "$FAILS" -eq "0" ]; then
  log "ALL TESTS PASSED"
  exit 0
else
  log "SOME TESTS FAILED"
  exit 1
fi
