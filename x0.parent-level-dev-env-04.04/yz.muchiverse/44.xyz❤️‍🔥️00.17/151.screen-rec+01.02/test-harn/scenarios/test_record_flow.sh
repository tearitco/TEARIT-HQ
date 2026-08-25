#!/bin/bash
# test_record_flow.sh - end-to-end proof: click RECORD, wait, click STOP,
# verify a real playable .mp4 landed in recordings/. Same "screenshot for
# proof, receipt for correctness" philosophy as 150.gl-canvas's
# test_drag_drop.sh, adapted for a click instead of a drag.
#
# The one thing this CANNOT automate, on purpose: the GNOME picker dialog
# that appears on screen_rec's first portal request. That's a compositor
# security surface, not an app window -- scripting past it would defeat the
# whole point of it. If screen_rec isn't already running, this script
# launches it and waits for you to click through that dialog once.
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SYS="$ROOT/system"
TH_OPS="$ROOT/test-harn/ops/+x"
PROOF_DIR="$ROOT/test-harn/scenarios/proof"
RESULT_RECEIPT="$PROOF_DIR/last_run.receipt.txt"
WIN_TITLE="screen-rec"

export SCREENREC_PROJECT_ROOT="$ROOT"
mkdir -p "$PROOF_DIR" "$ROOT/pieces/display" "$ROOT/pieces/control" "$ROOT/recordings"

# Button center in screen_rec_gui.c's own layout constants (STRIP_H=104,
# BUTTON_BAR_H=44, BUTTON_H=28, BUTTON_X0=16, window height=508). Kept in
# sync with system/screen_rec_gui.c by hand -- if that layout changes,
# update these two numbers.
BUTTON_CLICK_X=76
BUTTON_CLICK_Y_X11=382   # X11 top-down window-relative y (already flipped from GL bottom-origin)

fail() { echo "FAIL: $1"; echo "result=FAIL" > "$RESULT_RECEIPT"; echo "reason=$1" >> "$RESULT_RECEIPT"; exit 1; }

wait_for_kv() {
    local path="$1" key="$2" want="$3" timeout_s="$4"
    local waited=0
    while [ "$waited" -lt "$timeout_s" ]; do
        if [ -f "$path" ]; then
            val="$(grep "^${key}=" "$path" 2>/dev/null | tail -1 | cut -d= -f2-)"
            if [ "$val" = "$want" ]; then return 0; fi
        fi
        sleep 1
        waited=$((waited + 1))
    done
    return 1
}

echo "=== screen-rec end-to-end test ==="

if ! pgrep -f "system/screen_rec\$" > /dev/null; then
    echo "screen_rec not running -- launching it now."
    echo ">>> A GNOME 'Share' picker dialog should appear. Pick a monitor and click Share. <<<"
    "$SYS/screen_rec" > /tmp/screen_rec_test.log 2>&1 &
    echo $! > /tmp/screen_rec_test.pid
fi

echo "Waiting up to 60s for portal negotiation + first preview frame..."
waited=0
while [ ! -f "$ROOT/pieces/display/rgb_frame.receipt.txt" ] && [ "$waited" -lt 60 ]; do
    sleep 1; waited=$((waited + 1))
done
[ -f "$ROOT/pieces/display/rgb_frame.receipt.txt" ] || fail "portal/capture never came up (no rgb_frame.receipt.txt after 60s) -- did you click Share on the picker?"
echo "capture is live."

if ! pgrep -f screen_rec_gui > /dev/null; then
    echo "screen_rec_gui not running -- launching it."
    "$SYS/screen_rec_gui" > /tmp/screen_rec_gui_test.log 2>&1 &
    echo $! > /tmp/screen_rec_gui_test.pid
    sleep 1
fi

echo "Confirming the GUI window is provably showing real bytes (checksum_match in gui_display.receipt.txt)..."
wait_for_kv "$ROOT/pieces/display/gui_display.receipt.txt" "checksum_match" "1" 10 \
    || fail "gui_display.receipt.txt never reported checksum_match=1 -- preview pipeline is broken"
echo "checksum verified: GUI is displaying exactly what screen_rec produced."

"$TH_OPS/tk_screenshot.+x" "$WIN_TITLE" "$PROOF_DIR/01_before_record.ppm" \
    || fail "tk_screenshot failed before recording"

echo "Clicking RECORD..."
"$TH_OPS/tk_click.+x" "$WIN_TITLE" "$BUTTON_CLICK_X" "$BUTTON_CLICK_Y_X11" \
    || fail "tk_click (RECORD) failed"

wait_for_kv "$ROOT/pieces/display/recorder_state.receipt.txt" "recording" "1" 5 \
    || fail "recorder_state.receipt.txt never showed recording=1 after clicking RECORD"
OUTPUT_PATH="$(grep '^output_path=' "$ROOT/pieces/display/recorder_state.receipt.txt" | cut -d= -f2-)"
echo "recording started -> $OUTPUT_PATH"

sleep 1
"$TH_OPS/tk_screenshot.+x" "$WIN_TITLE" "$PROOF_DIR/02_during_record.ppm" \
    || fail "tk_screenshot failed during recording"

RECORD_SECONDS=4
echo "Recording for ${RECORD_SECONDS}s..."
sleep "$RECORD_SECONDS"

echo "Clicking STOP..."
"$TH_OPS/tk_click.+x" "$WIN_TITLE" "$BUTTON_CLICK_X" "$BUTTON_CLICK_Y_X11" \
    || fail "tk_click (STOP) failed"

wait_for_kv "$ROOT/pieces/display/recorder_state.receipt.txt" "recording" "0" 5 \
    || fail "recorder_state.receipt.txt never showed recording=0 after clicking STOP"
echo "recording stopped."

"$TH_OPS/tk_screenshot.+x" "$WIN_TITLE" "$PROOF_DIR/03_after_stop.ppm" \
    || fail "tk_screenshot failed after stopping"

[ -n "$OUTPUT_PATH" ] && [ -s "$OUTPUT_PATH" ] || fail "output file '$OUTPUT_PATH' missing or empty"

PROBE="$(ffprobe -v error -show_entries stream=codec_name,width,height -show_entries format=duration \
    -of default=noprint_wrappers=1 "$OUTPUT_PATH" 2>&1)"
echo "$PROBE" | grep -q "codec_name=h264" || fail "ffprobe didn't report h264 for $OUTPUT_PATH"
echo "ffprobe OK:"
echo "$PROBE"

{
    echo "result=PASS"
    echo "output_path=$OUTPUT_PATH"
    echo "record_seconds_requested=$RECORD_SECONDS"
    echo "$PROBE"
} > "$RESULT_RECEIPT"

echo "=== PASS -- proof screenshots + $RESULT_RECEIPT ==="
