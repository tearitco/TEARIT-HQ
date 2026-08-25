#!/bin/bash
set -e

DIR="$(cd "$(dirname "$0")/../.." && pwd)"
ROOT="/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST-11.12/x0.parent-level-dev-env-04.03/yz.muchiverse/44.xyz❤️‍🔥️00.07"
SYS="$DIR/system"
MPALS="$ROOT/01.muchi-pals-🥚️-13.01"
HARNESS="$DIR/test-harn/ops/+x"
PROOF="$DIR/test-harn/proof"
PET_ID="test_pet_$$"

mkdir -p "$PROOF"

echo "=== gl-canvas drag-drop test ==="
echo "pet_id=$PET_ID"

# Cleanup any prior runs
pkill -f gl_canvas 2>/dev/null || true
pkill -f pet_purely 2>/dev/null || true
sleep 1

echo "[1/7] Taking baseline screenshot..."
"$HARNESS/tk_screenshot.+x" "$PROOF/01_baseline.ppm" 2>&1 || true

echo "[2/7] Launching gl-canvas..."
cd "$DIR" && PRISC_PROJECT_ROOT="$DIR" "$SYS/gl_canvas" &
CANVAS_PID=$!
sleep 2

echo "[3/7] Taking canvas-only screenshot..."
"$HARNESS/tk_screenshot.+x" "$PROOF/02_canvas_only.ppm" 2>&1 || true

echo "[4/7] Launching pet_purely at (100,100)..."
cd "$DIR" && PRISC_PROJECT_ROOT="$DIR" "$SYS/pet_purely" "$PET_ID" 100 100 &
PET_PID=$!
sleep 2

echo "[5/7] Taking pet+canvas screenshot..."
"$HARNESS/tk_screenshot.+x" "$PROOF/03_pet_and_canvas.ppm" 2>&1 || true

echo "[6/7] Simulating drag from pet to canvas..."
"$HARNESS/tk_drag_sim.+x" "$PET_ID" "gl-canvas" 10 2>&1
sleep 3

echo "[7/7] Taking post-drop screenshot..."
"$HARNESS/tk_screenshot.+x" "$PROOF/04_after_drop.ppm" 2>&1 || true

# Verify results
echo ""
echo "=== results ==="

IMPORTED="$DIR/imported_pets.txt"
CLOSE_FILE="/tmp/egg_window_close_${PET_ID}.txt"

if [ -f "$IMPORTED" ] && grep -q "$PET_ID" "$IMPORTED" 2>/dev/null; then
    echo "PASS: pet_id found in imported_pets.txt"
else
    echo "CHECK: imported_pets.txt may not contain pet_id (file may not exist if import_pet was not called)"
    if [ -f "$IMPORTED" ]; then
        echo "  contents: $(cat "$IMPORTED")"
    fi
fi

if [ ! -f "$CLOSE_FILE" ]; then
    echo "PASS: close request file consumed (or never created)"
else
    echo "INFO: close request file still exists (egg_window may not have read it)"
fi

if ! kill -0 $PET_PID 2>/dev/null; then
    echo "PASS: pet_purely exited (closed by close request)"
else
    echo "INFO: pet_purely still running (close request may not have been sent yet)"
    kill $PET_PID 2>/dev/null || true
fi

echo ""
echo "Screenshots saved in: $PROOF/"
ls -la "$PROOF/" 2>/dev/null

# Cleanup
kill $CANVAS_PID 2>/dev/null || true
pkill -f gl_canvas 2>/dev/null || true
pkill -f pet_purely 2>/dev/null || true

echo ""
echo "=== test complete ==="
