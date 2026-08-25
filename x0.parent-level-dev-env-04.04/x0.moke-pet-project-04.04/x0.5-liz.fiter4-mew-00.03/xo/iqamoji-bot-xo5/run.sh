#!/bin/bash
# run.sh for Bot5
BOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$BOT_DIR/../.." && pwd)"
cd "$BOT_DIR"

if [ ! -f "./bot5_orchestrator" ]; then
    echo "Building..."
    bash build.sh || exit 1
fi

echo "=== Launching Bot5 (Foreground) ==="
echo "Project Root: $PROJECT_ROOT"

# Run in foreground (waits & exits cleanly)
./bot5_orchestrator "$PROJECT_ROOT"

# 💡 If you ever want it to run in the background and free your terminal immediately:
# ./bot5_orchestrator "$PROJECT_ROOT" &
# echo "Orchestrator running in background (PID $!)"
