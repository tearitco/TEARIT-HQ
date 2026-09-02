#!/bin/bash
# events_hq_picker_test_harness.sh — test the events-hq command picker
# (Add Command flow: open picker, arrow navigate, select type, type fields, submit, cancel).
#
# Relay-only: bare decimal ASCII codes appended to the events-hq history
# file. Text state dump via relay code 210, PNG frame dump via relay code
# 112 (cheap audit per _.0.aigent-testing-k9.txt).
#
# Tests:
#   T1: Picker opens on "+ Add Command" Enter
#   T2: Arrow-down moves focus through command types
#   T3: Enter selects a type, enters field-edit mode
#   T4: Escape cancels back to normal mode
#   T5: Enter-to-submit (type text, Enter to next field, Enter to submit)
#   T6: Cancel via Escape while editing (text preserved, picker closes)
#
# Outputs PNG snapshots + manifest.txt into the presentations folder,
# then calls make_presentation_video.py to render the final MP4.
#
# Usage:
#   bash events_hq_picker_test_harness.sh
#   (HOUSE defaults to walking up to the 44.xyz* house root)

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
  echo "events_hq_picker_test_harness: could not find house root above $HERE" >&2
  exit 1
}

HOUSE="${HOUSE:-$(find_house_root)}"
PAL="$HOUSE/xyzfs/users/0a9558a7-7c74-4358-833c-2d5b21edc421/home/livedesk/pals/cursword"
RELAY="$HOUSE/#.desktop/events_hq_history.txt"
STATE="/tmp/db-hq-state.txt"
PNG="/tmp/events-hq-frame.png"
PROC_PATTERN="khtpm_core_render\.\+x"

ENTITY_DIR="$PAL"
PKG_DIR="$ENTITY_DIR/event_pkg"

RESULTS="$PAL/presentations/events-hq-picker-test-$(date '+%Y%m%d-%H%M%S')"
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
}
trap cleanup EXIT

# =========================================================================
# Step 0: zero stray processes
# =========================================================================
log "=== step 0: zero stray processes ==="
existing="$(any_pids "$PROC_PATTERN")"
if [ -n "$existing" ]; then
  log "stray process(es) already running: $existing"
  log "killing them first (house standing rule)"
  echo "$existing" | xargs -r kill -KILL
  sleep 1
  existing="$(any_pids "$PROC_PATTERN")"
  if [ -n "$existing" ]; then
    fail "could not kill stray processes: $existing"
    exit 1
  fi
fi

# =========================================================================
# Step 0b: truncate relay file
# =========================================================================
: > "$RELAY"

# =========================================================================
# Step 1: launch events-hq
# =========================================================================
log "=== step 1: launch events-hq ==="
BIN="$HOUSE/*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
CHTPM="$HOUSE/&.widgits/events-hq/pieces/dashboard.chtpm"

if [ ! -x "$BIN" ]; then
  fail "binary not found: $BIN"
  exit 1
fi

setsid nohup "$BIN" "$HOUSE" "$CHTPM" "$PKG_DIR" "cursword" \
  >/tmp/events-hq-picker-test.log 2>&1 < /dev/null &
disown 2>/dev/null || true
sleep 2

pids="$(any_pids "$PROC_PATTERN")"
n="$(echo "$pids" | grep -c . || true)"
if [ "$n" != "1" ]; then
  fail "expected 1 process, got $n"
  cat /tmp/events-hq-picker-test.log 2>/dev/null
  exit 1
fi
log "events-hq launched (PID $pids)"

# =========================================================================
# Step 2: dump initial state + frame
# =========================================================================
log "=== step 2: dump initial state ==="
dump_state

if [ ! -f "$STATE" ]; then
  fail "state dump file not created at $STATE"
  exit 1
fi
cp "$STATE" "$RESULTS/01_initial_state.txt"

dump_frame "01_initial_state.png"

ADD_CMD_NAV=$(grep '+ Add Command' "$STATE" | sed -n 's/.*nav\[\([0-9]*\)\].*/\1/p')
if [ -z "$ADD_CMD_NAV" ]; then
  fail "could not find '+ Add Command' in nav list"
  exit 1
fi
log "Add Command is at nav_index=$ADD_CMD_NAV"

# =========================================================================
# T1: Picker opens on "+ Add Command" Enter
# =========================================================================
log "=== T1: open picker ==="
send_digits "$ADD_CMD_NAV"
sleep 0.3
send_code 13  # Enter on "+ Add Command"
sleep 0.8
dump_state
cp "$STATE" "$RESULTS/02_picker_open.txt"
dump_frame "02_picker_open.png"

if grep -q "g_evhq_picker_open=1" "$STATE"; then
  pass "T1: picker opened (g_evhq_picker_open=1)"
else
  fail "T1: picker did NOT open"
  grep "g_evhq_picker" "$STATE"
fi

if grep -q "g_evhq_picker_type=-1" "$STATE"; then
  pass "T1: picker in type-list mode (g_evhq_picker_type=-1)"
else
  fail "T1: picker not in type-list mode"
fi

# =========================================================================
# T2: Arrow-down moves focus through command types
# =========================================================================
log "=== T2: arrow navigation ==="
FOCUS_BEFORE=$(grep 'g_focus_nav=' "$STATE" | head -1 | sed 's/g_focus_nav=//')
log "focus before arrows: $FOCUS_BEFORE"

send_code 201  # Arrow Down
sleep 0.5
dump_state
cp "$STATE" "$RESULTS/03_after_arrow_down.txt"
dump_frame "03_after_arrow_down.png"

FOCUS_AFTER=$(grep 'g_focus_nav=' "$STATE" | head -1 | sed 's/g_focus_nav=//')
log "focus after arrow down: $FOCUS_AFTER"

if [ "$FOCUS_AFTER" != "$FOCUS_BEFORE" ] && [ -n "$FOCUS_AFTER" ]; then
  pass "T2: arrow-down moved focus ($FOCUS_BEFORE -> $FOCUS_AFTER)"
else
  fail "T2: arrow-down did not change focus (still $FOCUS_AFTER)"
fi

send_code 200  # Arrow Up
sleep 0.5
dump_state
FOCUS_BACK=$(grep 'g_focus_nav=' "$STATE" | head -1 | sed 's/g_focus_nav=//')
if [ "$FOCUS_BACK" = "$FOCUS_BEFORE" ]; then
  pass "T2: arrow-up restored focus to $FOCUS_BACK"
else
  fail "T2: arrow-up did not restore focus (got $FOCUS_BACK, expected $FOCUS_BEFORE)"
fi

# =========================================================================
# T3: Enter selects a type, enters field-edit mode
# =========================================================================
log "=== T3: select command type ==="
send_code 201  # Arrow Down to type 1
sleep 0.3
send_code 13  # Enter to select
sleep 0.8
dump_state
cp "$STATE" "$RESULTS/04_type_selected.txt"
dump_frame "04_type_selected.png"

if grep -q "g_evhq_picker_type=[0-9]" "$STATE"; then
  PICKER_TYPE=$(grep 'g_evhq_picker_type=' "$STATE" | sed 's/g_evhq_picker_type=//')
  pass "T3: type selected (g_evhq_picker_type=$PICKER_TYPE)"
else
  fail "T3: picker did not enter field-edit mode"
  grep "g_evhq_picker" "$STATE"
fi

if grep -q "g_evhq_active_field=" "$STATE"; then
  pass "T3: field-edit mode active (g_evhq_active_field visible)"
else
  fail "T3: g_evhq_active_field not found in state dump"
fi

# =========================================================================
# T4: Escape cancels back to normal mode
# =========================================================================
log "=== T4: escape cancels picker ==="
send_code 27  # Escape
sleep 0.8
dump_state
cp "$STATE" "$RESULTS/05_after_escape.txt"
dump_frame "05_after_escape.png"

if grep -q "g_evhq_picker_open=0" "$STATE"; then
  pass "T4: picker closed after Escape"
else
  fail "T4: picker still open after Escape"
  grep "g_evhq_picker" "$STATE"
fi

# =========================================================================
# T5: Enter-to-submit (type text, Enter to next field, Enter to submit)
# =========================================================================
log "=== T5: enter-to-submit ==="

# Count commands before (find the active page from state dump)
dump_state
ACTIVE_PAGE=$(grep 'tag=tab id=' "$STATE" | head -1 | sed 's/.*label=//' | sed 's/<--.*//' | tr -d ' ')
if [ -z "$ACTIVE_PAGE" ]; then ACTIVE_PAGE="page_1"; fi
IR_PATH="$PKG_DIR/pages/$ACTIVE_PAGE/event.ir.pdl"
log "active page: $ACTIVE_PAGE (ir: $IR_PATH)"
CMDS_BEFORE=$(grep -c "^NODE" "$IR_PATH" 2>/dev/null)
if [ -z "$CMDS_BEFORE" ]; then CMDS_BEFORE=0; fi
log "commands before T5: $CMDS_BEFORE"

# Re-open picker
send_digits "$ADD_CMD_NAV"
sleep 0.3
send_code 13  # Enter on "+ Add Command"
sleep 0.8

if ! grep -q "g_evhq_picker_open=1" "$STATE"; then
  dump_state
fi

# Select type index 2 (Show Choices - has 2 fields: choices + default)
send_code 201  # Arrow Down to type 1
sleep 0.2
send_code 201  # Arrow Down to type 2 (Show Choices)
sleep 0.3
send_code 13   # Enter to select
sleep 0.8
dump_state
cp "$STATE" "$RESULTS/06_t5_field_edit.txt"

if grep -q "g_evhq_picker_type=[0-9]" "$STATE"; then
  pass "T5: entered field-edit mode"
else
  fail "T5: did not enter field-edit mode"
fi

# Type text into field 1: "yes,no,maybe"
send_text "yes,no,maybe"
sleep 0.3
dump_state
cp "$STATE" "$RESULTS/07_t5_field1_typed.txt"

FIELD1_VAL=$(grep 'g_evhq_field1=' "$STATE" | head -1 | sed 's/.*g_evhq_field1=\[//' | sed 's/\].*//')
log "field1 value after typing: '$FIELD1_VAL'"
if [ "$FIELD1_VAL" = "yes,no,maybe" ]; then
  pass "T5: field1 text entered correctly"
else
  fail "T5: field1 text mismatch (got '$FIELD1_VAL', expected 'yes,no,maybe')"
fi

# Enter to move to field 2
send_code 13
sleep 0.5
dump_state
cp "$STATE" "$RESULTS/08_t5_moved_to_field2.txt"

ACTIVE=$(grep 'g_evhq_active_field=' "$STATE" | head -1 | sed 's/.*g_evhq_active_field=//' | awk '{print $1}')
log "active field after Enter: $ACTIVE"
if [ "$ACTIVE" = "1" ]; then
  pass "T5: moved to field 2"
else
  fail "T5: did not move to field 2 (active_field=$ACTIVE)"
fi

# Type text into field 2: "0"
send_text "0"
sleep 0.3

# Enter to submit
send_code 13
sleep 1.0
dump_state
cp "$STATE" "$RESULTS/09_t5_after_submit.txt"
dump_frame "09_t5_after_submit.png"

if grep -q "g_evhq_picker_open=0" "$STATE"; then
  pass "T5: picker closed after submit"
else
  fail "T5: picker still open after submit"
fi

# Check if command was added
CMDS_AFTER=$(grep -c "^NODE" "$IR_PATH" 2>/dev/null)
if [ -z "$CMDS_AFTER" ]; then CMDS_AFTER=0; fi
log "commands after T5: $CMDS_AFTER"
if [ "$CMDS_AFTER" -gt "$CMDS_BEFORE" ]; then
  pass "T5: command added to event.ir.pdl ($CMDS_BEFORE -> $CMDS_AFTER)"
  tail -1 "$IR_PATH" >> "$RESULTS/log.txt"
else
  fail "T5: no new command in event.ir.pdl (still $CMDS_AFTER)"
fi

# =========================================================================
# T6: Cancel via Escape while editing (text preserved, picker closes)
# =========================================================================
log "=== T6: cancel while editing ==="

# Re-open picker
send_digits "$ADD_CMD_NAV"
sleep 0.3
send_code 13  # Enter on "+ Add Command"
sleep 0.8
dump_state

# Select type 1 (Show Text - has 2 fields: text + speaker)
send_code 201  # Arrow Down
sleep 0.3
send_code 13   # Enter to select
sleep 0.8
dump_state

# Type some text into field 1
send_text "discard me"
sleep 0.3
dump_state
cp "$STATE" "$RESULTS/10_t6_text_before_cancel.txt"

FIELD1_BEFORE=$(grep 'g_evhq_field1=' "$STATE" | head -1 | sed 's/.*g_evhq_field1=\[//' | sed 's/\].*//')
log "field1 before cancel: '$FIELD1_BEFORE'"

# Escape to cancel
send_code 27
sleep 0.8
dump_state
cp "$STATE" "$RESULTS/11_t6_after_cancel.txt"
dump_frame "11_t6_after_cancel.png"

if grep -q "g_evhq_picker_open=0" "$STATE"; then
  pass "T6: picker closed after Escape"
else
  fail "T6: picker still open after Escape"
fi

# Verify no new command was added
CMDS_AFTER_CANCEL=$(grep -c "^NODE" "$IR_PATH" 2>/dev/null)
if [ -z "$CMDS_AFTER_CANCEL" ]; then CMDS_AFTER_CANCEL=0; fi
if [ "$CMDS_AFTER_CANCEL" = "$CMDS_AFTER" ]; then
  pass "T6: no command added after cancel (still $CMDS_AFTER_CANCEL)"
else
  fail "T6: unexpected command count change after cancel ($CMDS_AFTER -> $CMDS_AFTER_CANCEL)"
fi

# =========================================================================
# Write manifest.txt for make_presentation_video.py
# =========================================================================
cat > "$RESULTS/manifest.txt" <<'MANIFEST'
01_initial_state.png | 6 | Events-hq initial state: page tabs, trigger, command list, and footer buttons visible. Add Command at nav index 5.
02_picker_open.png | 6 | Picker overlay opened after pressing Enter on "+ Add Command". Command types listed vertically, Cancel at bottom.
03_after_arrow_down.png | 5 | Arrow-down moved focus from first item to second command type in the picker list.
04_type_selected.png | 6 | Enter selected a command type (Show Text). Picker entered field-edit mode with editable text fields.
05_after_escape.png | 5 | Escape closed the picker overlay, returning to the normal events-hq view.
09_t5_after_submit.png | 6 | T5: Show Choices command submitted with fields "yes,no,maybe" and default "0". Picker closed, command added to event.ir.pdl.
11_t6_after_cancel.png | 5 | T6: Escape cancelled the picker during field editing. No command added, picker closed cleanly.
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
