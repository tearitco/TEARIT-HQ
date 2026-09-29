#!/bin/sh
# button.sh - launch robot-chat for one chatbot entity's own real dir.
# Same real dispatch convention every METHOD-launched button.sh in this
# house uses: bare METHOD action string invoked as
#   <action> '<entity_dir>' '<house_root>' >/dev/null 2>&1 &
# and the same argv[3]=entity_dir shape events-hq's own button.sh
# already established (khtpm_core_render.c's generic ARG3 instance-dir
# hook) - one shared robot-chat.xhtpm template, multi-instance by
# entity_dir, exactly like events-hq is multi-instance by event_pkg.
#
# Usage: sh button.sh <entity_dir> [house_root]
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
OPS_DIR="$HERE/../../_.monads/_.livedesk-taskbar/ops"
BIN="$OPS_DIR/+x/khtpm_core_render.+x"
CHTPM="$HERE/robot-chat.xhtpm"

if [ ! -x "$BIN" ]; then
    (cd "$OPS_DIR" && sh build_core_render.sh) || true
fi
sh "$HERE/ops/build_robot_chat_manager.sh" || true

ENTITY_DIR="${1:?usage: button.sh <entity_dir> [house_root]}"
ENTITY_DIR="$(cd "$ENTITY_DIR" && pwd)"
H="${2:-}"
if [ -z "$H" ] || [ ! -d "$H" ]; then H="$(cd "$HERE/../.." && pwd)"; fi
LABEL="$(basename "$ENTITY_DIR")"

# Scoped kill: only a prior robot-chat instance already open on THIS
# SAME entity_dir, never another entity's - same real byte-exact
# /proc/<pid>/cmdline compare events-hq's own button.sh uses (never a
# hand-escaped pgrep regex - house paths routinely carry emoji/parens).
same_entity_pids() {
    for pid in $(pgrep -f "khtpm_core_render\.\+x" 2>/dev/null || true); do
        if [ -r "/proc/$pid/cmdline" ]; then
            if tr '\0' '\n' < "/proc/$pid/cmdline" 2>/dev/null | grep -qxF "$CHTPM" && \
               tr '\0' '\n' < "/proc/$pid/cmdline" 2>/dev/null | grep -qxF "$ENTITY_DIR"; then
                echo "$pid"
            fi
        fi
    done
}
pids="$(same_entity_pids)"
if [ -n "$pids" ]; then
    echo "$pids" | xargs -r kill -TERM
    sleep 0.3
fi

setsid nohup "$BIN" "$H" "$CHTPM" "$ENTITY_DIR" "$LABEL" \
    >/tmp/robot-chat-"$LABEL".log 2>&1 < /dev/null &
disown 2>/dev/null || true
sleep 1

pids="$(same_entity_pids)"
if [ -n "$pids" ]; then
    echo "robot-chat launched for $LABEL (PID $pids)"
else
    echo "robot-chat: FAILED to launch for $LABEL - check /tmp/robot-chat-$LABEL.log" >&2
    cat /tmp/robot-chat-"$LABEL".log 2>/dev/null >&2
    exit 1
fi
