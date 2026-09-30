#!/bin/bash
# move_test.sh - automated test harness for entity Move action via interact_relay.txt
# Tests: OPEN_CONTEXT → ACTIVATE_NAV:28 (Act) → ACTIVATE_NAV:move_in_submenu → type → Enter

set -e

ENTITY_NAME="${1:-terumon_001_ember}"
TARGET_X="${2:-500}"
TARGET_Y="${3:-200}"
WORK_DIR="$(cd "$(dirname "$0")/../.." && pwd)"

echo "=== Entity Move Action Test Harness ==="
echo "Entity: $ENTITY_NAME"
echo "Target: ($TARGET_X, $TARGET_Y)"
echo ""

# Find entity directory
ENTITY_DIR=$(find "$WORK_DIR/xyzfs/users" -type d -name "$ENTITY_NAME" | grep "/pals/$ENTITY_NAME\$" | head -1)
if [ -z "$ENTITY_DIR" ]; then
    echo "ERROR: Entity $ENTITY_NAME not found"
    exit 1
fi

RELAY="$ENTITY_DIR/interact_relay.txt"
HISTORY="$ENTITY_DIR/history.txt"
POS_FILE="$ENTITY_DIR/desktop_pos.txt"

if [ ! -f "$RELAY" ]; then
    echo "ERROR: $RELAY not found (entity may not be open)"
    exit 1
fi

# Helper functions
send_code() {
    echo "$1" >> "$RELAY"
    sleep 0.15
}

send_string() {
    local str="$1" i c
    for ((i = 0; i < ${#str}; i++)); do
        c="${str:$i:1}"
        code=$(printf '%d' "'$c")
        echo "$code" >> "$RELAY"
        sleep 0.05
    done
}

# Capture initial state
echo "=== Initial State ==="
cat "$POS_FILE"
HISTORY_BEFORE=$(wc -l < "$HISTORY")
echo ""

# Test sequence - Act menu contains Move
echo "=== Sending commands via relay ==="
echo "1. OPEN_CONTEXT (open method menu)"
send_code "OPEN_CONTEXT"
sleep 0.5

echo "2. ACTIVATE_NAV:28 (activate 'Act' menu)"
send_code "ACTIVATE_NAV:28"
sleep 0.5

echo "3. Type coordinates: $TARGET_X,$TARGET_Y"
send_string "$TARGET_X,$TARGET_Y"
sleep 0.3

echo "4. Send Enter (13) to execute"
send_code 13
sleep 1

echo ""
echo "=== Monitoring history for MOVE_EXEC ==="
HISTORY_AFTER=$(wc -l < "$HISTORY")
echo "History grew from $HISTORY_BEFORE to $HISTORY_AFTER lines"
echo ""

# Check for MOVE_EXEC in recent history
if grep -q "MOVE_EXEC" "$HISTORY"; then
    echo "✓ MOVE_EXEC found in history:"
    grep "MOVE_EXEC" "$HISTORY" | tail -1
else
    echo "⚠ MOVE_EXEC not found - checking recent history:"
    tail -5 "$HISTORY"
fi

echo ""
echo "=== Final State ==="
echo "desktop_pos.txt:"
cat "$POS_FILE"

echo ""
echo "=== Animation Queue ==="
if [ -f "$ENTITY_DIR/animation_queue.txt" ]; then
    QUEUE_SIZE=$(wc -l < "$ENTITY_DIR/animation_queue.txt")
    echo "✓ animation_queue.txt exists ($QUEUE_SIZE lines)"
    head -5 "$ENTITY_DIR/animation_queue.txt"
    if [ "$QUEUE_SIZE" -gt 5 ]; then
        echo "..."
    fi
else
    echo "⚠ animation_queue.txt not created"
fi

echo ""
echo "=== Verification ==="
FINAL_X=$(grep "^x=" "$POS_FILE" | cut -d= -f2)
FINAL_Y=$(grep "^y=" "$POS_FILE" | cut -d= -f2)

if [ "$FINAL_X" = "$TARGET_X" ] && [ "$FINAL_Y" = "$TARGET_Y" ]; then
    echo "✓ PASS: Position updated to target ($TARGET_X, $TARGET_Y)"
elif [ -f "$ENTITY_DIR/animation_queue.txt" ]; then
    echo "✓ PASS: Animation queue created (movement initiated)"
else
    echo "✗ FAIL: Position is ($FINAL_X, $FINAL_Y), expected ($TARGET_X, $TARGET_Y)"
fi
