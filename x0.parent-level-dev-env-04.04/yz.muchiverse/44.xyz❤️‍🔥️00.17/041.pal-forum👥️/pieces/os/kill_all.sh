#!/bin/bash
# kill_all.sh - pal-forum process cleanup (mass-refactor 2026-07-26,
# ported from 041.pal-chain's own session-scoped kill_all.sh, itself
# ported from 01.muchi-pals-🥚️-13.01's variant).
#
# SESSION-SCOPED: pal-forum runs multiple concurrent sessions (button.sh's
# own "run" action, pieces/sessions/<id>/), each launching binaries via
# the SAME relative argv from its own throwaway directory - a plain
# substring pkill cannot tell sessions apart by command line alone and
# would kill every OTHER concurrent session's processes too. $1: optional
# session directory to scope to (matched via /proc/<pid>/cwd). Called
# this way by system/orchestrator.c (always passes its own cwd). No $1 =
# global sweep, for the top-level manual `./button.sh kill`.
SESSION_DIR="$1"

surgical_kill() {
    local name="$1"
    local pattern="$2"
    if [ -n "$SESSION_DIR" ]; then
        for pid in $(pgrep -f "$pattern" 2>/dev/null); do
            local cwd
            cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null)"
            # REAL BUG, LIVE-CAUGHT (2026-07-26, see pal-chain's own
            # identical fix + #.haiku+/!.xyzos-pitfalls+1.txt): once this
            # session's own directory has been `rm -rf`'d, a process
            # that still has it open as cwd shows up here as
            # "$SESSION_DIR (deleted)" - a byte-for-byte compare then
            # never matches, permanently orphaning that process. Strip
            # the suffix before comparing.
            cwd="${cwd% (deleted)}"
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

echo "=== pal-forum kill_all.sh - surgical cleanup ${SESSION_DIR:+(session $SESSION_DIR)} ==="

surgical_kill "orchestrator" "system/orchestrator"
surgical_kill "renderer" "system/renderer"
surgical_kill "keyboard_input" "system/keyboard_input"
surgical_kill "chtpm_parser_pal" "system/chtpm_parser_pal"
surgical_kill "chtpm_rgb_render" "system/chtpm_rgb_render"
# ROOT-CAUSED 2026-07-26 (see #.haiku+/!.xyzos-pitfalls+1.txt): + must be
# escaped - pgrep/pkill treat an unescaped + as an extended-regex
# quantifier, so "ops/+x/..." never matched the real literal path at
# all, silently killing nothing every run.
surgical_kill "palnet_peer" "ops/\+x/palnet_peer"
surgical_kill "forum_inbox_watcher" "ops/\+x/forum_inbox_watcher"

if [ -n "$SESSION_DIR" ]; then
    for pid in $(pgrep -f 'system/prisc\+x' 2>/dev/null); do
        cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null)"
        cwd="${cwd% (deleted)}"
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
    if ps aux | grep -E "system/(renderer|keyboard_input|chtpm_parser_pal|orchestrator|chtpm_rgb_render)|ops/\+x/(palnet_peer|forum_inbox_watcher)" | grep -v grep >/dev/null 2>&1; then
        echo "WARNING: Some processes still running:"
        ps aux | grep -E "system/(renderer|keyboard_input|chtpm_parser_pal|orchestrator|chtpm_rgb_render)|ops/\+x/(palnet_peer|forum_inbox_watcher)" | grep -v grep
    else
        echo "All pal-forum processes terminated."
    fi
    echo "Cleanup complete."
fi
