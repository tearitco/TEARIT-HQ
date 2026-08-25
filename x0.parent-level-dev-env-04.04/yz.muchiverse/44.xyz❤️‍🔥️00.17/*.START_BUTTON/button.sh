#!/bin/bash
# button.sh - HOUSE LOADER (START_BUTTON)
# Same-TTY handoff: when an entry is chosen, yield keyboard, kill UI,
# run target ./button.sh run in THIS terminal, then return to loader.
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

run_one_session() {
    local SESSION_ID SESSION_DIR
    SESSION_ID="$(date +%s)-$$"
    SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
    mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
             "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
             "$SESSION_DIR/projects/start-button/manager"

    # No symlinks — C processes resolve shared/persistent files via PRISC_PROJECT_ROOT env var
    # piece.pdl dirs must be writable for scan — copy structure, link parent pieces tree carefully
    mkdir -p "$SESSION_DIR/projects/start-button/pieces"
    for s in home system widgets apps store; do
        mkdir -p "$SESSION_DIR/projects/start-button/pieces/$s"
        # start with install copies if present
        if [ -f "$SCRIPT_DIR/projects/start-button/pieces/$s/piece.pdl" ]; then
            cp -f "$SCRIPT_DIR/projects/start-button/pieces/$s/piece.pdl" \
                  "$SESSION_DIR/projects/start-button/pieces/$s/piece.pdl" 2>/dev/null || true
        fi
    done

    cd "$SESSION_DIR"
    : > pieces/apps/player_app/interact_relay.txt
    : > pieces/keyboard/history.txt
    : > pieces/display/start_screen_changed.txt
    : > pieces/system/start_state.txt
    rm -f pieces/system/handoff_launch.txt pieces/system/quit_request.txt pieces/system/quit_flag.txt

    cat > pieces/system/start_state.txt << 'EOF'
last_message=Pick a category (System / Widgets / Apps / App Store).
EOF
    cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=start-button
active_target_id=home
EOSTATE
    echo "pieces/chtpm/layouts/home.chtpm" > pieces/display/current_layout.txt

    export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
    export PRISC_INSTALL_ROOT="$SCRIPT_DIR"
    export PRISC_PROJECT_ID="start-button"
    # Keep durable history path if parent run exported it
    if [ -n "${PRISC_FRAME_HISTORY:-}" ]; then
        export PRISC_FRAME_HISTORY
    fi

    # Pre-scan catalogs into session piece.pdl
    if [ -x ./ops/+x/start_scan.+x ]; then
        ./ops/+x/start_scan.+x all >/dev/null 2>&1 || true
    fi
    if [ -x ./ops/+x/start_compose_frame.+x ]; then
        ./ops/+x/start_compose_frame.+x >/dev/null 2>&1 || true
    fi

    ./system/renderer &
    RENDERER_PID=$!
    ./system/chtpm_parser_pal pieces/chtpm/layouts/home.chtpm >/dev/null 2>&1 &
    CHTPM_PID=$!

    kill_own_module() {
        local pid cwd
        for pid in $(pgrep -f "system/prisc\+x" 2>/dev/null); do
            cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null)"
            if [ "$cwd" = "$SESSION_DIR" ]; then
                kill -9 "$pid" 2>/dev/null
            fi
        done
    }

    # HANDOFF path is under session; copy out before rm -rf
    HANDOFF_COPY=""
    trap 'kill "$RENDERER_PID" "$CHTPM_PID" 2>/dev/null; kill_own_module' INT TERM

    : > pieces/apps/player_app/history.txt
    ./system/keyboard_input
    # keyboard exited: Ctrl+C or handoff yield

    if [ -s pieces/system/handoff_launch.txt ]; then
        HANDOFF_COPY="$(cat pieces/system/handoff_launch.txt | head -1)"
    fi

    kill "$RENDERER_PID" "$CHTPM_PID" 2>/dev/null
    kill_own_module
    wait "$RENDERER_PID" 2>/dev/null || true
    wait "$CHTPM_PID" 2>/dev/null || true

    mkdir -p "$SCRIPT_DIR/pieces/system"
    if [ -n "$HANDOFF_COPY" ]; then
        printf '%s\n' "$HANDOFF_COPY" > "$SCRIPT_DIR/pieces/system/last_handoff.txt"
    else
        rm -f "$SCRIPT_DIR/pieces/system/last_handoff.txt"
    fi

    rm -rf "$SESSION_DIR"
    trap - INT TERM

    if [ -n "$HANDOFF_COPY" ]; then
        return 0
    fi
    return 1
}

case "$ACTION" in
    compile|c|build)
        bash "$SCRIPT_DIR/scripts/build.sh"
        ;;
    run|r|start)
        # DEV: always compile on run so source edits are live without a
        # separate `./button.sh compile`. Drop this (or gate on env) later
        # when binaries are considered stable.
        echo "=== START_BUTTON: auto-compile (dev) ==="
        if ! bash "$SCRIPT_DIR/scripts/build.sh"; then
            echo "START_BUTTON: compile failed — not launching."
            exit 1
        fi
        # Durable frame audit log — cleared every new top-level run (session
        # also has pieces/display/frame_history.txt, wiped with the session).
        mkdir -p "$SCRIPT_DIR/debug"
        : > "$SCRIPT_DIR/debug/frame_history.txt"
        export PRISC_FRAME_HISTORY="$SCRIPT_DIR/debug/frame_history.txt"
        mkdir -p "$SCRIPT_DIR/pieces/system"
        while true; do
            if run_one_session; then
                TARGET="$(cat "$SCRIPT_DIR/pieces/system/last_handoff.txt" 2>/dev/null | head -1)"
                rm -f "$SCRIPT_DIR/pieces/system/last_handoff.txt"
                if [ -n "$TARGET" ] && [ -d "$TARGET" ] && [ -f "$TARGET/button.sh" ]; then
                    echo ""
                    echo "=== START_BUTTON: launching $TARGET (same terminal) ==="
                    echo ""
                    # Foreground: owns this TTY until exit, then loader restarts
                    (cd "$TARGET" && bash ./button.sh run) || true
                    echo ""
                    echo "=== START_BUTTON: returned from $TARGET — reopening loader ==="
                    echo ""
                    continue
                fi
            fi
            break
        done
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
                 system/chtpm_parser_pal ops/+x/start_scan.+x \
                 ops/+x/start_compose_frame.+x ops/+x/start_menu_input.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b"; else echo "MISSING $b"; fi
        done
        ;;
    help|h|-h|--help)
        echo "START_BUTTON / HOUSE LOADER"
        echo "  compile | run | kill | check | help"
        echo "  run = auto-compile (dev) then category pre-screen."
        echo "  Selected program runs in THIS terminal; exits back to loader."
        ;;
    *)
        echo "Unknown: $ACTION"; exit 1 ;;
esac
