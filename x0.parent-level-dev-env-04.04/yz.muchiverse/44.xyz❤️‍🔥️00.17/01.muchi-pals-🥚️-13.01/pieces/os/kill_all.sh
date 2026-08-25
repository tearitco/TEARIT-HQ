#!/bin/bash
# kill_all.sh - muchi-pals process cleanup (mass-refactor 2026-07-26,
# ported from 101.mutaclsym's pieces/os/kill_all.sh)
#
# SESSION-SCOPED, UNLIKE MUTACLSYM'S (deliberate adaptation): mutaclsym
# only ever runs one game at a time, so a bare `pkill -9 -f "system/
# renderer"` is safe there - it can only ever match that one instance.
# muchi-pals runs MULTIPLE CONCURRENT SESSIONS (button.sh's own "run"
# action, dox/03-session-isolation.md), each launching binaries via the
# SAME relative argv (./system/renderer, ./system/chtpm_parser_pal, ...)
# from its own throwaway pieces/sessions/<id>/ directory - a plain
# substring pkill cannot tell sessions apart by command line alone and
# would kill every OTHER concurrent session's processes too, exactly the
# bug button.sh's own kill_own_module() helper was written to avoid for
# the persistent pal module. This script accepts an optional $1: the
# session directory to scope killing to (matched via /proc/<pid>/cwd,
# which IS unique per session). Called this way by system/orchestrator.c
# (always passes its own cwd). If $1 is omitted, falls back to a global
# sweep across every session - the deliberate behavior for the top-level
# manual `./button.sh kill` "nuclear option", which is meant to be global.
SESSION_DIR="$1"

surgical_kill() {
    local name="$1"
    local pattern="system/${name}"
    if [ -n "$SESSION_DIR" ]; then
        for pid in $(pgrep -f "$pattern" 2>/dev/null); do
            local cwd
            cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null)"
            if [ "$cwd" = "$SESSION_DIR" ]; then
                echo "Killing $name (PID $pid, session $SESSION_DIR)..."
                kill -9 "$pid" 2>/dev/null
            fi
        done
    else
        if pgrep -f "$pattern" > /dev/null 2>&1; then
            echo "Killing $name (all sessions)..."
            pkill -9 -f "$pattern" 2>/dev/null
        fi
    fi
}

echo "=== muchi-pals kill_all.sh - surgical cleanup ${SESSION_DIR:+(session $SESSION_DIR)} ==="

# Layer 1: Kill by binary name (surgical, session-scoped if SESSION_DIR given)
surgical_kill "orchestrator"
surgical_kill "renderer"
surgical_kill "keyboard_input"
surgical_kill "chtpm_parser_pal"

# Layer 2: Kill this session's own persistent prisc+x module by cwd match -
# same technique button.sh's own kill_own_module() already uses, since
# every session launches the SAME relative argv (pal/main_loop_chtpm.pal).
if [ -n "$SESSION_DIR" ]; then
    for pid in $(pgrep -f 'system/prisc\+x' 2>/dev/null); do
        cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null)"
        if [ "$cwd" = "$SESSION_DIR" ]; then
            echo "Killing residual prisc+x module (PID $pid, session $SESSION_DIR)..."
            kill -9 "$pid" 2>/dev/null
        fi
    done
else
    if pgrep -f 'system/prisc\+x' > /dev/null 2>&1; then
        echo "Killing residual prisc+x module(s) (all sessions)..."
        pkill -9 -f 'system/prisc\+x' 2>/dev/null
    fi
fi

sleep 0.2

if [ -n "$SESSION_DIR" ]; then
    echo "Session cleanup complete for $SESSION_DIR."
else
    echo ""
    echo "Checking for residual processes..."
    if ps aux | grep -E "system/(renderer|keyboard_input|chtpm_parser_pal|orchestrator)" | grep -v grep >/dev/null 2>&1; then
        echo "WARNING: Some processes still running:"
        ps aux | grep -E "system/(renderer|keyboard_input|chtpm_parser_pal|orchestrator)" | grep -v grep
    else
        echo "All muchi-pals processes terminated."
    fi
    echo "Cleanup complete."
fi
