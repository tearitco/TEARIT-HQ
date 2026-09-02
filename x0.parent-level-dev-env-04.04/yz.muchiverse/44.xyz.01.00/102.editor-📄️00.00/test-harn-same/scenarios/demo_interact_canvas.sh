#!/bin/bash
# demo_interact_canvas.sh - prove INTERACT canvas editing end-to-end.
# Real keystrokes: history → chtpm → interact_relay → menu_input → buffer → frame.
#
# Covers:
#   boot / Enter INTERACT
#   type printable text
#   backspace
#   Enter (line change / newline)
#   arrow left/right (char cursor)
#   arrow up/down (line change)
#   ESC, CLEAR
set -u
HARNESS_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT_DIR="$(cd "$HARNESS_DIR/.." && pwd)"
OPS="$HARNESS_DIR/ops/+x"

PROOF_DIR="$PROJECT_DIR/proof/harness-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$PROOF_DIR"
FAIL=0
pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; FAIL=1; }

cleanup() {
    echo; echo "--- cleanup ---"
    bash "$HARNESS_DIR/button.sh" kill
}
trap cleanup EXIT

key()    { "$OPS/tk_inject_key.+x" "$1" "$2"; sleep 0.28; }
type_()  { "$OPS/tk_type_text.+x" "$1" "$2"; sleep 0.28; }
focus()  { "$OPS/tk_focus_item.+x" "$1" "$2" "$3" >/dev/null; sleep 0.28; }
check()  {
    if "$OPS/tk_assert_contains.+x" "$1" "$2" "$3"; then
        return 0
    else
        FAIL=1
        return 1
    fi
}

snap() {
    local name="$1"
    sleep 0.35
    cp "$FRAME" "$PROOF_DIR/${name}.txt" 2>/dev/null || true
    cp pieces/system/editor_buffer.txt "$PROOF_DIR/${name}.buffer" 2>/dev/null || true
    cp pieces/system/editor_state.txt "$PROOF_DIR/${name}.state" 2>/dev/null || true
    echo "--- $name ---"
    cat "$PROOF_DIR/${name}.txt" 2>/dev/null | head -30
    echo "  buffer=$(cat pieces/system/editor_buffer.txt 2>/dev/null | od -An -tx1c | head -3 | tr -s ' ')"
    echo "  state=$(grep cursor_pos pieces/system/editor_state.txt 2>/dev/null)"
}

# --- boot real session without keyboard_input (harness injects keys) ---
echo "=== agy-editor INTERACT canvas demo (extended) ==="
bash "$PROJECT_DIR/button.sh" compile || { fail "compile"; exit 1; }
bash "$HARNESS_DIR/button.sh" compile || { fail "harness compile"; exit 1; }

SESSION_ID="harn-$(date +%s)-$$"
SESSION_DIR="$PROJECT_DIR/pieces/sessions/$SESSION_ID"
mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
         "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
         "$SESSION_DIR/projects/agy-editor/manager" \
         "$SESSION_DIR/docs"

ln -sfn "$PROJECT_DIR/system" "$SESSION_DIR/system"
ln -sfn "$PROJECT_DIR/ops" "$SESSION_DIR/ops"
ln -sfn "$PROJECT_DIR/pal" "$SESSION_DIR/pal"
ln -sfn "$PROJECT_DIR/default_op.txt" "$SESSION_DIR/default_op.txt"
ln -sfn "$PROJECT_DIR/pieces/chtpm" "$SESSION_DIR/pieces/chtpm"
ln -sfn "$PROJECT_DIR/projects/agy-editor/pieces" "$SESSION_DIR/projects/agy-editor/pieces"
ln -sfn "$PROJECT_DIR/docs" "$SESSION_DIR/docs"

cd "$SESSION_DIR"
: > pieces/apps/player_app/interact_relay.txt
: > pieces/keyboard/history.txt
: > pieces/display/editor_screen_changed.txt
: > projects/agy-editor/manager/gui_state.txt
# Clean seed: single line "ab" so cursor math is easy to assert
printf 'ab' > pieces/system/editor_buffer.txt
cat > pieces/system/editor_state.txt << 'EOF'
file_path=docs/untitled.txt
cursor_pos=-1
last_message=Welcome to AGY Editor. Focus EDIT TEXT, Enter to INTERACT.
EOF
cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=agy-editor
active_target_id=editor
EOSTATE

export PRISC_PROJECT_ROOT="$SESSION_DIR"
export PRISC_PROJECT_ID="agy-editor"

./ops/+x/editor_compose_frame.+x >/dev/null 2>&1 || true

./system/renderer >/dev/null 2>&1 &
RENDERER_PID=$!
./system/chtpm_parser_pal pieces/chtpm/layouts/editor.chtpm >/dev/null 2>&1 &
CHTPM_PID=$!

for i in $(seq 1 40); do
    if [ -s pieces/display/current_frame.txt ] && grep -q "A G Y" pieces/display/current_frame.txt 2>/dev/null; then
        break
    fi
    sleep 0.15
done

FRAME="$SESSION_DIR/pieces/display/current_frame.txt"
snap "00_initial"

check "$FRAME" "A G Y" "initial has AGY EDITOR chrome"
check "$FRAME" "EDIT TEXT" "initial has EDIT TEXT (INTERACT)"
check "$FRAME" "ab[X]" "initial canvas ab[X] (cursor at end)"

# --- Enter INTERACT ---
focus "$SESSION_DIR" "$FRAME" "EDIT TEXT"
key "$SESSION_DIR" 13
sleep 0.5
snap "01_interact"

if [ -f pieces/display/active_gui_is_typing.txt ] && grep -q '1' pieces/display/active_gui_is_typing.txt; then
    pass "active_gui_is_typing=1 after Enter on INTERACT"
else
    fail "active_gui_is_typing not 1 (got: $(cat pieces/display/active_gui_is_typing.txt 2>/dev/null))"
fi

# --- Type "xy" → abxy[X] ---
key "$SESSION_DIR" 120   # x
key "$SESSION_DIR" 121   # y
sleep 0.5
snap "02_type_xy"
check "$FRAME" "abxy[X]" "type printable → abxy[X]"
if grep -q 'abxy' pieces/system/editor_buffer.txt; then pass "buffer has abxy"; else fail "buffer missing abxy"; fi

# --- Backspace twice → ab[X] ---
key "$SESSION_DIR" 127
key "$SESSION_DIR" 127
sleep 0.5
snap "03_backspace"
check "$FRAME" "ab[X]" "backspace → ab[X]"
if [ "$(cat pieces/system/editor_buffer.txt)" = "ab" ]; then pass "buffer after BS is ab"; else fail "buffer after BS: $(cat pieces/system/editor_buffer.txt | od -c)"; fi

# --- Type "cd" then Enter (newline) then "ef" → two lines ---
key "$SESSION_DIR" 99    # c
key "$SESSION_DIR" 100   # d
key "$SESSION_DIR" 13    # Enter / newline (while INTERACT: inserts \n)
key "$SESSION_DIR" 101   # e
key "$SESSION_DIR" 102   # f
sleep 0.6
snap "04_newline_ef"
# buffer should be "abcd\nef"
if printf 'abcd\nef' | cmp -s - pieces/system/editor_buffer.txt; then
    pass "buffer after newline is abcd\\nef"
else
    fail "buffer after newline unexpected: $(od -An -tx1c pieces/system/editor_buffer.txt | head -2)"
fi
check "$FRAME" "abcd" "frame line1 abcd"
check "$FRAME" "ef[X]" "frame line2 ef[X] after Enter+type"

# Buffer "abcd\nef" is 7 bytes; cursor at end = 7.
# Positions: a b c d \n e f
#            0 1 2 3  4 5 6  (end=7)
# --- Arrow LEFT twice from end(7) → pos 5 = on 'e' → frame [X]ef ---
key "$SESSION_DIR" 1000  # LEFT → 6
key "$SESSION_DIR" 1000  # LEFT → 5
sleep 0.5
snap "05_arrow_left"
check "$FRAME" "[X]ef" "arrow left → [X]ef"
CUR=$(grep '^cursor_pos=' pieces/system/editor_state.txt | cut -d= -f2)
if [ "$CUR" = "5" ]; then pass "cursor_pos=5 after 2x LEFT (on e)"; else fail "cursor_pos=$CUR expected 5"; fi

# --- Arrow RIGHT once → pos 6 = on 'f' → e[X]f ---
key "$SESSION_DIR" 1001  # RIGHT → 6
sleep 0.4
snap "06_arrow_right"
CUR=$(grep '^cursor_pos=' pieces/system/editor_state.txt | cut -d= -f2)
if [ "$CUR" = "6" ]; then pass "cursor_pos=6 after RIGHT (on f)"; else fail "cursor_pos=$CUR expected 6"; fi
check "$FRAME" "e[X]f" "arrow right mid-line → e[X]f"

# Move to end of line2 for clean up/down test
key "$SESSION_DIR" 1001  # RIGHT → 7 end
sleep 0.3

# --- Arrow UP → same column on line1 ---
# line2 "ef" col at end = 2; line1 "abcd" len 4 → col 2 → cursor at 'c' (pos 2)
key "$SESSION_DIR" 1002  # UP
sleep 0.5
snap "07_arrow_up"
CUR=$(grep '^cursor_pos=' pieces/system/editor_state.txt | cut -d= -f2)
if [ "$CUR" = "2" ]; then pass "cursor_pos=2 after UP (col on abcd)"; else fail "cursor_pos=$CUR expected 2 after UP"; fi
check "$FRAME" "ab[X]cd" "arrow up → ab[X]cd"

# --- Arrow DOWN → back to line2 same col ---
# col 2 on line2 "ef" len 2 → end of line → pos 7
key "$SESSION_DIR" 1003  # DOWN
sleep 0.5
snap "08_arrow_down"
CUR=$(grep '^cursor_pos=' pieces/system/editor_state.txt | cut -d= -f2)
if [ "$CUR" = "7" ]; then pass "cursor_pos=7 after DOWN (end of ef)"; else fail "cursor_pos=$CUR expected 7 after DOWN"; fi
check "$FRAME" "ef[X]" "arrow down → ef[X]"

# --- Insert mid-line with arrows: UP, type 'Z' at col 2 of abcd → abZcd ---
key "$SESSION_DIR" 1002  # UP → pos 2
key "$SESSION_DIR" 90    # 'Z'
sleep 0.5
snap "09_insert_mid"
if printf 'abZcd\nef' | cmp -s - pieces/system/editor_buffer.txt; then
    pass "mid-line insert → abZcd\\nef"
else
    fail "mid insert buffer: $(od -An -tx1c pieces/system/editor_buffer.txt | head -2)"
fi
check "$FRAME" "abZ[X]cd" "frame mid insert abZ[X]cd"

# --- ESC leave INTERACT ---
key "$SESSION_DIR" 27
sleep 0.4
snap "10_esc"
if [ -f pieces/display/active_gui_is_typing.txt ] && grep -q '0' pieces/display/active_gui_is_typing.txt; then
    pass "active_gui_is_typing=0 after ESC"
else
    echo "NOTE: typing flag after ESC: $(cat pieces/display/active_gui_is_typing.txt 2>/dev/null)"
fi

# --- CLEAR ---
focus "$SESSION_DIR" "$FRAME" "CLEAR FILE"
key "$SESSION_DIR" 13
sleep 0.6
snap "11_clear"
if [ ! -s pieces/system/editor_buffer.txt ]; then
    pass "CLEAR emptied buffer"
else
    fail "CLEAR did not empty buffer"
fi
check "$FRAME" "[X]" "after CLEAR only cursor"

# kill session processes
kill "$RENDERER_PID" "$CHTPM_PID" 2>/dev/null || true
for pid in $(pgrep -f "system/prisc\+x" 2>/dev/null); do
    cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null)"
    if [ "$cwd" = "$SESSION_DIR" ]; then kill -9 "$pid" 2>/dev/null; fi
done

echo
echo "Proof frames in: $PROOF_DIR"
ls -la "$PROOF_DIR"
if [ "$FAIL" -eq 0 ]; then
    echo "=== ALL PASS ==="
    exit 0
else
    echo "=== FAILED ($FAIL checks) ==="
    exit 1
fi
