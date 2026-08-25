#!/bin/bash
# button.sh - tile-picker widget (real CHTPM session, house standard),
# modeled directly on &.widgits/file-menu/button.sh's own
# run_widget_session() (read in full 2026-08-04 before this rebuild -
# see this house's own feedback_chtpm_read_precedent_first.md memory).
#
# Replaces an earlier, WRONG version of this widget: a bespoke raw
# X11/GLX window with its own hand-rolled keyboard handling, confirmed
# broken (user could not interact with it at all) because it never wrote
# into history.txt - chtpm_parser_pal.c, the real owner of ALL nav/focus/
# digit-jump/Enter-to-activate logic in this house, never saw a
# keystroke from it. This version uses the same real pipeline every
# other project uses: keyboard_input.c/gl_mirror.c -> history.txt ->
# chtpm_parser_pal.c -> interact_relay.txt -> tp_menu_input.
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

run_widget_session() {
    local PROFILE="${RUN_PROFILE:-}"
    if [ -z "$PROFILE" ]; then
        if [ -z "$DISPLAY" ]; then PROFILE="app"; else PROFILE="widget"; fi
    fi
    SESSION_ID="$(date +%s)-$$"
    SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
    mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
             "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard"

    cp -r "$SCRIPT_DIR/system" "$SESSION_DIR/system" 2>/dev/null || true
    cp -r "$SCRIPT_DIR/ops" "$SESSION_DIR/ops" 2>/dev/null || true
    cp -r "$SCRIPT_DIR/pal" "$SESSION_DIR/pal" 2>/dev/null || true
    cp -r "$SCRIPT_DIR/pieces/chtpm" "$SESSION_DIR/pieces/chtpm" 2>/dev/null || true
    cp -r "$SCRIPT_DIR/pieces/registry" "$SESSION_DIR/pieces/registry" 2>/dev/null || true
    cp -r "$SCRIPT_DIR/pieces/system/picker_items.txt" "$SESSION_DIR/pieces/system/picker_items.txt" 2>/dev/null || true
    cp -r "$SCRIPT_DIR/default_op.txt" "$SESSION_DIR/default_op.txt" 2>/dev/null || true

    cd "$SESSION_DIR"
    : > pieces/apps/player_app/history.txt
    : > pieces/apps/player_app/interact_relay.txt
    : > pieces/keyboard/history.txt
    : > pieces/display/tp_screen_changed.txt
    : > pieces/display/frame_changed.txt
    : > pieces/system/tp_state.txt
    echo "$(cd "$SCRIPT_DIR/../.." && pwd)" > pieces/system/house_root.txt
    cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=tile-picker
active_target_id=tile-picker
EOSTATE

    export PRISC_PROJECT_ROOT="$SESSION_DIR"
    export PRISC_PROJECT_ID="tile-picker"

    if [ -x ./ops/+x/tp_compose_frame.+x ]; then
        ./ops/+x/tp_compose_frame.+x >/dev/null 2>&1 || true
    fi

    ./system/renderer &
    RENDERER_PID=$!
    ./system/chtpm_parser_pal pieces/chtpm/layouts/tile_picker_main.chtpm >/dev/null 2>&1 &
    CHTPM_PID=$!

    GL_PID=""
    RGB_PID=""
    if [ "$PROFILE" = "widget" ] || ([ "$PROFILE" = "app" ] && [ -n "$DISPLAY" ]); then
        waited=0
        while [ ! -s pieces/display/current_frame.txt ] && [ "$waited" -lt 20 ]; do
            sleep 0.1
            waited=$((waited + 1))
        done
        if [ -z "$NO_GL" ] && [ -x ./system/gl_mirror ]; then
            ./system/gl_mirror >/dev/null 2>&1 &
            GL_PID=$!
            # REAL FIX 2026-08-04, direct instruction ("give each window
            # a pid"): GLUT doesn't set _NET_WM_PID itself and this
            # house's WM doesn't synthesize one (confirmed via xprop) -
            # tag this window with the real PID ourselves, right after
            # spawn, so a drag-drop consumer (tp_desktop_window.c) can
            # find THIS specific widget's window later regardless of
            # how many other identically-titled "wsr-pal RGB mirror"
            # windows are also open.
            if [ -x ./ops/+x/tp_set_wm_pid.+x ]; then
                # Real GL_PID (bash's own $!) didn't match the actual
                # running gl_mirror process in testing (gl_mirror forks
                # internally) - re-resolve the true PID via cwd-scoped
                # pgrep, same technique kill_own_module() below already
                # uses, right before tagging.
                (
                    sleep 0.3
                    real_pid=""
                    for cand in $(pgrep -f "system/gl_mirror" 2>/dev/null); do
                        cwd="$(readlink -f "/proc/$cand/cwd" 2>/dev/null)"
                        if [ "$cwd" = "$SESSION_DIR" ]; then real_pid="$cand"; fi
                    done
                    [ -n "$real_pid" ] && ./ops/+x/tp_set_wm_pid.+x "wsr-pal RGB mirror" "$real_pid" >/dev/null 2>&1
                ) &
            fi
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
            if [ "$cwd" = "$SESSION_DIR" ]; then kill -9 "$pid" 2>/dev/null; fi
        done
    }

    # Step 2 symlink-migration fix: copy mutable session state back
    # to the real project root before the session dir is deleted
    # (the old symlinks made these writes land at the real root for
    # free; cp -r sessions need this explicit copy-back). Merge
    # semantics - adds/overwrites, never deletes. Volatile files
    # (quit_flag, pids, history, relays, gui_state) are NOT copied.
    persist_session_state() {
        mkdir -p "$(dirname "$SCRIPT_DIR/pieces/system/picker_items.txt")" 2>/dev/null || true
        cp -r "$SESSION_DIR/pieces/system/picker_items.txt" "$SCRIPT_DIR/pieces/system/picker_items.txt" 2>/dev/null || true
    }
    trap 'kill "$RENDERER_PID" "$CHTPM_PID" "$GL_PID" "$RGB_PID" 2>/dev/null; kill_own_module; persist_session_state; rm -rf "$SESSION_DIR"' EXIT INT TERM

    : > pieces/apps/player_app/history.txt
    ./system/keyboard_input

    kill "$RENDERER_PID" "$CHTPM_PID" "$GL_PID" "$RGB_PID" 2>/dev/null
    kill_own_module
}

case "$ACTION" in
    compile|c|build)
        bash "$SCRIPT_DIR/scripts/build.sh"
        ;;
    run-app|app|a)
        RUN_PROFILE=app run_widget_session
        ;;
    run-widget|widget|w)
        RUN_PROFILE=widget run_widget_session
        ;;
    run|r|start)
        run_widget_session
        ;;
    kill|k|stop)
        pkill -f "system/keyboard_input" 2>/dev/null
        pkill -f "system/renderer" 2>/dev/null
        pkill -f "system/prisc\+x" 2>/dev/null
        pkill -f "system/chtpm_parser_pal" 2>/dev/null
        echo "done"
        ;;
    check|verify)
        for b in system/prisc+x system/keyboard_input system/renderer \
                 system/chtpm_parser_pal system/gl_mirror system/chtpm_rgb_render \
                 ops/+x/tp_compose_frame.+x ops/+x/tp_menu_input.+x \
                 ops/+x/tp_set_brush.+x ops/+x/tp_place_desktop.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b"; else echo "MISSING $b"; fi
        done
        ;;
    help|h|-h|--help|*)
        echo "tile-picker widget — pick a glyph, place it live on the desktop"
        echo "  run | r | start          - Auto-detect mode: ASCII if headless, GL if DISPLAY"
        echo "  run-widget | widget | w  - Force GL window mode"
        echo "  run-app | app | a        - Force ASCII terminal mode"
        echo "  compile                  - Build ops + system"
        echo "  kill                     - Kill processes"
        echo "  check                    - Verify binaries"
        ;;
esac
