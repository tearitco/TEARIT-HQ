#!/bin/bash
# button.sh — launch open-hai (taskbar cell 14 "ai") as its own detached
# X11 process, same launch shape as open_db_hq.sh.
# Usage: button.sh <house_root>
#
# REAL, NEW 2026-09-01 (xperiments/khtpm-generic-dispatch-design.md's
# own real conversion writeup) - open-hai's own real hand-rolled
# renderer (khtpm_open_hai_render.c) is REPLACED here by the SAME
# shared renderer every other khtpm window uses (khtpm_core_render.+x),
# pointed at a real, checked-in bootstrap open-hai.xhtpm. That bootstrap
# declares a <module> tag (the renderer's own generic launch_module()
# mechanism, real, already proven for db-hq/events-hq/chat-hai, newly
# wired up for this default/popup mode too) which starts
# khtpm_open_hai_manager.+x as a real child process - the manager then
# overwrites the SAME .chtpm file with its own live, real projection
# (sessions/transcript/model/sound/tool-approval, via its own
# write_chtpm_projection()), picked up by the renderer's generic
# capability #1 (live .chtpm re-parse) within one tick. This script
# only ever launches ONE process (the renderer) - the manager is its
# real child, tied to its lifetime exactly like db-hq/events-hq/chat-
# hai's own managers already are, so closing the window also stops the
# manager (no separate PID for this script to track).
#
# khtpm_open_hai_render.c/khtpm_open_hai_manager.c's OWN old real entry
# point (launch_module() from inside the render process) is left in
# place, unused, as a real rollback reference - not deleted. build_
# open_hai.sh (the old render binary's own build script) is likewise
# untouched.
#
# REAL FIX 2026-08-13 (still applies - kept verbatim): this used to
# launch unconditionally, no matter how many instances were already
# running. Live testing found FIVE concurrent render processes alive
# at once, all racing on the same relay file/session dir -
# `pkill -9 <name>` does not reliably match this binary given the
# emoji-laden house-root path in argv, so repeated manual kills
# silently failed and every fresh launch piled another instance on
# top. Root cause + full writeup: _.0.aigent-testing-k9.txt "SCOPE
# ADDENDUM 2026-08-13". FIX: kill any existing instance via `pgrep -f`
# (matches the FULL command line, not just the truncated process name -
# reliable even with the emoji path) before launching, TERM then
# escalate to KILL, and confirm exactly one real PID after launch -
# same proven pattern as *.livedesk-taskbar/ops/run_khtpm_strip.sh's
# own kill_khtpm().
set -e
HOUSE_ROOT="${1:-}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "open-hai button.sh: need house_root as argv[1]" >&2
    exit 1
fi
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

HERE="$(cd "$(dirname "$0")" && pwd)"
OPS_DIR="$HERE/ops"
# open-hai.xhtpm is a STATIC template (x11-hq style) - the manager
# writes state/ui.txt, never this file, so no bootstrap/restore dance.
XHTPM="$HERE/open-hai.xhtpm"

# The shared renderer lives in the taskbar's own ops dir (khtpm_core_
# render.+x is shared house-wide, not open-hai's own binary) - real,
# same path every other default-mode consumer's own launcher uses.
RENDER_OPS_DIR="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops"
BIN="$RENDER_OPS_DIR/+x/khtpm_core_render.+x"
MANAGER_BIN="$OPS_DIR/+x/khtpm_open_hai_manager.+x"

if [ ! -x "$BIN" ]; then
    (cd "$RENDER_OPS_DIR" && sh build_core_render.sh) || true
fi
if [ ! -x "$BIN" ]; then
    echo "open-hai button.sh: build failed, missing $BIN" >&2
    exit 1
fi
if [ ! -x "$MANAGER_BIN" ]; then
    (cd "$OPS_DIR" && sh build_open_hai_manager.sh) || true
fi
if [ ! -x "$MANAGER_BIN" ]; then
    echo "open-hai button.sh: build failed, missing $MANAGER_BIN" >&2
    exit 1
fi
if [ ! -f "$XHTPM" ]; then
    echo "open-hai button.sh: missing template $XHTPM" >&2
    exit 1
fi

AUDIT_DIR="$HOUSE_ROOT/&.widgits/open-hai/pieces/audit"
mkdir -p "$AUDIT_DIR"

# NOTE: pgrep exits 1 (nonzero) when it finds nothing - under `set -e`
# a bare `pids="$(open_hai_pids)"` assignment would abort the whole
# script the moment no process is found. Every call site below is
# guarded with `|| true` for exactly this reason - lost an hour to
# this exact silent-abort live before catching it (script would print
# the "killing existing instance" line, then just vanish with no
# further output and no launched process, because set -e killed it
# right after the post-kill pgrep found zero remaining PIDs).
#
# Matches THIS specific renderer+chtpm combo (not any other khtpm_core_
# render.+x window - entity menus, db-hq, etc. all share that same
# binary name), excluding any real per-instance chat carrying its own
# `--data-root` (chat_button.sh's own real instances, left alone here -
# it symmetrically only kills instances bound to ITS data root). /proc
# cmdline is NUL-separated: join tokens with spaces before the
# substring check.
open_hai_pids() {
    local p joined
    for p in $(pgrep -f "khtpm_core_render\.\+x.*open-hai\.xhtpm" 2>/dev/null || true); do
        joined="$(tr '\0' ' ' < "/proc/$p/cmdline" 2>/dev/null || true)"
        case "$joined" in
            *--data-root\ *) ;;   # per-instance chat - not ours to kill
            *) echo "$p" ;;
        esac
    done
    return 0
}

# REAL, NEW 2026-09-01 - also kill any leftover instance of the OLD
# hand-rolled renderer this launcher used to start (khtpm_open_hai_
# render.+x) - a real, one-time transition safeguard so a stale pre-
# switch process (launched by an older copy of this script, or left
# running from before this change) doesn't keep its own session/relay
# files open alongside the new mechanism. Same real --data-root
# exclusion as open_hai_pids() above.
old_render_pids() {
    local p joined
    for p in $(pgrep -f "khtpm_open_hai_render\.\+x" 2>/dev/null || true); do
        joined="$(tr '\0' ' ' < "/proc/$p/cmdline" 2>/dev/null || true)"
        case "$joined" in
            *--data-root\ *) ;;
            *) echo "$p" ;;
        esac
    done
    return 0
}

pids="$(open_hai_pids)$(printf '\n')$(old_render_pids)"
pids="$(echo "$pids" | grep -v '^$' || true)"
if [ -n "$pids" ]; then
    echo "open-hai button.sh: killing existing instance(s): $(echo $pids | tr '\n' ' ')"
    echo "$pids" | xargs -r kill -TERM
    sleep 1
    pids="$(open_hai_pids)$(printf '\n')$(old_render_pids)"
    pids="$(echo "$pids" | grep -v '^$' || true)"
    if [ -n "$pids" ]; then
        echo "open-hai button.sh: still alive after TERM, escalating to KILL: $(echo $pids | tr '\n' ' ')"
        echo "$pids" | xargs -r kill -KILL
        sleep 1
    fi
fi

setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" \
    >"$AUDIT_DIR/open-hai.log" 2>&1 < /dev/null &
# REAL FIX 2026-08-25 - see open_db_hq.sh's own identical line for the
# full rationale (au11-hq direct request: "why cant u write that final
# pid to a file and reconsume it?") - records THIS script's own $!, the
# real window binary's own PID/session leader, not the outer taskbar
# wrapper's.
echo $! >> "$HOUSE_ROOT/#.desktop/livedesk_launched_pids.txt" 2>/dev/null || true
disown 2>/dev/null || true
sleep 1

pids="$(open_hai_pids)"
n="$(echo "$pids" | grep -c . || true)"
if [ "$n" = "1" ]; then
    echo "open-hai launched (PID $pids, log=$AUDIT_DIR/open-hai.log)"
elif [ "$n" -gt 1 ] 2>/dev/null; then
    echo "open-hai button.sh: WARNING - $n instances alive after launch (expected 1): $(echo $pids | tr '\n' ' ')" >&2
else
    echo "open-hai button.sh: FAILED to launch - check the log:" >&2
    cat "$AUDIT_DIR/open-hai.log" 2>/dev/null >&2
    exit 1
fi
