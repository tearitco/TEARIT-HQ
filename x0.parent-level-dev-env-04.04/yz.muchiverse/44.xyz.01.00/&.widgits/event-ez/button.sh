#!/bin/sh
# event-ez widget — 4th event-editor variant, file-menu shape.
# Direct user goal (2026-08-05): "click nav buttons and fill in cli-io
# blanks" - real cli_io text fields ARE the primary authoring UI here,
# not an ASCII-form afterthought. Mirrors event-editor/button.sh's own
# proven real-pipeline structure (session isolation, three-layer-race
# wait loop, CHTPM -> current_frame.txt -> rgb -> gl_mirror).
#
# ENV:
#   EZ_PKG_NAME=<name>   shown in the chrome (e.g. "dog")
#   EZ_PKG_DIR=<path>    real event_pkg dir Save (KEY:5) writes into
#
ACTION="${1:-help}"
[ "$#" -gt 0 ] && shift

SHOW_ASCII="${SHOW_ASCII:-0}"
NO_GL="${NO_GL:-0}"
for a in "$@"; do
    case "$a" in
        --ascii|ascii) SHOW_ASCII=1 ;;
        --no-ascii) SHOW_ASCII=0 ;;
        --no-gl) NO_GL=1 ;;
    esac
done

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
    profile="$1"

    if [ -z "$MUTA" ] || [ ! -x "$MUTA/system/chtpm_parser_pal" ]; then
        echo "Need mutaclysm system (chtpm_parser_pal). MUTA=$MUTA"
        exit 1
    fi
    if [ ! -x "$SCRIPT_DIR/ops/+x/ez_compose_frame.+x" ]; then
        mkdir -p "$SCRIPT_DIR/ops/+x"
        gcc -Wall -O2 -o "$SCRIPT_DIR/ops/+x/ez_compose_frame.+x" "$SCRIPT_DIR/ops/ez_compose_frame.c" || exit 1
        gcc -Wall -O2 -o "$SCRIPT_DIR/ops/+x/ez_menu_input.+x" "$SCRIPT_DIR/ops/ez_menu_input.c" || exit 1
    fi

    SESSION_ID="$(date +%s)-$$"
    SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
    export SESSION_DIR

    mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
             "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
             "$SESSION_DIR/projects/event-ez/manager" "$SESSION_DIR/pieces/os" \
             "$SESSION_DIR/pieces/debug/frames"

    cp -r "$MUTA/system" "$SESSION_DIR/system"
    cp -r "$SCRIPT_DIR/ops" "$SESSION_DIR/ops"
    cp -r "$SCRIPT_DIR/pal" "$SESSION_DIR/pal"
    cp -r "$SCRIPT_DIR/default_op.txt" "$SESSION_DIR/default_op.txt"
    cp -r "$SCRIPT_DIR/pieces/chtpm" "$SESSION_DIR/pieces/chtpm"
    if [ -d "$MUTA/pieces/registry" ]; then
        cp -r "$MUTA/pieces/registry" "$SESSION_DIR/pieces/registry"
    else
        echo "WARN: no $MUTA/pieces/registry — GL text will be blank"
    fi

    cd "$SESSION_DIR" || exit 1

    : > pieces/display/frame_changed.txt
    : > pieces/display/renderer_pulse.txt
    : > pieces/display/ez_screen_changed.txt
    : > pieces/apps/player_app/history.txt
    : > pieces/keyboard/history.txt
    : > pieces/apps/player_app/interact_relay.txt
    : > pieces/os/proc_list.txt
    : > projects/event-ez/manager/gui_state.txt

    cat > pieces/system/ez_state.txt << EOF
pkg_name=${EZ_PKG_NAME:-(none)}
pkg_dir=${EZ_PKG_DIR:-}
behavior=
last_message=widget profile: GL primary; SHOW_ASCII=$SHOW_ASCII
EOF
    cat > pieces/apps/player_app/state.txt << EOF
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=event-ez
active_target_id=event_ez
launch_profile=$profile
ascii_renderer=$SHOW_ASCII
gl_window=1
EOF

    export PRISC_PROJECT_ROOT="$SESSION_DIR"
    export PRISC_PROJECT_ID="event-ez"

    ./ops/+x/ez_compose_frame.+x >/dev/null 2>&1 || true

    WAIT_PID=""

    if [ "$SHOW_ASCII" = "1" ]; then
        ./system/renderer >/dev/null 2>&1 &
        _log_pid $! renderer
        echo "ASCII secondary: terminal renderer ON"
    else
        echo "ASCII secondary: OFF (frame still in current_frame.txt)"
    fi

    ./system/chtpm_parser_pal pieces/chtpm/layouts/event_ez.chtpm >/dev/null 2>&1 &
    _log_pid $! chtpm_parser_pal

    # Same "three-layer race" wait, ported directly from event-editor's
    # own real fix - chtpm_rgb_render must not start until
    # chtpm_parser_pal's own first real compose has produced a real
    # current_frame.txt, or its baseline pulse-size read latches onto an
    # empty frame and never re-renders until some later pulse.
    waited=0
    while [ ! -s pieces/display/current_frame.txt ] && [ "$waited" -lt 20 ]; do
        sleep 0.1
        waited=$((waited + 1))
    done

    if [ -x ./system/chtpm_rgb_render ]; then
        ./system/chtpm_rgb_render >/dev/null 2>&1 &
        _log_pid $! chtpm_rgb_render
    fi

    if [ "$NO_GL" != "1" ] && [ -n "${DISPLAY:-}" ] && [ -x ./system/gl_mirror ]; then
        ./system/gl_mirror >/dev/null 2>&1 &
        WAIT_PID=$!
        _log_pid $WAIT_PID gl_mirror
        echo "GL primary: gl_mirror on DISPLAY=$DISPLAY"
    else
        echo "WARN: no GL (NO_GL=$NO_GL DISPLAY=${DISPLAY:-empty})"
    fi

    trap '_kill_session_procs; rm -rf "$SESSION_DIR"' EXIT INT TERM

    echo "Event-EZ — profile=$profile"
    echo "  session: $SESSION_DIR"
    echo "  truth:   pieces/display/current_frame.txt"
    echo "  1-4 behavior | cli_io blanks | 5=Save | Ctrl+C quit"

    if [ "$SHOW_ASCII" = "1" ] && [ -x ./system/keyboard_input ]; then
        ./system/keyboard_input
    elif [ -n "$WAIT_PID" ]; then
        wait "$WAIT_PID" 2>/dev/null || wait
    else
        while true; do sleep 3600; done
    fi
}

case "$ACTION" in
    kill|k|stop|emergency)
        for b in gl_mirror chtpm_rgb_render chtpm_parser_pal prisc+x renderer keyboard_input; do
            pkill -x "$b" 2>/dev/null || true
        done
        rm -rf "$SCRIPT_DIR/pieces/sessions"/* 2>/dev/null || true
        echo "event-ez: killed exact bins + wiped sessions"
        ;;

    compile|c|build)
        mkdir -p "$SCRIPT_DIR/ops/+x"
        for op in ez_compose_frame ez_menu_input; do
            [ -f "$SCRIPT_DIR/ops/$op.c" ] || continue
            gcc -Wall -O2 -o "$SCRIPT_DIR/ops/+x/$op.+x" "$SCRIPT_DIR/ops/$op.c" \
                && echo "OK $op" || echo "FAIL $op"
        done
        if [ -x "$MUTA/system/chtpm_parser_pal" ]; then
            echo "OK system <- muta"
        else
            echo "MISSING muta system"
        fi
        ;;

    run-widget|widget|run|r|start)
        _start_session widget
        ;;

    run-ascii|ascii)
        SHOW_ASCII=1
        _start_session widget
        ;;

    check)
        for b in ops/+x/ez_compose_frame.+x ops/+x/ez_menu_input.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK $b"; else echo "MISSING $b"; fi
        done
        ;;

    help|h|-h|--help|*)
        cat <<EOF
event-ez widget — 4th event-editor variant ("click nav, fill blanks")

  sh button.sh r              # headless GL
  sh button.sh compile | check | kill

  EZ_PKG_NAME=dog EZ_PKG_DIR=<path>/event_pkg sh button.sh r
EOF
        ;;
esac
