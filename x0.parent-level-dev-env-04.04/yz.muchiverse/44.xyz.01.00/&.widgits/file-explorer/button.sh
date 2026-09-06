#!/bin/bash
# button.sh - launch the File Explorer widget as its own detached X11
# window, standalone (browsing house_root, LOAD mode) for direct
# testing/checking - not the normal way apps embed it (they use the
# real <module> tag directly), but real toy.pdl convention needs a
# standalone entry point same as every other checkable toy.
#
# REAL FIX 2026-09-05, direct live request ("add them to tb sub menus
# so i can check them") - was argv[1]=house_root (only usable when
# launched by hand); real toy.pdl convention (khtpm_taskbar_manager.c's
# own livedesk_build_toys_menu()) invokes `sh button.sh run`, argv[1]
# only, no house_root passed - derived here the same way pdl-read's
# own button.sh already does (two levels up, since this file lives at
# house_root/&.widgits/file-explorer/).
#
# 11.brainstorm/2026-09-05/PDL-READER-AND-FILE-EXPLORER-WIDGET.md.
# Same real shape as @.apps/pdl-read/button.sh: the shared renderer
# (khtpm_core_render.+x) renders a static file-explorer-pal.xhtpm,
# whose own <module> tag launch_module()'s the real manager
# (file_explorer_manager.+x) as its child - closing the window stops
# the manager too, no separate PID for this script to track.
set -e
ACTION="${1:-run}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

OPS_DIR="$SCRIPT_DIR/ops"
XHTPM="$SCRIPT_DIR/file-explorer-pal.xhtpm"

RENDER_OPS_DIR="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops"
BIN="$RENDER_OPS_DIR/+x/khtpm_core_render.+x"
MANAGER_BIN="$OPS_DIR/+x/file_explorer_manager.+x"

if [ ! -x "$BIN" ]; then
    (cd "$RENDER_OPS_DIR" && sh build_core_render.sh) || true
fi
if [ ! -x "$BIN" ]; then
    echo "file-explorer button.sh: build failed, missing $BIN" >&2
    exit 1
fi
if [ ! -x "$MANAGER_BIN" ]; then
    (cd "$OPS_DIR" && sh build_file_explorer_manager.sh) || true
fi
if [ ! -x "$MANAGER_BIN" ]; then
    echo "file-explorer button.sh: build failed, missing $MANAGER_BIN" >&2
    exit 1
fi
if [ ! -f "$XHTPM" ]; then
    echo "file-explorer button.sh: missing template $XHTPM" >&2
    exit 1
fi

if [ "$ACTION" != "run" ]; then
    exit 0
fi

# Same real "set -e safe pgrep" convention as pdl-read/button.sh.
fe_pids() {
    pgrep -f "khtpm_core_render\.\+x.*file-explorer-pal\.xhtpm" 2>/dev/null || true
}

pids="$(fe_pids)"
if [ -n "$pids" ]; then
    echo "file-explorer button.sh: killing existing instance(s): $(echo $pids | tr '\n' ' ')"
    echo "$pids" | xargs -r kill -TERM
    sleep 1
    pids="$(fe_pids)"
    if [ -n "$pids" ]; then
        echo "file-explorer button.sh: still alive after TERM, escalating to KILL: $(echo $pids | tr '\n' ' ')"
        echo "$pids" | xargs -r kill -KILL
        sleep 1
    fi
fi

LOG="/tmp/file-explorer-pal.log"
setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >"$LOG" 2>&1 < /dev/null &
disown 2>/dev/null || true
sleep 1

pids="$(fe_pids)"
n="$(echo "$pids" | grep -c . || true)"
if [ "$n" = "1" ]; then
    echo "file-explorer launched (PID $pids, log=$LOG)"
elif [ "$n" -gt 1 ] 2>/dev/null; then
    echo "file-explorer button.sh: WARNING - $n instances alive after launch (expected 1): $(echo $pids | tr '\n' ' ')" >&2
else
    echo "file-explorer button.sh: FAILED to launch - check the log:" >&2
    cat "$LOG" 2>/dev/null >&2
    exit 1
fi
