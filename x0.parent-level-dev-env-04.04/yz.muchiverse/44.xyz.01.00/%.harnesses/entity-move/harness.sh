#!/bin/bash
# entity-move test harness - reproduce entity disappearance, verify fixes
# Usage: bash harness.sh [entity_name] [target_x] [target_y]

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORK_DIR="$(cd "$SCRIPT_DIR/../../" && pwd)"
HQ_CRYPTS="$WORK_DIR/$.crypts"
ENTITY_NAME="${1:-asa}"  # Default entity
TARGET_X="${2:-200}"
TARGET_Y="${3:-200}"

echo "=== Entity Move Harness ==="
echo "Entity: $ENTITY_NAME"
echo "Target: ($TARGET_X, $TARGET_Y)"
echo ""

# Find entity directory
ENTITY_DIR=$(find "$WORK_DIR/xyzfs/users" -type d -name "$ENTITY_NAME" 2>/dev/null | grep "/pals/$ENTITY_NAME\$" | head -1)
if [ -z "$ENTITY_DIR" ]; then
    echo "ERROR: Entity $ENTITY_NAME not found"
    echo "Available entities:"
    ls "$WORK_DIR/xyzfs/users/*/home/livedesk/pals/" 2>/dev/null | head -10
    exit 1
fi

echo "Entity directory: $ENTITY_DIR"
echo ""

# Get current position
echo "=== Current State ==="
if [ -f "$ENTITY_DIR/desktop_pos.txt" ]; then
    echo "Current desktop_pos.txt:"
    cat "$ENTITY_DIR/desktop_pos.txt"
else
    echo "ERROR: No desktop_pos.txt found"
    exit 1
fi
echo ""

# Test the move_entity_animated op directly
echo "=== Testing move_entity_animated.+x ==="
MOVE_OP=$(find "$WORK_DIR" -name "move_entity_animated.+x" -type f 2>/dev/null | head -1)
if [ -z "$MOVE_OP" ] || [ ! -x "$MOVE_OP" ]; then
    echo "ERROR: move_entity_animated.+x not found"
    echo "Searching for it..."
    find "$WORK_DIR" -name "*move_entity*" 2>/dev/null || echo "Not found anywhere"
    exit 1
fi

echo "Op found: $MOVE_OP"
echo "Calling: $MOVE_OP $ENTITY_DIR $TARGET_X $TARGET_Y"
$MOVE_OP "$ENTITY_DIR" $TARGET_X $TARGET_Y
echo "Op executed"
echo ""

# Check results
echo "=== After Move ==="
if [ -f "$ENTITY_DIR/desktop_pos.txt" ]; then
    echo "Updated desktop_pos.txt:"
    cat "$ENTITY_DIR/desktop_pos.txt"
else
    echo "ERROR: desktop_pos.txt missing after move!"
    exit 1
fi
echo ""

if [ -f "$ENTITY_DIR/animation_queue.txt" ]; then
    echo "animation_queue.txt created:"
    head -20 "$ENTITY_DIR/animation_queue.txt"
    QUEUE_SIZE=$(wc -l < "$ENTITY_DIR/animation_queue.txt")
    echo "... ($QUEUE_SIZE lines total)"
else
    echo "WARNING: animation_queue.txt not created"
fi
echo ""

# Verify position changed
FINAL_POS=$(grep "^x=" "$ENTITY_DIR/desktop_pos.txt" | cut -d= -f2)
if [ "$FINAL_POS" = "$TARGET_X" ]; then
    echo "✓ Position updated to target ($TARGET_X, $TARGET_Y)"
else
    echo "✗ Position NOT updated! Still at $FINAL_POS"
    exit 1
fi
echo ""

echo "=== Summary ==="
echo "✓ move_entity_animated.+x executed successfully"
echo "✓ desktop_pos.txt updated with target coordinates"
if [ -f "$ENTITY_DIR/animation_queue.txt" ]; then
    echo "✓ animation_queue.txt created with waypoints"
fi
echo ""
echo "Next: Manually launch entity window with 'button.sh run' from $.crypts/"
echo "      and verify whether entity appears at new position or disappears."
echo ""
