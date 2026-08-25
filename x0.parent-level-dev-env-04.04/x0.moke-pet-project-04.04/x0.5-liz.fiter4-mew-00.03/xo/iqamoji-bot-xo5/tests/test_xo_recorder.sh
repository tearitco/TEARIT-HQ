#!/bin/bash
# test_xo_recorder.sh - Verify XO Bot Recording Infrastructure (Exo-Sovereign Edition)
# Path: 1.TPMOS_c_+rmmp_84.05/xo/bot5/tests/test_xo_recorder.sh

PROJECT_ROOT="1.TPMOS_c_+rmmp_84.05"
BOT_ROOT="$PROJECT_ROOT/xo/bot5"
REC_SRC="$BOT_ROOT/cognition/record_session.c"
REC_BIN="$BOT_ROOT/cognition/+x/record_session.+x"
PIECES_ROOT="$BOT_ROOT/pieces"

echo "=== XO RECORDER TEST (EXO-SOVEREIGN) ==="

# 1. Compile Op
mkdir -p "$BOT_ROOT/cognition/+x"
gcc -DMAX_CMD=16384 -o "$REC_BIN" "$REC_SRC"
if [ $? -eq 0 ]; then
    echo "✓ Compilation successful"
else
    echo "✗ Compilation FAILED"
    exit 1
fi

# 2. Mock Environment (in system pieces)
mkdir -p "$PROJECT_ROOT/pieces/display"
mkdir -p "$PROJECT_ROOT/pieces/keyboard"
mkdir -p "$PIECES_ROOT/memories"
echo "Mock Frame 1" > "$PROJECT_ROOT/pieces/display/current_frame.txt"
echo "[2026-04-22 10:00:00] KEY_PRESSED: 10" > "$PROJECT_ROOT/pieces/keyboard/history.txt"

# 3. Run Recording (5 seconds)
echo "Running recorder for 5 seconds..."
"$REC_BIN" "$PROJECT_ROOT" &
REC_PID=$!

sleep 2
echo "Injecting new frame and key..."
echo "Mock Frame 2" > "$PROJECT_ROOT/pieces/display/current_frame.txt"
echo "[2026-04-22 10:00:02] KEY_PRESSED: 13" >> "$PROJECT_ROOT/pieces/keyboard/history.txt"
sleep 3

kill -SIGINT $REC_PID
wait $REC_PID

# 4. Verify Artifacts
MEM_DIR=$(ls -dt $PIECES_ROOT/memories/rec_* | head -1)
OLD_NAME=$(basename "$MEM_DIR")
NEW_NAME="test_memory_goal"

echo "Checking results in $MEM_DIR..."

if [ -f "$MEM_DIR/session_complete.txt" ]; then
    echo "✓ Session complete marker found"
else
    echo "✗ Session complete marker MISSING"
fi

if [ -f "$MEM_DIR/input_log.txt" ]; then
    echo "✓ Input log found:"
    cat "$MEM_DIR/input_log.txt"
else
    echo "✗ Input log MISSING"
fi

# 5. Test Renaming
echo "=== Testing Renaming Op ==="
REN_SRC="$BOT_ROOT/cognition/rename_memory.c"
REN_BIN="$BOT_ROOT/cognition/+x/rename_memory.+x"

gcc -o "$REN_BIN" "$REN_SRC"
"$REN_BIN" "$PROJECT_ROOT" "$OLD_NAME" "$NEW_NAME"

if [ -d "$PIECES_ROOT/memories/$NEW_NAME" ]; then
    echo "✓ Renaming successful"
else
    echo "✗ Renaming FAILED"
fi

if grep -q "$NEW_NAME" "$PIECES_ROOT/memory_map.kvp"; then
    echo "✓ Registry entry found"
else
    echo "✗ Registry entry MISSING"
fi

echo "=== TEST COMPLETE ==="
