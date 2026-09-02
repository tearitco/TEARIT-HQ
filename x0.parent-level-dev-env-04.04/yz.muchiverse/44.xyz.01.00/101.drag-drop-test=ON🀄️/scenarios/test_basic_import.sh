#!/bin/bash
# test_basic_import.sh - Basic drag-drop import test scenario
#
# This scenario:
# 1. Launches mutaclsym (with gl_mirror) and muchi-pals (with egg_window)
# 2. Positions windows via config file
# 3. Waits for windows to update
# 4. Simulates drag-drop
# 5. Verifies pet was imported
# 6. Cleans up
set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HARNESS_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
OPS_X="$HARNESS_DIR/ops/+x"

# Project directories
PROJECT_ROOT="$(cd "$HARNESS_DIR/.." && pwd)"
MUTACLSYM_DIR="$PROJECT_ROOT/101.mutaclsym🧟‍♂️️+18.00"
MUCHIPALS_DIR="$PROJECT_ROOT/01.muchi-pals-🥚️-13.01"
EXCHANGE_DIR="$PROJECT_ROOT/exchange"
CONFIG_FILE="$PROJECT_ROOT/drag_drop_test.pdl"

# Test config
PET_ID="egg_1"
GL_MIRROR_NAME="mutaclsym RGB mirror"
EGG_WINDOW_NAME="pet $PET_ID"

# Position config
GL_X=100
GL_Y=100
EGG_X=800
EGG_Y=100

# Drag config
DRAG_START_X=840
DRAG_START_Y=140
DRAG_END_X=420
DRAG_END_Y=202

PASS_COUNT=0
FAIL_COUNT=0

pass() {
    echo "PASS: $1"
    PASS_COUNT=$((PASS_COUNT + 1))
}

fail() {
    echo "FAIL: $1"
    FAIL_COUNT=$((FAIL_COUNT + 1))
}

cleanup() {
    echo ""
    echo "Cleaning up..."
    bash "$HARNESS_DIR/button.sh" kill >/dev/null 2>&1
    echo "Test complete: $PASS_COUNT passed, $FAIL_COUNT failed"
}

trap cleanup EXIT

echo "=== Drag-Drop Import Test ==="
echo ""

# Step 1: Compile ops if needed
echo "Step 1: Compiling ops..."
if [ ! -x "$OPS_X/dd_set_positions.+x" ]; then
    bash "$HARNESS_DIR/button.sh" compile
fi

# Step 2: Clean exchange directory
echo "Step 2: Cleaning exchange directory..."
rm -rf "$EXCHANGE_DIR/$PET_ID" 2>/dev/null

# Step 3: Launch mutaclsym
echo "Step 3: Launching mutaclsym..."
cd "$MUTACLSYM_DIR"
export PRISC_PROJECT_ROOT="$MUTACLSYM_DIR"
export PRISC_EXCHANGE_ROOT="$EXCHANGE_DIR"
bash button.sh run &
MUTACLSYM_PID=$!
cd "$HARNESS_DIR"

# Wait for gl_mirror to start
echo "  Waiting for gl_mirror window..."
for i in $(seq 1 30); do
    GL_WID=$("$OPS_X/dd_find_window.+x" "$GL_MIRROR_NAME" 2>/dev/null)
    if [ $? -eq 0 ] && [ -n "$GL_WID" ]; then
        echo "  Found gl_mirror: window $GL_WID"
        break
    fi
    sleep 1
done

if [ -z "$GL_WID" ]; then
    fail "gl_mirror window not found after 30 seconds"
    exit 1
fi
pass "gl_mirror launched"

# Step 4: Launch muchi-pals
echo "Step 4: Launching muchi-pals..."
cd "$MUCHIPALS_DIR"
export PRISC_PROJECT_ROOT="$MUCHIPALS_DIR"
export PRISC_PROJECT_ID="muchi-pals"
export PRISC_EXCHANGE_ROOT="$EXCHANGE_DIR"
bash button.sh run &
MUCHIPALS_PID=$!
cd "$HARNESS_DIR"

# Wait for menu to start
echo "  Waiting for muchi-pals menu..."
sleep 3

# Step 5: Open pet window
echo "Step 5: Opening pet window..."
# Find muchi-pals menu and inject key to open window
# The menu should have "Open Window" as option 1
# We'll use key injection to select it
MENU_PID=$(pgrep -f "muchi-pals.*menu" | head -1)
if [ -n "$MENU_PID" ]; then
    # Find the keyboard history file for this session
    SESSION_DIR=$(ls -dt "$MUCHIPALS_DIR/pieces/sessions/"*/ 2>/dev/null | head -1)
    if [ -n "$SESSION_DIR" ]; then
        # Inject Enter key to select "Open Window"
        echo "[$(date '+%Y-%m-%d %H:%M:%S')] KEY_PRESSED: 13" >> "$SESSION_DIR/pieces/keyboard/history.txt"
        echo "  Injected Enter key to open window"
    fi
fi

# Wait for egg_window to appear
echo "  Waiting for egg_window..."
EGG_WID=""
for i in $(seq 1 30); do
    EGG_WID=$("$OPS_X/dd_find_window.+x" "$EGG_WINDOW_NAME" 2>/dev/null)
    if [ $? -eq 0 ] && [ -n "$EGG_WID" ]; then
        echo "  Found egg_window: window $EGG_WID"
        break
    fi
    sleep 1
done

if [ -z "$EGG_WID" ]; then
    fail "egg_window not found after 30 seconds"
    exit 1
fi
pass "egg_window launched"

# Step 6: Position windows
echo "Step 6: Positioning windows..."
"$OPS_X/dd_set_positions.+x" "$CONFIG_FILE" $GL_X $GL_Y $EGG_X $EGG_Y
echo "  Waiting 2 seconds for windows to poll and update..."
sleep 2
pass "windows positioned"

# Step 7: Simulate drag-drop
echo "Step 7: Simulating drag-drop..."
"$OPS_X/dd_drag_drop.+x" $DRAG_START_X $DRAG_START_Y $DRAG_END_X $DRAG_END_Y 20 50
echo "  Waiting 3 seconds for import to complete..."
sleep 3
pass "drag-drop simulated"

# Step 8: Verify results
echo "Step 8: Verifying results..."

# Check if pet directory exists in exchange
if [ -d "$EXCHANGE_DIR/$PET_ID" ]; then
    pass "pet directory exists in exchange"
else
    fail "pet directory not found in exchange"
fi

# Check if state.txt exists
if [ -f "$EXCHANGE_DIR/$PET_ID/state.txt" ]; then
    pass "state.txt exists"
else
    fail "state.txt not found"
fi

# Check if piece.pdl exists
if [ -f "$EXCHANGE_DIR/$PET_ID/piece.pdl" ]; then
    pass "piece.pdl exists"
else
    fail "piece.pdl not found"
fi

# Check state.txt content
if [ -f "$EXCHANGE_DIR/$PET_ID/state.txt" ]; then
    if grep -q "hp" "$EXCHANGE_DIR/$PET_ID/state.txt"; then
        pass "state.txt contains hp field"
    else
        fail "state.txt missing hp field"
    fi
fi

# Step 9: Check if egg_window closed
echo "Step 9: Checking if egg_window closed..."
sleep 2
EGG_WID_AFTER=$("$OPS_X/dd_find_window.+x" "$EGG_WINDOW_NAME" 2>/dev/null)
if [ $? -ne 0 ] || [ -z "$EGG_WID_AFTER" ]; then
    pass "egg_window closed after import"
else
    fail "egg_window still open after import"
fi

echo ""
echo "=== Test Complete ==="
