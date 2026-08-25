#!/usr/bin/env bash
# iqa-fork/auto-checkin.sh - THE BOSS, AUTOMATED.
# Runs status.sh (health of every thread), then ACTS:
#   - restarts iqa loop if dead
#   - restarts builder loop if dead
#   - leaves a one-line verdict per run (for groq/gemma to read later)
# This is the "at some point automate even the boss check-in" step:
# after the node wiring is proven, this script is what a cron/launchd
# on the box (or on Linux via ssh) calls every N minutes - no human, and
# no opencode session needed to keep the LAN busy.
#
# Usage: auto-checkin.sh [--fix]     (--fix actually restarts dead threads)
# The groq/gemma involvement: the KPI tail below can be handed to the
# teacher (llama3-groq-tool-use) to write a one-line triage note. Until
# that is wired, --fix is the deterministic restart policy.
set -u
DIR="$(cd "$(dirname "$0")" && pwd)"
FIX=0
[ "${1:-}" = "--fix" ] && FIX=1

TS=$(date -u +%s)
echo "=== CHECKIN $TS ==="
bash "$DIR/status.sh"

# --- produce the IQA report (hourly summary, written each check-in) ---
bash "$DIR/report.sh" 24 >/dev/null 2>&1
echo "report: iqa-fork/reports/REPORT-$(date -u +%Y-%m-%d).txt"

# --- gather thread pids ---
BUILDER_PIDS=$(pgrep -f 'builder/run.sh' | wc -l)
IQA_PIDS=$(pgrep -f 'loop\.sh' | wc -l)

# --- act on dead threads ---
if [ "$IQA_PIDS" -eq 0 ]; then
    echo "iqa_loop: DEAD"
    if [ "$FIX" -eq 1 ]; then
        cd "$DIR" && nohup bash loop.sh 0 180 >/tmp/iqa_loop.out 2>&1 &
        echo "iqa_loop: RESTARTED"
    fi
else
    echo "iqa_loop: alive"
fi

if [ "$BUILDER_PIDS" -eq 0 ]; then
    echo "builder: DEAD"
    if [ "$FIX" -eq 1 ]; then
        cd "$DIR/../046.open-gema🤖️+1" && nohup bash builder/run.sh >/tmp/builder.out 2>&1 &
        echo "builder: RESTARTED"
    fi
else
    echo "builder: alive"
fi

echo "=== CHECKIN_DONE $TS ==="
