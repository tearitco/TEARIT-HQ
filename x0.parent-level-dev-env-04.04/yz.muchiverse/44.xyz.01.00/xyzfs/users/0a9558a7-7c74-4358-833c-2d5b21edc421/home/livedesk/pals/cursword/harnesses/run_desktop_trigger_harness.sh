#!/bin/bash
# run_desktop_trigger_harness.sh - real, end-to-end proof of the
# desktop-entity trigger layer (EVENT-TRIGGER-LAYER-PLAN.md / PLAY-
# MODE-ENTITY-HARNESS-DESIGN.md's desktop extension), direct
# instruction: "i want to use the positions the castle and cursword
# are current at... then we will make a harness that runs the entire
# test."
#
# Deliberately does NOT reset cursword/castle to any fixed test
# position - uses whatever their real, live desktop_pos.txt says right
# now (mr_move_to_entity.+x already re-reads both live, so this just
# means: don't touch them before running). Ensures Play Mode is on
# (writing the same real state file the 1.play taskbar toggle writes,
# not a separate mechanism), ensures cursword/castle/the bridge watcher
# are alive (launching any that aren't, same real invocation shape
# proven live this session), then walks cursword to castle and checks
# for REAL evidence the whole chain fired - a genuinely new ledger
# line AND a real gameplay-menu process, not just an exit code.
#
# Usage: sh run_desktop_trigger_harness.sh
set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# Walk up from this harness's own real location
# (xyzfs/users/<uuid>/home/livedesk/pals/cursword/harnesses/) to the
# real house root - same upward-search technique play_event.sh already
# uses, not a hardcoded relative path (survives a future house move).
D="$SCRIPT_DIR"
HOUSE=""
while [ "$D" != "/" ]; do
    for cand in "$D"/101.mutaclsym*/system; do
        if [ -d "$cand" ]; then HOUSE="$D"; break 2; fi
    done
    D="$(dirname "$D")"
done
if [ -z "$HOUSE" ]; then
    echo "FAIL: could not locate house root (101.mutaclsym*/system) above $SCRIPT_DIR" >&2
    exit 1
fi

USER_UUID="0a9558a7-7c74-4358-833c-2d5b21edc421"
PALS_DIR="$HOUSE/xyzfs/users/$USER_UUID/home/livedesk/pals"
CURSWORD_DIR="$PALS_DIR/cursword"
CASTLE_DIR="$PALS_DIR/castle"
RENDER_BIN="$HOUSE/*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
WATCHER_BIN="$HOUSE/*.monads/*.livedesk-taskbar/ops/+x/khtpm_desktop_trigger_watcher.+x"
MOVE_OP="$HOUSE/&.widgits/events-hq/ops/+x/mr_move_to_entity.+x"
PLAY_STATE="$HOUSE/#.desktop/khtpm_play_mode.state.txt"
LEDGER="$HOUSE/#.desktop/master_ledger.txt"

fail() { echo "FAIL: $1" >&2; exit 1; }

echo "--- 1. Ensure Play Mode is ON ---"
if ! grep -q "^mode=on" "$PLAY_STATE" 2>/dev/null; then
    echo "mode=on" > "$PLAY_STATE"
    echo "Play Mode was off - turned on (same real state file 8.player > 1.play writes)."
else
    echo "Play Mode already on."
fi

echo "--- 2. Ensure cursword is alive ---"
if ! pgrep -f "khtpm_core_render\.\+x.*pals/cursword\$" >/dev/null; then
    PRISC_PROJECT_ROOT="$HOUSE" setsid "$RENDER_BIN" "$CURSWORD_DIR" >/tmp/harness_cursword.log 2>&1 < /dev/null &
    disown
    sleep 2
    echo "cursword launched."
else
    echo "cursword already alive."
fi

echo "--- 3. Ensure castle is alive ---"
if ! pgrep -f "khtpm_core_render\.\+x.*pals/castle\$" >/dev/null; then
    PRISC_PROJECT_ROOT="$HOUSE" setsid "$RENDER_BIN" "$CASTLE_DIR" >/tmp/harness_castle.log 2>&1 < /dev/null &
    disown
    sleep 2
    echo "castle launched."
else
    echo "castle already alive."
fi

echo "--- 4. Ensure the desktop trigger watcher is alive ---"
if ! pgrep -f "khtpm_desktop_trigger_watcher\.\+x" >/dev/null; then
    setsid "$WATCHER_BIN" "$HOUSE" >/tmp/harness_watcher.log 2>&1 < /dev/null &
    disown
    sleep 1
    echo "watcher launched."
else
    echo "watcher already alive."
fi

echo "--- 5. Real, current positions (not reset) ---"
[ -f "$CURSWORD_DIR/desktop_pos.txt" ] || fail "cursword has no desktop_pos.txt"
[ -f "$CASTLE_DIR/desktop_pos.txt" ] || fail "castle has no desktop_pos.txt"
echo "cursword: $(tr '\n' ' ' < "$CURSWORD_DIR/desktop_pos.txt")"
echo "castle:   $(tr '\n' ' ' < "$CASTLE_DIR/desktop_pos.txt")"

echo "--- 5b. Ensure a genuinely FRESH approach ---"
# Real, confirmed behavior (not a bug): the trigger is edge-triggered
# (fires on write_pos(), i.e. only when a position ACTUALLY changes),
# not level-triggered - if cursword is already sitting on castle's own
# cell from a previous run, walking "to" it again is a real 0-step
# no-op and correctly produces no new ledger line. For the harness to
# prove the trigger fresh every run, step cursword one cell AWAY first
# if it's already exactly on castle's cell, then the real move below
# genuinely re-approaches.
CX=$(awk -F= '/^x=/{print $2}' "$CURSWORD_DIR/desktop_pos.txt")
CY=$(awk -F= '/^y=/{print $2}' "$CURSWORD_DIR/desktop_pos.txt")
TX=$(awk -F= '/^x=/{print $2}' "$CASTLE_DIR/desktop_pos.txt")
TY=$(awk -F= '/^y=/{print $2}' "$CASTLE_DIR/desktop_pos.txt")
if [ "$CX" = "$TX" ] && [ "$CY" = "$TY" ]; then
    AWAY_Y=$((CY + 160))
    echo "cursword already on castle's cell - stepping away to $CX,$AWAY_Y first"
    echo "MOVE_TO:$CX,$AWAY_Y" > "$CURSWORD_DIR/interact_relay.txt"
    sleep 1
else
    echo "cursword not on castle's cell - real fresh approach, no pre-step needed"
fi

echo "--- 6. Snapshot ledger baseline ---"
BASELINE_SIZE=0
[ -f "$LEDGER" ] && BASELINE_SIZE=$(wc -c < "$LEDGER")
echo "ledger baseline: $BASELINE_SIZE bytes"

echo "--- 7. Close any pre-existing gameplay-menu process ---"
# Real, confirmed behavior (not a bug): launch_khtpm_menu() enforces a
# real single-instance convention (kills the prior instance of the
# SAME menu before relaunching) - re-firing the trigger while a
# previous gameplay-menu popup is still open replaces it in place, so
# a plain "did the process COUNT go up" check can't see a re-fire (it
# stays at 1 the whole time: old killed, new started). Real fix: close
# any leftover instance first, so baseline is always a clean 0 and any
# appearance afterward is unambiguous.
pkill -f "menu_gameplay\.chtpm" 2>/dev/null
sleep 0.3
MENU_BASELINE=$(pgrep -f "menu_gameplay\.chtpm" 2>/dev/null | wc -l)
echo "gameplay-menu processes before: $MENU_BASELINE"

echo "--- 8. Run the real move (cursword -> castle, from wherever they actually are) ---"
if ! "$MOVE_OP" "$CURSWORD_DIR" "$CASTLE_DIR" "$HOUSE"; then
    fail "mr_move_to_entity.+x did not report arrival"
fi

echo "--- 9. Check for a genuinely NEW touched_npc ledger line ---"
NEW_LEDGER=""
if [ -f "$LEDGER" ]; then
    NEW_LEDGER=$(tail -c +$((BASELINE_SIZE + 1)) "$LEDGER")
fi
echo "$NEW_LEDGER" | grep -q "touched_npc" || fail "no new touched_npc ledger line appeared"
echo "$NEW_LEDGER" | grep -q "target:castle" || fail "new ledger line doesn't target castle"
echo "New ledger line(s):"
echo "$NEW_LEDGER" | grep "touched_npc"

echo "--- 10. Poll for the real gameplay-menu process (up to 3s) ---"
FIRED=0
for i in 1 2 3 4 5 6; do
    NOW=$(pgrep -f "menu_gameplay\.chtpm" 2>/dev/null | wc -l)
    if [ "$NOW" -gt "$MENU_BASELINE" ]; then FIRED=1; break; fi
    sleep 0.5
done
[ "$FIRED" = "1" ] || fail "no new menu_gameplay.chtpm process appeared after the touch"

echo ""
echo "=== PASS ==="
echo "cursword walked to castle's real, current position, the shared"
echo "master_ledger.txt genuinely gained a touched_npc line, and the"
echo "desktop trigger watcher fired castle's real gameplay menu"
echo "automatically - full loop proven, no manual step after this"
echo "script's own single invocation."
exit 0
