#!/bin/bash
# kill_all.sh - pal-chain process cleanup (mass-refactor 2026-07-26,
# ported from 01.muchi-pals-🥚️-13.01's session-scoped variant, itself
# ported from 101.mutaclsym's pieces/os/kill_all.sh).
#
# SESSION-SCOPED: pal-chain runs multiple concurrent sessions (button.sh's
# own "run" action, pieces/sessions/<id>/), each launching binaries via
# the SAME relative argv from its own throwaway directory - a plain
# substring pkill cannot tell sessions apart by command line alone and
# would kill every OTHER concurrent session's processes too (the exact
# bug button.sh's own kill_own_module() helper exists to avoid for the
# persistent pal module). $1: optional session directory to scope to
# (matched via /proc/<pid>/cwd). Called this way by system/orchestrator.c
# (always passes its own cwd). No $1 = global sweep, for the top-level
# manual `./button.sh kill`.
SESSION_DIR="$1"

surgical_kill() {
    local name="$1"
    local pattern="$2"
    if [ -n "$SESSION_DIR" ]; then
        for pid in $(pgrep -f "$pattern" 2>/dev/null); do
            local cwd
            cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null)"
            # REAL BUG, LIVE-CAUGHT (2026-07-26): if this session's own
            # directory was already `rm -rf`'d (button.sh's own trap does
            # this) by the time this runs, a process that still has it
            # open as cwd shows up here as "$SESSION_DIR (deleted)", not
            # the plain path - a byte-for-byte comparison against
            # SESSION_DIR then silently NEVER matches, permanently
            # orphaning that process (observed: chain_miner.+x and
            # chain_inbox_watcher.+x leaking across every single test
            # run, one each, indefinitely, until manually found and
            # killed). Strip the suffix before comparing.
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

echo "=== pal-chain kill_all.sh - surgical cleanup ${SESSION_DIR:+(session $SESSION_DIR)} ==="

surgical_kill "orchestrator" "system/orchestrator"
surgical_kill "renderer" "system/renderer"
surgical_kill "keyboard_input" "system/keyboard_input"
surgical_kill "chtpm_parser_pal" "system/chtpm_parser_pal"
surgical_kill "chtpm_rgb_render" "system/chtpm_rgb_render"
# ROOT-CAUSED 2026-07-26 (see #.haiku+/!.xyzos-pitfalls+1.txt): the +
# in these patterns MUST be escaped - pgrep/pkill treat an unescaped +
# as an extended-regex quantifier ("one or more of the preceding
# char"), not a literal plus, so "ops/+x/..." (unescaped) never
# matches the real literal path "ops/+x/..." at all. This silently
# killed nothing for these 3 daemons every single run until fixed -
# same bug as the "not root-caused" mystery documented in 044.pal-chat-
# irc's own kill_all.sh.
surgical_kill "palnet_peer" "ops/\+x/palnet_peer"
surgical_kill "chain_miner" "ops/\+x/chain_miner"
surgical_kill "chain_inbox_watcher" "ops/\+x/chain_inbox_watcher"

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
    if ps aux | grep -E "system/(renderer|keyboard_input|chtpm_parser_pal|orchestrator|chtpm_rgb_render)|ops/\+x/(palnet_peer|chain_miner|chain_inbox_watcher)" | grep -v grep >/dev/null 2>&1; then
        echo "WARNING: Some processes still running:"
        ps aux | grep -E "system/(renderer|keyboard_input|chtpm_parser_pal|orchestrator|chtpm_rgb_render)|ops/\+x/(palnet_peer|chain_miner|chain_inbox_watcher)" | grep -v grep
    else
        echo "All pal-chain processes terminated."
    fi
    echo "Cleanup complete."
fi
