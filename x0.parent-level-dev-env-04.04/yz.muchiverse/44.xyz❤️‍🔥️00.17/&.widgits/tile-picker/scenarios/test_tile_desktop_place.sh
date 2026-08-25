#!/bin/bash
# test_tile_desktop_place.sh - real end-to-end proof of the tile-picker
# desktop-placer path, same harness shape as
# 101.drag-drop-test=ON🀄️/scenarios/test_basic_import.sh (pass/fail
# counters, cleanup trap, poll-with-retry for window state) - but this
# project's own tile-picker doesn't depend on any of that project's ops,
# so nothing is imported from there. Window lookup uses xwininfo, not
# dd_find_window.+x/xdotool - xdotool is not installed on this machine
# (confirmed 2026-08-04), and xwininfo is a plain X11 client tool with no
# extra dependency.
#
# Covers:
#   1. tp_set_brush + tp_place_desktop -> real package on disk
#   2. tp_place_desktop's spawn -> a real, live GL window (title "tile:<glyph>")
#   3. tp_import_from_desktop -> real PLACE_TILE onto a real map.txt cell
#      (via a scratch copy of mutaclysm's own real ops, same technique
#      used to first prove this chain works, 2026-08-04)
#   4. removing the package -> the live window notices and self-closes
# Usage: test_tile_desktop_place.sh [--leave-open]
#   --leave-open   direct instruction 2026-08-04 ("leave picker window
#                   open... always best proof"): skips step 6 (removing
#                   the package / closing the window) and skips this
#                   harness's own cleanup trap killing the window or
#                   deleting the scratch dir, so the real live window and
#                   its real package are still there on disk/on screen
#                   after the script exits, for direct visual inspection.
set -u

LEAVE_OPEN=0
[ "${1:-}" = "--leave-open" ] && LEAVE_OPEN=1

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TP_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
OPS_X="$TP_DIR/ops/+x"
HOUSE_ROOT="$(cd "$TP_DIR/../.." && pwd)"
MUTA_SRC="$HOUSE_ROOT/101.mutaclsym🧟‍♂️️+18.01"

WORK_DIR="$(mktemp -d /tmp/tp_desktop_harness.XXXXXX)"
STATE_DIR="$WORK_DIR/tp_widget_state"
DESK_DIR="$WORK_DIR/desktop"
SCRATCH_MUTA="$WORK_DIR/mutaclysm_scratch"
GLYPH='Z'

PASS_COUNT=0
FAIL_COUNT=0
SPAWNED_PID=""

pass() { echo "PASS: $1"; PASS_COUNT=$((PASS_COUNT + 1)); }
fail() { echo "FAIL: $1"; FAIL_COUNT=$((FAIL_COUNT + 1)); }

find_window() {
    # prints 1 if a window titled exactly "$1" is currently mapped, else 0
    xwininfo -root -tree 2>/dev/null | grep -F "\"$1\"" >/dev/null 2>&1 && echo 1 || echo 0
}

cleanup() {
    echo ""
    if [ "$LEAVE_OPEN" = "1" ]; then
        echo "Leaving window + scratch dir open for visual proof:"
        echo "  package: $PKG_DIR"
        echo "  scratch: $WORK_DIR"
        echo "  (right-click the window, or 'rm -rf \"$PKG_DIR\"', to close it)"
    else
        echo "Cleaning up..."
        pkill -f "tp_desktop_window.+x $DESK_DIR" >/dev/null 2>&1
        rm -rf "$WORK_DIR"
    fi
    echo "Test complete: $PASS_COUNT passed, $FAIL_COUNT failed"
}
trap cleanup EXIT

echo "=== tile-picker desktop-placer harness ==="
echo ""

echo "Step 1: Compiling tile-picker ops..."
if [ ! -x "$OPS_X/tp_desktop_window.+x" ]; then
    bash "$TP_DIR/button.sh" compile
fi
[ -x "$OPS_X/tp_desktop_window.+x" ] && pass "ops compiled" || { fail "ops missing after compile"; exit 1; }

echo "Step 2: Setting up scratch dirs..."
mkdir -p "$STATE_DIR" "$DESK_DIR" "$SCRATCH_MUTA"
cp -r "$MUTA_SRC/ops" "$MUTA_SRC/pieces" "$SCRATCH_MUTA/" 2>/dev/null
mkdir -p "$SCRATCH_MUTA/pieces/system/widget_cmds"
# NOTE: tp_place/tp_set_brush/tp_import_from_desktop all fopen(inbox,"a")
# with no mkdir - they assume pieces/system/widget_cmds/ already exists
# (real mutaclysm only creates it via muta_widget_cmds.c's own
# ensure_dirs(), called at ITS startup, not tile-picker's). A project
# where mutaclysm has never run even once would silently drop the very
# first tile-picker command. Flagged as a real gap 2026-08-04 - this
# mkdir models "mutaclysm has been launched at least once already",
# which is the realistic case, not a workaround for a harness-only bug.
{
    echo "session_root=$SCRATCH_MUTA"
    echo "inbox_path=$SCRATCH_MUTA/pieces/system/widget_cmds/inbox.txt"
    echo "kind=game_world"
    echo "project_id=mutaclysm"
} > "$STATE_DIR/focus.txt"
if [ -d "$SCRATCH_MUTA/ops" ] && [ -d "$SCRATCH_MUTA/pieces" ]; then
    pass "scratch dirs ready"
else
    fail "scratch mutaclysm copy failed"
    exit 1
fi

echo "Step 3: tp_set_brush + tp_place_desktop..."
"$OPS_X/tp_set_brush.+x" "$STATE_DIR" "$GLYPH" >/dev/null
"$OPS_X/tp_place_desktop.+x" "$STATE_DIR" "$DESK_DIR" >/dev/null
PKG_DIR=$(find "$DESK_DIR/tiles" -maxdepth 1 -mindepth 1 -type d | head -1)
if [ -n "$PKG_DIR" ] && [ -f "$PKG_DIR/glyph.txt" ] && [ -f "$PKG_DIR/meta.pdl" ]; then
    pass "desktop package written ($PKG_DIR)"
else
    fail "desktop package not found"
    exit 1
fi

echo "Step 4: Waiting for live GL window 'tile:$GLYPH'..."
FOUND=0
for i in $(seq 1 20); do
    if [ "$(find_window "tile:$GLYPH")" = "1" ]; then FOUND=1; break; fi
    sleep 0.3
done
[ "$FOUND" = "1" ] && pass "live GL window appeared" || fail "live GL window never appeared"

echo "Step 5: Importing desktop package into map_start(4,4)..."
BEFORE=$(sed -n '5p' "$SCRATCH_MUTA/pieces/world_01/map_start/map.txt" | cut -c5)
"$OPS_X/tp_import_from_desktop.+x" "$STATE_DIR" "$PKG_DIR" map_start 4 4 >/dev/null
PRISC_PROJECT_ROOT="$SCRATCH_MUTA" "$SCRATCH_MUTA/ops/+x/muta_widget_cmds.+x" >/dev/null
AFTER=$(sed -n '5p' "$SCRATCH_MUTA/pieces/world_01/map_start/map.txt" | cut -c5)
if [ "$AFTER" = "$GLYPH" ] && [ "$BEFORE" != "$AFTER" ]; then
    pass "map.txt cell really changed ('$BEFORE' -> '$AFTER')"
else
    fail "map.txt cell did not change as expected (before='$BEFORE' after='$AFTER')"
fi

if [ "$LEAVE_OPEN" = "1" ]; then
    echo "Step 6: --leave-open set, skipping package removal / window-close check."
else
    echo "Step 6: Removing package, expecting window to self-close..."
    rm -rf "$PKG_DIR"
    CLOSED=0
    for i in $(seq 1 20); do
        if [ "$(find_window "tile:$GLYPH")" = "0" ]; then CLOSED=1; break; fi
        sleep 0.3
    done
    [ "$CLOSED" = "1" ] && pass "window self-closed after package removed" || fail "window still open after package removed"
fi

echo ""
echo "=== Test Complete ==="
