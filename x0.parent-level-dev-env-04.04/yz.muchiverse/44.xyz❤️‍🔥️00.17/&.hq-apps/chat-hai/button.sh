#!/bin/bash
# button.sh — launch chat-hai (taskbar cell 14 "h-ai" Chat submenu) as its own
# detached X11 process. Follows exact open-hai pattern but passes chtpm path.
# Usage: button.sh <house_root>
set -e
HOUSE_ROOT="${1:-}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "chat-hai button.sh: need house_root as argv[1]" >&2
    exit 1
fi
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

HERE="$(cd "$(dirname "$0")" && pwd)"
OPS_DIR="$HERE/ops"
# REAL §5d.12 (2026-08-16, khtpm-merge-how2.md) - chat-hai merged into
# the shared khtpm_entity_menu_render.+x binary (last of the 5 window
# apps). Old chat_hai_hq_render.+x kept as reference/rollback, unused.
BIN="$(cd "$HERE/../../*.monads/*.livedesk-taskbar/ops" && pwd)/+x/khtpm_entity_menu_render.+x"
CHTPM="$HERE/chat-hai.chtpm"

if [ ! -x "$BIN" ]; then
    echo "chat-hai button.sh: build failed, missing $BIN" >&2
    exit 1
fi

AUDIT_DIR="$HOUSE_ROOT/&.hq-apps/chat-hai/pieces/audit"
mkdir -p "$AUDIT_DIR"

# REAL FIX 2026-08-16 (Stage 2d shell/manager split, au11-hq/khtpm-
# merge-how2.md + local-2do-15.txt's own chat-hai entry): this used to
# manually start chat_hai_loop.sh itself, separately from the renderer -
# not the real <module> mechanism. chat-hai.chtpm now has a real
# <module src="ops/chat_hai_loop.sh"/> tag; chat_hai_hq_render.c's own
# main() reads it and fork()+execv()s the loop itself (ported
# launch_module(), same shape as db-hq/events-hq). This script now only
# launches the renderer - the loop is the renderer's own child process.
#
# REAL BEHAVIOR CHANGE, confirmed with direct instruction 2026-08-16:
# the loop used to deliberately OUTLIVE the renderer (this script's own
# "leave it running" guard, removed here) - tying its lifetime to the
# renderer (full parity with db-hq/events-hq) means closing chat-hai's
# window now ALSO stops the persona loop, a real, deliberate change from
# the old behavior.
chat_hai_pids() { pgrep -f "khtpm_entity_menu_render\.\+x.*chat-hai\.chtpm" 2>/dev/null || true; }
chat_hai_loop_pids() { pgrep -f "chat_hai_loop\.sh" 2>/dev/null || true; }

pids="$(chat_hai_pids) $(chat_hai_loop_pids)"
pids="$(echo "$pids" | tr ' ' '\n' | grep -v '^$' || true)"
if [ -n "$pids" ]; then
    echo "chat-hai button.sh: killing existing instance(s): $(echo $pids | tr '\n' ' ')"
    echo "$pids" | xargs -r kill -TERM
    sleep 1
    pids="$(chat_hai_pids) $(chat_hai_loop_pids)"
    pids="$(echo "$pids" | tr ' ' '\n' | grep -v '^$' || true)"
    if [ -n "$pids" ]; then
        echo "chat-hai button.sh: still alive after TERM, escalating to KILL: $(echo $pids | tr '\n' ' ')"
        echo "$pids" | xargs -r kill -KILL
        sleep 1
    fi
fi

setsid nohup "$BIN" "$HOUSE_ROOT" "$CHTPM" \
    >"$AUDIT_DIR/chat-hai.log" 2>&1 < /dev/null &
# REAL FIX 2026-08-25 - see open_db_hq.sh's own identical line for the
# full rationale (au11-hq direct request: "why cant u write that final
# pid to a file and reconsume it?") - records THIS script's own $!, the
# real window binary's own PID/session leader, not the outer taskbar
# wrapper's.
echo $! >> "$HOUSE_ROOT/#.desktop/livedesk_launched_pids.txt" 2>/dev/null || true
disown 2>/dev/null || true
sleep 1

pids="$(chat_hai_pids)"
n="$(echo "$pids" | grep -c . || true)"
loop_n="$(chat_hai_loop_pids | grep -c . || true)"
if [ "$n" = "1" ] && [ "$loop_n" = "1" ]; then
    echo "chat-hai launched (PID $pids, self-spawned loop PID $(chat_hai_loop_pids), log=$AUDIT_DIR/chat-hai.log)"
elif [ "$n" -gt 1 ] 2>/dev/null; then
    echo "chat-hai button.sh: WARNING - $n instances alive after launch (expected 1): $(echo $pids | tr '\n' ' ')" >&2
elif [ "$n" = "1" ]; then
    echo "chat-hai button.sh: WARNING - renderer launched but loop wasn't self-spawned (check <module> tag in chat-hai.chtpm)" >&2
else
    echo "chat-hai button.sh: FAILED to launch - check the log:" >&2
    cat "$AUDIT_DIR/chat-hai.log" 2>/dev/null >&2
    exit 1
fi
