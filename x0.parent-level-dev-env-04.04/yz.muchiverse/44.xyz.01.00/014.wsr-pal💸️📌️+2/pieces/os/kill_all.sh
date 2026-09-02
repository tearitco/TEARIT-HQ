#!/bin/bash
# kill_all.sh - wsr-pal process cleanup
# 3-layer cascading kill: process group → file-backed PID → surgical SIGKILL

surgical_kill() {
    local name="$1"
    local pattern="system/${name}"
    if pgrep -f "$pattern" > /dev/null 2>&1; then
        echo "Killing $name..."
        pkill -9 -f "$pattern" 2>/dev/null
    fi
}

echo "=== wsr-pal kill_all.sh - surgical cleanup ==="

# Layer 1: Kill by binary name (surgical)
surgical_kill "orchestrator"
surgical_kill "renderer"
surgical_kill "keyboard_input"
surgical_kill "chtpm_parser_pal"
surgical_kill "chtpm_rgb_render"
surgical_kill "gl_mirror"

# Layer 2: Kill PAL scripts (prisc+x)
if pgrep -f '\.pal$' > /dev/null 2>&1; then
    echo "Killing residual prisc+x module(s) by .pal argument..."
    pkill -9 -f '\.pal$' 2>/dev/null
fi

# Layer 3: Kill any process running from this project
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

sleep 0.2

echo ""
echo "Checking for residual processes..."
if ps aux | grep -E "system/(renderer|keyboard_input|chtpm_parser_pal|orchestrator|chtpm_rgb_render|gl_mirror)" | grep -v grep >/dev/null 2>&1; then
    echo "WARNING: Some processes still running:"
    ps aux | grep -E "system/(renderer|keyboard_input|chtpm_parser_pal|orchestrator|chtpm_rgb_render|gl_mirror)" | grep -v grep
else
    echo "All wsr-pal processes terminated."
fi

rm -f pieces/system/quit_flag.txt 2>/dev/null
rm -f pieces/os/proc_list.txt 2>/dev/null

echo "Cleanup complete."
