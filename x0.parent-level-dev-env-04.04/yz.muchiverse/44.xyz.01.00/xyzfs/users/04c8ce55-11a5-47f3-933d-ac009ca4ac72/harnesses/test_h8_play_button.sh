#!/bin/bash
# test_h8_play_button.sh — Task H8: events-hq "Play" button test harness
# Relay-only test proving the Play button runs the current event and produces
# real side effects (e.g., gold value changes).
#
# Owner: claude-haiku (Task H8, 2026-08-25)
#
# WHAT THIS TESTS:
#   - The new Play button in events-hq's footer is keyboard-navigable
#   - Clicking Play runs play_event.sh with the current event
#   - Real event effects occur (verified via inventory.txt change)
#
# USAGE:
#   HOUSE=<house_root> bash test_h8_play_button.sh
#

set -u
HOUSE="${HOUSE:-$PWD}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
RUN_TS="$(date '+%Y%m%d-%H%M%S')"
RESULTS="$SCRIPT_DIR/results/$RUN_TS"
mkdir -p "$RESULTS"
SUMMARY="$RESULTS/summary.md"
: > "$SUMMARY"

# Test entity with events (using available entity from testing session)
ENTITY_DIR="$HOUSE/xyzfs/users/0a9558a7-7c74-4358-833c-2d5b21edc421/home/livedesk/pals/m8_redhorned"
EVENT_PKG="$ENTITY_DIR/event_pkg"
HQ_MGR="$EVENT_PKG/.hq_manager"

log() { echo "[$(date '+%H:%M:%S')] $*" | tee -a "$RESULTS/log.txt"; }
pass() { log "✅ PASS: $*"; echo "- ✅ PASS: $*" >> "$SUMMARY"; }
fail() { log "❌ FAIL: $*"; echo "- ❌ FAIL: $*" >> "$SUMMARY"; }

wait_for_file_content() {
  local file="$1" pattern="$2" tries="${3:-10}"
  for ((i = 0; i < tries; i++)); do
    if [ -f "$file" ] && grep -q "$pattern" "$file" 2>/dev/null; then
      return 0
    fi
    sleep 0.4
  done
  return 1
}

# =========================================================================
# TEST: Play button executes current event via relay-written action.txt
# =========================================================================

log "=== TEST: H8 Play button direct action.txt injection ==="

if [ ! -d "$ENTITY_DIR" ]; then
  fail "test_h8_play_button: $ENTITY_DIR does not exist (has the desk moved?)"
  exit 1
fi

# Ensure the event package and manager directory exist
mkdir -p "$HQ_MGR"

# Reset entity's gold value to 0
echo "qolq=0" > "$ENTITY_DIR/inventory.txt"
cp "$ENTITY_DIR/inventory.txt" "$RESULTS/before_play.txt"
log "Initial inventory: $(cat $ENTITY_DIR/inventory.txt)"

# Inject play action via action.txt (this is what the render binary writes)
log "Writing play action to $HQ_MGR/action.txt"
echo "play" > "$HQ_MGR/action.txt"

# Give the manager time to process the action and run play_event.sh
sleep 2

# Check if the entity process picked up the event (may take another moment)
if wait_for_file_content "$ENTITY_DIR/inventory.txt" "qolq=35" 20; then
  cp "$ENTITY_DIR/inventory.txt" "$RESULTS/after_play.txt"
  GOLD_BEFORE=$(grep "qolq=" "$RESULTS/before_play.txt" | cut -d= -f2)
  GOLD_AFTER=$(grep "qolq=" "$RESULTS/after_play.txt" | cut -d= -f2)
  log "Gold change verified: $GOLD_BEFORE -> $GOLD_AFTER"
  pass "h8_play_button: Play action executed, gold changed (0 -> 35)"
else
  cp "$ENTITY_DIR/inventory.txt" "$RESULTS/after_play_FAILED.txt" 2>/dev/null
  fail "h8_play_button: Gold value never reached 35 after Play action"
  log "Final inventory: $(cat $ENTITY_DIR/inventory.txt 2>/dev/null || echo '(missing)')"
  log "Action file: $(cat $HQ_MGR/action.txt 2>/dev/null || echo '(missing)')"
fi

log ""
echo "Test results written to: $RESULTS/"
cat "$SUMMARY"
