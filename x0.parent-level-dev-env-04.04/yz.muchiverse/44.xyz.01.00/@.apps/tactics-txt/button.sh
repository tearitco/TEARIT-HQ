#!/bin/bash
# button.sh - launcher for tactics-txt, modeled directly on
# @.apps/my-chara-txt's own button.sh (real interact+module chtpm
# pattern, session-isolation per xyzos-standards §23). Single-player,
# both sides local for now (NO_NET=1) - P1 skeleton only.
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# House root - two levels up (tactics-txt is directly under @.apps/),
# same computation civ-txt's own button.sh uses - see that file's own
# comment / &.widgits/WIDGIT_BIBLE.md's house_root.txt convention.
HOUSE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Own-scoped board-viewer widget cleanup (2026-08-03, direct user
# correction caught while building piececraft-xyz - see that project's
# own button.sh header comment on this same change for the full
# writeup: after board-viewer's own ledger registration was scoped
# per-host ("board-viewer:tactics-txt"), a real gap remained - if this
# project's own game session dies without its board-viewer widget also
# dying, the orphaned widget stays alive and gets silently REFOCUSED
# (not respawned) by the next OPEN_BOARD_WIDGET press, invisible if
# that window isn't already on top. Kill any surviving same-scope
# widget on START (safety net) AND on QUIT (so a clean Ctrl+C never
# leaves one behind).
kill_own_board_widget() {
    pkill -f "board-viewer/button.sh run-widget $SCRIPT_DIR" 2>/dev/null || true
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
        # comment above for why this must run on every fresh start.
        kill_own_board_widget
        SESSION_ID="$(date +%s)-$$"
        SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
        mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
                 "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
                 "$SESSION_DIR/pieces/os" "$SESSION_DIR/projects/tactics-txt/manager"
        mkdir -p "$SCRIPT_DIR/data"

        cp -r "$SCRIPT_DIR/pieces/os/kill_all.sh" "$SESSION_DIR/pieces/os/kill_all.sh" 2>/dev/null
        cp -r "$SCRIPT_DIR/system" "$SESSION_DIR/system"
        cp -r "$SCRIPT_DIR/ops" "$SESSION_DIR/ops"
        cp -r "$SCRIPT_DIR/pal" "$SESSION_DIR/pal"
        cp -r "$SCRIPT_DIR/default_op.txt" "$SESSION_DIR/default_op.txt"
        cp -r "$SCRIPT_DIR/pieces/chtpm" "$SESSION_DIR/pieces/chtpm"
        cp -r "$SCRIPT_DIR/pieces/registry" "$SESSION_DIR/pieces/registry"
        cp -r "$SCRIPT_DIR/projects/tactics-txt/pieces" "$SESSION_DIR/projects/tactics-txt/pieces"
        cp -r "$SCRIPT_DIR/data" "$SESSION_DIR/data"
        # pieces/battle_01/units/<id>/ - REAL PERSISTENT DATA, dynamically
        # mkdir'd by CONFIRM_START/tactics_menu_input.c itself at runtime
        # (not pre-existing like config.txt) - must be symlinked as a
        # whole DIRECTORY (not a file) so any new subdirectory a live
        # session creates under it actually lands in the real project
        # root, not the ephemeral session dir. Same bug class as my-
        # chara-txt's own plots.txt / civ-txt's own board.txt symlink
        # omissions, live-caught here too (units directory came up
        # empty after a real CONFIRM_START run until this was added).
        mkdir -p "$SCRIPT_DIR/pieces/battle_01/units"
        cp -r "$SCRIPT_DIR/pieces/battle_01" "$SESSION_DIR/pieces/battle_01"

        cd "$SESSION_DIR"
        : > pieces/apps/player_app/interact_relay.txt
        : > pieces/keyboard/history.txt
        : > pieces/system/quit_flag.txt
        : > pieces/display/tactics_screen_changed.txt
        : > pieces/display/frame_changed.txt
        : > projects/tactics-txt/manager/gui_state.txt
        # For OPEN_BOARD_WIDGET (same pattern as civ-txt's own) to find
        # &.widgits/ and this project's own REAL (non-session) root -
        # see @.apps/BOARD_WIDGET_ARCHITECTURE.md §4.
        echo "$HOUSE_DIR" > pieces/system/house_root.txt
        echo "$SCRIPT_DIR" > pieces/system/real_project_root.txt

        if [ ! -f "$SCRIPT_DIR/pieces/system/config.txt" ]; then
            mkdir -p "$SCRIPT_DIR/pieces/system"
            cat > "$SCRIPT_DIR/pieces/system/config.txt" << 'EOCONFIG'
battle_id=tactics-001
mode=
turn=1
active_side=1
actions_remaining_this_turn=5
game_state=setup
EOCONFIG
        fi
        if [ ! -f "$SCRIPT_DIR/pieces/system/units.txt" ]; then
            cat > "$SCRIPT_DIR/pieces/system/units.txt" << 'EOUNITS'
side_1_unit_0_profession=warrior
side_1_unit_0_hp=20
side_1_unit_1_profession=chef
side_1_unit_1_hp=15
side_1_unit_2_profession=farmer
side_1_unit_2_hp=15
side_2_unit_0_profession=warrior
side_2_unit_0_hp=20
side_2_unit_1_profession=clown
side_2_unit_1_hp=15
side_2_unit_2_profession=lawyer
side_2_unit_2_hp=15
EOUNITS
        fi
        cp -r "$SCRIPT_DIR/pieces/system/config.txt" "$SESSION_DIR/pieces/system/config.txt"
        cp -r "$SCRIPT_DIR/pieces/system/units.txt" "$SESSION_DIR/pieces/system/units.txt"
        # board.txt - REAL PERSISTENT DATA, generated by CONFIRM_START,
        # read by the shared board-viewer widget from this project's own
        # REAL (non-session) root - same exact bug class as my-chara-
        # txt's own plots.txt symlink omission if this is forgotten (it
        # would silently write into the ephemeral session dir instead).
        touch "$SCRIPT_DIR/pieces/system/board.txt"
        cp -r "$SCRIPT_DIR/pieces/system/board.txt" "$SESSION_DIR/pieces/system/board.txt"
        # terrain_legend.txt - real per-host data-bank board-viewer reads
        # instead of hardcoding tactics-txt's own terrain in its C source
        # (Phase 0 of @.apps/piececraft-xyz/PIECECRAFT_XYZ_DESIGN.md §0a -
        # see &.widgits/board-viewer/ops/bv_render_3d.c's own
        # load_terrain_legend() for the full writeup). Seeded once, same
        # if-missing pattern config.txt already uses, so a fresh checkout
        # still works without this file having been hand-created.
        if [ ! -f "$SCRIPT_DIR/pieces/system/terrain_legend.txt" ]; then
            mkdir -p "$SCRIPT_DIR/pieces/system"
            cat > "$SCRIPT_DIR/pieces/system/terrain_legend.txt" << 'EOLEGEND'
# glyph|height|r|g|b|asset_hex|name  ("-" asset_hex = no texture)
.|0.0|120|170|80|1F33E|grass
^|0.8|140|120|100|26F0|high-ground
~|-0.3|40|90|180|1F30A|water
#|1.8|90|80|75|-|wall
EOLEGEND
        fi
        cp -r "$SCRIPT_DIR/pieces/system/terrain_legend.txt" "$SESSION_DIR/pieces/system/terrain_legend.txt"
        # widget_cmds/inbox.txt + board_widget_bridge.txt - real widget->
        # host command delivery (2026-08-03, Phase 0 follow-up per
        # @.apps/piececraft-xyz/PIECECRAFT_XYZ_DESIGN.md §4a) - see
        # civ-txt's own button.sh for the full comment.
        mkdir -p "$SCRIPT_DIR/pieces/system/widget_cmds"
        touch "$SCRIPT_DIR/pieces/system/widget_cmds/inbox.txt"
        cp -r "$SCRIPT_DIR/pieces/system/widget_cmds" "$SESSION_DIR/pieces/system/widget_cmds"
        cat > "$SCRIPT_DIR/pieces/system/board_widget_bridge.txt" << EOF
inbox_path=pieces/system/widget_cmds/inbox.txt
kind=board_game
project_id=tactics-txt
display_name=Tactics-txt
EOF
        cp -r "$SCRIPT_DIR/pieces/system/board_widget_bridge.txt" "$SESSION_DIR/pieces/system/board_widget_bridge.txt"
        # entities.txt - REAL PERSISTENT DATA, same class as board.txt -
        # the generic entity-render manifest board-viewer reads (see
        # ops/tactics_menu_input.c's own CONFIRM_START comment for the
        # full format/reasoning).
        touch "$SCRIPT_DIR/pieces/system/entities.txt"
        cp -r "$SCRIPT_DIR/pieces/system/entities.txt" "$SESSION_DIR/pieces/system/entities.txt"

        cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/setup_module.pal
project_id=tactics-txt
active_target_id=setup
EOSTATE

        export PRISC_PROJECT_ROOT="$SESSION_DIR"
        export PRISC_PROJECT_ID="tactics-txt"
        export NO_NET=1
        export PAL_LAYOUT="pieces/chtpm/layouts/setup.chtpm"
        "$SCRIPT_DIR/system/orchestrator" 2>>pieces/system/orchestrator.log &
        ORCH_PID=$!

        # OPTIONAL GL/RGB MIRROR - gated on NO_GL and a real DISPLAY,
        # skips gracefully otherwise. MUST wait for chtpm_parser_pal's
        # own first real compose before launching chtpm_rgb_render, or
        # rgb_frame.raw gets stuck permanently all-black/stale. This is
        # the BASIC TEXT mirror only (main.chtpm/setup.chtpm content) -
        # NOT the real board widget, which is now a fully separate
        # program/window (&.widgits/board-viewer, spawned via
        # OPEN_BOARD_WIDGET, same as civ-txt) - see this project's own
        # HANDOFF_NEXT_SESSION.md for its current status.
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
            mkdir -p "$(dirname "$SCRIPT_DIR/pieces/system/units.txt")" 2>/dev/null || true
            cp -r "$SESSION_DIR/pieces/system/units.txt" "$SCRIPT_DIR/pieces/system/units.txt" 2>/dev/null || true
            mkdir -p "$(dirname "$SCRIPT_DIR/pieces/system/board.txt")" 2>/dev/null || true
            cp -r "$SESSION_DIR/pieces/system/board.txt" "$SCRIPT_DIR/pieces/system/board.txt" 2>/dev/null || true
            mkdir -p "$(dirname "$SCRIPT_DIR/pieces/system/terrain_legend.txt")" 2>/dev/null || true
            cp -r "$SESSION_DIR/pieces/system/terrain_legend.txt" "$SCRIPT_DIR/pieces/system/terrain_legend.txt" 2>/dev/null || true
            mkdir -p "$(dirname "$SCRIPT_DIR/pieces/system/entities.txt")" 2>/dev/null || true
            cp -r "$SESSION_DIR/pieces/system/entities.txt" "$SCRIPT_DIR/pieces/system/entities.txt" 2>/dev/null || true
            mkdir -p "$SCRIPT_DIR/pieces/system/widget_cmds/" 2>/dev/null || true
            cp -r "$SESSION_DIR/pieces/system/widget_cmds/." "$SCRIPT_DIR/pieces/system/widget_cmds/" 2>/dev/null || true
            mkdir -p "$(dirname "$SCRIPT_DIR/pieces/system/board_widget_bridge.txt")" 2>/dev/null || true
            cp -r "$SESSION_DIR/pieces/system/board_widget_bridge.txt" "$SCRIPT_DIR/pieces/system/board_widget_bridge.txt" 2>/dev/null || true
        }
        trap 'kill "$ORCH_PID" "$GL_PID" "$RGB_PID" 2>/dev/null; wait "$ORCH_PID" 2>/dev/null; kill_own_module; kill_own_board_widget; persist_session_state; rm -rf "$SESSION_DIR"' EXIT INT TERM

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
        echo "done"
        ;;
    check|verify)
        for b in system/prisc+x system/keyboard_input system/renderer \
                 system/chtpm_parser_pal system/orchestrator \
                 ops/+x/tactics_menu_input.+x ops/+x/tactics_compose_frame.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b"; else echo "MISSING $b"; fi
        done
        for b in system/chtpm_rgb_render system/gl_mirror; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b (optional GL mirror)"; else echo "OPTIONAL-MISS $b"; fi
        done
        ;;
    help|h|-h|--help)
        echo "tactics-txt button.sh"
        echo ""
        echo "Usage: ./button.sh <action>"
        echo "  compile, c, build   - Build prisc+x + ops"
        echo "  run, r              - THE REAL PLAYABLE UI (interactive, needs a real terminal)"
        echo "  kill, k, stop       - Kill any lingering tactics-txt processes"
        echo "  check, verify       - Verify all binaries exist"
        echo "  help, h             - Show this help"
        ;;
    *)
        echo "Unknown action: $ACTION"
        exit 1
        ;;
esac
