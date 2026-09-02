#!/bin/bash
# run_queue.sh — task execution/automation layer for the FSM
# (au11-hq/HARNESS-DELEGATION-PIPELINE.md), direct instruction
# 2026-08-13: "a harness can execute and delegate massive token save."
#
# What this actually does, real not aspirational: processes every
# *.plan file sitting in plans/queue/ through run_plan.sh, one at a
# time (never parallel - only one open-hai instance can safely exist,
# see PITFALL 72), files each plan into plans/done/ or plans/failed/
# based on its REAL exit code (never a guess), and writes one
# aggregate RUN_SUMMARY per queue pass. This is what turns "a person
# (or Claude) manually runs one plan at a time" into "drop a plan file
# in a folder, it runs on its own" - the actual mechanism that makes
# recurring/routine delegation possible without a human or Claude
# babysitting each invocation.
#
# Usage: run_queue.sh
#   (no args - always processes the whole current queue/ directory,
#   run it again for another pass. Intentionally NOT self-looping or
#   daemonized - see "NOT wired to autostart/cron" note below.)
#
# NOT WIRED TO ANY SCHEDULER (deliberate, not an oversight): this
# script does not touch #.desktop/livedesk_taskbar.pdl, $.crypts/
# autostart.pdl, or the system crontab. Wiring recurring execution
# (cron, a systemd timer, or a call from autostart.pdl) is a real,
# separate, more sensitive step - touches boot-time/host-level
# automation, which warrants its own explicit go-ahead rather than
# being silently bundled into this build. This script is safe to run
# by hand or from an EXTERNAL scheduler once one is deliberately set
# up; it does not set one up itself.
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
QUEUE_DIR="$SCRIPT_DIR/plans/queue"
DONE_DIR="$SCRIPT_DIR/plans/done"
FAILED_DIR="$SCRIPT_DIR/plans/failed"
RUN_PLAN="$SCRIPT_DIR/run_plan.sh"

mkdir -p "$QUEUE_DIR" "$DONE_DIR" "$FAILED_DIR"

log() { echo "[queue] $*"; }

shopt -s nullglob
plans=("$QUEUE_DIR"/*.plan)
shopt -u nullglob

if [ "${#plans[@]}" -eq 0 ]; then
    log "queue empty, nothing to do"
    exit 0
fi

log "processing ${#plans[@]} plan(s) from $QUEUE_DIR"
n_pass=0
n_fail=0
TS="$(date +%Y%m%d-%H%M%S)"
RUN_SUMMARY="$SCRIPT_DIR/proof/queue_run_$TS.txt"
mkdir -p "$SCRIPT_DIR/proof"
{
    echo "run_queue.sh pass — $TS"
    echo "plans found: ${#plans[@]}"
    echo ""
} > "$RUN_SUMMARY"

for plan in "${plans[@]}"; do
    name="$(basename "$plan")"
    log "running: $name"
    if bash "$RUN_PLAN" "$plan" > "$SCRIPT_DIR/proof/queue_${name%.plan}_$TS.log" 2>&1; then
        echo "PASS  $name" >> "$RUN_SUMMARY"
        log "  PASS - moved to plans/done/"
        mv "$plan" "$DONE_DIR/"
        n_pass=$((n_pass + 1))
    else
        echo "FAIL  $name (see proof/queue_${name%.plan}_$TS.log)" >> "$RUN_SUMMARY"
        log "  FAIL - moved to plans/failed/"
        mv "$plan" "$FAILED_DIR/"
        n_fail=$((n_fail + 1))
    fi
done

{
    echo ""
    echo "totals: $n_pass pass, $n_fail fail"
} >> "$RUN_SUMMARY"

log "pass complete: $n_pass pass, $n_fail fail - summary at $RUN_SUMMARY"
cat "$RUN_SUMMARY"

[ "$n_fail" -eq 0 ]
