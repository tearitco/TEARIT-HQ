#!/usr/bin/env bash
# iqa-fork/loop.sh - keep the IQA distillation loop running.
# Usage: loop.sh [iterations] [sleep_seconds_between]
#   iterations  - default 0 = run until killed (turns AUTO_DELEGATION T2 on)
#   sleep       - default 120s between iterations
# Detached:  nohup bash iqa-fork/loop.sh 0 180 >/tmp/iqa_loop.out 2>&1 &
#            (or tmux/screen). Kill: pkill -f 'iqa-fork/loop.sh'
set -u
DIR="$(cd "$(dirname "$0")" && pwd)"
ITERS="${1:-0}"
SLEEP="${2:-120}"
i=0
while [ "$ITERS" -eq 0 ] || [ "$i" -lt "$ITERS" ]; do
    i=$((i + 1))
    echo "=== IQA iteration $i $(date -u +%Y-%m-%dT%H:%M:%SZ) ==="
    if ! bash "$DIR/run.sh"; then
        echo "iteration $i: run.sh FAILED - sleeping and retrying"
    fi
    if [ "$ITERS" -ne 0 ] && [ "$i" -ge "$ITERS" ]; then break; fi
    sleep "$SLEEP"
done
echo "loop.sh: done after $i iterations"
