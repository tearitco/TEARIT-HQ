#!/bin/sh
# context-menu widget — real CHTPM context menu, 2026-08-05.
# Direct user proposal + confirmation: "we could chtpm parser for the
# context menu, what do u think?" - replaces tp_desktop_window.c's own
# raw X11 popup with a real, uniform-to-edit CHTPM session. Mirrors
# event-ez/button.sh's own proven session-launch structure exactly.
#
# ENV:
#   CM_PKG_NAME=<name>   shown in the header
#   CM_PKG_DIR=<path>    real entity dir whose meta.pdl methods get read
#
ACTION="${1:-help}"
[ "$#" -gt 0 ] && shift

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
HOUSE=$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)
MUTA=$(ls -d "$HOUSE"/101.mutaclsym* 2>/dev/null | head -1)

_log_pid() {
    echo "$1 $2" >> pieces/os/proc_list.txt
}

_kill_session_procs() {
    if [ -f pieces/os/proc_list.txt ]; then
        while read -r pid name; do
            [ -n "$pid" ] || continue
            kill "$pid" 2>/dev/null || true
        done < pieces/os/proc_list.txt
    fi
    for pid in $(pgrep -x 'prisc+x' 2>/dev/null); do
        c=$(readlink -f "/proc/$pid/cwd" 2>/dev/null || true)
        if [ "$c" = "$SESSION_DIR" ]; then
            kill -9 "$pid" 2>/dev/null || true
        fi
    done
}

_start_session() {
    if [ -z "$MUTA" ] || [ ! -x "$MUTA/system/chtpm_parser_pal" ]; then
        echo "Need mutaclysm system (chtpm_parser_pal). MUTA=$MUTA"
        exit 1
    fi
    if [ ! -x "$SCRIPT_DIR/ops/+x/cm_compose_frame.+x" ]; then
        mkdir -p "$SCRIPT_DIR/ops/+x"
        gcc -Wall -O2 -o "$SCRIPT_DIR/ops/+x/cm_compose_frame.+x" "$SCRIPT_DIR/ops/cm_compose_frame.c" || exit 1
        gcc -Wall -O2 -o "$SCRIPT_DIR/ops/+x/cm_menu_input.+x" "$SCRIPT_DIR/ops/cm_menu_input.c" || exit 1
    fi

    SESSION_ID="$(date +%s)-$$"
    SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
    export SESSION_DIR

    mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
             "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
             "$SESSION_DIR/projects/context-menu/manager" "$SESSION_DIR/pieces/os"

    cp -r "$MUTA/system" "$SESSION_DIR/system"
    cp -r "$SCRIPT_DIR/ops" "$SESSION_DIR/ops"
    cp -r "$SCRIPT_DIR/pal" "$SESSION_DIR/pal"
    cp -r "$SCRIPT_DIR/default_op.txt" "$SESSION_DIR/default_op.txt"
    cp -r "$SCRIPT_DIR/pieces/chtpm" "$SESSION_DIR/pieces/chtpm"
    if [ -d "$MUTA/pieces/registry" ]; then
        cp -r "$MUTA/pieces/registry" "$SESSION_DIR/pieces/registry"
    fi

    cd "$SESSION_DIR" || exit 1

    : > pieces/display/frame_changed.txt
    : > pieces/display/renderer_pulse.txt
    : > pieces/display/cm_screen_changed.txt
    : > pieces/apps/player_app/history.txt
    : > pieces/keyboard/history.txt
    : > pieces/apps/player_app/interact_relay.txt
    : > pieces/os/proc_list.txt
    : > projects/context-menu/manager/gui_state.txt

    cat > pieces/system/cm_state.txt << EOF
pkg_name=${CM_PKG_NAME:-(none)}
pkg_dir=${CM_PKG_DIR:-}
last_message=
EOF
    cat > pieces/apps/player_app/state.txt << EOF
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=context-menu
active_target_id=context_menu
gl_window=1
EOF

    export PRISC_PROJECT_ROOT="$SESSION_DIR"
    export PRISC_PROJECT_ID="context-menu"

    ./ops/+x/cm_compose_frame.+x >/dev/null 2>&1 || true

    START_LAYOUT="${CM_START_LAYOUT:-cm_main.chtpm}"
    ./system/chtpm_parser_pal "pieces/chtpm/layouts/$START_LAYOUT" >/dev/null 2>&1 &
    _log_pid $! chtpm_parser_pal

    waited=0
    while [ ! -s pieces/display/current_frame.txt ] && [ "$waited" -lt 20 ]; do
        sleep 0.1
        waited=$((waited + 1))
    done

    # REAL, 2026-08-05, direct instruction ("do all 3 of the momentum
    # givers now" + "pairing with small-frame... before then?"): real
    # small-popup mode - CM_SMALL=1 uses the real small-frame render
    # variant (chtpm_rgb_render_small, -DFRAME_W=320 -DFRAME_H=176,
    # built alongside the default 640x768 binary without touching it -
    # see chtpm_rgb_render.c's own #ifndef FRAME_W comment) together
    # with gl_mirror's own real GL_MIRROR_BORDERLESS mode - the two
    # pieces PAIRED, not tested separately (separately looked "horrible"
    # per direct user report: full-size + borderless with no size
    # change).
    RENDER_BIN="./system/chtpm_rgb_render"
    if [ "${CM_SMALL:-0}" = "1" ] && [ -x ./system/chtpm_rgb_render_small ]; then
        RENDER_BIN="./system/chtpm_rgb_render_small"
    fi
    if [ -x "$RENDER_BIN" ]; then
        "$RENDER_BIN" >/dev/null 2>&1 &
        _log_pid $! chtpm_rgb_render
    fi

    WAIT_PID=""
    if [ -n "${DISPLAY:-}" ] && [ -x ./system/gl_mirror ]; then
        if [ "${CM_SMALL:-0}" = "1" ]; then
            GL_MIRROR_BORDERLESS=1 ./system/gl_mirror >/dev/null 2>&1 &
        else
            ./system/gl_mirror >/dev/null 2>&1 &
        fi
        WAIT_PID=$!
        _log_pid $WAIT_PID gl_mirror
    fi

    trap '_kill_session_procs; rm -rf "$SESSION_DIR"' EXIT INT TERM

    echo "context-menu — session: $SESSION_DIR"
    if [ -n "$WAIT_PID" ]; then
        wait "$WAIT_PID" 2>/dev/null || wait
    else
        while true; do sleep 3600; done
    fi
}

case "$ACTION" in
    kill|k|stop|emergency)
        for b in gl_mirror chtpm_rgb_render chtpm_parser_pal prisc+x; do
            pkill -x "$b" 2>/dev/null || true
        done
        rm -rf "$SCRIPT_DIR/pieces/sessions"/* 2>/dev/null || true
        echo "context-menu: killed exact bins + wiped sessions"
        ;;

    compile|c|build)
        mkdir -p "$SCRIPT_DIR/ops/+x"
        for op in cm_compose_frame cm_menu_input; do
            [ -f "$SCRIPT_DIR/ops/$op.c" ] || continue
            gcc -Wall -O2 -o "$SCRIPT_DIR/ops/+x/$op.+x" "$SCRIPT_DIR/ops/$op.c" \
                && echo "OK $op" || echo "FAIL $op"
        done
        ;;

    run|r|start)
        _start_session
        ;;

    help|h|-h|--help|*)
        cat <<EOF
context-menu widget — real CHTPM context menu

  CM_PKG_NAME=dog CM_PKG_DIR=<entity dir> sh button.sh run
  sh button.sh compile | kill
EOF
        ;;
esac
