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

# 2026-09-03 static-xhtpm port: route to button-pal.sh (chat-hai.xhtpm +
# compiled projector). CHAT_HAI_ROLLBACK=1 keeps the old chat-hai.chtpm.
[ -z "${CHAT_HAI_ROLLBACK:-}" ] && exec sh "$HERE/button-pal.sh" "$HOUSE_ROOT"
OPS_DIR="$HERE/ops"
# REAL §5d.12 (2026-08-16, khtpm-merge-how2.md) - chat-hai merged into
# the shared khtpm_core_render.+x binary (last of the 5 window
# apps). Old chat_hai_hq_render.+x kept as reference/rollback, unused.
BIN="$(cd "$HERE/../../*.monads/*.livedesk-taskbar/ops" && pwd)/+x/khtpm_core_render.+x"
CHTPM="$HERE/chat-hai.chtpm"
BOOTSTRAP_TEMPLATE="$HERE/chat-hai.chtpm.bootstrap"

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
chat_hai_pids() { pgrep -f "khtpm_core_render\.\+x.*chat-hai\.chtpm" 2>/dev/null || true; }
chat_hai_loop_pids() { pgrep -f "chat_hai_loop\.sh" 2>/dev/null || true; }
# REAL FIX 2026-09-01 (chat-hai's own migration onto the shared generic
# sidebar/panel/scrolllist/cli_io path) - the projector is a THIRD real
# process now (chat_hai_projector.sh, launched as the loop's own
# background child) and must be tracked/killed here too. Found live: a
# stale projector surviving past the loop/renderer kill can win a real
# race - it rewrites chat-hai.chtpm (no <module> tag, by design - see
# that script's own header comment) AFTER the bootstrap restore below
# but BEFORE the freshly-launched renderer's own one-time module scan,
# silently eating the <module> tag on the very run meant to fix it.
chat_hai_projector_pids() { pgrep -f "chat_hai_projector\.sh" 2>/dev/null || true; }

pids="$(chat_hai_pids) $(chat_hai_loop_pids) $(chat_hai_projector_pids)"
pids="$(echo "$pids" | tr ' ' '\n' | grep -v '^$' || true)"
if [ -n "$pids" ]; then
    echo "chat-hai button.sh: killing existing instance(s): $(echo $pids | tr '\n' ' ')"
    echo "$pids" | xargs -r kill -TERM
    sleep 1
    pids="$(chat_hai_pids) $(chat_hai_loop_pids) $(chat_hai_projector_pids)"
    pids="$(echo "$pids" | tr ' ' '\n' | grep -v '^$' || true)"
    if [ -n "$pids" ]; then
        echo "chat-hai button.sh: still alive after TERM, escalating to KILL: $(echo $pids | tr '\n' ' ')"
        echo "$pids" | xargs -r kill -KILL
        sleep 1
    fi
fi

# REAL FIX 2026-09-01 (chat-hai's own migration onto the shared
# generic sidebar/panel/scrolllist/cli_io path - chat-hai.chtpm is now
# a live, continuously-regenerated PROJECTION written by
# chat_hai_projector.sh, same real "self-healing bootstrap" fix
# open-hai/network-browser already needed: any stray write after the
# projector/loop dies mid-write can permanently erase the <module> tag,
# silently breaking every future launch - restore from the permanent,
# never-overwritten bootstrap template whenever that's found missing.
# MUST run AFTER the kill-and-confirm-dead block above, not before -
# found live: restoring the bootstrap first, then killing, lets a
# still-alive projector's own next tick clobber the just-restored
# bootstrap before this run's renderer ever reads it.
if [ -f "$BOOTSTRAP_TEMPLATE" ] && ! grep -q '<module' "$CHTPM" 2>/dev/null; then
    echo "chat-hai button.sh: $CHTPM lost its <module> tag (stray write) - restoring from $BOOTSTRAP_TEMPLATE"
    cp "$BOOTSTRAP_TEMPLATE" "$CHTPM"
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
# REAL FIX 2026-09-01 (found live, investigating a "loop wasn't self-
# spawned" warning that fired unpredictably against BOTH the original
# launch code and a real, tested alternative - confirmed to be neither
# a regression nor a race): chat_hai_loop.sh's own real per-round
# operation forks short-lived child subshells that ALSO carry
# "chat_hai_loop.sh" in their own inherited argv (confirmed live via
# `ps -eo pid,ppid,cmd` - a real grandchild subshell, own PPID pointing
# back at the real persistent loop, born ~40s into a real run) - a
# pgrep -f match on the script name can never reliably equal exactly 1
# for a script that behaves this way, by design, not a bug in the loop
# itself. Real fix: treat "at least 1" as healthy, not "exactly 1" -
# the exactly-1 renderer check below is unaffected (khtpm_core_render.+x
# has no equivalent transient-subshell behavior).
loop_n="$(chat_hai_loop_pids | grep -c . || true)"
if [ "$n" = "1" ] && [ "$loop_n" -ge "1" ] 2>/dev/null; then
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
