#!/bin/bash
# kill_all.sh - muchi-pal-agent process cleanup
# 3-layer cascading kill: process group → file-backed PID → surgical SIGKILL

surgical_kill() {
    local name="$1"
    local pattern="system/${name}"
    if pgrep -f "$pattern" > /dev/null 2>&1; then
        echo "Killing $name..."
        pkill -9 -f "$pattern" 2>/dev/null
    fi
}

echo "=== muchi-pal-agent kill_all.sh - surgical cleanup ==="

# Layer 1: Kill by binary name (surgical)
surgical_kill "orchestrator"
surgical_kill "renderer"
surgical_kill "keyboard_input"
surgical_kill "chtpm_parser_pal"
surgical_kill "chtpm_rgb_render"
surgical_kill "gl_mirror"

# Layer 1b: Kill manager ops.
# ROOT-CAUSED 2026-07-26 (see #.haiku+/!.xyzos-pitfalls+1.txt): the +
# MUST be escaped - pgrep/pkill treat an unescaped + as an extended-
# regex quantifier ("one or more of the preceding char"), not a
# literal plus, so "manager/+x/..." never matched the real literal
# path at all - this silently killed nothing, every single run, until
# fixed (same bug found in 044/041.pal-chain/041.pal-forum's own
# kill_all.sh files).
if pgrep -f "manager/\+x/path_nav_manager" > /dev/null 2>&1; then
    echo "Killing path_nav_manager..."
    pkill -9 -f "manager/\+x/path_nav_manager" 2>/dev/null
fi

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
if ps aux | grep -E "system/(renderer|keyboard_input|chtpm_parser_pal|orchestrator|chtpm_rgb_render|gl_mirror)|path_nav_manager" | grep -v grep >/dev/null 2>&1; then
    echo "WARNING: Some processes still running:"
    ps aux | grep -E "system/(renderer|keyboard_input|chtpm_parser_pal|orchestrator|chtpm_rgb_render|gl_mirror)|path_nav_manager" | grep -v grep
else
    echo "All muchi-pal-agent processes terminated."
fi

rm -f pieces/system/quit_flag.txt 2>/dev/null
rm -f pieces/os/proc_list.txt 2>/dev/null

echo "Cleanup complete."
