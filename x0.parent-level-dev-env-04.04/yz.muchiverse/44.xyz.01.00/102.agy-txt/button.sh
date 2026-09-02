#!/bin/bash
# button.sh - launcher for agy-txt (self-contained editor, PLAN.md
# Phase T2 stub). Adapted from 102.editor-📄️00.00/button.sh's own
# real, proven session-isolation shape - same pattern, own project.
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

case "$ACTION" in
    compile|c|build)
        bash "$SCRIPT_DIR/scripts/build.sh"
        ;;
    run|r|start)
        cd "$SCRIPT_DIR"
        # REAL BUG, LIVE-CAUGHT 2026-07-31: `run` never killed anything
        # before launching - a stale agy_browser_manager.+x from a PRIOR
        # session (never properly killed either, since `kill` itself was
        # missing this daemon from its own pkill list until this same
        # fix - see that action below) kept running indefinitely,
        # sharing the SAME real docs/ symlink every session uses, and
        # produced real, confusing symptoms in a fresh session: typed
        # text not persisting, LOAD/SAVE AS screens not responding to
        # nav past a certain point. A defensive kill here means every
        # launch starts clean regardless of whether a prior session was
        # ever properly torn down.
        # REAL BUG, LIVE-CAUGHT 2026-07-31 (found immediately after the
        # fix above): a bare `pkill; sleep 0.2` does NOT prove anything
        # actually died - pkill only sends the signal and returns
        # immediately. A threaded daemon like agy_browser_manager.+x can
        # outlive a flat 0.2s sleep, giving a real window where the OLD
        # process and the brand-new session's own freshly-launched
        # process are BOTH alive and BOTH consuming the same real
        # keystrokes - confirmed live: typed characters landed doubled
        # in the editor buffer right after this exact fix was added.
        # Poll until genuinely dead instead of guessing a fixed delay.
        PKILL_PATTERNS='system/keyboard_input system/renderer system/prisc\+x system/chtpm_parser_pal system/gl_mirror system/chtpm_rgb_render manager/\+x/agy_browser_manager\.\+x'
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
                 "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/apps/agy-txt/manager" \
                 "$SESSION_DIR/pieces/keyboard"
        # docs/ deliberately NOT pre-created (PITFALL 62) - `ln -sfn`
        # below needs SESSION_DIR/docs to not already exist as a real
        # directory, or it nests the symlink one level too deep instead
        # of replacing it, silently routing every relative "docs/..."
        # SAVE_AS into an ephemeral session-local directory that gets
        # rm -rf'd on exit instead of the durable house-level docs/.

        # No symlinks — C processes resolve shared/persistent files via PRISC_PROJECT_ROOT env var

        cd "$SESSION_DIR"
        : > pieces/apps/player_app/interact_relay.txt
        : > pieces/keyboard/history.txt
        : > pieces/display/frame_changed.txt
        : > pieces/display/editor_screen_changed.txt

        # House root for xyzfs resolution (2026-07-30, save-bug.txt's
        # own fix - resolve_xyzfs_home() in agy_widget_cmds.c reads
        # this, same as editor's own button.sh already writes it and
        # ledger_append.c already depends on it - agy-txt's own
        # button.sh never wrote this at all until now, silently
        # breaking SAVE_AS/LOAD's own new xyzfs-jail resolution).
        echo "$(cd "$SCRIPT_DIR/.." && pwd)" > pieces/system/house_root.txt

        # Seed buffer + cursor (Phase T3, same shape as editor's own
        # button.sh - reused ops expect these exact file names).
        if [ ! -f pieces/system/editor_buffer.txt ]; then
            printf 'hi agy-txt\n' > pieces/system/editor_buffer.txt
        fi
        cat > pieces/system/editor_state.txt << 'EOF'
file_path=untitled.txt
cursor_pos=-1
last_message=Welcome to agy-txt. Focus EDIT TEXT, Enter to INTERACT.
EOF

        cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=agy-txt
active_target_id=editor
EOSTATE

        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        export PRISC_PROJECT_ID="agy-txt"

        if [ -x "./ops/+x/agy_compose_view.+x" ]; then
            ./ops/+x/agy_compose_view.+x >/dev/null 2>&1 || true
        elif [ -x "./ops/+x/agy_compose_stub.+x" ]; then
            ./ops/+x/agy_compose_stub.+x >/dev/null 2>&1 || true
        fi

        ./system/renderer &
        RENDERER_PID=$!
        ./system/chtpm_parser_pal pieces/chtpm/layouts/editor.chtpm >/dev/null 2>&1 &
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
            # REAL BUG, live-caught 2026-07-30: this only ever matched
            # prisc+x (the PAL-driven module) - agy_browser_manager.+x
            # (the new native C manager, launched the identical
            # <module>-fork way for file_browser_save/load.chtpm) was
            # never in this pattern, so every session that ever entered
            # the file browser leaked one orphaned manager process on
            # exit, still polling files under an already-rm-rf'd
            # SESSION_DIR forever. Confirmed live via `ps aux` showing
            # 9 stale agy_browser_manager.+x processes after a handful
            # of test sessions.
            for pid in $(pgrep -f "system/prisc\+x\|manager/\+x/agy_browser_manager\.\+x" 2>/dev/null); do
                cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null)"
                if [ "$cwd" = "$SESSION_DIR" ]; then
                    kill -9 "$pid" 2>/dev/null
                fi
            done
        }

        trap 'kill "$RENDERER_PID" "$CHTPM_PID" "$GL_PID" "$RGB_PID" 2>/dev/null; kill_own_module; rm -rf "$SESSION_DIR"' EXIT INT TERM

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
        # REAL BUG, LIVE-CAUGHT 2026-07-31: this standalone `kill` action
        # never included agy_browser_manager.+x (only the EXIT-trap's own
        # kill_own_module() above did, at line ~120) - every `bash
        # button.sh kill` between test runs left that daemon running
        # forever. Confirmed live: dozens of orphaned instances going
        # back a full day accumulated this way, each still polling/
        # writing real files - a real, plausible cause of hard-to-
        # reproduce state corruption in later sessions.
        pkill -f "manager/\+x/agy_browser_manager\.\+x" 2>/dev/null
        echo "done"
        ;;
    help|h|-h|--help)
        echo "agy-txt button.sh (Phase T2 stub - see PLAN.md)"
        echo "  compile, c, build   - Build ops + copy system/GL/RGB from wsr-pal"
        echo "  run, r              - Interactive (headless if no DISPLAY)"
        echo "  kill, k, stop       - Kill lingering processes"
        ;;
    *)
        echo "Unknown action: $ACTION"
        exit 1
        ;;
esac
