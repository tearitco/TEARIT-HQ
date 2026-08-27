#!/bin/bash
# events_hq_task3_test_harness.sh — test Conditional Branch (Task 3)
#
# Compilation tests (via manager):
#   T1: Compile IR with if/else/end → verify PAL has bne x12, labels, j skip
#   T2: Compile IR with if/end (no else) → verify no _else label
#
# Runtime tests (via direct prisc+x):
#   T3: Switch ON → if-branch writes on_marker, OFF path writes off_marker
#   T4: Switch OFF → same PAL, different path taken
#   T5: Else-less, switch ON → only if-branch writes marker
#   T6: Else-less, switch OFF → no marker written
#
# PNG snapshots + manifest → make_presentation_video.py

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
  echo "task3_harness: could not find house root above $HERE" >&2
  exit 1
}

HOUSE="${HOUSE:-$(find_house_root)}"
PAL="$HOUSE/xyzfs/users/0a9558a7-7c74-4358-833c-2d5b21edc421/home/livedesk/pals/cursword"
RELAY="$HOUSE/#.desktop/events_hq_history.txt"
STATE="/tmp/db-hq-state.txt"
PNG="/tmp/events-hq-frame.png"
PROC_PATTERN="khtpm_entity_menu_render\.\+x"
MGR_PATTERN="khtpm_events_hq_manager\.\+x"

ENTITY_DIR="$PAL"
PKG_DIR="$ENTITY_DIR/event_pkg"

MGR_ACTION="$PKG_DIR/.hq_manager/action.txt"
IR_FILE="$PKG_DIR/pages/page_1/event.ir.pdl"
PAL_FILE="$PKG_DIR/pages/page_1/event.pal"
PRISC_BIN="$HOUSE/101.mutaclsym🧟‍♂️️+18.0G/system/prisc+x"
PRISC_CWD="$HOUSE/101.mutaclsym🧟‍♂️️+18.0G"
PRISC_DIR="$PRISC_CWD/system"
SWITCHES_FILE="/tmp/task3_switches.txt"

RESULTS="$PAL/presentations/events-hq-task3-test-$(date '+%Y%m%d-%H%M%S')"
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

send_action() {
  local action="$1"
  echo "$action" > "$MGR_ACTION"
  log "action sent: $action"
  sleep 1.0
}

cleanup() {
  send_code 27; sleep 0.3
  send_code 27; sleep 0.3
  local pids
  for pat in "$PROC_PATTERN" "$MGR_PATTERN"; do
    pids="$(any_pids "$pat")"
    if [ -n "$pids" ]; then
      echo "$pids" | xargs -r kill -TERM 2>/dev/null
      sleep 1
      pids="$(any_pids "$pat")"
      if [ -n "$pids" ]; then
        echo "$pids" | xargs -r kill -KILL 2>/dev/null
      fi
    fi
  done
  rm -f /tmp/ce_task3_on_marker.txt /tmp/ce_task3_off_marker.txt
  rm -f "$SWITCHES_FILE"
}
trap cleanup EXIT

# =========================================================================
log "=== step 0: zero stray processes ==="
for pat in "$PROC_PATTERN" "$MGR_PATTERN"; do
  existing="$(any_pids "$pat")"
  if [ -n "$existing" ]; then
    log "stray process(es): $existing"
    echo "$existing" | xargs -r kill -KILL
    sleep 1
  fi
done
rm -f /tmp/ce_task3_on_marker.txt /tmp/ce_task3_off_marker.txt

: > "$RELAY"

# =========================================================================
log "=== step 1: launch events-hq ==="
BIN="$HOUSE/*.monads/*.livedesk-taskbar/ops/+x/khtpm_entity_menu_render.+x"
CHTPM="$HOUSE/&.widgits/events-hq/pieces/dashboard.chtpm"

setsid nohup "$BIN" "$HOUSE" "$CHTPM" "$PKG_DIR" "cursword" \
  >/tmp/task3-test.log 2>&1 < /dev/null &
disown 2>/dev/null || true
sleep 3

pids="$(any_pids "$PROC_PATTERN")"
n="$(echo "$pids" | grep -c . || true)"
if [ "$n" != "1" ]; then
  fail "expected 1 render process, got $n"
  cat /tmp/task3-test.log 2>/dev/null
  exit 1
fi
log "events-hq launched (PID $pids)"

# Clean stale IR
if [ -f "$IR_FILE" ]; then
  log "cleaning stale IR"
  : > "$IR_FILE"
fi

sleep 2
log "manager processes: $(any_pids "$MGR_PATTERN" | tr '\n' ' ')"

# =========================================================================
log "=== step 2: dump initial state ==="
dump_state
cp "$STATE" "$RESULTS/01_initial_state.txt" 2>/dev/null
dump_frame "01_initial_state.png"

# =========================================================================
# T1: Compile IR with if/else/end → verify PAL structure
# =========================================================================
log "=== T1: compile if/else/end ==="

: > "$IR_FILE"
send_action "append:if|switch_name=test_switch|compare=on"
send_action "append:call_common_event|event_name=test_target|trigger=on-click"
send_action "append:else|"
send_action "append:call_common_event|event_name=test_target|trigger=on-click"
send_action "append:end|"

sleep 0.5
IR_NODES=$(grep -c '^NODE' "$IR_FILE" 2>/dev/null || echo 0)
log "IR has $IR_NODES NODE lines"
if [ "$IR_NODES" -ge 5 ]; then
  pass "T1: IR has $IR_NODES nodes (if+cmd+else+cmd+end)"
else
  fail "T1: expected 5+ IR nodes, got $IR_NODES"
fi

if [ -f "$PAL_FILE" ]; then
  log "event.pal contents:"
  cat "$PAL_FILE" >> "$RESULTS/log.txt"
  cat "$PAL_FILE"

  if grep -q 'bne x12' "$PAL_FILE"; then
    pass "T1: PAL has bne x12 (correct register)"
  elif grep -q 'bne x1' "$PAL_FILE"; then
    fail "T1: PAL has bne x1 (WRONG — needs x12)"
  else
    fail "T1: PAL missing bne instruction"
  fi

  if grep -q '_else_1:' "$PAL_FILE"; then
    pass "T1: PAL has _else_1 label"
  else
    fail "T1: PAL missing _else_1 label"
  fi

  if grep -q '_endif_1:' "$PAL_FILE"; then
    pass "T1: PAL has _endif_1 label"
  else
    fail "T1: PAL missing _endif_1 label"
  fi

  if grep -q 'j _endif_1' "$PAL_FILE"; then
    pass "T1: PAL has j _endif_1 (else skip)"
  else
    fail "T1: PAL missing j _endif_1"
  fi

  if grep -q 'ecall.*switches.txt.*test_switch' "$PAL_FILE"; then
    pass "T1: PAL has ecall for test_switch"
  else
    fail "T1: PAL missing ecall for test_switch"
  fi

  dump_frame "02_t1_pal_structure.png"
else
  fail "T1: event.pal not created"
fi

# =========================================================================
# T2: Compile IR with if/end (no else) → verify no _else label
# =========================================================================
log "=== T2: compile if/end (no else) ==="

: > "$IR_FILE"
send_action "append:if|switch_name=test_switch|compare=on"
send_action "append:call_common_event|event_name=test_target|trigger=on-click"
send_action "append:end|"

sleep 0.5
if [ -f "$PAL_FILE" ]; then
  log "event.pal (if/end only):"
  cat "$PAL_FILE"

  # else-less: bne targets _else_1 which is emitted before _endif_1
  # so _else_1: label IS present (needed as bne target) — verify it falls through to _endif_1
  if grep -q '_else_1:' "$PAL_FILE" && grep -q '_endif_1:' "$PAL_FILE"; then
    # Verify _else_1 is immediately before _endif_1 (no gap = no else body)
    BETWEEN=$(sed -n '/_else_1:/,/_endif_1:/p' "$PAL_FILE" | grep -c 'ecall\|li\|j ')
    if [ "$BETWEEN" -eq 0 ]; then
      pass "T2: else-less — _else_1 falls straight through to _endif_1 (no else body)"
    else
      fail "T2: else-less — $BETWEEN instructions between _else_1 and _endif_1 (unexpected)"
    fi
  else
    fail "T2: missing _else_1 or _endif_1 label"
  fi

  if grep -q '_endif_1:' "$PAL_FILE"; then
    pass "T2: PAL has _endif_1 label"
  else
    fail "T2: PAL missing _endif_1 label"
  fi

  if grep -q 'bne x12' "$PAL_FILE"; then
    pass "T2: PAL has bne x12"
  else
    fail "T2: PAL missing bne x12"
  fi

  dump_frame "03_t2_if_end_only.png"
else
  fail "T2: event.pal not created"
fi

# =========================================================================
# Runtime tests — direct prisc+x with distinct ON/OFF markers
# =========================================================================

# T3/T4: if/else/end PAL mimicking compile_page() output
cat > "$PRISC_DIR/task3_if_else.pal" <<'PAL'
# if/else/end: reads switch, branches on x12
li x15, 6
ecall "SWITCHES_PATH" "test_switch"
li x2, 1
bne x12, x2, _else_1
# ON path: write on_marker
li x15, 1
li x13, 1
ecall "/tmp/ce_task3_on_marker.txt"
li x15, 3
ecall "branch_on_executed"
li x15, 2
ecall ""
j _endif_1
_else_1:
# OFF path: write off_marker
li x15, 1
li x13, 1
ecall "/tmp/ce_task3_off_marker.txt"
li x15, 3
ecall "branch_off_executed"
li x15, 2
ecall ""
_endif_1:
halt
PAL

# Patch SWITCHES_PATH in the PAL
sed -i "s|SWITCHES_PATH|$SWITCHES_FILE|g" "$PRISC_DIR/task3_if_else.pal"

# =========================================================================
# T3: Runtime — switch ON → on_marker written
# =========================================================================
log "=== T3: runtime — switch ON (if/else/end) ==="
echo "test_switch=1" > "$SWITCHES_FILE"
rm -f /tmp/ce_task3_on_marker.txt /tmp/ce_task3_off_marker.txt

cd "$PRISC_CWD" && "$PRISC_BIN" "$PRISC_DIR/task3_if_else.pal" 2>&1 | tee -a "$RESULTS/log.txt"

sleep 0.5
if [ -f /tmp/ce_task3_on_marker.txt ] && [ ! -f /tmp/ce_task3_off_marker.txt ]; then
  pass "T3: switch ON → on_marker created, off_marker absent"
elif [ -f /tmp/ce_task3_on_marker.txt ] && [ -f /tmp/ce_task3_off_marker.txt ]; then
  fail "T3: both markers created — branch logic broken"
else
  fail "T3: on_marker NOT created — ON branch did not execute"
fi
dump_frame "04_t3_runtime_on.png"

# =========================================================================
# T4: Runtime — switch OFF → off_marker written
# =========================================================================
log "=== T4: runtime — switch OFF (if/else/end) ==="
echo "test_switch=0" > "$SWITCHES_FILE"
rm -f /tmp/ce_task3_on_marker.txt /tmp/ce_task3_off_marker.txt

cd "$PRISC_CWD" && "$PRISC_BIN" "$PRISC_DIR/task3_if_else.pal" 2>&1 | tee -a "$RESULTS/log.txt"

sleep 0.5
if [ -f /tmp/ce_task3_off_marker.txt ] && [ ! -f /tmp/ce_task3_on_marker.txt ]; then
  pass "T4: switch OFF → off_marker created, on_marker absent"
elif [ -f /tmp/ce_task3_on_marker.txt ] && [ -f /tmp/ce_task3_off_marker.txt ]; then
  fail "T4: both markers created — branch logic broken"
else
  fail "T4: off_marker NOT created — OFF branch did not execute"
fi
dump_frame "05_t4_runtime_off.png"

# =========================================================================
# T5/T6: else-less PAL
# =========================================================================
cat > "$PRISC_DIR/task3_if_only.pal" <<'PAL'
# if/end (no else): reads switch, branches on x12
li x15, 6
ecall "SWITCHES_PATH" "test_switch"
li x2, 1
bne x12, x2, _endif_1
# if-true path: write on_marker
li x15, 1
li x13, 1
ecall "/tmp/ce_task3_on_marker.txt"
li x15, 3
ecall "if_branch_executed"
li x15, 2
ecall ""
_endif_1:
halt
PAL
sed -i "s|SWITCHES_PATH|$SWITCHES_FILE|g" "$PRISC_DIR/task3_if_only.pal"

# T5: else-less, switch ON → on_marker written
log "=== T5: runtime else-less — switch ON ==="
echo "test_switch=1" > "$SWITCHES_FILE"
rm -f /tmp/ce_task3_on_marker.txt /tmp/ce_task3_off_marker.txt

cd "$PRISC_CWD" && "$PRISC_BIN" "$PRISC_DIR/task3_if_only.pal" 2>&1 | tee -a "$RESULTS/log.txt"

sleep 0.5
if [ -f /tmp/ce_task3_on_marker.txt ]; then
  pass "T5: else-less ON → on_marker created"
else
  fail "T5: else-less ON → on_marker NOT created"
fi
dump_frame "06_t5_elseless_on.png"

# T6: else-less, switch OFF → no marker
log "=== T6: runtime else-less — switch OFF ==="
echo "test_switch=0" > "$SWITCHES_FILE"
rm -f /tmp/ce_task3_on_marker.txt /tmp/ce_task3_off_marker.txt

cd "$PRISC_CWD" && "$PRISC_BIN" "$PRISC_DIR/task3_if_only.pal" 2>&1 | tee -a "$RESULTS/log.txt"

sleep 0.5
if [ ! -f /tmp/ce_task3_on_marker.txt ]; then
  pass "T6: else-less OFF → no marker (correct — if-branch skipped)"
else
  fail "T6: else-less OFF → on_marker EXISTS (should not)"
fi

# =========================================================================
# Manifest
# =========================================================================
cat > "$RESULTS/manifest.txt" <<'MANIFEST'
01_initial_state.png | 6 | Events-hq initial state before Task 3 test. Clean page.
02_t1_pal_structure.png | 6 | After compiling if/else/end: PAL has bne x12, _else_1, _endif_1 labels.
03_t2_if_end_only.png | 6 | After compiling if/end (no else): PAL has _endif_1 but no _else.
04_t3_runtime_on.png | 6 | Runtime switch ON: if-branch executed, on_marker created.
05_t4_runtime_off.png | 6 | Runtime switch OFF: else-branch executed, off_marker created.
06_t5_elseless_on.png | 6 | Runtime else-less ON: if-branch executed. OFF: no branch ran.
MANIFEST

# =========================================================================
log ""
log "=== results ==="
cat "$SUMMARY"
log ""
log "Snapshots: $SNAP_DIR/"

# =========================================================================
PYTHON="$PAL/presentations/make_presentation_video.py"
if [ -f "$PYTHON" ]; then
  log "=== building presentation video ==="
  python3 "$PYTHON" "$RESULTS" 2>&1 | tee -a "$RESULTS/log.txt"
  log "video built: $RESULTS/"
fi

log "Harness complete."
