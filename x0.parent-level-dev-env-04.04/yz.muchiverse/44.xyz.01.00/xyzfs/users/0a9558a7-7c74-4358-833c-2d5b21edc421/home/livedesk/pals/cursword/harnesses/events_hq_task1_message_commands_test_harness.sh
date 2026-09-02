#!/bin/bash
# events_hq_task1_message_commands_test_harness.sh — Task 1 (Message +
# Character commands): real, text-verifiable end-to-end proof.
#
# Proves, for each new command, the FULL real chain the task demands:
#   IR node (event.ir.pdl) -> khtpm_events_hq_manager compile_page()
#   -> event.pal bytecode + cmd_N.sh wrappers -> prisc+x executes
#   -> the real mr_* op writes REAL state (variables.txt /
#   character_state.pdl / input_number_result.txt / choice_result.txt /
#   history.txt / messages.txt / relay) -> text-state-dump verification.
#
# Also proves the gap-#0 DESIGN (CURSWORD-SOUL-VISION.md §4): a blocking
# message op pauses the EXISTING game clock via lc_clock `ticker <id>
# off` (running=0 in #.desktop/clocks/<id>.pdl) while the blocking
# popup is open, and resumes it after. common_events_manager.c's tick
# loop is gated on the same running flag (checked in its own build).
#
# "Player" input during blocking ops is provided by an auto-picker that
# races the real result files the window's choice machinery would write
# (the same two result paths mr_* / mr_show_choices poll) - this is how
# a blocking pick is exercised headlessly, per the house text-state-dump
# convention (_.0.aigent-testing-k9.txt). No renderer, no PNG, no X11.
#
# Disclaimer by design: the sandbox entity lives UNDER the real house
# root (xyzfs walk in cmd_N.sh resolves $D -> house root there) so the
# registry-driven wrappers resolve their ops - same convention the
# cursword live entity itself uses. Game-clock pause/resume touches the
# real gameclock0000 but a trap ALWAYS restores its original running
# value.

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
  echo "task1_harness: could not find house root above $HERE" >&2
  exit 1
}

HOUSE="${HOUSE:-$(find_house_root)}"
PAL="$HOUSE/xyzfs/users/0a9558a7-7c74-4358-833c-2d5b21edc421/home/livedesk/pals/cursword"
PAL_HARNESSES="$PAL/harnesses/pal"
EVHQQPS="$HOUSE/&.widgits/events-hq/ops"
MGR="$EVHQQPS/+x/khtpm_events_hq_manager.+x"
LCBIN="$HOUSE/&.widgits/livedesk-clock/ops/+x/lc_clock.+x"
PRISC="$(ls -d "$HOUSE"/101.mutaclsym*+18.0G/system/prisc+x 2>/dev/null | head -1)"
if [ -z "$PRISC" ] || [ ! -x "$PRISC" ]; then
  PRISC="$(ls -d "$HOUSE"/101.mutaclsym*+*/system/prisc+x 2>/dev/null | head -1)"
fi
PRISC_DIR="$(dirname "$PRISC")"
# prisc+x resolves its ops file (default_op.txt) against the CWD FIRST,
# then next to the binary; the game's ops file lives at the mutaclsym
# ROOT (101.mutaclsym<ver>/default_op.txt), so prisc must run from there.
PRISC_CWD="$(dirname "$PRISC_DIR")"

GAMECLOCK="$HOUSE/#.desktop/clocks/gameclock0000.pdl"

TS=$(date '+%Y%m%d-%H%M%S')
SAND_ROOT="$PAL/harnesses/.task1_sandbox"
SAND="$SAND_ROOT/$TS"
EVENT_PKG="$SAND/event_pkg"
IR="$EVENT_PKG/pages/page_1/event.ir.pdl"
PAL_FILE="$EVENT_PKG/pages/page_1/event.pal"
MGR_ACTION="$EVENT_PKG/.hq_manager/action.txt"

RESULTS="$PAL/presentations/events-hq-task1-message-commands-$TS"
mkdir -p "$RESULTS"
SUMMARY="$RESULTS/summary.txt"

log() { echo "[$(date '+%H:%M:%S')] $*" | tee -a "$RESULTS/log.txt"; }
pass() { log "PASS: $*"; echo "PASS: $*" >> "$SUMMARY"; }
fail() { log "FAIL: $*"; echo "FAIL: $*" >> "$SUMMARY"; }

# --- cleanup + gameclock restore ALWAYS ---
ORIG_RUNNING="$(grep '^running=' "$GAMECLOCK" 2>/dev/null | head -1 | cut -d= -f2 | tr -d ' \r\n')"
[ -z "$ORIG_RUNNING" ] && ORIG_RUNNING=1
cleanup() {
  # restore the real game clock to its ORIGINAL running state no matter what
  if [ -x "$LCBIN" ] && [ -f "$GAMECLOCK" ]; then
    "$LCBIN" "$HOUSE" ticker gameclock0000 "$([ "$ORIG_RUNNING" = "1" ] && echo on || echo off)" >/dev/null 2>&1
  fi
  for pat in "khtpm_events_hq_manager\.\+x" "task1_picker" "prisc\+x"; do
    pids="$(pgrep -f "$pat" 2>/dev/null || true)"
    [ -n "$pids" ] && echo "$pids" | xargs -r kill -TERM 2>/dev/null
  done
  sleep 0.5
  [ -d "$SAND" ] && rm -rf "$SAND"
  echo "cleanup done (gameclock0000 running restored to $ORIG_RUNNING)" >> "$RESULTS/log.txt"
}
trap cleanup EXIT

# --- auto-picker: plays the "player" for blocking popups (headless) ---
# Watches the sandbox entity dir for the OBJECT files the blocking ops
# write, decides which popup type it is (digit rows vs item rows vs
# choices), and writes the pick into the SAME result file path the op is
# polling - exactly what the window's choice machinery would do.
start_picker() {
  (
    mkdir -p "$SAND"
    echo "PICKER start $(date '+%H:%M:%S')" >> "$SAND/picker_clock.log"
    while [ "${PICKER_DONE:-0}" = "0" ]; do
      if [ -f "$SAND/.mr_objects.tmp.pdl" ]; then
        kind="items"; grep -qE '^\s*OBJECT \| label=[0-9] \|' "$SAND/.mr_objects.tmp.pdl" 2>/dev/null && kind="digits"
        ans="1"
        if [ "$kind" = "digits" ]; then ans="5"; echo "PICKER digit popup -> answer 5" >> "$SAND/picker_clock.log"; fi
        [ "$kind" = "items" ] && echo "PICKER item popup -> answer 1" >> "$SAND/picker_clock.log"
        echo "$ans" > "$SAND/.mr_result.tmp.txt"
      fi
      if [ -f "$SAND/.show_choices_objects.tmp.pdl" ]; then
        echo "PICKER choices popup -> answer 1" >> "$SAND/picker_clock.log"
        echo "1" > "$SAND/.show_choices_result.tmp.txt"
      fi
      # gap-#0 observable: while a blocking message dialog is open the
      # real game clock must be paused (running=0). Record when seen.
      if [ -f "$GAMECLOCK" ]; then
        r="$(grep '^running=' "$GAMECLOCK" | head -1 | cut -d= -f2 | tr -d ' \r\n')"
        [ "$r" = "0" ] && echo "CLOCK_OBSERVED running=0 (blocking dialog open, clock paused)" >> "$SAND/picker_clock.log"
      fi
      sleep 0.05
    done
  ) &
  PICKER_PID=$!
  export PICKER_PID
}

stop_picker() { PICKER_DONE=1; sleep 0.3; kill "$PICKER_PID" 2>/dev/null; }

send_action() {
  echo "$1" > "$MGR_ACTION"
  log "action sent: $1"
  sleep 1.6
}

# =========================================================================
log "=== step 0: preflight ==="
if [ -z "$PRISC" ] || [ ! -x "$PRISC" ]; then
  fail "prisc+x not found; cannot run bytecode"
  exit 1
fi
if [ ! -x "$MGR" ]; then
  fail "manager binary not found: $MGR"
  exit 1
fi
if [ ! -x "$LCBIN" ]; then
  fail "lc_clock binary not found: $LCBIN"
  exit 1
fi
log "prisc=$PRISC"

log "=== step 0: clean stray processes ==="
for pat in "khtpm_events_hq_manager\.\+x" "prisc\+x"; do
  existing="$(pgrep -f "$pat" 2>/dev/null || true)"
  if [ -n "$existing" ]; then
    echo "$existing" | xargs -r kill -KILL 2>/dev/null
    sleep 0.5
  fi
done

# =========================================================================
log "=== step 1: build sandbox package (under house root for \$D walk) ==="
mkdir -p "$EVENT_PKG/pages/page_1" "$MGR_ACTION" 2>/dev/null
rmdir "$MGR_ACTION" 2>/dev/null
mkdir -p "$EVENT_PKG/.hq_manager"

# real entity-level state the ops read (inventory for select_item).
# show_text (node 1) passes a real FILE path, not literal text - so a
# greeting.txt is created in the same entity dir the wrapper cds into.
cat > "$SAND/inventory.txt" <<'EOF'
qolq=12
sword=1
shield=1
potion=3
EOF
printf 'Sandbox greeting\n' > "$SAND/greeting.txt"

# IR with every Task 1 command EXCEPT the last (erase_event) - erase_event
# is appended through the REAL manager append: action to force a full
# recompile, proving the no-param command flows through the live path too.
cat > "$IR" <<'EOF'
SECTION      | KEY                | VALUE
----------------------------------------
META         | piece_id           | task1-sandbox
STATE        | source             | events-hq
NODE         | id=1 type=show_text | text=greeting.txt
NODE         | id=2 type=show_choices | choices=Fire,Ice,Volt|default=2
NODE         | id=3 type=input_number | var_name=hp|digits=1
NODE         | id=4 type=select_item | var_name=pick_idx
NODE         | id=5 type=scrolling_text | text=Room 5 is aflame
NODE         | id=6 type=change_transparency | value=on
NODE         | id=7 type=followers | value=hide
NODE         | id=8 type=show_animation | animation_id=spin
EOF

# =========================================================================
log "=== step 2: launch events-hq manager (standalone, real house root) ==="
setsid nohup "$MGR" "$HOUSE" "$EVENT_PKG" "task1-sandbox" >"$RESULTS/mgr.log" 2>&1 </dev/null &
disown 2>/dev/null || true
sleep 2
pids="$(pgrep -f 'khtpm_events_hq_manager\.\+x' 2>/dev/null | grep -c . || true)"
if [ "$pids" != "1" ]; then
  fail "expected exactly 1 manager process, got $pids"
  cat "$RESULTS/mgr.log" 2>/dev/null
  exit 1
fi
pass "manager launched"

# =========================================================================
log "=== step 3: append erase_event via the REAL append: action (forces full recompile) ==="
send_action "append:erase_event|"

NODES=$(grep -c '^NODE' "$IR" 2>/dev/null || echo 0)
if [ "$NODES" = "9" ]; then
  pass "IR has 9 NODEs after append (show_text, show_choices, input_number, select_item, scrolling_text, transparency, followers, animation, erase)"
else
  fail "IR expected 9 NODEs, got $NODES"
fi

# =========================================================================
log "=== step 4: compile proof - event.pal + cmd_N.sh wrappers ==="
if [ ! -f "$PAL_FILE" ]; then
  fail "event.pal not created"
  exit 1
else
  pass "event.pal compiled"
  log "event.pal:"
  cat "$PAL_FILE" >> "$RESULTS/log.txt"
  cat "$PAL_FILE"
fi

for t in show_text show_choices input_number select_item scrolling_text \
         change_transparency followers show_animation erase_event; do
  if grep -q "exec cmd_" "$PAL_FILE" && grep -q "$t" "$IR"; then
    log "IR node for $t present"
  fi
done

blocks=$(for f in "$EVENT_PKG/pages/page_1"/cmd_*.sh; do [ -f "$f" ] && basename "$f"; done | sort -u)
log "cmd_N.sh wrappers: $blocks"
count=$(echo "$blocks" | grep -c '^cmd_' || true)
if [ "$count" = "9" ]; then
  pass "9 cmd_N.sh wrappers generated (one per node)"
else
  fail "expected 9 wrappers, got $count"
fi

# 2/4/3/4/4 are the hard-arg commands - confirm the wrapper bodies route
# to the NEW ops with the real args substituted.
grep -H 'mr_input_number' "$EVENT_PKG/pages/page_1"/cmd_*.sh | sed "s|.*cmd_|cmd_|" >> "$RESULTS/log.txt"
if grep -l 'mr_input_number' "$EVENT_PKG/pages/page_1"/cmd_*.sh >/dev/null 2>&1 &&
   grep -l 'mr_select_item' "$EVENT_PKG/pages/page_1"/cmd_*.sh >/dev/null 2>&1 &&
   grep -l 'mr_scrolling_text' "$EVENT_PKG/pages/page_1"/cmd_*.sh >/dev/null 2>&1 &&
   grep -l 'mr_character' "$EVENT_PKG/pages/page_1"/cmd_*.sh >/dev/null 2>&1; then
  pass "wrappers call the NEW ops (mr_input_number/mr_select_item/mr_scrolling_text/mr_character)"
else
  fail "wrapper op routing incorrect"
fi

cp "$PAL_FILE" "$RESULTS/event.pal.txt"
for f in "$EVENT_PKG/pages/page_1"/cmd_*.sh; do
  [ -f "$f" ] && { echo "### $f"; cat "$f"; } >> "$RESULTS/log.txt"
done

# =========================================================================
log "=== step 5: runtime proof - run the compiled bytecode under prisc+x ==="
start_picker
cp "$IR" "${IR}.bak"
(
  cd "$PRISC_CWD" || exit 1
  "$PRISC" "$PAL_FILE" >> "$RESULTS/prisc.log" 2>&1
)
PRISC_RC=$?
stop_picker

if [ "$PRISC_RC" = "0" ]; then
  pass "prisc+x ran the compiled event to halt (exit 0)"
else
  fail "prisc+x exit $PRISC_RC"
fi
cat "$RESULTS/prisc.log" >> "$RESULTS/log.txt"
cp "$SAND/picker_clock.log" "$RESULTS/picker_clock.log"

# =========================================================================
log "=== step 6: text-state-dump verification ==="
PASS=0; FAILS=0

# 6a: variables.txt got input_number (5) and select_item (1)
if grep -q '^hp=5' "$SAND/variables.txt" 2>/dev/null; then
  pass "input_number stored hp=5 in variables.txt"; PASS=1
else
  fail "input_number variable not stored (hp=5) in variables.txt"; FAILS=1
fi
if grep -q '^pick_idx=1' "$SAND/variables.txt" 2>/dev/null; then
  pass "select_item stored pick_idx=1 in variables.txt"
else
  fail "select_item variable not stored"
fi

# 6b: choice_result.txt from show_choices
if [ -f "$SAND/choice_result.txt" ] && grep -q 'choice_result=1' "$SAND/choice_result.txt" 2>/dev/null; then
  pass "show_choices picked index 1 (choice_result=1)"
else
  fail "show_choices result missing/incorrect"
fi

# 6c: input_number meta
if [ -f "$SAND/input_number_result.txt" ] && grep -q 'input_number_value=5' "$SAND/input_number_result.txt" 2>/dev/null; then
  pass "input_number_result.txt value=5"
else
  fail "input_number_result.txt missing/incorrect"
fi

# 6d: select_item meta (picked index 1 = second inventory row = sword
# after header line qolq=12, per mr_select_item's key/row parsing)
if [ -f "$SAND/.select_item_result.txt" ] && grep -q 'select_item_value=1' "$SAND/.select_item_result.txt" 2>/dev/null && grep -q 'select_item_key=sword' "$SAND/.select_item_result.txt" 2>/dev/null; then
  pass "select_item_result key=sword"
else
  fail "select_item_result.txt missing/incorrect"
fi

# 6e: character_state.pdl — all four character commands
chk=""
for kv in "transparency=on" "followers=hide" "animation=spin" "erase=1"; do
  grep -q "^$kv$" "$SAND/character_state.pdl" 2>/dev/null && chk="$chk $kv"
done
if [ "$chk" = " transparency=on followers=hide animation=spin erase=1" ]; then
  pass "character_state.pdl: all 4 character commands recorded ($chk)"
else
  fail "character_state.pdl expected transparency/followers/animation/erase rows, got '$(cat "$SAND/character_state.pdl" 2>/dev/null)'"
fi

# 6f: show_text + scrolling_text relay artifacts
if [ -f "$SAND/.scrolling_text.tmp.txt" ] && grep -q 'Room 5 is aflame' "$SAND/.scrolling_text.tmp.txt"; then
  pass "scrolling_text wrote its text file"
else
  fail "scrolling_text text file missing/wrong"
fi
RELAY_LAST="$(tail -c 1024 "$SAND/interact_relay.txt" 2>/dev/null | tr -d '\0')"
if echo "$RELAY_LAST" | grep -q 'SHOW_TEXT_FILE:'; then
  pass "interact_relay.txt carries a SHOW_TEXT_FILE popup relay line"
else
  fail "interact_relay.txt missing SHOW_TEXT_FILE relay"
fi

# 6g: audit ledger (messages.txt + history.txt)
for need in "SHOW_CHOICES result=1" "INPUT_NUMBER var=hp" "SELECT_ITEM var=pick_idx" "SCROLLING_TEXT queued" "CHARACTER transparency=on" "CHARACTER erase=1"; do
  if grep -qF "$need" "$SAND/history.txt" 2>/dev/null; then
    pass "history.txt audit: $need"
  else
    fail "history.txt missing: $need"
  fi
done

# 6h: gap-#0 — blocking dialog paused the real game clock, then resumed
if grep -q 'CLOCK_OBSERVED running=0' "$SAND/picker_clock.log" 2>/dev/null; then
  pass "gap-#0: game clock observed running=0 while a blocking dialog was open"
else
  fail "gap-#0: clock pause NOT observed during blocking dialog"
fi
NOW_RUNNING="$(grep '^running=' "$GAMECLOCK" | head -1 | cut -d= -f2 | tr -d ' \r\n')"
if [ "$NOW_RUNNING" = "1" ]; then
  pass "game clock resumed (running=1 after event finished)"
else
  fail "game clock NOT resumed (running=$NOW_RUNNING)"
fi

# 6i: state dumps + presentation copies
cp "$SAND/variables.txt" "$RESULTS/01_variables.txt" 2>/dev/null
cp "$SAND/character_state.pdl" "$RESULTS/02_character_state.pdl" 2>/dev/null
cp "$SAND/choice_result.txt" "$RESULTS/03_choice_result.txt" 2>/dev/null
cp "$SAND/input_number_result.txt" "$RESULTS/04_input_number_result.txt" 2>/dev/null
cp "$SAND/.select_item_result.txt" "$RESULTS/05_select_item_result.txt" 2>/dev/null
cp "$SAND/interact_relay.txt" "$RESULTS/06_interact_relay.txt" 2>/dev/null
cp "$SAND/.scrolling_text.tmp.txt" "$RESULTS/07_scrolling_text.txt" 2>/dev/null
cp "$SAND/history.txt" "$RESULTS/08_history.txt" 2>/dev/null
log "state dumps copied into $RESULTS"

# stop the manager
pids="$(pgrep -f 'khtpm_events_hq_manager\.\+x' 2>/dev/null || true)"
[ -n "$pids" ] && echo "$pids" | xargs -r kill -TERM 2>/dev/null
sleep 1

echo "" >> "$SUMMARY"
echo "harness done TS=$TS - see $RESULTS" >> "$SUMMARY"
echo "$SUMMARY"