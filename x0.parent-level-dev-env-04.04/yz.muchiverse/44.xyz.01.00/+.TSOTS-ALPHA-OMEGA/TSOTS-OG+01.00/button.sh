#!/bin/bash
# button.sh - launcher for TSOTS (This Order Still Stands), the
# bible-verse reorder game. Adapted from 102.agy-txt/button.sh's own
# real, proven session-isolation shape - same pattern, own project.
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

case "$ACTION" in
    compile|c|build)
        bash "$SCRIPT_DIR/scripts/build.sh"
        ;;
    run|r|start)
        cd "$SCRIPT_DIR"
        # Defensive kill before launch (REAL BUG, LIVE-CAUGHT in agy-txt
        # 2026-07-31): a stale system process from a PRIOR session can
        # outlive its teardown and keep sharing this project's real
        # files. Poll until genuinely dead - a flat `pkill; sleep` does
        # NOT prove anything actually died.
        PKILL_PATTERNS='system/keyboard_input system/renderer system/prisc\+x system/chtpm_parser_pal system/gl_mirror system/chtpm_rgb_render'
        for pat in $PKILL_PATTERNS; do
            pkill -f "$pat" 2>/dev/null
        done
        waited=0
        while [ "$waited" -lt 30 ]; do
            still_alive=0
            for pat in $PKILL_PATTERNS; do
                pgrep -f "$pat" >/dev/null 2>&1 && still_alive=1
            done
            [ "$still_alive" = "0" ] && break
            sleep 0.1
            waited=$((waited + 1))
        done
        if [ "$still_alive" = "1" ]; then
            for pat in $PKILL_PATTERNS; do
                pkill -9 -f "$pat" 2>/dev/null
            done
            sleep 0.2
        fi
        PROFILE="${RUN_PROFILE:-}"
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
                 "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard"

        cp -r "$SCRIPT_DIR/system" "$SESSION_DIR/system"
        cp -r "$SCRIPT_DIR/ops" "$SESSION_DIR/ops"
        cp -r "$SCRIPT_DIR/pal" "$SESSION_DIR/pal"
        cp -r "$SCRIPT_DIR/default_op.txt" "$SESSION_DIR/default_op.txt"
        cp -r "$SCRIPT_DIR/pieces/chtpm" "$SESSION_DIR/pieces/chtpm"
        cp -r "$SCRIPT_DIR/pieces/registry" "$SESSION_DIR/pieces/registry" 2>/dev/null || true
        # location.txt holds the bible path (task-1 fix) - the deal op
        # reads it relative to the session root.
        cp -r "$SCRIPT_DIR/location.txt" "$SESSION_DIR/location.txt"

        cd "$SESSION_DIR"
        : > pieces/apps/player_app/interact_relay.txt
        : > pieces/keyboard/history.txt
        : > pieces/display/frame_changed.txt
        : > pieces/display/game_screen_changed.txt

        cat > pieces/system/game_state.txt << 'EOSTATE'
elo=1000
wins=0
losses=0
round=0
status=none
answer=
last_result=none
last_delta=0
EOSTATE
        : > pieces/system/round.txt
        : > pieces/system/solution.txt

        cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/menu_loop.pal
project_id=tsots
active_target_id=menu
EOSTATE

        export PRISC_PROJECT_ROOT="$SESSION_DIR"
        export PRISC_PROJECT_ID="tsots"

        if [ -x "./ops/+x/tsots_compose.+x" ]; then
            ./ops/+x/tsots_compose.+x >/dev/null 2>&1 || true
        fi

        ./system/renderer &
        RENDERER_PID=$!
        ./system/chtpm_parser_pal pieces/chtpm/layouts/menu.chtpm >/dev/null 2>&1 &
        CHTPM_PID=$!

        GL_PID=""
        RGB_PID=""
        if [ "$PROFILE" = "widget" ] || ([ "$PROFILE" = "app" ] && [ -n "$DISPLAY" ]); then
            # Same three-layer race fix as editor's own button.sh
            # (PITFALL 54) - wait for a real compose before opening GL.
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
            mkdir -p "$(dirname "$SCRIPT_DIR/location.txt")" 2>/dev/null || true
            cp -r "$SESSION_DIR/location.txt" "$SCRIPT_DIR/location.txt" 2>/dev/null || true
        }
        trap 'kill "$RENDERER_PID" "$CHTPM_PID" "$GL_PID" "$RGB_PID" 2>/dev/null; kill_own_module; persist_session_state; rm -rf "$SESSION_DIR"' EXIT INT TERM

        : > pieces/apps/player_app/history.txt

        ./system/keyboard_input

        kill "$RENDERER_PID" "$CHTPM_PID" "$GL_PID" "$RGB_PID" 2>/dev/null
        kill_own_module
        ;;
    kill|k|stop)
        pkill -f "system/keyboard_input" 2>/dev/null
        pkill -f "system/renderer" 2>/dev/null
        pkill -f "system/prisc\+x" 2>/dev/null
        pkill -f "system/chtpm_parser_pal" 2>/dev/null
        pkill -f "system/gl_mirror" 2>/dev/null
        pkill -f "system/chtpm_rgb_render" 2>/dev/null
        echo "done"
        ;;
    help|h|-h|--help)
        echo "TSOTS button.sh (This Order Still Stands - bible verse reorder game)"
        echo "  compile, c, build   - Build ops + copy system/GL/RGB from wsr-pal"
        echo "  run, r              - Interactive (headless if no DISPLAY)"
        echo "  kill, k, stop       - Kill lingering processes"
        ;;
    *)
        echo "Unknown action: $ACTION"
        exit 1
        ;;
esac
