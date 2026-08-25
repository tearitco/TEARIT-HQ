#!/bin/bash
# button.sh - file-menu widget (ops + GL widget chrome)
# House pattern: session-isolated UI state + shared system/ops/pal.
# run-widget: GL-primary window for file operations against a focused editor.
# Profile: RUN_PROFILE=app (default) or RUN_PROFILE=widget
#   app mode:    ascii_renderer=1, keyboard_input foreground
#   widget mode: ascii_renderer=0, no keyboard_input, gl_mirror for GL + input
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

run_widget_session() {
    local FOCUS_SESSION="${1:-}"
    local PROFILE="${RUN_PROFILE:-}"
    # Auto-detect profile: headless (no DISPLAY) → app mode, else → widget mode
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
             "$SESSION_DIR/projects/file-menu/manager"

    cp -r "$SCRIPT_DIR/system" "$SESSION_DIR/system" 2>/dev/null || true
    cp -r "$SCRIPT_DIR/ops" "$SESSION_DIR/ops" 2>/dev/null || true
    cp -r "$SCRIPT_DIR/pal" "$SESSION_DIR/pal" 2>/dev/null || true
    cp -r "$SCRIPT_DIR/pieces/chtpm" "$SESSION_DIR/pieces/chtpm" 2>/dev/null || true
    cp -r "$SCRIPT_DIR/pieces/registry" "$SESSION_DIR/pieces/registry" 2>/dev/null || true
    cp -r "$SCRIPT_DIR/default_op.txt" "$SESSION_DIR/default_op.txt" 2>/dev/null || true
    mkdir -p "$SESSION_DIR/projects/file-menu/pieces"
    cp -r "$SCRIPT_DIR/projects/file-menu/pieces/file-menu" \
            "$SESSION_DIR/projects/file-menu/pieces/file-menu" 2>/dev/null || true

    cd "$SESSION_DIR"
    : > pieces/apps/player_app/history.txt
    : > pieces/apps/player_app/interact_relay.txt
    : > pieces/keyboard/history.txt
    : > pieces/display/fm_screen_changed.txt
    : > pieces/display/frame_changed.txt
    : > pieces/display/renderer_pulse.txt
    : > pieces/system/fm_state.txt
    echo "$(cd "$SCRIPT_DIR/../.." && pwd)" > pieces/system/house_root.txt
    cat > pieces/system/fm_state.txt << 'EOF'
path_buffer=
EOF
    cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=file-menu
active_target_id=file-menu
EOSTATE

    export PRISC_PROJECT_ROOT="$SESSION_DIR"
    export PRISC_PROJECT_ID="file-menu"

    # If no focus given, try ledger to find an active editor
    if [ -z "$FOCUS_SESSION" ] && [ -x ./ops/+x/ledger_peers.+x ]; then
        FOCUS_SESSION="$(PRISC_PROJECT_ROOT="$SESSION_DIR" ./ops/+x/ledger_peers.+x editor 2>/dev/null | head -1 | cut -d'|' -f1)"
    fi
    if [ -n "$FOCUS_SESSION" ] && [ -x ./ops/+x/fm_set_focus.+x ]; then
        ./ops/+x/fm_set_focus.+x "$SESSION_DIR/pieces/system" "$FOCUS_SESSION" >/dev/null 2>&1 || true
    fi

    if [ -x ./ops/+x/fm_compose_frame.+x ]; then
        ./ops/+x/fm_compose_frame.+x >/dev/null 2>&1 || true
    fi

    # Start both ASCII renderer and layout parser (chtpm_parser_pal runs PAL, renderer prints ASCII)
    ./system/renderer &
    RENDERER_PID=$!
    ./system/chtpm_parser_pal pieces/chtpm/layouts/file_menu_main.chtpm >/dev/null 2>&1 &
    CHTPM_PID=$!

    GL_PID=""
    RGB_PID=""

    # Optionally start GL widget (widget mode or auto-detection)
    if [ "$PROFILE" = "widget" ] || ([ "$PROFILE" = "app" ] && [ -n "$DISPLAY" ]); then
        if [ -x ./ops/+x/ledger_append.+x ]; then
            PRISC_PROJECT_ROOT="$SESSION_DIR" \
                ./ops/+x/ledger_append.+x ONLINE widget file-menu "$SESSION_DIR" $$ \
                "FILE MENU" pieces/system/widget_cmds/inbox.txt >/dev/null 2>&1 || true
        fi
        # THREE-LAYER RACE FIX (re-applied 2026-07-30 — see
        # !.xyzos-pitfalls+1.txt PITFALL 54 and this project's own
        # widget-bug-fix-j29.txt under @.apps/text-editor-xyz/: this
        # exact wait step was documented as already fixed here once,
        # found MISSING again this session — regression, cause not yet
        # determined). chtpm_rgb_render's own initial render + baseline
        # pulse-size read must happen AFTER chtpm_parser_pal's first
        # compose_frame(), or rgb_frame.raw gets stuck permanently
        # all-black (the poll loop only re-renders on a pulse-size
        # CHANGE from its own baseline).
        waited=0
        while [ ! -s pieces/display/current_frame.txt ] && [ "$waited" -lt 20 ]; do
            sleep 0.1
            waited=$((waited + 1))
        done
        if [ -z "$NO_GL" ] && [ -x ./system/gl_mirror ]; then
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
        mkdir -p "$SCRIPT_DIR/projects/file-menu/pieces/file-menu/" 2>/dev/null || true
        cp -r "$SESSION_DIR/projects/file-menu/pieces/file-menu/." "$SCRIPT_DIR/projects/file-menu/pieces/file-menu/" 2>/dev/null || true
    }
    trap 'if [ -x ./ops/+x/ledger_append.+x ]; then PRISC_PROJECT_ROOT="$SESSION_DIR" ./ops/+x/ledger_append.+x OFFLINE widget file-menu "$SESSION_DIR" $$ "FILE MENU" pieces/system/widget_cmds/inbox.txt >/dev/null 2>&1 || true; fi; kill "$RENDERER_PID" "$CHTPM_PID" "$GL_PID" "$RGB_PID" 2>/dev/null; kill_own_module; persist_session_state; rm -rf "$SESSION_DIR"' EXIT INT TERM

    # Clear history and run keyboard input in foreground
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
            echo "Usage: ./button.sh focus <editor_session_root>"
            exit 1
        fi
        mkdir -p "$SCRIPT_DIR/pieces/sessions/live-focus"
        if [ -x "$SCRIPT_DIR/ops/+x/fm_set_focus.+x" ]; then
            "$SCRIPT_DIR/ops/+x/fm_set_focus.+x" "$SCRIPT_DIR/pieces/sessions/live-focus" "$FOCUS"
        fi
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
                 system/chtpm_parser_pal ops/+x/fm_compose_frame.+x \
                 ops/+x/fm_menu_input.+x ops/+x/fm_set_focus.+x \
                 ops/+x/fm_enqueue_cmd.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b"; else echo "MISSING $b"; fi
        done
        ;;
    help|h|-h|--help)
        echo "file-menu widget — File browser for project operations"
        echo ""
        echo "Usage: ./button.sh <action> [editor_session]"
        echo "  run | r | start          - Auto-detect mode: ASCII if headless, GL if DISPLAY"
        echo "  run-widget | widget | w  - Force GL window mode"
        echo "  run-app | app | a        - Force ASCII terminal mode"
        echo "  run <session>            - Auto-detect mode, focused on specific project"
        echo "  focus <session>          - Pre-set project focus"
        echo "  compile                  - Build ops + system"
        echo "  kill                     - Kill processes"
        echo "  check                    - Verify binaries"
        echo ""
        echo "Modes:"
        echo "  ASCII (app mode): keyboard input foreground, renders to terminal"
        echo "  GL (widget mode): window mode, gl_mirror handles rendering + input"
        echo ""
        echo "Environment:"
        echo "  RUN_PROFILE=app          - Force ASCII mode regardless of DISPLAY"
        echo "  RUN_PROFILE=widget       - Force GL mode (default if DISPLAY set)"
        echo ""
        echo "Commands sent to focused project: NEW / SAVE / SAVE_AS:<path> / LOAD:<path>"
        ;;
    *)
        echo "Unknown: $ACTION"; exit 1 ;;
esac
