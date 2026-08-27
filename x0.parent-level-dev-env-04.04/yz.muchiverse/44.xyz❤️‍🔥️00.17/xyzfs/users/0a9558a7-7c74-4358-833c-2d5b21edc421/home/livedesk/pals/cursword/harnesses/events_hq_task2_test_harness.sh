#!/bin/bash
# events_hq_task2_test_harness.sh — test Call Common Event (Task 2)
#
# Tests the bracket-drop [{trigger}] syntax and runtime execution:
#   T1: Submit call_common_event WITH trigger → verify OP line has trigger arg
#   T2: Submit call_common_event WITHOUT trigger → verify bracket dropped
#   T3: Play event → verify target common event ran (marker file)
#
# Relay-only, PNG snapshots + manifest → make_presentation_video.py

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
  echo "task2_harness: could not find house root above $HERE" >&2
  exit 1
}

HOUSE="${HOUSE:-$(find_house_root)}"
PAL="$HOUSE/xyzfs/users/0a9558a7-7c74-4358-833c-2d5b21edc421/home/livedesk/pals/cursword"
RELAY="$HOUSE/#.desktop/events_hq_history.txt"
STATE="/tmp/db-hq-state.txt"
PNG="/tmp/events-hq-frame.png"
PROC_PATTERN="khtpm_entity_menu_render\.\+x"

ENTITY_DIR="$PAL"
PKG_DIR="$ENTITY_DIR/event_pkg"
CE_DIR="$HOUSE/common_events/test_target"

RESULTS="$PAL/presentations/events-hq-task2-test-$(date '+%Y%m%d-%H%M%S')"
SNAP_DIR="$RESULTS/snapshots"
mkdir -p "$SNAP_DIR"
SUMMARY="$RESULTS/summary.txt"

log() { echo "[$(date '+%H:%M:%S')] $*" | tee -a "$RESULTS/log.txt"; }
pass() { log "PASS: $*"; echo "PASS: $*" >> "$SUMMARY"; }
fail() { log "FAIL: $*"; echo "FAIL: $*" >> "$SUMMARY"; }

send_code() {
  echo "$1" >> "$RELAY"
  sleep 0.3
}

send_digits() {
  local n="$1" i c
  for ((i = 0; i < ${#n}; i++)); do
    c="${n:$i:1}"
    echo -n "${c}" | od -An -tu1 | tr -d ' ' >> "$RELAY"
    printf '\n' >> "$RELAY"
    sleep 0.05
  done
}

send_text() {
  local s="$1" i c
  for ((i = 0; i < ${#s}; i++)); do
    c="${s:$i:1}"
    printf '%s' "$c" | od -An -tu1 | tr -d ' \n' >> "$RELAY"
    printf '\n' >> "$RELAY"
    sleep 0.05
  done
}

dump_state() {
  send_code 210
  sleep 0.5
}

dump_frame() {
  send_code 112
  sleep 1.5
  if [ -f "$PNG" ]; then
    local dest="$SNAP_DIR/$1"
    cp "$PNG" "$dest"
    log "frame dumped: $dest"
  else
    log "WARNING: PNG not created for $1"
  fi
}

any_pids() {
  pgrep -f "$1" 2>/dev/null || true
}

cleanup() {
  send_code 27; sleep 0.3
  send_code 27; sleep 0.3
  local pids
  pids="$(any_pids "$PROC_PATTERN")"
  if [ -n "$pids" ]; then
    echo "$pids" | xargs -r kill -TERM 2>/dev/null
    sleep 1
    pids="$(any_pids "$PROC_PATTERN")"
    if [ -n "$pids" ]; then
      echo "$pids" | xargs -r kill -KILL 2>/dev/null
    fi
  fi
  rm -f /tmp/ce_test_marker.txt /tmp/ce_nested_marker.txt
}
trap cleanup EXIT

# =========================================================================
# Step 0: zero stray processes + clean marker
# =========================================================================
log "=== step 0: zero stray processes ==="
existing="$(any_pids "$PROC_PATTERN")"
if [ -n "$existing" ]; then
  log "stray process(es) already running: $existing"
  echo "$existing" | xargs -r kill -KILL
  sleep 1
fi

rm -f /tmp/ce_test_marker.txt

# =========================================================================
# Step 0b: truncate relay
# =========================================================================
: > "$RELAY"

# =========================================================================
# Step 1: launch events-hq
# =========================================================================
log "=== step 1: launch events-hq ==="
BIN="$HOUSE/*.monads/*.livedesk-taskbar/ops/+x/khtpm_entity_menu_render.+x"
CHTPM="$HOUSE/&.widgits/events-hq/pieces/dashboard.chtpm"

setsid nohup "$BIN" "$HOUSE" "$CHTPM" "$PKG_DIR" "cursword" \
  >/tmp/task2-test.log 2>&1 < /dev/null &
disown 2>/dev/null || true
sleep 3

pids="$(any_pids "$PROC_PATTERN")"
n="$(echo "$pids" | grep -c . || true)"
if [ "$n" != "1" ]; then
  fail "expected 1 process, got $n"
  cat /tmp/task2-test.log 2>/dev/null
  exit 1
fi
log "events-hq launched (PID $pids)"

# Clean stale commands from previous runs — truncate the IR so we start fresh
IR_FILE="$PKG_DIR/pages/page_1/event.ir.pdl"
if [ -f "$IR_FILE" ]; then
  log "cleaning stale IR: $IR_FILE"
  : > "$IR_FILE"
fi

# =========================================================================
# Step 2: dump initial state
# =========================================================================
log "=== step 2: dump initial state ==="
dump_state
cp "$STATE" "$RESULTS/01_initial_state.txt"
dump_frame "01_initial_state.png"

ADD_CMD_NAV=$(grep '+ Add Command' "$STATE" | sed -n 's/.*nav\[\([0-9]*\)\].*/\1/p')
if [ -z "$ADD_CMD_NAV" ]; then
  fail "could not find '+ Add Command' in nav list"
  exit 1
fi
log "Add Command is at nav_index=$ADD_CMD_NAV"

# =========================================================================
# T1: Submit call_common_event WITH trigger → verify OP line
# =========================================================================
log "=== T1: call_common_event WITH trigger ==="

# Open picker
send_digits "$ADD_CMD_NAV"
sleep 0.3
send_code 13
sleep 0.8
dump_state

# Navigate to call_common_event (type index 7, picker_focus = 7)
# Focus starts at 1 (Change Gold), arrow-down 6 times to reach 7 (Call Common Event)
for i in $(seq 1 6); do
  send_code 201
  sleep 0.2
done
dump_state
cp "$STATE" "$RESULTS/02_picker_type_list.png.txt"
dump_frame "02_picker_type_list.png"

# Select call_common_event
send_code 13
sleep 0.8
dump_state
cp "$STATE" "$RESULTS/03_field_edit_with_trigger.txt"

if grep -q "g_evhq_picker_type=[0-9]" "$STATE"; then
  pass "T1: entered field-edit mode for call_common_event"
else
  fail "T1: did not enter field-edit mode"
fi

# Type target event name in field 1
send_text "test_target"
sleep 0.3

# Move to field 2 (Enter advances to next field for 2-field commands)
send_code 13
sleep 0.5
dump_state

# Field 2 is a select2 cycle: [None] < > → press Right (relay 203) once to get "on-click"
send_code 203
sleep 0.5
dump_state

# Submit (Enter on field 2 → evhq_submit_picker)
send_code 13
sleep 2.0
dump_state
cp "$STATE" "$RESULTS/05_after_submit_with_trigger.txt"
dump_frame "04_after_submit_with_trigger.png"

if grep -q "g_evhq_picker_open=0" "$STATE"; then
  pass "T1: picker closed after submit"
else
  fail "T1: picker still open after submit"
fi

# Verify event.pal has OP call_event with trigger
PAL_FILE="$PKG_DIR/pages/page_1/event.pal"
if [ -f "$PAL_FILE" ]; then
  log "event.pal contents:"
  cat "$PAL_FILE" >> "$RESULTS/log.txt"
  cat "$PAL_FILE"
  # PAL template: OP call_event "{event_name}" [{trigger}]
  # trigger is unquoted in output: OP call_event "test_target" on-click
  if grep -q 'OP call_event "test_target" on-click' "$PAL_FILE"; then
    pass "T1: event.pal has OP call_event with trigger arg"
  elif grep -q 'OP call_event "test_target"' "$PAL_FILE"; then
    fail "T1: event.pal has OP call_event but MISSING trigger arg"
  else
    fail "T1: event.pal does not contain expected OP call_event line"
  fi
else
  fail "T1: event.pal not created"
fi

# =========================================================================
# T2: Submit call_common_event WITHOUT trigger → verify bracket dropped
# =========================================================================
log "=== T2: call_common_event WITHOUT trigger ==="

# Re-extract nav index — "+ Add Command" shifts after T1 added a command
dump_state
ADD_CMD_NAV=$(grep '+ Add Command' "$STATE" | sed -n 's/.*nav\[\([0-9]*\)\].*/\1/p')
if [ -z "$ADD_CMD_NAV" ]; then
  fail "T2: could not find '+ Add Command' in nav list"
  exit 1
fi
log "T2: Add Command is now at nav_index=$ADD_CMD_NAV"

# Re-open picker
send_digits "$ADD_CMD_NAV"
sleep 0.3
send_code 13
sleep 0.8

# Navigate to call_common_event again
for i in $(seq 1 6); do
  send_code 201
  sleep 0.2
done

# Select
send_code 13
sleep 0.8

# Type target name in field 1 only
send_text "test_target"
sleep 0.3

# Submit WITHOUT filling field 2 (Enter on field 1 → for 2-field commands,
# Enter advances to field 2, so we need Right to skip to Cancel, then
# actually we want to submit with empty field2.
# Actually: for 2-field commands, Enter on field 0 moves to field 1.
# We need to type nothing in field 1, move to field 2, type nothing, then Enter.
# But we already typed in field 1. Let me re-think.
# Better: Enter on field 1 (which has text) → moves to field 2 (empty)
# Then Enter on field 2 (empty) → submits
send_code 13
sleep 0.5

# Now on field 2 (empty). Enter submits.
send_code 13
sleep 2.0
dump_state
cp "$STATE" "$RESULTS/07_after_submit_no_trigger.txt"
dump_frame "07_after_submit_no_trigger.png"

# Verify event.pal has OP call_event WITHOUT trigger arg (bracket dropped)
if [ -f "$PAL_FILE" ]; then
  log "event.pal after no-trigger submit:"
  cat "$PAL_FILE" >> "$RESULTS/log.txt"
  cat "$PAL_FILE"
  # T1's OP line has trigger, T2's should not — check the LAST OP call_event line
  LAST_OP=$(grep 'OP call_event' "$PAL_FILE" | tail -1)
  log "T2: last OP line: $LAST_OP"
  if echo "$LAST_OP" | grep -q 'OP call_event "test_target"' && \
     ! echo "$LAST_OP" | grep -q 'on-click'; then
    pass "T2: bracket dropped — last OP call_event has no trigger arg"
  elif echo "$LAST_OP" | grep -q 'on-click'; then
    fail "T2: trigger arg still present on last OP line (bracket did NOT drop)"
  else
    fail "T2: event.pal missing expected OP call_event line"
  fi
else
  fail "T2: event.pal not created"
fi

# =========================================================================
# T3: Play event → verify target common event ran
# =========================================================================
log "=== T3: runtime execution ==="

# Re-write event.pal with trigger for runtime test
# (T2's no-trigger version won't match greet_player's Autorun trigger,
#  but test_target has on-click trigger so it works)
cat > "$PAL_FILE" <<'EOF'
# event.pal - call_common_event runtime test
# Calls test_target common event (on-click trigger)
# PAL template produces unquoted trigger: OP call_event "name" trigger
OP call_event "test_target" on-click
halt
EOF

log "event.pal for runtime test:"
cat "$PAL_FILE"

# Play the event
PLAY="$HOUSE/*.monads/*.muchi-pet/ops/play_event.sh"
rm -f /tmp/ce_test_marker.txt
cd "$HOUSE/101.mutaclsym🧟‍♂️️+18.0G/system" && \
  MUCHI_CALLER_PKG="$PKG_DIR" bash "$PLAY" "$PAL" "$HOUSE" 2>&1 | tee -a "$RESULTS/log.txt"

sleep 1
dump_state
dump_frame "08_after_play.png"

# Check marker file
if [ -f /tmp/ce_test_marker.txt ]; then
  MARKER_CONTENT=$(cat /tmp/ce_test_marker.txt)
  pass "T3: target event ran — marker file created: '$MARKER_CONTENT'"
else
  fail "T3: marker file NOT created — target event did not run"
fi

# Also check master_ledger for evidence
if [ -f "$CE_DIR/event_pkg/master_ledger.txt" ]; then
  log "target master_ledger:"
  tail -5 "$CE_DIR/event_pkg/master_ledger.txt" >> "$RESULTS/log.txt"
  tail -5 "$CE_DIR/event_pkg/master_ledger.txt"
fi

# =========================================================================
# T4: Nesting — test_target calls nested_inner inside itself
# =========================================================================
log "=== T4: nested call chain ==="

# test_target's event.pal now includes: OP call_event "nested_inner" on-click
# nested_inner writes /tmp/ce_nested_marker.txt
# Verify both markers from T3's play still exist, then re-play with fresh markers

rm -f /tmp/ce_test_marker.txt /tmp/ce_nested_marker.txt

# Re-write outer event.pal to call test_target (which calls nested_inner)
cat > "$PAL_FILE" <<'EOF'
# event.pal - nesting test: outer → test_target → nested_inner
OP call_event "test_target" on-click
halt
EOF

log "event.pal for nesting test:"
cat "$PAL_FILE"

cd "$HOUSE/101.mutaclsym🧟‍♂️️+18.0G/system" && \
  MUCHI_CALLER_PKG="$PKG_DIR" bash "$PLAY" "$PAL" "$HOUSE" on-click 2>&1 | tee -a "$RESULTS/log.txt"

sleep 1

if [ -f /tmp/ce_test_marker.txt ] && [ -f /tmp/ce_nested_marker.txt ]; then
  NESTED_OUTER=$(cat /tmp/ce_test_marker.txt)
  NESTED_INNER=$(cat /tmp/ce_nested_marker.txt)
  pass "T4: nesting works — outer='$NESTED_OUTER', inner='$NESTED_INNER'"
elif [ -f /tmp/ce_test_marker.txt ]; then
  fail "T4: outer marker exists but nested_inner did NOT run (no nested marker)"
else
  fail "T4: neither marker created — call chain broken"
fi

# =========================================================================
# Write manifest.txt for make_presentation_video.py
# =========================================================================
cat > "$RESULTS/manifest.txt" <<'MANIFEST'
01_initial_state.png | 6 | Events-hq initial state before Task 2 test. Clean page with Add Command button visible.
02_picker_type_list.png | 6 | Picker overlay open showing all command types. Call Common Event at the bottom of the list.
04_after_submit_with_trigger.png | 6 | After submit: event.pal compiled with OP call_event line including trigger argument.
07_after_submit_no_trigger.png | 6 | After submit: event.pal compiled with OP call_event, trigger bracket dropped (no second arg).
08_after_play.png | 6 | After playing event: marker file proves target common event executed successfully.
MANIFEST

# =========================================================================
# Summary
# =========================================================================
log ""
log "=== results ==="
cat "$SUMMARY"
log ""
log "Snapshots: $SNAP_DIR/"
log "Manifest: $RESULTS/manifest.txt"

# =========================================================================
# Build presentation video
# =========================================================================
PYTHON="$PAL/presentations/make_presentation_video.py"
if [ -f "$PYTHON" ]; then
  log "=== building presentation video ==="
  python3 "$PYTHON" "$RESULTS" 2>&1 | tee -a "$RESULTS/log.txt"
  log "video built: $RESULTS/"
else
  log "WARNING: make_presentation_video.py not found at $PYTHON"
fi

log "Harness complete."
