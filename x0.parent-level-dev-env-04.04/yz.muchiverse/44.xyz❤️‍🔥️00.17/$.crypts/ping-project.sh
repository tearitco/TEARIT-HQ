#!/bin/bash
# ping-project.sh - generic, dead-simple smoke test: does this project's
# own button.sh actually launch a real session and render a real, non-
# empty frame? Nothing more. For any project that doesn't have (or
# doesn't yet have) a real gameplay scenario, this is a minimal-but-
# honest "does it run at all" check - not a substitute for a real
# scenario, just a floor everything can clear.
#
# Prints the same "=== OVERALL: PASS ===" / "=== OVERALL: FAIL ==="
# convention harness-runner.sh's own classify_harness() already looks
# for, so this drops straight into the house-wide sweep like any other
# scenario.
#
# USAGE (called directly):
#   ping-project.sh <project_dir> [ready_grep] [timeout_secs]
#     project_dir  - the project's own root (has button.sh)
#     ready_grep   - a string to grep for in current_frame.txt as proof
#                    of a REAL render, not just a transient loading/blank
#                    frame (default: project's own dirname, uppercased
#                    with spaces between letters, e.g. "my-lawyer" ->
#                    tries a few common title-banner shapes - if none
#                    match, falls back to "any non-empty frame twice in
#                    a row" as a weaker but still real liveness check)
#     timeout_secs - how long to wait for a ready frame (default 30)
#
# TO WIRE INTO A PROJECT'S OWN test-harn*/ HARNESS: add a scenario file
# (e.g. scenarios/demo_ping.sh) that's just:
#   #!/bin/bash
#   HARNESS_DIR="$(cd "$(dirname "$0")/.." && pwd)"
#   PROJECT_DIR="$(cd "$HARNESS_DIR/.." && pwd)"
#   HOUSE_DIR="$(cd "$PROJECT_DIR/../.." && pwd)"   # adjust ../.. depth to the real house root
#   exec bash "$HOUSE_DIR/\$.crypts/ping-project.sh" "$PROJECT_DIR"
# then point button.sh's own `demo`/whatever action at that scenario
# file, same as any other project's own harness. See my-lawyer's own
# test-harn-same/scenarios/demo_ping.sh for a real, working example.

set -u
(set -o pipefail) 2>/dev/null && set -o pipefail

PROJECT_DIR="${1:-}"
READY_GREP="${2:-}"
TIMEOUT="${3:-30}"

if [ -z "$PROJECT_DIR" ] || [ ! -f "$PROJECT_DIR/button.sh" ]; then
    echo "usage: ping-project.sh <project_dir> [ready_grep] [timeout_secs]" >&2
    echo "=== OVERALL: FAIL ==="
    exit 1
fi
PROJECT_DIR="$(cd "$PROJECT_DIR" && pwd)"
NAME="$(basename "$PROJECT_DIR")"

echo "=== ping-project: $NAME ==="

cleanup() {
    echo "--- cleanup ---"
    (cd "$PROJECT_DIR" && bash button.sh kill >/dev/null 2>&1) || true
}
trap cleanup EXIT

(cd "$PROJECT_DIR" && bash button.sh kill >/dev/null 2>&1) || true
rm -rf "$PROJECT_DIR/pieces/sessions" 2>/dev/null

LOG="$(mktemp)"
cd "$PROJECT_DIR"
NO_GL=1 bash button.sh run < /dev/null > "$LOG" 2>&1 &
disown 2>/dev/null || true

SESS=""
for i in $(seq 1 "$TIMEOUT"); do
    CANDIDATE=$(cd "$PROJECT_DIR" && ls -dt pieces/sessions/*/ 2>/dev/null | head -1)
    if [ -n "$CANDIDATE" ]; then
        FRAME_PATH="$PROJECT_DIR/${CANDIDATE}pieces/display/current_frame.txt"
        if [ -s "$FRAME_PATH" ]; then
            if [ -n "$READY_GREP" ]; then
                if grep -q "$READY_GREP" "$FRAME_PATH" 2>/dev/null; then
                    SESS="$CANDIDATE"
                    break
                fi
            else
                # No specific string given - require the SAME non-empty
                # content across two 1s-apart polls, so a mid-write or
                # transient loading placeholder can't false-pass.
                SNAP1="$(cat "$FRAME_PATH" 2>/dev/null)"
                sleep 1
                SNAP2="$(cat "$FRAME_PATH" 2>/dev/null)"
                if [ -n "$SNAP1" ] && [ "$SNAP1" = "$SNAP2" ]; then
                    SESS="$CANDIDATE"
                    break
                fi
            fi
        fi
    fi
    sleep 1
done

if [ -z "$SESS" ]; then
    echo "FAIL: no real, stable, non-empty frame appeared within ${TIMEOUT}s (check button.sh's own launch log: $LOG)"
    echo "=== OVERALL: FAIL ==="
    exit 1
fi

echo "PASS: session launched and rendered a real frame ($SESS)"

# One more liveness beat: confirm the session's own process tree is
# actually still alive a moment later, not a one-shot render followed by
# a silent crash.
sleep 1
if [ -n "$(cd "$PROJECT_DIR" && ls -dt pieces/sessions/*/ 2>/dev/null | head -1)" ]; then
    echo "PASS: session dir still present after a 1s beat (not a one-shot crash)"
else
    echo "FAIL: session dir vanished after launch - crashed or exited immediately"
    echo "=== OVERALL: FAIL ==="
    exit 1
fi

echo "=== OVERALL: PASS ==="
