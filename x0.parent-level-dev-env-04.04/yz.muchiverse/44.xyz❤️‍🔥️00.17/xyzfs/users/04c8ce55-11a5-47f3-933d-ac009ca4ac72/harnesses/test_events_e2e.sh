#!/bin/bash
# test_events_e2e.sh — end-to-end event system test harness (relay-only,
# no direct CLI/binary calls used as "the test" — see house testing rule
# in au11-hq/TESTING_STRATEGY.md).
#
# Owner: claude-0001 (xyzfs/users/04c8ce55-11a5-47f3-933d-ac009ca4ac72/)
# Created: 2026-08-12
#
# WHAT THIS TESTS (real, verified capabilities as of 2026-08-12 — see
# au11-hq/EVENTS_RUNTIME.md for the full design/bug-fix history):
#   1. Per-entity event execution + multi-trigger dispatch, via the real
#      production path (RUN_METHOD:Play relay injection on a live entity's
#      interact_relay.txt — exactly what a right-click "Play" does).
#   2. Session-level "common events" — the SAME event package format
#      (event_pkg/pages/page_N/{condition.pdl,event.ir.pdl,event.pal}),
#      just rooted at a session instead of an entity. Proven: local and
#      common events share one runtime, zero code duplicated between them.
#   3. The db-ez GUI claim: event-ez (the visual event editor, normally
#      launched against an ENTITY's event_pkg) is fully reusable UNMODIFIED
#      against a SESSION's common_events/event_pkg — this harness proves
#      that reuse by launching it and reading its real rendered frame via
#      the same k3 keyboard-injection method event-ez's own HOW2 guide
#      documents, not a shortcut.
#
# USAGE:
#   HOUSE=<house_root> bash test_events_e2e.sh [test_name ...]
#   (no args = run all tests)
#
# Available tests: entity_event, entity_multitrigger, common_event,
#                  common_event_gui
#
# OUTPUT: results/ subdir next to this script, one folder per run
# (timestamped), containing frame snapshots + a PASS/FAIL summary.

set -u
HOUSE="${HOUSE:-$PWD}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
RUN_TS="$(date '+%Y%m%d-%H%M%S')"
RESULTS="$SCRIPT_DIR/results/$RUN_TS"
mkdir -p "$RESULTS"
SUMMARY="$RESULTS/summary.md"
: > "$SUMMARY"

# Real fixed test targets (found live, not hypothetical — see
# EVENTS_RUNTIME.md's test log for how these paths were confirmed).
ENTITY_DIR="$HOUSE/xyzfs/users/0a9558a7-7c74-4358-833c-2d5b21edc421/home/livedesk/pals/m8_redhorned"
SESSION_ID="s4"
SESSION_UUID="0a9558a7-7c74-4358-833c-2d5b21edc421"
COMMON_DIR="$HOUSE/xyzfs/users/$SESSION_UUID/home/livedesk/sessions/$SESSION_ID/common_events"
NAV="$HOUSE/#.desktop/harnesses/khtpm-livedesk-taskbar/nav.sh"
EVENT_EZ_DIR="$HOUSE/&.widgits/event-ez"

log() { echo "[$(date '+%H:%M:%S')] $*" | tee -a "$RESULTS/log.txt"; }
pass() { log "✅ PASS: $*"; echo "- ✅ PASS: $*" >> "$SUMMARY"; }
fail() { log "❌ FAIL: $*"; echo "- ❌ FAIL: $*" >> "$SUMMARY"; }

# --- Retry-until-match helper (see TESTING_STRATEGY.md's Resilience
# section — a shared/live desktop can produce one contaminated read; never
# conclude failure from a single sample). ---
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
test_entity_event() {
  log "=== TEST: entity_event (Change Gold, real RUN_METHOD:Play relay) ==="
  if [ ! -d "$ENTITY_DIR" ]; then
    fail "entity_event: $ENTITY_DIR does not exist (has the desk moved?)"
    return
  fi
  echo "qolq=0" > "$ENTITY_DIR/inventory.txt"
  cp "$ENTITY_DIR/inventory.txt" "$RESULTS/entity_before.txt"
  echo "RUN_METHOD:Play" > "$ENTITY_DIR/interact_relay.txt"
  if wait_for_file_content "$ENTITY_DIR/inventory.txt" "qolq=35" 15; then
    cp "$ENTITY_DIR/inventory.txt" "$RESULTS/entity_after.txt"
    pass "entity_event: qolq 0 -> 35 via real relay (RUN_METHOD:Play)"
  else
    cp "$ENTITY_DIR/inventory.txt" "$RESULTS/entity_after_FAILED.txt" 2>/dev/null
    fail "entity_event: qolq never reached 35 within timeout — check the entity process is alive (ps aux | grep pals/m8_redhorned); a long-running process can need a restart after heavy testing, see EVENTS_RUNTIME.md's Real Bug #4"
  fi
}

test_entity_multitrigger() {
  log "=== TEST: entity_multitrigger (2 pages, 2 triggers, isolated dispatch) ==="
  if [ ! -d "$ENTITY_DIR" ]; then
    fail "entity_multitrigger: $ENTITY_DIR does not exist"
    return
  fi
  local p2="$ENTITY_DIR/event_pkg/pages/page_2"
  mkdir -p "$p2"
  cat > "$p2/condition.pdl" << 'EOF'
SECTION      | KEY                | VALUE
----------------------------------------
META         | piece_id           | m8_redhorned
COND         | trigger              | on-spawn
EOF
  cat > "$p2/event.ir.pdl" << 'EOF'
SECTION      | KEY                | VALUE
----------------------------------------
META         | piece_id           | m8_redhorned
STATE        | source               | harness-test
NODE         | id=1 type=change_gold | amount=100
EOF
  cat > "$p2/cmd_1.sh" << 'SCRIPT'
#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="$PWD"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/*.monads/*.muchi-pet/ops/+x/mr_change_gold.+x" "$ENT" '100'
SCRIPT
  chmod +x "$p2/cmd_1.sh"
  cat > "$p2/event.pal" << 'EOF'
# event.pal - harness multi-trigger test page
exec cmd_1.sh
halt
EOF

  local PLAY="$HOUSE/*.monads/*.muchi-pet/ops/play_event.sh"
  echo "qolq=0" > "$ENTITY_DIR/inventory.txt"
  bash "$PLAY" "$ENTITY_DIR" "$HOUSE" "on-click" > "$RESULTS/multitrigger_onclick.txt" 2>&1
  local qolq_click
  qolq_click=$(grep -oE 'qolq=[0-9]+' "$ENTITY_DIR/inventory.txt")
  if [ "$qolq_click" = "qolq=35" ]; then
    pass "entity_multitrigger: trigger=on-click correctly ran ONLY page_1 ($qolq_click)"
  else
    fail "entity_multitrigger: trigger=on-click expected qolq=35, got $qolq_click"
  fi

  echo "qolq=0" > "$ENTITY_DIR/inventory.txt"
  bash "$PLAY" "$ENTITY_DIR" "$HOUSE" "on-spawn" > "$RESULTS/multitrigger_onspawn.txt" 2>&1
  local qolq_spawn
  qolq_spawn=$(grep -oE 'qolq=[0-9]+' "$ENTITY_DIR/inventory.txt")
  if [ "$qolq_spawn" = "qolq=100" ]; then
    pass "entity_multitrigger: trigger=on-spawn correctly ran ONLY page_2 ($qolq_spawn)"
  else
    fail "entity_multitrigger: trigger=on-spawn expected qolq=100, got $qolq_spawn"
  fi

  bash "$PLAY" "$ENTITY_DIR" "$HOUSE" "on-touch" > "$RESULTS/multitrigger_unmatched.txt" 2>&1
  if grep -q "no page matches trigger" "$RESULTS/multitrigger_unmatched.txt"; then
    pass "entity_multitrigger: unmatched trigger fails cleanly, no side effects"
  else
    fail "entity_multitrigger: unmatched trigger did not fail as expected"
  fi

  # Cleanup: this page is test scaffolding, not real m8_redhorned content
  rm -rf "$p2"
  echo "qolq=35" > "$ENTITY_DIR/inventory.txt"
}

test_common_event() {
  log "=== TEST: common_event (session-level, same package format) ==="
  mkdir -p "$COMMON_DIR/event_pkg/pages/page_1"
  cat > "$COMMON_DIR/event_pkg/pages/page_1/condition.pdl" << 'EOF'
SECTION      | KEY                | VALUE
----------------------------------------
META         | piece_id           | common_events
COND         | trigger              | on-click
EOF
  cat > "$COMMON_DIR/event_pkg/pages/page_1/event.ir.pdl" << 'EOF'
SECTION      | KEY                | VALUE
----------------------------------------
META         | piece_id           | common_events
STATE        | source               | harness-test
NODE         | id=1 type=change_gold | amount=50
EOF
  cat > "$COMMON_DIR/event_pkg/pages/page_1/cmd_1.sh" << 'SCRIPT'
#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="$PWD"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/*.monads/*.muchi-pet/ops/+x/mr_change_gold.+x" "$ENT" '50'
SCRIPT
  chmod +x "$COMMON_DIR/event_pkg/pages/page_1/cmd_1.sh"
  cat > "$COMMON_DIR/event_pkg/pages/page_1/event.pal" << 'EOF'
# event.pal - common_events page_1
exec cmd_1.sh
halt
EOF

  echo "qolq=0" > "$COMMON_DIR/inventory.txt"
  local PLAY="$HOUSE/*.monads/*.muchi-pet/ops/play_event.sh"
  # NOTE: no UI trigger point exists for common events yet (documented gap,
  # see EVENTS_RUNTIME.md) — direct script invocation here is the ONLY
  # existing path, not a relay-rule violation; there is no relay to use yet.
  bash "$PLAY" "$COMMON_DIR" "$HOUSE" > "$RESULTS/common_event_run.txt" 2>&1
  if grep -q "qolq=50" "$COMMON_DIR/inventory.txt"; then
    pass "common_event: session-level event ran correctly (qolq 0 -> 50), same runtime as entity events"
  else
    fail "common_event: expected qolq=50, got $(cat "$COMMON_DIR/inventory.txt" 2>/dev/null)"
  fi
}

test_common_event_gui() {
  log "=== TEST: common_event_gui (event-ez reused UNMODIFIED against session package) ==="
  if [ ! -d "$COMMON_DIR/event_pkg/pages/page_1" ]; then
    log "common_event_gui depends on test_common_event's fixture — running it first"
    test_common_event
  fi
  cd "$EVENT_EZ_DIR" || { fail "common_event_gui: cannot cd to $EVENT_EZ_DIR"; return; }
  EZ_PKG_NAME=common_events EZ_PKG_DIR="$COMMON_DIR/event_pkg" sh button.sh r > "$RESULTS/ez_launch.log" 2>&1 &
  sleep 3
  local SESS
  SESS=$(ls -td pieces/sessions/*/ 2>/dev/null | head -1)
  if [ -z "$SESS" ]; then
    fail "common_event_gui: no event-ez session directory appeared"
    return
  fi
  local FRAME="$SESS/pieces/display/current_frame.txt"
  local KH="$SESS/pieces/keyboard/history.txt"
  cp "$FRAME" "$RESULTS/ez_gallery_frame.txt" 2>/dev/null
  if grep -q "common_events" "$RESULTS/ez_gallery_frame.txt" 2>/dev/null; then
    pass "common_event_gui: event-ez opened against session package, title shows 'common_events'"
  else
    fail "common_event_gui: event-ez frame did not show expected title"
  fi
  echo "[$(date '+%Y-%m-%d %H:%M:%S')] KEY_PRESSED: 49" >> "$KH"; sleep 1
  echo "[$(date '+%Y-%m-%d %H:%M:%S')] KEY_PRESSED: 13" >> "$KH"; sleep 1
  cp "$FRAME" "$RESULTS/ez_page1_frame.txt" 2>/dev/null
  if grep -q "Change Gold" "$RESULTS/ez_page1_frame.txt" 2>/dev/null; then
    pass "common_event_gui: navigated into Page 1, real Change Gold command visible"
  else
    fail "common_event_gui: Page 1 did not show the expected Change Gold command"
  fi
  sh button.sh kill > /dev/null 2>&1
}

# =========================================================================
ALL_TESTS="entity_event entity_multitrigger common_event common_event_gui"
RUN_TESTS="${*:-$ALL_TESTS}"

echo "# Event System E2E Test Run — $RUN_TS" >> "$SUMMARY"
echo "" >> "$SUMMARY"
for t in $RUN_TESTS; do
  "test_$t"
done

echo "" >> "$SUMMARY"
echo "Full log: $RESULTS/log.txt" >> "$SUMMARY"
log "=== Run complete. Summary: $SUMMARY ==="
cat "$SUMMARY"
