#!/bin/bash
# capture_baseline.sh - re-runnable event-retrofit pretest/compare harness.
# EVENT-MODULARITY-AND-BUILD-SPEED.md §1: "we will be careful retrofitting
# ergo testing" - this captures the SAME artifacts before and after each
# retrofit step (event_page_to_pal.+x, auto-create hook, drop-target
# handler, retroactive sweep) against the same real test entity
# (robot_chat_001), so a real diff - not a guess - shows whether existing
# behavior held.
#
# Usage: capture_baseline.sh <out_suffix>
#   e.g. capture_baseline.sh before   -> robot_chat_001_*_before.txt
#        capture_baseline.sh after1   -> robot_chat_001_*_after1.txt
# Diff any two: diff robot_chat_001_ui_before.txt robot_chat_001_ui_after1.txt

set -e
SUFFIX="${1:?usage: capture_baseline.sh <out_suffix>}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00"
ENT="$ROOT/xyzfs/users/0a9558a7-7c74-4358-833c-2d5b21edc421/home/livedesk/pals/robot_chat_001"

echo "-- file list --"
( cd "$ENT" && find . -type f | sort ) > "$HERE/robot_chat_001_filelist_$SUFFIX.txt"

echo "-- functional trace: cmd_1.sh --"
( cd "$ENT" && bash event_pkg/pages/page_1/cmd_1.sh ) > "$HERE/robot_chat_001_cmd1_output_$SUFFIX.txt" 2>&1
echo "exit=$?" >> "$HERE/robot_chat_001_cmd1_output_$SUFFIX.txt"

echo "-- events-hq live state (launch, snapshot, kill) --"
bash "$ROOT/&.widgits/events-hq/button.sh" "$ENT" "$ROOT" > /tmp/events_hq_capture_launch.log 2>&1
sleep 2
cp "$ENT/event_pkg/.hq_manager/ui.txt" "$HERE/robot_chat_001_ui_$SUFFIX.txt"
cp "$ENT/event_pkg/.hq_manager/pages.state.txt" "$HERE/robot_chat_001_pages_state_$SUFFIX.txt"
pkill -f "khtpm_events_hq_manager.*robot_chat_001" 2>/dev/null || true
pkill -f "events-hq.xhtpm.*robot_chat_001" 2>/dev/null || true

echo "captured: $SUFFIX"
