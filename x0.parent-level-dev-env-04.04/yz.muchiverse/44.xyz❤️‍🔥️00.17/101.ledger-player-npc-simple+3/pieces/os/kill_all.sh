#!/bin/bash
# kill_all.sh - LPNS+MAP+4 process cleanup
# Modeled on x0.moke pieces/os/kill_all.sh and 2.muchi-verse/kill_all.sh
# Uses surgical_kill() pattern with SIGKILL for reliable cleanup

surgical_kill() {
    local name="$1"
    local pattern="system/${name}"
    if pgrep -f "$pattern" > /dev/null 2>&1; then
        echo "Killing $name..."
        pkill -9 -f "$pattern" 2>/dev/null
    fi
}

echo "=== LPNS+MAP+4 kill_all.sh - surgical cleanup ==="

# Layer 1: Kill by binary name (surgical)
surgical_kill "orchestrator"
surgical_kill "renderer"
surgical_kill "keyboard_input"
surgical_kill "chtpm_parser_pal"
surgical_kill "game_manager"

# Layer 2: Kill PAL scripts (prisc+x)
if pgrep -f '\.pal$' > /dev/null 2>&1; then
    echo "Killing residual prisc+x module(s) by .pal argument..."
    pkill -9 -f '\.pal$' 2>/dev/null
fi

# Layer 3: Nuclear option - kill any process running from this project
# Matches processes whose cwd is under this project's directory
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
for pid in $(pgrep -f "system/"); do
    exe="$(readlink -f "/proc/$pid/exe" 2>/dev/null)"
    case "$exe" in
        "$PROJECT_DIR"/*)
            echo "Killing stray process $pid ($exe)..."
            kill -9 "$pid" 2>/dev/null
            ;;
    esac
done

# Wait for processes to die
sleep 0.2

# Verify all dead
echo ""
echo "Checking for residual processes..."
if ps aux | grep -E "system/(renderer|keyboard_input|chtpm_parser_pal|game_manager|orchestrator)" | grep -v grep >/dev/null 2>&1; then
    echo "WARNING: Some processes still running:"
    ps aux | grep -E "system/(renderer|keyboard_input|chtpm_parser_pal|game_manager|orchestrator)" | grep -v grep
else
    echo "All LPNS processes terminated."
fi

# Clean up state files
rm -f pieces/system/quit_flag.txt 2>/dev/null
rm -f pieces/os/proc_list.txt 2>/dev/null

echo "Cleanup complete."
