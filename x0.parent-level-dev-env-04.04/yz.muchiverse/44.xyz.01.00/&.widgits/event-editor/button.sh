#!/bin/sh
# event-editor widget — file-menu shape (POSIX sh: works with sh button.sh r)
#
# DEFAULT = widget: GL primary, no second TTY
#   CHTPM → current_frame.txt → chtpm_rgb_render → gl_mirror
# OPTIONAL ASCII: SHOW_ASCII=1 or: sh button.sh r --ascii
#
# NEVER freeglut custom menus.
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
    # $1=pid $2=name — append to session proc list
    echo "$1 $2" >> pieces/os/proc_list.txt
}

_kill_session_procs() {
    # kill pids recorded in pieces/os/proc_list.txt if present
    if [ -f pieces/os/proc_list.txt ]; then
        while read -r pid name; do
            [ -n "$pid" ] || continue
            kill "$pid" 2>/dev/null || true
        done < pieces/os/proc_list.txt
    fi
    # session-scoped prisc only
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
    if [ ! -x "$SCRIPT_DIR/ops/+x/ee_compose_frame.+x" ]; then
        mkdir -p "$SCRIPT_DIR/ops/+x"
        gcc -Wall -O2 -o "$SCRIPT_DIR/ops/+x/ee_compose_frame.+x" "$SCRIPT_DIR/ops/ee_compose_frame.c" || exit 1
        gcc -Wall -O2 -o "$SCRIPT_DIR/ops/+x/ee_menu_input.+x" "$SCRIPT_DIR/ops/ee_menu_input.c" || exit 1
    fi

    SESSION_ID="$(date +%s)-$$"
    SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
    export SESSION_DIR

    mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
             "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
             "$SESSION_DIR/projects/event-editor/manager" "$SESSION_DIR/pieces/os"

    cp -r "$MUTA/system" "$SESSION_DIR/system"
    cp -r "$SCRIPT_DIR/ops" "$SESSION_DIR/ops"
    cp -r "$SCRIPT_DIR/pal" "$SESSION_DIR/pal"
    cp -r "$SCRIPT_DIR/default_op.txt" "$SESSION_DIR/default_op.txt"
    cp -r "$SCRIPT_DIR/pieces/chtpm" "$SESSION_DIR/pieces/chtpm"
    # fonts for GL text (chtpm_rgb_render)
    if [ -d "$MUTA/pieces/registry" ]; then
        cp -r "$MUTA/pieces/registry" "$SESSION_DIR/pieces/registry"
    else
        echo "WARN: no $MUTA/pieces/registry — GL text will be blank"
    fi

    cd "$SESSION_DIR" || exit 1

    : > pieces/display/frame_changed.txt
    : > pieces/display/renderer_pulse.txt
    : > pieces/display/ee_screen_changed.txt
    : > pieces/apps/player_app/history.txt
    : > pieces/keyboard/history.txt
    : > pieces/apps/player_app/interact_relay.txt
    : > pieces/os/proc_list.txt
    : > projects/event-editor/manager/gui_state.txt

    cat > pieces/system/ee_state.txt << EOF
view_mode=commands
pkg_name=${EE_PKG_NAME:-door_guard}
pkg_dir=${EE_PKG_DIR:-}
page=1
last_message=widget profile: GL primary; SHOW_ASCII=$SHOW_ASCII
EOF
    cat > pieces/apps/player_app/state.txt << EOF
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=event-editor
active_target_id=event_editor
launch_profile=$profile
ascii_renderer=$SHOW_ASCII
gl_window=1
EOF

    export PRISC_PROJECT_ROOT="$SESSION_DIR"
    export PRISC_PROJECT_ID="event-editor"

    ./ops/+x/ee_compose_frame.+x >/dev/null 2>&1 || true

    WAIT_PID=""

    # Optional ASCII secondary
    if [ "$SHOW_ASCII" = "1" ]; then
        ./system/renderer >/dev/null 2>&1 &
        _log_pid $! renderer
        echo "ASCII secondary: terminal renderer ON"
    else
        echo "ASCII secondary: OFF (frame still in current_frame.txt)"
    fi

    # CHTPM → current_frame.txt
    ./system/chtpm_parser_pal pieces/chtpm/layouts/event_editor.chtpm >/dev/null 2>&1 &
    _log_pid $! chtpm_parser_pal

    # REAL FIX 2026-08-04, direct instruction ("requires enter press
    # before it renders text, otherwise blank, lets fix that bug no
    # matter wut"): the exact "THREE-LAYER RACE" already documented and
    # fixed in file-menu/tile-picker/board-viewer's own button.sh was
    # simply MISSING here - chtpm_rgb_render started immediately, before
    # chtpm_parser_pal's own first real parse_chtm()+compose_frame() had
    # produced a real current_frame.txt, so its own baseline pulse-size
    # read latched onto an empty/incomplete frame - it then never
    # re-rendered until some LATER pulse (e.g. a keypress) grew the
    # marker past that wrong baseline, which is exactly "blank until
    # Enter." Same real wait-loop fix as those other three projects.
    waited=0
    while [ ! -s pieces/display/current_frame.txt ] && [ "$waited" -lt 20 ]; do
        sleep 0.1
        waited=$((waited + 1))
    done

    # RGB from ASCII frame
    if [ -x ./system/chtpm_rgb_render ]; then
        ./system/chtpm_rgb_render >/dev/null 2>&1 &
        _log_pid $! chtpm_rgb_render
    fi

    # GL primary
    if [ "$NO_GL" != "1" ] && [ -n "${DISPLAY:-}" ] && [ -x ./system/gl_mirror ]; then
        ./system/gl_mirror >/dev/null 2>&1 &
        WAIT_PID=$!
        _log_pid $WAIT_PID gl_mirror
        echo "GL primary: gl_mirror on DISPLAY=$DISPLAY"
    else
        echo "WARN: no GL (NO_GL=$NO_GL DISPLAY=${DISPLAY:-empty})"
    fi

    trap '_kill_session_procs; rm -rf "$SESSION_DIR"' EXIT INT TERM

    echo "Event Editor — profile=$profile  (like file-menu)"
    echo "  session: $SESSION_DIR"
    echo "  truth:   pieces/display/current_frame.txt"
    echo "  UI:      GL mirrors rgb of that frame"
    echo "  KEY:5 Commands|Scratch | multi-digit | Ctrl+C quit"

    if [ "$SHOW_ASCII" = "1" ] && [ -x ./system/keyboard_input ]; then
        ./system/keyboard_input
    elif [ -n "$WAIT_PID" ]; then
        wait "$WAIT_PID" 2>/dev/null || wait
    else
        # no GL and no keyboard: stay until signal
        while true; do sleep 3600; done
    fi
}

case "$ACTION" in
    kill|k|stop|emergency)
        for b in gl_mirror chtpm_rgb_render chtpm_parser_pal prisc+x ee_gl_mock renderer keyboard_input; do
            pkill -x "$b" 2>/dev/null || true
        done
        rm -rf "$SCRIPT_DIR/pieces/sessions"/* 2>/dev/null || true
        echo "event-editor: killed exact bins + wiped sessions"
        ;;

    compile|c|build)
        mkdir -p "$SCRIPT_DIR/ops/+x"
        for op in ee_compose_frame ee_menu_input \
                  ee_resolve_desktop ee_open_request_write ee_open_request_read \
                  ee_package_init ee_import_to_world ee_export_entity; do
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
        for b in ops/+x/ee_compose_frame.+x ops/+x/ee_menu_input.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK $b"; else echo "MISSING $b"; fi
        done
        for b in chtpm_parser_pal chtpm_rgb_render gl_mirror renderer keyboard_input prisc+x; do
            if [ -x "$MUTA/system/$b" ]; then echo "OK system/$b"; else echo "MISSING system/$b"; fi
        done
        ;;

    help|h|-h|--help|*)
        cat <<EOF
event-editor widget  =  file-menu shape for mutaclysm events

  sh button.sh r              # same as run-widget (headless GL)
  sh button.sh run
  sh button.sh run-widget

  OPTIONAL ASCII secondary:
  sh button.sh r --ascii
  SHOW_ASCII=1 sh button.sh r

  sh button.sh compile | check | kill

Pipeline: chtpm -> current_frame.txt -> rgb -> gl_mirror
Fonts: session links muta pieces/registry (required for GL text)
EOF
        ;;
esac
