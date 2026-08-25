#!/bin/bash
# kill_all.sh - pal-chat-irc process cleanup
# 3-layer cascading kill: process group → file-backed PID → surgical SIGKILL

surgical_kill() {
    local name="$1"
    local pattern="system/${name}"
    if pgrep -f "$pattern" > /dev/null 2>&1; then
        echo "Killing $name..."
        pkill -9 -f "$pattern" 2>/dev/null
    fi
}

echo "=== pal-chat-irc kill_all.sh - surgical cleanup ==="

# Layer 1: Kill by binary name (surgical)
surgical_kill "orchestrator"
surgical_kill "renderer"
surgical_kill "keyboard_input"
surgical_kill "chtpm_parser_pal"

# Layer 1b: Kill daemon ops.
# ROOT-CAUSED 2026-07-26 (was marked "not root-caused" below - it is
# now): the pgrep pattern here was "ops/+x/..." with the + UNESCAPED -
# in pgrep/pkill's own extended-regex matching, an unescaped `+` is a
# quantifier ("one or more of the preceding char"), not a literal plus,
# so this pattern was silently searching for "ops" + one-or-more
# slashes + "x/..." - which never matches the real literal path
# "ops/+x/..." at all. The `for pid in $(pgrep ...)` loop below was
# therefore ALWAYS iterating over zero PIDs, killing nothing, every
# single time - while the correctly-escaped detection check further
# down ("ops/\+x/...") correctly kept finding the (never-killed)
# survivors, producing exactly the "pgrep finds them but pkill doesn't
# kill them" symptom that used to be documented here as "not root-
# caused." Escaping the + fixes it for real.
for pid in $(pgrep -f "ops/\+x/palnet_peer" 2>/dev/null); do
    echo "Killing palnet_peer (PID $pid)..."
    kill -9 "$pid" 2>/dev/null
done
for pid in $(pgrep -f "ops/\+x/chat_inbox_watcher" 2>/dev/null); do
    echo "Killing chat_inbox_watcher (PID $pid)..."
    kill -9 "$pid" 2>/dev/null
done

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
# XYZOS-PITFALLS #20/21 (2026-07-26): this check used to omit
# palnet_peer/chat_inbox_watcher entirely, so it could print "All
# terminated" while those two daemons were still alive and burning CPU -
# they ARE targeted above (Layer 1b) but that kill was never verified.
if ps aux | grep -E "system/(renderer|keyboard_input|chtpm_parser_pal|orchestrator)|ops/\+x/(palnet_peer|chat_inbox_watcher)" | grep -v grep >/dev/null 2>&1; then
    echo "WARNING: Some processes still running:"
    ps aux | grep -E "system/(renderer|keyboard_input|chtpm_parser_pal|orchestrator)|ops/\+x/(palnet_peer|chat_inbox_watcher)" | grep -v grep
    # one more direct, unconditional pass (no pgrep-gate race) before giving up
    pkill -9 -f "ops/\+x/palnet_peer" 2>/dev/null
    pkill -9 -f "ops/\+x/chat_inbox_watcher" 2>/dev/null
    sleep 0.3
    if ps aux | grep -E "system/(renderer|keyboard_input|chtpm_parser_pal|orchestrator)|ops/\+x/(palnet_peer|chat_inbox_watcher)" | grep -v grep >/dev/null 2>&1; then
        echo "WARNING: still running after retry - manual kill -9 by PID needed."
    else
        echo "All pal-chat-irc processes terminated (after retry)."
    fi
else
    echo "All pal-chat-irc processes terminated."
fi

rm -f pieces/system/quit_flag.txt 2>/dev/null
rm -f pieces/os/proc_list.txt 2>/dev/null

echo "Cleanup complete."
