#!/bin/bash
# button.sh - launch csv-hq as its own detached X11 window.
# Real toy.pdl convention (khtpm_taskbar_manager.c's own
# livedesk_build_toys_menu()): invoked as `sh button.sh run`, argv[1]
# only - house_root is NOT passed, derived here the same way every
# other @.apps/ toy's own button.sh already does.
#
# Same real shape as @.apps/pdl-read/button.sh and @.apps/text-edit-hq/
# button.sh: the shared renderer renders a static csv-hq-pal.xhtpm,
# whose own <module> tag launch_module()'s the real manager
# (csv_hq_manager.+x) as its child - closing the window stops the
# manager too.
set -e
ACTION="${1:-run}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

OPS_DIR="$SCRIPT_DIR/ops"
XHTPM="$SCRIPT_DIR/csv-hq-pal.xhtpm"

RENDER_OPS_DIR="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops"
BIN="$RENDER_OPS_DIR/+x/khtpm_core_render.+x"
MANAGER_BIN="$OPS_DIR/+x/csv_hq_manager.+x"

if [ ! -x "$BIN" ]; then
    (cd "$RENDER_OPS_DIR" && sh build_core_render.sh) || true
fi
if [ ! -x "$BIN" ]; then
    echo "csv-hq button.sh: build failed, missing $BIN" >&2
    exit 1
fi
if [ ! -x "$MANAGER_BIN" ]; then
    (cd "$OPS_DIR" && sh build_csv_hq_manager.sh) || true
fi
if [ ! -x "$MANAGER_BIN" ]; then
    echo "csv-hq button.sh: build failed, missing $MANAGER_BIN" >&2
    exit 1
fi
if [ ! -f "$XHTPM" ]; then
    echo "csv-hq button.sh: missing template $XHTPM" >&2
    exit 1
fi

if [ "$ACTION" != "run" ]; then
    exit 0
fi

# Same real "set -e safe pgrep" convention as pdl-read/button.sh.
csvhq_pids() {
    pgrep -f "khtpm_core_render\.\+x.*csv-hq-pal\.xhtpm" 2>/dev/null || true
}

pids="$(csvhq_pids)"
if [ -n "$pids" ]; then
    echo "csv-hq button.sh: killing existing instance(s): $(echo $pids | tr '\n' ' ')"
    echo "$pids" | xargs -r kill -TERM
    sleep 1
    pids="$(csvhq_pids)"
    if [ -n "$pids" ]; then
        echo "$pids" | xargs -r kill -KILL
        sleep 1
    fi
fi

LOG="/tmp/csv-hq-pal.log"
setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >"$LOG" 2>&1 < /dev/null &
disown 2>/dev/null || true
sleep 1

pids="$(csvhq_pids)"
n="$(echo "$pids" | grep -c . || true)"
if [ "$n" = "1" ]; then
    echo "csv-hq launched (PID $pids, log=$LOG)"
elif [ "$n" -gt 1 ] 2>/dev/null; then
    echo "csv-hq button.sh: WARNING - $n instances alive after launch (expected 1): $(echo $pids | tr '\n' ' ')" >&2
else
    echo "csv-hq button.sh: FAILED to launch - check the log:" >&2
    cat "$LOG" 2>/dev/null >&2
    exit 1
fi
