#!/bin/bash
# %.harnesses/iqabod-loop/button.sh - iqabod curriculum loop harness.
# Drives the supervisor ops against the LAN nodes, set-and-forget capable.
# Reads a tiny queue file, calls train_step + eval_curriculum for each
# entry, appends a report row, prints a one-line summary, then sleeps.
#
#   ./button.sh once          one pass over the queue, then exit
#   ./button.sh loop          keep cycling until killed (set-and-forget)
#   ./button.sh status        print last report rows + queue
#   ./button.sh enqueue <name> <node> [topic]
#   ./button.sh dequeue <name>
#
# Files (tiny, all on THIS box - storage rule):
#   curriculum-queue.txt   "name node" lines, one per entry
#   training-report.txt    appended TRAIN/EVAL/EVAL_SUM rows (pdl-ish)
#
# The node holds payloads (~/iqabod-store/<name>/). This box holds only
# the queue + report + curricula.pdl manifest.
set -u
HARNESS_DIR="$(cd "$(dirname "$0")" && pwd)"
AGENT_ROOT="${PRISC_PROJECT_ROOT:-}"
if [ -z "$AGENT_ROOT" ]; then
    AGENT_ROOT="$(cd "$HARNESS_DIR/../.." && pwd)"
    [ -d "$AGENT_ROOT/ops" ] || AGENT_ROOT="$HARNESS_DIR/../../.."
fi
OPS_X="$AGENT_ROOT/ops/+x"
QUEUE="$HARNESS_DIR/curriculum-queue.txt"
REPORT="$HARNESS_DIR/training-report.txt"
LOOP_DELAY="${IQABOD_LOOP_DELAY:-300}"
ACTION="${1:-help}"

req_ops() {
    for op in train_step eval_curriculum; do
        [ -x "$OPS_X/$op.+x" ] || { echo "missing op: $OPS_X/$op.+x (compile ops first)" >&2; return 1; }
    done
}

run_entry() {
    name="$1"; node="$2"
    ts="$(date '+%Y-%m-%d %H:%M')"
    echo "== $ts $name@$node ==" | tee -a "$REPORT"
    "$OPS_X/train_step.+x" "$name" "$node" | tee -a "$REPORT"
    "$OPS_X/eval_curriculum.+x" "$name" "$node" | tee -a "$REPORT"
}

# KPI asserts for one run_entry. Defaults:
#   max loss before we call training "stuck" (ln V floor -> should be way
#   below; conv-g1 hits ~0.0002), max <UNK> leaks we tolerate per probe,
#   min train steps so a 0-epoch run can't pass.
KPI_MAX_LOSS="${IQABOD_KPI_MAX_LOSS:-0.5}"
KPI_MAX_UNK="${IQABOD_KPI_MAX_UNK:-1}"
KPI_MIN_STEPS="${IQABOD_KPI_MIN_STEPS:-10}"

assert_entry() {
    name="$1"; node="$2"
    # re-parse the rows we just appended for THIS entry only
    rows="$(grep "^\(TRAIN\|EVAL_SUM\)|$name|$node|" "$REPORT" | tail -3)"
    loss=$(echo "$rows" | grep "^TRAIN|" | sed 's/.*final_loss=//')
    steps=$(echo "$rows" | grep "^TRAIN|" | sed 's/.*epochs=\([0-9]*\).*/\1/')
    unk=$(echo "$rows" | grep "^EVAL_SUM|" | sed 's/.*max_unk=//')
    sfail=$(echo "$rows" | grep "^EVAL_SUM|" | sed 's/.*ssh_fail=//')
    RESULT=0
    [ -z "$loss" ] && { echo "FAIL: $name@$node no TRAIN row parsed"; RESULT=1; }
    [ -z "$unk" ] && { echo "FAIL: $name@$node no EVAL_SUM row parsed"; RESULT=1; }
    if [ -n "$loss" ]; then
        awk -v l="$loss" -v k="$KPI_MAX_LOSS" 'BEGIN{exit !(l<k)}' || {
            echo "FAIL: $name@$node loss=$loss >= KPI_MAX_LOSS=$KPI_MAX_LOSS"; RESULT=1; }
    fi
    if [ -n "$steps" ]; then
        awk -v s="$steps" -v m="$KPI_MIN_STEPS" 'BEGIN{exit !(s>=m)}' || {
            echo "FAIL: $name@$node epochs=$steps < KPI_MIN_STEPS=$KPI_MIN_STEPS"; RESULT=1; }
    fi
    if [ -n "$unk" ]; then
        awk -v u="$unk" -v k="$KPI_MAX_UNK" 'BEGIN{exit !(u<=k)}' || {
            echo "FAIL: $name@$node max_unk=$unk > KPI_MAX_UNK=$KPI_MAX_UNK"; RESULT=1; }
    fi
    if [ -n "$sfail" ] && [ "$sfail" != "0" ]; then
        echo "FAIL: $name@$node ssh_fail=$sfail"; RESULT=1; fi
    [ "$RESULT" = "0" ] && echo "PASS: $name@$node loss=$loss steps=$steps unk=$unk" 
    return $RESULT
}

once() {
    req_ops || exit 1
    [ -f "$QUEUE" ] || { echo "queue empty ($QUEUE)"; exit 0; }
    entries="$(grep -v '^#' "$QUEUE" 2>/dev/null)"
    [ -n "$entries" ] || { echo "queue empty ($QUEUE)"; exit 0; }
    PASSED=0; FAILED=0
    while IFS= read -r line; do
        [ -z "$line" ] && continue
        set -- $line
        run_entry "$1" "$2"
        if assert_entry "$1" "$2"; then PASSED=$((PASSED+1)); else FAILED=$((FAILED+1)); fi
    done <<< "$entries"
    echo "QUEUE_DONE|$(date '+%Y-%m-%d %H:%M')|entries=$(printf '%s\n' "$entries" | wc -l)|pass=$PASSED|fail=$FAILED"
    [ "$FAILED" = "0" ]
}

case "$ACTION" in
    once|run)
        once
        ;;
    loop|forever)
        while true; do once || echo "LOOP_PASS_KPI_FAILED"; sleep "$LOOP_DELAY"; done
        ;;
    status|s)
        [ -f "$REPORT" ] && tail -8 "$REPORT" || echo "no report yet"
        echo "--- queue ---"
        [ -f "$QUEUE" ] && grep -v '^#' "$QUEUE" || echo "(empty)"
        ;;
    enqueue|add)
        [ $# -ge 3 ] || { echo "usage: button.sh enqueue <name> <node> [topic]"; exit 1; }
        mkdir -p "$HARNESS_DIR"
        echo "$2 $3" >> "$QUEUE"
        echo "enqueued $2 $3"
        ;;
    dequeue|rm)
        [ $# -ge 2 ] || { echo "usage: button.sh dequeue <name>"; exit 1; }
        [ -f "$QUEUE" ] && grep -v "^$2 " "$QUEUE" > "$QUEUE.tmp" && mv "$QUEUE.tmp" "$QUEUE"
        echo "dequeued $2"
        ;;
    help|h|-h|--help|*)
        echo "%.harnesses/iqabod-loop/button.sh - iqabod curriculum loop harness"
        echo "Usage: ./button.sh <once|loop|status|enqueue|dequeue|help>"
        echo "  once runs train+eval and asserts KPIs (loss<$KPI_MAX_LOSS, unk<=$KPI_MAX_UNK, steps>=$KPI_MIN_STEPS);"
        echo "      exits nonzero if any entry fails - use it as the ALWAYS regression check"
        echo "  loop cycles forever; status prints queue + last report"
        echo "  KPI env overrides: IQABOD_KPI_MAX_LOSS IQABOD_KPI_MAX_UNK IQABOD_KPI_MIN_STEPS IQABOD_LOOP_DELAY"
        echo "  queue:  $QUEUE"
        echo "  report: $REPORT"
        ;;
esac
