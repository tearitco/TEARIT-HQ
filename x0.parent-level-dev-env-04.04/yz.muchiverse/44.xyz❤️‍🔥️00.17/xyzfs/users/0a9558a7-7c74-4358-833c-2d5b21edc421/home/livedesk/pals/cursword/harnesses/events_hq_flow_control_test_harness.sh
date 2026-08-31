#!/bin/bash
# events_hq_flow_control_test_harness.sh — Flow Control's remainder
# (Loop / Break Loop / Repeat Above / Lock): real, text-verifiable e2e
# proof for the tier-3 structural commands + Comment / Exit Event /
# Label / Jump to Label / Wait.
#
# Covers (7): comment, exit_event, label, jump_to_label, loop,
# break_loop, repeat_above (+ wait, the registry-mandated pacing
# primitive, and BOTH control_switch writes + Conditional Branch reads
# that the registry's Loop design depends on).
#
# Proves, per command, the FULL real chain:
#   IR node (event.ir.pdl) -> khtpm_events_hq_manager compile_page()
#   -> event.pal bytecode -> prisc+x executes -> REAL state files
#   (switches.txt / items.txt / actor_1_stats.txt) -> text-state-dump
#   verification.
#
# Why the page is shaped this way:
#   - every command here except control_switch/change_items/change_level
#     is a STRUCTURAL marker whose only observable effect is WHERE the
#     compiled jumps/halt land — so the proof is a precise final-state
#     fingerprint produced by following the real edges once through the
#     real VM:
#
#        j user_MID            <- jump_to_label (forward, skipped arrival
#                                region by design)
#        user_MID:             <- label (arrival point re-proven at
#                                runtime by setting FC_PROVED_JUMP)
#        ...control_switch FC_PROVED_JUMP 1...
#        _loop_1:              <- loop
#        change_items tick +1  <- observable per-iteration effect
#        if FC_LOOP_GATE on    <- real Conditional Branch (GET switch)
#            break_loop        <- j _loop_end_1  (2nd iteration, gate on)
#        else
#            control_switch FC_LOOP_GATE 1 (1st iteration, gate 0 -> off
#                                so the else flips it -> loop stops after
#                                EXACTLY ONE backward traversal)
#        end
#        wait 25              <- sleep 25000 (pacing, real OP_SLEEP)
#        repeat_above         <- j _loop_1 + _loop_end_1:
#        exit_event           <- halt (REAL OP_HALT mid-page)
#        change_level +5      <- AFTER the halt: must NEVER apply
#        change_items bogus   <- AFTER the halt: must NEVER apply
#
#   -> item_tick=2 proves the BACKWARD edge (repeat_above -> loop) was
#      really traversed exactly once; level stays 1 proves exit_event
#      really halted mid-page; FC_LOOP_GATE=1 proves the FIRST-iteration
#      else branch ran; FC_PROVED_JUMP=1 proves the label jump landed.
#
#   - comment emits NOTHING (its text must not appear in event.pal).
#
# Also a live regression proof for the Task-2b byte-length fixes: every
# ecall here carries the 280+ byte absolute sandbox path (multi-byte
# emoji dirs) through BOTH the "{STATE_DIR}" template-value sink
# (control_switch) and prisc+x's per-line buffers — both FAILED at 255
# before those two fixes.
#
# No visuals. No renderer. No PNG. Text-state-dump only.

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
  echo "flow_harness: could not find house root above $HERE" >&2
  exit 1
}

HOUSE="${HOUSE:-$(find_house_root)}"
PAL="$HOUSE/xyzfs/users/0a9558a7-7c74-4358-833c-2d5b21edc421/home/livedesk/pals/cursword"
EVHQQPS="$HOUSE/&.widgits/events-hq/ops"
MGR="$EVHQQPS/+x/khtpm_events_hq_manager.+x"
PRISC="$(ls -d "$HOUSE"/101.mutaclsym*+18.0G/system/prisc+x 2>/dev/null | head -1)"
if [ -z "$PRISC" ] || [ ! -x "$PRISC" ]; then
  PRISC="$(ls -d "$HOUSE"/101.mutaclsym*+*/system/prisc+x 2>/dev/null | head -1)"
fi
PRISC_DIR="$(dirname "$PRISC")"
PRISC_CWD="$(dirname "$PRISC_DIR")"

TS=$(date '+%Y%m%d-%H%M%S')
SAND_ROOT="$PAL/harnesses/.flowctl_sandbox"
SAND="$SAND_ROOT/$TS"
EVENT_PKG="$SAND/event_pkg"
IR="$EVENT_PKG/pages/page_1/event.ir.pdl"
PAL_FILE="$EVENT_PKG/pages/page_1/event.pal"
MGR_ACTION="$EVENT_PKG/.hq_manager/action.txt"

RESULTS="$PAL/presentations/events-hq-flow-control-$TS"
mkdir -p "$RESULTS"
SUMMARY="$RESULTS/summary.txt"

log() { echo "[$(date '+%H:%M:%S')] $*" | tee -a "$RESULTS/log.txt"; }
pass() { log "PASS: $*"; echo "PASS: $*" >> "$SUMMARY"; }
fail() { log "FAIL: $*"; echo "FAIL: $*" >> "$SUMMARY"; }

cleanup() {
  for pat in "khtpm_events_hq_manager\.\+x" "prisc\+x"; do
    pids="$(pgrep -f "$pat" 2>/dev/null || true)"
    [ -n "$pids" ] && echo "$pids" | xargs -r kill -TERM 2>/dev/null
  done
  sleep 0.5
  [ -d "$SAND" ] && rm -rf "$SAND" && [ -d "$SAND_ROOT" ] && rmdir "$SAND_ROOT" 2>/dev/null
  echo "cleanup done" >> "$RESULTS/log.txt"
}
trap cleanup EXIT

send_action() {
  echo "$1" > "$MGR_ACTION"
  log "action sent: $1"
  sleep 1.6
}

# =========================================================================
log "=== step 0: preflight ==="
if [ -z "$PRISC" ] || [ ! -x "$PRISC" ]; then fail "prisc+x not found"; exit 1; fi
if [ ! -x "$MGR" ]; then fail "manager binary not found: $MGR"; exit 1; fi
log "prisc=$PRISC"

log "=== clean stray processes ==="
for pat in "khtpm_events_hq_manager\.\+x" "prisc\+x"; do
  existing="$(pgrep -f "$pat" 2>/dev/null || true)"
  [ -n "$existing" ] && echo "$existing" | xargs -r kill -KILL 2>/dev/null
done
sleep 0.5

# =========================================================================
log "=== step 1: sandbox package + IR (comment/exit/label/jump/loop/break/repeat) ==="
mkdir -p "$EVENT_PKG/pages/page_1" "$EVENT_PKG/.hq_manager"

# Pre-seed: switch gate OFF (the only authored loop exit per the
# registry design) + a start level so "change_level AFTER exit_event"
# is provably a skipped delta (level would be 6 if it ever ran).
printf 'FC_LOOP_GATE=0\n' > "$SAND/switches.txt"
printf 'level=1\n' > "$SAND/actor_1_stats.txt"

cat > "$IR" <<'EOF'
SECTION      | KEY                | VALUE
----------------------------------------
META         | piece_id           | flowctl-sandbox
STATE        | source             | events-hq
NODE         | id=1 type=comment | text=flow-control-harness
NODE         | id=2 type=jump_to_label | name=MID
NODE         | id=3 type=label | name=MID
NODE         | id=4 type=control_switch | switch_name=FC_PROVED_JUMP|switch_value=1
NODE         | id=5 type=loop | 
NODE         | id=6 type=change_items | item_id=tick|amount=1
NODE         | id=7 type=if | switch_name=FC_LOOP_GATE|compare=on
NODE         | id=8 type=break_loop | 
NODE         | id=9 type=else | 
NODE         | id=10 type=control_switch | switch_name=FC_LOOP_GATE|switch_value=1
NODE         | id=11 type=end | 
NODE         | id=12 type=wait | ms=25
NODE         | id=13 type=repeat_above | 
NODE         | id=14 type=exit_event | 
NODE         | id=15 type=change_level | actor_id=1|amount=5
NODE         | id=16 type=change_items | item_id=should_not_exist|amount=999
EOF

# =========================================================================
log "=== step 2: launch events-hq manager (standalone, real house root) ==="
setsid nohup "$MGR" "$HOUSE" "$EVENT_PKG" "flowctl-sandbox" >"$RESULTS/mgr.log" 2>&1 </dev/null &
disown 2>/dev/null || true
sleep 2

# The manager only compiles on a REAL action (IR is pre-written, so an
# identity comment edit forces a full recompile through the live path).
send_action "edit:1|comment|text=flow-control-harness"
sleep 2

pids="$(pgrep -f 'khtpm_events_hq_manager\.\+x' 2>/dev/null | grep -c . || true)"
if [ "$pids" != "1" ]; then
  fail "expected exactly 1 manager process, got $pids"
  cat "$RESULTS/mgr.log" 2>/dev/null
  exit 1
fi
pass "manager launched"

# =========================================================================
log "=== step 3: compile proof ==="
if [ ! -f "$PAL_FILE" ]; then
  fail "event.pal not created"
  cat "$RESULTS/mgr.log" 2>/dev/null
  exit 1
else
  pass "event.pal compiled"
  cat "$PAL_FILE" >> "$RESULTS/log.txt"
fi

NODES=$(grep -c '^NODE' "$IR" 2>/dev/null || echo 0)
if [ "$NODES" = "16" ]; then
  pass "IR has 16 NODEs (comment/jump/label/switch/loop/if/break/else/end/wait/repeat/exit)"
else
  fail "IR expected 16 NODEs, got $NODES"
fi

if grep -q '^j user_MID$' "$PAL_FILE" && grep -q '^user_MID:$' "$PAL_FILE"; then
  pass "jump_to_label emits 'j user_MID' and label emits 'user_MID:'"
else
  fail "label/jump_to_label emission missing"
fi

if grep -q '^_loop_1:$' "$PAL_FILE" && grep -q '^j _loop_1$' "$PAL_FILE"; then
  pass "loop emits '_loop_1:' and repeat_above emits backward 'j _loop_1'"
else
  fail "loop/repeat_above backward edge missing from PAL"
fi

if grep -q '^_loop_end_1:$' "$PAL_FILE"; then
  pass "repeat_above emits the '_loop_end_1:' break target"
else
  fail "_loop_end_1: break target missing from PAL"
fi

if grep -q '^j _loop_end_1$' "$PAL_FILE"; then
  pass "break_loop emits forward 'j _loop_end_1'"
else
  fail "break_loop jump missing from PAL"
fi

if grep -q 'bne x12, x2, _else_' "$PAL_FILE"; then
  pass "the loop's Conditional Branch compiles to a real bne on the switch GET"
else
  fail "conditional-branch bne missing inside the loop"
fi

HALT_LINES=$(grep -c '^halt$' "$PAL_FILE" 2>/dev/null || echo 0)
if [ "$HALT_LINES" -ge "1" ]; then
  pass "exit_event emits real 'halt' (found $HALT_LINES halt lines, incl. the page trailer)"
else
  fail "no halt emitted by exit_event"
fi
FIRST_HALT=$(grep -n '^halt$' "$PAL_FILE" | head -1 | cut -d: -f1)
LEVEL_ECALL=$(grep -n 'actor_1_stats.txt" "level"' "$PAL_FILE" | head -1 | cut -d: -f1)
if [ -n "$FIRST_HALT" ] && [ -n "$LEVEL_ECALL" ] && [ "$FIRST_HALT" -lt "$LEVEL_ECALL" ]; then
  pass "exit_event's halt is emitted BEFORE the tail change_level (blocks it at runtime)"
else
  fail "exit_event halt order wrong vs the skipped tail (halt line $FIRST_HALT, level ecall $LEVEL_ECALL)"
fi

if grep -q '^sleep 25000$' "$PAL_FILE"; then
  pass "wait 25ms emits real 'sleep 25000'"
else
  fail "wait sleep emission missing"
fi

if grep -q 'flow-control-harness' "$PAL_FILE"; then
  fail "comment text leaked into event.pal (comment must be a compile no-op)"
else
  pass "comment emits nothing (no-op; text absent from event.pal)"
fi

if grep -q '^li x15, 7$' "$PAL_FILE" && grep -q 'switches.txt' "$PAL_FILE"; then
  pass "control_switch emits real SET_KV_INT ecalls against switches.txt"
else
  fail "control_switch SET ecalls missing"
fi

cp "$PAL_FILE" "$RESULTS/event.pal.txt"

# =========================================================================
log "=== step 4: runtime proof - run the compiled bytecode under prisc+x ==="
(
  cd "$PRISC_CWD" || exit 1
  "$PRISC" "$PAL_FILE" >> "$RESULTS/prisc.log" 2>&1
)
PRISC_RC=$?
copy_rc=0
if [ "$PRISC_RC" = "0" ]; then
  pass "prisc+x ran the compiled event to halt (exit 0)"
else
  fail "prisc+x exit $PRISC_RC (non-zero)"
  copy_rc=1
fi
cat "$RESULTS/prisc.log" >> "$RESULTS/log.txt"

# =========================================================================
log "=== step 5: text-state-dump verification ==="

if grep -q '^item_tick=2$' "$SAND/items.txt" 2>/dev/null; then
  pass "item_tick=2: loop body truly executed TWICE (backward edge traversed exactly once)"
else
  fail "item_tick expected 2, got '$(cat "$SAND/items.txt" 2>/dev/null)'"
fi

if grep -q '^FC_LOOP_GATE=1$' "$SAND/switches.txt" 2>/dev/null; then
  pass "FC_LOOP_GATE=1: 1st iteration took the else branch, 2nd broke (terminating loop)"
else
  fail "FC_LOOP_GATE expected 1, got '$(cat "$SAND/switches.txt" 2>/dev/null)'"
fi

if grep -q '^FC_PROVED_JUMP=1$' "$SAND/switches.txt" 2>/dev/null; then
  pass "FC_PROVED_JUMP=1: Label/Jump to Label landed (execution reached user_MID)"
else
  fail "FC_PROVED_JUMP missing; label/jump_to_label never arrived"
fi

if grep -q '^level=1$' "$SAND/actor_1_stats.txt" 2>/dev/null &&
   ! grep -q '=6' "$SAND/actor_1_stats.txt" 2>/dev/null; then
  pass "level stays 1: exit_event halted BEFORE change_level (+5 never applied)"
else
  fail "level changed! exit_event failed to halt: '$(cat "$SAND/actor_1_stats.txt" 2>/dev/null)'"
fi

if ! grep -q 'should_not_exist' "$SAND/items.txt" 2>/dev/null; then
  pass "no should_not_exist key: the tail change_items after exit_event never ran"
else
  fail "post-exit_event change_items leaked into items.txt"
fi

cp "$SAND/switches.txt"      "$RESULTS/01_switches.txt"     2>/dev/null
cp "$SAND/items.txt"         "$RESULTS/02_items.txt"        2>/dev/null
cp "$SAND/actor_1_stats.txt" "$RESULTS/03_actor_1_stats.txt" 2>/dev/null
log "state dumps copied into $RESULTS"

echo "" >> "$SUMMARY"
echo "harness done TS=$TS - see $RESULTS" >> "$SUMMARY"
echo "$SUMMARY"