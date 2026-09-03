#!/bin/bash
# button.sh — launch network-browser-hq as its own detached X11 process.
# Usage: button.sh <house_root>
#
# REAL, NEW 2026-09-01 (khtpm-generic-dispatch-design.md's own real
# conversion writeup) - network-browser's own real hand-rolled renderer
# (network_browser_render.c) is REPLACED here by the SAME shared
# renderer every other khtpm window uses (khtpm_core_render.+x), pointed
# at a real, checked-in bootstrap network-browser-hq.chtpm. That bootstrap
# declares a <module> tag (the renderer's own generic launch_module()
# mechanism, real, already proven for db-hq/events-hq/chat-hai/open-hai,
# newly wired up for this default/popup mode too) which starts
# network_browser_manager.+x as a real child process - the manager then
# overwrites the SAME .chtpm file with its own live, real projection
# (current page/address/links/status, via its own write_chtpm_projection()),
# picked up by the renderer's generic capability #1 (live .chtpm re-parse)
# within one tick. This script only ever launches ONE process (the
# renderer) - the manager is its real child, tied to its lifetime exactly
# like db-hq/events-hq/chat-hai/open-hai's own managers already are, so
# closing the window also stops the manager (no separate PID for this
# script to track).
#
# network_browser_render.c's OWN old real entry point (the real manager)
# is left in place, unused, as a real rollback reference - not deleted.
# build.sh (the old render binary's own build script) is likewise
# untouched.
set -e
HOUSE_ROOT="${1:-}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "network-browser button.sh: need house_root as argv[1]" >&2
    exit 1
fi
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

HERE="$(cd "$(dirname "$0")" && pwd)"
# 2026-09-03 static-template port: default to the STATIC network-browser-hq.xhtpm
# (manager runs in g_mode_ui via <module id="ui">, writes #.desktop/
# network-browser-hq_ui.txt - no per-tick markup regen). NB_ROLLBACK=1
# selects the pre-port generated .chtpm path instead.
if [ -n "${NB_ROLLBACK:-}" ]; then
    CHTPM="$HERE/network-browser-hq.chtpm"
else
    CHTPM="$HERE/network-browser-hq.xhtpm"
fi

# The shared renderer lives in the taskbar's own ops dir (khtpm_core_
# render.+x is shared house-wide, not network-browser's own binary) - real,
# same path every other default-mode consumer's own launcher uses.
RENDER_OPS_DIR="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops"
BIN="$RENDER_OPS_DIR/+x/khtpm_core_render.+x"
MANAGER_BIN="$HERE/+x/network_browser_manager.+x"

if [ ! -x "$BIN" ]; then
    (cd "$RENDER_OPS_DIR" && sh build_core_render.sh) || true
fi
if [ ! -x "$BIN" ]; then
    echo "network-browser button.sh: build failed, missing $BIN" >&2
    exit 1
fi
if [ ! -x "$MANAGER_BIN" ]; then
    (cd "$HERE" && sh build_network_browser_manager.sh) || true
fi
if [ ! -x "$MANAGER_BIN" ]; then
    echo "network-browser button.sh: build failed, missing $MANAGER_BIN" >&2
    exit 1
fi

BOOTSTRAP_TEMPLATE="$HERE/network-browser-hq.chtpm.bootstrap"
# The .xhtpm is static and checked in - it is never overwritten, so the
# bootstrap self-heal below only applies to the rollback .chtpm path.
if [ -z "${NB_ROLLBACK:-}" ]; then
    [ -f "$CHTPM" ] || { echo "network-browser button.sh: missing $CHTPM" >&2; exit 1; }
elif [ ! -f "$BOOTSTRAP_TEMPLATE" ]; then
    echo "network-browser button.sh: missing bootstrap $BOOTSTRAP_TEMPLATE" >&2
    exit 1
fi

# REAL FIX 2026-09-01 (live report: after the manager's own real
# projection overwrites network-browser-hq.chtpm - its normal, intended
# job every tick - a STRAY manager process (an orphan from a previous
# relaunch, or any other writer) landing one more write on this SAME path
# after the window that launched it is gone permanently erases the
# <module> tag this launcher's whole mechanism depends on: the NEXT launch
# finds no module tag, and NEVER starts a manager again at all, silently,
# with zero error - confirmed live, reproduced multiple times).
# Self-heal here, every launch: a real, permanent, never-overwritten
# template (network-browser-hq.chtpm.bootstrap, copied once from the real
# checked-in bootstrap) restores the live path whenever it's missing its
# own <module> tag, before this script ever execs the renderer.
if [ -n "${NB_ROLLBACK:-}" ]; then
    if [ ! -f "$CHTPM" ]; then
        cp "$BOOTSTRAP_TEMPLATE" "$CHTPM"
    fi
    if ! grep -q '<module' "$CHTPM" 2>/dev/null; then
        echo "network-browser button.sh: $CHTPM lost its <module> tag (stray write) - restoring from $BOOTSTRAP_TEMPLATE"
        cp "$BOOTSTRAP_TEMPLATE" "$CHTPM"
    fi
fi

AUDIT_DIR="$HOUSE_ROOT/&.hq-apps/network/audit"
mkdir -p "$AUDIT_DIR"

# NOTE: pgrep exits 1 (nonzero) when it finds nothing - under `set -e`
# a bare `pids="$(network_browser_pids)"` assignment would abort the whole
# script the moment no process is found. Every call site below is
# guarded with `|| true` for exactly this reason.
#
# Matches THIS specific renderer+chtpm combo (not any other khtpm_core_
# render.+x window - entity menus, db-hq, etc. all share that same
# binary name).
network_browser_pids() {
    local p joined
    for p in $(pgrep -f "khtpm_core_render\.\+x.*network-browser-hq\.[cx]htpm" 2>/dev/null || true); do
        joined="$(tr '\0' ' ' < "/proc/$p/cmdline" 2>/dev/null || true)"
        echo "$p"
    done
    return 0
}

# REAL, NEW 2026-09-01 - also kill any leftover instance of the OLD
# hand-rolled renderer this launcher used to start (network_browser_
# render.+x) - a real, one-time transition safeguard so a stale pre-
# switch process doesn't keep its own network fetch running.
old_render_pids() {
    local p joined
    for p in $(pgrep -f "network_browser_render\.\+x" 2>/dev/null || true); do
        echo "$p"
    done
    return 0
}

pids="$(network_browser_pids)$(printf '\n')$(old_render_pids)"
pids="$(echo "$pids" | grep -v '^$' || true)"
if [ -n "$pids" ]; then
    echo "network-browser button.sh: killing existing instance(s): $(echo $pids | tr '\n' ' ')"
    echo "$pids" | xargs -r kill -TERM
    sleep 1
    pids="$(network_browser_pids)$(printf '\n')$(old_render_pids)"
    pids="$(echo "$pids" | grep -v '^$' || true)"
    if [ -n "$pids" ]; then
        echo "network-browser button.sh: still alive after TERM, escalating to KILL: $(echo $pids | tr '\n' ' ')"
        echo "$pids" | xargs -r kill -KILL
        sleep 1
    fi
fi

setsid nohup "$BIN" "$HOUSE_ROOT" "$CHTPM" \
    >"$AUDIT_DIR/network-browser.log" 2>&1 < /dev/null &
echo $! >> "$HOUSE_ROOT/#.desktop/livedesk_launched_pids.txt" 2>/dev/null || true
disown 2>/dev/null || true
sleep 1

pids="$(network_browser_pids)"
n="$(echo "$pids" | grep -c . || true)"
if [ "$n" = "1" ]; then
    echo "network-browser launched (PID $pids, log=$AUDIT_DIR/network-browser.log)"
elif [ "$n" -gt 1 ] 2>/dev/null; then
    echo "network-browser button.sh: WARNING - $n instances alive after launch (expected 1): $(echo $pids | tr '\n' ' ')" >&2
else
    echo "network-browser button.sh: FAILED to launch - check the log:" >&2
    cat "$AUDIT_DIR/network-browser.log" 2>/dev/null >&2
    exit 1
fi
