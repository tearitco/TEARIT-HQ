#!/bin/bash
# button.sh - yahoo-broker widget (shared broker viewer for yahoo-app)
# Modeled directly on &.widgits/board-viewer/button.sh (proven widget launcher).
# Two launch profiles: app=ASCII, widget=GL.
# Spawned on demand from yahoo-app's bank screen via OPEN_BROKER_WIDGET.
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

run_widget_session() {
    local FOCUS_PROJECT_ROOT="${1:-}"
    local PROFILE="${RUN_PROFILE:-}"
    if [ -z "$PROFILE" ]; then
        if [ -z "$DISPLAY" ]; then
            PROFILE="app"
        else
            PROFILE="widget"
        fi
    fi

    SESSION_ID="$(date +%s)-$$"
    SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
    mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
             "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
             "$SESSION_DIR/projects/yahoo-broker/manager"

    cp -r "$SCRIPT_DIR/system" "$SESSION_DIR/system" 2>/dev/null || true
    cp -r "$SCRIPT_DIR/ops" "$SESSION_DIR/ops" 2>/dev/null || true
    cp -r "$SCRIPT_DIR/pal" "$SESSION_DIR/pal" 2>/dev/null || true
    cp -r "$SCRIPT_DIR/pieces/chtpm" "$SESSION_DIR/pieces/chtpm" 2>/dev/null || true
    cp -r "$SCRIPT_DIR/pieces/registry" "$SESSION_DIR/pieces/registry" 2>/dev/null || true
    cp -r "$SCRIPT_DIR/default_op.txt" "$SESSION_DIR/default_op.txt" 2>/dev/null || true
    mkdir -p "$SESSION_DIR/projects/yahoo-broker/pieces"
    cp -r "$SCRIPT_DIR/projects/yahoo-broker/pieces/yahoo_broker" \
            "$SESSION_DIR/projects/yahoo-broker/pieces/yahoo_broker" 2>/dev/null || true
    cp -r "$SCRIPT_DIR/projects/yahoo-broker/pieces/broker_widget" \
            "$SESSION_DIR/projects/yahoo-broker/pieces/broker_widget" 2>/dev/null || true
    cp -r "$SCRIPT_DIR/data" "$SESSION_DIR/data" 2>/dev/null || true

    cd "$SESSION_DIR"
    : > pieces/apps/player_app/interact_relay.txt
    : > pieces/keyboard/history.txt
    : > pieces/display/broker_screen_changed.txt
    : > pieces/display/frame_changed.txt
    : > pieces/display/renderer_pulse.txt
    echo "$HOUSE_DIR" > pieces/system/house_root.txt

    if [ -n "$FOCUS_PROJECT_ROOT" ]; then
        cat > pieces/system/broker_state.txt << EOF
focused_project_id=yahoo-app
focused_project_root=$FOCUS_PROJECT_ROOT
selected_broker=
broker_balance=0.00
EOF
    fi

    export PRISC_PROJECT_ROOT="$SESSION_DIR"
    export PRISC_PROJECT_ID="yahoo-broker"
    export PAL_LAYOUT="pieces/chtpm/layouts/broker_widget.chtpm"
    "$SCRIPT_DIR/system/orchestrator" 2>>pieces/system/orchestrator.log &
    ORCH_PID=$!

    GL_PID=""
    RGB_PID=""
    if [ -z "$NO_GL" ] && [ -n "$DISPLAY" ]; then
        waited=0
        while [ ! -s pieces/display/current_frame.txt ] && [ "$waited" -lt 20 ]; do
            sleep 0.1
            waited=$((waited + 1))
        done
        if [ -x ./system/gl_mirror ]; then
            ./system/gl_mirror >/dev/null 2>&1 &
            GL_PID=$!
        fi
        if [ -x ./system/chtpm_rgb_render ]; then
            ./system/chtpm_rgb_render >/dev/null 2>&1 &
            RGB_PID=$!
        fi
    fi

    kill_own_module() {
        local pid cwd
        for pid in $(pgrep -f "system/prisc\+x" 2>/dev/null); do
            cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null)"
            cwd="${cwd% (deleted)}"
            if [ "$cwd" = "$SESSION_DIR" ]; then
                kill -9 "$pid" 2>/dev/null
            fi
        done
    }

    # Step 2 symlink-migration fix: copy mutable session state back
    # to the real project root before the session dir is deleted
    # (the old symlinks made these writes land at the real root for
    # free; cp -r sessions need this explicit copy-back). Merge
    # semantics - adds/overwrites, never deletes. Volatile files
    # (quit_flag, pids, history, relays, gui_state) are NOT copied.
    persist_session_state() {
        mkdir -p "$SCRIPT_DIR/projects/yahoo-broker/pieces/yahoo_broker/" 2>/dev/null || true
        cp -r "$SESSION_DIR/projects/yahoo-broker/pieces/yahoo_broker/." "$SCRIPT_DIR/projects/yahoo-broker/pieces/yahoo_broker/" 2>/dev/null || true
        mkdir -p "$SCRIPT_DIR/projects/yahoo-broker/pieces/broker_widget/" 2>/dev/null || true
        cp -r "$SESSION_DIR/projects/yahoo-broker/pieces/broker_widget/." "$SCRIPT_DIR/projects/yahoo-broker/pieces/broker_widget/" 2>/dev/null || true
        mkdir -p "$SCRIPT_DIR/data/" 2>/dev/null || true
        cp -r "$SESSION_DIR/data/." "$SCRIPT_DIR/data/" 2>/dev/null || true
    }
    trap 'kill "$ORCH_PID" "$GL_PID" "$RGB_PID" 2>/dev/null; wait "$ORCH_PID" 2>/dev/null; kill_own_module; persist_session_state; rm -rf "$SESSION_DIR"' EXIT INT TERM

    ./system/keyboard_input
}

case "$ACTION" in
    run-widget)
        run_widget_session "$2"
        ;;
    help|h|-h|--help)
        echo "yahoo-broker widget"
        echo ""
        echo "Usage: ./button.sh <action>"
        echo "  run-widget <project_root>  - Launch broker widget for a project"
        echo "  help                       - This help"
        ;;
    *)
        echo "Unknown: $ACTION (try ./button.sh help)"
        exit 1
        ;;
esac
