#!/bin/bash
# button.sh - Match Setup WIDGIT for TSC_ELO (W1). A SEPARATE widget
# program with its own session/system/ops/pal/GL window, per WIDGIT_BIBLE.
# Modeled directly on &.widgits/board-viewer/button.sh (the house's proven
# widget launcher): session isolation, symlinked static assets, real local
# system/ binaries, widget profile = NO keyboard_input (GL owns input via
# gl_mirror -> pieces/keyboard/history.txt -> this session's own
# chtpm_parser_pal -> interact_relay.txt -> prisc+x read_history).
#
# The host (TSC_ELO button.sh) launches this with:
#   RUN_PROFILE=widget bash <this>/button.sh run-widget <host_session_root>
# Communication with the host is FILE-MEDIATED ONLY (focus.txt + the host's
# pieces/system/widget_cmds/inbox.txt + status.txt) - never subprocesses.
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

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
             "$SESSION_DIR/projects/setup/manager"

    cp -r "$SCRIPT_DIR/system" "$SESSION_DIR/system" 2>/dev/null || true
    cp -r "$SCRIPT_DIR/ops" "$SESSION_DIR/ops" 2>/dev/null || true
    cp -r "$SCRIPT_DIR/pal" "$SESSION_DIR/pal" 2>/dev/null || true
    cp -r "$SCRIPT_DIR/pieces/chtpm" "$SESSION_DIR/pieces/chtpm" 2>/dev/null || true
    cp -r "$SCRIPT_DIR/pieces/registry" "$SESSION_DIR/pieces/registry" 2>/dev/null || true
    cp -r "$SCRIPT_DIR/default_op.txt" "$SESSION_DIR/default_op.txt" 2>/dev/null || true
    mkdir -p "$SESSION_DIR/projects/setup/pieces"
    cp -r "$SCRIPT_DIR/projects/setup/pieces/setup" \
            "$SESSION_DIR/projects/setup/pieces/setup" 2>/dev/null || true

    cd "$SESSION_DIR"
    : > pieces/apps/player_app/history.txt
    : > pieces/apps/player_app/interact_relay.txt
    : > pieces/keyboard/history.txt
    : > pieces/display/setup_screen_changed.txt
    : > pieces/display/frame_changed.txt
    : > pieces/display/renderer_pulse.txt
    echo "$(cd "$SCRIPT_DIR/../../../.." && pwd)" > pieces/system/house_root.txt
    cat > pieces/system/setup_state.txt << 'EOSTATE'
mode=HvH
rating=1000
name=Player1
last_message=Set up the match, then START.
EOSTATE

    cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=setup
active_target_id=setup
EOSTATE

    export PRISC_PROJECT_ROOT="$SESSION_DIR"
    export PRISC_PROJECT_ID="setup"

    # Point this widget at the host session (inbox/status cmd-bus paths).
    if [ -n "$FOCUS_PROJECT_ROOT" ] && [ -x ./ops/+x/setup_set_focus.+x ]; then
        ./ops/+x/setup_set_focus.+x "$SESSION_DIR/pieces/system" "$FOCUS_PROJECT_ROOT" >/dev/null 2>&1 || true
    fi

    if [ -x ./ops/+x/setup_compose_frame.+x ]; then
        ./ops/+x/setup_compose_frame.+x >/dev/null 2>&1 || true
    fi

    ./system/renderer &
    RENDERER_PID=$!
    ./system/chtpm_parser_pal pieces/chtpm/layouts/setup_main.chtpm >/dev/null 2>&1 &
    CHTPM_PID=$!

    GL_PID=""
    RGB_PID=""

    if [ "$PROFILE" = "widget" ] || ([ "$PROFILE" = "app" ] && [ -n "$DISPLAY" ]); then
        # PITFALL 54: chtpm_rgb_render's initial render must happen AFTER
        # chtpm_parser_pal's first compose, or rgb_frame.raw stays black.
        waited=0
        while [ ! -s pieces/display/current_frame.txt ] && [ "$waited" -lt 20 ]; do
            sleep 0.1
            waited=$((waited + 1))
        done
        if [ -z "$NO_GL" ] && [ -x ./system/gl_mirror ]; then
            # Open this WIDGIT's own GL window BESIDE the host's (which
            # parks at the WM default): a fixed offset keeps two glut
            # windows from stacking on top of each other (live-observed
            # overlap in TSC_ELO's first launch).
            export GL_MIRROR_X="${GL_MIRROR_X:-680}"
            export GL_MIRROR_Y="${GL_MIRROR_Y:-90}"
            ./system/gl_mirror >/dev/null 2>&1 &
            GL_PID=$!
        fi
        if [ -z "$NO_GL" ] && [ -x ./system/chtpm_rgb_render ]; then
            ./system/chtpm_rgb_render >/dev/null 2>&1 &
            RGB_PID=$!
        fi
    fi

    kill_own_module() {
        local pid cwd
        for pid in $(pgrep -f "system/prisc\+x" 2>/dev/null); do
            cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null)"
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
        mkdir -p "$SCRIPT_DIR/projects/setup/pieces/setup/" 2>/dev/null || true
        cp -r "$SESSION_DIR/projects/setup/pieces/setup/." "$SCRIPT_DIR/projects/setup/pieces/setup/" 2>/dev/null || true
    }
    trap 'kill "$RENDERER_PID" "$CHTPM_PID" "$GL_PID" "$RGB_PID" 2>/dev/null; kill_own_module; persist_session_state; rm -rf "$SESSION_DIR"' EXIT INT TERM

    if [ "$PROFILE" = "widget" ]; then
        # Widget profile: NO foreground keyboard_input (GL owns input via
        # gl_mirror's history -> this session's parser relay). Wait for
        # the parser to exit.
        wait "$CHTPM_PID" 2>/dev/null
    else
        : > pieces/apps/player_app/history.txt
        ./system/keyboard_input
    fi

    kill "$RENDERER_PID" "$CHTPM_PID" "$GL_PID" "$RGB_PID" 2>/dev/null
    kill_own_module
}

case "$ACTION" in
    compile|c|build)
        bash "$SCRIPT_DIR/scripts/build.sh"
        ;;
    run-app|app|a)
        RUN_PROFILE=app run_widget_session "${2:-}"
        ;;
    run-widget|widget|w)
        RUN_PROFILE=widget run_widget_session "${2:-}"
        ;;
    run|r|start)
        run_widget_session "${2:-}"
        ;;
    focus|f)
        FOCUS="${2:-}"
        if [ -z "$FOCUS" ]; then
            echo "Usage: ./button.sh focus <host_session_root>"
            exit 1
        fi
        mkdir -p "$SCRIPT_DIR/pieces/sessions/live-focus"
        if [ -x "$SCRIPT_DIR/ops/+x/setup_set_focus.+x" ]; then
            "$SCRIPT_DIR/ops/+x/setup_set_focus.+x" "$SCRIPT_DIR/pieces/sessions/live-focus" "$FOCUS"
        fi
        ;;
    kill|k|stop)
        pkill -f "system/keyboard_input" 2>/dev/null
        pkill -f "system/renderer" 2>/dev/null
        pkill -f "system/prisc\+x" 2>/dev/null
        pkill -f "system/chtpm_parser_pal" 2>/dev/null
        pkill -f "system/chtpm_rgb_render" 2>/dev/null
        pkill -f "system/gl_mirror" 2>/dev/null
        echo "done"
        ;;
    check|verify)
        for b in system/prisc+x system/keyboard_input system/renderer \
                 system/chtpm_parser_pal ops/+x/setup_compose_frame.+x \
                 ops/+x/setup_menu_input.+x ops/+x/setup_set_focus.+x \
                 ops/+x/setup_enqueue_cmd.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b"; else echo "MISSING $b"; fi
        done
        for b in system/chtpm_rgb_render system/gl_mirror; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b (optional GL)"; else echo "OPTIONAL-MISS $b"; fi
        done
        ;;
    help|h|-h|--help)
        echo "Match Setup WIDGIT (TSC_ELO W1) — separate GL widget program"
        echo ""
        echo "Usage: ./button.sh <action> [host_session_root]"
        echo "  run-widget | widget | w  - Force GL window mode (used by host)"
        echo "  run-app | app | a        - Force ASCII terminal mode"
        echo "  focus <session_root>     - Re-point at a host session"
        echo "  compile                  - Build ops + copy system binaries"
        echo "  kill                     - Kill processes"
        echo "  check                    - Verify binaries"
        ;;
    *)
        echo "Unknown: $ACTION"; exit 1 ;;
esac
