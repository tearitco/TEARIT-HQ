#!/bin/bash
# button.sh - launcher for my-lawyer, modeled directly on my-biotech's
# own button.sh (session-isolation per xyzos-standards §23).
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

case "$ACTION" in
    compile|c|build)
        bash "$SCRIPT_DIR/scripts/build.sh"
        ;;
    run|r|start)
        cd "$SCRIPT_DIR"
        if [ ! -x "$SCRIPT_DIR/system/orchestrator" ]; then
            echo "Compiling..."
            bash "$SCRIPT_DIR/scripts/build.sh"
        fi
        SESSION_ID="$(date +%s)-$$"
        SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
        mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
                 "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
                 "$SESSION_DIR/pieces/os" "$SESSION_DIR/projects/my-lawyer/manager"
        mkdir -p "$SCRIPT_DIR/data/corpus" "$SCRIPT_DIR/data/cases"
        mkdir -p "$SCRIPT_DIR/projects/my-lawyer/pieces/main" \
                 "$SCRIPT_DIR/projects/my-lawyer/pieces/docket" \
                 "$SCRIPT_DIR/projects/my-lawyer/pieces/case" \
                 "$SCRIPT_DIR/projects/my-lawyer/pieces/mylawyer_menu"

        cp -r "$SCRIPT_DIR/pieces/os/kill_all.sh" "$SESSION_DIR/pieces/os/kill_all.sh" 2>/dev/null
        cp -r "$SCRIPT_DIR/system" "$SESSION_DIR/system"
        cp -r "$SCRIPT_DIR/ops" "$SESSION_DIR/ops"
        cp -r "$SCRIPT_DIR/pal" "$SESSION_DIR/pal"
        cp -r "$SCRIPT_DIR/default_op.txt" "$SESSION_DIR/default_op.txt"
        cp -r "$SCRIPT_DIR/pieces/chtpm" "$SESSION_DIR/pieces/chtpm"
        cp -r "$SCRIPT_DIR/pieces/registry" "$SESSION_DIR/pieces/registry"
        cp -r "$SCRIPT_DIR/projects/my-lawyer/pieces" "$SESSION_DIR/projects/my-lawyer/pieces"
        cp -r "$SCRIPT_DIR/data" "$SESSION_DIR/data"

        cd "$SESSION_DIR"
        : > pieces/apps/player_app/interact_relay.txt
        : > pieces/keyboard/history.txt
        : > pieces/system/quit_flag.txt
        : > pieces/display/mylawyer_screen_changed.txt
        : > pieces/display/frame_changed.txt
        : > projects/my-lawyer/manager/gui_state.txt

        if [ ! -f "$SCRIPT_DIR/pieces/system/config.txt" ]; then
            mkdir -p "$SCRIPT_DIR/pieces/system"
            cat > "$SCRIPT_DIR/pieces/system/config.txt" << 'EOCONFIG'
game_id=my-lawyer-001
player_name=Adam Chen
day=1
max_days=10
money=500
active_case_id=0
game_state=playing
EOCONFIG
        fi
        cp -r "$SCRIPT_DIR/pieces/system/config.txt" "$SESSION_DIR/pieces/system/config.txt"

        cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=my-lawyer
active_target_id=main
EOSTATE

        export PRISC_PROJECT_ROOT="$SESSION_DIR"
        export PRISC_PROJECT_ID="my-lawyer"
        export NO_NET=1
        export PAL_LAYOUT="pieces/chtpm/layouts/main.chtpm"
        "$SCRIPT_DIR/system/orchestrator" 2>>pieces/system/orchestrator.log &
        ORCH_PID=$!

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
            mkdir -p "$(dirname "$SCRIPT_DIR/pieces/system/config.txt")" 2>/dev/null || true
            cp -r "$SESSION_DIR/pieces/system/config.txt" "$SCRIPT_DIR/pieces/system/config.txt" 2>/dev/null || true
        }
        trap 'kill "$ORCH_PID" 2>/dev/null; wait "$ORCH_PID" 2>/dev/null; kill_own_module; persist_session_state; rm -rf "$SESSION_DIR"' EXIT INT TERM

        ./system/keyboard_input

        kill "$ORCH_PID" 2>/dev/null
        kill_own_module
        ;;
    kill|k|stop)
        pkill -f "system/keyboard_input" 2>/dev/null
        pkill -f "system/renderer" 2>/dev/null
        pkill -f "system/prisc\+x" 2>/dev/null
        pkill -f "system/chtpm_parser_pal" 2>/dev/null
        pkill -f "system/chtpm_rgb_render" 2>/dev/null
        pkill -f "system/orchestrator" 2>/dev/null
        echo "done"
        ;;
    check|verify)
        for b in system/prisc+x system/keyboard_input system/renderer \
                 system/chtpm_parser_pal system/orchestrator \
                 ops/+x/mylawyer_menu_input.+x ops/+x/mylawyer_compose_frame.+x \
                 ops/+x/mylawyer_case_worker.+x ops/+x/mylawyer_judge_worker.+x \
                 ops/+x/connect_op.+x ops/+x/json_parser.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b"; else echo "MISSING $b"; fi
        done
        ;;
    help|h|-h|--help)
        echo "my-lawyer button.sh"
        echo "Usage: ./button.sh <action>"
        echo "  compile, c, build   - Build prisc+x + ops"
        echo "  run, r              - THE REAL PLAYABLE UI"
        echo "  kill, k, stop       - Kill any lingering my-lawyer processes"
        echo "  check, verify       - Verify all binaries exist"
        ;;
    *)
        echo "Unknown action: $ACTION"
        exit 1
        ;;
esac
