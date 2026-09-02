#!/bin/bash
# button.sh - launcher for aomorai-editor, modeled directly on
# @.apps/civ-txt's own button.sh (real interact+module chtpm
# pattern, session-isolation per xyzos-standards §23). P1 clone phase
# is verified and done (mutant-clone.txt, HANDOFF_NEXT_SESSION.md) -
# real Phase 2 divergence now in progress per civ-vs-piece.md and
# phase2-plan.md. keybinds.txt (real, space=jump/g=mine/h=build) and
# board_widget_bridge.txt/widget_cmds/ (real widget->host command
# delivery, JUMP/MINE/BUILD/MOVE all have real handlers in pc_menu_
# input.c) are seeded below, same real shape civ-txt's own button.sh
# uses.
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# House root - two levels up from this project (aomorai-editor is
# directly under @.apps/), same computation civ-txt's/tactics-txt's own
# button.sh use. Written into each session as house_root.txt so ops
# (like pc_menu_input.c's OPEN_BOARD_WIDGET handler) can find sibling
# projects/widgets without hardcoding a path.
HOUSE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Own-scoped board-viewer widget cleanup (2026-08-03, direct user
# correction: "kill orphan on quit(ctrl+c) and re kill on start just in
# case" - after board-viewer's own ledger registration was scoped
# per-host ("board-viewer:aomorai-editor", see &.widgits/board-viewer/
# button.sh's own header comment), a real gap remained: if aomorai-editor-
# xyz's own game session dies WITHOUT its board-viewer widget also
# dying (crash, force-kill, or simply Ctrl+C not reaching the widget's
# own separate process group), the orphaned widget stays alive and gets
# silently REFOCUSED (not respawned) by the next OPEN_BOARD_WIDGET press
# - correct per the per-project scoping fix, but invisible to the user
# if that orphaned window isn't already on top, which looks exactly
# like "the board didn't open." Two-layer real fix: kill any surviving
# same-scope widget on START (safety net for a prior crash) AND on
# QUIT (so a clean Ctrl+C never leaves one behind in the first place).
# pkill -f matches the widget's own real invocation command line
# directly (board-viewer/button.sh run-widget <this project's own real
# path>) - simpler and more direct than parsing the ledger from bash.
kill_own_board_widget() {
    pkill -f "board-viewer/button.sh run-widget $SCRIPT_DIR" 2>/dev/null || true
}

# REAL, NEW 2026-08-04, direct instruction ("make sure we add demons to
# quit kill... im not running a session" - a REAL orphaned pc_clock_
# daemon.+x was found live, from a session that had already ended, still
# writing to world_01/state.txt and racing with other real writers,
# causing real file corruption). Same exact real two-layer pattern
# kill_own_board_widget() already established above (kill on START as a
# safety net for a prior crash, AND on QUIT so a clean exit never leaves
# one behind) - pc_clock_daemon.+x is a real persistent background
# process (ops/pc_clock_daemon.c's own header comment), same real class
# of "must not outlive its session" risk the board-viewer widget already
# had.
kill_own_clock_daemon() {
    # REAL FIX 2026-08-04, direct user report ("random" ticker behavior,
    # traced to FOUR real daemons from four past sessions all running
    # concurrently at once): this pattern never actually matched
    # anything. The daemon is launched via pc_menu_input.c's own
    # launch_clock_daemon_if_needed() using the EPHEMERAL SESSION root
    # (ops/ is a real symlink INTO that session), so its own real
    # command line is ".../pieces/sessions/<id>/ops/+x/pc_clock_daemon.
    # +x" - never literally "$SCRIPT_DIR/ops/...". Real fix: match the
    # real binary's own suffix only (no session-specific prefix), which
    # catches every past session's own real orphan, not just this one.
    pkill -f "ops/\+x/pc_clock_daemon\.\+x" 2>/dev/null || true
    rm -f "$SCRIPT_DIR/pieces/system/pc_clock_daemon.pid" 2>/dev/null || true
}

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
        # Safety-net cleanup - see kill_own_board_widget()'s own header
        # comment above for why this must run on every fresh start, not
        # just on quit.
        kill_own_board_widget
        kill_own_clock_daemon
        SESSION_ID="$(date +%s)-$$"
        SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
        mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
                 "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
                 "$SESSION_DIR/pieces/os" "$SESSION_DIR/projects/aomorai-editor/manager"
        mkdir -p "$SCRIPT_DIR/data"

        cp -r "$SCRIPT_DIR/pieces/os/kill_all.sh" "$SESSION_DIR/pieces/os/kill_all.sh" 2>/dev/null
        cp -r "$SCRIPT_DIR/system" "$SESSION_DIR/system"
        cp -r "$SCRIPT_DIR/ops" "$SESSION_DIR/ops"
        cp -r "$SCRIPT_DIR/pal" "$SESSION_DIR/pal"
        cp -r "$SCRIPT_DIR/default_op.txt" "$SESSION_DIR/default_op.txt"
        cp -r "$SCRIPT_DIR/pieces/chtpm" "$SESSION_DIR/pieces/chtpm"
        cp -r "$SCRIPT_DIR/pieces/registry" "$SESSION_DIR/pieces/registry" 2>/dev/null
        cp -r "$SCRIPT_DIR/projects/aomorai-editor/pieces" "$SESSION_DIR/projects/aomorai-editor/pieces"
        cp -r "$SCRIPT_DIR/data" "$SESSION_DIR/data"

        cd "$SESSION_DIR"
        : > pieces/apps/player_app/interact_relay.txt
        : > pieces/keyboard/history.txt
        : > pieces/system/quit_flag.txt
        : > pieces/display/pc_screen_changed.txt
        : > pieces/display/frame_changed.txt
        : > projects/aomorai-editor/manager/gui_state.txt
        # For OPEN_BOARD_WIDGET (and any other future widget-spawn
        # logic) to find &.widgits/ and aomorai-editor's own REAL
        # (non-session) project root.
        echo "$HOUSE_DIR" > pieces/system/house_root.txt
        echo "$SCRIPT_DIR" > pieces/system/real_project_root.txt

        # Fresh per-session config.txt seeded with starting state if the
        # persistent one under the REAL project dir doesn't exist yet.
        # Same shape civ-txt's own P1 config uses - generic game config,
        # NOT hero/player position (that's Phase 2+ entity state).
        if [ ! -f "$SCRIPT_DIR/pieces/system/config.txt" ]; then
            mkdir -p "$SCRIPT_DIR/pieces/system"
            cat > "$SCRIPT_DIR/pieces/system/config.txt" << 'EOCONFIG'
game_id=aomorai-editor-001
turn=1
turn_order_index=0
victory_condition=
map_scale=
combat_resolution=
treasury=50
city_count=1
game_state=setup
EOCONFIG
        fi
        cp -r "$SCRIPT_DIR/pieces/system/config.txt" "$SESSION_DIR/pieces/system/config.txt"
        # board.txt is REAL PERSISTENT DATA (like config.txt), generated
        # by CONFIRM_START and read by the board-viewer widget from
        # aomorai-editor's own REAL (non-session) root - must be
        # symlinked into every session the same way config.txt is.
        touch "$SCRIPT_DIR/pieces/system/board.txt"
        cp -r "$SCRIPT_DIR/pieces/system/board.txt" "$SESSION_DIR/pieces/system/board.txt"
        # entities.txt - real generic entity manifest board-viewer
        # reads. Not yet populated - aomorai-editor has no real
        # cities/units data yet in P1 clone phase.
        touch "$SCRIPT_DIR/pieces/system/entities.txt"
        cp -r "$SCRIPT_DIR/pieces/system/entities.txt" "$SESSION_DIR/pieces/system/entities.txt"

        # widget_cmds/inbox.txt + board_widget_bridge.txt - real Phase 2
        # widget->host command delivery (phase2-plan.md §6 step 1 -
        # keybinds.txt is real now, JUMP/MINE/BUILD/MOVE all have real
        # handlers in pc_menu_input.c). Same real shape civ-txt's own
        # button.sh already uses - board-viewer's own bv_menu_input.c
        # reads keybinds.txt + this bridge file directly from the REAL
        # (non-session) project root, so nothing here needs session-
        # symlinking for board-viewer's OWN reads, but civ-txt's own
        # convention still symlinks the inbox itself so THIS project's
        # own pc_menu_input.c (running inside the session) can drain it.
        mkdir -p "$SCRIPT_DIR/pieces/system/widget_cmds"
        touch "$SCRIPT_DIR/pieces/system/widget_cmds/inbox.txt"
        cp -r "$SCRIPT_DIR/pieces/system/widget_cmds" "$SESSION_DIR/pieces/system/widget_cmds"
        cat > "$SCRIPT_DIR/pieces/system/board_widget_bridge.txt" << EOF
inbox_path=pieces/system/widget_cmds/inbox.txt
kind=board_game
project_id=aomorai-editor
display_name=Aomorai-editor
EOF
        cp -r "$SCRIPT_DIR/pieces/system/board_widget_bridge.txt" "$SESSION_DIR/pieces/system/board_widget_bridge.txt"

        cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/new_game_module.pal
project_id=aomorai-editor
active_target_id=new_game
EOSTATE

        export PRISC_PROJECT_ROOT="$SESSION_DIR"
        export PRISC_PROJECT_ID="aomorai-editor"
        export NO_NET=1
        export PAL_LAYOUT="pieces/chtpm/layouts/new_game.chtpm"
        "$SCRIPT_DIR/system/orchestrator" 2>>pieces/system/orchestrator.log &
        ORCH_PID=$!

        # OPTIONAL GL/RGB MIRROR - gated on NO_GL and a real DISPLAY,
        # skips gracefully otherwise. MUST wait for chtpm_parser_pal's
        # own first real compose before launching chtpm_rgb_render, or
        # rgb_frame.raw gets stuck permanently all-black/stale.
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
            mkdir -p "$(dirname "$SCRIPT_DIR/pieces/system/config.txt")" 2>/dev/null || true
            cp -r "$SESSION_DIR/pieces/system/config.txt" "$SCRIPT_DIR/pieces/system/config.txt" 2>/dev/null || true
            mkdir -p "$(dirname "$SCRIPT_DIR/pieces/system/board.txt")" 2>/dev/null || true
            cp -r "$SESSION_DIR/pieces/system/board.txt" "$SCRIPT_DIR/pieces/system/board.txt" 2>/dev/null || true
            mkdir -p "$(dirname "$SCRIPT_DIR/pieces/system/entities.txt")" 2>/dev/null || true
            cp -r "$SESSION_DIR/pieces/system/entities.txt" "$SCRIPT_DIR/pieces/system/entities.txt" 2>/dev/null || true
            mkdir -p "$SCRIPT_DIR/pieces/system/widget_cmds/" 2>/dev/null || true
            cp -r "$SESSION_DIR/pieces/system/widget_cmds/." "$SCRIPT_DIR/pieces/system/widget_cmds/" 2>/dev/null || true
            mkdir -p "$(dirname "$SCRIPT_DIR/pieces/system/board_widget_bridge.txt")" 2>/dev/null || true
            cp -r "$SESSION_DIR/pieces/system/board_widget_bridge.txt" "$SCRIPT_DIR/pieces/system/board_widget_bridge.txt" 2>/dev/null || true
        }
        trap 'kill "$ORCH_PID" "$GL_PID" "$RGB_PID" 2>/dev/null; wait "$ORCH_PID" 2>/dev/null; kill_own_module; kill_own_board_widget; kill_own_clock_daemon; persist_session_state; rm -rf "$SESSION_DIR"' EXIT INT TERM

        ./system/keyboard_input

        kill "$ORCH_PID" "$GL_PID" "$RGB_PID" 2>/dev/null
        kill_own_module
        ;;
    kill|k|stop)
        pkill -f "system/keyboard_input" 2>/dev/null
        pkill -f "system/renderer" 2>/dev/null
        pkill -f "system/prisc\+x" 2>/dev/null
        pkill -f "system/chtpm_parser_pal" 2>/dev/null
        pkill -f "system/chtpm_rgb_render" 2>/dev/null
        pkill -f "system/gl_mirror" 2>/dev/null
        pkill -f "system/orchestrator" 2>/dev/null
        kill_own_board_widget
        kill_own_clock_daemon
        echo "done"
        ;;
    check|verify)
        for b in system/prisc+x system/keyboard_input system/renderer \
                 system/chtpm_parser_pal system/orchestrator \
                 ops/+x/pc_menu_input.+x ops/+x/pc_compose_frame.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b"; else echo "MISSING $b"; fi
        done
        for b in system/chtpm_rgb_render system/gl_mirror; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b (optional GL mirror)"; else echo "OPTIONAL-MISS $b"; fi
        done
        ;;
    help|h|-h|--help)
        echo "aomorai-editor button.sh"
        echo ""
        echo "Usage: ./button.sh <action>"
        echo "  compile, c, build   - Build prisc+x + ops"
        echo "  run, r              - THE REAL PLAYABLE UI (interactive, needs a real terminal)"
        echo "  kill, k, stop       - Kill any lingering aomorai-editor processes"
        echo "  check, verify       - Verify all binaries exist"
        echo "  help, h             - Show this help"
        ;;
    *)
        echo "Unknown action: $ACTION"
        exit 1
        ;;
esac
