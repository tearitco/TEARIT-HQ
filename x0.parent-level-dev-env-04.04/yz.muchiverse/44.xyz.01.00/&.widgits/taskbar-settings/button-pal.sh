#!/bin/sh
# button-pal.sh - PARALLEL launcher for the static-xhtpm taskbar-settings
# / swatch-picker window (HANDOFF-scope-nav-and-chtpm-port.md §5).
#
#   sh button-pal.sh <house_root>
#
# The old ops/button_taskbar_settings.sh + ops/taskbar_settings.chtpm +
# the g_is_swatch_picker C path stay untouched as rollback. This is NOT
# wired into any menu/launcher.
#
# Difference from button_taskbar_settings.sh:
#   - renders &.widgits/taskbar-settings/taskbar-settings-pal.xhtpm
#     (class="taskbar-settings-pal database-window", NOT "swatch-picker")
#   - the xhtpm's one <module> is taskbar_settings_projector.+x, which
#     reads #.desktop/taskbar_settings_state.txt and writes
#     #.desktop/taskbar_settings_ui.txt
#   - the generic default/popup mode does NOT fork swatch_picker_manager
#     (only the g_is_swatch_picker path did), so this script starts the
#     UNMODIFIED swatch_picker_manager.+x itself, in the background
#   - house-global single instance (the window is not multi-instance);
#     no ARG3 / instance-dir hook
set -e
HOUSE_ROOT="${1:-}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "taskbar-settings-pal: need house_root as argv[1]" >&2
    exit 1
fi
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

SETSID="setsid"
[ "$(uname)" = "Darwin" ] && SETSID=""

HERE="$(cd "$(dirname "$0")" && pwd)"
XHTPM="$HERE/taskbar-settings-pal.xhtpm"
OPS_DIR="$(ls -d "$HOUSE_ROOT"/*.monads/*.livedesk-taskbar/ops)"
BIN="$OPS_DIR/+x/khtpm_core_render.+x"
MGR="$OPS_DIR/+x/swatch_picker_manager.+x"
PROJ="$HERE/ops/+x/taskbar_settings_projector.+x"

[ -x "$BIN" ]  || (cd "$OPS_DIR" && sh build_core_render.sh) || true
[ -x "$MGR" ]  || (cd "$OPS_DIR" && sh build_core_render.sh) || true
[ -x "$PROJ" ] || (cd "$HERE/ops" && sh build_taskbar_settings_projector.sh) || true
for f in "$BIN" "$MGR" "$PROJ" "$XHTPM"; do
    [ -e "$f" ] || { echo "taskbar-settings-pal: missing $f" >&2; exit 1; }
done

# Single-instance guard: match this renderer by the xhtpm path (the
# binary is shared) and the manager by its binary name. Byte-compare
# /proc/<pid>/cmdline, no regex (house paths carry emoji/parens).
running_pids() {
    for pid in $(pgrep -f 'khtpm_core_render\.\+x' 2>/dev/null || true); do
        if [ -r "/proc/$pid/cmdline" ] && \
           tr '\0' '\n' < "/proc/$pid/cmdline" 2>/dev/null | grep -qxF "$XHTPM"; then
            echo "$pid"
        fi
    done
    pgrep -f 'swatch_picker_manager\.\+x' 2>/dev/null || true
}

pids="$(running_pids)"
if [ -n "$pids" ]; then
    echo "taskbar-settings-pal: replacing running instance(s): $(echo $pids | tr '\n' ' ')"
    echo "$pids" | xargs -r kill -TERM
    sleep 1
    pids="$(running_pids)"
    [ -n "$pids" ] && { echo "$pids" | xargs -r kill -KILL; sleep 1; }
fi

# 1. the unmodified 2-phase pick manager. It wipes
#    #.desktop/taskbar_settings_{action,state}.txt on start, polls
#    action.txt for PICK:/CLOSE, and exits after a completed 2-phase
#    pick (having exec'd apply_theme_op) or a CLOSE.
$SETSID nohup "$MGR" "$HOUSE_ROOT" \
    >/tmp/taskbar-settings-pal-mgr.log 2>&1 < /dev/null &
disown 2>/dev/null || true

# 2. the shared renderer on the static template (2-arg launch: no x/y,
#    no ARG3). Its one <module> is the projector.
$SETSID nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" \
    >/tmp/taskbar-settings-pal.log 2>&1 < /dev/null &
disown 2>/dev/null || true
sleep 1

pids="$(running_pids)"
n="$(echo "$pids" | grep -c . || true)"
if [ "$n" -ge 2 ] 2>/dev/null; then
    echo "taskbar-settings-pal launched (pids $(echo $pids | tr '\n' ' ')) log=/tmp/taskbar-settings-pal.log"
else
    echo "taskbar-settings-pal: launch incomplete ($n proc) - check /tmp/taskbar-settings-pal.log" >&2
    cat /tmp/taskbar-settings-pal.log 2>/dev/null >&2
    exit 1
fi
